#pragma once

#include "test/cpp/jit/test_utils.h"
#include "torch/csrc/jit/argument_spec.h"

namespace torch {
namespace jit {
namespace test {

int device(const autograd::Variable& v) {
  return v.type().is_cuda() ? v.get_device() : -1;
}

bool isEqual(at::IntArrayRef lhs, at::IntArrayRef rhs) {
  return lhs.size() == rhs.size() &&
      std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

bool isEqual(const ArgumentInfo& ti, const autograd::Variable& v) {
  if (!ti.defined())
    return ti.defined() == v.defined();
  return ti.device() == device(v) && ti.requires_grad() == v.requires_grad() &&
    ti.type() == v.scalar_type();
}

autograd::Variable var(at::Type& t, at::IntArrayRef sizes, bool requires_grad) {
  return autograd::make_variable(at::rand(sizes, t.options()), requires_grad);
}
autograd::Variable undef() {
  return autograd::Variable();
}

void testArgumentSpec() {
  auto& CF = at::CPU(at::kFloat);
  auto& CD = at::CPU(at::kDouble);
  auto& GF = at::CUDA(at::kFloat);
  auto& GD = at::CUDA(at::kDouble);

  auto graph = std::make_shared<Graph>();
  auto type = TensorType::get();

  // Build up a fake graph
  auto ga = SymbolicVariable::asNewInput(*graph, type);
  auto gb = SymbolicVariable::asNewInput(*graph, type);
  auto gc = SymbolicVariable::asNewInput(*graph, type);
  auto gd = SymbolicVariable::asNewInput(*graph, type);
  auto ge = SymbolicVariable::asNewInput(*graph, type);
  auto gr = ga + gb + gc + gd + ge;
  graph->registerOutput(gr.value());

  ArgumentSpecCreator arg_spec_creator(*graph);

  auto list = createStack({var(CF, {1}, true),
                           var(CD, {1, 2}, false),
                           var(GF, {}, true),
                           var(GD, {4, 5, 6}, false),
                           undef()});

  // make sure we have some non-standard strides
  list[1].toTensor().transpose_(0, 1);

  // same list but different backing values
  auto list2 = createStack({var(CF, {1}, true),
                            var(CD, {1, 2}, false),
                            var(GF, {}, true),
                            var(GD, {4, 5, 6}, false),
                            undef()});
  list2[1].toTensor().transpose_(0, 1);


  ArgumentSpec a = arg_spec_creator.create(true, list);
  ArgumentSpec b = arg_spec_creator.create(true, list);
  ASSERT_EQ(a.hashCode(), b.hashCode());

  ASSERT_EQ(a, b);
  ArgumentSpec d = arg_spec_creator.create(true, list2);
  ASSERT_EQ(d, a);
  ASSERT_EQ(d.hashCode(), a.hashCode());

  for (size_t i = 0; i < list.size(); ++i) {
    ASSERT_TRUE(isEqual(a.at(i), list[i].toTensor()));
  }
  ArgumentSpec no_grad = arg_spec_creator.create(/*with_grad=*/false, list);
  ASSERT_TRUE(no_grad != a);

  std::unordered_set<ArgumentSpec> spec;
  spec.insert(std::move(a));
  ASSERT_TRUE(spec.count(b) > 0);
  ASSERT_EQ(spec.count(no_grad), 0);
  spec.insert(std::move(no_grad));
  ASSERT_EQ(spec.count(arg_spec_creator.create(true, list)), 1);

  list2[1].toTensor().transpose_(0, 1);
  ArgumentSpec c = arg_spec_creator.create(true, list2); // same as list, except for one stride
  ASSERT_FALSE(c == a);
  ASSERT_EQ(spec.count(c), 1);

  /*Stack stack = {var(CF, {1, 2}, true), 3, var(CF, {1, 2}, true)};
  CompleteArgumentSpec with_const(true, stack);
  ASSERT_EQ(with_const.at(2).sizes().size(), 2);*/
}

} // namespace test
} // namespace jit
} // namespace torch
