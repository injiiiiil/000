#pragma once

#include <ATen/Dispatch.h>
#include <ATen/core/DistributionsHelper.h>
#include <ATen/native/TensorIterator.h>
#include <ATen/native/cpu/Loops.h>
#include <mutex>

namespace at { namespace native { namespace templates {

template<typename RNG>
static void random_from_to_kernel(TensorIterator& iter, uint64_t range, int64_t base, RNG* generator) {
  AT_DISPATCH_ALL_TYPES_AND(at::ScalarType::Bool, iter.dtype(), "random_from_to_cpu", [&] {
    std::lock_guard<std::mutex> lock(generator->mutex_);
    if ((
      std::is_same<scalar_t, int64_t>::value ||
      std::is_same<scalar_t, double>::value ||
      std::is_same<scalar_t, float>::value) && range >= 1ULL << 32)
    {
      cpu_serial_kernel(iter, [range, base, generator]() -> scalar_t {
        return static_cast<scalar_t>(static_cast<int64_t>((generator->random64() % range) + base));
      });
    } else {
      cpu_serial_kernel(iter, [range, base, generator]() -> scalar_t {
        return static_cast<scalar_t>(static_cast<int64_t>((generator->random() % range) + base));
      });
    }
  });
}

template<typename RNG>
static void random_kernel(TensorIterator& iter, RNG* generator) {
  std::lock_guard<std::mutex> lock(generator->mutex_);
  if (isFloatingType(iter.dtype())) {
    AT_DISPATCH_FLOATING_TYPES_AND2(at::ScalarType::Half, at::ScalarType::BFloat16, iter.dtype(), "random_cpu", [&] {
      if (std::is_same<scalar_t, double>::value) {
        cpu_serial_kernel(iter, [generator]() -> scalar_t {
          return generator->random64() % static_cast<uint64_t>((1ULL << std::numeric_limits<scalar_t>::digits) + 1);
        });
      } else {
        cpu_serial_kernel(iter, [generator]() -> scalar_t {
          return generator->random() % static_cast<uint64_t>((1ULL << std::numeric_limits<scalar_t>::digits) + 1);
        });
      }
    });
  } else if (isIntegralType(iter.dtype(), /*includeBool=*/true)) {
    AT_DISPATCH_INTEGRAL_TYPES_AND(at::ScalarType::Bool, iter.dtype(), "random_cpu", [&] {
      if (std::is_same<scalar_t, int64_t>::value) {
        cpu_serial_kernel(iter, [generator]() -> scalar_t {
          return generator->random64() % (static_cast<uint64_t>(std::numeric_limits<scalar_t>::max()) + 1);
        });
      } else if (std::is_same<scalar_t, bool>::value) {
        cpu_serial_kernel(iter, [generator]() -> scalar_t {
          return generator->random() & 1;
        });
      } else {
        cpu_serial_kernel(iter, [generator]() -> scalar_t {
          return generator->random() % (static_cast<uint64_t>(std::numeric_limits<scalar_t>::max()) + 1);
        });
      }
    });
  }
}

// This is the special kernel to handle single specific case:
// from(inclusive) = std::numeric_limits<int64_t>::lowest()
// to(exclusive) = None (= std::numeric_limits<int64_t>::max() + 1)
template<typename RNG>
static void random_full_64_range_kernel(TensorIterator& iter, RNG* generator) {
  AT_DISPATCH_ALL_TYPES(iter.dtype(), "random64_cpu", [&] {
    std::lock_guard<std::mutex> lock(generator->mutex_);
    if (std::is_same<scalar_t, int64_t>::value ||
        std::is_same<scalar_t, double>::value ||
        std::is_same<scalar_t, float>::value) {
      cpu_serial_kernel(iter, [generator]() -> scalar_t {
        return generator->random64(); // use all 64 bits
      });
    } else {
      TORCH_CHECK(false, "random_full_64_range_kernel handles only int64, double and float");
    }
  });
}

template<typename RNG>
void cauchy_kernel(TensorIterator& iter, double median, double sigma, RNG* generator) {
  AT_DISPATCH_FLOATING_TYPES(iter.dtype(), "cauchy_cpu", [&]() {
    std::lock_guard<std::mutex> lock(generator->mutex_);
    cpu_serial_kernel(iter, [median, sigma, generator]() -> scalar_t {
      at::cauchy_distribution<double> cauchy(median, sigma);
      return (scalar_t)cauchy(generator);
    });
  });
}

}}}
