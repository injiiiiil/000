#pragma once
#include <torch/csrc/jit/fuser/config.h>
#if USE_CUDA_FUSER || USE_CPU_FUSER

#include <torch/csrc/WindowsTorchApiMacro.h>
#include <torch/csrc/jit/fuser/arg_spec.h>
#include <torch/csrc/jit/fuser/partition_desc.h>
#include <torch/csrc/jit/fuser/tensor_desc.h>
#include <torch/csrc/jit/ir.h>

#include <iostream>
#include <string>
#include <tuple>
#include <vector>

namespace torch {
namespace jit {
namespace fuser {

// Creates a CPU or CUDA kernel for the given graph.
// Returns a tuple consisting of the generated code (as a string),
// two vectors of PartitionDescs, the chunk and concat descriptions,
// respectively, and a bool indicating whether the generated code
// generates random numbers.
// TODO: the partition descriptions should be generated by the executor.
TORCH_API std::tuple<
    std::string,
    std::vector<PartitionDesc>,
    std::vector<PartitionDesc>,
    bool>
generateKernel(
    const std::string& name,
    const Graph& graph,
    const std::vector<TensorDesc>& input_desc,
    const std::vector<TensorDesc>& output_desc,
    const bool use_cuda);

} // namespace fuser
} // namespace jit
} // namespace torch

#endif // USE_CUDA_FUSER || USE_CPU_FUSER
