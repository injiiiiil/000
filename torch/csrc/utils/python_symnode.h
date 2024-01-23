#pragma once

#include <c10/core/SafePyObject.h>
#include <c10/core/SymNodeImpl.h>

#include <ATen/core/SingletonSymNodeImpl.h>
#include <torch/csrc/PyInterpreter.h>
#include <torch/csrc/autograd/python_variable.h>
#include <torch/csrc/utils/pybind.h>

namespace torch {

TORCH_PYTHON_API py::handle get_symint_class();
TORCH_PYTHON_API py::handle get_symfloat_class();
TORCH_PYTHON_API py::handle get_symbool_class();

// NB: These functions must not be called too early, otherwise torch not setup.
// Alternate design is to have torch "register" the object to us
inline bool is_symint(py::handle obj) {
  return py::isinstance(obj, get_symint_class());
}
inline bool is_symfloat(py::handle obj) {
  return py::isinstance(obj, get_symfloat_class());
}
inline bool is_symbool(py::handle obj) {
  return py::isinstance(obj, get_symbool_class());
}

namespace impl {

// This c10::SymNodeImpl simply backends to a Python object that
// implements the API.   The Python object is the source of truth,
// this is just an adapter so C++ calls can get to the object.
class PythonSymNodeImpl : public c10::SymNodeImpl {
 public:
  PythonSymNodeImpl(py::object pyobj, c10::optional<bool> is_singleton=c10::nullopt) : c10::SymNodeImpl() {
    py::gil_scoped_acquire acquire;
    if (is_singleton.has_value()) {
      is_singleton_ = is_singleton.value();
    } else {
      is_singleton_ = pyobj.attr("is_singleton")().is(py::handle(Py_True));
    }
    pyobj_ = std::make_shared<c10::SafePyObject>(
        pyobj.release().ptr(), getPyInterpreter());
    key_set_ = is_singleton_ ? c10::py_singleton_ks : c10::DispatchKeySet();
  };

  c10::SymNode wrap_int(int64_t num) override {
    py::gil_scoped_acquire acquire;
    auto r = getPyObj().attr("wrap_int")(num);
    return c10::make_intrusive<PythonSymNodeImpl>(std::move(r));
  }

  c10::SymNode wrap_float(double num) override {
    py::gil_scoped_acquire acquire;
    auto r = getPyObj().attr("wrap_float")(num);
    return c10::make_intrusive<PythonSymNodeImpl>(std::move(r));
  }

  c10::SymNode wrap_bool(bool num) override {
    py::gil_scoped_acquire acquire;
    auto r = getPyObj().attr("wrap_bool")(num);
    return c10::make_intrusive<PythonSymNodeImpl>(std::move(r));
  }

#define TORCH_SYMNODE_SIZES_STRIDES(n)                                        \
  c10::SymNode n(                                                             \
      c10::ArrayRef<c10::SymNode> sizes, c10::ArrayRef<c10::SymNode> strides) \
      override {                                                              \
    py::gil_scoped_acquire acquire;                                           \
    auto r = getPyObj().attr(#n)(sizes, strides);                             \
    return c10::make_intrusive<PythonSymNodeImpl>(std::move(r), false);       \
  }

  // clang-format off
    TORCH_SYMNODE_SIZES_STRIDES(is_contiguous)
    TORCH_SYMNODE_SIZES_STRIDES(is_channels_last_contiguous_2d)
    TORCH_SYMNODE_SIZES_STRIDES(is_channels_last_contiguous_3d)
    TORCH_SYMNODE_SIZES_STRIDES(is_channels_last_strides_2d)
    TORCH_SYMNODE_SIZES_STRIDES(is_channels_last_strides_3d)
    TORCH_SYMNODE_SIZES_STRIDES(is_non_overlapping_and_dense)
  // clang-format on

#undef TORCH_SYMNODE_SIZES_STRIDES

  bool bool_() override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("bool_")().is(py::handle(Py_True));
  }

  bool is_int() override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("is_int")().is(py::handle(Py_True));
  }

  bool is_float() override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("is_float")().is(py::handle(Py_True));
  }

  bool is_bool() override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("is_bool")().is(py::handle(Py_True));
  }

  bool is_singleton() override {
    return is_singleton_;
  }

  c10::TensorImpl* singleton_vec() override {
    py::gil_scoped_acquire acquire;
    return getPyObj()
        .attr("singleton_vec")()
        .cast<at::Tensor>()
        .unsafeGetTensorImpl();
  }

  int64_t singleton_sum_vec() override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("singleton_sum_vec")().cast<int64_t>();
  }

  bool has_hint() override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("has_hint")().is(py::handle(Py_True));
  }

  int64_t guard_int(const char* file, int64_t line) override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("guard_int")(file, line).cast<int64_t>();
  }

  double guard_float(const char* file, int64_t line) override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("guard_float")(file, line).cast<double>();
  }

  bool guard_bool(const char* file, int64_t line) override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("guard_bool")(file, line).cast<bool>();
  }

  bool expect_true(const char* file, int64_t line) override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("expect_true")(file, line).cast<bool>();
  }

  bool expect_size(const char* file, int64_t line) override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("expect_size")(file, line).cast<bool>();
  }

  int64_t int_() override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("int_")().cast<int64_t>();
  }

  c10::optional<int64_t> maybe_as_int() override {
    py::gil_scoped_acquire acquire;
    const auto& r = getPyObj().attr("maybe_as_int")();
    if (r.is_none()) {
      return c10::nullopt;
    } else {
      return r.cast<int64_t>();
    }
  }

  std::string str() override {
    py::gil_scoped_acquire acquire;
    return getPyObj().attr("str")().cast<std::string>();
  }

  c10::SymNode dispatch_sym_ite_(
      const char* fname,
      const c10::SymNode& other,
      const c10::SymNode& third) {
    auto pother = dynamic_cast<PythonSymNodeImpl*>(other.get());
    auto pthird = dynamic_cast<PythonSymNodeImpl*>(third.get());
    TORCH_CHECK(pother);
    TORCH_CHECK(pthird);
    py::gil_scoped_acquire acquire;
    auto r = getPyObj().attr(fname)(pother->getPyObj(), pthird->getPyObj());
    return c10::make_intrusive<PythonSymNodeImpl>(r, false);
  }

  c10::SymNode dispatch_common_(const char* fname, const c10::SymNode& other, bool mb_singleton=false) {
    auto pother = dynamic_cast<PythonSymNodeImpl*>(other.get());
    TORCH_CHECK(pother);
    py::gil_scoped_acquire acquire;
    auto r = getPyObj().attr(fname)(pother->getPyObj());
    if (mb_singleton) {
      return c10::make_intrusive<PythonSymNodeImpl>(r);
    } else {
      return c10::make_intrusive<PythonSymNodeImpl>(r, false);
    }
  }

  c10::SymNode dispatch_common_(const char* fname, bool mb_singleton=false) {
    py::gil_scoped_acquire acquire;
    auto r = getPyObj().attr(fname)();
    if (mb_singleton) {
      return c10::make_intrusive<PythonSymNodeImpl>(r);
    } else {
      return c10::make_intrusive<PythonSymNodeImpl>(r, false);
    }
  }

  c10::SymNode add(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode sub(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode mul(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other, true);
  }

  c10::SymNode truediv(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode pow(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode floordiv(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode mod(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode eq(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode ne(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode gt(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode lt(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode le(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode ge(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode sym_min(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }
  c10::SymNode sym_max(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode sym_and(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode sym_or(const c10::SymNode& other) override {
    return dispatch_common_(__func__, other);
  }

  c10::SymNode sym_ite(const c10::SymNode& other, const c10::SymNode& third)
      override {
    return dispatch_sym_ite_(__func__, other, third);
  }

  c10::SymNode sym_not() override {
    return dispatch_common_(__func__);
  }

  c10::SymNode ceil() override {
    return dispatch_common_(__func__);
  }

  c10::SymNode floor() override {
    return dispatch_common_(__func__);
  }

  c10::SymNode neg() override {
    return dispatch_common_(__func__, true);
  }

  c10::SymNode clone() override {
    return dispatch_common_(__func__, true);
  }

  c10::SymNode sym_float() override {
    return dispatch_common_(__func__);
  }

  py::handle getPyObj() {
    return py::handle(pyobj_->ptr(getPyInterpreter()));
  }
  bool is_singleton_;
  std::shared_ptr<c10::SafePyObject> pyobj_ = nullptr;
};

} // namespace impl
} // namespace torch
