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

#if HWY_HAVE_FLOAT16
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
// HWY_INLINE works around a Clang SVE compiler bug where all but the first
// 128 bits (the NEON register) of actual are zero.
template <class D, typename T = TFromD<D>, class V = Vec<D>>
HWY_INLINE void AssertVecEqual(D d, const T* expected, VecArg<V> actual,
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
// HWY_INLINE works around a Clang SVE compiler bug where all but the first
// 128 bits (the NEON register) of actual are zero.
template <class D, typename T = TFromD<D>, class V = Vec<D>>
HWY_INLINE void AssertVecEqual(D d, VecArg<V> expected, VecArg<V> actual,
                               const char* filename, int line) {
  auto expected_lanes = AllocateAligned<T>(Lanes(d));
  Store(expected, d, expected_lanes.get());
  AssertVecEqual(d, expected_lanes.get(), actual, filename, line);
}

// Only checks the valid mask elements (those whose index < Lanes(d)).
template <class D>
HWY_NOINLINE void AssertMaskEqual(D d, VecArg<Mask<D>> a, VecArg<Mask<D>> b,
                                  const char* filename, int line) {
  // lvalues prevented MSAN failure in farm_sve.
  const Vec<D> va = VecFromMask(d, a);
  const Vec<D> vb = VecFromMask(d, b);
  AssertVecEqual(d, va, vb, filename, line);

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

// Calls Test for each CappedTag<T, N> where N is in [kMinLanes, kMul * kMinArg]
// and the resulting Lanes() is in [min_lanes, max_lanes]. The upper bound
// is required to ensure capped vectors remain extendable. Implemented by
// recursively halving kMul until it is zero.
template <typename T, size_t kMul, size_t kMinArg, class Test>
struct ForeachCappedR {
  static void Do(size_t min_lanes, size_t max_lanes) {
    const CappedTag<T, kMul * kMinArg> d;

    // If we already don't have enough lanes, stop.
    const size_t lanes = Lanes(d);
    if (lanes < min_lanes) return;

    if (lanes <= max_lanes) {
      Test()(T(), d);
    }
    ForeachCappedR<T, kMul / 2, kMinArg, Test>::Do(min_lanes, max_lanes);
  }
};

// Base case to stop the recursion.
template <typename T, size_t kMinArg, class Test>
struct ForeachCappedR<T, 0, kMinArg, Test> {
  static void Do(size_t, size_t) {}
};

#if HWY_HAVE_SCALABLE

constexpr int MinVectorSize() {
#if HWY_TARGET == HWY_RVV
  // Actually 16 for the application processor profile, but the intrinsics are
  // defined as if VLEN might be only 64: there is no vuint64mf2_t.
  return 8;
#else
  return 16;
#endif
}

template <typename T>
constexpr int MinPow2() {
  // Highway follows RVV LMUL in that the smallest fraction is 1/8th (encoded
  // as kPow2 == -3). The fraction also must not result in zero lanes for the
  // smallest possible vector size.
  return HWY_MAX(-3, -static_cast<int>(CeilLog2(MinVectorSize() / sizeof(T))));
}

// Iterates kPow2 upward through +3.
template <typename T, int kPow2, int kAddPow2, class Test>
struct ForeachShiftR {
  static void Do(size_t min_lanes) {
    const ScalableTag<T, kPow2 + kAddPow2> d;

    // Precondition: [kPow2, 3] + kAddPow2 is a valid fraction of the minimum
    // vector size, so we always have enough lanes, except ForGEVectors.
    if (Lanes(d) >= min_lanes) {
      Test()(T(), d);
    } else {
      fprintf(stderr, "%d lanes < %d: T=%d pow=%d\n",
              static_cast<int>(Lanes(d)), static_cast<int>(min_lanes),
              static_cast<int>(sizeof(T)), kPow2 + kAddPow2);
      HWY_ASSERT(min_lanes != 1);
    }

    ForeachShiftR<T, kPow2 + 1, kAddPow2, Test>::Do(min_lanes);
  }
};

// Base case to stop the recursion.
template <typename T, int kAddPow2, class Test>
struct ForeachShiftR<T, 4, kAddPow2, Test> {
  static void Do(size_t) {}
};
#else
// ForeachCappedR already handled all possible sizes.
#endif  // HWY_HAVE_SCALABLE

// These adapters may be called directly, or via For*Types:

// Calls Test for all power of two N in [1, Lanes(d) >> kPow2]. This is for
// ops that widen their input, e.g. Combine (not supported by HWY_SCALAR).
template <class Test, int kPow2 = 1>
struct ForExtendableVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
    constexpr size_t kMaxCapped = HWY_LANES(T);
    // Skip CappedTag that are already full vectors.
    const size_t max_lanes = Lanes(ScalableTag<T>()) >> kPow2;
    (void)kMaxCapped;
    (void)max_lanes;
#if HWY_TARGET == HWY_SCALAR
    // not supported
#else
    ForeachCappedR<T, (kMaxCapped >> kPow2), 1, Test>::Do(1, max_lanes);
#if HWY_TARGET == HWY_RVV
    // For each [MinPow2, 3 - kPow2]; counter is [MinPow2 + kPow2, 3].
    ForeachShiftR<T, MinPow2<T>() + kPow2, -kPow2, Test>::Do(1);
#elif HWY_HAVE_SCALABLE
    // For each [MinPow2, 0 - kPow2]; counter is [MinPow2 + kPow2 + 3, 3].
    ForeachShiftR<T, MinPow2<T>() + kPow2 + 3, -kPow2 - 3, Test>::Do(1);
#endif
#endif  // HWY_SCALAR
  }
};

// Calls Test for all power of two N in [1 << kPow2, Lanes(d)]. This is for ops
// that narrow their input, e.g. UpperHalf.
template <class Test, int kPow2 = 1>
struct ForShrinkableVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
    constexpr size_t kMinLanes = size_t{1} << kPow2;
    constexpr size_t kMaxCapped = HWY_LANES(T);
    // For shrinking, an upper limit is unnecessary.
    constexpr size_t max_lanes = kMaxCapped;

    (void)kMinLanes;
    (void)max_lanes;
    (void)max_lanes;
#if HWY_TARGET == HWY_SCALAR
    // not supported
#else
    ForeachCappedR<T, (kMaxCapped >> kPow2), kMinLanes, Test>::Do(kMinLanes,
                                                                  max_lanes);
#if HWY_TARGET == HWY_RVV
    // For each [MinPow2 + kPow2, 3]; counter is [MinPow2 + kPow2, 3].
    ForeachShiftR<T, MinPow2<T>() + kPow2, 0, Test>::Do(kMinLanes);
#elif HWY_HAVE_SCALABLE
    // For each [MinPow2 + kPow2, 0]; counter is [MinPow2 + kPow2 + 3, 3].
    ForeachShiftR<T, MinPow2<T>() + kPow2 + 3, -3, Test>::Do(kMinLanes);
#endif
#endif  // HWY_TARGET == HWY_SCALAR
  }
};

// Calls Test for all supported power of two vectors of at least kMinBits.
// Examples: AES or 64x64 require 128 bits, casts may require 64 bits.
template <size_t kMinBits, class Test>
struct ForGEVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
    constexpr size_t kMaxCapped = HWY_LANES(T);
    constexpr size_t kMinLanes = kMinBits / 8 / sizeof(T);
    // An upper limit is unnecessary.
    constexpr size_t max_lanes = kMaxCapped;
    (void)max_lanes;
#if HWY_TARGET == HWY_SCALAR
    (void)kMinLanes;  // not supported
#else
    ForeachCappedR<T, HWY_LANES(T) / kMinLanes, kMinLanes, Test>::Do(kMinLanes,
                                                                     max_lanes);
#if HWY_TARGET == HWY_RVV
    // Can be 0 (handled below) if kMinBits > 64.
    constexpr size_t kRatio = MinVectorSize() * 8 / kMinBits;
    constexpr int kMinPow2 =
        kRatio == 0 ? 0 : -static_cast<int>(CeilLog2(kRatio));
    // For each [kMinPow2, 3]; counter is [kMinPow2, 3].
    ForeachShiftR<T, kMinPow2, 0, Test>::Do(kMinLanes);
#elif HWY_HAVE_SCALABLE
    // Can be 0 (handled below) if kMinBits > 128.
    constexpr size_t kRatio = MinVectorSize() * 8 / kMinBits;
    constexpr int kMinPow2 =
        kRatio == 0 ? 0 : -static_cast<int>(CeilLog2(kRatio));
    // For each [kMinPow2, 0]; counter is [kMinPow2 + 3, 3].
    ForeachShiftR<T, kMinPow2 + 3, -3, Test>::Do(kMinLanes);
#endif
#endif  // HWY_TARGET == HWY_SCALAR
  }
};

template <class Test>
using ForGE128Vectors = ForGEVectors<128, Test>;

// Calls Test for all N that can be promoted (not the same as Extendable because
// HWY_SCALAR has one lane). Also used for ZipLower, but not ZipUpper.
template <class Test, int kPow2 = 1>
struct ForPromoteVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
    constexpr size_t kFactor = size_t{1} << kPow2;
    static_assert(kFactor >= 2 && kFactor * sizeof(T) <= sizeof(uint64_t), "");
    constexpr size_t kMaxCapped = HWY_LANES(T);
    constexpr size_t kMinLanes = kFactor;
    // Skip CappedTag that are already full vectors.
    const size_t max_lanes = Lanes(ScalableTag<T>()) >> kPow2;
    (void)kMaxCapped;
    (void)kMinLanes;
    (void)max_lanes;
#if HWY_TARGET == HWY_SCALAR
    ForeachCappedR<T, 1, 1, Test>::Do(1, 1);
#else
    // TODO(janwas): call Extendable if kMinLanes check not required?
    ForeachCappedR<T, (kMaxCapped >> kPow2), 1, Test>::Do(kMinLanes, max_lanes);
#if HWY_TARGET == HWY_RVV
    // For each [MinPow2, 3 - kPow2]; counter is [MinPow2 + kPow2, 3].
    ForeachShiftR<T, MinPow2<T>() + kPow2, -kPow2, Test>::Do(kMinLanes);
#elif HWY_HAVE_SCALABLE
    // For each [MinPow2, 0 - kPow2]; counter is [MinPow2 + kPow2 + 3, 3].
    ForeachShiftR<T, MinPow2<T>() + kPow2 + 3, -kPow2 - 3, Test>::Do(kMinLanes);
#endif
#endif  // HWY_SCALAR
  }
};

// Calls Test for all N than can be demoted (not the same as Shrinkable because
// HWY_SCALAR has one lane).
template <class Test, int kPow2 = 1>
struct ForDemoteVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
    constexpr size_t kMinLanes = size_t{1} << kPow2;
    constexpr size_t kMaxCapped = HWY_LANES(T);
    // For shrinking, an upper limit is unnecessary.
    constexpr size_t max_lanes = kMaxCapped;

    (void)kMinLanes;
    (void)max_lanes;
    (void)max_lanes;
#if HWY_TARGET == HWY_SCALAR
    ForeachCappedR<T, 1, 1, Test>::Do(1, 1);
#else
    ForeachCappedR<T, (kMaxCapped >> kPow2), kMinLanes, Test>::Do(kMinLanes,
                                                                  max_lanes);

// TODO(janwas): call Extendable if kMinLanes check not required?
#if HWY_TARGET == HWY_RVV
    // For each [MinPow2 + kPow2, 3]; counter is [MinPow2 + kPow2, 3].
    ForeachShiftR<T, MinPow2<T>() + kPow2, 0, Test>::Do(kMinLanes);
#elif HWY_HAVE_SCALABLE
    // For each [MinPow2 + kPow2, 0]; counter is [MinPow2 + kPow2 + 3, 3].
    ForeachShiftR<T, MinPow2<T>() + kPow2 + 3, -3, Test>::Do(kMinLanes);
#endif
#endif  // HWY_TARGET == HWY_SCALAR
  }
};

// For LowerHalf/Quarter.
template <class Test, int kPow2 = 1>
struct ForHalfVectors {
  template <typename T>
  void operator()(T /*unused*/) const {
    constexpr size_t kMinLanes = size_t{1} << kPow2;
    constexpr size_t kMaxCapped = HWY_LANES(T);
    // For shrinking, an upper limit is unnecessary.
    constexpr size_t max_lanes = kMaxCapped;

    (void)kMinLanes;
    (void)max_lanes;
    (void)max_lanes;
#if HWY_TARGET == HWY_SCALAR
    ForeachCappedR<T, 1, 1, Test>::Do(1, 1);
#else
//    ForeachCappedR<T, (kMaxCapped >> kPow2), kMinLanes, Test>::Do(kMinLanes,
//                                                                  max_lanes);

// TODO(janwas): call Extendable if kMinLanes check not required?
#if HWY_TARGET == HWY_RVV
    // For each [MinPow2 + kPow2, 3]; counter is [MinPow2 + kPow2, 3].
    ForeachShiftR<T, MinPow2<T>() + kPow2, 0, Test>::Do(kMinLanes);
#elif HWY_HAVE_SCALABLE
    // For each [MinPow2 + kPow2, 0]; counter is [MinPow2 + kPow2 + 3, 3].
    ForeachShiftR<T, MinPow2<T>() + kPow2 + 3, -3, Test>::Do(kMinLanes);
#endif
#endif  // HWY_TARGET == HWY_SCALAR
  }
};

// Calls Test for all power of two N in [1, Lanes(d)]. This is the default
// for ops that do not narrow nor widen their input, nor require 128 bits.
template <class Test>
struct ForPartialVectors {
  template <typename T>
  void operator()(T t) const {
    ForExtendableVectors<Test, 0>()(t);
  }
};

// Type lists to shorten call sites:

template <class Func>
void ForSignedTypes(const Func& func) {
  func(int8_t());
  func(int16_t());
  func(int32_t());
#if HWY_HAVE_INTEGER64
  func(int64_t());
#endif
}

template <class Func>
void ForUnsignedTypes(const Func& func) {
  func(uint8_t());
  func(uint16_t());
  func(uint32_t());
#if HWY_HAVE_INTEGER64
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
#if HWY_HAVE_FLOAT64
  func(double());
#endif
}

template <class Func>
void ForAllTypes(const Func& func) {
  ForIntegerTypes(func);
  ForFloatTypes(func);
}

template <class Func>
void ForUIF16(const Func& func) {
  func(uint16_t());
  func(int16_t());
#if HWY_HAVE_FLOAT16
  func(float16_t());
#endif
}

template <class Func>
void ForUIF32(const Func& func) {
  func(uint32_t());
  func(int32_t());
  func(float());
}

template <class Func>
void ForUIF64(const Func& func) {
#if HWY_HAVE_INTEGER64
  func(uint64_t());
  func(int64_t());
#endif
#if HWY_HAVE_FLOAT64
  func(double());
#endif
}

template <class Func>
void ForUIF3264(const Func& func) {
  ForUIF32(func);
  ForUIF64(func);
}

template <class Func>
void ForUIF163264(const Func& func) {
  ForUIF16(func);
  ForUIF3264(func);
}

// For tests that involve loops, adjust the trip count so that emulated tests
// finish quickly (but always at least 2 iterations to ensure some diversity).
constexpr size_t AdjustedReps(size_t max_reps) {
#if HWY_ARCH_RVV
  return HWY_MAX(max_reps / 32, 2);
#elif HWY_ARCH_ARM
  return HWY_MAX(max_reps / 4, 2);
#elif HWY_IS_DEBUG_BUILD
  return HWY_MAX(max_reps / 8, 2);
#else
  return HWY_MAX(max_reps, 2);
#endif
}

// Same as above, but the loop trip count will be 1 << max_pow2.
constexpr size_t AdjustedLog2Reps(size_t max_pow2) {
  // If "negative" (unsigned wraparound), use original.
#if HWY_ARCH_RVV
  return HWY_MIN(max_pow2 - 4, max_pow2);
#elif HWY_ARCH_ARM
  return HWY_MIN(max_pow2 - 1, max_pow2);
#elif HWY_IS_DEBUG_BUILD
  return HWY_MIN(max_pow2 - 1, max_pow2);
#else
  return max_pow2;
#endif
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // per-target include guard
