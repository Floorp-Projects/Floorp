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

#include <stddef.h>
#include <stdint.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/interleaved_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct TestLoadStoreInterleaved2 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);

    RandomState rng;

    // Data to be interleaved
    auto bytes = AllocateAligned<T>(2 * N);
    for (size_t i = 0; i < 2 * N; ++i) {
      bytes[i] = static_cast<T>(Random32(&rng) & 0xFF);
    }
    const auto in0 = Load(d, &bytes[0 * N]);
    const auto in1 = Load(d, &bytes[1 * N]);

    // Interleave here, ensure vector results match scalar
    auto expected = AllocateAligned<T>(3 * N);
    auto actual_aligned = AllocateAligned<T>(3 * N + 1);
    T* actual = actual_aligned.get() + 1;

    for (size_t rep = 0; rep < 100; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        expected[2 * i + 0] = bytes[0 * N + i];
        expected[2 * i + 1] = bytes[1 * N + i];
        // Ensure we do not write more than 2*N bytes
        expected[2 * N + i] = actual[2 * N + i] = 0;
      }
      StoreInterleaved2(in0, in1, d, actual);
      size_t pos = 0;
      if (!BytesEqual(expected.get(), actual, 3 * N * sizeof(T), &pos)) {
        Print(d, "in0", in0, pos / 4);
        Print(d, "in1", in1, pos / 4);
        const size_t i = pos;
        fprintf(stderr, "interleaved i=%d %f %f %f %f  %f %f %f %f\n",
                static_cast<int>(i), static_cast<double>(actual[i]),
                static_cast<double>(actual[i + 1]),
                static_cast<double>(actual[i + 2]),
                static_cast<double>(actual[i + 3]),
                static_cast<double>(actual[i + 4]),
                static_cast<double>(actual[i + 5]),
                static_cast<double>(actual[i + 6]),
                static_cast<double>(actual[i + 7]));
        HWY_ASSERT(false);
      }

      Vec<D> out0, out1;
      LoadInterleaved2(d, actual, out0, out1);
      HWY_ASSERT_VEC_EQ(d, in0, out0);
      HWY_ASSERT_VEC_EQ(d, in1, out1);
    }
  }
};

HWY_NOINLINE void TestAllLoadStoreInterleaved2() {
#if HWY_TARGET == HWY_RVV
  // Segments are limited to 8 registers, so we can only go up to LMUL=2.
  const ForExtendableVectors<TestLoadStoreInterleaved2, 2> test;
#else
  const ForPartialVectors<TestLoadStoreInterleaved2> test;
#endif
  ForAllTypes(test);
}

// Workaround for build timeout on GCC 12 aarch64, see #776
#if HWY_COMPILER_GCC_ACTUAL >= 1200 && HWY_ARCH_ARM_A64
#define HWY_BROKEN_LOAD34 1
#else
#define HWY_BROKEN_LOAD34 0
#endif

#if !HWY_BROKEN_LOAD34

struct TestLoadStoreInterleaved3 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);

    RandomState rng;

    // Data to be interleaved
    auto bytes = AllocateAligned<T>(3 * N);
    for (size_t i = 0; i < 3 * N; ++i) {
      bytes[i] = static_cast<T>(Random32(&rng) & 0xFF);
    }
    const auto in0 = Load(d, &bytes[0 * N]);
    const auto in1 = Load(d, &bytes[1 * N]);
    const auto in2 = Load(d, &bytes[2 * N]);

    // Interleave here, ensure vector results match scalar
    auto expected = AllocateAligned<T>(4 * N);
    auto actual_aligned = AllocateAligned<T>(4 * N + 1);
    T* actual = actual_aligned.get() + 1;

    for (size_t rep = 0; rep < 100; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        expected[3 * i + 0] = bytes[0 * N + i];
        expected[3 * i + 1] = bytes[1 * N + i];
        expected[3 * i + 2] = bytes[2 * N + i];
        // Ensure we do not write more than 3*N bytes
        expected[3 * N + i] = actual[3 * N + i] = 0;
      }
      StoreInterleaved3(in0, in1, in2, d, actual);
      size_t pos = 0;
      if (!BytesEqual(expected.get(), actual, 4 * N * sizeof(T), &pos)) {
        Print(d, "in0", in0, pos / 3, N);
        Print(d, "in1", in1, pos / 3, N);
        Print(d, "in2", in2, pos / 3, N);
        const size_t i = pos;
        fprintf(stderr, "interleaved i=%d %f %f %f  %f %f %f\n",
                static_cast<int>(i), static_cast<double>(actual[i]),
                static_cast<double>(actual[i + 1]),
                static_cast<double>(actual[i + 2]),
                static_cast<double>(actual[i + 3]),
                static_cast<double>(actual[i + 4]),
                static_cast<double>(actual[i + 5]));
        HWY_ASSERT(false);
      }

      Vec<D> out0, out1, out2;
      LoadInterleaved3(d, actual, out0, out1, out2);
      HWY_ASSERT_VEC_EQ(d, in0, out0);
      HWY_ASSERT_VEC_EQ(d, in1, out1);
      HWY_ASSERT_VEC_EQ(d, in2, out2);
    }
  }
};

HWY_NOINLINE void TestAllLoadStoreInterleaved3() {
#if HWY_TARGET == HWY_RVV
  // Segments are limited to 8 registers, so we can only go up to LMUL=2.
  const ForExtendableVectors<TestLoadStoreInterleaved3, 2> test;
#else
  const ForPartialVectors<TestLoadStoreInterleaved3> test;
#endif
  ForAllTypes(test);
}

struct TestLoadStoreInterleaved4 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);

    RandomState rng;

    // Data to be interleaved
    auto bytes = AllocateAligned<T>(4 * N);

    for (size_t i = 0; i < 4 * N; ++i) {
      bytes[i] = static_cast<T>(Random32(&rng) & 0xFF);
    }
    const auto in0 = Load(d, &bytes[0 * N]);
    const auto in1 = Load(d, &bytes[1 * N]);
    const auto in2 = Load(d, &bytes[2 * N]);
    const auto in3 = Load(d, &bytes[3 * N]);

    // Interleave here, ensure vector results match scalar
    auto expected = AllocateAligned<T>(5 * N);
    auto actual_aligned = AllocateAligned<T>(5 * N + 1);
    T* actual = actual_aligned.get() + 1;

    for (size_t rep = 0; rep < 100; ++rep) {
      for (size_t i = 0; i < N; ++i) {
        expected[4 * i + 0] = bytes[0 * N + i];
        expected[4 * i + 1] = bytes[1 * N + i];
        expected[4 * i + 2] = bytes[2 * N + i];
        expected[4 * i + 3] = bytes[3 * N + i];
        // Ensure we do not write more than 4*N bytes
        expected[4 * N + i] = actual[4 * N + i] = 0;
      }
      StoreInterleaved4(in0, in1, in2, in3, d, actual);
      size_t pos = 0;
      if (!BytesEqual(expected.get(), actual, 5 * N * sizeof(T), &pos)) {
        Print(d, "in0", in0, pos / 4);
        Print(d, "in1", in1, pos / 4);
        Print(d, "in2", in2, pos / 4);
        Print(d, "in3", in3, pos / 4);
        const size_t i = pos;
        fprintf(stderr, "interleaved i=%d %f %f %f %f  %f %f %f %f\n",
                static_cast<int>(i), static_cast<double>(actual[i]),
                static_cast<double>(actual[i + 1]),
                static_cast<double>(actual[i + 2]),
                static_cast<double>(actual[i + 3]),
                static_cast<double>(actual[i + 4]),
                static_cast<double>(actual[i + 5]),
                static_cast<double>(actual[i + 6]),
                static_cast<double>(actual[i + 7]));
        HWY_ASSERT(false);
      }

      Vec<D> out0, out1, out2, out3;
      LoadInterleaved4(d, actual, out0, out1, out2, out3);
      HWY_ASSERT_VEC_EQ(d, in0, out0);
      HWY_ASSERT_VEC_EQ(d, in1, out1);
      HWY_ASSERT_VEC_EQ(d, in2, out2);
      HWY_ASSERT_VEC_EQ(d, in3, out3);
    }
  }
};

HWY_NOINLINE void TestAllLoadStoreInterleaved4() {
#if HWY_TARGET == HWY_RVV
  // Segments are limited to 8 registers, so we can only go up to LMUL=2.
  const ForExtendableVectors<TestLoadStoreInterleaved4, 2> test;
#else
  const ForPartialVectors<TestLoadStoreInterleaved4> test;
#endif
  ForAllTypes(test);
}

#endif  // !HWY_BROKEN_LOAD34

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyInterleavedTest);
HWY_EXPORT_AND_TEST_P(HwyInterleavedTest, TestAllLoadStoreInterleaved2);
#if !HWY_BROKEN_LOAD34
HWY_EXPORT_AND_TEST_P(HwyInterleavedTest, TestAllLoadStoreInterleaved3);
HWY_EXPORT_AND_TEST_P(HwyInterleavedTest, TestAllLoadStoreInterleaved4);
#endif
}  // namespace hwy

#endif
