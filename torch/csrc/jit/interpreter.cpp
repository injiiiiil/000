#include <torch/csrc/jit/interpreter.h>

#include <ATen/Parallel.h>
#include <ATen/core/ivalue.h>
#include <c10/core/thread_pool.h>
#include <c10/util/Exception.h>
#include <torch/csrc/autograd/edge.h>
#include <torch/csrc/autograd/grad_mode.h>
#include <torch/csrc/autograd/variable.h>
#include <torch/csrc/jit/constants.h>
#include <torch/csrc/jit/graph_executor.h>
#include <torch/csrc/jit/ir.h>
#include <torch/csrc/jit/operator.h>
#include <torch/csrc/jit/script/jit_exception.h>

#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace torch {
namespace jit {

// Before we translate to intepreter instructions, we do
// some preprocessing of the graph to turn it into a form that is closer
// to what the instructions will look like.
// In particular we:
// *  Computes whether a input to a node is the last use, so we can issue MOVE
//    rather than LOAD instructions.
// *  Drop nodes are inserted for any node that is unused to create a dummy use
//    that will cause the interpreter to free the node.
//    A drop node just pops its input off the stack to  ensure the interpreter
//    releases references to nodes that are never used. Drop nodes are also
//    inserted when the last use of a node is in some conditionally run control
//    flow (e.g. one side of an If) and the interpreter must free the node only
//    after the control flow has reconverged
// Outputs are:
// * graph - the post processed copy of g
// * move_flags[n] - a list of booleans, one for each input,
//   indicating whether this is the last use of the value. The interpreter
//   should generate a move rather than a copy in this case.

namespace {

// insert Drop nodes to kill references for anything unused:
// this can happen in a few places, e.g. when a node returns
// many values but only one is used
// a, b = foo()
// return a
void dropUnused(Block* b) {
  auto createDropIfUnused = [&](ArrayRef<Value*> values) -> Node* {
    std::vector<Value*> to_drop;
    for (auto v : values) {
      if (v->uses().size() == 0 && v->node()->kind() != prim::Constant)
        to_drop.push_back(v);
    }
    if (to_drop.size() == 0)
      return nullptr;
    return b->owningGraph()->create(prim::Drop, to_drop, 0);
  };

  if (auto d = createDropIfUnused(b->inputs())) {
    b->prependNode(d);
  }
  for (auto n : b->nodes()) {
    if (auto d = createDropIfUnused(n->outputs())) {
      d->insertAfter(n);
    }
    for (auto b : n->blocks())
      dropUnused(b);
  }
}

// for each input, should we move rather than copy the inputs
std::unordered_map<Node*, std::vector<uint8_t>> findLastUses(Graph& g) {
  // struct to share common data structures
  struct FindLastUses {
    Graph& graph;
    // have we seen this value, yet, if not, it is the last use of the value
    std::unordered_set<Value*> seen;

    std::unordered_map<Node*, std::vector<uint8_t>> move_flags;
    // A map from an If or Loop node to the optional Drop block that
    // occurs directly after it to release any tensors that go out of scope
    // when the If/Loop exits. These are created and inserted on demand.
    std::unordered_map<Node*, Node*> drop_for_node;

    FindLastUses(Graph& g) : graph(g) {
      scanBlock(graph.block());
    }
    void scanBlock(Block* b) {
      scanNode(b->return_node());
      for (auto n : b->nodes().reverse()) {
        scanNode(n);
      }
    }
    void scanNode(Node* n) {
      for (auto b : n->blocks()) {
        scanBlock(b);
      }
      move_flags[n].resize(n->inputs().size());
      // scan backwards so if a value is used twice in the list then it is a
      // move
      for (size_t i = n->inputs().size(); i > 0; --i) {
        scanUse(n, i - 1);
      }
    }
    void scanUse(Node* n, size_t i) {
      auto& move_flags_n = move_flags[n];
      auto v = n->inputs()[i];
      auto inserted = seen.insert(v).second;
      if (!inserted) {
        move_flags_n[i] = false;
        return;
      }

      // the last use of v may be in a nested block of an If or Loop statement
      // find the node 'same_depth_node' at the same depth as the definition of
      // v, and consider that node to be the last use of v. This ensures we do
      // not delete nodes in nested scopes that may be executed multiple times
      // and that nodes used on one side of an if
      // but not the other get deleted regardless of the branch
      // e.g.
      // a = 4
      // while <...>:
      //   y = a + a
      // drop(a)
      // In other words, we find the first program point for v that
      // _reverse_ dominates the definition of v, and add a drop point there.
      Node* same_depth_node = findOwnerInBlock(n, v->node()->owningBlock());
      AT_ASSERT(
          same_depth_node); // failure means v is not in scope for n, use lint!

      // In the case where v and n are in the same block, just mark
      // its move_flags to be true
      if (same_depth_node == n) {
        move_flags_n[i] = true;
        return;
      }

      // in the case where the use is nested in a block
      // add a Drop node after that block which will drop 'v'.
      move_flags_n[i] = false;
      addToDropIfNotExists(
          findOrCreateDropInstructionForNode(same_depth_node), v);
    }

    // finds the node in block 'block' that contains in 'n'
    // or nullptr if no such node exists, e.g.:
    // n0: a = 4
    // n1: if <cond>:
    // n2:    b = a + a
    // findOwnerInBlock(n2, n0.block()) == n1
    Node* findOwnerInBlock(Node* n, Block* block) {
      while (n != nullptr && block != n->owningBlock()) {
        n = n->owningBlock()->owningNode();
      }
      return n;
    }

    Node* findOrCreateDropInstructionForNode(Node* n) {
      auto it = drop_for_node.find(n);
      if (it == drop_for_node.end()) {
        auto drop_node = graph.create(prim::Drop, 0);
        drop_node->insertAfter(n);
        it = drop_for_node.emplace(n, drop_node).first;
      }
      return it->second;
    }

    void addToDropIfNotExists(Node* drop, Value* v) {
      if (v->node()->kind() == prim::Constant) {
        return;
      }
      for (auto i : drop->inputs()) {
        // we already accounted for this use
        if (i == v)
          return;
      }
      drop->addInput(v);
      move_flags[drop].push_back(true);
    }
  };

  return FindLastUses(g).move_flags;
}
} // namespace

// pre-processing that happens once per graph
struct PreprocessGraph {
  PreprocessGraph(Graph& g) : graph(g.copy()) {
    dropUnused(graph->block());
    // fill in move_flags by scanning blocks;
    move_flags = findLastUses(*graph);
  }
  // Outputs of the preprocessing:
  std::shared_ptr<Graph> graph;
  // for each input, should we move rather than copy the inputs
  std::unordered_map<Node*, std::vector<uint8_t>> move_flags;
};

// instructs look like:
// op_code X, N
// meaning of X, N depend on the op:
// O - index into operator table
// R - index into register table
// I - literal integer
// C - index into constant table
// P - jump offset relative to beginning of current instruction

#define FORALL_OPCODES(_)                                                   \
  _(OP, "O") /* invoke operator X */                                        \
  _(LOAD, "R") /* push a value from a register X */                         \
  _(MOVE, "R") /* push a value from register X, clearing the register */    \
  _(STOREN, "RI") /* store N values to registers [X, X+N) */                \
  _(STORE, "R") /* store 1 value to registers X */                          \
  _(DROP, "") /* drop 1 value from the top of the stack */                  \
  _(DROPR, "R") /* clear register X */                                      \
  _(LOADC, "C") /* push the constant X */                                   \
  _(JF, "P") /* pop the top of the stack, if false, branch to P */          \
  _(JMP, "P") /* unconditional branch to X */                               \
  _(LOOP, "PI") /* perform a loop, X is where to branch if cond is false */ \
  _(RET, "") /* exit execution */                                           \
  _(WAIT, "") /* wait for a future to be complete */

enum OpCode : uint8_t {
#define DEFINE_OP(op, _) op,
  FORALL_OPCODES(DEFINE_OP)
#undef DEFINE_OP
};

std::ostream& operator<<(std::ostream& out, OpCode op) {
  switch (op) {
#define OP_STRING(x, _) \
  case x:               \
    return out << #x;
    FORALL_OPCODES(OP_STRING)
#undef OP_STRING
  }
  return out;
}

const char* OpInfo(OpCode op) {
  switch (op) {
#define OP_INFO(x, info) \
  case x:                \
    return info;
    FORALL_OPCODES(OP_INFO)
#undef OP_INFO
  }
  return nullptr;
}

struct Instruction {
  OpCode op;
  uint8_t padding; // currently unused
  uint16_t N;
  int32_t X;
  // TODO: check for overflow
  Instruction(OpCode op, int32_t X, uint16_t N)
      : op(op), padding(0), N(N), X(X) {}
};

static_assert(sizeof(Instruction) == 8, "Instructions should be 8 bytes");
std::ostream& operator<<(std::ostream& out, Instruction inst) {
  // TODO: use op info to print out the op in a more user-friendly way
  int nargs = strlen(OpInfo(inst.op));
  out << inst.op;
  if (nargs > 0) {
    out << " " << inst.X;
  }
  if (nargs > 1) {
    out << " " << inst.N;
  }
  return out;
}

// for keeping track of the current node
struct WithCurrentNode {
  WithCurrentNode(Node** loc, Node* new_value) : loc_(loc), old_value_(*loc_) {
    *loc = new_value;
  }
  ~WithCurrentNode() {
    *loc_ = old_value_;
  }

 private:
  Node** loc_;
  Node* old_value_;
};

struct CodeImpl {
  friend struct InterpreterState;
  std::vector<Instruction> instructions_;

  // same length as instructions.
  // what node in the graph cause this
  // instruction to be emitted?
  std::vector<Node*> instructions_source_;

  std::vector<IValue> constant_table_;
  std::vector<Operation> operator_table_;
  int register_size_ = 0;
  size_t n_outputs;
  size_t n_inputs;

  // We MUST hold onto graph here because some Operators stored in the
  // instruction lists have dependencies on meta-data stored in the graph
  // that would be dead otherwise.
  // It is also very useful for debugging interpreter problems to
  // keep this around.
  std::shared_ptr<Graph> graph_;
  c10::optional<std::vector<GraphExecutor*>> grad_executors_;
  PreprocessGraph preprocess_;

  std::unordered_map<Value*, int>
      value_to_reg_; // map from unique of nodes to register in register table
  Node* current_node_; // used in creation of code to keep track
                       // of node being emitted

  CodeImpl(const std::shared_ptr<Graph>& graph)
      : preprocess_(*graph), current_node_(preprocess_.graph->return_node()) {
    graph_ = preprocess_.graph;
    n_outputs = graph_->outputs().size();
    n_inputs = graph_->inputs().size();
    // std::cout << *graph_ << "\n";
    emitCodeForBlock(graph_->block());
    insertInstruction(RET);
  }

  void insertInstruction(OpCode op, int64_t X = 0, uint64_t N = 0) {
    instructions_.emplace_back(op, X, N);
    instructions_source_.emplace_back(current_node_);
  }

  int allocRegs(at::ArrayRef<Value*> vs) {
    int result = register_size_;
    for (Value* v : vs) {
      AT_ASSERT(value_to_reg_.count(v) == 0);
      value_to_reg_[v] = register_size_++;
    }
    return result;
  }

  int registerFor(Value* v) {
    return value_to_reg_.at(v);
  }

  void emitLoadInputs(Node* node) {
    auto move_flag_it = preprocess_.move_flags.at(node).begin();
    for (Value* input : node->inputs()) {
      int reg = registerFor(input);
      bool moved = (*move_flag_it++);
      OpCode op;
      if (input->node()->kind() == prim::Constant) {
        op = LOADC;
      } else if (moved) {
        op = MOVE;
      } else {
        op = LOAD;
      }
      insertInstruction(op, reg);
    }
  }

  void emitOperator(Node* node) {
    emitLoadInputs(node);
    insertInstruction(OP, operator_table_.size());
    operator_table_.emplace_back(getOperation(node));
    emitStoreOutputs(node);
  }

  void emitWait(Node* node) {
    emitLoadInputs(node);
    insertInstruction(WAIT);
    emitStoreOutputs(node);
  }

  void emitDrop(at::ArrayRef<Value*> to_drop) {
    for (Value* input : to_drop) {
      insertInstruction(DROPR, registerFor(input));
    }
  }

  void emitStoreOutputs(Node* node) {
    size_t N = node->outputs().size();
    if (N == 0)
      return;
    int regs = allocRegs(node->outputs());
    if (N == 1) {
      insertInstruction(STORE, regs);
    } else {
      insertInstruction(STOREN, regs, node->outputs().size());
    }
  }

  int insertConstant(IValue value) {
    int result = constant_table_.size();
    constant_table_.emplace_back(std::move(value));
    return result;
  }

  void emitConstant(Node* node) {
    // constants are just put in the constant table
    value_to_reg_[node->output()] =
        insertConstant(toIValue(node->output()).value());
  }

  void emitIf(Node* node) {
    emitLoadInputs(node);
    size_t start_if = instructions_.size();
    insertInstruction(JF, 0); // dummy offset to be filled in
    emitCodeForBlock(node->blocks().at(0));
    insertInstruction(JMP, 0); // dummy offset
    size_t start_else = instructions_.size();
    instructions_[start_if].X = start_else - start_if;
    emitCodeForBlock(node->blocks().at(1));
    instructions_[start_else - 1].X = instructions_.size() - (start_else - 1);
    emitStoreOutputs(node);
  }

  void emitLoop(Node* loop) {
    insertInstruction(LOADC, insertConstant(0));
    emitLoadInputs(loop);
    size_t start = instructions_.size();
    insertInstruction(LOOP, 0, loop->inputs().size()); // dummy offset
    emitCodeForBlock(loop->blocks().at(0));
    insertInstruction(JMP, start - instructions_.size());
    instructions_[start].X = instructions_.size() - start;
    emitStoreOutputs(loop);
  }

  void emitNode(Node* node) {
    WithCurrentNode guard(&current_node_, node);
    switch (node->kind()) {
      default:
        emitOperator(node);
        break;
      case prim::Drop:
        emitDrop(node->inputs());
        break;
      case prim::Constant:
        emitConstant(node);
        break;
      case prim::If:
        emitIf(node);
        break;
      case prim::Loop:
        emitLoop(node);
        break;
      case aten::wait:
        emitWait(node);
        break;
      case prim::Param:
        emitStoreOutputs(node);
        break;
      case prim::Return:
        emitLoadInputs(node);
        break;
    }
  }

  void emitCodeForBlock(Block* block) {
    emitNode(block->param_node());
    for (auto node : block->nodes()) {
      emitNode(node);
    }
    emitNode(block->return_node());
  }

  const std::vector<GraphExecutor*>& grad_executors() {
    if (!grad_executors_) {
      grad_executors_.emplace();
      for (Operation& op : operator_table_) {
        if (auto executor = detail::getGradExecutor(op)) {
          grad_executors_->push_back(executor);
        }
      }
    }
    return *grad_executors_;
  }

  void dump(std::ostream& out, size_t i) const {
    out << i << " " << instructions_[i];
    if (instructions_[i].op == OP) {
      out << " # " << *instructions_source_[i];
    } else {
      out << "\n";
    }
  }

  void dump(std::ostream& out) const {
    out << *graph_ << "\n";
    for (size_t i = 0; i < instructions_.size(); ++i) {
      dump(out, i);
    }
  }
};

// InterpreterState state that and used to compute a Code
struct InterpreterStateImpl : c10::intrusive_ptr_target {
  InterpreterStateImpl(const Code& code)
      : function(code.pImpl), registers(function->register_size_) {}

 private:
  // pc is critical for the interperter to pick up the progress from suspend
  size_t pc = 0;

  // if we need to suspend, where do we reset the stack?
  // answer: to where it was when we were called, not
  // including any inputs to this function
  int64_t stack_start_ = -1;
  c10::intrusive_ptr<Future> future;
  std::shared_ptr<CodeImpl> function; // keep function alive

  // this holds all the tensors for this interpreter run
  // we don't bother minimizing the size of this vector, since the extra
  // memory used by the pointers in this will be small
  // instead we are very aggresive about releasing tensors when they become dead
  // to make sure memory management happens efficiently.
  // We optimize for the case where derivatives are run with retain_graph=False
  // in the case where it is true, then the interpreter and this array get
  // copied if this every becomes a bottleneck then we _should_ consider
  // minimizing the total number or register
  std::vector<IValue> registers;

  c10::intrusive_ptr<InterpreterStateImpl> intrusive_from_this() {
    c10::raw::intrusive_ptr::incref(this);
    return c10::intrusive_ptr<InterpreterStateImpl>::reclaim(this);
  }

  bool runImpl(Stack& stack) {
    // if we have never run before, then we might have to return the
    // stack when we suspend, record where it starts so we return the right
    // stack
    if (stack_start_ == -1) {
      TORCH_INTERNAL_ASSERT(stack.size() >= function->n_inputs);
      stack_start_ = stack.size() - function->n_inputs;
    } else {
      // during restarts, all of the stack is always our own, so we leave
      // nothing
      stack_start_ = 0;
    }
    try {
      return runInterpreterLoop(stack);
    } catch (std::exception& e) {
      // Error from the current thread
      bool is_jit_exception = dynamic_cast<JITException*>(&e);
      handleError(
          function->instructions_source_[pc]->sourceRange().wrapException(
              e, "operation failed in interpreter"),
          is_jit_exception);
      return false;
    }
  }

  bool runInterpreterLoop(Stack& stack) {
    // function->dump(std::cout);
    Instruction* instructions = function->instructions_.data();
    IValue* constants = function->constant_table_.data();
    Operation* operators = function->operator_table_.data();
    while (true) {
      // std::cout << "RUNNING ";
      // function->dump(std::cout, pc);
      Instruction inst = instructions[pc];
      switch (inst.op) {
        case OP:
          operators[inst.X](stack);
          ++pc;
          break;
        case LOAD:
          stack.emplace_back(registers[inst.X]);
          ++pc;
          break;
        case MOVE:
          stack.emplace_back(std::move(registers[inst.X]));
          ++pc;
          break;
        case STORE:
          registers[inst.X] = pop(stack);
          ++pc;
          break;
        case STOREN:
          for (size_t i = inst.N; i > 0; --i) {
            registers[inst.X + i - 1] = pop(stack);
          }
          ++pc;
          break;
        case DROP:
          pop(stack);
          ++pc;
          break;
        case DROPR:
          registers[inst.X] = IValue();
          ++pc;
          break;
        case LOADC:
          stack.emplace_back(constants[inst.X]);
          ++pc;
          break;
        case JF:
          pc += (pop(stack).toBool()) ? 1 : inst.X;
          break;
        case JMP:
          pc += inst.X;
          break;
        case LOOP: {
          // stack: iteration_count, max_iter, cond, loop_carried_deps...
          auto frame = stack.end() - (inst.N + 1);
          int64_t trip_count = frame[0].toInt();
          int64_t max_trip_count = frame[1].toInt();
          bool cond = frame[2].toBool();
          if (trip_count < max_trip_count && cond) {
            frame[2] = trip_count;
            frame[0] = trip_count + 1;
            ++pc;
          } else {
            size_t n_loop_carried = inst.N - 2;
            for (size_t i = 0; i < n_loop_carried; ++i) {
              frame[i] = std::move(frame[i + 3]);
            }
            drop(stack, 3); // iteration_count, max_iter, cond
            pc += inst.X;
          }
        } break;
        case RET:
          if (future) {
            auto num_outputs = function->n_outputs;
            if (num_outputs == 1) {
              future->markCompleted(stack.back());
            } else {
              future->markCompleted(
                  Tuple::create(jit::last(stack, num_outputs).vec()));
            }
          }
          return false;
        case WAIT:
          auto future = stack.back().toFuture();
          if (!future->completed()) {
            getOrCreateFuture();

            // callback needs to be a struct rather than a lambda so that
            // we can move the stack to the other thread
            struct Callback {
              Callback(
                  c10::intrusive_ptr<InterpreterStateImpl> state,
                  Stack stack)
                  : state_(std::move(state)), stack_(std::move(stack)) {}
              void operator()() {
                at::launch(InterpreterContinuation(
                    state_,
                    std::move(stack_),
                    autograd::GradMode::is_enabled()));
              }

             private:
              InterpreterState state_;
              Stack stack_;
            };

            // we are suspending, so we need to reset the stack to where we
            // started if it started empty, except for the inputs we can avoid a
            // true copy by swapping, which leaves the original stack empty.
            Stack copied;
            if (stack_start_ == 0) {
              copied.swap(stack);
            } else {
              copied.insert(
                  copied.begin(),
                  std::make_move_iterator(stack.begin() + stack_start_),
                  std::make_move_iterator(stack.end()));
              stack.resize(stack_start_);
            }
            future->addCallback(
                Callback(intrusive_from_this(), std::move(copied)));

            return true;
          }
          stack.pop_back();
          stack.emplace_back(future->value());
          ++pc;
          break;
      }
    }
  }

  void handleError(std::string&& error_msg, bool is_jit_exception) {
    if (future) {
      future->markCompleted(Future::FutureError(std::move(error_msg)));
    } else if (is_jit_exception) {
      throw JITException(std::move(error_msg));
    } else {
      throw std::runtime_error(std::move(error_msg));
    }
  }

 public:
  c10::intrusive_ptr<Future> getOrCreateFuture() {
    if (!future) {
      future = c10::make_intrusive<Future>();
    }
    return future;
  }

  c10::intrusive_ptr<Future> runAsync(Stack& stack) {
    getOrCreateFuture();
    runImpl(stack);
    return future;
  }

  void run(Stack& stack) {
    if (runImpl(stack)) {
      future->wait();

      auto num_outputs = function->n_outputs;
      if (num_outputs == 1) {
        push(stack, future->value());
      } else {
        auto tuple = future->value().toTuple();
        for (const auto& value : tuple->elements()) {
          push(stack, value);
        }
      }
    }
  }
};

std::ostream& operator<<(std::ostream& out, const Code& code) {
  out << *code.pImpl->graph_ << "\n";
  code.pImpl->dump(out);
  return out;
}

Code::Code(const std::shared_ptr<Graph>& graph) : pImpl(new CodeImpl(graph)) {}
Code::~Code() = default;

const std::vector<GraphExecutor*>& Code::grad_executors() {
  return pImpl->grad_executors();
}

InterpreterState::InterpreterState(const Code& code)
    : pImpl(c10::make_intrusive<InterpreterStateImpl>(code)) {}
InterpreterState::~InterpreterState() = default;

void InterpreterState::run(Stack& stack) {
  static_cast<InterpreterStateImpl*>(pImpl.get())->run(stack);
}

c10::intrusive_ptr<Future> InterpreterState::runAsync(Stack& stack) {
  return static_cast<InterpreterStateImpl*>(pImpl.get())->runAsync(stack);
}

c10::intrusive_ptr<Future> InterpreterState::getFuture() {
  return static_cast<InterpreterStateImpl*>(pImpl.get())->getOrCreateFuture();
}

InterpreterState::InterpreterState(
    c10::intrusive_ptr<c10::intrusive_ptr_target> pImpl_)
    : pImpl(std::move(pImpl_)) {}

void InterpreterContinuation::operator()() {
  autograd::AutoGradMode grad_mode(grad_mode_enabled);
  state.runAsync(stack);
}
} // namespace jit
} // namespace torch
