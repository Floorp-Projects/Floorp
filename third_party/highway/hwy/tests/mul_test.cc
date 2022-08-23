// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0
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

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/mul_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestUnsignedMul {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    const auto v1 = Set(d, T(1));
    const auto vi = Iota(d, 1);
    const auto vj = Iota(d, 3);
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);

    HWY_ASSERT_VEC_EQ(d, v0, Mul(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v1, Mul(v1, v1));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(v1, vi));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(vi, v1));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((1 + i) * (1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vi));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((1 + i) * (3 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vj));

    const T max = LimitsMax<T>();
    const auto vmax = Set(d, max);
    HWY_ASSERT_VEC_EQ(d, vmax, Mul(vmax, v1));
    HWY_ASSERT_VEC_EQ(d, vmax, Mul(v1, vmax));

    const size_t bits = sizeof(T) * 8;
    const uint64_t mask = (1ull << bits) - 1;
    const T max2 = (static_cast<uint64_t>(max) * max) & mask;
    HWY_ASSERT_VEC_EQ(d, Set(d, max2), Mul(vmax, vmax));
  }
};

struct TestSignedMul {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);

    const auto v0 = Zero(d);
    const auto v1 = Set(d, T(1));
    const auto vi = Iota(d, 1);
    const auto vn = Iota(d, -T(N));  // no i8 supported, so no wraparound
    HWY_ASSERT_VEC_EQ(d, v0, Mul(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v1, Mul(v1, v1));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(v1, vi));
    HWY_ASSERT_VEC_EQ(d, vi, Mul(vi, v1));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((1 + i) * (1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vi));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((-T(N) + T(i)) * T(1u + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vn, vi));
    HWY_ASSERT_VEC_EQ(d, expected.get(), Mul(vi, vn));
  }
};

HWY_NOINLINE void TestAllMul() {
  const ForPartialVectors<TestUnsignedMul> test_unsigned;
  // No u8.
  test_unsigned(uint16_t());
  test_unsigned(uint32_t());
  // No u64.

  const ForPartialVectors<TestSignedMul> test_signed;
  // No i8.
  test_signed(int16_t());
  test_signed(int32_t());
  // No i64.
}

struct TestMulHigh {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using Wide = MakeWide<T>;
    const size_t N = Lanes(d);
    auto in_lanes = AllocateAligned<T>(N);
    auto expected_lanes = AllocateAligned<T>(N);

    const auto vi = Iota(d, 1);
    // no i8 supported, so no wraparound
    const auto vni = Iota(d, T(static_cast<T>(~N + 1)));

    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d, v0, MulHigh(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v0, MulHigh(v0, vi));
    HWY_ASSERT_VEC_EQ(d, v0, MulHigh(vi, v0));

    // Large positive squared
    for (size_t i = 0; i < N; ++i) {
      in_lanes[i] = T(LimitsMax<T>() >> i);
      expected_lanes[i] = T((Wide(in_lanes[i]) * in_lanes[i]) >> 16);
    }
    auto v = Load(d, in_lanes.get());
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(v, v));

    // Large positive * small positive
    for (size_t i = 0; i < N; ++i) {
      expected_lanes[i] = T((Wide(in_lanes[i]) * T(1u + i)) >> 16);
    }
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(v, vi));
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(vi, v));

    // Large positive * small negative
    for (size_t i = 0; i < N; ++i) {
      expected_lanes[i] = T((Wide(in_lanes[i]) * T(i - N)) >> 16);
    }
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(v, vni));
    HWY_ASSERT_VEC_EQ(d, expected_lanes.get(), MulHigh(vni, v));
  }
};

HWY_NOINLINE void TestAllMulHigh() {
  ForPartialVectors<TestMulHigh> test;
  test(int16_t());
  test(uint16_t());
}

struct TestMulFixedPoint15 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d, v0, MulFixedPoint15(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v0, MulFixedPoint15(v0, v0));

    const size_t N = Lanes(d);
    auto in1 = AllocateAligned<T>(N);
    auto in2 = AllocateAligned<T>(N);
    auto expected = AllocateAligned<T>(N);

    // Random inputs in each lane
    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(10000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        in1[i] = static_cast<T>(Random64(&rng) & 0xFFFF);
        in2[i] = static_cast<T>(Random64(&rng) & 0xFFFF);
      }

      for (size_t i = 0; i < N; ++i) {
        // There are three ways to compute the results. x86 and ARM are defined
        // using 32-bit multiplication results:
        const int arm = (2 * in1[i] * in2[i] + 0x8000) >> 16;
        const int x86 = (((in1[i] * in2[i]) >> 14) + 1) >> 1;
        // On other platforms, split the result into upper and lower 16 bits.
        const auto v1 = Set(d, in1[i]);
        const auto v2 = Set(d, in2[i]);
        const int hi = GetLane(MulHigh(v1, v2));
        const int lo = GetLane(Mul(v1, v2)) & 0xFFFF;
        const int split = 2 * hi + ((lo + 0x4000) >> 15);
        expected[i] = static_cast<T>(arm);
        if (in1[i] != -32768 || in2[i] != -32768) {
          HWY_ASSERT_EQ(arm, x86);
          HWY_ASSERT_EQ(arm, split);
        }
      }

      const auto a = Load(d, in1.get());
      const auto b = Load(d, in2.get());
      HWY_ASSERT_VEC_EQ(d, expected.get(), MulFixedPoint15(a, b));
    }
  }
};

HWY_NOINLINE void TestAllMulFixedPoint15() {
  ForPartialVectors<TestMulFixedPoint15>()(int16_t());
}

struct TestMulEven {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using Wide = MakeWide<T>;
    const Repartition<Wide, D> d2;
    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d2, Zero(d2), MulEven(v0, v0));

    const size_t N = Lanes(d);
    auto in_lanes = AllocateAligned<T>(N);
    auto expected = AllocateAligned<Wide>(Lanes(d2));
    for (size_t i = 0; i < N; i += 2) {
      in_lanes[i + 0] = LimitsMax<T>() >> i;
      if (N != 1) {
        in_lanes[i + 1] = 1;  // unused
      }
      expected[i / 2] = Wide(in_lanes[i + 0]) * in_lanes[i + 0];
    }

    const auto v = Load(d, in_lanes.get());
    HWY_ASSERT_VEC_EQ(d2, expected.get(), MulEven(v, v));
  }
};

struct TestMulEvenOdd64 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const auto v0 = Zero(d);
    HWY_ASSERT_VEC_EQ(d, Zero(d), MulEven(v0, v0));
    HWY_ASSERT_VEC_EQ(d, Zero(d), MulOdd(v0, v0));

    const size_t N = Lanes(d);
    if (N == 1) return;

    auto in1 = AllocateAligned<T>(N);
    auto in2 = AllocateAligned<T>(N);
    auto expected_even = AllocateAligned<T>(N);
    auto expected_odd = AllocateAligned<T>(N);

    // Random inputs in each lane
    RandomState rng;
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        in1[i] = Random64(&rng);
        in2[i] = Random64(&rng);
      }

      for (size_t i = 0; i < N; i += 2) {
        expected_even[i] = Mul128(in1[i], in2[i], &expected_even[i + 1]);
        expected_odd[i] = Mul128(in1[i + 1], in2[i + 1], &expected_odd[i + 1]);
      }

      const auto a = Load(d, in1.get());
      const auto b = Load(d, in2.get());
      HWY_ASSERT_VEC_EQ(d, expected_even.get(), MulEven(a, b));
      HWY_ASSERT_VEC_EQ(d, expected_odd.get(), MulOdd(a, b));
    }
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllMulEven() {
  ForGEVectors<64, TestMulEven> test;
  test(int32_t());
  test(uint32_t());

  ForGEVectors<128, TestMulEvenOdd64>()(uint64_t());
}

#ifndef HWY_NATIVE_FMA
#error "Bug in set_macros-inl.h, did not set HWY_NATIVE_FMA"
#endif

struct TestMulAdd {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto k0 = Zero(d);
    const auto kNeg0 = Set(d, T(-0.0));
    const auto v1 = Iota(d, 1);
    const auto v2 = Iota(d, 2);
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT_VEC_EQ(d, k0, MulAdd(k0, k0, k0));
    HWY_ASSERT_VEC_EQ(d, v2, MulAdd(k0, v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, MulAdd(v1, k0, v2));
    HWY_ASSERT_VEC_EQ(d, k0, NegMulAdd(k0, k0, k0));
    HWY_ASSERT_VEC_EQ(d, v2, NegMulAdd(k0, v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, NegMulAdd(v1, k0, v2));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 1) * (i + 2));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulAdd(v2, v1, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulAdd(v1, v2, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(Neg(v2), v1, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(v1, Neg(v2), k0));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 2) * (i + 2) + (i + 1));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulAdd(v2, v2, v1));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(Neg(v2), v2, v1));

    for (size_t i = 0; i < N; ++i) {
      expected[i] =
          T(-T(i + 2u) * static_cast<T>(i + 2) + static_cast<T>(1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulAdd(v2, v2, v1));

    HWY_ASSERT_VEC_EQ(d, k0, MulSub(k0, k0, k0));
    HWY_ASSERT_VEC_EQ(d, kNeg0, NegMulSub(k0, k0, k0));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = -T(i + 2);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(k0, v1, v2));
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v1, k0, v2));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(Neg(k0), v1, v2));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(v1, Neg(k0), v2));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 1) * (i + 2));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v1, v2, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v2, v1, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(Neg(v1), v2, k0));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(v2, Neg(v1), k0));

    for (size_t i = 0; i < N; ++i) {
      expected[i] = static_cast<T>((i + 2) * (i + 2) - (1 + i));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulSub(v2, v2, v1));
    HWY_ASSERT_VEC_EQ(d, expected.get(), NegMulSub(Neg(v2), v2, v1));
  }
};

HWY_NOINLINE void TestAllMulAdd() {
  ForFloatTypes(ForPartialVectors<TestMulAdd>());
}

struct TestReorderWidenMulAccumulate {
  template <typename TN, class DN>
  HWY_NOINLINE void operator()(TN /*unused*/, DN dn) {
    using TW = MakeWide<TN>;
    const RepartitionToWide<DN> dw;
    const auto f0 = Zero(dw);
    const auto f1 = Set(dw, 1.0f);
    const auto fi = Iota(dw, 1);
    const auto bf0 = ReorderDemote2To(dn, f0, f0);
    const auto bf1 = ReorderDemote2To(dn, f1, f1);
    const auto bfi = ReorderDemote2To(dn, fi, fi);
    const size_t NW = Lanes(dw);
    auto delta = AllocateAligned<TW>(2 * NW);
    for (size_t i = 0; i < 2 * NW; ++i) {
      delta[i] = 0.0f;
    }

    // Any input zero => both outputs zero
    auto sum1 = f0;
    HWY_ASSERT_VEC_EQ(dw, f0,
                      ReorderWidenMulAccumulate(dw, bf0, bf0, f0, sum1));
    HWY_ASSERT_VEC_EQ(dw, f0, sum1);
    HWY_ASSERT_VEC_EQ(dw, f0,
                      ReorderWidenMulAccumulate(dw, bf0, bfi, f0, sum1));
    HWY_ASSERT_VEC_EQ(dw, f0, sum1);
    HWY_ASSERT_VEC_EQ(dw, f0,
                      ReorderWidenMulAccumulate(dw, bfi, bf0, f0, sum1));
    HWY_ASSERT_VEC_EQ(dw, f0, sum1);

    // delta[p] := 1.0, all others zero. For each p: Dot(delta, all-ones) == 1.
    for (size_t p = 0; p < 2 * NW; ++p) {
      delta[p] = 1.0f;
      const auto delta0 = Load(dw, delta.get() + 0);
      const auto delta1 = Load(dw, delta.get() + NW);
      delta[p] = 0.0f;
      const auto bf_delta = ReorderDemote2To(dn, delta0, delta1);

      {
        sum1 = f0;
        const auto sum0 =
            ReorderWidenMulAccumulate(dw, bf_delta, bf1, f0, sum1);
        HWY_ASSERT_EQ(1.0f, GetLane(SumOfLanes(dw, Add(sum0, sum1))));
      }
      // Swapped arg order
      {
        sum1 = f0;
        const auto sum0 =
            ReorderWidenMulAccumulate(dw, bf1, bf_delta, f0, sum1);
        HWY_ASSERT_EQ(1.0f, GetLane(SumOfLanes(dw, Add(sum0, sum1))));
      }
      // Start with nonzero sum0 or sum1
      {
        sum1 = delta1;
        const auto sum0 =
            ReorderWidenMulAccumulate(dw, bf_delta, bf1, delta0, sum1);
        HWY_ASSERT_EQ(2.0f, GetLane(SumOfLanes(dw, Add(sum0, sum1))));
      }
      // Start with nonzero sum0 or sum1, and swap arg order
      {
        sum1 = delta1;
        const auto sum0 =
            ReorderWidenMulAccumulate(dw, bf1, bf_delta, delta0, sum1);
        HWY_ASSERT_EQ(2.0f, GetLane(SumOfLanes(dw, Add(sum0, sum1))));
      }
    }
  }
};

HWY_NOINLINE void TestAllReorderWidenMulAccumulate() {
  ForShrinkableVectors<TestReorderWidenMulAccumulate>()(bfloat16_t());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyMulTest);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllMul);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllMulHigh);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllMulFixedPoint15);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllMulEven);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllMulAdd);
HWY_EXPORT_AND_TEST_P(HwyMulTest, TestAllReorderWidenMulAccumulate);
}  // namespace hwy

#endif
