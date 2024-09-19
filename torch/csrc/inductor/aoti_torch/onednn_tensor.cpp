#include <ATen/Config.h>
#include <torch/csrc/inductor/aoti_torch/onednn_tensor.h>

#if AT_ONEDNN_ENABLED()
#include <ATen/native/onednn/ONEDNNCommon.h>
#include <ideep.hpp>
#endif

namespace torch::aot_inductor {

#if AT_ONEDNN_ENABLED()

void* data_ptr_from_onednn(at::Tensor* onednn_tensor) {
  return reinterpret_cast<void*>(
      at::native::data_ptr_from_onednn(*onednn_tensor));
}

at::Tensor onednn_tensor_from_data_ptr(
    void* data_ptr,
    at::IntArrayRef dims,
    at::ScalarType dtype,
    at::Device device,
    const uint8_t* opaque_metadata,
    int64_t opaque_metadata_size) {
  return at::native::onednn_tensor_from_data_ptr(
      data_ptr, dims, dtype, device, opaque_metadata, opaque_metadata_size);
}

#else

void* data_ptr_from_onednn(at::Tensor* onednn_tensor) {
  TORCH_CHECK(false, "MKL-DNN build is disabled");
}

at::Tensor onednn_tensor_from_data_ptr(
    void* data_ptr,
    at::IntArrayRef dims,
    at::ScalarType dtype,
    at::Device device,
    const uint8_t* opaque_metadata,
    int64_t opaque_metadata_size) {
  TORCH_CHECK(false, "MKL-DNN build is disabled");
}

#endif

} // namespace torch::aot_inductor
