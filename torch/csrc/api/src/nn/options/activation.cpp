#include <torch/nn/options/activation.h>

namespace torch {
namespace nn {

HardshrinkOptions::HardshrinkOptions(double lambda) : lambda_(lambda) {}

SoftmaxOptions::SoftmaxOptions(int64_t dim) : dim_(dim) {}

SoftminOptions::SoftminOptions(int64_t dim) : dim_(dim) {}

} // namespace nn
} // namespace torch
