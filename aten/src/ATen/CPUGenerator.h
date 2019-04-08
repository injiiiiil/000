#pragma once

#include <ATen/core/Generator.h>
#include <ATen/core/MT19937RNGEngine.h>
#include <ATen/core/PhiloxRNGEngine.h>

namespace at {

struct CAFFE2_API CPUGenerator : public CloneableGenerator<CPUGenerator, Generator> {
  // Constructors
  CPUGenerator(uint64_t seed_in = default_rng_seed_val);
  CPUGenerator(mt19937 engine_in);

  // CPUGenerator methods
  void set_current_seed(uint64_t seed) override;
  uint64_t current_seed() const override;
  static DeviceType device_type();
  uint32_t random();
  uint64_t random64();
  uint32_t normal_cache_index();
  void set_normal_cache_index(uint32_t index);
  at::detail::Array<float, 2> normal_cache_floats();
  at::detail::Array<double, 2> normal_cache_doubles();
  void set_normal_cache_floats(at::detail::Array<float, 2> randoms);
  void set_normal_cache_doubles(at::detail::Array<double, 2> randoms);

private:
  CloneableGenerator<CPUGenerator, Generator>* clone_impl() const override;
  at::mt19937 engine_;
  uint32_t normal_cache_index_;
  at::detail::Array<float, 2> normal_cache_floats_;
  at::detail::Array<double, 2> normal_cache_doubles_;
};

namespace detail {

CAFFE2_API std::unique_ptr<CPUGenerator>& getDefaultCPUGenerator();
CAFFE2_API std::unique_ptr<CPUGenerator> createCPUGenerator(uint64_t seed_val = default_rng_seed_val);
CAFFE2_API uint64_t getNonDeterministicRandom();

} // namespace detail

}
