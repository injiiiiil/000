#include <ATen/ATen.h>

#include <ATen/Dispatch.h>
#include <ATen/native/Lerp.h>
#include <ATen/native/TensorIterator.h>
#include <ATen/native/cpu/Loops.h>

namespace at {
namespace native {
namespace {

static void lerp_kernel_scalar(
    Tensor& ret,
    const Tensor& self,
    const Tensor& end,
    Scalar weight) {
  auto iter = TensorIterator();
  iter.add_output(ret);
  iter.add_input(self);
  iter.add_input(end);
  iter.build();
  AT_DISPATCH_FLOATING_TYPES(ret.scalar_type(), "lerp_kernel_scalar", [&] {
    scalar_t weight_val = weight.to<scalar_t>();
    at::native::cpu_kernel(
        iter,
        [weight_val](scalar_t self_val, scalar_t end_val) {
          return (weight_val < 0.5)
              ? self_val + weight_val * (end_val - self_val)
              : end_val - (end_val - self_val) * (1 - weight_val);
        });
  });
}

static void lerp_kernel_tensor(
    Tensor& ret,
    const Tensor& self,
    const Tensor& end,
    const Tensor& weights) {
  auto iter = TensorIterator();
  iter.add_output(ret);
  iter.add_input(self);
  iter.add_input(end);
  iter.add_input(weights);
  iter.build();
  AT_DISPATCH_FLOATING_TYPES(ret.scalar_type(), "lerp_kernel_tensor", [&] {
    at::native::cpu_kernel(
        iter,
        [](scalar_t self_val, scalar_t end_val, scalar_t weight_val) {
          return (weight_val < 0.5)
              ? self_val + weight_val * (end_val - self_val)
              : end_val - (end_val - self_val) * (1 - weight_val);
        });
  });
}

} // anonymous namespace

REGISTER_DISPATCH(lerp_kernel_scalar_weight, &lerp_kernel_scalar);
REGISTER_DISPATCH(lerp_kernel_tensor_weight, &lerp_kernel_tensor);

} // namespace native
} // namespace at
