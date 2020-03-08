#pragma once

#include <torch/csrc/jit/fuser/common/ir.h>
#include <torch/csrc/jit/fuser/common/iter_visitor.h>
#include <torch/csrc/jit/fuser/common/iriostream.h>
#include <torch/csrc/jit/fuser/common/ir_printer.h>

#include <c10/util/Exception.h>
#include <torch/csrc/WindowsTorchApiMacro.h>

#include <algorithm>
#include <deque>
#include <iostream>
#include <sstream>
#include <queue>
#include <set>
#include <unordered_map>
#include <vector>

namespace torch {
namespace jit {
namespace fuser {

// TODO: replaceAllUsesOfLeftWithRight(Val, Val) for values
// TODO: hasUses(Val)
// TODO: plumb through helpers, ex. Val.replaceAllUsesWith(Val)
//          (registration makes this easy)
// TODO: DCE pass

/* This file is critical to the lifetime model of the IR system. FusionGuard is
 * a convenient way to set what base container instance holds your IR.
 * Statements that are defined are registered through the FusionGuard with a
 * particular Fusion. You need to create your instances of Fusion and are
 * responsible for holding on to them however, FusionGuard allows you to set
 * this so that constructors of Expr and Vals are automatically registered. If
 * there are other container or derived classes of Statement it is important to
 * make sure it gets registered with Fusion so it is properly cleaned up.
 *
 * Fusion is generally thought of as a translated fusion group from the JIT. It
 * is likely a single kernel, although, we don't have to stick to this in the
 * future and could in theory generate multiple kernels with the logic to
 * execute them.
 *
 * Fusion also allows users to set input/output values that will allow us to
 * figure out how to hook up runtime data to and from the JIT as well as provide
 * us mechanisms for dependency analysis and DCE including safety checks.
 *
 */

struct Fusion;

struct TORCH_API FusionGuard {
 public:
  static thread_local Fusion* cur_fusion;
  Fusion* prev_fusion;

  FusionGuard(Fusion* fusion) {
    prev_fusion = cur_fusion;
    cur_fusion = fusion;
  }

  ~FusionGuard() {
    cur_fusion = prev_fusion;
  }

  static Fusion* getCurFusion() {
    return cur_fusion;
  }
};

struct ExprSort : public IterVisitor{

  std::vector<Expr*> exprs;

  void handle(Expr* expr) override {
    exprs.push_back(expr);
  }

  static std::vector<Expr*> getExprs(Fusion* fusion, bool from_outputs_only, bool breadth_first){
    ExprSort es;
    es.traverse(fusion, from_outputs_only, breadth_first);
    return es.exprs;
  }

};

/*
 * Fusion is mutable but unique. Nodes cannot be copied in any way from one
 * Fusion to another. If anything like that is desired, it would require
 * duplicating all values and exprs. Fusion is considered to contain SSA, though
 * this could also change in the future if there is a good reason to do so.
 *
 * Fusion should provide sorting mechanisms on the Expr's it holds. This could
 * be done in a few ways and we should support as many of them as is convenient.
 * For example we may want to be able to extract out only the expr's that are
 * associated with a particular output, or set of outputs. Fusion should also
 * provide sorted and unsorted iterables of its exprs. This could be either in
 * depth first or breadth first traversal of the dependency chain. Having these
 * sorted lists could make it easy to convert some passes to iteration based
 * instead of recursive, or even allow us to more easily generate a pass that is
 * partially recursive and partially iterative.
 *
 */

struct TORCH_API Fusion : public IRInputOutput {
  Fusion() {}

  // Not copyable
  Fusion(const Fusion& other) = delete;
  Fusion& operator=(const Fusion& other) = delete;

  Fusion(Fusion&& other) = delete;
  Fusion& operator=(Fusion&& other) = delete;

  ~Fusion() {
    for (auto it = val_set_.begin(); it != val_set_.end(); ++it) {
      delete *it;
    }

    for (auto it = expr_set_.begin(); it != expr_set_.end(); ++it) {
      delete *it;
    }
  };

    /*
   * Break dependency chains associated with Expr, remove references to expr
   * delete expr.
   */
  void removeExpr(Expr* expr) {
    assertInFusion(expr, "Cannot remove expr ");
      // If we hit this error too frequently, we could lighten the restrictions so that removing something
      // that doesn't exist simply does nothing. For now, we're going with the strictest model which errors.

    for (auto out : expr->outputs())
      if (origin_.find(out) != origin_.end())
        if (origin_.find(out)->second == expr)
          origin_.erase(out);

    for (auto inp : expr->inputs()) {
      if (uses_.find(inp) != uses_.end()) {
        if (uses_.find(inp)->second.find(expr) !=
            uses_.find(inp)->second.end()) {
          uses_.find(inp)->second.erase(expr);
        }
      }
    }

    expr_set_.erase(expr);

    delete expr;
  }

  /*
   * Completely remove val from the fusion, break all dependencies associated with it.
   */
  void removeVal(Val* val) {
    
    assertInFusion(val, "Cannot remove val ");

    for(Val* inp : inputs())
      if(val->same_as(inp))
        TORCH_CHECK(false,
          "Cannot remove val as it is an input of the fusion.");

    for(Val* out : outputs())
      if(val->same_as(out))
        TORCH_CHECK(false,
          "Cannot remove val as it is an output of the fusion.");

    Expr* orig = origin(val);
    if(orig != nullptr)
      removeExpr(origin(val));

    for(Expr* use : uses(val))
      removeExpr(use);

    val_set_.erase(val);

  }


  // Functions for adding inputs and outputs
  void addInput(Val* const input) {
    assertInFusion(input, "Cannot register input ");
    IRInputOutput::addInput(input);
  }

  void addOutput(Val* const output) {
    assertInFusion(output, "Cannot register output ");
    IRInputOutput::addOutput(output);
  }

  // Functions for querying / enumerating IR objets
  bool inFusion(const Statement* stmt) const {
    bool infusion = stmt->fusion() == this;
    Statement* nonconst_stmt = const_cast<Statement*>(stmt);

    if (stmt->isExpr())
      infusion &=
          expr_set_.find(static_cast<Expr*>(nonconst_stmt)) != expr_set_.end();
    if (stmt->isVal())
      infusion &=
          val_set_.find(static_cast<Val*>(nonconst_stmt)) != val_set_.end();

    return infusion;
  }

  // Functions for querying / enumerating IR objets
  void assertInFusion(const Statement* stmt, std::string msg = "") const {
    if(inFusion(stmt))
      return;
    std::stringstream errmsg;
    errmsg << msg << " it was not found in the active fusion.";
    TORCH_CHECK(false, errmsg.str());
  }


  /*
   * Return list of exprs, can choose to be topologically sorted. We can start by
   * only traversing back from registered outputs, or from any terminating Vals.
   * Can also select depth first traversal, or breadth first.1
   *
   * from_outputs_only : will only sort DAG associated with registered outputs
   * breadth_first : breadth first topological sort
   * TODO: Add more tests.
   * TODO: Implement breadth_first
   */
  std::vector<Expr*> exprs(
      bool from_outputs_only = false,
      bool breadth_first = false) {
    
    if (breadth_first)
      TORCH_INTERNAL_ASSERT(false, "Not implemented yet.");
    return ExprSort::getExprs(this, from_outputs_only, breadth_first);

  }

  void print() {
    std::cout << "%kernel {\n";
    IRMathPrinter op_exprs(std::cout); 
    op_exprs.handle(this); 
    IRTransformPrinter t_exprs(std::cout); 
    t_exprs.handle(this); 
    std::cout << "}\n";
  }

  /*
   * Simple Registration methods. These methods do 2 things:
   * Register the Statment/Val/Expr
   */
  
  StmtNameType registerVal(Val* val) {
    if (val->fusion()) {
      if(val->fusion() != this){
        std::stringstream ss;
        ss << val << " was not found in the active fusion.";
        TORCH_CHECK(false, ss.str());
      }
      if (inFusion(val)) {
        return val->name();
      }
    }
    val_set_.emplace(val);
    return getValName(*(val->getValType()));
  }

  /*
   * When we register an expression, we want to update the dependency tracking
   * of Vals. We add expr to our general expr_set_, we add use tracking for
   * inputs and origin tracking for outputs.
   */
  StmtNameType registerExpr(Expr* expr) {
    if (expr->fusion()) {
      if(expr->fusion() != this){
        std::stringstream ss;
        ss << expr << " was not found in the active fusion.";
        TORCH_CHECK(false, ss.str());
      }
      if (inFusion(expr)) {
        return expr->name();
      }
    }

    for (Val* input : expr->inputs()) {
      registerVal(input);
      if (uses_.find(input) == uses_.end()) {
        uses_[input] = {expr};
      } else {
        uses_.find(input)->second.emplace(expr);
      }
    }

    for (Val* output : expr->outputs()) {
      registerVal(output);
      auto it = origin_.find(output);
      if (it != origin_.end()) {
        removeExpr(it->second); //will also remove origin entry
      }

      origin_[output] = expr;
    }

    expr_set_.emplace(expr);
    return getExprName();
  }

  StmtNameType registerStatement(Statement* stmt) {
    if (inFusion(stmt))
      return stmt->name();

    if (stmt->isVal()) {
      return registerVal(static_cast<Val*>(stmt));
    } else if (stmt->isExpr()) {
      return registerExpr(static_cast<Expr*>(stmt));
    }

    TORCH_INTERNAL_ASSERT(false, "Could not register statement as Fusion could not recognize its type.");
    return UNINITIALIZED_STMTNAMETYPE;
  }

  bool used(Val* val) const {
    assertInFusion(val, "Cannot detect if val was used, ");
    return (uses_.find(val) != uses_.end()) &&
        (uses_.find(val)->second.size() > 0);
  }

  const std::set<Val*>& vals() const noexcept {
    return val_set_;
  }

  const std::set<Expr*>& unordered_exprs() const noexcept {
    return expr_set_;
  }

  const std::set<Expr*>& uses(Val* val) const {
    assertInFusion(val, "Cannot detect where val was used, ");
    if(uses_.find(val) != uses_.end())
      return uses_.find(val)->second;
    return std::move(std::set<Expr*>());
  }

  Expr* origin(Val* val) const {
    assertInFusion(val, "Cannot dettect the origin of val, ");
    auto it = origin_.find(val);

    if (it == origin_.end())
      return nullptr;

    return it->second;
  }

  const Expr* origin(const Val* val) const {
    assertInFusion(val, "Cannot dettect the origin of val, ");
    auto it = origin_.find(const_cast<Val*>(val));
    if( it == origin_.end())
      return nullptr;
    return it->second;  
  }
 private:
  std::set<Val*> val_set_;
  std::set<Expr*> expr_set_;

  //map from valtype to individual name counters
  std::unordered_map<ValType, StmtNameType> val_type_name_map = {
    {ValType::Tensor,       0},
    {ValType::TensorView,   0},
    {ValType::TensorDomain, 0},
    {ValType::IterDomain,   0},
    {ValType::Scalar,       0}
  };

  //Generic counters
  StmtNameType val_name_counter_ = 0;
  StmtNameType expr_name_counter_ = 0;

  StmtNameType getValName(ValType vtype) {
    if(val_type_name_map.find(vtype) != val_type_name_map.end())
      return val_type_name_map[vtype]++;
    return val_name_counter_++;
  }
  StmtNameType getExprName() {
    return expr_name_counter_++;
  }

  // Dependency tracking for Vals. Where did it come from? Where is it used?
  std::unordered_map<Val*, Expr*> origin_;
  std::unordered_map<Val*, std::set<Expr*>> uses_;
};

} // namespace fuser
} // namespace jit
} // namespace torch
