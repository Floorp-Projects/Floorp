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

#include <stddef.h>
#include <stdint.h>
#include <string.h>  // memset

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/compare_test.cc"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// All types.
struct TestEquality {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v2 = Iota(d, 2);
    const auto v2b = Iota(d, 2);
    const auto v3 = Iota(d, 3);

    const auto mask_false = MaskFalse(d);
    const auto mask_true = MaskTrue(d);

    HWY_ASSERT_MASK_EQ(d, mask_false, Eq(v2, v3));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq(v3, v2));
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq(v2, v2));
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq(v2, v2b));

    HWY_ASSERT_MASK_EQ(d, mask_true, Ne(v2, v3));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne(v3, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne(v2, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne(v2, v2b));
  }
};

HWY_NOINLINE void TestAllEquality() {
  ForAllTypes(ForPartialVectors<TestEquality>());
}

// a > b should be true, verify that for Gt/Lt and with swapped args.
template <class D>
void EnsureGreater(D d, TFromD<D> a, TFromD<D> b, const char* file, int line) {
  const auto mask_false = MaskFalse(d);
  const auto mask_true = MaskTrue(d);

  const auto va = Set(d, a);
  const auto vb = Set(d, b);
  AssertMaskEqual(d, mask_true, Gt(va, vb), file, line);
  AssertMaskEqual(d, mask_false, Lt(va, vb), file, line);

  // Swapped order
  AssertMaskEqual(d, mask_false, Gt(vb, va), file, line);
  AssertMaskEqual(d, mask_true, Lt(vb, va), file, line);

  // Also ensure irreflexive
  AssertMaskEqual(d, mask_false, Gt(va, va), file, line);
  AssertMaskEqual(d, mask_false, Gt(vb, vb), file, line);
  AssertMaskEqual(d, mask_false, Lt(va, va), file, line);
  AssertMaskEqual(d, mask_false, Lt(vb, vb), file, line);
}

#define HWY_ENSURE_GREATER(d, a, b) EnsureGreater(d, a, b, __FILE__, __LINE__)

struct TestStrictUnsigned {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const T max = LimitsMax<T>();
    const auto v0 = Zero(d);
    const auto v2 = And(Iota(d, T(2)), Set(d, 255));  // 0..255

    const auto mask_false = MaskFalse(d);

    // Individual values of interest
    HWY_ENSURE_GREATER(d, 2, 1);
    HWY_ENSURE_GREATER(d, 1, 0);
    HWY_ENSURE_GREATER(d, 128, 127);
    HWY_ENSURE_GREATER(d, max, max / 2);
    HWY_ENSURE_GREATER(d, max, 1);
    HWY_ENSURE_GREATER(d, max, 0);

    // Also use Iota to ensure lanes are independent
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(v2, v0));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(v0, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(v0, v0));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(v0, v0));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(v2, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(v2, v2));
  }
};

HWY_NOINLINE void TestAllStrictUnsigned() {
  ForUnsignedTypes(ForPartialVectors<TestStrictUnsigned>());
}

struct TestStrictInt {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const T min = LimitsMin<T>();
    const T max = LimitsMax<T>();
    const auto v0 = Zero(d);
    const auto v2 = And(Iota(d, T(2)), Set(d, 127));  // 0..127
    const auto vn = Sub(Neg(v2), Set(d, 1));          // -1..-128

    const auto mask_false = MaskFalse(d);
    const auto mask_true = MaskTrue(d);

    // Individual values of interest
    HWY_ENSURE_GREATER(d, 2, 1);
    HWY_ENSURE_GREATER(d, 1, 0);
    HWY_ENSURE_GREATER(d, 0, -1);
    HWY_ENSURE_GREATER(d, -1, -2);
    HWY_ENSURE_GREATER(d, max, max / 2);
    HWY_ENSURE_GREATER(d, max, 1);
    HWY_ENSURE_GREATER(d, max, 0);
    HWY_ENSURE_GREATER(d, max, -1);
    HWY_ENSURE_GREATER(d, max, min);
    HWY_ENSURE_GREATER(d, 0, min);
    HWY_ENSURE_GREATER(d, min / 2, min);

    // Also use Iota to ensure lanes are independent
    HWY_ASSERT_MASK_EQ(d, mask_true, Gt(v2, vn));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt(vn, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(v2, vn));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(vn, v2));

    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(v0, v0));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(v2, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(vn, vn));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(v0, v0));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(v2, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(vn, vn));
  }
};

HWY_NOINLINE void TestAllStrictInt() {
  ForSignedTypes(ForPartialVectors<TestStrictInt>());
}

struct TestStrictFloat {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const T huge_neg = T(-1E35);
    const T huge_pos = T(1E36);
    const auto v0 = Zero(d);
    const auto v2 = Iota(d, T(2));
    const auto vn = Neg(v2);

    const auto mask_false = MaskFalse(d);
    const auto mask_true = MaskTrue(d);

    // Individual values of interest
    HWY_ENSURE_GREATER(d, 2, 1);
    HWY_ENSURE_GREATER(d, 1, 0);
    HWY_ENSURE_GREATER(d, 0, -1);
    HWY_ENSURE_GREATER(d, -1, -2);
    HWY_ENSURE_GREATER(d, huge_pos, 1);
    HWY_ENSURE_GREATER(d, huge_pos, 0);
    HWY_ENSURE_GREATER(d, huge_pos, -1);
    HWY_ENSURE_GREATER(d, huge_pos, huge_neg);
    HWY_ENSURE_GREATER(d, 0, huge_neg);

    // Also use Iota to ensure lanes are independent
    HWY_ASSERT_MASK_EQ(d, mask_true, Gt(v2, vn));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt(vn, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(v2, vn));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(vn, v2));

    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(v0, v0));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(v2, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt(vn, vn));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(v0, v0));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(v2, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, Gt(vn, vn));
  }
};

HWY_NOINLINE void TestAllStrictFloat() {
  ForFloatTypes(ForPartialVectors<TestStrictFloat>());
}

struct TestWeakFloat {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v2 = Iota(d, T(2));
    const auto vn = Iota(d, -T(Lanes(d)));

    const auto mask_false = MaskFalse(d);
    const auto mask_true = MaskTrue(d);

    HWY_ASSERT_MASK_EQ(d, mask_true, Ge(v2, v2));
    HWY_ASSERT_MASK_EQ(d, mask_true, Le(vn, vn));

    HWY_ASSERT_MASK_EQ(d, mask_true, Ge(v2, vn));
    HWY_ASSERT_MASK_EQ(d, mask_true, Le(vn, v2));

    HWY_ASSERT_MASK_EQ(d, mask_false, Le(v2, vn));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ge(vn, v2));
  }
};

HWY_NOINLINE void TestAllWeakFloat() {
  ForFloatTypes(ForPartialVectors<TestWeakFloat>());
}

class TestLt128 {
  template <class D>
  static HWY_NOINLINE Vec<D> Make128(D d, uint64_t hi, uint64_t lo) {
    alignas(16) uint64_t in[2];
    in[0] = lo;
    in[1] = hi;
    return LoadDup128(d, in);
  }

 public:
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const V v00 = Zero(d);
    const V v01 = Make128(d, 0, 1);
    const V v10 = Make128(d, 1, 0);
    const V v11 = Add(v01, v10);

    const auto mask_false = MaskFalse(d);
    const auto mask_true = MaskTrue(d);

    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v00, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v01, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v10, v10));

    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v00, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v01, v10));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v01, v11));

    // Reversed order
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v01, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v10, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v11, v01));

    // Also check 128-bit blocks are independent
    const V iota = Iota(d, 1);
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, iota, Add(iota, v01)));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, iota, Add(iota, v10)));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, Add(iota, v01), iota));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, Add(iota, v10), iota));

    // Max value
    const V vm = Make128(d, LimitsMax<T>(), LimitsMax<T>());
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, vm, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, vm, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, vm, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, vm, v10));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, vm, v11));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v00, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v01, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v10, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v11, vm));
  }
};

HWY_NOINLINE void TestAllLt128() { ForGEVectors<128, TestLt128>()(uint64_t()); }

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyCompareTest);
HWY_EXPORT_AND_TEST_P(HwyCompareTest, TestAllEquality);
HWY_EXPORT_AND_TEST_P(HwyCompareTest, TestAllStrictUnsigned);
HWY_EXPORT_AND_TEST_P(HwyCompareTest, TestAllStrictInt);
HWY_EXPORT_AND_TEST_P(HwyCompareTest, TestAllStrictFloat);
HWY_EXPORT_AND_TEST_P(HwyCompareTest, TestAllWeakFloat);
HWY_EXPORT_AND_TEST_P(HwyCompareTest, TestAllLt128);
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif
