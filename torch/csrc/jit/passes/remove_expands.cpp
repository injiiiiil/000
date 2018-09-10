#include "torch/csrc/jit/passes/remove_expands.h"
#include "torch/csrc/jit/passes/dead_code_elimination.h"

namespace torch { namespace jit {

static void RemoveExpands(Block* block) {
  for (auto it = block->nodes().begin(), end = block->nodes().end(); it != end;
       ++it) {
    for (auto sub : it->blocks())
      RemoveExpands(sub);
    if (it->kind() == aten::expand && it->get<bool>(attr::implicit) != false) {
      it->output()->replaceAllUsesWith(it->namedInput(attr::self));
      it.destroyCurrent();
    }
  }
}

void RemoveExpands(const std::shared_ptr<Graph>& graph) {
  RemoveExpands(graph->block());
}


}}
