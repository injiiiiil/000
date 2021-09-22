#pragma once

#include <torch/csrc/WindowsTorchApiMacro.h>

#include <torch/csrc/jit/codegen/cuda/ir_interface_nodes.h>
#include <torch/csrc/jit/codegen/cuda/type.h>

//
// The operations defined in this header is intended as user facing functions.
// The user will provide the necessary input TensorViews and the function will
// create the correct intermediate nodes and return the output TensorViews.
//

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {

struct ForwardNormResult {
  TensorView* output = nullptr;
  TensorView* mean = nullptr;
  TensorView* invstd = nullptr;
};

struct BackwardNormResult {
  TensorView* grad_input = nullptr;
  TensorView* grad_weight = nullptr;
  TensorView* grad_bias = nullptr;
};

TORCH_CUDA_CU_API TensorView* softmax(TensorView* x, int dim);

TORCH_CUDA_CU_API TensorView* softmax_backward(
    TensorView* dy,
    TensorView* y,
    const int dim,
    TensorView* x);

TORCH_CUDA_CU_API ForwardNormResult layer_norm(
    TensorView* x,
    const std::vector<int64_t>& norm_shape,
    TensorView* weight,
    TensorView* bias,
    Val* eps);

TORCH_CUDA_CU_API ForwardNormResult layer_norm(
    TensorView* x,
    const size_t kNormShapeNumDims,
    TensorView* weight,
    TensorView* bias,
    Val* eps);

TORCH_CUDA_CU_API BackwardNormResult layer_norm_backward(
    TensorView* dy,
    TensorView* x,
    const std::vector<int64_t>& norm_shape,
    TensorView* mean,
    TensorView* rstd,
    TensorView* weight,
    TensorView* bias,
    const std::vector<bool>& output_mask);

TORCH_CUDA_CU_API ForwardNormResult batch_norm(
    TensorView* x,
    TensorView* weight,
    TensorView* bias,
    TensorView* running_mean,
    TensorView* running_var,
    const bool kTraining,
    Val* momentum,
    Val* eps);

TORCH_CUDA_CU_API BackwardNormResult batch_norm_backward(
    TensorView* x,
    TensorView* dy,
    TensorView* weight,
    TensorView* running_mean,
    TensorView* running_var,
    TensorView* save_mean,
    TensorView* save_invstd,
    const bool kTraining,
    Val* eps,
    const std::vector<bool>& output_mask);

TORCH_CUDA_CU_API ForwardNormResult instance_norm(
    TensorView* x,
    TensorView* weight,
    TensorView* bias,
    TensorView* running_mean,
    TensorView* running_var,
    const bool kUseInputStats,
    Val* momentum,
    Val* eps);

} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
