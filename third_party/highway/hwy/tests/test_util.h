// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HWY_TESTS_TEST_UTIL_H_
#define HWY_TESTS_TEST_UTIL_H_

// Target-independent helper functions for use by *_test.cc.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>

#include "hwy/aligned_allocator.h"
#include "hwy/base.h"
#include "hwy/highway.h"

namespace hwy {

// The maximum vector size used in tests when defining test data. DEPRECATED.
constexpr size_t kTestMaxVectorSize = 64;

// 64-bit random generator (Xorshift128+). Much smaller state than std::mt19937,
// which triggers a compiler bug.
class RandomState {
 public:
  explicit RandomState(const uint64_t seed = 0x123456789ull) {
    s0_ = SplitMix64(seed + 0x9E3779B97F4A7C15ull);
    s1_ = SplitMix64(s0_);
  }

  HWY_INLINE uint64_t operator()() {
    uint64_t s1 = s0_;
    const uint64_t s0 = s1_;
    const uint64_t bits = s1 + s0;
    s0_ = s0;
    s1 ^= s1 << 23;
    s1 ^= s0 ^ (s1 >> 18) ^ (s0 >> 5);
    s1_ = s1;
    return bits;
  }

 private:
  static uint64_t SplitMix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

  uint64_t s0_;
  uint64_t s1_;
};

static HWY_INLINE uint32_t Random32(RandomState* rng) {
  return static_cast<uint32_t>((*rng)());
}

static HWY_INLINE uint64_t Random64(RandomState* rng) {
  return (*rng)();
}

// Prevents the compiler from eliding the computations that led to "output".
// Works by indicating to the compiler that "output" is being read and modified.
// The +r constraint avoids unnecessary writes to memory, but only works for
// built-in types.
template <class T>
inline void PreventElision(T&& output) {
#if HWY_COMPILER_MSVC
  (void)output;
#else   // HWY_COMPILER_MSVC
  asm volatile("" : "+r"(output) : : "memory");
#endif  // HWY_COMPILER_MSVC
}

bool BytesEqual(const void* p1, const void* p2, const size_t size,
                size_t* pos = nullptr);

void AssertStringEqual(const char* expected, const char* actual,
                       const char* target_name, const char* filename, int line);

namespace detail {

template <typename T, typename TU = MakeUnsigned<T>>
TU ComputeUlpDelta(const T expected, const T actual) {
  // Handle -0 == 0 and infinities.
  if (expected == actual) return 0;

  // Consider "equal" if both are NaN, so we can verify an expected NaN.
  // Needs a special case because there are many possible NaN representations.
  if (std::isnan(expected) && std::isnan(actual)) return 0;

  // Compute the difference in units of last place. We do not need to check for
  // differing signs; they will result in large differences, which is fine.
  TU ux, uy;
  CopyBytes<sizeof(T)>(&expected, &ux);
  CopyBytes<sizeof(T)>(&actual, &uy);

  // Avoid unsigned->signed cast: 2's complement is only guaranteed by C++20.
  const TU ulp = HWY_MAX(ux, uy) - HWY_MIN(ux, uy);
  return ulp;
}

// For implementing value comparisons etc. as type-erased functions to reduce
// template bloat.
struct TypeInfo {
  size_t sizeof_t;
  bool is_float;
  bool is_signed;
};

template <typename T>
HWY_INLINE TypeInfo MakeTypeInfo() {
  TypeInfo info;
  info.sizeof_t = sizeof(T);
  info.is_float = IsFloat<T>();
  info.is_signed = IsSigned<T>();
  return info;
}

bool IsEqual(const TypeInfo& info, const void* expected_ptr,
             const void* actual_ptr);

void TypeName(const TypeInfo& info, size_t N, char* string100);

void PrintArray(const TypeInfo& info, const char* caption,
                const void* array_void, size_t N, size_t lane_u = 0,
                size_t max_lanes = 7);

HWY_NORETURN void PrintMismatchAndAbort(const TypeInfo& info,
                                        const void* expected_ptr,
                                        const void* actual_ptr,
                                        const char* target_name,
                                        const char* filename, int line,
                                        size_t lane = 0, size_t num_lanes = 1);

void AssertArrayEqual(const TypeInfo& info, const void* expected_void,
                      const void* actual_void, size_t N,
                      const char* target_name, const char* filename, int line);

}  // namespace detail

// Returns a name for the vector/part/scalar. The type prefix is u/i/f for
// unsigned/signed/floating point, followed by the number of bits per lane;
// then 'x' followed by the number of lanes. Example: u8x16. This is useful for
// understanding which instantiation of a generic test failed.
template <typename T>
std::string TypeName(T /*unused*/, size_t N) {
  char string100[100];
  detail::TypeName(detail::MakeTypeInfo<T>(), N, string100);
  return string100;
}

// Compare non-vector, non-string T.
template <typename T>
HWY_INLINE bool IsEqual(const T expected, const T actual) {
  const auto info = detail::MakeTypeInfo<T>();
  return detail::IsEqual(info, &expected, &actual);
}

template <typename T>
HWY_INLINE void AssertEqual(const T expected, const T actual,
                            const char* target_name, const char* filename,
                            int line, size_t lane = 0) {
  const auto info = detail::MakeTypeInfo<T>();
  if (!detail::IsEqual(info, &expected, &actual)) {
    detail::PrintMismatchAndAbort(info, &expected, &actual, target_name,
                                  filename, line, lane);
  }
}

}  // namespace hwy

#endif  // HWY_TESTS_TEST_UTIL_H_
