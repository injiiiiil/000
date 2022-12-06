#ifdef USE_VULKAN_API

#include <ATen/ATen.h>
#include <ATen/core/dispatch/Dispatcher.h>
#include <ATen/native/vulkan/api/api.h>
#include <gtest/gtest.h>

#include <ATen/native/vulkan/ops/Common.h>
#include <ATen/native/vulkan/ops/Copy.h>
#include <ATen/native/vulkan/ops/Factory.h>
#include <ATen/native/vulkan/ops/QuantizedFunctions.h>
#include <ATen/native/quantized/cpu/QuantUtils.h>
#include <string.h>

#include <c10/util/irange.h>

/*
 * TODO: rename this file to something like vulkan_experimental_test and move
 * this under caffe2/fb/vulkan. This file should be used to test experimental
 * features of the Vulkan backend. vulkan_api_test cannot serve this purpose
 * because it cannot link against symbols in the ATen/native/vulkan folder.
 */

namespace {

bool checkRtol(
    const at::Tensor& diff,
    const std::vector<at::Tensor>& inputs,
    const float tolerated_error = 0) {
  float maxValue = 0.0f;

  for (const auto& tensor : inputs) {
    maxValue = fmax(tensor.abs().max().item<float>(), maxValue);
  }

#ifdef USE_VULKAN_FP16_INFERENCE
  constexpr float tolerance = 1e-2;
#else
  constexpr float tolerance = 1e-5;
#endif

  return diff.abs().max().item<float>() <= (tolerance * maxValue + tolerated_error);
}

bool almostEqual(const at::Tensor& a, const at::Tensor& b, const float tolerated_error = 0) {
  return checkRtol(a - b, {a, b}, tolerated_error);
}

/* Unused function
bool exactlyEqual(const at::Tensor& a, const at::Tensor& b) {
  return (a - b).abs().max().item<float>() == 0.0f;
}
*/

void showRtol(const at::Tensor& a, const at::Tensor& b) {
  const auto diff = (a - b).abs();

  float maxValue = a.abs().max().item<float>();
  maxValue = fmax(b.abs().max().item<float>(), maxValue);

#ifdef USE_VULKAN_FP16_INFERENCE
  constexpr float tolerance = 1e-2;
#else
  constexpr float tolerance = 1e-5;
#endif

  const float maxDiff = maxValue * tolerance;
  std::cout << "Max Diff allowed: " << maxDiff << std::endl;
  if (diff.sizes().size() == 2) {
    for (const auto y : c10::irange(diff.sizes()[0])) {
      std::cout << y << ":";
      for (const auto x : c10::irange(diff.sizes()[1])) {
        float diff_xy = diff[y][x].item<float>();
        if (diff_xy > maxDiff) {
          std::cout << std::setw(5) << x;
        } else {
          std::cout << std::setw(5) << " ";
        }
      }
      std::cout << std::endl;
    }
  }
}

template <class... Inputs>
inline std::vector<c10::IValue> makeStack(Inputs&&... inputs) {
  return {std::forward<Inputs>(inputs)...};
}

template <class... Args>
inline std::vector<c10::IValue> callOpByHandle(
    const c10::OperatorHandle& op,
    Args... args) {
  auto stack = makeStack(std::forward<Args>(args)...);
  c10::Dispatcher::singleton().callBoxed(op, &stack);
  return stack;
}

template <class... Args>
inline std::vector<c10::IValue> callOpByName(
    const char* func_name,
    const char* overload_name,
    Args... args) {
  const c10::optional<c10::OperatorHandle> op_handle =
      c10::Dispatcher::singleton().findSchema({func_name, overload_name});
  assert(op_handle.has_value());
  return callOpByHandle(op_handle.value(), std::forward<Args>(args)...);
}

} // namespace

namespace {

double rand01() {
  return (double)rand() / (double)RAND_MAX;
}

int64_t rand_pos_int(const int max_val) {
  return 1 + int64_t(rand01() * (max_val - 1));
}

at::Tensor produce_random_tensor(
    const at::IntArrayRef tensor_shape,
    const float s_min = 1.0,
    const float s_max = 100.0,
    const float shift = 0.45) {
  // tensor is randomly generated with values in the range
  // [-shift * s, (1-shift) * s), where s is randomly generated in the range
  // [s_min, s_max]
  // with these default values, s is randomly generated in the range [1, 100]
  // this means that the range of the tensor values could be as narrow as
  // [-0.45, 0.55) or as wide as [-45.0, 55.0)
  TORCH_CHECK(s_min > 0, "scalar lower bound must be positive");
  TORCH_CHECK(s_min <= s_max, "scalar lower bound must be <= upper bound");
  const auto scalar = s_min + (s_max - s_min) * (float)rand()/(float)RAND_MAX;
  return scalar *
    (at::rand(tensor_shape, at::device(at::kCPU).dtype(at::kFloat)) - shift);
}

double produce_random_scale(
    const double scale_min = 0.001,
    const double scale_max = 2.0) {
  TORCH_CHECK(scale_min <= scale_max, "scale min must be <= scale max");
  // scale is randomly generated in the range [scale_min, scale_max)
  return rand01() * (scale_max - scale_min) + scale_min;
}

int64_t produce_random_zero_point(const c10::ScalarType dtype) {
  int64_t zero_point;
  switch (dtype) {
    case c10::ScalarType::QUInt8:
      zero_point = int64_t(rand01() * 255);
      break;
    case c10::ScalarType::QInt8:
      zero_point = int64_t(rand01() * 255) - 127;
      break;
    case c10::ScalarType::QInt32:
      zero_point = int64_t(rand01() * 100000) - 200000;
      break;
    default:
      TORCH_CHECK(
        false, "Vulkan quantization currently not supported for dtype ", dtype
      );
  }
  return zero_point;
}

std::tuple<double, int64_t> compute_quant_params(
    const at::Tensor tensor,
    const c10::ScalarType dtype = c10::ScalarType::QUInt8) {
  int zero_point_min;
  int zero_point_max;
  if (dtype == c10::ScalarType::QUInt8) {
    zero_point_min = 0;
    zero_point_max = 255;
  } else if (dtype == c10::ScalarType::QInt8) {
    zero_point_min = -128;
    zero_point_max = 127;
  } else {
    TORCH_CHECK(false, "Computation of quant params only available for dtypes",
                       "QUInt8 and QInt8");
  }
  const auto tensor_max = tensor.max().item<double>();
  const auto tensor_min = tensor.min().item<double>();
  auto q_params = quant_utils::ChooseQuantizationParams(
      /*min=*/tensor_min,
      /*max=*/tensor_max,
      /*qmin=*/zero_point_min,
      /*qmax=*/zero_point_max,
      /*preserve_sparsity=*/false,
      /*force_scale_power_of_two=*/false,
      /*reduce_range=*/false);
  return std::tuple<double, int64_t>(q_params.scale, q_params.zero_point);
}

} // namespace

namespace {

class VulkanAPITest : public ::testing::Test {
 public:
  void SetUp() {
    if (!at::is_vulkan_available()) {
      GTEST_SKIP() << "Vulkan is not available";
    }
#if defined(USE_VULKAN_GPU_DIAGNOSTICS) && defined(__ANDROID__)
    at::native::vulkan::api::context()->reset_querypool();
#endif
  }

  void TearDown() {
#if defined(USE_VULKAN_GPU_DIAGNOSTICS) && defined(__ANDROID__)
    try {
      at::native::vulkan::api::context()->querypool().extract_results();
      at::native::vulkan::api::context()->querypool().print_results();
    } catch (const std::exception& e) {
      std::cout << "Could not get querypool results!"
                << " Reason: " << e.what() << std::endl;
    }
#endif
  }
};

at::Tensor cpu_to_vulkan(at::Tensor in_cpu) {
  auto options = in_cpu.options();
  if (options.dtype().toScalarType() == c10::ScalarType::QUInt8) {
    auto ret = at::native::vulkan::ops::_empty_affine_quantized(
        in_cpu.sizes(),
        c10::ScalarType::QUInt8,
        options.layout(),
        options.device(),
        options.pinned_memory(),
        in_cpu.q_scale(),
        in_cpu.q_zero_point(),
        c10::MemoryFormat::Contiguous);
    at::native::vulkan::ops::copy_(ret, in_cpu);
    return ret;
  } else {
    auto ret = at::empty(in_cpu.sizes(), options);
    at::native::vulkan::ops::copy_(ret, in_cpu);
    return ret;
  }
}

at::Tensor vulkan_to_cpu(at::Tensor vulkan, at::Tensor in_cpu) {
  auto q_options = in_cpu.options();
  if (q_options.dtype().toScalarType() == c10::ScalarType::QUInt8) {
    auto output = at::native::empty_affine_quantized(
        in_cpu.sizes(),
        q_options.dtype().toScalarType(),
        q_options.layout(),
        q_options.device(),
        q_options.pinned_memory(),
        in_cpu.q_scale(),
        in_cpu.q_zero_point());
    at::native::vulkan::ops::copy_(output, vulkan);
    return output;
  } else {
    auto output = at::empty(in_cpu.sizes(), q_options);
    at::native::vulkan::ops::copy_(output, vulkan);
    return output;
  }
}

TEST_F(VulkanAPITest, uniform_buffer_copy) {
  using namespace at::native::vulkan;

  struct TestStruct{
    int a;
    int b;
    int c;
  };

  TestStruct test_struct{4, 9, 10};

  api::UniformParamsBuffer params(api::context(), test_struct);
  api::UniformParamsBuffer params_copy = params;

  api::MemoryMap copy_mapping(
      params_copy.buffer(), api::MemoryAccessType::READ);

  TestStruct* test_copy_p = copy_mapping.template data<TestStruct>();

  ASSERT_TRUE(test_copy_p->a == test_struct.a);
  ASSERT_TRUE(test_copy_p->b == test_struct.b);
  ASSERT_TRUE(test_copy_p->c == test_struct.c);
}

TEST_F(VulkanAPITest, copy_to_buffer) {
  using namespace at::native::vulkan;

  at::Tensor test_tensors[] = {
    // 4D
    at::rand({7, 17, 134, 213}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
    // 3D
    at::rand({67, 134, 213}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
    // 2D
    at::rand({229, 213}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
    // 1D
    at::rand({1902}, at::TensorOptions(at::kCPU).dtype(at::kFloat)),
  };

  for (auto in_cpu : test_tensors) {
    ops::vTensor in_vk_copied = ops::to_vulkan(in_cpu, api::StorageType::BUFFER);
    at::Tensor out_copied = ops::from_vulkan(in_vk_copied);

    const auto check_copy = almostEqual(out_copied, in_cpu);

    if(!check_copy) {
      std::cout << "Copy failed on size " << in_cpu.sizes()
                << "with dtype" << in_cpu.dtype() << std::endl;
    }

    ASSERT_TRUE(check_copy);
  }
}

TEST_F(VulkanAPITest, copy_to_buffer_channels_last) {
  using namespace at::native::vulkan;

  at::TensorOptions options(at::kCPU);
  options = options.dtype(at::kFloat);

  at::Tensor test_tensors[] = {
    // 4D
    at::rand({7, 17, 134, 213}, options).to(at::MemoryFormat::ChannelsLast),
  };

  for (auto in_cpu : test_tensors) {
    ops::vTensor in_vk_copied = ops::to_vulkan(in_cpu, api::StorageType::BUFFER);
    at::Tensor out_copied = ops::from_vulkan(in_vk_copied);

    const auto check_copy = almostEqual(out_copied, in_cpu);

    if(!check_copy) {
      std::cout << "Copy failed on size " << in_cpu.sizes()
                << "with dtype" << in_cpu.dtype() << std::endl;
    }

    ASSERT_TRUE(check_copy);
  }
}

TEST_F(VulkanAPITest, support_vulkan) {
  const double scale = 0.1;
  const int64_t zero_point = 10;

  auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 12 -
      6;
  auto in_cpu_quantized = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);

  auto in_vulkan_quantized = cpu_to_vulkan(in_cpu_quantized);
  at::native::vulkan::api::PipelineBarrier pipeline_barrier{};
  at::native::vulkan::ops::vTensor& v_self =
      at::native::vulkan::ops::convert(in_vulkan_quantized);
  if (in_cpu.dtype() == c10::kQUInt8) {
    v_self.image(
        pipeline_barrier,
        at::native::vulkan::api::PipelineStage::COMPUTE,
        at::native::vulkan::api::MemoryAccessType::READ);
    v_self.image(
        pipeline_barrier,
        at::native::vulkan::api::PipelineStage::COMPUTE,
        at::native::vulkan::api::MemoryAccessType::WRITE);
  }
  auto output = vulkan_to_cpu(in_vulkan_quantized, in_cpu_quantized);
  const auto check = almostEqual(
      at::native::int_repr_quantized_cpu(in_cpu_quantized),
      at::native::int_repr_quantized_cpu(output));

  if (!check) {
    showRtol(
        at::native::int_repr_quantized_cpu(in_cpu_quantized),
        at::native::int_repr_quantized_cpu(output));
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantize_per_tensor) {
  const auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);

  auto output_for_quantized_vulkan = vulkan_to_cpu(out_vulkan, out_cpu);

  int rtol = 1;
  const auto check = at::allclose(
      at::native::int_repr_quantized_cpu(out_cpu),
      at::native::int_repr_quantized_cpu(output_for_quantized_vulkan),
      rtol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantize_dequantize) {
  const auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;
  // quantize tensors
  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  // dequantize tensors
  const auto out_cpu_deq = at::dequantize(out_cpu);
  const auto out_vulkan_deq = at::native::vulkan::ops::dequantize(out_vulkan);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu);

  float rtol = 1;
  float atol = 0.5;
  const auto check =
      at::allclose(in_cpu, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);

  const auto check_two =
      at::allclose(out_cpu_deq, output_for_dequantized_vulkan, rtol, atol);

  if (!check_two) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check_two);
}

void test_quantize_per_tensor_and_dequantize(
    const at::IntArrayRef input_shape,
    const double input_scale,
    const int input_zero_point,
    const c10::ScalarType dtype = c10::ScalarType::QUInt8) {
  at::Tensor input = produce_random_tensor(input_shape);

  // quantize tensors
  at::Tensor out_q_cpu = at::quantize_per_tensor(
    input, input_scale, input_zero_point, dtype);
  at::Tensor out_q_vk = at::quantize_per_tensor(
    input.vulkan(), input_scale, input_zero_point, dtype);

  // dequantize tensors
  const auto out_cpu_deq = at::dequantize(out_q_cpu);
  const auto out_vk_deq = at::dequantize(out_q_vk);
  const auto out_vk_deq_cpu = out_vk_deq.cpu();

  // check dequantized tensor are equal
  const float tolerance = input_scale;
  // tolerated error = scale, to allow for precision differences after dividing
  // by random scale, which could result on a difference of 1 unit in the
  // quantized result.
  const auto check = almostEqual(out_cpu_deq, out_vk_deq_cpu, tolerance);

  if (!check) {
    const auto error = at::abs(out_vk_deq_cpu - out_cpu_deq).max().item<float>();
    std::cout
      << "Quantize and Dequantize failed with input shape: " << input_shape
      << " scale: " << input_scale << " and zero point: " << input_zero_point
    << std::endl;
    std::cout << "Error: " << error << std::endl;
  }
  ASSERT_TRUE(check);
}

void test_quantize_per_tensor_and_dequantize_random(
    const c10::ScalarType dtype) {
  const double scale = produce_random_scale();
  const int64_t zero_point = produce_random_zero_point(dtype);
  const at::IntArrayRef tensor_shape =
    {rand_pos_int(30), rand_pos_int(30), rand_pos_int(100), rand_pos_int(100)};
  test_quantize_per_tensor_and_dequantize(
    tensor_shape, scale, zero_point, dtype);
}

TEST_F(VulkanAPITest, quantize_per_tensor_and_dequantize_quint8) {
  const c10::ScalarType dtype = c10::ScalarType::QUInt8;
  test_quantize_per_tensor_and_dequantize({1, 1, 1, 1}, 0.13, 21, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 1, 4}, 0.3, 87, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 4, 1}, 0.2, 120, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 7, 7}, 0.3, 87, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 8, 8}, 0.1, 10, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 8, 8}, 0.04, 97, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 11, 17}, 0.07, 15, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 12, 17}, 0.1, 10, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 12, 17}, 0.1, 10, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 17, 12}, 0.1, 10, dtype);
  test_quantize_per_tensor_and_dequantize({2, 4, 17, 12}, 0.1, 10, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 10, 14}, 0.001, 101, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 10, 14}, 0.009, 43, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 10, 15}, 0.1, 19, dtype);
  test_quantize_per_tensor_and_dequantize({4, 4, 9, 17}, 0.1, 19, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 25, 29}, 0.1, 19, dtype);
  test_quantize_per_tensor_and_dequantize({4, 4, 25, 29}, 0.1, 19, dtype);
  test_quantize_per_tensor_and_dequantize({11, 17, 25, 29}, 0.027, 89, dtype);

  for (int i = 0; i < 20; i += 1) {
    test_quantize_per_tensor_and_dequantize_random(dtype);
  }
}

TEST_F(VulkanAPITest, quantize_per_tensor_and_dequantize_qint8) {
  const c10::ScalarType dtype = c10::ScalarType::QInt8;
  test_quantize_per_tensor_and_dequantize({1, 1, 1, 1}, 0.13, -21, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 1, 4}, 0.3, 87, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 4, 1}, 0.2, -120, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 7, 7}, 0.3, 87, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 8, 8}, 0.1, -10, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 8, 8}, 0.04, 97, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 11, 17}, 0.07, -15, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 12, 17}, 0.1, 10, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 12, 17}, 0.1, -10, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 17, 12}, 0.1, 10, dtype);
  test_quantize_per_tensor_and_dequantize({2, 4, 17, 12}, 0.1, -10, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 10, 14}, 0.001, 101, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 10, 14}, 0.009, -43, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 10, 15}, 0.1, 19, dtype);
  test_quantize_per_tensor_and_dequantize({4, 4, 9, 17}, 0.1, -19, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 25, 29}, 0.1, 19, dtype);
  test_quantize_per_tensor_and_dequantize({4, 4, 25, 29}, 0.1, -19, dtype);
  test_quantize_per_tensor_and_dequantize({11, 17, 25, 29}, 0.027, 89, dtype);

  for (int i = 0; i < 20; i += 1) {
    test_quantize_per_tensor_and_dequantize_random(dtype);
  }
}

TEST_F(VulkanAPITest, quantize_per_tensor_and_dequantize_qint32) {
  const c10::ScalarType dtype = c10::ScalarType::QInt32;
  test_quantize_per_tensor_and_dequantize({1, 1, 1, 1}, 0.13, -21123, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 1, 4}, 0.339, 8734, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 4, 1}, 0.228, -12023, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 7, 7}, 0.338, 8723, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 8, 8}, 0.193, -1023, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 8, 8}, 0.0449, 972, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 11, 17}, 0.073, -15, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 12, 17}, 0.1572, 102, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 12, 17}, 0.147, -156, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 17, 12}, 0.129, 10448, dtype);
  test_quantize_per_tensor_and_dequantize({2, 4, 17, 12}, 0.137, -10, dtype);
  test_quantize_per_tensor_and_dequantize({1, 1, 10, 14}, 0.001, 101, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 10, 14}, 0.009, -43267, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 10, 15}, 0.1243, 19, dtype);
  test_quantize_per_tensor_and_dequantize({4, 4, 9, 17}, 0.1889, -19784, dtype);
  test_quantize_per_tensor_and_dequantize({3, 5, 25, 29}, 0.1345, 196, dtype);
  test_quantize_per_tensor_and_dequantize({4, 4, 25, 29}, 0.129, -19489, dtype);
  test_quantize_per_tensor_and_dequantize({11, 17, 25, 29}, 0.027, 89, dtype);

  for (int i = 0; i < 20; i += 1) {
    test_quantize_per_tensor_and_dequantize_random(dtype);
  }
}

TEST_F(VulkanAPITest, quantized_add) {
  const auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan2 = in_cpu2.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale, zero_point, c10::ScalarType::QUInt8);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto reg_added_tensors = callOpByName(
      "quantized::add",
      "",
      out_cpu, out_cpu2, scale3, zero_point3);
  const auto vulk_added_tensors = at::native::vulkan::ops::quantized_add(
      out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(vulk_added_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu2);

  float rtol = 0;
  float atol = 0.5;
  const auto check = at::allclose(
      at::dequantize(reg_added_tensors[0].toTensor()), output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantized_add_broadcast) {
  const auto in_cpu =
      at::rand({2, 13, 1, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 =
      at::rand({2, 13, 32, 1}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan2 = in_cpu2.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale, zero_point, c10::ScalarType::QUInt8);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto reg_added_tensors = callOpByName(
      "quantized::add",
      "",
      out_cpu, out_cpu2, scale3, zero_point3);
  const auto vulk_added_tensors = at::native::vulkan::ops::quantized_add(
      out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto in_cpu3 =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(vulk_added_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu3);

  float rtol = 0;
  float atol = 0.5;
   const auto check = at::allclose(
      at::dequantize(reg_added_tensors[0].toTensor()), output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantized_add_broadcast1) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu =
      at::rand({2, 12, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 =
      at::rand({12, 1, 1}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan2 = in_cpu2.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale, zero_point, c10::ScalarType::QUInt8);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto reg_added_tensors = callOpByName(
      "quantized::add",
      "",
      out_cpu, out_cpu2, scale3, zero_point3);
  const auto vulk_added_tensors = at::native::vulkan::ops::quantized_add(
      out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto in_cpu3 =
      at::rand({2, 12, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(vulk_added_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu3);

  float rtol = 0;
  float atol = 0.5;
  const auto check = at::allclose(
      at::dequantize(reg_added_tensors[0].toTensor()), output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantized_add_broadcast2) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu =
      at::rand({32, 1}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 =
      at::rand({1, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan2 = in_cpu2.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale, zero_point, c10::ScalarType::QUInt8);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto reg_added_tensors = callOpByName(
      "quantized::add",
      "",
      out_cpu, out_cpu2, scale3, zero_point3);
  const auto vulk_added_tensors = at::native::vulkan::ops::quantized_add(
      out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto in_cpu3 =
      at::rand({32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(vulk_added_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu3);

  float rtol = 0;
  float atol = 0.5;
  const auto check = at::allclose(
      at::dequantize(reg_added_tensors[0].toTensor()), output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}


TEST_F(VulkanAPITest, quantized_add_broadcast3) {
  if (!at::is_vulkan_available()) {
    return;
  }

  const auto in_cpu =
      at::rand({32, 24}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 =
      at::rand({1}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan2 = in_cpu2.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale, zero_point, c10::ScalarType::QUInt8);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto reg_added_tensors = callOpByName(
      "quantized::add",
      "",
      out_cpu, out_cpu2, scale3, zero_point3);
  const auto vulk_added_tensors = at::native::vulkan::ops::quantized_add(
      out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto in_cpu3 =
      at::rand({32, 24}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(vulk_added_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu3);

  float rtol = 0;
  float atol = 0.5;
  const auto check = at::allclose(
      at::dequantize(reg_added_tensors[0].toTensor()), output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantized_add_dif_params) {
  const auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan2 = in_cpu2.vulkan();
  const double scale = 0.1;
  const int zero_point = 10;
  const double scale2 = 0.2;
  const int zero_point2 = 20;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale2, zero_point2, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale2, zero_point2, c10::ScalarType::QUInt8);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto reg_added_tensors = callOpByName(
      "quantized::add",
      "",
      out_cpu, out_cpu2, scale3, zero_point3);
  const auto vulk_added_tensors = at::native::vulkan::ops::quantized_add(
      out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(vulk_added_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu2);

  float rtol = 0;
  float atol = 0.5;
  const auto check = at::allclose(
      at::dequantize(reg_added_tensors[0].toTensor()), output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, conv2d) {
  constexpr int64_t groups = 1;
  constexpr std::array<int64_t, 2u> stride{2, 2};
  constexpr std::array<int64_t, 2u> padding{1, 1};
  // TODO: Support conv2d with dilation != 1
  constexpr std::array<int64_t, 2u> dilation{1, 1};

  constexpr struct {
    uint32_t batches;
    uint32_t channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
          batches,
          channels,
          width,
          height,
      };
    }
  } input{1, 3, 8, 8};

  constexpr struct {
    uint32_t output_channels;
    uint32_t input_channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
          output_channels,
          input_channels,
          width,
          height,
      };
    }
  } weights{1, input.channels, 3, 3};

  float r1 = 0.1;
  float r2 = 0.7;
  const auto input_cpu = (r1 - r2) *
          at::rand(input.size(), at::device(at::kCPU).dtype(at::kFloat)) +
      r2;
  const auto weights_cpu = (r1 - r2) *
          at::rand(weights.size(), at::device(at::kCPU).dtype(at::kFloat)) +
      r2;
  const auto bias_cpu = (r1 - r2) *
          at::rand({weights.output_channels},
                   at::device(at::kCPU).dtype(at::kFloat)) +
      r2;

  const double w_scale = 0.1;
  const int w_zero_point = 10;

  const double b_scale = 0.1;
  const int b_zero_point = 10;

  const auto weight_q = at::quantize_per_tensor(
      weights_cpu, w_scale, w_zero_point, c10::ScalarType::QUInt8);
  const auto bias_q = at::quantize_per_tensor(
      bias_cpu, b_scale, b_zero_point, c10::ScalarType::QUInt8);

  const auto output_cpu = at::conv2d(
      input_cpu, weights_cpu, bias_cpu, stride, padding, dilation, groups);

  const double scale = 0.10;
  const int zero_point = 10;
  const auto shape_match =
      at::rand({1, 1, 4, 4}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = input_cpu.vulkan();
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);

  const double scale2 = 0.15;
  const int zero_point2 = 15;
  const auto output_vulkan = at::native::vulkan::ops::quantized_conv2d(
      out_vulkan,
      weight_q,
      bias_q,
      stride,
      padding,
      dilation,
      groups,
      scale2,
      zero_point2);

  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(output_vulkan);
  auto output_for_dequantized_vulkan =
      vulkan_to_cpu(out_vulkan_deq, shape_match);

  float rtol = 0;
  float atol = 1.5;
  const auto check =
      at::allclose(output_cpu, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, conv2d_pw) {
  constexpr int64_t groups = 1;
  constexpr std::array<int64_t, 2u> stride{1, 1};
  constexpr std::array<int64_t, 2u> padding{0, 0};
  constexpr std::array<int64_t, 2u> dilation{1, 1};

  constexpr struct {
    uint32_t batches;
    uint32_t channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
          batches,
          channels,
          width,
          height,
      };
    }
  } input{1, 17, 127, 397};

  constexpr struct {
    uint32_t output_channels;
    uint32_t input_channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
          output_channels,
          input_channels,
          width,
          height,
      };
    }
  } weights{29, input.channels, 1, 1};

  float r1 = 0.1;
  float r2 = 0.7;
  const auto input_cpu = (r1 - r2) *
          at::rand(input.size(), at::device(at::kCPU).dtype(at::kFloat)) +
      r2;
  const auto weights_cpu = (r1 - r2) *
          at::rand(weights.size(), at::device(at::kCPU).dtype(at::kFloat)) +
      r2;
  const auto bias_cpu = (r1 - r2) *
          at::rand({weights.output_channels},
                   at::device(at::kCPU).dtype(at::kFloat)) +
      r2;

  const double w_scale = 0.1;
  const int w_zero_point = 10;

  const double b_scale = 0.1;
  const int b_zero_point = 10;

  const auto weight_q = at::quantize_per_tensor(
      weights_cpu, w_scale, w_zero_point, c10::ScalarType::QUInt8);
  const auto bias_q = at::quantize_per_tensor(
      bias_cpu, b_scale, b_zero_point, c10::ScalarType::QUInt8);

  const auto output_cpu = at::conv2d(
      input_cpu, weights_cpu, bias_cpu, stride, padding, dilation, groups);

  const double scale = 0.10;
  const int zero_point = 10;
  const auto shape_match =
      at::rand({1, 29, 127, 397}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = input_cpu.vulkan();
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);

  const double scale2 = 0.15;
  const int zero_point2 = 15;
  const auto output_vulkan = at::native::vulkan::ops::quantized_conv2d(
      out_vulkan,
      weight_q,
      bias_q,
      stride,
      padding,
      dilation,
      groups,
      scale2,
      zero_point2);

  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(output_vulkan);
  auto output_for_dequantized_vulkan =
      vulkan_to_cpu(out_vulkan_deq, shape_match);

  float rtol = 0;
  float atol = 1.5;
  const auto check =
      at::allclose(output_cpu, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, conv2d_dw) {
  constexpr int64_t groups = 7;
  constexpr std::array<int64_t, 2u> stride{2, 3};
  constexpr std::array<int64_t, 2u> padding{0, 4};
  constexpr std::array<int64_t, 2u> dilation{3, 1};

  constexpr struct {
    uint32_t batches;
    uint32_t channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
          batches,
          channels,
          width,
          height,
      };
    }
  } input{1, groups, 137, 199};

  constexpr struct {
    uint32_t output_channels;
    uint32_t input_channels;
    uint32_t width;
    uint32_t height;

    std::array<int64_t, 4u> size() const {
      return {
          output_channels,
          input_channels,
          width,
          height,
      };
    }
  } weights{groups, 1, 17, 7};

  float r1 = 0;
  float r2 = 0.2;
  const auto input_cpu = (r1 - r2) *
          at::rand(input.size(), at::device(at::kCPU).dtype(at::kFloat)) +
      r2;
  const auto weights_cpu = (r1 - r2) *
          at::rand(weights.size(), at::device(at::kCPU).dtype(at::kFloat)) +
      r2;
  const auto bias_cpu = (r1 - r2) *
          at::rand({weights.output_channels},
                   at::device(at::kCPU).dtype(at::kFloat)) +
      r2;

  const double w_scale = 0.1;
  const int w_zero_point = 10;

  const double b_scale = 0.1;
  const int b_zero_point = 10;

  const auto weight_q = at::quantize_per_tensor(
      weights_cpu, w_scale, w_zero_point, c10::ScalarType::QUInt8);
  const auto bias_q = at::quantize_per_tensor(
      bias_cpu, b_scale, b_zero_point, c10::ScalarType::QUInt8);

  const auto output_cpu = at::conv2d(
      input_cpu, weights_cpu, bias_cpu, stride, padding, dilation, groups);

  const double scale = 0.10;
  const int zero_point = 10;
  const auto shape_match =
      at::rand({1, 7, 45, 67}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = input_cpu.vulkan();
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);

  const double scale2 = 0.15;
  const int zero_point2 = 15;
  const auto output_vulkan = at::native::vulkan::ops::quantized_conv2d(
      out_vulkan,
      weight_q,
      bias_q,
      stride,
      padding,
      dilation,
      groups,
      scale2,
      zero_point2);

  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(output_vulkan);
  auto output_for_dequantized_vulkan =
      vulkan_to_cpu(out_vulkan_deq, shape_match);

  float rtol = 0;
  float atol = 1;
  const auto check =
      at::allclose(output_cpu, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantized_sub) {
  float r1 = 4.0;
  float r2 = 7.0;

  float r3 = 2.0;
  float r4 = 5.0;
  const auto in_cpu = (r1 - r2) *
          at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) +
      r2;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 = (r3 - r4) *
          at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) +
      r4;
  const auto in_vulkan2 = in_cpu2.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale, zero_point, c10::ScalarType::QUInt8);

  const auto reg_subtracted_tensors = at::sub(in_cpu, in_cpu2);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto vulk_subtracted_tensors = at::native::vulkan::ops::quantized_sub(
      out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(vulk_subtracted_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu2);

  float rtol = 0;
  float atol = 0.5;
  const auto check = at::allclose(
      reg_subtracted_tensors, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantized_mul) {
  const auto in_cpu =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 =
      at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) * 6;
  const auto in_vulkan2 = in_cpu2.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale, zero_point, c10::ScalarType::QUInt8);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto reg_mul_tensors = callOpByName(
      "quantized::mul", "", out_cpu, out_cpu2, scale3, zero_point3);
  const auto vulk_mul_tensors = at::native::vulkan::ops::quantized_mul(
      out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(vulk_mul_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu2);

  float rtol = 0;
  float atol = 1.5;
  const auto check = at::allclose(
      at::dequantize(reg_mul_tensors[0].toTensor()),
      output_for_dequantized_vulkan,
      rtol,
      atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantized_div) {
  float r1 = 2.0;
  float r2 = 3.5;

  float r3 = 4.0;
  float r4 = 5.5;
  const auto in_cpu = (r1 - r2) *
          at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) +
      r2;
  const auto in_vulkan = in_cpu.vulkan();
  const auto in_cpu2 = (r3 - r4) *
          at::rand({2, 13, 32, 27}, at::device(at::kCPU).dtype(at::kFloat)) +
      r4;
  const auto in_vulkan2 = in_cpu2.vulkan();

  const double scale = 0.1;
  const int zero_point = 10;

  const auto out_cpu = at::quantize_per_tensor(
      in_cpu, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_cpu2 = at::quantize_per_tensor(
      in_cpu2, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto out_vulkan2 = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan2, scale, zero_point, c10::ScalarType::QUInt8);

  const auto reg_div_tensors = at::div(in_cpu, in_cpu2);

  const double scale3 = 0.15;
  const int zero_point3 = 15;
  const auto vulk_div_tensors = at::native::vulkan::ops::quantized_div(
      out_vulkan, out_vulkan2, scale3, zero_point3);

  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(vulk_div_tensors);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu2);

  float rtol = 0;
  float atol = 1;
  const auto check =
      at::allclose(reg_div_tensors, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

TEST_F(VulkanAPITest, quantized_upsample_nearest2d) {
  const auto in_cpu =
      at::rand({2, 13, 12, 27}, at::TensorOptions(at::kCPU).dtype(at::kFloat));
  const auto out_cpu = at::upsample_nearest2d(in_cpu, {4, 6}, 1, 1);

  const double scale = 0.1;
  const int zero_point = 10;

  const auto in_vulkan = in_cpu.vulkan();
  const auto out_vulkan = at::native::vulkan::ops::quantize_per_tensor(
      in_vulkan, scale, zero_point, c10::ScalarType::QUInt8);
  const auto upsample_vulkan =
      at::native::vulkan::ops::quantized_upsample_nearest2d(
          out_vulkan, {4, 6}, 1, 1);

  const auto in_cpu2 =
      at::rand({2, 13, 4, 6}, at::TensorOptions(at::kCPU).dtype(at::kFloat));
  const auto out_vulkan_deq =
      at::native::vulkan::ops::dequantize(upsample_vulkan);
  auto output_for_dequantized_vulkan = vulkan_to_cpu(out_vulkan_deq, in_cpu2);

  float rtol = 0;
  float atol = 1;
  const auto check =
      at::allclose(out_cpu, output_for_dequantized_vulkan, rtol, atol);

  if (!check) {
    std::cout << "Max Diff allowed: " << rtol << std::endl;
  }

  ASSERT_TRUE(check);
}

std::tuple<double, double, int64_t, int64_t> produce_inputs_for_binary_op(
    const bool compute_quantization_params,
    const bool random_quantization_params,
    const char* op_name,
    const at::IntArrayRef input1_shape,
    const at::IntArrayRef input2_shape,
    double in1_scale, double in2_scale,
    int in1_zero_point, int in2_zero_point,
    at::Tensor& input1_cpu, at::Tensor& input1_cpu_q,
    at::Tensor& input1_cpu_deq,
    at::Tensor& input1_vk, at::Tensor& input1_vk_q,
    at::Tensor& input1_vk_deq, at::Tensor& input1_vk_deq_cpu,
    at::Tensor& input2_cpu, at::Tensor& input2_cpu_q,
    at::Tensor& input2_cpu_deq,
    at::Tensor& input2_vk, at::Tensor& input2_vk_q,
    at::Tensor& input2_vk_deq, at::Tensor& input2_vk_deq_cpu) {

  int num_attempts = 5;
    // in order to make sure we start with input tensors that are numerically
    // the same (cpu vs vulkan), we allow multiple attempts when randomly
    // generating the inputs. If the cpu quantized tensor and the vk quantized
    // tensors are not the same (maybe off by 1 due to differences in rounding
    // and precision), we try again.
  for (int i = 0; i < num_attempts; i += 1) {
    // produce random inputs
    input1_cpu = produce_random_tensor(input1_shape);
    input2_cpu = produce_random_tensor(input1_shape);

    if (compute_quantization_params) {
      // compute appropiate scale and zero point for inputs
      const auto in1_quant_params = compute_quant_params(input1_cpu);
      in1_scale = std::get<0>(in1_quant_params);
      in1_zero_point = std::get<1>(in1_quant_params);

      const auto in2_quant_params = compute_quant_params(input2_cpu);
      in2_scale = std::get<0>(in2_quant_params);
      in2_zero_point = std::get<1>(in2_quant_params);
    } else if (random_quantization_params) {
      // produce random scale and zero point for inputs
      in1_scale = produce_random_scale();
      in1_zero_point = produce_random_zero_point(c10::ScalarType::QUInt8);

      in2_scale = produce_random_scale();
      in2_zero_point = produce_random_zero_point(c10::ScalarType::QUInt8);
    }

    // we do this, to avoid dividing by zero
    if (strcmp(op_name, "quantized::div") == 0) {
      const auto non_zero_sign = input2_cpu.sign() - input2_cpu.sign().abs() + 1;
        // non_zero_sign = 1 if the value is non negative, and -1 if it is negative
      input2_cpu = input2_cpu + in2_scale * non_zero_sign;
        // this will force abs(input2_cpu) >= in2_scale, which means that none of
        // the quantized values of the second input will be equal to the zero point.

      // we might end up dividing by 0, if we allow random scale and zero point
      // of the divisor.
      if (random_quantization_params) {
        const auto in2_quant_params = compute_quant_params(input2_cpu);
        in2_scale = std::get<0>(in2_quant_params);
        in2_zero_point = std::get<1>(in2_quant_params);
      }
    }

    // quantize cpu inputs
    input1_cpu_q = at::quantize_per_tensor(
        input1_cpu, in1_scale, in1_zero_point, c10::ScalarType::QUInt8);
    input2_cpu_q = at::quantize_per_tensor(
        input2_cpu, in2_scale, in2_zero_point, c10::ScalarType::QUInt8);

    // dequantize quantized cpu inputs
    input1_cpu_deq = at::dequantize(input1_cpu_q);
    input2_cpu_deq = at::dequantize(input2_cpu_q);

    // vulkan quantized inputs
    input1_vk = input1_cpu.vulkan();
    input1_vk_q = at::quantize_per_tensor(
        input1_vk, in1_scale, in1_zero_point, c10::ScalarType::QUInt8);
    input2_vk = input2_cpu.vulkan();
    input2_vk_q = at::quantize_per_tensor(
        input2_vk, in2_scale, in2_zero_point, c10::ScalarType::QUInt8);

    // dequantize quantized vulkan inputs
    input1_vk_deq = at::dequantize(input1_vk_q);
    input2_vk_deq = at::dequantize(input2_vk_q);

    input1_vk_deq_cpu = input1_vk_deq.cpu();
    input2_vk_deq_cpu = input2_vk_deq.cpu();

    const float input1_dif = at::abs(input1_cpu_deq - input1_vk_deq_cpu).max().item<float>();
    const float input2_dif = at::abs(input2_cpu_deq - input2_vk_deq_cpu).max().item<float>();
    if (input1_dif < 1e-5 && input2_dif < 1e-5 && input1_dif < in1_scale/2 && input2_dif < in2_scale/2) {
      break;
    }
  }

  return {in1_scale, in2_scale, in1_zero_point, in2_zero_point};
}

at::Tensor apply_cpu_quantized_binary_op(
    const char* op_name,
    at::Tensor input1_cpu_deq,
    at::Tensor input2_cpu_deq) {
  if (strcmp(op_name, "quantized::add") == 0) {
    return at::add(input1_cpu_deq, input2_cpu_deq);
  } else if (strcmp(op_name, "quantized::sub") == 0) {
    return at::sub(input1_cpu_deq, input2_cpu_deq);
  } else if (strcmp(op_name, "quantized::mul") == 0) {
    return at::mul(input1_cpu_deq, input2_cpu_deq);
  } else if (strcmp(op_name, "quantized::div") == 0) {
    return at::div(input1_cpu_deq, input2_cpu_deq);
  } else {
    TORCH_CHECK(false, "Invalid op");
  }
}

at::Tensor apply_vulkan_quantized_binary_op(
    const char* op_name,
    at::Tensor input1_vk_q,
    at::Tensor input2_vk_q,
    double out_scale,
    int64_t out_zero_point) {
  if (strcmp(op_name, "quantized::add") == 0) {
    return at::native::vulkan::ops::quantized_add(
      input1_vk_q, input2_vk_q, out_scale, out_zero_point);
  } else if (strcmp(op_name, "quantized::sub") == 0) {
    return at::native::vulkan::ops::quantized_sub(
      input1_vk_q, input2_vk_q, out_scale, out_zero_point);
  } else if (strcmp(op_name, "quantized::mul") == 0) {
    return at::native::vulkan::ops::quantized_mul(
      input1_vk_q, input2_vk_q, out_scale, out_zero_point);
  } else if (strcmp(op_name, "quantized::div") == 0) {
    return at::native::vulkan::ops::quantized_div(
      input1_vk_q, input2_vk_q, out_scale, out_zero_point);
  } else {
    TORCH_CHECK(false, "Invalid op");
  }
}

void test_quantized_binary_op(
    const bool compute_quantization_params,
    const bool random_quantization_params,
    const char* op_name,
    const at::IntArrayRef input1_shape,
    const at::IntArrayRef input2_shape,
    double in1_scale_default = 0.103,
    double in2_scale_default = 0.171,
    double out_scale_default = 0.139,
    int64_t in1_zero_point_default = 11,
    int64_t in2_zero_point_default = 9,
    int64_t out_zero_point_default = 17) {

  // produce inputs
  at::Tensor input1_cpu, input1_cpu_q, input1_cpu_deq;
  at::Tensor input1_vk, input1_vk_q, input1_vk_deq, input1_vk_deq_cpu;
  at::Tensor input2_cpu, input2_cpu_q, input2_cpu_deq;
  at::Tensor input2_vk, input2_vk_q, input2_vk_deq, input2_vk_deq_cpu;

  auto input_params = produce_inputs_for_binary_op(
    compute_quantization_params, random_quantization_params, op_name,
    input1_shape, input2_shape,
    in1_scale_default, in2_scale_default,
    in1_zero_point_default, in2_zero_point_default,
    input1_cpu, input1_cpu_q, input1_cpu_deq,
    input1_vk, input1_vk_q, input1_vk_deq, input1_vk_deq_cpu,
    input2_cpu, input2_cpu_q, input2_cpu_deq,
    input2_vk, input2_vk_q, input2_vk_deq, input2_vk_deq_cpu);

  double in1_scale = std::get<0>(input_params);
  double in2_scale = std::get<1>(input_params);
  int64_t in1_zero_point = std::get<2>(input_params);
  int64_t in2_zero_point = std::get<3>(input_params);

  double out_scale = out_scale_default;
  int64_t out_zero_point = out_zero_point_default;

  // apply op on dequantized cpu tensors
  at::Tensor output_cpu = apply_cpu_quantized_binary_op(
    op_name, input1_cpu_deq, input2_cpu_deq);

  if (compute_quantization_params || random_quantization_params) {
    // compute appropiate scale and zero point for output
    const auto out_quant_params = compute_quant_params(output_cpu);
    out_scale = std::get<0>(out_quant_params);
    out_zero_point = std::get<1>(out_quant_params);
  }

  // quantize and dequantize cpu output
  const auto output_cpu_q = at::quantize_per_tensor(
      output_cpu, out_scale, out_zero_point, c10::ScalarType::QUInt8);
  const auto output_cpu_deq = at::dequantize(output_cpu_q);

  // vulkan quantized output
  at::Tensor output_vk_q = apply_vulkan_quantized_binary_op(
    op_name, input1_vk_q, input2_vk_q, out_scale, out_zero_point);

  const auto output_vk_deq = at::dequantize(output_vk_q);
  const auto output_vk_deq_cpu = output_vk_deq.cpu();

  // check
  const float tolerance =
    (compute_quantization_params || random_quantization_params) ? out_scale : 0;
  const auto check = almostEqual(output_cpu_deq, output_vk_deq_cpu, tolerance);

  if (!check) {
    const auto vk_q_error = at::abs(output_vk_deq_cpu - output_cpu_deq).max().item<float>();
    std::cout << "Binary op " << op_name << " failed with inputs: " << std::endl;
    std::cout << "input1: shape " << input1_shape << " scale " << in1_scale
              << " and zero point " << in1_zero_point << std::endl;
    std::cout << "input2: shape " << input2_shape << " scale " << in2_scale
              << " and zero point " << in2_zero_point << std::endl;
    std::cout << "output scale " << out_scale
              << " and zero point " << out_zero_point << std::endl;
    std::cout << "error: " << vk_q_error << std::endl;
  }
  ASSERT_TRUE(check);
}

void quantized_binary_op_test_set(
    const char* op_name) {
  // fixed params
  test_quantized_binary_op(false, false, op_name, {1, 1, 1, 1}, {1, 1, 1, 1});
  test_quantized_binary_op(false, false, op_name, {1, 1, 8, 8}, {1, 1, 8, 8});
  test_quantized_binary_op(false, false, op_name, {1, 1, 12, 17}, {1, 1, 12, 17});
  test_quantized_binary_op(false, false, op_name, {2, 13, 32, 27}, {2, 13, 32, 27});
  test_quantized_binary_op(false, false, op_name, {7, 15, 6, 17}, {7, 15, 1, 17}); // broadcasting
  test_quantized_binary_op(false, false, op_name, {7, 1, 6, 17}, {7, 5, 6, 17}); // broadcasting

  // compute params
  test_quantized_binary_op(true, false, op_name, {1, 1, 1, 1}, {1, 1, 1, 1});
  test_quantized_binary_op(true, false, op_name, {1, 1, 8, 8}, {1, 1, 8, 8});
  test_quantized_binary_op(true, false, op_name, {1, 1, 12, 17}, {1, 1, 12, 17});
  test_quantized_binary_op(true, false, op_name, {2, 13, 32, 27}, {2, 13, 32, 27});
  test_quantized_binary_op(true, false, op_name, {7, 15, 6, 17}, {7, 15, 1, 17}); // broadcasting
  test_quantized_binary_op(true, false, op_name, {7, 1, 6, 17}, {7, 5, 6, 17}); // broadcasting

  // random params
  test_quantized_binary_op(false, true, op_name, {1, 1, 1, 1}, {1, 1, 1, 1});
  test_quantized_binary_op(false, true, op_name, {1, 1, 8, 8}, {1, 1, 8, 8});
  test_quantized_binary_op(false, true, op_name, {1, 1, 12, 17}, {1, 1, 12, 17});
  test_quantized_binary_op(false, true, op_name, {2, 13, 32, 27}, {2, 13, 32, 27});
  test_quantized_binary_op(false, true, op_name, {7, 15, 6, 17}, {7, 15, 1, 17}); // broadcasting
  test_quantized_binary_op(false, true, op_name, {7, 1, 6, 17}, {7, 5, 6, 17}); // broadcasting

  // random shape and params
  for (int i = 0; i < 10; i += 1) {
    const at::IntArrayRef tensor_shape =
      {
        rand_pos_int(30),
        rand_pos_int(30),
        rand_pos_int(100),
        rand_pos_int(100)
      };
    test_quantized_binary_op(false, true, op_name, tensor_shape, tensor_shape);
  }
}

TEST_F(VulkanAPITest, quantized_add_tests) {
  quantized_binary_op_test_set("quantized::add");
}

TEST_F(VulkanAPITest, quantized_sub_tests) {
  quantized_binary_op_test_set("quantized::sub");
}

TEST_F(VulkanAPITest, quantized_mul_tests) {
  quantized_binary_op_test_set("quantized::mul");
}

TEST_F(VulkanAPITest, quantized_div_tests) {
  quantized_binary_op_test_set("quantized::div");
}

} // namespace

#endif /* USE_VULKAN_API */
