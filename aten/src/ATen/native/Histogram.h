#pragma once

#include <ATen/ATen.h>
#include <ATen/native/DispatchStub.h>

#include <tuple>

namespace at { namespace native {

using histogram_fn = void(*)(const Tensor&, const c10::optional<Tensor>&, bool, Tensor&, Tensor&);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
DECLARE_DISPATCH(histogram_fn, histogram_stub);

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
DECLARE_DISPATCH(histogram_fn, histogram_linear_stub);

}} // namespace at::native
