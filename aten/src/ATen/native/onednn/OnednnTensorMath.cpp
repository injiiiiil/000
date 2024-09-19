#define TORCH_ASSERT_ONLY_METHOD_OPERATORS
#include <ATen/core/Tensor.h>
#include <ATen/Config.h>
#include <ATen/Parallel.h>
#include <ATen/cpu/vec/functional.h>
#include <ATen/cpu/vec/vec.h>

#ifndef AT_PER_OPERATOR_HEADERS
#include <ATen/NativeFunctions.h>
#else
#include <ATen/ops/zero_native.h>
#endif

#if !AT_ONEDNN_ENABLED()

namespace at {
namespace native {

Tensor& onednn_zero_(Tensor& self) {
  TORCH_CHECK(false, "onednn_zero_: ATen not compiled with ONEDNN support");
}

} // namespace native
} // namespace at

#else // AT_ONEDNN_ENABLED

#include <ATen/native/onednn/ONEDNNCommon.h>

namespace at {
namespace native {

Tensor& onednn_zero_(Tensor& self) {
  using Vec = vec::Vectorized<float>;

  ideep::tensor& x = itensor_from_onednn(self);

  auto n = x.get_nelems();
  auto* x_ = static_cast<float*>(x.get_data_handle());
  parallel_for(0, n, 2048, [x_](int64_t begin, int64_t end) {
    vec::map(
        [](Vec /* unused */) { return 0.0; },
        x_ + begin,
        x_ + begin,
        end - begin);
  });

  return self;
}

} // namespace native
} // namespace at

#endif // AT_ONEDNN_ENABLED
