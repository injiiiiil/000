#include "lazy_tensor_core/csrc/ops/var.h"

#include "lazy_tensor_core/csrc/compiler/node_lowering.h"
#include "lazy_tensor_core/csrc/helpers.h"
#include "lazy_tensor_core/csrc/reduction.h"
#include "lazy_tensor_core/csrc/tensor_util.h"
#include "lazy_tensor_core/csrc/torch_util.h"
#include "lazy_tensors/computation_client/util.h"

namespace torch_lazy_tensors {
namespace ir {
namespace ops {

Var::Var(const torch::lazy::Value& input, std::vector<int64_t> dimensions,
         int64_t correction, bool keep_reduced_dimensions)
    : TsNode(torch::lazy::OpKind(at::aten::var), {input},
           /*num_outputs=*/1,
           torch::lazy::MHash(dimensions, correction,
                                     keep_reduced_dimensions)),
      dimensions_(std::move(dimensions)),
      correction_(correction),
      keep_reduced_dimensions_(keep_reduced_dimensions) {
  SetShapeDeferred(
      [&]() { return compiler::NodeLowering::Get()->Infer(this); });
}

NodePtr Var::Clone(OpList operands) const {
  return torch::lazy::MakeNode<Var>(operands.at(0), dimensions_, correction_,
                       keep_reduced_dimensions_);
}

std::string Var::ToString() const {
  std::stringstream ss;
  ss << TsNode::ToString() << ", dimensions=(" << c10::Join(", ", dimensions_)
     << "), correction=" << correction_
     << ", keep_reduced_dimensions=" << keep_reduced_dimensions_;
  return ss.str();
}

}  // namespace ops
}  // namespace ir
}  // namespace torch_lazy_tensors
