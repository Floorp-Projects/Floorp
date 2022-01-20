// Copyright 2019 Google LLC
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

// Target-specific helper functions for use by *_test.cc.

#include <inttypes.h>
#include <stdint.h>

#include "hwy/base.h"
#include "hwy/tests/hwy_gtest.h"
#include "hwy/tests/test_util.h"

// Per-target include guard
#if defined(HIGHWAY_HWY_TESTS_TEST_UTIL_INL_H_) == \
    defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_TESTS_TEST_UTIL_INL_H_
#undef HIGHWAY_HWY_TESTS_TEST_UTIL_INL_H_
#else
#define HIGHWAY_HWY_TESTS_TEST_UTIL_INL_H_
#endif

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_NOINLINE void PrintValue(T value) {
  uint8_t byte;
  CopyBytes<1>(&value, &byte);  // endian-safe: we ensured sizeof(T)=1.
  fprintf(stderr, "0x%02X,", byte);
}

#if HWY_CAP_FLOAT16
HWY_NOINLINE void PrintValue(float16_t value) {
  uint16_t bits;
  CopyBytes<2>(&value, &bits);
  fprintf(stderr, "0x%02X,", bits);
}
#endif



template <typename T, HWY_IF_NOT_LANE_SIZE(T, 1)>
HWY_NOINLINE void PrintValue(T value) {
  fprintf(stderr, "%g,", double(value));
}

// Prints lanes around `lane`, in memory order.
template <class D, class V = Vec<D>>
void Print(const D d, const char* caption, VecArg<V> v, size_t lane_u = 0,
           size_t max_lanes = 7) {
  using T = TFromD<D>;
  const size_t N = Lanes(d);
  auto lanes = AllocateAligned<T>(N);
  Store(v, d, lanes.get());

  const auto info = hwy::detail::MakeTypeInfo<T>();
  hwy::detail::PrintArray(info, caption, lanes.get(), N, lane_u, max_lanes);
}

// Compare expected vector to vector.
template <class D, typename T = TFromD<D>, class V = Vec<D>>
void AssertVecEqual(D d, const T* expected, VecArg<V> actual,
                    const char* filename, const int line) {
  const size_t N = Lanes(d);
  auto actual_lanes = AllocateAligned<T>(N);
  Store(actual, d, actual_lanes.get());

  const auto info = hwy::detail::MakeTypeInfo<T>();
  const char* target_name = hwy::TargetName(HWY_TARGET);
  hwy::detail::AssertArrayEqual(info, expected, actual_lanes.get(), N,
                                target_name, filename, line);
}

// Compare expected lanes to vector.
template <class D, typename T = TFromD<D>, class V = Vec<D>>
HWY_NOINLINE void AssertVecEqual(D d, VecArg<V> expected, VecArg<V> actual,
                                 const char* filename, int line) {
  auto expected_lanes = AllocateAligned<T>(Lanes(d));
  Store(expected, d, expected_lanes.get());
  AssertVecEqual(d, expected_lanes.get(), actual, filename, line);
}

// Only checks the valid mask elements (those whose index < Lanes(d)).
template <class D>
HWY_NOINLINE void AssertMaskEqual(D d, VecArg<Mask<D>> a, VecArg<Mask<D>> b,
                                  const char* filename, int line) {
  AssertVecEqual(d, VecFromMask(d, a), VecFromMask(d, b), filename, line);

  const char* target_name = hwy::TargetName(HWY_TARGET);
  AssertEqual(CountTrue(d, a), CountTrue(d, b), target_name, filename, line);
  AssertEqual(AllTrue(d, a), AllTrue(d, b), target_name, filename, line);
  AssertEqual(AllFalse(d, a), AllFalse(d, b), target_name, filename, line);

  // TODO(janwas): remove RVV once implemented (cast or vse1)
#if HWY_TARGET != HWY_RVV && HWY_TARGET != HWY_SCALAR
  const size_t N = Lanes(d);
  const Repartition<uint8_t, D> d8;
  const size_t N8 = Lanes(d8);
  auto bits_a = AllocateAligned<uint8_t>(HWY_MAX(8, N8));
  auto bits_b = AllocateAligned<uint8_t>(HWY_MAX(8, N8));
  memset(bits_a.get(), 0, N8);
  memset(bits_b.get(), 0, N8);
  const size_t num_bytes_a = StoreMaskBits(d, a, bits_a.get());
  const size_t num_bytes_b = StoreMaskBits(d, b, bits_b.get());
  AssertEqual(num_bytes_a, num_bytes_b, target_name, filename, line);
  size_t i = 0;
  // First check whole bytes (if that many elements are still valid)
  for (; i < N / 8; ++i) {
    if (bits_a[i] != bits_b[i]) {
      fprintf(stderr, "Mismatch in byte %" PRIu64 ": %d != %d\n",
              static_cast<uint64_t>(i), bits_a[i], bits_b[i]);
      Print(d8, "expect", Load(d8, bits_a.get()), 0, N8);
      Print(d8, "actual", Load(d8, bits_b.get()), 0, N8);
      hwy::Abort(filename, line, "Masks not equal");
    }
  }
  // Then the valid bit(s) in the last byte.
  const size_t remainder = N % 8;
  if (remainder != 0) {
    const int mask = (1 << remainder) - 1;
    const int valid_a = bits_a[i] & mask;
    const int valid_b = bits_b[i] & mask;
    if (valid_a != valid_b) {
      fprintf(stderr, "Mismatch in last byte %" PRIu64 ": %d != %d\n",
              static_cast<uint64_t>(i), valid_a, valid_b);
      Print(d8, "expect", Load(d8, bits_a.get()), 0, N8);
      Print(d8, "actual", Load(d8, bits_b.get()), 0, N8);
      hwy::Abort(filename, line, "Masks not equal");
    }
  }
#endif
}

// Only sets valid elements (those whose index < Lanes(d)). This helps catch
// tests that are not masking off the (undefined) upper mask elements.
//
// TODO(janwas): with HWY_NOINLINE GCC zeros the upper half of AVX2 masks.
template <class D>
HWY_INLINE Mask<D> MaskTrue(const D d) {
  return FirstN(d, Lanes(d));
}

template <class D>
HWY_INLINE Mask<D> MaskFalse(const D d) {
  const auto zero = Zero(RebindToSigned<D>());
  return RebindMask(d, Lt(zero, zero));
}

#ifndef HWY_ASSERT_EQ

#define HWY_ASSERT_EQ(expected, actual)                                     \
  hwy::AssertEqual(expected, actual, hwy::TargetName(HWY_TARGET), __FILE__, \
                   __LINE__)

#define HWY_ASSERT_STRING_EQ(expected, actual)                          \
  hwy::AssertStringEqual(expected, actual, hwy::TargetName(HWY_TARGET), \
                         __FILE__, __LINE__)

#define HWY_ASSERT_VEC_EQ(d, expected, actual) \
  AssertVecEqual(d, expected, actual, __FILE__, __LINE__)

#define HWY_ASSERT_MASK_EQ(d, expected, actual) \
  AssertMaskEqual(d, expected, actual, __FILE__, __LINE__)

#endif  // HWY_ASSERT_EQ

// Helpers for instantiating tests with combinations of lane types / counts.

// For ensuring we do not call tests with D such that widening D results in 0
// lanes. Example: assume T=u32, VLEN=256, and fraction=1/8: there is no 1/8th
// of a u64 vector in this case.
template <class D, HWY_IF_NOT_LANE_SIZE_D(D, 8)>
HWY_INLINE size_t PromotedLanes(const D d) {
  return Lanes(RepartitionToWide<decltype(d)>());
}
// Already the widest possible T, cannot widen.
template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_INLINE size_t PromotedLanes(const D d) {
  return Lanes(d);
}

// For all power of two N in [kMinLanes, kMul * kMinLanes] (so that recursion
// stops at kMul == 0). Note that N may be capped or a fraction.
template <typename T, size_t kMul, size_t kMinLanes, class Test,
          bool kPromote = false>
struct ForeachSizeR {
  static void Do() {
    const Simd<T, kMul * kMinLanes> d;

    // Skip invalid fractions (e.g. 1/8th of u32x4).
    const size_t lanes = kPromote ? PromotedLanes(d) : Lanes(d);
    if (lanes < kMinLanes) return;

    Test()(T(), d);

    static_assert(kMul != 0, "Recursion should have ended already");
    ForeachSizeR<T, kMul / 2, kMinLanes, Test, kPromote>::Do();
  }
};

// Base case to stop the recursion.
template <typename T, size_t kMinLanes, class Test, bool kPromote>
struct ForeachSizeR<T, 0, kMinLanes, Test, kPromote> {
  static void Do() {}
};

// These adapters may be called directly, or via For*Types:

// Calls Test for all power of two N in [1, Lanes(d) / kFactor]. This is for
// ops that widen their input, e.g. Combine (not supported by HWY_SCALAR).
template <class Test, size_t kFactor = 2>
struct ForExtendableVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    // not supported
#else
    constexpr bool kPromote = true;
#if HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 8 / kFactor, HWY_LANES(T), Test, kPromote>::Do();
    // TODO(janwas): also capped
    // ForeachSizeR<T, (16 / sizeof(T)) / kFactor, 1, Test, kPromote>::Do();
#elif HWY_TARGET == HWY_SVE || HWY_TARGET == HWY_SVE2
    // Capped
    ForeachSizeR<T, (16 / sizeof(T)) / kFactor, 1, Test, kPromote>::Do();
    // Fractions
    ForeachSizeR<T, 8 / kFactor, HWY_LANES(T) / 8, Test, kPromote>::Do();
#else
    ForeachSizeR<T, HWY_LANES(T) / kFactor, 1, Test, kPromote>::Do();
#endif
#endif  // HWY_SCALAR
  }
};

// Calls Test for all power of two N in [kFactor, Lanes(d)]. This is for ops
// that narrow their input, e.g. UpperHalf.
template <class Test, size_t kFactor = 2>
struct ForShrinkableVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    // not supported
#elif HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 8 / kFactor, kFactor * HWY_LANES(T), Test>::Do();
    // TODO(janwas): also capped
#elif HWY_TARGET == HWY_SVE || HWY_TARGET == HWY_SVE2
    // Capped
    ForeachSizeR<T, (16 / sizeof(T)) / kFactor, kFactor, Test>::Do();
    // Fractions
    ForeachSizeR<T, 8 / kFactor, kFactor * HWY_LANES(T) / 8, Test>::Do();
#elif HWY_TARGET == HWY_SCALAR
    // not supported
#else
    ForeachSizeR<T, HWY_LANES(T) / kFactor, kFactor, Test>::Do();
#endif
  }
};

// Calls Test for all power of two N in [16 / sizeof(T), Lanes(d)]. This is for
// ops that require at least 128 bits, e.g. AES or 64x64 = 128 mul.
template <class Test>
struct ForGE128Vectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    // not supported
#elif HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 8, HWY_LANES(T), Test>::Do();
    // TODO(janwas): also capped
    // ForeachSizeR<T, 1, (16 / sizeof(T)), Test>::Do();
#elif HWY_TARGET == HWY_SVE || HWY_TARGET == HWY_SVE2
    // Capped
    ForeachSizeR<T, 1, 16 / sizeof(T), Test>::Do();
    // Fractions
    ForeachSizeR<T, 8, HWY_LANES(T) / 8, Test>::Do();
#else
    ForeachSizeR<T, HWY_LANES(T) / (16 / sizeof(T)), (16 / sizeof(T)),
                 Test>::Do();
#endif
  }
};

// Calls Test for all power of two N in [8 / sizeof(T), Lanes(d)]. This is for
// ops that require at least 64 bits, e.g. casts.
template <class Test>
struct ForGE64Vectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    // not supported
#elif HWY_TARGET == HWY_RVV
    ForeachSizeR<T, 8, HWY_LANES(T), Test>::Do();
    // TODO(janwas): also capped
    // ForeachSizeR<T, 1, (8 / sizeof(T)), Test>::Do();
#elif HWY_TARGET == HWY_SVE || HWY_TARGET == HWY_SVE2
    // Capped
    ForeachSizeR<T, 1, 8 / sizeof(T), Test>::Do();
    // Fractions
    ForeachSizeR<T, 8, HWY_LANES(T) / 8, Test>::Do();
#else
    ForeachSizeR<T, HWY_LANES(T) / (8 / sizeof(T)), (8 / sizeof(T)),
                 Test>::Do();
#endif
  }
};

// Calls Test for all N that can be promoted (not the same as Extendable because
// HWY_SCALAR has one lane). Also used for ZipLower, but not ZipUpper.
template <class Test, size_t kFactor = 2>
struct ForPromoteVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    ForeachSizeR<T, 1, 1, Test, /*kPromote=*/true>::Do();
#else
    return ForExtendableVectors<Test, kFactor>()(T());
#endif
  }
};

// Calls Test for all N than can be demoted (not the same as Shrinkable because
// HWY_SCALAR has one lane). Also used for LowerHalf, but not UpperHalf.
template <class Test, size_t kFactor = 2>
struct ForDemoteVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
#if HWY_TARGET == HWY_SCALAR
    ForeachSizeR<T, 1, 1, Test>::Do();
#else
    return ForShrinkableVectors<Test, kFactor>()(T());
#endif
  }
};

// Calls Test for all power of two N in [1, Lanes(d)]. This is the default
// for ops that do not narrow nor widen their input, nor require 128 bits.
template <class Test>
struct ForPartialVectors {
  template <typename T>
  void operator()(T t) const {
    ForExtendableVectors<Test, 1>()(t);
  }
};

// Type lists to shorten call sites:

template <class Func>
void ForSignedTypes(const Func& func) {
  func(int8_t());
  func(int16_t());
  func(int32_t());
#if HWY_CAP_INTEGER64
  func(int64_t());
#endif
}

template <class Func>
void ForUnsignedTypes(const Func& func) {
  func(uint8_t());
  func(uint16_t());
  func(uint32_t());
#if HWY_CAP_INTEGER64
  func(uint64_t());
#endif
}

template <class Func>
void ForIntegerTypes(const Func& func) {
  ForSignedTypes(func);
  ForUnsignedTypes(func);
}

template <class Func>
void ForFloatTypes(const Func& func) {
  func(float());
#if HWY_CAP_FLOAT64
  func(double());
#endif
}

template <class Func>
void ForAllTypes(const Func& func) {
  ForIntegerTypes(func);
  ForFloatTypes(func);
}

template <class Func>
void ForUIF3264(const Func& func) {
  func(uint32_t());
  func(int32_t());
#if HWY_CAP_INTEGER64
  func(uint64_t());
  func(int64_t());
#endif

  ForFloatTypes(func);
}

template <class Func>
void ForUIF163264(const Func& func) {
  ForUIF3264(func);
  func(uint16_t());
  func(int16_t());
#if HWY_CAP_FLOAT16
  func(float16_t());
#endif
}

// For tests that involve loops, adjust the trip count so that emulated tests
// finish quickly (but always at least 2 iterations to ensure some diversity).
constexpr size_t AdjustedReps(size_t max_reps) {
#if HWY_ARCH_RVV
  return HWY_MAX(max_reps / 16, 2);
#elif HWY_ARCH_ARM
  return HWY_MAX(max_reps / 4, 2);
#elif HWY_IS_DEBUG_BUILD
  return HWY_MAX(max_reps / 8, 2);
#else
  return HWY_MAX(max_reps, 2);
#endif
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // per-target include guard
