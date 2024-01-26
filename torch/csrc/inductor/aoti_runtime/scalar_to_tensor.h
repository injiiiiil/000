#pragma once

#include <torch/csrc/inductor/aoti_runtime/model.h>
#include <torch/csrc/inductor/aoti_torch/c/shim.h>

namespace torch {
namespace aot_inductor {

template <typename T>
inline RAIIAtenTensorHandle scalar_to_tensor_handle(T value) {
  throw std::runtime_error("Unsupported scalar_to_tensor_handle");
}

// Specialize for supported C++ primitive types
#define AOTI_RUNTIME_SCALAR_TO_TENSOR(dtype, ctype)                         \
  template <>                                                               \
  inline RAIIAtenTensorHandle scalar_to_tensor_handle<ctype>(ctype value) { \
    AtenTensorHandle tensor_handle;                                         \
    AOTI_TORCH_ERROR_CODE_CHECK(                                            \
        aoti_torch_scalar_to_tensor_##dtype(value, &tensor_handle));        \
    return RAIIAtenTensorHandle(tensor_handle);                             \
  }

AOTI_RUNTIME_SCALAR_TO_TENSOR(float32, float)
AOTI_RUNTIME_SCALAR_TO_TENSOR(float64, double)
AOTI_RUNTIME_SCALAR_TO_TENSOR(uint8, uint8_t)
AOTI_RUNTIME_SCALAR_TO_TENSOR(uint16, uint16_t)
AOTI_RUNTIME_SCALAR_TO_TENSOR(uint32, uint32_t)
AOTI_RUNTIME_SCALAR_TO_TENSOR(uint64, uint64_t)
AOTI_RUNTIME_SCALAR_TO_TENSOR(int8, int8_t)
AOTI_RUNTIME_SCALAR_TO_TENSOR(int16, int16_t)
AOTI_RUNTIME_SCALAR_TO_TENSOR(int32, int32_t)
AOTI_RUNTIME_SCALAR_TO_TENSOR(int64, int64_t)
AOTI_RUNTIME_SCALAR_TO_TENSOR(bool, bool)
#undef AOTI_RUNTIME_SCALAR_TO_TENSOR

} // namespace aot_inductor
} // namespace torch
