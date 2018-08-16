#if defined USE_CUDA && !(defined _WIN32) && !(defined USE_ROCM)
#pragma once

#include "torch/csrc/jit/code_template.h"

namespace torch { namespace jit { namespace cudafuser {

/*with type_as not checking type of its input, a fusion group can have non-fp32 tensor as input.
Correct code for this case is generated, however, nvrtc does not know how to handle int*_t integer types,
so typedefs help it handle those cases*/

auto type_declarations_template = CodeTemplate(R"(
#if defined(__CUDACC_RTC__)
typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef short int  int16_t;
typedef long long int int64_t;
${HalfHeader}
${RandHeader}
#endif
typedef ${IndexType} IndexType;
template<typename T, size_t N>
struct TensorInfo {
  T * data;
  IndexType sizes[N];
  IndexType strides[N];
};
)");

// We rewrite the code for philox RNG from curand as nvrtc couldn't resolve the
// curand header correctly.
constexpr auto rand_support_literal = R"(

  class Philox {
  public:
    __device__ inline Philox(unsigned long long seed,
                             unsigned long long subsequence,
                             unsigned long long offset) {
      key.x = (unsigned int)seed;
      key.y = (unsigned int)(seed >> 32);
      counter = make_uint4(0, 0, 0, 0);
      counter.z = (unsigned int)(subsequence);
      counter.w = (unsigned int)(subsequence >> 32);
      STATE = 0;
      incr_n(offset / 4);
    }

    __device__ inline unsigned long operator()() {
      if(STATE == 0) {
        uint4 counter_ = counter;
        uint2 key_ = key;
        for(int i = 0; i < 9; i++) {
          counter_ = single_round(counter_, key_);
          key_.x += (kPhilox10A); key_.y += (kPhilox10B);
        }
        output = single_round(counter_, key_);
        incr();
      }
      unsigned long ret;
      switch(STATE) {
        case 0: ret = output.x; break;
        case 1: ret = output.y; break;
        case 2: ret = output.z; break;
        case 3: ret = output.w; break;
      }
      STATE = (STATE + 1) % 4;
      return ret;
    }

  private:
    uint4 counter;
    uint4 output;
    uint2 key;
    unsigned int STATE;
    __device__ inline void incr_n(unsigned long long n) {
      unsigned int nlo = (unsigned int)(n);
      unsigned int nhi = (unsigned int)(n >> 32);
      counter.x += nlo;
      if (counter.x < nlo)
        nhi++;
      counter.y += nhi;
      if (nhi <= counter.y)
        return;
      if (++counter.z)
        return;
      ++counter.w;
    }
    __device__ inline void incr() {
      if (++counter.x)
        return;
      if (++counter.y)
        return;
      if (++counter.z)
        return;
      ++counter.w;
    }
    __device__ unsigned int mulhilo32(unsigned int a, unsigned int b,
                                      unsigned int *result_high) {
      *result_high = __umulhi(a, b);
      return a*b;
    }

    __device__ inline uint4 single_round(uint4 ctr, uint2 key) {
      unsigned int hi0;
      unsigned int hi1;
      unsigned int lo0 = mulhilo32(kPhiloxSA, ctr.x, &hi0);
      unsigned int lo1 = mulhilo32(kPhiloxSB, ctr.z, &hi1);

      uint4 ret = {hi1 ^ ctr.y ^ key.x, lo1, hi0 ^ ctr.w ^ key.y, lo0};
      return ret;
    }

    static const unsigned long kPhilox10A = 0x9E3779B9;
    static const unsigned long kPhilox10B = 0xBB67AE85;
    static const unsigned long kPhiloxSA = 0xD2511F53;
    static const unsigned long kPhiloxSB = 0xCD9E8D57;
  };

  // Inverse of 2^32.
  #define M_RAN_INVM32 2.3283064e-10f
  __device__  __inline__ float uniform(unsigned int x) {
    return x * M_RAN_INVM32;
  }
)";

constexpr auto rand_param = ",unsigned long long seed, unsigned long long offset";
constexpr auto rand_init = R"(
  int idx = blockIdx.x*blockDim.x + threadIdx.x;
  Philox rnd(seed, idx, offset);
)";

auto cuda_compilation_unit_template = CodeTemplate(R"(
${type_declarations}

extern "C" __global__
void ${kernelName}(IndexType totalElements, ${formals} ${RandParam}) {
  ${RandInit}
  for (IndexType linearIndex = blockIdx.x * blockDim.x + threadIdx.x;
        linearIndex < totalElements;
        linearIndex += gridDim.x * blockDim.x) {
      // Convert `linearIndex` into an offset of tensor:
      ${tensorOffsets}
      // calculate the results
      ${kernelBody}
    }
}
)");

// This snippet enables half support in the jit. Following the pattern for
// reductions, fp16 input data is immediately upconverted to float
// with __half2float(). All mathematical operations are done on float
// values, and if needed the intermediate float representation is
// converted to half with __float2half() when writing to a half tensor.
constexpr auto half_support_literal  = R"(
#define __HALF_TO_US(var) *(reinterpret_cast<unsigned short *>(&(var)))
#define __HALF_TO_CUS(var) *(reinterpret_cast<const unsigned short *>(&(var)))
#if defined(__cplusplus)
  struct __align__(2) __half {
    __host__ __device__ __half() { }

  protected:
    unsigned short __x;
  };

  /* All intrinsic functions are only available to nvcc compilers */
  #if defined(__CUDACC__)
    /* Definitions of intrinsics */
    __device__ __half __float2half(const float f) {
      __half val;
      asm("{  cvt.rn.f16.f32 %0, %1;}\n" : "=h"(__HALF_TO_US(val)) : "f"(f));
      return val;
    }

    __device__ float __half2float(const __half h) {
      float val;
      asm("{  cvt.f32.f16 %0, %1;}\n" : "=f"(val) : "h"(__HALF_TO_CUS(h)));
      return val;
    }
  #endif /* defined(__CUDACC__) */
#endif /* defined(__cplusplus) */
#undef __HALF_TO_US
#undef __HALF_TO_CUS

typedef __half half;
)";

// curDimIndex = linearId % sizes[i]; // % sizes[i] is not needed for d == 0, because we already guard for numel outside the index calculation
// offset += curDimIndex*strides[i]; // *strides[i] is optional if list_is_cont becaause strides.back() == 1
// linearId /= sizes[i];
auto dim_calc = CodeTemplate(R"(
//printf("tensor ${tensor} sizes[${d}] = %d, strides[${d}] = %d\n", ${tensor}.sizes[${d}],${tensor}.strides[${d}]);
size_t ${tensor}_dimIndex${d} = ${tensor}_linearIndex ${mod_sizes};
${tensor}_offset += ${tensor}_dimIndex${d} ${times_stride};
)");

} // namespace cudafuser
} // namespace jit
} // namespace torch

#endif // defined USE_CUDA && !(defined _WIN32) && !(defined USE_ROCM)