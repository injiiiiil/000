#pragma once

#include <c10/core/ScalarType.h>
#include <c10/util/Optional.h>

#include "lazy_tensor_core/csrc/ts_backend/TsNode.h"

namespace torch_lazy_tensors {
namespace ir {
namespace ops {

class CumProd : public TsNode {
 public:
  CumProd(const torch::lazy::Value& input, int64_t dim,
          c10::optional<at::ScalarType> dtype);

  std::string ToString() const override;

  NodePtr Clone(OpList operands) const override;

  int64_t dim() const { return dim_; }

  const c10::optional<at::ScalarType>& dtype() const { return dtype_; }

 private:
  int64_t dim_;
  c10::optional<at::ScalarType> dtype_;
};

}  // namespace ops
}  // namespace ir
}  // namespace torch_lazy_tensors
