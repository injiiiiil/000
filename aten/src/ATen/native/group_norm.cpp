#include <ATen/native/group_norm.h>

#include <array>
#include <functional>
#include <numeric>
#include <tuple>
#include <vector>

#include <ATen/ATen.h>
#include <ATen/AccumulateType.h>
#include <ATen/CPUApplyUtils.h>
#include <ATen/Config.h>
#include <ATen/NativeFunctions.h>
#include <ATen/Parallel.h>

namespace at {
namespace native {

std::tuple<Tensor, Tensor, Tensor> native_group_norm(
    const Tensor& X,
    const Tensor& gamma /* optional */,
    const Tensor& beta /* optional */,
    int64_t N,
    int64_t C,
    int64_t HxW,
    int64_t group,
    double eps) {
  Tensor Y = at::native::empty_like(X, LEGACY_CONTIGUOUS_MEMORY_FORMAT);
  Tensor mean = at::empty({N, group}, X.options());
  Tensor rstd = at::empty({N, group}, X.options());
  GroupNormKernel(
      X.device().type(),
      X,
      gamma,
      beta,
      N,
      C,
      HxW,
      group,
      eps,
      &Y,
      &mean,
      &rstd);
  return std::make_tuple(Y, mean, rstd);
}

std::tuple<Tensor, Tensor, Tensor> native_group_norm_backward(
    const Tensor& dY,
    const Tensor& X,
    const Tensor& mean,
    const Tensor& rstd,
    const Tensor& gamma,
    int64_t N,
    int64_t C,
    int64_t HxW,
    int64_t group,
    std::array<bool, 3> grad_input_mask) {
  Tensor dX;
  Tensor dgamma;
  Tensor dbeta;
  if (grad_input_mask[0]) {
    dX = at::native::empty_like(X, LEGACY_CONTIGUOUS_MEMORY_FORMAT);
  }
  if (grad_input_mask[1]) {
    dgamma = at::native::empty_like(gamma, LEGACY_CONTIGUOUS_MEMORY_FORMAT);
  }
  if (grad_input_mask[2]) {
    dbeta = at::native::empty_like(gamma, LEGACY_CONTIGUOUS_MEMORY_FORMAT);
  }
  GroupNormBackwardKernel(
      X.device().type(),
      dY,
      X,
      mean,
      rstd,
      gamma,
      N,
      C,
      HxW,
      group,
      &dX,
      &dgamma,
      &dbeta);
  return std::make_tuple(dX, dgamma, dbeta);
}

Tensor group_norm(
    const Tensor& input,
    int64_t num_groups,
    const Tensor& weight /* optional */,
    const Tensor& bias /* optional */,
    double eps,
    bool cudnn_enabled) {
  const int64_t N = input.size(0);
  const int64_t C = input.size(1);
  TORCH_CHECK(
      C % num_groups == 0,
      "Expected number of channels in input to be divisible by ",
      "num_groups, but got input of shape ",
      input.sizes(),
      " and "
      "num_groups=",
      num_groups);
  TORCH_CHECK(
      !weight.defined() || (weight.dim() == 1 && weight.numel() == C),
      "Expected weight to be a vector of size equal to the number of ",
      "channels in input, but got weight of shape ",
      weight.sizes(),
      " and input of shape ",
      input.sizes());
  TORCH_CHECK(
      !bias.defined() || (bias.dim() == 1 && bias.numel() == C),
      "Expected bias to be a vector of size equal to the number of ",
      "channels in input, but got bias of shape ",
      weight.sizes(),
      " and input of shape ",
      input.sizes());

  const auto input_shape = input.sizes();
  const int64_t HxW = std::accumulate(
      input_shape.cbegin() + 2,
      input_shape.cend(),
      1LL,
      std::multiplies<int64_t>());

  if (input.device().is_cpu()) {
    const auto& X = input.is_contiguous() ? input : input.contiguous();
    const auto& gamma = weight.is_contiguous() ? weight : weight.contiguous();
    const auto& beta = bias.is_contiguous() ? bias : bias.contiguous();
    return std::get<0>(
        at::native_group_norm(X, gamma, beta, N, C, HxW, num_groups, eps));
  }

  // Apply group norm
  const int64_t b = input.size(0);
  const int64_t c = input.size(1);
  auto input_reshaped =
      input.contiguous().view({1, b * num_groups, b ? -1 : 1});
  auto out = at::batch_norm(
      input_reshaped, {}, {}, {}, {}, true, 0, eps, cudnn_enabled);
  out = out.view(input_shape);
  if (!weight.defined() && !bias.defined()) {
    return out;
  }
  std::vector<int64_t> affine_param_shape(input.dim(), 1);
  affine_param_shape[1] = C;
  if (weight.defined() && bias.defined()) {
    return bias.view(affine_param_shape)
        .addcmul(out, weight.view(affine_param_shape), 1);
  } else if (weight.defined()) {
    return out.mul(weight.view(affine_param_shape));
  } else {
    return out.add(bias.view(affine_param_shape));
  }
}

DEFINE_DISPATCH(GroupNormKernel);
DEFINE_DISPATCH(GroupNormBackwardKernel);

} // namespace native
} // namespace at
