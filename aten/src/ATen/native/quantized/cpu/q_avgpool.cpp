#include <ATen/ATen.h>
#include <ATen/NativeFunctions.h>
#include <ATen/Parallel.h>
#include <ATen/native/Pool.h>
#include <ATen/core/op_registration/op_registration.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace at {
namespace native {
namespace {

template <typename scalar_t, typename underlying_t>
static void avg_pool2d_out_frame(
          scalar_t *input_data,
          scalar_t *output_data,
          int64_t nbatch,
          int64_t nInputPlane,
          int64_t inputWidth,
          int64_t inputHeight,
          int64_t outputWidth,
          int64_t outputHeight,
          int kW,
          int kH,
          int dW,
          int dH,
          int padW,
          int padH,
          bool count_include_pad,
          c10::optional<int64_t> divisor_override)
{
  at::parallel_for(0, nInputPlane, 0, [&](int64_t start, int64_t end) {
    for (auto k = start; k < end; k++)
    {
      int64_t p;
      for(p = 0; p < nbatch; p++)
      {
        int64_t xx, yy;
        /* For all output pixels... */
        scalar_t *ptr_output = output_data + p*nInputPlane*outputWidth*outputHeight + k*outputWidth*outputHeight;
        const scalar_t *ptr_input = input_data + p*nInputPlane*inputWidth*inputHeight + k*inputWidth*inputHeight;
        int64_t i;
        for(i = 0; i < outputWidth*outputHeight; i++)
          (ptr_output+i)->val_ = 0;

        for(yy = 0; yy < outputHeight; yy++)
        {
          for(xx = 0; xx < outputWidth; xx++)
          {
            /* Compute the mean of the input image... */
            int64_t hstart = yy * dH - padH;
            int64_t wstart = xx * dW - padW;
            int64_t hend = std::min(hstart + kH, inputHeight + padH);
            int64_t wend = std::min(wstart + kW, inputWidth + padW);
            int pool_size = (hend - hstart) * (wend - wstart);
            hstart = std::max(hstart, (int64_t) 0);
            wstart = std::max(wstart, (int64_t) 0);
            hend = std::min(hend, inputHeight);
            wend = std::min(wend, inputWidth);

            int64_t sum = 0;

            int divide_factor;
            if (divisor_override.has_value()) {
              divide_factor = divisor_override.value();
            } else {
              if(count_include_pad) {
                divide_factor = pool_size;
              } else {
                divide_factor = (hend - hstart) * (wend - wstart);
              }
            }

            int64_t kx, ky;

            for(ky = hstart; ky < hend; ky++)
            {
              for(kx = wstart; kx < wend; kx++)
                sum += (ptr_input + ky*inputWidth + kx)->val_;
            }
            /* Update output */
            ptr_output->val_ = static_cast<underlying_t>(std::nearbyint(sum/divide_factor));
            ptr_output++;
          }
        }
      }
    }
  });
}

void avg_pool2d_out_template(
          Tensor &output,
          const Tensor &input_,
          int kW,
          int kH,
          int dW,
          int dH,
          int padW,
          int padH,
          bool count_include_pad,
          c10::optional<int64_t> divisor_override)
{
  const int64_t nbatch = input_.ndimension() == 4 ? input_.size(-4) : 1;
  const int64_t nInputPlane = input_.size(-3);
  const int64_t inputHeight = input_.size(-2);
  const int64_t inputWidth = input_.size(-1);

  const int64_t outputHeight = output.size(-2);
  const int64_t outputWidth = output.size(-1);



  TORCH_CHECK(output.is_contiguous(), "avg_pool2d: output must be contiguous");

  Tensor input = input_.contiguous();

  AT_DISPATCH_QINT_TYPES(input.scalar_type(), "avg_pool2d_out_frame", [&] {
      scalar_t *input_data = input.data_ptr<scalar_t>();
      scalar_t *output_data = output.data_ptr<scalar_t>();

      avg_pool2d_out_frame<scalar_t, underlying_t>(
        input_data,
        output_data,
        nbatch,
        nInputPlane,
        inputWidth, inputHeight,
        outputWidth, outputHeight,
        kW, kH,
        dW, dH,
        padW, padH,
        count_include_pad,
        divisor_override);
    }
  );
}

inline std::pair<int, int> get_kernel(IntArrayRef kernel_size) {
  TORCH_CHECK(kernel_size.size() == 1 || kernel_size.size() == 2,
    "avg_pool2d: kernel_size must either be a single int, or a tuple of two ints");
  const int kH = safe_downcast<int, int64_t>(kernel_size[0]);
  const int kW = kernel_size.size() == 1 ? kH : safe_downcast<int, int64_t>(kernel_size[1]);
  return std::make_pair(kW, kH);
}

inline std::pair<int, int> get_stride(IntArrayRef stride, int kW, int kH) {
  TORCH_CHECK(stride.empty() || stride.size() == 1 || stride.size() == 2,
    "avg_pool2d: stride must either be omitted, a single int, or a tuple of two ints");
  const int dH = stride.empty() ? kH : safe_downcast<int, int64_t>(stride[0]);
  const int dW = stride.empty() ? kW :
                 stride.size() == 1 ? dH : safe_downcast<int, int64_t>(stride[1]);
  return std::make_pair(dW, dH);
}

inline std::pair<int, int> get_padding(IntArrayRef padding) {
  TORCH_CHECK(padding.size() == 1 || padding.size() == 2,
    "avg_pool2d: padding must either be a single int, or a tuple of two ints");
  const int padH = safe_downcast<int, int64_t>(padding[0]);
  const int padW = padding.size() == 1 ? padH : safe_downcast<int, int64_t>(padding[1]);
  return std::make_pair(padW, padH);
}

std::vector<int64_t> get_output_shape(const Tensor& input_, int kW, int kH, int dW, int dH, int padW, int padH, bool ceil_mode) {

  const int64_t nbatch = input_.ndimension() == 4 ? input_.size(-4) : 1;
  const int64_t nInputPlane = input_.size(-3);
  const int64_t inputHeight = input_.size(-2);
  const int64_t inputWidth = input_.size(-1);
  const int64_t outputHeight = pooling_output_shape<int64_t>(inputHeight, kH, padH, dH, 1, ceil_mode);
  const int64_t outputWidth = pooling_output_shape<int64_t>(inputWidth, kW, padW, dW, 1, ceil_mode);
  if (input_.ndimension() == 3) {
    return {nInputPlane, outputHeight, outputWidth};
  }
  return {nbatch, nInputPlane, outputHeight, outputWidth};
}

} // namespace

Tensor& quantized_avg_pool2d_out(
  Tensor& output,
  const Tensor& input,
  IntArrayRef kernel_size,
  IntArrayRef stride,
  IntArrayRef padding,
  bool ceil_mode,
  bool count_include_pad,
  c10::optional<int64_t> divisor_override) {
  int kW, kH, dW, dH, padW, padH;
  std::tie(kW, kH) = get_kernel(kernel_size);
  std::tie(dW, dH) = get_stride(stride, kW, kH);
  std::tie(padW, padH) = get_padding(padding);

  TORCH_CHECK(!divisor_override.has_value() || divisor_override.value() != 0,
    "divisor must be not zero");

  auto output_shape = get_output_shape(input, kW, kH, dW, dH, padW, padH, ceil_mode);
  TORCH_CHECK(
      output.is_quantized() && output.sizes() == output_shape,
      "Output Tensor must be quantized and have a shape of ",
      "{ ",
      output_shape,
      " }.");

  avg_pool2d_out_template(output, input, kW, kH, dW, dH, padW, padH, count_include_pad, divisor_override);

  return output;
}

Tensor quantized_avg_pool2d(
  const Tensor& input,
  IntArrayRef kernel_size,
  IntArrayRef stride,
  IntArrayRef padding,
  bool ceil_mode,
  bool count_include_pad,
  c10::optional<int64_t> divisor_override) {
  int kW, kH, dW, dH, padW, padH;
  std::tie(kW, kH) = get_kernel(kernel_size);
  std::tie(dW, dH) = get_stride(stride, kW, kH);
  std::tie(padW, padH) = get_padding(padding);

  TORCH_CHECK(!divisor_override.has_value() || divisor_override.value() != 0,
    "divisor must be not zero");

  auto output_shape = get_output_shape(input, kW, kH, dW, dH, padW, padH, ceil_mode);
  // so far, we have no support of resize on qtensor, let's allocate aot
  auto output = at::_empty_affine_quantized(
            output_shape, input.options(), input.q_scale(), input.q_zero_point());

  avg_pool2d_out_template(output, input, kW, kH, dW, dH, padW, padH, count_include_pad, divisor_override);

  return output;
}

} // namespace native
} // namespace at
