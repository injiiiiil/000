#pragma once

#include <ATen/ATen.h>
#include <torch/csrc/WindowsTorchApiMacro.h> // TORCH_API
#include <torch/csrc/jit/ir.h>

namespace torch {
namespace jit {
namespace fuser {
namespace cpu {

class CPUFusionBackend : public FusionBackend {
public:
  virtual bool isFusible(const Node* const node) override;
  virtual bool isFusible(
      const Node* const fusion,
      const Node* const node) override {return false;};
  virtual int fuse(const Node* const node) override;
  virtual void compileFusion(Node* fusion) override;
  virtual void callFusion(
      const Node* const fusion,
      Stack& stack) override;
};

}}}} // namespace torch::jit::fuser::cpu
