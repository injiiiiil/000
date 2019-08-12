#include <torch/csrc/distributed/rpc/functions.h>

namespace torch {
namespace distributed {
namespace rpc {

void processRequestBlocking(uint64_t from, Message&& request, RpcAgent& agent) {
  switch (request.type()) {
    case MessageType::SCRIPT_CALL: {
      ScriptCall op = ScriptCall::fromMessage(request);

      auto stack = op.stack();
      op.op()->getOperation()(stack);
      AT_ASSERT(stack.size() == 1, "Return value of a builtin operator or a "
          "TorchScript function should be a single IValue, got a vector of "
          "size ", stack.size());

      auto response = ScriptRet(std::move(stack.front())).toMessage();
      response.setId(request.id());
      agent.send(from, std::move(response));
      break;
    }
    default: {
      AT_ERROR("Request type ", request.type(), " not supported.");
    }
  }
}

}
}
}
