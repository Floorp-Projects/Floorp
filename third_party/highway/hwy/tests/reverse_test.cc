// Copyright 2022 Google LLC
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

#include <stddef.h>

#include "hwy/base.h"

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/reverse_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestReverse {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const RebindToUnsigned<D> du;  // Iota does not support float16_t.
    const auto v = BitCast(d, Iota(du, 1));
    auto expected = AllocateAligned<T>(N);

    // Can't set float16_t value directly, need to permute in memory.
    auto copy = AllocateAligned<T>(N);
    Store(v, d, copy.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] = copy[N - 1 - i];
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Reverse(d, v));
  }
};

struct TestReverse2 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const RebindToUnsigned<D> du;  // Iota does not support float16_t.
    const auto v = BitCast(d, Iota(du, 1));
    auto expected = AllocateAligned<T>(N);

    // Can't set float16_t value directly, need to permute in memory.
    auto copy = AllocateAligned<T>(N);
    Store(v, d, copy.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] = copy[i ^ 1];
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Reverse2(d, v));
  }
};

struct TestReverse4 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const RebindToUnsigned<D> du;  // Iota does not support float16_t.
    const auto v = BitCast(d, Iota(du, 1));
    auto expected = AllocateAligned<T>(N);

    // Can't set float16_t value directly, need to permute in memory.
    auto copy = AllocateAligned<T>(N);
    Store(v, d, copy.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] = copy[i ^ 3];
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Reverse4(d, v));
  }
};

struct TestReverse8 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const RebindToUnsigned<D> du;  // Iota does not support float16_t.
    const auto v = BitCast(d, Iota(du, 1));
    auto expected = AllocateAligned<T>(N);

    // Can't set float16_t value directly, need to permute in memory.
    auto copy = AllocateAligned<T>(N);
    Store(v, d, copy.get());
    for (size_t i = 0; i < N; ++i) {
      expected[i] = copy[i ^ 7];
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), Reverse8(d, v));
  }
};

HWY_NOINLINE void TestAllReverse() {
  // 8-bit is not supported because Risc-V uses rgather of Lanes - Iota,
  // which requires 16 bits.
  ForUIF163264(ForPartialVectors<TestReverse>());
}

HWY_NOINLINE void TestAllReverse2() {
  // 8-bit is not supported because Risc-V uses rgather of Lanes - Iota,
  // which requires 16 bits.
  ForUIF64(ForGEVectors<128, TestReverse2>());
  ForUIF32(ForGEVectors<64, TestReverse2>());
  ForUIF16(ForGEVectors<32, TestReverse2>());
}

HWY_NOINLINE void TestAllReverse4() {
  // 8-bit is not supported because Risc-V uses rgather of Lanes - Iota,
  // which requires 16 bits.
  ForUIF64(ForGEVectors<256, TestReverse4>());
  ForUIF32(ForGEVectors<128, TestReverse4>());
  ForUIF16(ForGEVectors<64, TestReverse4>());
}

HWY_NOINLINE void TestAllReverse8() {
  // 8-bit is not supported because Risc-V uses rgather of Lanes - Iota,
  // which requires 16 bits.
  ForUIF64(ForGEVectors<512, TestReverse8>());
  ForUIF32(ForGEVectors<256, TestReverse8>());
  ForUIF16(ForGEVectors<128, TestReverse8>());
}

struct TestReverseBlocks {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    const RebindToUnsigned<D> du;  // Iota does not support float16_t.
    const auto v = BitCast(d, Iota(du, 1));
    auto expected = AllocateAligned<T>(N);

    constexpr size_t kLanesPerBlock = 16 / sizeof(T);
    const size_t num_blocks = N / kLanesPerBlock;
    HWY_ASSERT(num_blocks != 0);

    // Can't set float16_t value directly, need to permute in memory.
    auto copy = AllocateAligned<T>(N);
    Store(v, d, copy.get());
    for (size_t i = 0; i < N; ++i) {
      const size_t idx_block = i / kLanesPerBlock;
      const size_t base = (num_blocks - 1 - idx_block) * kLanesPerBlock;
      expected[i] = copy[base + (i % kLanesPerBlock)];
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ReverseBlocks(d, v));
  }
};

HWY_NOINLINE void TestAllReverseBlocks() {
  ForAllTypes(ForGEVectors<128, TestReverseBlocks>());
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyReverseTest);
HWY_EXPORT_AND_TEST_P(HwyReverseTest, TestAllReverse);
HWY_EXPORT_AND_TEST_P(HwyReverseTest, TestAllReverse2);
HWY_EXPORT_AND_TEST_P(HwyReverseTest, TestAllReverse4);
HWY_EXPORT_AND_TEST_P(HwyReverseTest, TestAllReverse8);
HWY_EXPORT_AND_TEST_P(HwyReverseTest, TestAllReverseBlocks);
}  // namespace hwy

#endif
