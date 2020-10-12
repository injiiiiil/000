#include <torch/library.h>

#include <ATen/core/boxing/KernelFunction.h>

using torch::CppFunction;

TORCH_LIBRARY_IMPL(_, Named, m) {
  m.fallback(CppFunction::makeNamedNotSupported());
}

TORCH_LIBRARY_IMPL(aten, Named, m) {
  m.impl("_bmm", CppFunction::makeFallthrough());
  m.impl("_bmm.out", CppFunction::makeFallthrough());
  m.impl("_cdist_forward", CppFunction::makeFallthrough());
  m.impl("_fused_dropout", CppFunction::makeFallthrough());
  m.impl("_local_scalar_dense", CppFunction::makeFallthrough());
  m.impl("_sparse_log_softmax.Dimname", CppFunction::makeFallthrough());
  m.impl("_sparse_log_softmax.int", CppFunction::makeFallthrough());
  m.impl("_sparse_softmax.Dimname", CppFunction::makeFallthrough());
  m.impl("_sparse_softmax.int", CppFunction::makeFallthrough());
  m.impl("_std", CppFunction::makeFallthrough());
  m.impl("_var", CppFunction::makeFallthrough());
  m.impl("abs", CppFunction::makeFallthrough());
  m.impl("abs.out", CppFunction::makeFallthrough());
  m.impl("abs_", CppFunction::makeFallthrough());
  m.impl("absolute", CppFunction::makeFallthrough());
  m.impl("absolute.out", CppFunction::makeFallthrough());
  m.impl("absolute_", CppFunction::makeFallthrough());
  m.impl("acos", CppFunction::makeFallthrough());
  m.impl("acos.out", CppFunction::makeFallthrough());
  m.impl("acos_", CppFunction::makeFallthrough());
  m.impl("acosh", CppFunction::makeFallthrough());
  m.impl("acosh.out", CppFunction::makeFallthrough());
  m.impl("acosh_", CppFunction::makeFallthrough());
  m.impl("add.Scalar", CppFunction::makeFallthrough());
  m.impl("add.Tensor", CppFunction::makeFallthrough());
  m.impl("add.out", CppFunction::makeFallthrough());
  m.impl("add_.Scalar", CppFunction::makeFallthrough());
  m.impl("add_.Tensor", CppFunction::makeFallthrough());
  m.impl("add_relu.Tensor", CppFunction::makeFallthrough());
  m.impl("add_relu.out", CppFunction::makeFallthrough());
  m.impl("add_relu_.Tensor", CppFunction::makeFallthrough());
  m.impl("addcdiv", CppFunction::makeFallthrough());
  m.impl("addcdiv.out", CppFunction::makeFallthrough());
  m.impl("addcdiv_", CppFunction::makeFallthrough());
  m.impl("addcmul", CppFunction::makeFallthrough());
  m.impl("addcmul.out", CppFunction::makeFallthrough());
  m.impl("addcmul_", CppFunction::makeFallthrough());
  m.impl("addmm", CppFunction::makeFallthrough());
  m.impl("addmm.out", CppFunction::makeFallthrough());
  m.impl("addmm_", CppFunction::makeFallthrough());
  m.impl("addmv", CppFunction::makeFallthrough());
  m.impl("addmv.out", CppFunction::makeFallthrough());
  m.impl("addmv_", CppFunction::makeFallthrough());
  m.impl("alias", CppFunction::makeFallthrough());
  m.impl("align_as", CppFunction::makeFallthrough());
  m.impl("align_tensors", CppFunction::makeFallthrough());
  m.impl("align_to", CppFunction::makeFallthrough());
  m.impl("align_to.ellipsis_idx", CppFunction::makeFallthrough());
  m.impl("all", CppFunction::makeFallthrough());
  m.impl("angle", CppFunction::makeFallthrough());
  m.impl("angle.out", CppFunction::makeFallthrough());
  m.impl("any", CppFunction::makeFallthrough());
  m.impl("arccosh", CppFunction::makeFallthrough());
  m.impl("arccosh.out", CppFunction::makeFallthrough());
  m.impl("arccosh_", CppFunction::makeFallthrough());
  m.impl("as_strided", CppFunction::makeFallthrough());
  m.impl("asin", CppFunction::makeFallthrough());
  m.impl("asin.out", CppFunction::makeFallthrough());
  m.impl("asin_", CppFunction::makeFallthrough());
  m.impl("asinh", CppFunction::makeFallthrough());
  m.impl("asinh.out", CppFunction::makeFallthrough());
  m.impl("asinh_", CppFunction::makeFallthrough());
  m.impl("atan", CppFunction::makeFallthrough());
  m.impl("atan.out", CppFunction::makeFallthrough());
  m.impl("atan2", CppFunction::makeFallthrough());
  m.impl("atan2.out", CppFunction::makeFallthrough());
  m.impl("atan2_", CppFunction::makeFallthrough());
  m.impl("atan_", CppFunction::makeFallthrough());
  m.impl("atanh", CppFunction::makeFallthrough());
  m.impl("atanh.out", CppFunction::makeFallthrough());
  m.impl("atanh_", CppFunction::makeFallthrough());
  m.impl("bernoulli", CppFunction::makeFallthrough());
  m.impl("bernoulli.out", CppFunction::makeFallthrough());
  m.impl("bernoulli_.Tensor", CppFunction::makeFallthrough());
  m.impl("bernoulli_.float", CppFunction::makeFallthrough());
  m.impl("bitwise_not", CppFunction::makeFallthrough());
  m.impl("bitwise_not.out", CppFunction::makeFallthrough());
  m.impl("bitwise_not_", CppFunction::makeFallthrough());
  m.impl("bmm", CppFunction::makeFallthrough());
  m.impl("bmm.out", CppFunction::makeFallthrough());
  m.impl("cat", CppFunction::makeFallthrough());
  m.impl("cat.names", CppFunction::makeFallthrough());
  m.impl("cat.names_out", CppFunction::makeFallthrough());
  m.impl("cat.out", CppFunction::makeFallthrough());
  m.impl("cauchy_", CppFunction::makeFallthrough());
  m.impl("cdist", CppFunction::makeFallthrough());
  m.impl("ceil", CppFunction::makeFallthrough());
  m.impl("ceil.out", CppFunction::makeFallthrough());
  m.impl("ceil_", CppFunction::makeFallthrough());
  m.impl("chunk", CppFunction::makeFallthrough());
  m.impl("clamp", CppFunction::makeFallthrough());
  m.impl("clamp.out", CppFunction::makeFallthrough());
  m.impl("clamp_", CppFunction::makeFallthrough());
  m.impl("clamp_max", CppFunction::makeFallthrough());
  m.impl("clamp_max.out", CppFunction::makeFallthrough());
  m.impl("clamp_max_", CppFunction::makeFallthrough());
  m.impl("clamp_min", CppFunction::makeFallthrough());
  m.impl("clamp_min.out", CppFunction::makeFallthrough());
  m.impl("clamp_min_", CppFunction::makeFallthrough());
  m.impl("clone", CppFunction::makeFallthrough());
  m.impl("conj", CppFunction::makeFallthrough());
  m.impl("conj.out", CppFunction::makeFallthrough());
  m.impl("contiguous", CppFunction::makeFallthrough());
  m.impl("copy_", CppFunction::makeFallthrough());
  m.impl("copy_imag", CppFunction::makeFallthrough());
  m.impl("copy_imag.out", CppFunction::makeFallthrough());
  m.impl("copy_real", CppFunction::makeFallthrough());
  m.impl("copy_real.out", CppFunction::makeFallthrough());
  m.impl("cos", CppFunction::makeFallthrough());
  m.impl("cos.out", CppFunction::makeFallthrough());
  m.impl("cos_", CppFunction::makeFallthrough());
  m.impl("cosh", CppFunction::makeFallthrough());
  m.impl("cosh.out", CppFunction::makeFallthrough());
  m.impl("cosh_", CppFunction::makeFallthrough());
  m.impl("cummax", CppFunction::makeFallthrough());
  m.impl("cummax.dimname", CppFunction::makeFallthrough());
  m.impl("cummax.dimname_out", CppFunction::makeFallthrough());
  m.impl("cummax.out", CppFunction::makeFallthrough());
  m.impl("cummin", CppFunction::makeFallthrough());
  m.impl("cummin.dimname", CppFunction::makeFallthrough());
  m.impl("cummin.dimname_out", CppFunction::makeFallthrough());
  m.impl("cummin.out", CppFunction::makeFallthrough());
  m.impl("cumprod", CppFunction::makeFallthrough());
  m.impl("cumprod.dimname", CppFunction::makeFallthrough());
  m.impl("cumprod.dimname_out", CppFunction::makeFallthrough());
  m.impl("cumprod.out", CppFunction::makeFallthrough());
  m.impl("cumsum", CppFunction::makeFallthrough());
  m.impl("cumsum.dimname", CppFunction::makeFallthrough());
  m.impl("cumsum.dimname_out", CppFunction::makeFallthrough());
  m.impl("cumsum.out", CppFunction::makeFallthrough());
  m.impl("deg2rad", CppFunction::makeFallthrough());
  m.impl("deg2rad.out", CppFunction::makeFallthrough());
  m.impl("deg2rad_", CppFunction::makeFallthrough());
  m.impl("detach", CppFunction::makeFallthrough());
  m.impl("detach_", CppFunction::makeFallthrough());
  m.impl("diagonal", CppFunction::makeFallthrough());
  m.impl("diagonal.Dimname", CppFunction::makeFallthrough());
  m.impl("digamma", CppFunction::makeFallthrough());
  m.impl("digamma.out", CppFunction::makeFallthrough());
  m.impl("digamma_", CppFunction::makeFallthrough());
  m.impl("div.Scalar", CppFunction::makeFallthrough());
  m.impl("div.Tensor", CppFunction::makeFallthrough());
  m.impl("div.out", CppFunction::makeFallthrough());
  m.impl("div_.Scalar", CppFunction::makeFallthrough());
  m.impl("div_.Tensor", CppFunction::makeFallthrough());
  m.impl("dot", CppFunction::makeFallthrough());
  m.impl("dot.out", CppFunction::makeFallthrough());
  m.impl("dropout", CppFunction::makeFallthrough());
  m.impl("dropout_", CppFunction::makeFallthrough());
  m.impl("empty_like", CppFunction::makeFallthrough());
  m.impl("eq.Scalar", CppFunction::makeFallthrough());
  m.impl("eq.Scalar_out", CppFunction::makeFallthrough());
  m.impl("eq.Tensor", CppFunction::makeFallthrough());
  m.impl("eq.Tensor_out", CppFunction::makeFallthrough());
  m.impl("equal", CppFunction::makeFallthrough());
  m.impl("erf", CppFunction::makeFallthrough());
  m.impl("erf.out", CppFunction::makeFallthrough());
  m.impl("erf_", CppFunction::makeFallthrough());
  m.impl("erfc", CppFunction::makeFallthrough());
  m.impl("erfc.out", CppFunction::makeFallthrough());
  m.impl("erfc_", CppFunction::makeFallthrough());
  m.impl("erfinv", CppFunction::makeFallthrough());
  m.impl("erfinv.out", CppFunction::makeFallthrough());
  m.impl("erfinv_", CppFunction::makeFallthrough());
  m.impl("exp", CppFunction::makeFallthrough());
  m.impl("exp.out", CppFunction::makeFallthrough());
  m.impl("exp_", CppFunction::makeFallthrough());
  m.impl("expand", CppFunction::makeFallthrough());
  m.impl("expm1", CppFunction::makeFallthrough());
  m.impl("expm1.out", CppFunction::makeFallthrough());
  m.impl("expm1_", CppFunction::makeFallthrough());
  m.impl("exponential_", CppFunction::makeFallthrough());
  m.impl("fill_.Scalar", CppFunction::makeFallthrough());
  m.impl("fill_.Tensor", CppFunction::makeFallthrough());
  m.impl("flatten.DimnameList", CppFunction::makeFallthrough());
  m.impl("flatten.named_out_dim", CppFunction::makeFallthrough());
  m.impl("flatten.using_ints", CppFunction::makeFallthrough());
  m.impl("flatten.using_names", CppFunction::makeFallthrough());
  m.impl("floor", CppFunction::makeFallthrough());
  m.impl("floor.out", CppFunction::makeFallthrough());
  m.impl("floor_", CppFunction::makeFallthrough());
  m.impl("floor_divide", CppFunction::makeFallthrough());
  m.impl("floor_divide.Scalar", CppFunction::makeFallthrough());
  m.impl("floor_divide.out", CppFunction::makeFallthrough());
  m.impl("floor_divide_.Scalar", CppFunction::makeFallthrough());
  m.impl("floor_divide_.Tensor", CppFunction::makeFallthrough());
  m.impl("frac", CppFunction::makeFallthrough());
  m.impl("frac.out", CppFunction::makeFallthrough());
  m.impl("frac_", CppFunction::makeFallthrough());
  m.impl("full_like", CppFunction::makeFallthrough());
  m.impl("ge.Scalar", CppFunction::makeFallthrough());
  m.impl("ge.Scalar_out", CppFunction::makeFallthrough());
  m.impl("ge.Tensor", CppFunction::makeFallthrough());
  m.impl("ge.Tensor_out", CppFunction::makeFallthrough());
  m.impl("geometric_", CppFunction::makeFallthrough());
  m.impl("gt.Scalar", CppFunction::makeFallthrough());
  m.impl("gt.Scalar_out", CppFunction::makeFallthrough());
  m.impl("gt.Tensor", CppFunction::makeFallthrough());
  m.impl("gt.Tensor_out", CppFunction::makeFallthrough());
  m.impl("hypot", CppFunction::makeFallthrough());
  m.impl("hypot.out", CppFunction::makeFallthrough());
  m.impl("hypot_", CppFunction::makeFallthrough());
  m.impl("i0", CppFunction::makeFallthrough());
  m.impl("i0.out", CppFunction::makeFallthrough());
  m.impl("i0_", CppFunction::makeFallthrough());
  m.impl("igamma", CppFunction::makeFallthrough());
  m.impl("igamma.out", CppFunction::makeFallthrough());
  m.impl("igamma_", CppFunction::makeFallthrough());
  m.impl("imag", CppFunction::makeFallthrough());
  m.impl("index_fill.Dimname_Scalar", CppFunction::makeFallthrough());
  m.impl("index_fill.Dimname_Tensor", CppFunction::makeFallthrough());
  m.impl("index_fill.int_Scalar", CppFunction::makeFallthrough());
  m.impl("index_fill.int_Tensor", CppFunction::makeFallthrough());
  m.impl("index_fill_.Dimname_Scalar", CppFunction::makeFallthrough());
  m.impl("index_fill_.Dimname_Tensor", CppFunction::makeFallthrough());
  m.impl("index_fill_.int_Scalar", CppFunction::makeFallthrough());
  m.impl("index_fill_.int_Tensor", CppFunction::makeFallthrough());
  m.impl("is_coalesced", CppFunction::makeFallthrough());
  m.impl("is_complex", CppFunction::makeFallthrough());
  m.impl("is_floating_point", CppFunction::makeFallthrough());
  m.impl("is_nonzero", CppFunction::makeFallthrough());
  m.impl("is_pinned", CppFunction::makeFallthrough());
  m.impl("is_same_size", CppFunction::makeFallthrough());
  m.impl("is_signed", CppFunction::makeFallthrough());
  m.impl("isfinite", CppFunction::makeFallthrough());
  m.impl("isinf", CppFunction::makeFallthrough());
  m.impl("isnan", CppFunction::makeFallthrough());
  m.impl("item", CppFunction::makeFallthrough());
  m.impl("kthvalue", CppFunction::makeFallthrough());
  m.impl("kthvalue.dimname", CppFunction::makeFallthrough());
  m.impl("kthvalue.dimname_out", CppFunction::makeFallthrough());
  m.impl("kthvalue.values", CppFunction::makeFallthrough());
  m.impl("le.Scalar", CppFunction::makeFallthrough());
  m.impl("le.Scalar_out", CppFunction::makeFallthrough());
  m.impl("le.Tensor", CppFunction::makeFallthrough());
  m.impl("le.Tensor_out", CppFunction::makeFallthrough());
  m.impl("lgamma", CppFunction::makeFallthrough());
  m.impl("lgamma.out", CppFunction::makeFallthrough());
  m.impl("lgamma_", CppFunction::makeFallthrough());
  m.impl("log", CppFunction::makeFallthrough());
  m.impl("log.out", CppFunction::makeFallthrough());
  m.impl("log10", CppFunction::makeFallthrough());
  m.impl("log10.out", CppFunction::makeFallthrough());
  m.impl("log10_", CppFunction::makeFallthrough());
  m.impl("log1p", CppFunction::makeFallthrough());
  m.impl("log1p.out", CppFunction::makeFallthrough());
  m.impl("log1p_", CppFunction::makeFallthrough());
  m.impl("log2", CppFunction::makeFallthrough());
  m.impl("log2.out", CppFunction::makeFallthrough());
  m.impl("log2_", CppFunction::makeFallthrough());
  m.impl("log_", CppFunction::makeFallthrough());
  m.impl("log_normal_", CppFunction::makeFallthrough());
  m.impl("log_softmax.Dimname", CppFunction::makeFallthrough());
  m.impl("log_softmax.int", CppFunction::makeFallthrough());
  m.impl("logaddexp", CppFunction::makeFallthrough());
  m.impl("logaddexp.out", CppFunction::makeFallthrough());
  m.impl("logaddexp2", CppFunction::makeFallthrough());
  m.impl("logaddexp2.out", CppFunction::makeFallthrough());
  m.impl("logcumsumexp", CppFunction::makeFallthrough());
  m.impl("logcumsumexp.dimname", CppFunction::makeFallthrough());
  m.impl("logcumsumexp.dimname_out", CppFunction::makeFallthrough());
  m.impl("logcumsumexp.out", CppFunction::makeFallthrough());
  m.impl("logical_and", CppFunction::makeFallthrough());
  m.impl("logical_and.out", CppFunction::makeFallthrough());
  m.impl("logical_and_", CppFunction::makeFallthrough());
  m.impl("logical_not", CppFunction::makeFallthrough());
  m.impl("logical_not.out", CppFunction::makeFallthrough());
  m.impl("logical_not_", CppFunction::makeFallthrough());
  m.impl("logical_or", CppFunction::makeFallthrough());
  m.impl("logical_or.out", CppFunction::makeFallthrough());
  m.impl("logical_or_", CppFunction::makeFallthrough());
  m.impl("logical_xor", CppFunction::makeFallthrough());
  m.impl("logical_xor.out", CppFunction::makeFallthrough());
  m.impl("logical_xor_", CppFunction::makeFallthrough());
  m.impl("logsumexp", CppFunction::makeFallthrough());
  m.impl("logsumexp.names", CppFunction::makeFallthrough());
  m.impl("logsumexp.names_out", CppFunction::makeFallthrough());
  m.impl("logsumexp.out", CppFunction::makeFallthrough());
  m.impl("lt.Scalar", CppFunction::makeFallthrough());
  m.impl("lt.Scalar_out", CppFunction::makeFallthrough());
  m.impl("lt.Tensor", CppFunction::makeFallthrough());
  m.impl("lt.Tensor_out", CppFunction::makeFallthrough());
  m.impl("masked_fill.Scalar", CppFunction::makeFallthrough());
  m.impl("masked_fill.Tensor", CppFunction::makeFallthrough());
  m.impl("masked_fill_.Scalar", CppFunction::makeFallthrough());
  m.impl("masked_fill_.Tensor", CppFunction::makeFallthrough());
  m.impl("masked_select", CppFunction::makeFallthrough());
  m.impl("masked_select.out", CppFunction::makeFallthrough());
  m.impl("matmul", CppFunction::makeFallthrough());
  m.impl("matmul.out", CppFunction::makeFallthrough());
  m.impl("max", CppFunction::makeFallthrough());
  m.impl("max.dim", CppFunction::makeFallthrough());
  m.impl("max.dim_max", CppFunction::makeFallthrough());
  m.impl("max.names_dim", CppFunction::makeFallthrough());
  m.impl("max.names_dim_max", CppFunction::makeFallthrough());
  m.impl("max_pool1d", CppFunction::makeFallthrough());
  m.impl("max_pool1d_with_indices", CppFunction::makeFallthrough());
  m.impl("max_pool2d", CppFunction::makeFallthrough());
  m.impl("max_pool2d_with_indices", CppFunction::makeFallthrough());
  m.impl("max_pool3d", CppFunction::makeFallthrough());
  m.impl("max_pool3d_with_indices", CppFunction::makeFallthrough());
  m.impl("mean", CppFunction::makeFallthrough());
  m.impl("mean.dim", CppFunction::makeFallthrough());
  m.impl("mean.names_dim", CppFunction::makeFallthrough());
  m.impl("mean.names_out", CppFunction::makeFallthrough());
  m.impl("mean.out", CppFunction::makeFallthrough());
  m.impl("median", CppFunction::makeFallthrough());
  m.impl("median.dim", CppFunction::makeFallthrough());
  m.impl("median.dim_values", CppFunction::makeFallthrough());
  m.impl("median.names_dim", CppFunction::makeFallthrough());
  m.impl("median.names_dim_values", CppFunction::makeFallthrough());
  m.impl("nanmedian", CppFunction::makeFallthrough());
  m.impl("nanmedian.dim", CppFunction::makeFallthrough());
  m.impl("nanmedian.dim_values", CppFunction::makeFallthrough());
  m.impl("nanmedian.names_dim", CppFunction::makeFallthrough());
  m.impl("nanmedian.names_dim_values", CppFunction::makeFallthrough());
  m.impl("min", CppFunction::makeFallthrough());
  m.impl("min.dim", CppFunction::makeFallthrough());
  m.impl("min.dim_min", CppFunction::makeFallthrough());
  m.impl("min.names_dim", CppFunction::makeFallthrough());
  m.impl("min.names_dim_min", CppFunction::makeFallthrough());
  m.impl("mm", CppFunction::makeFallthrough());
  m.impl("mm.out", CppFunction::makeFallthrough());
  m.impl("mode", CppFunction::makeFallthrough());
  m.impl("mode.dimname", CppFunction::makeFallthrough());
  m.impl("mode.dimname_out", CppFunction::makeFallthrough());
  m.impl("mode.values", CppFunction::makeFallthrough());
  m.impl("mul.Tensor", CppFunction::makeFallthrough());
  m.impl("mul.out", CppFunction::makeFallthrough());
  m.impl("mul_.Tensor", CppFunction::makeFallthrough());
  m.impl("mv", CppFunction::makeFallthrough());
  m.impl("mv.out", CppFunction::makeFallthrough());
  m.impl("narrow", CppFunction::makeFallthrough());
  m.impl("narrow.Tensor", CppFunction::makeFallthrough());
  m.impl("ne.Scalar", CppFunction::makeFallthrough());
  m.impl("ne.Scalar_out", CppFunction::makeFallthrough());
  m.impl("ne.Tensor", CppFunction::makeFallthrough());
  m.impl("ne.Tensor_out", CppFunction::makeFallthrough());
  m.impl("neg", CppFunction::makeFallthrough());
  m.impl("neg.out", CppFunction::makeFallthrough());
  m.impl("neg_", CppFunction::makeFallthrough());
  m.impl("nextafter", CppFunction::makeFallthrough());
  m.impl("nextafter.out", CppFunction::makeFallthrough());
  m.impl("nextafter_", CppFunction::makeFallthrough());
  m.impl("normal_", CppFunction::makeFallthrough());
  m.impl("ones_like", CppFunction::makeFallthrough());
  m.impl("output_nr", CppFunction::makeFallthrough());
  m.impl("polygamma", CppFunction::makeFallthrough());
  m.impl("polygamma.out", CppFunction::makeFallthrough());
  m.impl("polygamma_", CppFunction::makeFallthrough());
  m.impl("pow.Scalar", CppFunction::makeFallthrough());
  m.impl("pow.Scalar_out", CppFunction::makeFallthrough());
  m.impl("pow.Tensor_Scalar", CppFunction::makeFallthrough());
  m.impl("pow.Tensor_Scalar_out", CppFunction::makeFallthrough());
  m.impl("pow.Tensor_Tensor", CppFunction::makeFallthrough());
  m.impl("pow.Tensor_Tensor_out", CppFunction::makeFallthrough());
  m.impl("pow_.Scalar", CppFunction::makeFallthrough());
  m.impl("pow_.Tensor", CppFunction::makeFallthrough());
  m.impl("prod", CppFunction::makeFallthrough());
  m.impl("prod.Dimname_out", CppFunction::makeFallthrough());
  m.impl("prod.dim_Dimname", CppFunction::makeFallthrough());
  m.impl("prod.dim_int", CppFunction::makeFallthrough());
  m.impl("prod.int_out", CppFunction::makeFallthrough());
  m.impl("rad2deg", CppFunction::makeFallthrough());
  m.impl("rad2deg.out", CppFunction::makeFallthrough());
  m.impl("rad2deg_", CppFunction::makeFallthrough());
  m.impl("rand_like", CppFunction::makeFallthrough());
  m.impl("randn_like", CppFunction::makeFallthrough());
  m.impl("random_", CppFunction::makeFallthrough());
  m.impl("random_.from", CppFunction::makeFallthrough());
  m.impl("random_.to", CppFunction::makeFallthrough());
  m.impl("real", CppFunction::makeFallthrough());
  m.impl("reciprocal", CppFunction::makeFallthrough());
  m.impl("reciprocal.out", CppFunction::makeFallthrough());
  m.impl("reciprocal_", CppFunction::makeFallthrough());
  m.impl("refine_names", CppFunction::makeFallthrough());
  m.impl("relu", CppFunction::makeFallthrough());
  m.impl("relu_", CppFunction::makeFallthrough());
  m.impl("rename", CppFunction::makeFallthrough());
  m.impl("rename_", CppFunction::makeFallthrough());
  m.impl("reshape", CppFunction::makeFallthrough());
  m.impl("resize_", CppFunction::makeFallthrough());
  m.impl("resize_as_", CppFunction::makeFallthrough());
  m.impl("result_type.Scalar", CppFunction::makeFallthrough());
  m.impl("result_type.Scalar_Tensor", CppFunction::makeFallthrough());
  m.impl("result_type.Tensor", CppFunction::makeFallthrough());
  m.impl("round", CppFunction::makeFallthrough());
  m.impl("round.out", CppFunction::makeFallthrough());
  m.impl("round_", CppFunction::makeFallthrough());
  m.impl("rsqrt", CppFunction::makeFallthrough());
  m.impl("rsqrt.out", CppFunction::makeFallthrough());
  m.impl("rsqrt_", CppFunction::makeFallthrough());
  m.impl("rsub.Scalar", CppFunction::makeFallthrough());
  m.impl("rsub.Tensor", CppFunction::makeFallthrough());
  m.impl("select.Dimname", CppFunction::makeFallthrough());
  m.impl("select.int", CppFunction::makeFallthrough());
  m.impl("sigmoid", CppFunction::makeFallthrough());
  m.impl("sigmoid.out", CppFunction::makeFallthrough());
  m.impl("sigmoid_", CppFunction::makeFallthrough());
  m.impl("sign", CppFunction::makeFallthrough());
  m.impl("sign.out", CppFunction::makeFallthrough());
  m.impl("sign_", CppFunction::makeFallthrough());
  m.impl("signbit", CppFunction::makeFallthrough());
  m.impl("signbit.out", CppFunction::makeFallthrough());
  m.impl("sin", CppFunction::makeFallthrough());
  m.impl("sin.out", CppFunction::makeFallthrough());
  m.impl("sin_", CppFunction::makeFallthrough());
  m.impl("sinh", CppFunction::makeFallthrough());
  m.impl("sinh.out", CppFunction::makeFallthrough());
  m.impl("sinh_", CppFunction::makeFallthrough());
  m.impl("size.Dimname", CppFunction::makeFallthrough());
  m.impl("size.int", CppFunction::makeFallthrough());
  m.impl("slice.Tensor", CppFunction::makeFallthrough());
  m.impl("softmax.Dimname", CppFunction::makeFallthrough());
  m.impl("softmax.int", CppFunction::makeFallthrough());
  m.impl("split.Tensor", CppFunction::makeFallthrough());
  m.impl("split_with_sizes", CppFunction::makeFallthrough());
  m.impl("sqrt", CppFunction::makeFallthrough());
  m.impl("sqrt.out", CppFunction::makeFallthrough());
  m.impl("sqrt_", CppFunction::makeFallthrough());
  m.impl("square", CppFunction::makeFallthrough());
  m.impl("square_", CppFunction::makeFallthrough());
  m.impl("squeeze", CppFunction::makeFallthrough());
  m.impl("squeeze.dim", CppFunction::makeFallthrough());
  m.impl("squeeze.dimname", CppFunction::makeFallthrough());
  m.impl("std", CppFunction::makeFallthrough());
  m.impl("std.dim", CppFunction::makeFallthrough());
  m.impl("std.names_dim", CppFunction::makeFallthrough());
  m.impl("std.names_out", CppFunction::makeFallthrough());
  m.impl("std.out", CppFunction::makeFallthrough());
  m.impl("std_mean", CppFunction::makeFallthrough());
  m.impl("std_mean.dim", CppFunction::makeFallthrough());
  m.impl("std_mean.names_dim", CppFunction::makeFallthrough());
  m.impl("stride.Dimname", CppFunction::makeFallthrough());
  m.impl("stride.int", CppFunction::makeFallthrough());
  m.impl("sub.Scalar", CppFunction::makeFallthrough());
  m.impl("sub.Tensor", CppFunction::makeFallthrough());
  m.impl("sub.out", CppFunction::makeFallthrough());
  m.impl("sub_.Scalar", CppFunction::makeFallthrough());
  m.impl("sub_.Tensor", CppFunction::makeFallthrough());
  m.impl("sum", CppFunction::makeFallthrough());
  m.impl("sum.DimnameList_out", CppFunction::makeFallthrough());
  m.impl("sum.IntList_out", CppFunction::makeFallthrough());
  m.impl("sum.dim_DimnameList", CppFunction::makeFallthrough());
  m.impl("sum.dim_IntList", CppFunction::makeFallthrough());
  m.impl("t", CppFunction::makeFallthrough());
  m.impl("tan", CppFunction::makeFallthrough());
  m.impl("tan.out", CppFunction::makeFallthrough());
  m.impl("tan_", CppFunction::makeFallthrough());
  m.impl("tanh", CppFunction::makeFallthrough());
  m.impl("tanh.out", CppFunction::makeFallthrough());
  m.impl("tanh_", CppFunction::makeFallthrough());
  m.impl("tensor_split.indices", CppFunction::makeFallthrough());
  m.impl("tensor_split.sections", CppFunction::makeFallthrough());
  m.impl("threshold", CppFunction::makeFallthrough());
  m.impl("threshold.out", CppFunction::makeFallthrough());
  m.impl("threshold_", CppFunction::makeFallthrough());
  m.impl("to.device", CppFunction::makeFallthrough());
  m.impl("to.dtype", CppFunction::makeFallthrough());
  m.impl("to.dtype_layout", CppFunction::makeFallthrough());
  m.impl("transpose.Dimname", CppFunction::makeFallthrough());
  m.impl("transpose.int", CppFunction::makeFallthrough());
  m.impl("true_divide.Scalar", CppFunction::makeFallthrough());
  m.impl("true_divide.Tensor", CppFunction::makeFallthrough());
  m.impl("true_divide.out", CppFunction::makeFallthrough());
  m.impl("true_divide_.Scalar", CppFunction::makeFallthrough());
  m.impl("true_divide_.Tensor", CppFunction::makeFallthrough());
  m.impl("trunc", CppFunction::makeFallthrough());
  m.impl("trunc.out", CppFunction::makeFallthrough());
  m.impl("trunc_", CppFunction::makeFallthrough());
  m.impl("unbind.Dimname", CppFunction::makeFallthrough());
  m.impl("unbind.int", CppFunction::makeFallthrough());
  m.impl("unflatten.Dimname", CppFunction::makeFallthrough());
  m.impl("unflatten.int", CppFunction::makeFallthrough());
  m.impl("uniform_", CppFunction::makeFallthrough());
  m.impl("unsafe_chunk", CppFunction::makeFallthrough());
  m.impl("unsafe_split.Tensor", CppFunction::makeFallthrough());
  m.impl("unsafe_split_with_sizes", CppFunction::makeFallthrough());
  m.impl("vander", CppFunction::makeFallthrough());
  m.impl("var", CppFunction::makeFallthrough());
  m.impl("var.dim", CppFunction::makeFallthrough());
  m.impl("var.names_dim", CppFunction::makeFallthrough());
  m.impl("var.names_out", CppFunction::makeFallthrough());
  m.impl("var.out", CppFunction::makeFallthrough());
  m.impl("var_mean", CppFunction::makeFallthrough());
  m.impl("var_mean.dim", CppFunction::makeFallthrough());
  m.impl("var_mean.names_dim", CppFunction::makeFallthrough());
  m.impl("zero_", CppFunction::makeFallthrough());
  m.impl("zeros_like", CppFunction::makeFallthrough());

  // These weren't marked as supporting named tensors but were implicitly
  // supported because they were manually registered.  I'm not sure
  // if these registrations are right or not, but they preserve old behavior
  // (and some of them are exercised by the test suite).
  m.impl("backward", CppFunction::makeFallthrough());
  m.impl("set_data", CppFunction::makeFallthrough());
  m.impl("data", CppFunction::makeFallthrough());
  m.impl("is_leaf", CppFunction::makeFallthrough());
  m.impl("_version", CppFunction::makeFallthrough());
  m.impl("requires_grad_", CppFunction::makeFallthrough());
  m.impl("requires_grad", CppFunction::makeFallthrough());
  m.impl("retain_grad", CppFunction::makeFallthrough());
}
