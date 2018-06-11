#include <Python.h>

#include "tensor_conversion_dispatch.h"

#include "torch/csrc/utils/auto_gil.h"
#include "torch/csrc/utils/cuda_lazy_init.h"

#include <ATen/AutoGPU.h>

#include <cstddef>

namespace torch { namespace utils {

at::Tensor dispatch_type_conversion(
    const at::Tensor & self,
    const at::Type & type,
    at::optional<int32_t> device_index,
    bool non_blocking) {
  if (type.is_cuda()) {
    torch::utils::cuda_lazy_init();
  }
  AutoNoGIL no_gil;

  // See the documentation in tensor_conversion_dispatch.h to understand this.
  if (!device_index.has_value() && self.is_cuda()) {
    device_index = self.get_device();
  }
  if (*device_index == -1) {
    device_index.reset();
  }

  at::AutoGPU auto_gpu(device_index);

  const int32_t tensor_device = self.is_cuda() ? self.get_device() : -1;
  if (self.is_cuda() && type.is_cuda() && tensor_device != at::current_device()) {
    // copy if the devices are different even if the types are the same
    return type.copy(self, non_blocking);
  }

  // Don't specialize cross-backend copies
  if (self.type().backend() != type.backend()) {
    return self.toType(type, non_blocking);
  }

  // Dispatch to specialized, traceable cast operators for the JIT. These
  // specialized ops are ATen native and thus have the tracing mechanisms auto-
  // generated, whereas the default case is not traceable since it requires a
  // Type as a parameter/attribute. TODO: support Types in the JIT and remove
  // this once we have that
  switch (type.scalarType()) {
#define DEFINE_CAST_DISPATCH(_1, n, _2)   \
  case at::ScalarType::n: {               \
    return self._cast_##n(non_blocking); \
  } break;
    AT_FORALL_SCALAR_TYPES(DEFINE_CAST_DISPATCH)
#undef DEFINE_CAST_DISPATCH
    default: { return self.toType(type, non_blocking); } break;
  }
}

}} // namespace torch::utils
