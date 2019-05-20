#include <torch/csrc/utils/tensor_numpy.h>
#include <torch/csrc/utils/numpy_stub.h>

#ifndef USE_NUMPY
namespace torch { namespace utils {
PyObject* tensor_to_numpy(const at::Tensor& tensor) {
  throw std::runtime_error("PyTorch was compiled without NumPy support");
}
at::Tensor tensor_from_numpy(PyObject* obj) {
  throw std::runtime_error("PyTorch was compiled without NumPy support");
}
bool is_numpy_scalar(PyObject* obj) {
  throw std::runtime_error("PyTorch was compiled without NumPy support");
}
at::Tensor tensor_from_cuda_array_interface(PyObject* obj) {
    throw std::runtime_error("PyTorch was compiled without NumPy support");
}
}}
#else

#include <torch/csrc/DynamicTypes.h>
#include <torch/csrc/Exceptions.h>
#include <torch/csrc/autograd/python_variable.h>
#include <torch/csrc/utils/object_ptr.h>

#include <ATen/ATen.h>
#include <ATen/TensorUtils.h>
#include <memory>
#include <sstream>
#include <stdexcept>

using namespace at;
using namespace torch::autograd;

namespace torch { namespace utils {

static std::vector<npy_intp> to_numpy_shape(IntArrayRef x) {
  // shape and stride conversion from int64_t to npy_intp
  auto nelem = x.size();
  auto result = std::vector<npy_intp>(nelem);
  for (size_t i = 0; i < nelem; i++) {
    result[i] = static_cast<npy_intp>(x[i]);
  }
  return result;
}

static std::vector<int64_t> to_aten_shape(int ndim, npy_intp* values) {
  // shape and stride conversion from npy_intp to int64_t
  auto result = std::vector<int64_t>(ndim);
  for (int i = 0; i < ndim; i++) {
    result[i] = static_cast<int64_t>(values[i]);
  }
  return result;
}

static std::vector<int64_t> seq_to_aten_shape(PyObject *py_seq) {
  int ndim = PySequence_Length(py_seq);
  if (ndim == -1) {
    throw TypeError("shape and strides must be sequences");
  }
  auto result = std::vector<int64_t>(ndim);
  for (int i = 0; i < ndim; i++) {
    auto item = THPObjectPtr(PySequence_GetItem(py_seq, i));
    if (!item) throw python_error();

    result[i] = PyLong_AsLongLong(item);
    if (result[i] == -1 && PyErr_Occurred()) throw python_error();
  }
  return result;
}

static int aten_to_dtype(const ScalarType scalar_type);

PyObject* tensor_to_numpy(const at::Tensor& tensor) {
  if (tensor.is_cuda()) {
    throw TypeError(
        "can't convert CUDA tensor to numpy. Use Tensor.cpu() to "
        "copy the tensor to host memory first.");
  }
  if (tensor.is_sparse()) {
    throw TypeError(
        "can't convert sparse tensor to numpy. Use Tensor.to_dense() to "
        "convert to a dense tensor first.");
  }
  if (tensor.type().backend() != Backend::CPU) {
    throw TypeError("NumPy conversion for %s is not supported", tensor.type().toString().c_str());
  }
  auto dtype = aten_to_dtype(tensor.scalar_type());
  auto sizes = to_numpy_shape(tensor.sizes());
  auto strides = to_numpy_shape(tensor.strides());
  // NumPy strides use bytes. Torch strides use element counts.
  auto element_size_in_bytes = tensor.element_size();
  for (auto& stride : strides) {
    stride *= element_size_in_bytes;
  }

  auto array = THPObjectPtr(PyArray_New(
      &PyArray_Type,
      tensor.dim(),
      sizes.data(),
      dtype,
      strides.data(),
      tensor.data_ptr(),
      0,
      NPY_ARRAY_ALIGNED | NPY_ARRAY_WRITEABLE,
      nullptr));
  if (!array) return nullptr;

  // TODO: This attempts to keep the underlying memory alive by setting the base
  // object of the ndarray to the tensor and disabling resizes on the storage.
  // This is not sufficient. For example, the tensor's storage may be changed
  // via Tensor.set_, which can free the underlying memory.
  PyObject* py_tensor = THPVariable_Wrap(make_variable(tensor, false));
  if (!py_tensor) throw python_error();
  if (PyArray_SetBaseObject((PyArrayObject*)array.get(), py_tensor) == -1) {
    return nullptr;
  }
  // Use the private storage API
  tensor.storage().unsafeGetStorageImpl()->set_resizable(false);

  return array.release();
}

at::Tensor tensor_from_numpy(PyObject* obj) {
  if (!PyArray_Check(obj)) {
    throw TypeError("expected np.ndarray (got %s)", Py_TYPE(obj)->tp_name);
  }

  auto array = (PyArrayObject*)obj;
  int ndim = PyArray_NDIM(array);
  auto sizes = to_aten_shape(ndim, PyArray_DIMS(array));
  auto strides = to_aten_shape(ndim, PyArray_STRIDES(array));
  // NumPy strides use bytes. Torch strides use element counts.
  auto element_size_in_bytes = PyArray_ITEMSIZE(array);
  for (auto& stride : strides) {
    if (stride%element_size_in_bytes != 0) {
      throw ValueError(
        "given numpy array strides not a multiple of the element byte size. "
        "Copy the numpy array to reallocate the memory.");
    }
    stride /= element_size_in_bytes;
  }

  size_t storage_size = 1;
  for (int i = 0; i < ndim; i++) {
    if (strides[i] < 0) {
      throw ValueError(
          "some of the strides of a given numpy array are negative. This is "
          "currently not supported, but will be added in future releases.");
    }
    // XXX: this won't work for negative strides
    storage_size += (sizes[i] - 1) * strides[i];
  }

  void* data_ptr = PyArray_DATA(array);
  if (!PyArray_EquivByteorders(PyArray_DESCR(array)->byteorder, NPY_NATIVE)) {
    throw ValueError(
        "given numpy array has byte order different from the native byte order. "
        "Conversion between byte orders is currently not supported.");
  }
  Py_INCREF(obj);
  return at::from_blob(
      data_ptr,
      sizes,
      strides,
      [obj](void* data) {
          AutoGIL gil;
          Py_DECREF(obj);
      },
      at::device(kCPU).dtype(numpy_dtype_to_aten(PyArray_TYPE(array)))
  );
}

static int aten_to_dtype(const ScalarType scalar_type) {
  switch (scalar_type) {
    case kDouble: return NPY_DOUBLE;
    case kFloat: return NPY_FLOAT;
    case kHalf: return NPY_HALF;
    case kLong: return NPY_INT64;
    case kInt: return NPY_INT32;
    case kShort: return NPY_INT16;
    case kChar: return NPY_INT8;
    case kByte: return NPY_UINT8;
    case kBool: return NPY_BOOL;
    default:
      throw ValueError("Got unsupported ScalarType ", toString(scalar_type));
  }
}

ScalarType numpy_dtype_to_aten(int dtype) {
  switch (dtype) {
    case NPY_DOUBLE: return kDouble;
    case NPY_FLOAT: return kFloat;
    case NPY_HALF: return kHalf;
    case NPY_INT32: return kInt;
    case NPY_INT16: return kShort;
    case NPY_INT8: return kChar;
    case NPY_UINT8: return kByte;
    case NPY_BOOL: return kBool;
    default:
      // Workaround: MSVC does not support two switch cases that have the same value
      if (dtype == NPY_LONGLONG || dtype == NPY_INT64) {
        return kLong;
      } else {
        break;
      }
  }
  auto pytype = THPObjectPtr(PyArray_TypeObjectFromType(dtype));
  if (!pytype) throw python_error();
  throw TypeError(
      "can't convert np.ndarray of type %s. The only supported types are: "
      "float64, float32, float16, int64, int32, int16, int8, and uint8.",
      ((PyTypeObject*)pytype.get())->tp_name);
}

bool is_numpy_scalar(PyObject* obj) {
  return (PyArray_IsIntegerScalar(obj) ||
          PyArray_IsScalar(obj, Floating));
}

at::Tensor tensor_from_cuda_array_interface(PyObject* obj) {
  PyObject *cuda_dict = PyObject_GetAttrString(obj, "__cuda_array_interface__");
  TORCH_INTERNAL_ASSERT(cuda_dict != nullptr);

  // In the following code block, we extract the values of `__cuda_array_interface__`
  std::vector<int64_t> sizes;
  ScalarType dtype;
  void *data_ptr;
  std::vector<int64_t> strides;
  try {
    if (!PyDict_Check(cuda_dict)) {
      throw TypeError("`__cuda_array_interface__` must be a dict");
    }
    PyObject *py_shape = PyDict_GetItemString(cuda_dict, "shape");
    PyObject *py_typestr = PyDict_GetItemString(cuda_dict, "typestr");
    PyObject *py_data = PyDict_GetItemString(cuda_dict, "data");
    if (py_shape == nullptr || py_typestr == nullptr || py_data == nullptr) {
      throw TypeError("attributes `shape`, `typestr`, and `data` must exist");
    }

    // Extract the `shape` attribute
    sizes = seq_to_aten_shape(py_shape);

    // Extract the `typestr` attribute
    int dtype_size_in_bytes;
    {
      PyArray_Descr *descr;
      if(!PyArray_DescrConverter(py_typestr, &descr)) {
        throw ValueError("cannot parse `typestr`");
      }
      dtype = numpy_dtype_to_aten(descr->type_num);
      dtype_size_in_bytes = descr->elsize;
      TORCH_INTERNAL_ASSERT(dtype_size_in_bytes > 0);
    }

    // Extract the `data` attribute
    if(!PyTuple_Check(py_data) || PyTuple_GET_SIZE(py_data) != 2) {
      throw TypeError("`data` must be a 2-tuple of (int, bool)");
    }
    data_ptr = PyLong_AsVoidPtr(PyTuple_GET_ITEM(py_data, 0));
    int read_only = PyObject_IsTrue(PyTuple_GET_ITEM(py_data, 1));
    if (PyErr_Occurred() || read_only == -1) {
      throw TypeError("`data` must be a 2-tuple (int, bool)");
    }
    if (read_only) {
      throw TypeError("the read only flag is not supported, should always be False");
    }

    // Extract the `strides` attribute
    PyObject *py_strides = PyDict_GetItemString(cuda_dict, "strides");
    if (py_strides != nullptr && py_strides != Py_None) {
      if (PySequence_Length(py_strides) == -1 || PySequence_Length(py_strides) != PySequence_Length(py_shape)) {
        throw TypeError("strides must be a sequence of the same length as shape");
      }
      strides = seq_to_aten_shape(py_strides);
    } else {
      strides = at::detail::defaultStrides(sizes);
    }

    // __cuda_array_interface__ strides use bytes. Torch strides use element counts.
    for (auto& stride : strides) {
      if (stride%dtype_size_in_bytes != 0) {
        throw ValueError(
            "given array strides not a multiple of the element byte size. "
            "Make a copy of the array to reallocate the memory.");
        }
      stride /= dtype_size_in_bytes;
    }
  } catch (const ValueError &e) {
    Py_DECREF(cuda_dict);
    throw;
  }

  Py_INCREF(obj);
  return at::from_blob(
      data_ptr,
      sizes,
      strides,
      [obj](void* data) {
          AutoGIL gil;
          Py_DECREF(obj);
      },
      at::device(kCUDA).dtype(dtype)
  );
}
}} // namespace torch::utils

#endif  // USE_NUMPY
