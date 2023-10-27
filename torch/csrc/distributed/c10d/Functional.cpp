#include <torch/csrc/distributed/c10d/Functional.hpp>

#include <shared_mutex>

#include <ATen/ATen.h>
#include <ATen/core/op_registration/op_registration.h>
#include <c10/core/DispatchKey.h>
#include <torch/csrc/autograd/function.h>
#include <torch/csrc/distributed/c10d/GroupRegistry.hpp>
#include <torch/csrc/distributed/c10d/ProcessGroup.hpp>
#include <torch/csrc/distributed/c10d/RankLocal.hpp>

namespace {

class WorkRegistry {
 public:
  void register_work(
      const at::Tensor& tensor,
      c10::intrusive_ptr<c10d::Work> work) {
    const auto storage = tensor.storage().getWeakStorageImpl();
    std::unique_lock lock(lock_);
    auto [it, inserted] = registry_.emplace(storage, work);
    TORCH_CHECK(
        inserted || it->second != work,
        "The tensor storage is already associated with another work.");
  }

  c10::intrusive_ptr<c10d::Work> pop_work(const at::Tensor& tensor) {
    const auto storage = tensor.storage().getWeakStorageImpl();
    std::unique_lock lock(lock_);
    auto it = registry_.find(storage);
    TORCH_CHECK(
        it != registry_.end(),
        "No pending collective is associated with the tensor storage. "
        "This typically means that the tensor is not a collective output, "
        "or the tensor has already been waited on.");
    auto work = it->second;
    registry_.erase(it);
    return work;
  }

 private:
  std::unordered_map<
      c10::weak_intrusive_ptr<c10::StorageImpl>,
      c10::intrusive_ptr<c10d::Work>>
      registry_;
  std::mutex lock_;
};

const std::unordered_map<std::string, c10d::ReduceOp> str_to_reduce_op = {
    {"sum", c10d::ReduceOp(c10d::ReduceOp::RedOpType::SUM)},
    {"avg", c10d::ReduceOp(c10d::ReduceOp::RedOpType::AVG)},
    {"product", c10d::ReduceOp(c10d::ReduceOp::RedOpType::PRODUCT)},
    {"min", c10d::ReduceOp(c10d::ReduceOp::RedOpType::MIN)},
    {"max", c10d::ReduceOp(c10d::ReduceOp::RedOpType::MAX)},
    {"band", c10d::ReduceOp(c10d::ReduceOp::RedOpType::BAND)},
    {"bor", c10d::ReduceOp(c10d::ReduceOp::RedOpType::BOR)},
    {"bxor", c10d::ReduceOp(c10d::ReduceOp::RedOpType::BXOR)},
    // TODO: support premul_sum
    // {"premul_sum", c10d::ReduceOp(c10d::ReduceOp::RedOpType::PREMUL_SUM)},
    {"unused", c10d::ReduceOp(c10d::ReduceOp::RedOpType::UNUSED)}};

c10d::ReduceOp to_reduce_op(const std::string& reduce_op) {
  auto it = str_to_reduce_op.find(reduce_op);
  TORCH_CHECK(
      it != str_to_reduce_op.end(), "Unrecognized reduce_op: ", reduce_op);
  return it->second;
}

at::Tensor all_reduce_(
    at::Tensor input,
    const std::string& reduce_op,
    const std::string& group_name) {
  c10d::AllreduceOptions opts;
  opts.reduceOp = to_reduce_op(reduce_op);

  std::vector<at::Tensor> inputs{input};
  auto group = c10d::resolve_process_group(group_name);
  auto work = group->allreduce(inputs, opts);
  c10d::RankLocal<WorkRegistry>::get().register_work(input, work);
  return input;
}

at::Tensor all_reduce(
    const at::Tensor& input,
    const std::string& reduce_op,
    const std::string& group_name) {
  auto output = input.clone();
  return all_reduce_(output, reduce_op, group_name);
}

std::vector<at::Tensor> all_reduce_coalesced_(
    std::vector<at::Tensor> inputs,
    const std::string& reduce_op,
    const std::string& group_name) {
  c10d::AllreduceCoalescedOptions opts;
  opts.reduceOp = to_reduce_op(reduce_op);

  auto group = c10d::resolve_process_group(group_name);
  auto work = group->allreduce_coalesced(inputs, opts);
  for (const auto& tensor : inputs) {
    c10d::RankLocal<WorkRegistry>::get().register_work(tensor, work);
  }
  return inputs;
}

std::vector<at::Tensor> all_reduce_coalesced(
    const std::vector<at::Tensor>& inputs,
    const std::string& reduce_op,
    const std::string& group_name) {
  std::vector<at::Tensor> outputs;
  for (const auto& tensor : inputs) {
    outputs.push_back(tensor.clone());
  }
  return all_reduce_coalesced_(outputs, reduce_op, group_name);
}

at::Tensor allocate_all_gather_output(
    const at::Tensor& input,
    int64_t group_size) {
  auto output_size = input.sizes().vec();
  output_size[0] *= group_size;
  return at::empty(
      output_size,
      at::TensorOptions().dtype(input.dtype()).device(input.device()));
}

std::vector<at::Tensor> all_gather_into_tensor_coalesced(
    const std::vector<at::Tensor>& inputs,
    const int64_t group_size,
    const std::string& group_name) {
  std::vector<at::Tensor> outputs;
  for (const auto& tensor : inputs) {
    outputs.push_back(allocate_all_gather_output(tensor, group_size));
  }

  auto group = c10d::resolve_process_group(group_name);
  auto work = group->allgather_into_tensor_coalesced(
      outputs, const_cast<std::vector<at::Tensor>&>(inputs));
  for (const auto& tensor : outputs) {
    c10d::RankLocal<WorkRegistry>::get().register_work(tensor, work);
  }
  return outputs;
}

at::Tensor all_gather_into_tensor(
    const at::Tensor& input,
    const int64_t group_size,
    const std::string& group_name) {
  std::vector<at::Tensor> inputs{input};
  return all_gather_into_tensor_coalesced(inputs, group_size, group_name)[0];
}

at::Tensor allocate_reduce_scatter_output(
    const at::Tensor& input,
    const int64_t group_size) {
  auto output_size = input.sizes().vec();
  if (output_size[0] % group_size != 0) {
    LOG(WARNING) << "The first dimension of the reduce_scatter input ("
                 << output_size[0] << ") is not divisible by the group size ("
                 << group_size << ").";
  }
  output_size[0] /= group_size;
  return at::empty(
      output_size,
      at::TensorOptions().dtype(input.dtype()).device(input.device()));
}

std::vector<at::Tensor> reduce_scatter_tensor_coalesced(
    const std::vector<at::Tensor>& inputs,
    const std::string& reduce_op,
    const int64_t group_size,
    const std::string& group_name) {
  c10d::ReduceScatterOptions opts;
  opts.reduceOp = to_reduce_op(reduce_op);
  std::vector<at::Tensor> outputs;
  for (const auto& tensor : inputs) {
    outputs.push_back(allocate_reduce_scatter_output(tensor, group_size));
  }

  auto group = c10d::resolve_process_group(group_name);
  auto work = group->reduce_scatter_tensor_coalesced(
      outputs, const_cast<std::vector<at::Tensor>&>(inputs), opts);
  for (const auto& tensor : outputs) {
    c10d::RankLocal<WorkRegistry>::get().register_work(tensor, work);
  }
  return outputs;
}

at::Tensor reduce_scatter_tensor(
    at::Tensor input,
    const std::string& reduce_op,
    const int64_t group_size,
    const std::string& group_name) {
  std::vector<at::Tensor> inputs{input};
  return reduce_scatter_tensor_coalesced(
      inputs, reduce_op, group_size, group_name)[0];
}

at::Tensor wait_tensor(const at::Tensor& tensor) {
  auto work = c10d::RankLocal<WorkRegistry>::get().pop_work(tensor);
  work->wait();
  return tensor;
}

} // namespace

TORCH_LIBRARY(_c10d_functional, m) {
  m.def(
      "all_reduce(Tensor input, str reduce_op, str group_name) -> Tensor",
      torch::dispatch(
          c10::DispatchKey::CompositeExplicitAutograd, ::all_reduce));

  m.def(
      "all_reduce_(Tensor(a!) input, str reduce_op, str group_name) -> Tensor(a!)",
      torch::dispatch(
          c10::DispatchKey::CompositeExplicitAutograd, ::all_reduce_));

  m.def(
      "all_reduce_coalesced(Tensor[] inputs, str reduce_op, str group_name) -> Tensor[]",
      torch::dispatch(
          c10::DispatchKey::CompositeExplicitAutograd, ::all_reduce_coalesced));

  m.def(
      "all_reduce_coalesced_(Tensor[](a!) inputs, str reduce_op, str group_name) -> Tensor[](a!)",
      torch::dispatch(
          c10::DispatchKey::CompositeExplicitAutograd,
          ::all_reduce_coalesced_));

  m.def(
      "all_gather_into_tensor(Tensor input, int group_size, str group_name) -> Tensor",
      torch::dispatch(
          c10::DispatchKey::CompositeExplicitAutograd,
          ::all_gather_into_tensor));

  m.def(
      "all_gather_into_tensor_coalesced(Tensor[] inputs, int group_size, str group_name) -> Tensor[]",
      torch::dispatch(
          c10::DispatchKey::CompositeExplicitAutograd,
          ::all_gather_into_tensor_coalesced));

  m.def(
      "reduce_scatter_tensor(Tensor input, str reduce_op, int group_size, str group_name) -> Tensor",
      torch::dispatch(
          c10::DispatchKey::CompositeExplicitAutograd,
          ::reduce_scatter_tensor));

  m.def(
      "reduce_scatter_tensor_coalesced(Tensor[] inputs, str reduce_op, int group_size, str group_name) -> Tensor[]",
      torch::dispatch(
          c10::DispatchKey::CompositeExplicitAutograd,
          ::reduce_scatter_tensor_coalesced));

  m.def(
      "wait_tensor(Tensor tensor) -> Tensor",
      torch::dispatch(
          c10::DispatchKey::CompositeExplicitAutograd, ::wait_tensor));
}
