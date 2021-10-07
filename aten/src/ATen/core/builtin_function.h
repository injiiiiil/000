#pragma once

#include <ATen/core/function.h>
#include <ATen/core/ivalue.h>
#include <c10/util/Exception.h>
#include <c10/util/intrusive_ptr.h>
#include <functional>
#include <utility>

namespace torch {
namespace jit {

struct BuiltinOpFunction : public Function {
  BuiltinOpFunction(
      c10::QualifiedName qualname,
      c10::FunctionSchema schema,
      std::function<void(Stack&)> callable,
      std::string doc_string = "")
      : name_(std::move(qualname)),
        callable_(std::move(callable)),
        schema_(std::move(schema)),
        doc_string_(std::move(doc_string)) {
    TORCH_INTERNAL_ASSERT(schema_.returns().size() == 1);
  }

  const std::string& doc_string() const override {
    return doc_string_;
  }

  void run(Stack& stack) override {
    callable_(stack);
  }

  c10::intrusive_ptr<c10::ivalue::Future> runAsync(
      Stack& stack,
      TaskLauncher /* not used */) override {
    run(stack);
    auto res = c10::make_intrusive<c10::ivalue::Future>(stack.front().type());
    res->markCompleted(std::move(stack.front()));
    return res;
  }

  const c10::QualifiedName& qualname() const override {
    return name_;
  }

  // if this isn't yet defined, run its method_creator function
  void ensure_defined() override {
    // nop
  }

  GraphExecutor& get_executor() override {
    TORCH_INTERNAL_ASSERT(false , "BuiltinFunction had a GraphExecutor requested "
      "from it. This probably indicates that the JIT calling context needs a "
      "special case on torch::jit::tryToGraphFunction()");
  }

  const c10::FunctionSchema& getSchema() const override {
    return schema_;
  }

  size_t num_inputs() const override {
    return schema_.arguments().size();
  }

  Function& setSchema(c10::FunctionSchema schema) override {
    schema_ = std::move(schema);
    return *this;
  }

  ~BuiltinOpFunction() override {}

 private:
  c10::QualifiedName name_;

  std::function<void(Stack&)> callable_;

  c10::FunctionSchema schema_;

  std::string doc_string_;
};

} // namespace jit
} // namespace torch
