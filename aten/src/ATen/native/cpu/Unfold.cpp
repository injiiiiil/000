#include <ATen/Parallel.h>
#include <ATen/cpu/vec256/vec256.h>
#include <ATen/native/Unfold.h>
#include <ATen/native/cpu/Loops.h>
#include <cmath>

namespace at {
namespace native {

namespace {

template <typename scalar_t>
static inline void cadd(
    scalar_t* z,
    const scalar_t* x,
    const scalar_t* y,
    const scalar_t c,
    int64_t n) {
  using Vec = vec256::Vec256<scalar_t>;
  char* ptrs[] = {reinterpret_cast<char*>(z),
                  reinterpret_cast<char*>(const_cast<scalar_t*>(x)),
                  reinterpret_cast<char*>(const_cast<scalar_t*>(y)),
                  reinterpret_cast<char*>(const_cast<scalar_t*>(&c))};
  vectorized_loop(
      ptrs,
      n,
      3,
      [](scalar_t x, scalar_t y, scalar_t c) -> scalar_t { return x + c * y; },
      [](Vec x, Vec y, Vec c) -> Vec { return x + c * y; });
}

template <typename scalar_t>
static void unfolded_acc(
    scalar_t* finput_data,
    scalar_t* input_data,
    int64_t kH,
    int64_t kW,
    int64_t dH,
    int64_t dW,
    int64_t padH,
    int64_t padW,
    int64_t n_input_plane,
    int64_t input_height,
    int64_t input_width,
    int64_t output_height,
    int64_t output_width) {
  at::parallel_for(0, n_input_plane, 0, [&](int64_t start, int64_t end) {
    for (auto nip = start; nip < end; nip++) {
      int64_t kw, kh, y, x;
      int64_t ix, iy;
      for (kh = 0; kh < kH; kh++) {
        for (kw = 0; kw < kW; kw++) {
          scalar_t* src = finput_data +
              nip * ((size_t)kH * kW * output_height * output_width) +
              kh * ((size_t)kW * output_height * output_width) +
              kw * ((size_t)output_height * output_width);
          scalar_t* dst =
              input_data + nip * ((size_t)input_height * input_width);
          if (padW > 0 || padH > 0) {
            int64_t lpad, rpad;
            for (y = 0; y < output_height; y++) {
              iy = (int64_t)y * dH - padH + kh;
              if (iy < 0 || iy >= input_height) {
              } else {
                if (dW == 1) {
                  ix = 0 - padW + kw;
                  lpad = std::max<int64_t>(0, padW - kw);
                  rpad = std::max<int64_t>(0, padW - (kW - kw - 1));
                  scalar_t* dst_slice =
                      dst + (size_t)iy * input_width + ix + lpad;
                  cadd(
                      dst_slice,
                      dst_slice,
                      src + (size_t)y * output_width + lpad,
                      static_cast<scalar_t>(1),
                      output_width - lpad - rpad);
                  // note: THVector_add could handle 1 value better
                } else {
                  for (x = 0; x < output_width; x++) {
                    ix = (int64_t)x * dW - padW + kw;
                    if (ix < 0 || ix >= input_width) {
                    } else {
                      scalar_t* dst_slice = dst + (size_t)iy * input_width + ix;
                      cadd(
                          dst_slice,
                          dst_slice,
                          src + (size_t)y * output_width + x,
                          static_cast<scalar_t>(1),
                          1);
                    }
                  }
                }
              }
            }
          } else {
            for (y = 0; y < output_height; y++) {
              iy = (int64_t)y * dH + kh;
              ix = 0 + kw;
              if (dW == 1) {
                scalar_t* dst_slice = dst + (size_t)iy * input_width + ix;
                cadd(
                    dst_slice,
                    dst_slice,
                    src + (size_t)y * output_width,
                    static_cast<scalar_t>(1),
                    output_width); // note: THVector_add could handle 1 value
                                   // better
              } else {
                for (x = 0; x < output_width; x++) {
                  scalar_t* dst_slice =
                      dst + (size_t)iy * input_width + ix + x * dW;
                  cadd(
                      dst_slice,
                      dst_slice,
                      src + (size_t)y * output_width + x,
                      static_cast<scalar_t>(1),
                      1);
                }
              }
            }
          }
        }
      }
    }
  });
}

/* note: due to write issues, this one cannot be parallelized as well as
 * unfolded_copy */
void unfolded_acc_kernel(
    Tensor& finput,
    Tensor& input,
    int64_t kH,
    int64_t kW,
    int64_t dH,
    int64_t dW,
    int64_t padH,
    int64_t padW,
    int64_t n_input_plane,
    int64_t input_height,
    int64_t input_width,
    int64_t output_height,
    int64_t output_width) {
  // This function assumes that
  // output_height*dH does not overflow a int64_t
  // output_width*dW does not overflow a int64_t

  AT_DISPATCH_FLOATING_TYPES_AND(
      at::ScalarType::BFloat16, input.scalar_type(), "unfolded_acc", [&] {
        scalar_t* finput_data = finput.data_ptr<scalar_t>();
        scalar_t* input_data = input.data_ptr<scalar_t>();

        unfolded_acc(
            finput_data,
            input_data,
            kH,
            kW,
            dH,
            dW,
            padH,
            padW,
            n_input_plane,
            input_height,
            input_width,
            output_height,
            output_width);
      });
}

template <typename scalar_t>
static void unfolded_copy(
    scalar_t* input_data,
    scalar_t* finput_data,
    int kH,
    int kW,
    int dH,
    int dW,
    int padH,
    int padW,
    int n_input_plane,
    int input_height,
    int input_width,
    int output_height,
    int output_width) {
  at::parallel_for(
      0, (int64_t)n_input_plane * kH * kW, 0, [&](int64_t start, int64_t end) {
        for (auto k = start; k < end; k++) {
          int64_t nip = k / (kH * kW);
          int64_t rest = k % (kH * kW);
          int64_t kh = rest / kW;
          int64_t kw = rest % kW;
          int x, y;
          int64_t ix, iy;
          scalar_t* dst = finput_data +
              nip * ((size_t)kH * kW * output_height * output_width) +
              kh * ((size_t)kW * output_height * output_width) +
              kw * ((size_t)output_height * output_width);
          scalar_t* src =
              input_data + nip * ((size_t)input_height * input_width);
          if (padW > 0 || padH > 0) {
            int64_t lpad, rpad;
            for (y = 0; y < output_height; y++) {
              iy = (int64_t)y * dH - padH + kh;
              if (iy < 0 || iy >= input_height) {
                memset(
                    dst + (size_t)y * output_width,
                    0,
                    sizeof(scalar_t) * output_width);
              } else {
                if (dW == 1) {
                  ix = 0 - padW + kw;
                  lpad = fmaxf(0, padW - kw);
                  rpad = fmaxf(0, padW - (kW - kw - 1));
                  if (output_width - rpad - lpad <= 0) {
                    memset(
                        dst + (size_t)y * output_width,
                        0,
                        sizeof(scalar_t) * output_width);
                  } else {
                    if (lpad > 0)
                      memset(
                          dst + (size_t)y * output_width,
                          0,
                          sizeof(scalar_t) * lpad);
                    memcpy(
                        dst + (size_t)y * output_width + lpad,
                        src + (size_t)iy * input_width + ix + lpad,
                        sizeof(scalar_t) * (output_width - rpad - lpad));
                    if (rpad > 0)
                      memset(
                          dst + (size_t)y * output_width + output_width - rpad,
                          0,
                          sizeof(scalar_t) * rpad);
                  }
                } else {
                  for (x = 0; x < output_width; x++) {
                    ix = (int64_t)x * dW - padW + kw;
                    if (ix < 0 || ix >= input_width)
                      memset(
                          dst + (size_t)y * output_width + x,
                          0,
                          sizeof(scalar_t) * 1);
                    else
                      memcpy(
                          dst + (size_t)y * output_width + x,
                          src + (size_t)iy * input_width + ix,
                          sizeof(scalar_t) * (1));
                  }
                }
              }
            }
          } else {
            for (y = 0; y < output_height; y++) {
              iy = (int64_t)y * dH + kh;
              ix = 0 + kw;
              if (dW == 1)
                memcpy(
                    dst + (size_t)y * output_width,
                    src + (size_t)iy * input_width + ix,
                    sizeof(scalar_t) * output_width);
              else {
                for (x = 0; x < output_width; x++)
                  memcpy(
                      dst + (size_t)y * output_width + x,
                      src + (size_t)iy * input_width + ix + (int64_t)x * dW,
                      sizeof(scalar_t) * (1));
              }
            }
          }
        }
      });
}

void unfolded_copy_kernel(
    Tensor& finput,
    Tensor& input,
    int64_t kH,
    int64_t kW,
    int64_t dH,
    int64_t dW,
    int64_t padH,
    int64_t padW,
    int64_t n_input_plane,
    int64_t input_height,
    int64_t input_width,
    int64_t output_height,
    int64_t output_width) {
  // This function assumes that
  // kH*kW does not overflow an int
  // n_input_plane*kH*kW does not overflow a int64_t
  // output_height*dH does not overflow a int64_t
  // output_width*dW does not overflow a int64_t

  AT_DISPATCH_ALL_TYPES_AND(
      at::ScalarType::BFloat16, input.scalar_type(), "unfolded_copy", [&] {
        scalar_t* input_data = input.data_ptr<scalar_t>();
        scalar_t* finput_data = finput.data_ptr<scalar_t>();

        unfolded_copy(
            input_data,
            finput_data,
            kH,
            kW,
            dH,
            dW,
            padH,
            padW,
            n_input_plane,
            input_height,
            input_width,
            output_height,
            output_width);
      });
}

} // namespace

REGISTER_DISPATCH(unfolded_copy_stub, &unfolded_copy_kernel);
REGISTER_DISPATCH(unfolded_acc_stub, &unfolded_acc_kernel);

} // namespace native
} // namespace at
