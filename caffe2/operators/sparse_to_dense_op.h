#ifndef CAFFE2_OPERATORS_SPARSE_TO_DENSE_OP_H_
#define CAFFE2_OPERATORS_SPARSE_TO_DENSE_OP_H_

#include "caffe2/core/context.h"
#include "caffe2/core/operator.h"
#include "caffe2/utils/math.h"

namespace caffe2 {

template <class Context>
class SparseToDenseOp final : public Operator<Context> {
 public:
  USE_OPERATOR_CONTEXT_FUNCTIONS;
  USE_DISPATCH_HELPER;

  SparseToDenseOp(const OperatorDef& operator_def, Workspace* ws)
      : Operator<Context>(operator_def, ws),
        output_first_dim_(
            OperatorBase::GetSingleArgument<int>("output_first_dim", 0)) {}

  bool RunOnDevice() override {
    return DispatchHelper<TensorTypes<int32_t, int64_t>>::call(
        this, Input(INDICES));
  }

 private:
  template <typename TInd>
  int GetOutputFirstDim(
      const TInd* sparse_indices_vec,
      const int32_t sparse_indices_len) {
    if (output_first_dim_ > 0) {
      CAFFE_ENFORCE_EQ(InputSize(), 2);
      return output_first_dim_;
    }
    if (InputSize() == 3) {
      auto& data_to_infer_dim = Input(DATA_TO_INFER_DIM);
      CAFFE_ENFORCE_GE(data_to_infer_dim.ndim(), 1);
      return data_to_infer_dim.dim32(0);
    }
    if (sparse_indices_len <= 0) {
      return 0;
    }
    return 1 +
        *std::max_element(
               sparse_indices_vec, sparse_indices_vec + sparse_indices_len);
  }

  template <typename TInd>
  bool DoRunWithType() {
    return DispatchHelper<
        TensorTypes2<
            float,
            int32_t,
            int64_t,
            GenericTensorImplementation>,
        TInd>::call(this, Input(VALUES));
  }

  template <typename TInd, typename TData>
  bool DoRunWithType2() {
    auto& sparse_indices = Input(INDICES);
    CAFFE_ENFORCE_EQ(sparse_indices.ndim(), 1);
    auto& sparse_values = Input(VALUES);
    CAFFE_ENFORCE_GE(sparse_values.ndim(), 1);
    CAFFE_ENFORCE_EQ(sparse_indices.size(), sparse_values.dim(0));

    const TInd* sparse_indices_vec = sparse_indices.template data<TInd>();
    const int32_t sparse_indices_len = sparse_indices.dim32(0);
    const int output_first_dim =
        GetOutputFirstDim(sparse_indices_vec, sparse_indices_len);

    auto shape = sparse_values.dims();
    shape[0] = output_first_dim;
    auto* output = Output(0);
    output->Resize(shape);

    TData* output_data = output->template mutable_data<TData>();
    memset(output_data, 0, output->nbytes());
    const auto block_nitems = sparse_values.size_from_dim(1);
    const TData* sparse_values_vec = sparse_values.template data<TData>();

    for (int32_t i = 0; i < sparse_indices_len; i++) {
      const TInd idx = sparse_indices_vec[i];
      CAFFE_ENFORCE_GE(idx, 0);
      CAFFE_ENFORCE_LT(idx, output_first_dim);
      math::Add(
          block_nitems,
          output_data + idx * block_nitems,
          sparse_values_vec + i * block_nitems,
          output_data + idx * block_nitems,
          &context_);
    }
    return true;
  }

  template <typename TInd>
  bool DoRunWithOtherType2() {
    CAFFE_THROW(
        "SparseToDense is not implemented on tensor of type ",
        Input(VALUES).meta().name(),
        "Consider adding it a type in the list DispatchHelper or implementing "
        "a generic version (which won't work for duplicated indices though)");
  }

 private:
  int output_first_dim_;

  INPUT_TAGS(INDICES, VALUES, DATA_TO_INFER_DIM);
};

} // namespace caffe2

#endif // CAFFE2_OPERATORS_SPARSE_TO_DENSE_OP_H_
