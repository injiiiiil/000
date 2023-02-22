#pragma once
#include <c10/util/complex.h>
#include <ir_interface_nodes.h>
#include <ops/alias.h>
#include <ops/arith.h>
#include <ops/normalization.h>
#include <python_frontend/fusion_definition.h>
#include <utils.h>

#include <algorithm>
#include <complex>
#include <variant>

namespace nvfuser::python_frontend {

//! This enum it to give a Record Type for record hashing given that the
//! record type is otherwise determined via the success of dynamic casting.
//! This means that templated types are not specifically enumerated for
//! each set of template arguments.
enum class RecordType {
  Base = 0,
  BatchNormOp,
  BroadcastOp,
  BroadcastInDimOp,
  CastOp,
  Constant,
  End,
  FullOp,
  IotaOp,
  IndexSelectOp,
  TorchGatherOp,
  Op,
  Output,
  PermuteOp,
  ReductionOp,
  Scalar,
  SqueezeOp,
  Start,
  Tensor,
  TensorSizes,
  VarianceOp,
  VarianceMeanOp,
  ReshapeOp
};

//! RecordFunctor is the base class record for operations recorded by
//! the FusionDefinition.  It is, in essence, a node in the graph with
//! input edges, args, and outputs edges outputs where the stored
//! values are indices into the recorded state.
//!
//! The virual functor operator is executed on a cache miss to build the
//! appropriate part of the nvFuser Fusion IR for a given record.
//!
//! The hash and equality operators are used to facilitate the hashing of
//! RecordFunctors in a hash map given those operators need to be
//! specified for custom objects.
//!
//! The print function is used to print the given Record as a statement
//! in a python formated function.

struct RecordFunctor {
  RecordFunctor(
      std::vector<State> _args,
      std::vector<State> _outputs,
      std::string _name,
      RecordType _record_type)
      : args_(std::move(_args)),
        outputs_(std::move(_outputs)),
        name_(std::move(_name)),
        record_type_(_record_type) {}
  virtual ~RecordFunctor() = default;
  //! Allows for copying of Child Class objects with RecordFunctor pointers.
  virtual RecordFunctor* clone() = 0;

  //! The base class is placing the type, outputs, and args hashed as follows:
  //! | 63 - 56 | 55 - 48 | 47 ----------- 32 | 32 ------------------------  0 |
  //! | Type    | Outputs | Args              | Child Class Specified          |
  virtual size_t hash() const {
    size_t arg_hash = 0;
    for (auto arg : args_) {
      arg_hash ^= ((arg.index << 1) ^ static_cast<size_t>(arg.stype));
    }
    size_t output_hash = 0;
    for (auto output : outputs_) {
      output_hash ^= ((output.index << 1) ^ static_cast<size_t>(output.stype));
    }
    return ((static_cast<size_t>(record_type_) & 0xff) << 56) |
        ((output_hash & 0xff) << 48) | ((arg_hash & 0xffff) << 32);
  }

  //! The base virtual equality operator is defined so all child
  //! classes can utilize the check for the same args and outputs.
  virtual bool operator==(const RecordFunctor& other) const {
    auto result = (record_type_ == other.record_type_);
    result = result && (args_.size() == other.args_.size()) &&
        (outputs_.size() == other.outputs_.size());
    if (result) {
      for (size_t i = 0; i < args_.size(); ++i) {
        if ((args_[i].index != other.args_[i].index) ||
            (args_[i].stype != other.args_[i].stype)) {
          result = false;
          break;
        }
      }
    }
    if (result) {
      for (size_t i = 0; i < outputs_.size(); ++i) {
        if ((outputs_[i].index != other.outputs_[i].index) ||
            (outputs_[i].stype != other.outputs_[i].stype)) {
          result = false;
          break;
        }
      }
    }
    return result;
  }

  //! Abstraction for an operation to build this record's nvFuser Fusion IR
  //! piece if the recording has a cache miss.
  virtual void operator()(FusionDefinition& fd) = 0;

  //! The base print function when printing Record for a given FusionDefinition
  //! in python formated code.
  virtual void print(std::ostream& os, bool close_function = true) const {
    bool first_output = true;
    for (auto& output : outputs_) {
      if (first_output) {
        first_output = false;
      } else {
        os << ", ";
      }
      os << output;
    }
    if (outputs_.size() > 0) {
      os << " = "
         << "fd." << name_ << "(";
    } else {
      os << "fd." << name_ << "(";
    }
    bool first_arg = true;
    for (auto& arg : args_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << arg;
    }
    if (close_function) {
      os << ")";
    }
  }

  RecordType recordType() const {
    return record_type_;
  }

 protected:
  //! Inputs that are indices into the FusionDefinition's Recorded State.
  std::vector<State> args_;
  //! Outputs that are indices into the FusionDefinition's Recorded State.
  std::vector<State> outputs_;
  //! Record Name
  std::string name_;
  //! Record Type of child class used for hashing
  RecordType record_type_;
};

//! The OpRecord RecordFunctor is the most widely used child class because
//! it utilizes varidiac template arguments to represent unary, binary,
//! ternary, and other similar flavors of operations in nvFuser that have
//! a mix of Tensor and Scalar arguments only.
//!
//! The additional data memeber of this child class records the function
//! signature of the nvFuser Arith Operation to be replayed upon a cache
//! miss by the functor operator() call.

template <class OutType, class... ArgTypes>
struct OpRecord : RecordFunctor {
  OpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      std::string _name,
      std::function<OutType(ArgTypes...)> fusion_op)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            _name,
            RecordType::Op),
        fusion_op_(fusion_op) {}
  virtual ~OpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new OpRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.= at::Symbol
  //! | 31 -------------------------------------  0 |
  //! | Arith Function Sigs hash code               |
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    return result | (fusion_op_.target_type().hash_code() & 0xffffffff);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    // A succesfull cast indicates a RecordFunctor of the same child class
    if (auto child_ptr = dynamic_cast<const OpRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      if (result) {
        // Match the nvFuser arith function types
        result = result &&
            (fusion_op_.target_type() == child_ptr->fusion_op_.target_type());
        if (isDebugDumpEnabled(DebugDumpOption::PythonFrontendDebug)) {
          std::cout << "\nOpRecord: " << name_ << " Target Type [self: 0x"
                    << fusion_op_.target_type().name() << "] [other: 0x"
                    << child_ptr->fusion_op_.target_type().name() << "] ";
        }
        // Match the nvFuser arith function pointers
        // IMPORTANT! you need to dereference the target pointer in order
        // to match the function
        result = result &&
            (*fusion_op_.template target<OutType (*)(ArgTypes...)>() ==
             *child_ptr->fusion_op_
                  .template target<OutType (*)(ArgTypes...)>());
        if (isDebugDumpEnabled(DebugDumpOption::PythonFrontendDebug)) {
          std::cout
              << "Target  Ptr [self: 0x" << std::hex
              << (size_t)*fusion_op_.template target<OutType (*)(ArgTypes...)>()
              << "] [other: 0x" << std::hex
              << (size_t)*child_ptr->fusion_op_
                     .template target<OutType (*)(ArgTypes...)>()
              << "]\n";
        }
      }
    }
    return result;
  }

  //! The variadic set of indices for the number of args for this op are
  //! deduced by providing the index_sequence as a parameter.  Similarly,
  //! the tuple type is also deduced.
  //!
  //! The tuple type is used to decide whether to cast the input argument
  //! to a Fusion IR TensorView or leave it as a Fusion IR Val (Scalar).
  //!
  //! A deduced binary op could look like:
  //!   OutType opFunc<std::tuple<TensorView*, TensorView*>, 0, 1>
  //! A deduced ternary op could look like:
  //!   OutTupe opFunc<std::tuple<TensorView*, Val*, Val*>, 0, 1, 2>
  template <class TupleType, std::size_t... Is>
  OutType opFunc(
      FusionDefinition& fd,
      TupleType& tp,
      std::index_sequence<Is...>) {
    return fusion_op_(
        dynamic_cast<typename std::tuple_element<Is, TupleType>::type>(
            fd.getFusionState(args_.at(Is).index))...);
  }

  virtual void operator()(FusionDefinition& fd) final {
    using arg_tuple_t = std::tuple<ArgTypes...>;
    auto indices =
        std::make_index_sequence<std::tuple_size<arg_tuple_t>::value>();
    // The tuple variable is never populated, it is passed for its type.
    arg_tuple_t inputs;
    auto output = opFunc(fd, inputs, indices);
    fd.setFusionState(outputs_.at(0).index, output);
  }

 private:
  //! An nvFuser Arith Operation function signature
  std::function<OutType(ArgTypes...)> fusion_op_;
};

struct ReshapeOpRecord : RecordFunctor {
  ReshapeOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      std::vector<int64_t>& original_shape,
      std::vector<int64_t>& new_shape)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            "ops.reshape",
            RecordType::ReshapeOp),
        original_shape_(std::move(original_shape)),
        new_shape_(std::move(new_shape)) {}
  virtual ~ReshapeOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new ReshapeOpRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.
  //! | 31 -------------- 16 | 15 --------------  0 |
  //! | original_shape hash  | new_shape hash       |
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    size_t new_shape_hash = 0;
    for (auto shape : new_shape_) {
      new_shape_hash ^= static_cast<size_t>(shape);
    }
    size_t original_shape_hash = 0;
    for (auto shape : original_shape_) {
      original_shape_hash |= 1 << ((new_shape_.size() - 1) - shape);
    }
    original_shape_hash = (original_shape_hash & 0xffff) << 16;
    return result | original_shape_hash | (new_shape_hash & 0xffff);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const ReshapeOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      result &= std::equal(
          original_shape_.begin(),
          original_shape_.end(),
          child_ptr->original_shape_.begin());
      result &= std::equal(
          new_shape_.begin(), new_shape_.end(), child_ptr->new_shape_.begin());
    }
    return result;
  }

  void operator()(FusionDefinition& fd) final {
    auto arg = fd.getFusionState(args_.at(0).index)->template as<TensorView>();
    auto output = reshape(arg, original_shape_, new_shape_);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", original_shape=[";
    bool first_arg = true;
    for (auto shape : original_shape_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << shape;
    }
    os << "]";
    os << ", new_shape=[";
    first_arg = true;
    for (auto shape : new_shape_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << shape;
    }
    os << "]";
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! Represents the tensor dimensions of the input tensor.
  std::vector<int64_t> original_shape_;
  //! Represents the tensor dimensions of the output tensor.
  std::vector<int64_t> new_shape_;
};

struct PermuteOpRecord : RecordFunctor {
  PermuteOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      std::vector<int64_t>& dims)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            "ops.permute",
            RecordType::PermuteOp),
        dims_(std::move(dims)) {}
  virtual ~PermuteOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new PermuteOpRecord(*this);
  }

  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    size_t dims_hash = 0;
    for (auto dim : dims_) {
      dims_hash ^= static_cast<size_t>(dim);
    }
    return result | (dims_hash & 0xffff);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const PermuteOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      if (result) {
        result = (dims_.size() == child_ptr->dims_.size());
        if (result) {
          for (size_t i = 0; i < dims_.size(); ++i) {
            if (dims_[i] != child_ptr->dims_[i]) {
              result = false;
              break;
            }
          }
        }
      }
    }
    return result;
  }

  void operator()(FusionDefinition& fd) final {
    auto arg = fd.getFusionState(args_.at(0).index)->template as<TensorView>();
    auto output = permute(arg, dims_);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", dims=[";
    bool first_arg = true;
    for (auto dim : dims_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << dim;
    }
    os << "]";
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! Represents the mapping from the original shape to the new shape
  std::vector<int64_t> dims_;
};

struct SqueezeOpRecord : RecordFunctor {
  SqueezeOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      std::vector<int64_t>& original_shape,
      std::vector<int64_t>& dims)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            "ops.squeeze",
            RecordType::SqueezeOp),
        original_shape_(std::move(original_shape)),
        dims_(std::move(dims)) {}
  virtual ~SqueezeOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new SqueezeOpRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.
  //! | 31 -------------- 16 | 15 --------------  0 |
  //! | Squeeze Dim hash     | original_shape hash  |
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    size_t original_shape_hash = 0;
    for (auto shape : original_shape_) {
      original_shape_hash ^= static_cast<size_t>(shape);
    }
    size_t squeeze_dims_hash = 0;
    for (auto dim : dims_) {
      squeeze_dims_hash ^= static_cast<size_t>(dim);
    }
    squeeze_dims_hash = (squeeze_dims_hash & 0xffff) << 16;
    return result | squeeze_dims_hash | (original_shape_hash & 0xffff);
  }
  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const SqueezeOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      if (result) {
        result = (original_shape_.size() == child_ptr->original_shape_.size());
        if (result) {
          for (size_t i = 0; i < dims_.size(); ++i) {
            if (dims_[i] != child_ptr->dims_[i]) {
              result = false;
              break;
            }
          }
        }
        if (result) {
          for (size_t i = 0; i < original_shape_.size(); ++i) {
            if (original_shape_[i] != child_ptr->original_shape_[i]) {
              result = false;
              break;
            }
          }
        }
      }
    }
    return result;
  }

  void operator()(FusionDefinition& fd) final {
    auto arg = fd.getFusionState(args_.at(0).index)->template as<TensorView>();
    auto output = squeeze(arg, original_shape_, dims_);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", original_shape=[";
    bool first_arg = true;
    for (auto shape : original_shape_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << shape;
    }
    os << "], dims=[";
    first_arg = true;
    for (auto dim : dims_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << dim;
    }
    os << "]";
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! Represents the tensor dimensions of the input tensor.
  std::vector<int64_t> original_shape_;
  //! Dimension to squeeze.
  std::vector<int64_t> dims_;
};

//! Specialized Record Functor for the FusionDefinition's broadcast_in_dim op.

template <typename OutputShapeType>
struct BroadcastInDimOpRecord : RecordFunctor {
  BroadcastInDimOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      std::string _name,
      std::vector<OutputShapeType>& output_shape,
      std::vector<int64_t>& broadcast_dims)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            _name,
            RecordType::BroadcastInDimOp),
        output_shape_(std::move(output_shape)),
        broadcast_dims_(std::move(broadcast_dims)) {}
  virtual ~BroadcastInDimOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new BroadcastInDimOpRecord(*this);
  }

  inline size_t outputShapeHash(
      const std::vector<OutputShapeType>& shape) const;

  //! Child specific hash function in lower 32 bits.
  //! | 31 -------------- 16 | 15 --------------  0 |
  //! | broadcast_dims hash  | output_shape hash    |
  //!
  //! The output_shape hash is specialized in 2 ways using the method
  //! outputShapeHash:
  //! 1. int64_t - hashes dimension sizes.
  //! 2. State - hashes number of dimensions
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    size_t broadcast_dims_hash = 0;
    for (auto dim : broadcast_dims_) {
      broadcast_dims_hash |= 1 << ((output_shape_.size() - 1) - dim);
    }
    broadcast_dims_hash = (broadcast_dims_hash & 0xffff) << 16;
    return result | broadcast_dims_hash |
        (outputShapeHash(output_shape_) & 0xffff);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const BroadcastInDimOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      if (result) {
        result =
            ((output_shape_.size() == child_ptr->output_shape_.size()) &&
             (broadcast_dims_.size() == child_ptr->broadcast_dims_.size()));
        if (result) {
          for (size_t i = 0; i < output_shape_.size(); ++i) {
            if (output_shape_[i] != child_ptr->output_shape_[i]) {
              result = false;
              break;
            }
          }
        }
        if (result) {
          for (size_t i = 0; i < broadcast_dims_.size(); ++i) {
            if (broadcast_dims_[i] != child_ptr->broadcast_dims_[i]) {
              result = false;
              break;
            }
          }
        }
      }
    }
    return result;
  }

  inline c10::optional<std::vector<Val*>> expandShape(
      const FusionDefinition& fd,
      const std::vector<bool>& expand_dim,
      const std::vector<OutputShapeType>& shape) const;

  //! The operator() call is specialize with th expandShape() method based on
  //! the OutputShapeType template parameter
  virtual void operator()(FusionDefinition& fd) final {
    auto arg = fd.getFusionState(args_.at(0).index)->template as<TensorView>();

    const auto& arg_domains_nr = arg->domain()->noReductions();
    const auto arg_ndims = arg_domains_nr.size();
    TORCH_CHECK(
        output_shape_.size() >= arg_ndims,
        "The new shape is expected to be greater-then-or-equal to the input",
        output_shape_.size(),
        arg_ndims);
    TORCH_CHECK(
        arg_ndims == broadcast_dims_.size(),
        "The broadcast dimensions should match the input dimensions.",
        arg_ndims,
        broadcast_dims_.size());

    std::vector<bool> is_broadcast_dim(output_shape_.size(), true);
    std::vector<bool> is_expand_dim(output_shape_.size(), true);
    for (const auto idx : c10::irange(broadcast_dims_.size())) {
      if (idx > 0) {
        TORCH_CHECK(
            broadcast_dims_[idx - 1] < broadcast_dims_[idx],
            "Broadcast dimension is not greater than the previous value.");
      }
      TORCH_CHECK(
          broadcast_dims_[idx] < static_cast<int>(output_shape_.size()),
          "Invalid broadcast_dims value.");
      is_broadcast_dim.at(broadcast_dims_[idx]) = false;
      // Note: when we expand a broadcasted dimension, we need to expand it
      // to a concrete size, hence the need for `is_expand_dim` flag and the
      // expand operation following the broadcast.
      is_expand_dim.at(broadcast_dims_[idx]) =
          arg_domains_nr[idx]->isBroadcast();
    }

    auto output = broadcast(arg, is_broadcast_dim);

    c10::optional<std::vector<Val*>> expand_shape =
        expandShape(fd, is_expand_dim, output_shape_);
    if (expand_shape.has_value()) {
      output = expand(output, expand_shape.value());
    }
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", output_shape=[";
    bool first_arg = true;
    for (auto shape : output_shape_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << shape;
    }
    os << "]";
    os << ", broadcast_dims=[";
    first_arg = true;
    for (auto dim : broadcast_dims_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << dim;
    }
    os << "]";
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! Represents the tensor dimensions of the output tensor.
  std::vector<OutputShapeType> output_shape_;
  //! Communicates which dimensions of the output the input tensor maps.
  //! For instance, for output [2, 3, 4] and input [3]. This vector would
  //! contain [1].
  std::vector<int64_t> broadcast_dims_;
};

//! ouputShapeHash Specializations used by hash()

template <>
inline size_t BroadcastInDimOpRecord<int64_t>::outputShapeHash(
    const std::vector<int64_t>& shape) const {
  size_t shape_hash = 0;
  for (auto size : shape) {
    shape_hash ^= static_cast<size_t>(size);
  }
  return shape_hash;
}

template <>
inline size_t BroadcastInDimOpRecord<State>::outputShapeHash(
    const std::vector<State>& shape) const {
  return shape.size();
}

//! expandShape Specializations used by operator()

template <>
inline c10::optional<std::vector<Val*>> BroadcastInDimOpRecord<int64_t>::
    expandShape(
        const FusionDefinition& fd,
        const std::vector<bool>& expand_dim,
        const std::vector<int64_t>& shape) const {
  std::vector<Val*> expand_shape(shape.size(), nullptr);
  bool has_expand = false;
  for (const auto idx : c10::irange(shape.size())) {
    if (expand_dim[idx] && shape[idx] != 1 && shape[idx] != -1) {
      expand_shape[idx] = IrBuilder::create<Int>(shape[idx]);
      has_expand = true;
    } else {
      expand_shape[idx] = IrBuilder::create<Int>(-1);
    }
  }

  if (has_expand) {
    return c10::optional<std::vector<Val*>>(expand_shape);
  } else {
    return c10::nullopt;
  }
}

template <>
inline c10::optional<std::vector<Val*>> BroadcastInDimOpRecord<State>::
    expandShape(
        const FusionDefinition& fd,
        const std::vector<bool>& expand_dim,
        const std::vector<State>& shape) const {
  std::vector<Val*> expand_shape(shape.size(), nullptr);
  std::transform(
      shape.begin(),
      shape.end(),
      expand_shape.begin(),
      [&fd](const State& state) {
        return fd.getFusionState(state.index)->template as<Val>();
      });
  return c10::optional<std::vector<Val*>>(expand_shape);
}

//! Specialized Record Functor for the FusionDefinition's broadcast op.

struct BroadcastOpRecord : RecordFunctor {
  BroadcastOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      std::string _name,
      std::vector<bool>& is_broadcast_dim)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            _name,
            RecordType::BroadcastOp),
        is_broadcast_dim_(std::move(is_broadcast_dim)) {}
  virtual ~BroadcastOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new BroadcastOpRecord(*this);
  }

  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    size_t is_broadcast_dim_hash = 0;
    for (size_t i = 0; i < is_broadcast_dim_.size(); ++i) {
      is_broadcast_dim_hash |=
          (is_broadcast_dim_[i] << (is_broadcast_dim_.size() - 1 - i));
    }
    return result | (is_broadcast_dim_hash & 0xfff);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const BroadcastOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      result &= std::equal(
          is_broadcast_dim_.begin(),
          is_broadcast_dim_.end(),
          child_ptr->is_broadcast_dim_.begin());
    }
    return result;
  }

  virtual void operator()(FusionDefinition& fd) final {
    auto arg = fd.getFusionState(args_.at(0).index)->template as<TensorView>();
    auto output = broadcast(arg, is_broadcast_dim_);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", is_broadcast_dim=[";
    bool first_arg = true;
    for (auto dim : is_broadcast_dim_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << (dim ? "True" : "False");
    }
    os << "]";
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! Communicates which dimensions in the output are broadcasted.
  std::vector<bool> is_broadcast_dim_;
};

template <class OutType, class ArgType>
struct CastOpRecord : RecordFunctor {
  CastOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      std::string _name,
      std::function<OutType(DataType, ArgType)> fusion_op,
      PrimDataType dtype)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            _name,
            RecordType::CastOp),
        fusion_op_(fusion_op),
        dtype_(dtype) {}
  virtual ~CastOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new CastOpRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.
  //! | 31 --- 24 | 23 --------------------------  0 |
  //! | Dtype     | Arith Function Sig hash code     |
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    result |= ((static_cast<size_t>(dtype_) & 0xff) << 24);
    result |= (fusion_op_.target_type().hash_code() & 0xffffff);
    return result;
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const CastOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      if (result) {
        result = result &&
            (fusion_op_.target_type() == child_ptr->fusion_op_.target_type());
        if (isDebugDumpEnabled(DebugDumpOption::PythonFrontendDebug)) {
          std::cout << "\nCastOpRecord: " << name_ << " Target Type [self: 0x"
                    << fusion_op_.target_type().name() << "] [other: 0x"
                    << child_ptr->fusion_op_.target_type().name() << "]";
        }
        // IMPORTANT! you need to dereference the target pointer in order
        // to match the function
        result = result &&
            (*fusion_op_.template target<OutType (*)(DataType, ArgType)>() ==
             *child_ptr->fusion_op_
                  .template target<OutType (*)(DataType, ArgType)>());
        if (isDebugDumpEnabled(DebugDumpOption::PythonFrontendDebug)) {
          std::cout << " Target  Ptr [self: 0x" << std::hex
                    << (size_t)*fusion_op_
                           .template target<OutType (*)(DataType, ArgType)>()
                    << "] [other: 0x" << std::hex
                    << (size_t)*child_ptr->fusion_op_
                           .template target<OutType (*)(DataType, ArgType)>()
                    << "]\n";
        }
        result = result && (dtype_ == child_ptr->dtype_);
      }
    }
    return result;
  }

  virtual void operator()(FusionDefinition& fd) final {
    auto arg = dynamic_cast<ArgType>(fd.getFusionState(args_.at(0).index));
    auto output = fusion_op_(dtype_, arg);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", dtype=" << dtypeToPyString(dtype_);
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! nvFuser arith function signature
  std::function<OutType(DataType, ArgType)> fusion_op_;
  //! Type to cast to.
  PrimDataType dtype_;
};

//! Specialized Record Functor for recording FusionDefinition constant state.

template <typename ExprType, typename ValueType>
struct ConstantRecord : RecordFunctor {
  ConstantRecord(std::vector<State> _outputs, ValueType val, PrimDataType dtype)
      : RecordFunctor(
            {},
            std::move(_outputs),
            "define_constant",
            RecordType::Constant),
        value_(val),
        dtype_(dtype) {}
  virtual ~ConstantRecord() = default;
  virtual RecordFunctor* clone() final {
    return new ConstantRecord(*this);
  }

  //! Going to start out hashing nothing extra since hashing a complex number
  //! seems complicated.  Initially, the thought was to simply static cast the
  //! value_
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    return result;
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const ConstantRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      result = result && (value_ == child_ptr->value_);
    }
    return result;
  }

  virtual void operator()(FusionDefinition& fd) final {
    Val* output = IrBuilder::create<ExprType>(value_, dtype_);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    if (std::is_same<ValueType, bool>::value) {
      bool value = __toBool(value_);
      os << (value ? "True" : "False");
    } else if (
        std::is_same<ValueType, std::complex<float>>::value ||
        std::is_same<ValueType, std::complex<double>>::value) {
      os << std::showpoint << std::real(value_) << "+" << std::showpoint
         << std::imag(value_) << "j";
    } else {
      os << std::showpoint << value_;
    }

    os << ", dtype=" << dtypeToPyString(dtype_);

    if (close_function) {
      os << ")";
    }
  }

 private:
  //! The constants literal value.
  ValueType value_;

  //! The DataType provided
  PrimDataType dtype_;
};

//! Specialized Record Functor for recording FusionDefinition End.
//! The accompanying Fusion Cache Entry holds a Fusion Object.

struct EndRecord : RecordFunctor {
  EndRecord() : RecordFunctor({}, {}, "end", RecordType::End) {}
  virtual ~EndRecord() = default;
  virtual RecordFunctor* clone() final {
    return new EndRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.
  //! | 31 ---------------------------------------  0 |
  //! | None                                          |
  virtual size_t hash() const final {
    return RecordFunctor::hash();
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (dynamic_cast<const EndRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
    }
    return result;
  }

  virtual void operator()(FusionDefinition& fd) final {}
};

//! Specialized Record Functor for recording FusionDefinition input tensors.

struct TensorRecord : RecordFunctor {
  TensorRecord(
      std::vector<State> _outputs,
      std::vector<int64_t> _symbolic_sizes,
      std::vector<bool> _contiguous_info,
      PrimDataType _dtype,
      bool _is_cpu = false)
      : RecordFunctor(
            {},
            std::move(_outputs),
            "define_tensor",
            RecordType::Tensor),
        symbolic_sizes_(std::move(_symbolic_sizes)),
        contiguous_info_(std::move(_contiguous_info)),
        dtype_(_dtype),
        is_cpu_(_is_cpu) {}
  virtual ~TensorRecord() = default;
  virtual RecordFunctor* clone() final {
    return new TensorRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.
  //! |  31  | 30 --- 24 | 23 --------- 12 | 11 ---------  0 |
  //! | CPU? | Dtype     | Symbolic Sizes  | Contiguous Info |
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    size_t ssize_hash = 0;
    for (size_t i = 0; i < symbolic_sizes_.size(); ++i) {
      size_t ssize = 0;
      if (symbolic_sizes_[i] == -1) {
        ssize = 1;
      }
      ssize_hash |= (ssize << (symbolic_sizes_.size() - 1 - i));
    }
    size_t contig_hash = 0;
    for (size_t i = 0; i < contiguous_info_.size(); ++i) {
      contig_hash |= (contiguous_info_[i] << (contiguous_info_.size() - 1 - i));
    }

    result |= ((static_cast<size_t>(is_cpu_) & 0x1) << 31);
    result |= ((static_cast<size_t>(dtype_) & 0x7f) << 24);
    return result | ((ssize_hash & 0xfff) << 12) | (contig_hash & 0xfff);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const TensorRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      result = result && (dtype_ == child_ptr->dtype_);
      result = result && (is_cpu_ == child_ptr->is_cpu_);
      if (result) {
        result =
            ((symbolic_sizes_.size() == child_ptr->symbolic_sizes_.size()) &&
             (contiguous_info_.size() == child_ptr->contiguous_info_.size()));
        if (result) {
          for (size_t i = 0; i < symbolic_sizes_.size(); ++i) {
            if (symbolic_sizes_[i] != child_ptr->symbolic_sizes_[i]) {
              result = false;
              break;
            }
          }
        }
        if (result) {
          for (size_t i = 0; i < contiguous_info_.size(); ++i) {
            if (contiguous_info_[i] != child_ptr->contiguous_info_[i]) {
              result = false;
              break;
            }
          }
        }
      }
    }
    return result;
  }

  virtual void operator()(FusionDefinition& fd) final {
    auto tv = TensorViewBuilder()
                  .ndims(symbolic_sizes_.size())
                  .contiguity(contiguous_info_)
                  .shape(symbolic_sizes_)
                  .dtype(dtype_)
                  .build();

    if (symbolic_sizes_.empty() && is_cpu_) {
      tv->setCpuScalar(true);
    } else {
      TORCH_CHECK(!is_cpu_, "CPU non-scalar tensor is not supported!");
    }

    fd.setFusionState(outputs_.at(0).index, tv);
    fd.addInput(tv);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << "symbolic_sizes=[";
    bool first_arg = true;
    for (auto ss : symbolic_sizes_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << ss;
    }
    os << "], contiguous=[";
    first_arg = true;
    for (auto ci : contiguous_info_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      if (ci) {
        os << "True";
      } else {
        os << "False";
      }
    }
    os << "], dtype=" << dtypeToPyString(dtype_);
    os << ", is_cpu=" << (is_cpu_ ? "True" : "False");
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! A vector of tensor dimension sizes.
  //! This vector only captures sizes of -1 or 1 to indicate a symbolic
  //! dimension (-1) or a broadcast dimension (1).
  std::vector<int64_t> symbolic_sizes_;
  //! A vector to indicate whether the a tensor dimension is contiguous
  //! with the dimension just to its right.
  std::vector<bool> contiguous_info_;
  //! Tensor data type.
  PrimDataType dtype_;
  //! Notes a scalar CPU Tensor
  bool is_cpu_;
};

//! Specialized Record Functor for recording FusionDefinition outputs.

template <class OutputType>
struct OutputRecord : RecordFunctor {
  OutputRecord(std::vector<State> _args)
      : RecordFunctor(std::move(_args), {}, "add_output", RecordType::Output) {}
  virtual ~OutputRecord() = default;
  virtual RecordFunctor* clone() final {
    return new OutputRecord(*this);
  }

  //! Nothing extra necessary in hash
  //! Child specific hash function in lower 32 bits.
  //! | 31 ---------------------------------------  0 |
  //! | None                                          |
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    return result;
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const OutputRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
    }
    return result;
  }

  virtual void operator()(FusionDefinition& fd) final {
    auto output = fd.getFusionState(args_.at(0).index);
    Val* alias_input = nullptr;
    if (args_.size() == 2) {
      alias_input = fd.getFusionState(args_.at(1).index);
    }

    if (alias_input) {
      if (std::is_same<OutputType, TensorView>::value) {
        fd.aliasOutputToInput(output, alias_input);
      } else {
        TORCH_INTERNAL_ASSERT(false, "Scalar outputs should not alias inputs.");
      }
    } else {
      // With C++17, this statement should be "if constexpr"
      if (std::is_same<OutputType, TensorView>::value) {
        fd.addOutput(output->template as<TensorView>());
      } else {
        fd.addOutput(output);
      }
    }
  }
};

//! Specialized Record Functor for the FusionDefinition's sum/min/max ops.

struct ReductionOpRecord : RecordFunctor {
  ReductionOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      std::string _name,
      std::function<
          TensorView*(TensorView*, const std::vector<int>&, bool, DataType)>
          fusion_op,
      std::vector<int> axes,
      bool keep_dim,
      PrimDataType dtype)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            _name,
            RecordType::ReductionOp),
        fusion_op_(fusion_op),
        axes_(std::move(axes)),
        keep_dim_(keep_dim),
        dtype_(dtype) {}
  virtual ~ReductionOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new ReductionOpRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.
  //! | 31 -- 28 | 27 --- 20 | 19 -----------------  0 |
  //! | keep_dim | Dtype     | Axes Hash               |
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    size_t axes_hash = 0;
    // Normally I would make a little endian hash of the axes but I do not
    // know the size of the tensor based on just the record information.
    for (size_t i = 0; i < axes_.size(); ++i) {
      axes_hash |= (1 << axes_[i]);
    }

    return result | (static_cast<size_t>(keep_dim_) << 28) |
        ((static_cast<size_t>(dtype_) & 0xff) << 20) | (axes_hash & 0xfffff);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const ReductionOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      if (result) {
        result = result &&
            (fusion_op_.target_type() == child_ptr->fusion_op_.target_type());
        if (isDebugDumpEnabled(DebugDumpOption::PythonFrontendDebug)) {
          std::cout << "\nReductionOpRecord: " << name_
                    << " Target Type [self: 0x"
                    << fusion_op_.target_type().name() << "] [other: 0x"
                    << child_ptr->fusion_op_.target_type().name() << "]";
        }
        // IMPORTANT! you need to dereference the target pointer in order
        // to match the function
        result = result &&
            (*fusion_op_.template target<

                 TensorView* (*)(TensorView*, const std::vector<int>&, bool, DataType)>() ==
             *child_ptr->fusion_op_.template target<

                 TensorView* (*)(TensorView*, const std::vector<int>&, bool, DataType)>());
        if (isDebugDumpEnabled(DebugDumpOption::PythonFrontendDebug)) {
          std::cout
              << " Target  Ptr [self: 0x" << std::hex
              << (size_t)*fusion_op_.template target<

                     TensorView* (*)(TensorView*, const std::vector<int>&, bool, DataType)>()
              << "] [other: 0x" << std::hex
              << (size_t)*child_ptr->fusion_op_.template target<

                     TensorView* (*)(TensorView*, const std::vector<int>&, bool, DataType)>()
              << "]\n";
        }
        result = result && (keep_dim_ == child_ptr->keep_dim_);
        result = result && (dtype_ == child_ptr->dtype_);
        if (result) {
          result = (axes_.size() == child_ptr->axes_.size());
          if (result) {
            for (size_t i = 0; i < axes_.size(); ++i) {
              if (axes_[i] != child_ptr->axes_[i]) {
                result = false;
                break;
              }
            }
          }
        }
      }
    }
    return result;
  }

  virtual void operator()(FusionDefinition& fd) final {
    auto arg = fd.getFusionState(args_.at(0).index)->template as<TensorView>();
    auto output = fusion_op_(arg, axes_, keep_dim_, dtype_);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", axes=[";
    bool first_arg = true;
    for (auto axis : axes_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << axis;
    }
    os << "]";
    os << ", keepdim=" << (keep_dim_ ? "True" : "False");
    os << ", dtype=" << dtypeToPyString(dtype_);
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! nvFuser arith function signature for a given reduction operation
  std::function<
      TensorView*(TensorView*, const std::vector<int>&, bool, DataType)>
      fusion_op_;
  //! The tensor dimensions to reduce
  std::vector<int> axes_;
  //! Indicates whether to keep the reduced dimension(s).
  bool keep_dim_;
  //! The output data type.
  PrimDataType dtype_;
};

struct IndexSelectOpRecord : RecordFunctor {
  IndexSelectOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      int64_t dim)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            "ops.index_select",
            RecordType::IndexSelectOp),
        dim_(dim) {}
  virtual ~IndexSelectOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new IndexSelectOpRecord(*this);
  }

  void operator()(FusionDefinition& fd) final {
    auto arg1 = fd.getFusionState(args_.at(0).index)->template as<TensorView>();
    auto arg3 = fd.getFusionState(args_.at(1).index)->template as<TensorView>();

    Val* output = index_select(arg1, dim_, arg3);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", dim=" << dim_;
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! Dimension to select.
  int64_t dim_;
};

struct TorchGatherOpRecord : RecordFunctor {
  TorchGatherOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      int64_t dim)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            "ops.gather",
            RecordType::TorchGatherOp),
        dim_(dim) {}
  virtual ~TorchGatherOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new TorchGatherOpRecord(*this);
  }

  void operator()(FusionDefinition& fd) final {
    auto arg1 = fd.getFusionState(args_.at(0).index)->template as<TensorView>();
    auto arg3 = fd.getFusionState(args_.at(1).index)->template as<TensorView>();

    Val* output = torch_gather(arg1, dim_, arg3);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", dim=" << dim_;
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! Dimension to select.
  int64_t dim_;
};

//! Specialized Record Functor for recording FusionDefinition input scalars.

struct ScalarRecord : RecordFunctor {
  ScalarRecord(std::vector<State> _outputs, PrimDataType dtype)
      : RecordFunctor(
            {},
            std::move(_outputs),
            "define_scalar",
            RecordType::Scalar),
        dtype_(dtype) {}
  virtual ~ScalarRecord() = default;
  virtual RecordFunctor* clone() final {
    return new ScalarRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.
  //! | 31 ---------------------------------------  0 |
  //! | Dtype                                         |
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    return result | (static_cast<size_t>(dtype_) & 0xffffffff);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const ScalarRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      result = result && (dtype_ == child_ptr->dtype_);
    }
    return result;
  }

  virtual void operator()(FusionDefinition& fd) final {
    Val* output = nullptr;
    if (dtype_ == DataType::Double) {
      output = IrBuilder::create<Double>();
    } else if (dtype_ == DataType::ComplexDouble) {
      output = IrBuilder::create<ComplexDouble>();
    } else if (dtype_ == DataType::Bool) {
      output = IrBuilder::create<Bool>();
    } else if (dtype_ == DataType::Int) {
      output = IrBuilder::create<Int>();
    } else {
      TORCH_CHECK(false, "Dtype is not supported:", dtype_);
    }
    fd.addInput(output);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << "dtype=" << dtypeToPyString(dtype_);
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! Scalar data type.
  PrimDataType dtype_;
};

//! Specialized Record Functor for recording FusionDefinition Start.
//! There should only ever be one instance of this Record in the
//! Fusion Cache.

struct StartRecord : RecordFunctor {
  StartRecord() : RecordFunctor({}, {}, "start", RecordType::Start) {}
  virtual ~StartRecord() = default;
  virtual RecordFunctor* clone() final {
    return new StartRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.
  //! | 31 ---------------------------------------  0 |
  //! | None                                          |
  virtual size_t hash() const final {
    return RecordFunctor::hash();
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (dynamic_cast<const StartRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
    }
    return result;
  }

  virtual void operator()(FusionDefinition& fd) final {}
};

//! Specialized Record Functors for Normalization based ops.

struct NormOpRecord : RecordFunctor {
  NormOpRecord(
      std::vector<State> args,
      std::vector<State> outputs,
      std::string name,
      RecordType type,
      std::vector<int>& axes,
      int64_t correction,
      bool keep_dim)
      : RecordFunctor(std::move(args), std::move(outputs), name, type),
        axes_(axes),
        correction_(correction),
        keep_dim_(keep_dim) {}
  virtual ~NormOpRecord() = default;
  RecordFunctor* clone() override = 0;

  // I am skipping the bassel's correction value in the hash because
  // I suspect we might change it to a bool from a 64-bit value
  //! Child specific hash function in lower 32 bits.
  //! | 31 -- 28 | 27 -----------------------------  0 |
  //! | keep_dim | Axes Hash                           |
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    size_t axes_hash = 0;
    // Normally I would make a little endian hash of the axes but I do not
    // know the size of the tensor based on just the record information.
    for (size_t i = 0; i < axes_.size(); ++i) {
      axes_hash |= (1 << axes_[i]);
    }
    return result | (static_cast<size_t>(keep_dim_) << 28) |
        (axes_hash & 0xfffffff);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const NormOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      result = result && (correction_ == child_ptr->correction_);
      result = result && (keep_dim_ == child_ptr->keep_dim_);
      if (result) {
        result = (axes_.size() == child_ptr->axes_.size());
        if (result) {
          for (size_t i = 0; i < axes_.size(); ++i) {
            if (axes_[i] != child_ptr->axes_[i]) {
              result = false;
              break;
            }
          }
        }
      }
    }
    return result;
  }

  //! Each NormOp Child should define the operator() to build the IR
  void operator()(FusionDefinition& fd) override = 0;

  virtual void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", axes=[";
    bool first_arg = true;
    for (auto axis : axes_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << axis;
    }
    os << "]";
    os << ", correction=" << correction_;
    os << ", keepdim=" << (keep_dim_ ? "True" : "False");
    if (close_function) {
      os << ")";
    }
  }

 protected:
  //! Dimensions of tensor to reduce for variance calculation
  std::vector<int> axes_;
  //! Bessel's correction value
  int64_t correction_;
  //! Indicates whether to keep the reduced dimension(s).
  bool keep_dim_;
};

struct VarianceOpRecord : NormOpRecord {
  VarianceOpRecord(
      std::vector<State> args,
      std::vector<State> outputs,
      std::vector<int>& axes,
      int64_t correction,
      bool keep_dim)
      : NormOpRecord(
            std::move(args),
            std::move(outputs),
            "ops.var",
            RecordType::VarianceOp,
            axes,
            correction,
            keep_dim) {}
  virtual ~VarianceOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new VarianceOpRecord(*this);
  }

  virtual void operator()(FusionDefinition& fd) final {
    auto arg = fd.getFusionState(args_.at(0).index)->as<TensorView>();
    auto output = variance(arg, axes_, correction_, keep_dim_);
    fd.setFusionState(outputs_.at(0).index, output);
  }
};

//! VarianceMean requires a separate Record because nvFuser defines the output
//! of var_mean as a custom struct.
struct VarianceMeanOpRecord : NormOpRecord {
  VarianceMeanOpRecord(
      std::vector<State> args,
      std::vector<State> outputs,
      std::vector<int>& axes,
      int64_t correction,
      bool keep_dim)
      : NormOpRecord(
            std::move(args),
            std::move(outputs),
            "ops.var_mean",
            RecordType::VarianceMeanOp,
            axes,
            correction,
            keep_dim) {}
  virtual ~VarianceMeanOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new VarianceMeanOpRecord(*this);
  }

  void operator()(FusionDefinition& fd) final {
    auto arg = fd.getFusionState(args_.at(0).index)->as<TensorView>();
    auto output = variance_mean(arg, axes_, correction_, keep_dim_);
    fd.setFusionState(outputs_.at(0).index, output.var);
    fd.setFusionState(outputs_.at(1).index, output.mean);
  }
};

struct BatchNormOpRecord : RecordFunctor {
  BatchNormOpRecord(
      std::vector<State> args,
      std::vector<State> outputs,
      bool training,
      bool channels_last)
      : RecordFunctor(
            std::move(args),
            std::move(outputs),
            "ops.batch_norm",
            RecordType::BatchNormOp),
        training_(training),
        channels_last_(channels_last) {}
  virtual ~BatchNormOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new BatchNormOpRecord(*this);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const BatchNormOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
      result = result && (training_ == child_ptr->training_);
      result = result && (channels_last_ == child_ptr->channels_last_);
    }
    return result;
  }

  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    return result | (static_cast<size_t>(training_) << 28) |
        (static_cast<size_t>(channels_last_) << 29);
  }

  void operator()(FusionDefinition& fd) final {
    auto x = fd.getFusionState(args_.at(0).index)->as<TensorView>();
    auto weight = (args_.at(1).stype == StateType::Tensor)
        ? fd.getFusionState(args_.at(1).index)->as<TensorView>()
        : nullptr;
    auto bias = (args_.at(2).stype == StateType::Tensor)
        ? fd.getFusionState(args_.at(2).index)->as<TensorView>()
        : nullptr;
    auto running_mean = (args_.at(3).stype == StateType::Tensor)
        ? fd.getFusionState(args_.at(3).index)->as<TensorView>()
        : nullptr;
    auto running_var = (args_.at(4).stype == StateType::Tensor)
        ? fd.getFusionState(args_.at(4).index)->as<TensorView>()
        : nullptr;
    auto momentum = fd.getFusionState(args_.at(5).index)->as<Val>();
    auto eps = fd.getFusionState(args_.at(6).index)->as<Val>();
    auto output = batch_norm(
        x,
        weight,
        bias,
        running_mean,
        running_var,
        training_,
        momentum,
        eps,
        channels_last_);
    fd.setFusionState(outputs_.at(0).index, output.output);
    fd.setFusionState(outputs_.at(1).index, output.mean);
    fd.setFusionState(outputs_.at(2).index, output.invstd);
  }

  virtual void print(std::ostream& os, bool close_function = true) const final {
    RecordFunctor::print(os, false);
    os << ", training=" << (training_ ? "True" : "False");
    os << ", channels_last=" << (channels_last_ ? "True" : "False");
    if (close_function) {
      os << ")";
    }
  }

 private:
  bool training_;
  bool channels_last_;
};

//! Specialized Record Functor for the FusionDefinition's tensor_size op.
//! Uses the default hash() and print() methods of Record Functor

struct TensorSizesRecord : RecordFunctor {
  TensorSizesRecord(std::vector<State> args, std::vector<State> outputs)
      : RecordFunctor(
            std::move(args),
            std::move(outputs),
            "ops.tensor_sizes",
            RecordType::TensorSizes) {}
  virtual ~TensorSizesRecord() = default;
  virtual RecordFunctor* clone() final {
    return new TensorSizesRecord(*this);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (dynamic_cast<const TensorSizesRecord*>(&other)) {
      result = RecordFunctor::operator==(other);
    }
    return result;
  }

  void operator()(FusionDefinition& fd) final {
    auto arg = fd.getFusionState(args_.at(0).index)->as<TensorView>();
    auto sizes = tensor_sizes(arg);
    for (const auto idx : c10::irange(sizes.size())) {
      fd.setFusionState(outputs_.at(idx).index, sizes[idx]);
    }
  }
};

struct FullOpRecord : RecordFunctor {
  FullOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      std::vector<int64_t>& shape,
      PrimDataType dtype)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            "ops.full",
            RecordType::FullOp),
        shape_(std::move(shape)),
        dtype_(dtype) {}
  virtual ~FullOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new FullOpRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.
  //! | 31 --- 24 | 23 --------------------------  0 |
  //! | Dtype     | Shape hash code                  |
  virtual size_t hash() const final {
    auto result = RecordFunctor::hash();
    size_t shape_hash = 0;
    for (auto p : shape_) {
      shape_hash ^= static_cast<size_t>(p);
    }
    result |= ((static_cast<size_t>(dtype_) & 0xff) << 24);
    result |= (shape_hash & 0xffff);
    return result;
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const FullOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other) &&
          shape_ == child_ptr->shape_ && dtype_ == child_ptr->dtype_;
    }
    return result;
  }

  void operator()(FusionDefinition& fd) final {
    auto arg = fd.getFusionState(args_.at(0).index)->template as<Val>();

    std::vector<Val*> nvf_shape(shape_.size(), nullptr);
    for (const auto idx : c10::irange(shape_.size())) {
      nvf_shape[idx] = IrBuilder::create<Int>(shape_.at(idx));
    }
    auto output = full(nvf_shape, arg, dtype_);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  virtual void print(std::ostream& os, bool close_function = true)
      const override {
    RecordFunctor::print(os, false);
    os << ", shape=[";
    bool first_arg = true;
    for (auto p : shape_) {
      if (first_arg) {
        first_arg = false;
      } else {
        os << ", ";
      }
      os << p;
    }
    os << "]";
    os << ", dtype=" << dtypeToPyString(dtype_);
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! Represents shape of new tensor
  std::vector<int64_t> shape_;
  //! Type of output
  PrimDataType dtype_;
};

struct IotaOpRecord : RecordFunctor {
  IotaOpRecord(
      std::vector<State> _args,
      std::vector<State> _outputs,
      PrimDataType dtype)
      : RecordFunctor(
            std::move(_args),
            std::move(_outputs),
            "ops.iota",
            RecordType::IotaOp),
        dtype_(dtype) {}
  virtual ~IotaOpRecord() = default;
  virtual RecordFunctor* clone() final {
    return new IotaOpRecord(*this);
  }

  //! Child specific hash function in lower 32 bits.
  //! | 31 --------------------------------------  0 |
  //! | Dtype                                        |
  virtual size_t hash() const final {
    return RecordFunctor::hash() | static_cast<uint32_t>(dtype_);
  }

  virtual bool operator==(const RecordFunctor& other) const final {
    auto result = false;
    if (auto child_ptr = dynamic_cast<const IotaOpRecord*>(&other)) {
      result = RecordFunctor::operator==(other) && dtype_ == child_ptr->dtype_;
    }
    return result;
  }

  void operator()(FusionDefinition& fd) final {
    auto length = fd.getFusionState(args_.at(0).index);
    auto start = (args_.at(1).stype == StateType::Scalar)
        ? fd.getFusionState(args_.at(1).index)->as<Val>()
        : nullptr;
    auto step = (args_.at(2).stype == StateType::Scalar)
        ? fd.getFusionState(args_.at(2).index)->as<Val>()
        : nullptr;
    auto output = iota(length, start, step, dtype_);
    fd.setFusionState(outputs_.at(0).index, output);
  }

  virtual void print(std::ostream& os, bool close_function = true)
      const override {
    RecordFunctor::print(os, false);
    os << ", dtype=" << dtypeToPyString(dtype_);
    if (close_function) {
      os << ")";
    }
  }

 private:
  //! Type of output
  PrimDataType dtype_;
};

} // namespace nvfuser::python_frontend

//! Creating the template specialized hash and equal_to functions for a
//! RecordFunctor object in order to use hash maps (unordered_maps) in STL.
namespace std {
using namespace nvfuser::python_frontend;

template <>
struct hash<RecordFunctor*> {
  size_t operator()(const RecordFunctor* p) const {
    TORCH_CHECK(p, "The RecordFunctor Pointer for hashing is null!");
    return p->hash();
  }
};
template <>
struct equal_to<RecordFunctor*> {
  bool operator()(const RecordFunctor* p, const RecordFunctor* q) const {
    TORCH_CHECK(
        p,
        "The RecordFunctor Pointer on the lhs of an equality check is null!");
    TORCH_CHECK(
        q,
        "The RecordFunctor Pointer on the rhs of an equality check is null!");
    return p->operator==(*q);
  }
};
} // namespace std
