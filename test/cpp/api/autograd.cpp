#include <gtest/gtest.h>

#include <torch/autograd.h>

#include <torch/utils.h>
#include <test/cpp/api/support.h>

using namespace torch::autograd;

#define ASSERT_VARIABLE_EQ(a,b) ASSERT_TRUE(torch::allclose((a),(b)))
#define EXPECT_VARIABLE_EQ(a,b) EXPECT_TRUE(torch::allclose((a),(b)))

std::string graph_desc(std::shared_ptr<Node> node) {
  if (!node) {
    return "None";
  }
  auto result = node->name() + "(";
  auto next_edges = node->next_edges();
  for(auto& edge : next_edges) {
    result += graph_desc(edge.function);
  }
  return result+")";
}

TEST(CustomAutogradTest, CustomFunction) {
  struct MyFunction : public Function<MyFunction> {
    static variable_list forward(AutogradContext *ctx, Variable var1, int mul, Variable var2) {
      ctx->saved_data["mul"] = mul;
      ctx->save_for_backward({var1, var2});
      return {var1 + mul*var2 + var1*var2};
    }

    static variable_list backward(AutogradContext *ctx, variable_list grad_output) {
      int mul = ctx->saved_data["mul"].toInt();
      auto saved = ctx->get_saved_variables();
      auto var1 = saved[0];
      auto var2 = saved[1];
      variable_list output = {grad_output[0] + grad_output[0]*var2, Variable(), grad_output[0] * mul + grad_output[0] * var1};
      return output;
    }
  };

  Variable x = torch::randn({5,5}, torch::requires_grad());
  Variable y = torch::randn({5,5}, torch::requires_grad());
  auto res = MyFunction::apply(x,2,y)[0];
  auto go = torch::ones({}, torch::requires_grad());
  res.sum().backward(go, false, true);

  ASSERT_VARIABLE_EQ(x.grad(), y + torch::ones({5,5}));
  ASSERT_VARIABLE_EQ(y.grad(), x + torch::ones({5,5})*2);
}

TEST(CustomAutogradTest, FunctionReturnsInput) {
  struct MyFunction : public Function<MyFunction> {
    static variable_list forward(AutogradContext *ctx, Variable var1) {
      return {var1};
    }

    static variable_list backward(AutogradContext *ctx, variable_list grad_output) {
      return {grad_output[0]*2};
    }
  };

  Variable x(torch::ones(1, torch::requires_grad()));
  MyFunction::apply(x)[0].backward(torch::ones(1) , true, true);
  ASSERT_VARIABLE_EQ(x.grad(), torch::full(1,2));
}

TEST(CustomAutogradTest, NoGradCustomFunction) {
  // Custom Function should respect grad mode
 struct MyOp : public Function<MyOp> {
   static variable_list forward(AutogradContext *ctx, Variable x) {
     return {x+1};
   }

   static variable_list backward(AutogradContext *ctx, variable_list dy) {
     return dy;
   }
 };

 auto x = torch::ones({5,5}, torch::requires_grad());
 {
    at::NoGradGuard no_grad;
    auto y = MyOp::apply(x)[0];
    ASSERT_FALSE(y.requires_grad());
 }
}

TEST(CustomAutogradTest, MarkNonDifferentiable) {
  struct MyFunction : public Function<MyFunction> {
    static variable_list forward(AutogradContext *ctx, Variable v) {
      Variable output = v > 0;
      ctx->mark_non_differentiable({output});
      return {output};
    }

    static variable_list backward(AutogradContext *ctx, variable_list grad_output) {
      return { (grad_output[0]*0.0) };
    }
  };

  auto x = torch::randn({5,5}, torch::requires_grad());
  auto mask = MyFunction::apply(x)[0];
  ASSERT_FALSE(mask.requires_grad());
  auto y = x.masked_fill(mask, 0);
  y.sum().backward();
}

TEST(CustomAutogradTest, MarkNonDifferentiableMixed) {
  struct MyFunction : public Function<MyFunction> {
    static variable_list forward(AutogradContext *ctx, Variable input) {
      Variable a = input+1;
      Variable b = input+2;
      ctx->mark_non_differentiable({a});
      return {a,b};
    }

    static variable_list backward(AutogradContext *ctx, variable_list grad_output) {
      Variable &grad_a = grad_output[0], &grad_b = grad_output[1];
      EXPECT_VARIABLE_EQ(grad_a, torch::zeros({5,5}));
      EXPECT_VARIABLE_EQ(grad_b, torch::ones({5,5}));
      return {grad_b};
    }
  };

  auto x = torch::randn({5,5}, torch::requires_grad());
  auto out = MyFunction::apply(x);

  ASSERT_FALSE(out[0].requires_grad());
  ASSERT_TRUE(out[1].requires_grad());
  out[1].sum().backward();
  ASSERT_VARIABLE_EQ(x.grad(), torch::ones({5,5}));
}

TEST(CustomAutogradTest, MarkNonDifferentiableNone) {
  struct MyFunction : public Function<MyFunction> {
    static variable_list forward(AutogradContext *ctx, Variable input) {
      auto output = input.clone();
      ctx->mark_non_differentiable({output});
      return {output};
    }

    static variable_list backward(AutogradContext *ctx, variable_list grad_outputs) {
      return {};
    }
  };

  auto x = torch::randn({5,5}, torch::requires_grad());
  auto r = MyFunction::apply(x * x)[0];
  (r * x).sum().backward();
}

TEST(CustomAutogradTest, ReturnLeafInplace) {
  struct Inplace : public Function<Inplace> {
    static variable_list forward(AutogradContext *ctx, Variable a, Variable b) {
      ctx->mark_dirty({a});
      return {a.add_(b), b+2};
    }

    static variable_list backward(AutogradContext *ctx, variable_list grad_output) {
      return {grad_output[0], grad_output[0] + grad_output[1]};
    }
  };

  Variable x = torch::randn({5,5});
  Variable y = torch::randn({5,5}, torch::requires_grad());

  auto out = Inplace::apply(x,y);
  auto &q = out[0];
  ASSERT_TRUE(torch::equal(q, x));
  ASSERT_TRUE(q.requires_grad());
  q.sum().backward();
  ASSERT_VARIABLE_EQ(y.grad(), torch::ones({5,5}));
}

TEST(CustomAutogradTest, ReturnDuplicateInplace) {
  struct DoubleInplace : public Function<DoubleInplace> {
    static variable_list forward(AutogradContext *ctx, Variable x) {
      x.mul_(2);
      ctx->mark_dirty({x});
      return {x,x};
    }

    static variable_list backward(AutogradContext *ctsx, variable_list grad_outputs) {
      return {grad_outputs[0]*2 + grad_outputs[1]*2};
    }
  };

  auto x = torch::randn({5,5}, torch::requires_grad());

  ASSERT_THROWS_WITH(DoubleInplace::apply(x), "leaf Variable that requires grad");
  // TODO ASSERT_THROWS_WITH(DoubleInplace::apply(x.clone()[0]), "only one output");

  auto out = DoubleInplace::apply(x.clone());
  ASSERT_TRUE(torch::equal(out[0],out[1]));
}

TEST(CustomAutogradTest, ReturnDuplicate) {
  struct DoubleDuplicate : public Function<DoubleDuplicate> {
    static variable_list forward(AutogradContext *ctx, Variable x) {
      auto output = x*2;
      return {output, output};
    }

    static variable_list backward(AutogradContext *ctx, variable_list grad_outputs) {
      return {grad_outputs[0]*2 + grad_outputs[1]*2};
    }
  };

  auto x = torch::randn({5,5}, torch::requires_grad());
  auto out = DoubleDuplicate::apply(x);
  ASSERT_TRUE(torch::equal(out[0],out[1]));
}

TEST(CustomAutogradTest, InvalidGradients) {
  struct MyFunction : public Function<MyFunction> {
    static variable_list forward(AutogradContext *ctx, Variable x) {
      return {x*2};
    }

    static variable_list backward(AutogradContext *ctsx, variable_list grad_outputs) {
      return {torch::randn(10, torch::dtype(torch::kFloat).requires_grad(true))};
    }
  };

  auto input1 = torch::randn({5,5}, torch::dtype(torch::kFloat).requires_grad(true));
  ASSERT_THROWS_WITH(
    MyFunction::apply(input1)[0].sum().backward(), "expected shape");
  auto input2 = torch::randn(10, torch::dtype(torch::kDouble).requires_grad(true));
  ASSERT_THROWS_WITH(
    MyFunction::apply(input2)[0].sum().backward(), "expected type");
}

TEST(CustomAutogradTest, NoGradInput) {
  struct MyFunction : public Function<MyFunction> {
    static variable_list forward(AutogradContext*, Variable x) {
      return {x};
    }

    static variable_list backward(AutogradContext*, variable_list grad_outputs) {
      return grad_outputs;
    }
  };

  Variable x = torch::randn({5,5}, torch::requires_grad());
  Variable y;
  {
    at::NoGradGuard no_grad;
    y = MyFunction::apply(x)[0];
  }

  ASSERT_TRUE(x.requires_grad());
  ASSERT_FALSE(y.grad_fn());
}

TEST(CustomAutogradTest, TooManyGrads) {
  struct MyFunction : public Function<MyFunction> {
    static variable_list forward(AutogradContext*, Variable input) {
      return {input};
    }

    static variable_list backward(AutogradContext*, variable_list grad_output) {
      grad_output.insert(grad_output.end(), {Variable(), Variable()});
      return grad_output;
    }
  };
}

TEST(CustomAutogeradTest, DepNoGrad) {
  struct F1 : public Function<F1> {
    static variable_list forward(AutogradContext *ctx, Variable input) {
      auto out = torch::randn(input.sizes());
      ctx->mark_non_differentiable({out});
      return {input, out};
    }

    static variable_list backward(AutogradContext *ctx, variable_list grad_output) {
      return {grad_output[0]};
    }
  };

  struct F2 : public Function<F2> {
    static variable_list forward(AutogradContext*, Variable input, Variable ignore) {
      return {input};
    }

    static variable_list backward(AutogradContext*, variable_list grad_output) {
      return {grad_output[0], Variable()};
    }
  };

  auto x = torch::randn(5, torch::requires_grad());
  auto out = F1::apply(x);
  Variable &a = out[0], &b = out[1];
  b = b+1; // Separate F1 and F2 by another operation
  ASSERT_TRUE(a.requires_grad());
  ASSERT_FALSE(b.requires_grad());

  auto c = F2::apply(a,b)[0];
  c.backward(torch::ones(c.sizes()), false, false);
  ASSERT_VARIABLE_EQ(x.grad(), torch::ones(x.sizes()));
}

TEST(CustomAutogradTest, Reentrant) {
  static Variable y_data = torch::randn({2, 2});
  struct Reenter : public Function<Reenter> {
    static variable_list forward(AutogradContext *ctx, Variable input) {
      Variable output;
      {
        at::AutoGradMode enable_grad(true);
        auto x = make_variable(input.tensor_data(), true);
        auto y = make_variable(y_data.tensor_data(), true);
        output = x*y;

        ctx->saved_data["x"] = x;
        ctx->saved_data["y"] = y;
        ctx->saved_data["output_var"] = output;
      }
      return {output.detach()};
    }

    static variable_list backward(AutogradContext *ctx, variable_list grad_output) {
      {
        at::AutoGradMode enable_grad(true);
        auto out = ctx->saved_data["output_var"].toTensor();
        out.sum().backward();
      }
      return {ctx->saved_data["x"].toTensor().grad() * grad_output[0]};
    }
  };

  auto x = torch::randn({2,2}, torch::requires_grad());
  auto out = Reenter::apply(x)[0];
  out.sum().backward();
  ASSERT_VARIABLE_EQ(x.grad(), y_data);
}

// TODO Figure out how much many nested reentrant calls it takes to fail
// if threads aren't switched.
TEST(CustomAutogradest, DISABLED_DeepReentrant) {
  struct DeepReenter : public Function<DeepReenter> {
    static variable_list forward(AutogradContext *ctx, Variable x) {
      {
        at::AutoGradMode enable_grad(true);
        ctx->saved_data["x"] = make_variable(x.tensor_data(), true) -1;
      }
      return {ctx->saved_data["x"].toTensor().detach()};
    }

    static variable_list backward(AutogradContext*ctx, variable_list grad_output) {
      if (!ctx->saved_data["x"].toTensor().is_nonzero()) {
        return grad_output;
      }
      {
        at::AutoGradMode enable_grad(true);
        apply(ctx->saved_data["x"].toTensor())[0].sum().backward();
        return grad_output;
      }
    }
  };

  // This should not stack overflow
  auto v = torch::tensor(10000, torch::requires_grad());
  DeepReenter::apply(v)[0].sum().backward();
}

TEST(CustomAutograd, ReentrantPriority) {
  static std::vector<int> order;

  struct MyFunction : public Function<MyFunction> {
    static variable_list forward(AutogradContext*, Variable x) {
      return {x};
    }

    static variable_list backward(AutogradContext*, variable_list grad) {
      order.push_back(0);
      return grad;
    }
  };

  struct Reenter : public Function<Reenter> {
    static variable_list forward(AutogradContext *ctx, Variable x) {
      {
        at::AutoGradMode enable_grad(true);
        ctx->saved_data["x"] = make_variable(x.tensor_data(), true) -1;
      }
      return {ctx->saved_data["x"].toTensor().detach()};
    }

    static variable_list backward(AutogradContext*ctx, variable_list grad_output) {
      order.push_back(1);
      if (!ctx->saved_data["x"].toTensor().is_nonzero()) {
        return grad_output;
      }
      {
        at::AutoGradMode enable_grad(true);
        apply(ctx->saved_data["x"].toTensor())[0].sum().backward();
        return grad_output;
      }
    }
  };

  auto a = MyFunction::apply(torch::tensor(6, torch::requires_grad()))[0];
  auto b = Reenter::apply(torch::tensor(9, torch::requires_grad()))[0];
  auto v = a*b;
  v.backward();

  // All the reentrant tasks should be prioritized over the MyFunction backward
  // task.
  ASSERT_EQ(order.size(), 10);
  ASSERT_EQ(std::count(order.begin(), order.end(), 1), 9);
  ASSERT_EQ(order.back(), 0);
}

// TODO add these tests if needed
// test_once_differentiable
// test_sparse_backward
// test_save_output_nr
// test_free_deep_graph_pyfunction
// test_naughty_anomaly_access
// test_naughty_autograd-function_stashing_ctx
// test_custom_autograd_repeated_grad_grad
// test_return_leaf
// test_anomaly_detect_nan
// test_no_grad_copy
