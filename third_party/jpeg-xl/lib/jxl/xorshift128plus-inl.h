// Copyright (c) the JPEG XL Project
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

// Fast but weak random generator.

#if defined(LIB_JXL_XORSHIFT128PLUS_INL_H_) == defined(HWY_TARGET_TOGGLE)
#ifdef LIB_JXL_XORSHIFT128PLUS_INL_H_
#undef LIB_JXL_XORSHIFT128PLUS_INL_H_
#else
#define LIB_JXL_XORSHIFT128PLUS_INL_H_
#endif

#include <stddef.h>

#include <hwy/highway.h>
HWY_BEFORE_NAMESPACE();
namespace jxl {
namespace HWY_NAMESPACE {
namespace {

// These templates are not found via ADL.
using hwy::HWY_NAMESPACE::ShiftLeft;
using hwy::HWY_NAMESPACE::ShiftRight;

// Adapted from https://github.com/vpxyz/xorshift/blob/master/xorshift128plus/
// (MIT-license)
class Xorshift128Plus {
 public:
  // 8 independent generators (= single iteration for AVX-512)
  enum { N = 8 };

  explicit HWY_MAYBE_UNUSED Xorshift128Plus(const uint64_t seed) {
    // Init state using SplitMix64 generator
    s0_[0] = SplitMix64(seed + 0x9E3779B97F4A7C15ull);
    s1_[0] = SplitMix64(s0_[0]);
    for (size_t i = 1; i < N; ++i) {
      s0_[i] = SplitMix64(s1_[i - 1]);
      s1_[i] = SplitMix64(s0_[i]);
    }
  }

  HWY_INLINE HWY_MAYBE_UNUSED void Fill(uint64_t* HWY_RESTRICT random_bits) {
#if HWY_CAP_INTEGER64
    const HWY_FULL(uint64_t) d;
    for (size_t i = 0; i < N; i += Lanes(d)) {
      auto s1 = Load(d, s0_ + i);
      const auto s0 = Load(d, s1_ + i);
      const auto bits = s1 + s0;  // b, c
      Store(s0, d, s0_ + i);
      s1 ^= ShiftLeft<23>(s1);
      Store(bits, d, random_bits + i);
      s1 ^= s0 ^ ShiftRight<18>(s1) ^ ShiftRight<5>(s0);
      Store(s1, d, s1_ + i);
    }
#else
    for (size_t i = 0; i < N; ++i) {
      auto s1 = s0_[i];
      const auto s0 = s1_[i];
      const auto bits = s1 + s0;  // b, c
      s0_[i] = s0;
      s1 ^= s1 << 23;
      random_bits[i] = bits;
      s1 ^= s0 ^ (s1 >> 18) ^ (s0 >> 5);
      s1_[i] = s1;
    }
#endif
  }

 private:
  static uint64_t SplitMix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

  HWY_ALIGN uint64_t s0_[N];
  HWY_ALIGN uint64_t s1_[N];
};

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace jxl
HWY_AFTER_NAMESPACE();

#endif  // LIB_JXL_XORSHIFT128PLUS_INL_H_
