#include <ATen/Config.h>

#if AT_MKLDNN_ENABLED()

#include <ATen/Tensor.h>
#include <ATen/native/mkldnn/ConvPrepack.h>
#include <ATen/native/mkldnn/OpContext.h>
#include <ATen/native/mkldnn/Utils.h>
#include <torch/custom_class.h>
#include <torch/library.h>

namespace at {
namespace native {
namespace mkldnn {

using namespace internal::convolution;

static bool is_mkldnn_bf16_supported() {
#if defined(__aarch64__)
  return mkldnn_bf16_device_check_arm();
#else
  return mkldnn_bf16_device_check();
#endif
}

static bool is_mkldnn_fp16_supported() {
  return mkldnn_fp16_device_check();
}

constexpr bool is_mkldnn_acl_supported() {
  return AT_MKLDNN_ACL_ENABLED();
}

TORCH_LIBRARY(mkldnn, m) {
  m.class_<ConvOpContext>(TORCH_SELECTIVE_CLASS("ConvOpContext"))
      .def_pickle(
          [](const c10::intrusive_ptr<ConvOpContext>& op_context)
              -> SerializationTypeConvPrePack { // __getstate__
            return op_context->unpack();
          },
          [](SerializationTypeConvPrePack state)
              -> c10::intrusive_ptr<ConvOpContext> { // __setstate__
            return createConvPrePackOpContext(
                std::move(std::get<0>(state)),
                std::move(std::get<1>(state)),
                std::move(std::get<2>(state)),
                std::move(std::get<3>(state)),
                std::move(std::get<4>(state)),
                std::move(std::get<5>(state)),
                std::move(std::get<6>(state)),
                std::move(std::get<7>(state)));
          });

  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_linear_pointwise(Tensor X, Tensor W, Tensor? B, str attr, Scalar?[] scalars, str? algorithm) -> Tensor Y"));
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_linear_pointwise.binary(Tensor X, Tensor other, Tensor W, Tensor? B, str attr) -> Tensor Y"));
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_convolution_pointwise(Tensor X, Tensor W, Tensor? B, int[] padding, int[] stride, int[] dilation, int groups, str attr, Scalar?[] scalars, str? algorithm) -> Tensor Y"));
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_convolution_pointwise.binary(Tensor X, Tensor other, Tensor W, Tensor? B, int[] padding, int[] stride, int[] dilation, int groups, str binary_attr, Scalar? alpha, str? unary_attr, Scalar?[] unary_scalars, str? unary_algorithm) -> Tensor Y"));
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_convolution_pointwise_.binary(Tensor(a!) other, Tensor X, Tensor W, Tensor? B, int[] padding, int[] stride, int[] dilation, int groups, str binary_attr, Scalar? alpha, str? unary_attr, Scalar?[] unary_scalars, str? unary_algorithm) -> Tensor(a!) Y"));
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_convolution_transpose_pointwise(Tensor X, Tensor W, Tensor? B, int[] padding, int[] output_padding, int[] stride, int[] dilation, int groups, str attr, Scalar?[] scalars, str? algorithm) -> Tensor Y"));
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_reorder_convolution_transpose_weight(Tensor self, int[2] padding=0, int[2] output_padding=0, int[2] stride=1, int[2] dilation=1, int groups=1, int[]? input_size=None) -> Tensor Y"));
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_reorder_linear_weight(Tensor self, int? batch_size=None) -> Tensor Y"));
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_reorder_convolution_weight(Tensor self, int[2] padding=0, int[2] stride=1, int[2] dilation=1, int groups=1, int[]? input_size=None) -> Tensor Y"));
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_reorder_mkldnn_rnn_layer_weight(Tensor weight0, Tensor weight1, int hidden_size, bool reverse, bool has_biases, bool batch_first, int[]? input_size=None) -> Tensor[] Y"));
#if AT_ONEDNN_GRAPH_ENABLED()
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn::_graph_sdpa_pattern(Tensor Query, Tensor Key, Tensor Value, Tensor? scale, Tensor? inverse_scale, Tensor? attn_mask, Tensor? causal_mask, Tensor? causal_mask_value, bool transpose_query=False, bool transpose_key_twice=False, bool transpose_value=False, bool transpose_key_once=False, bool output_requires_transpose_and_reorder=False) -> Tensor Y"));
#endif
  m.def("_is_mkldnn_bf16_supported", &is_mkldnn_bf16_supported);
  m.def("_is_mkldnn_fp16_supported", &is_mkldnn_fp16_supported);
  m.def("_is_mkldnn_acl_supported", &is_mkldnn_acl_supported);
}

TORCH_LIBRARY(mkldnn_prepacked, m) {
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn_prepacked::conv2d_prepack(Tensor W, Tensor? B, int[2] stride, int[2] padding, int[2] dilation, int groups, int[4] input_size, str attr) -> __torch__.torch.classes.mkldnn.ConvOpContext"));

  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkldnn_prepacked::conv2d_run(Tensor X, __torch__.torch.classes.mkldnn.ConvOpContext W_prepack) -> Tensor Y"));
}

TORCH_LIBRARY_IMPL(mkldnn_prepacked, CPU, m) {
  m.impl(
      TORCH_SELECTIVE_NAME("mkldnn_prepacked::conv2d_prepack"),
      TORCH_FN(createConvPrePackOpContext));

  m.impl(
      TORCH_SELECTIVE_NAME("mkldnn_prepacked::conv2d_run"), TORCH_FN(conv_run));
}

} // namespace mkldnn
} // namespace native
} // namespace at

#endif // AT_MKLDNN_ENABLED()

#if AT_MKL_ENABLED() && AT_MKLDNN_ENABLED()

namespace at {
namespace native {
namespace mkl {

TORCH_LIBRARY(mkl, m) {
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkl::_mkl_reorder_linear_weight(Tensor X, int batch_size) -> Tensor"));
  m.def(TORCH_SELECTIVE_SCHEMA(
      "mkl::_mkl_linear(Tensor X, Tensor MKL_W, Tensor ORI_W, Tensor? B, int batch_size) -> Tensor"));
}

} // namespace mkl
} // namespace native
} // namespace at

#endif // AT_MKL_ENABLED && AT_MKLDNN_ENABLED
