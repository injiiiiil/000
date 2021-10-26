#include "lazy_tensor_core/csrc/ops/prod.h"

#include "lazy_tensor_core/csrc/compiler/node_lowering.h"
#include "lazy_tensor_core/csrc/reduction.h"
#include "lazy_tensor_core/csrc/tensor_util.h"
#include "lazy_tensor_core/csrc/torch_util.h"

namespace torch_lazy_tensors {
namespace ir {
namespace ops {

Prod::Prod(const torch::lazy::Value& input, std::vector<int64_t> dimensions,
           bool keep_reduced_dimensions, c10::optional<at::ScalarType> dtype)
    : TsNode(torch::lazy::OpKind(at::aten::prod), {input},
           /*num_outputs=*/1,
           torch::lazy::MHash(dimensions, keep_reduced_dimensions,
                                     OptionalOr<int>(dtype, -1))),
      dimensions_(std::move(dimensions)),
      keep_reduced_dimensions_(keep_reduced_dimensions),
      dtype_(dtype) {
  SetShapeDeferred(
      [&]() { return compiler::NodeLowering::Get()->Infer(this); });
}

NodePtr Prod::Clone(OpList operands) const {
  return torch::lazy::MakeNode<Prod>(operands.at(0), dimensions_, keep_reduced_dimensions_,
                        dtype_);
}

std::string Prod::ToString() const {
  std::stringstream ss;
  ss << TsNode::ToString() << ", dimensions=(" << c10::Join(", ", dimensions_)
     << "), keep_reduced_dimensions=" << keep_reduced_dimensions_
     << ", dtype=" << OptionalOr<int>(dtype_, -1);
  return ss.str();
}

}  // namespace ops
}  // namespace ir
}  // namespace torch_lazy_tensors
