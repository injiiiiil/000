#include <ATen/ATen.h>
#include <ATen/NativeFunctions.h>

namespace at {
namespace native {

Tensor cov(
    const Tensor& self,
    int64_t correction,
    const c10::optional<Tensor>& fweights,
    const c10::optional<Tensor>& aweights) {
  constexpr int64_t OBSERVATIONS_DIM = 1;

  TORCH_CHECK(
      self.ndimension() <= 2,
      "cov(): expected input to have two or fewer dimensions but got an input with ",
      self.ndimension(),
      " dimensions");

  TORCH_CHECK(
      self.scalar_type() != kBool, "cov(): bool dtype is not supported for input");

  // View input tensor as 2D (variables, observations)
  auto in = self.ndimension() < 2 ? self.view({1, -1}) : self;
  const auto num_observations = in.size(OBSERVATIONS_DIM);

  // The product of frequencies (fweights) and weights (aweights).
  Tensor w;

  if (fweights.has_value()) {
    w = fweights.value();
    TORCH_CHECK(
        w.ndimension() <= 1,
        "cov(): expected fweights to have one or fewer dimensions but got fweights with ",
        w.ndimension(),
        " dimensions");
    TORCH_CHECK(
        at::isIntegralType(w.scalar_type(), false),
        "cov(): expected fweights to have integral dtype but got fweights with ",
        w.scalar_type(),
        " dtype");
    TORCH_CHECK(
        w.numel() == num_observations,
        "cov(): expected fweights to have the same numel as there are observations in the input but got ",
        w.numel(),
        " != ",
        num_observations);
    TORCH_CHECK(
        num_observations == 0 || w.min().ge(0).item<bool>(),
        "cov(): fweights cannot be negative");
  }

  if (aweights.has_value()) {
    const auto& aw = aweights.value();
    TORCH_CHECK(
        aw.ndimension() <= 1,
        "cov(): expected aweights to have one or fewer dimensions but got aweights with ",
        aw.ndimension(),
        " dimensions");
    TORCH_CHECK(
        at::isFloatingType(aw.scalar_type()),
        "cov(): expected aweights to have floating point dtype but got aweights with ",
        aw.scalar_type(),
        " dtype");
    TORCH_CHECK(
        aw.numel() == num_observations,
        "cov(): expected aweights to have the same numel as there are observations in the input but got ",
        aw.numel(),
        " != ",
        num_observations);
    TORCH_CHECK(
        num_observations == 0 || aw.min().ge(0).item<bool>(),
        "cov(): aweights cannot be negative");
    w = w.defined() ? w * aw : aw;
  }

  // Compute a weighted average of the observations
  const auto w_sum = w.defined()
      ? w.sum()
      : at::scalar_tensor(num_observations, in.options().dtype(kLong));

  TORCH_CHECK(
      !w.defined() || w_sum.ne(0).item<bool>(),
      "cov(): weights sum to zero, can't be normalized");

  const auto avg = (w.defined() ? in * w : in).sum(OBSERVATIONS_DIM) / w_sum;

  // Compute the normalization factor
  Tensor norm_factor;

  if (w.defined() && aweights.has_value() && correction != 0) {
    norm_factor = w_sum - correction * (w * aweights.value()).sum() / w_sum;
  } else {
    norm_factor = w_sum - correction;
  }

  if (norm_factor.le(0).item<bool>()) {
    TORCH_WARN("cov(): degrees of freedom is <= 0");
    norm_factor.zero_();
  }

  // Compute covariance matrix
  in = in - avg.unsqueeze(1);
  const auto c = at::mm(in, (w.defined() ? in * w : in).t().conj());
  return at::true_divide(c, norm_factor).squeeze();
}

} // namespace native
} // namespace at
