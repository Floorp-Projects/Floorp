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

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/sort_test.cc"
#include "hwy/foreach_target.h"

#include "hwy/contrib/sort/sort-inl.h"
#include "hwy/tests/test_util-inl.h"
// clang-format on

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

#if HWY_TARGET != HWY_SCALAR && HWY_ARCH_X86

template <class D>
size_t K(D d) {
  return SortBatchSize(d);
}

template <SortOrder kOrder, class D>
void Validate(D d, const TFromD<D>* in, const TFromD<D>* out) {
  const size_t N = Lanes(d);
  // Ensure it matches the sort order
  for (size_t i = 0; i < K(d) - 1; ++i) {
    if (!verify::Compare(out[i], out[i + 1], kOrder)) {
      printf("range=%" PRIu64 " lane=%" PRIu64 " N=%" PRIu64 " %.0f %.0f\n\n",
             static_cast<uint64_t>(i), static_cast<uint64_t>(i),
             static_cast<uint64_t>(N), static_cast<float>(out[i + 0]),
             static_cast<float>(out[i + 1]));
      for (size_t i = 0; i < K(d); ++i) {
        printf("%.0f\n", static_cast<float>(out[i]));
      }

      printf("\n\nin was:\n");
      for (size_t i = 0; i < K(d); ++i) {
        printf("%.0f\n", static_cast<float>(in[i]));
      }
      fflush(stdout);
      HWY_ABORT("Sort is incorrect");
    }
  }

  // Also verify sums match (detects duplicated/lost values)
  double expected_sum = 0.0;
  double actual_sum = 0.0;
  for (size_t i = 0; i < K(d); ++i) {
    expected_sum += in[i];
    actual_sum += out[i];
  }
  if (expected_sum != actual_sum) {
    for (size_t i = 0; i < K(d); ++i) {
      printf("%.0f  %.0f\n", static_cast<float>(in[i]),
             static_cast<float>(out[i]));
    }
    HWY_ABORT("Mismatch");
  }
}

class TestReverse {
  template <SortOrder kOrder, class D>
  void TestOrder(D d, RandomState& /* rng */) {
    using T = TFromD<D>;
    const size_t N = Lanes(d);
    HWY_ASSERT((N % 4) == 0);
    auto in = AllocateAligned<T>(K(d));
    auto inout = AllocateAligned<T>(K(d));

    const size_t expected_size = SortBatchSize(d);

    for (size_t i = 0; i < K(d); ++i) {
      in[i] = static_cast<T>(K(d) - i);
      inout[i] = in[i];
    }

    const size_t actual_size = SortBatch<kOrder>(d, inout.get());
    HWY_ASSERT_EQ(expected_size, actual_size);
    Validate<kOrder>(d, in.get(), inout.get());
  }

 public:
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;
    TestOrder<SortOrder::kAscending>(d, rng);
    TestOrder<SortOrder::kDescending>(d, rng);
  }
};

void TestAllReverse() {
  TestReverse test;
  test(int32_t(), CappedTag<int32_t, 16>());
  test(uint32_t(), CappedTag<uint32_t, 16>());
}

class TestRanges {
  template <SortOrder kOrder, class D>
  void TestOrder(D d, RandomState& rng) {
    using T = TFromD<D>;
    const size_t N = Lanes(d);
    HWY_ASSERT((N % 4) == 0);
    auto in = AllocateAligned<T>(K(d));
    auto inout = AllocateAligned<T>(K(d));

    const size_t expected_size = SortBatchSize(d);

    // For each range, try all 0/1 combinations and set any other lanes to
    // random inputs.
    constexpr size_t kRange = 8;
    for (size_t range = 0; range < K(d); range += kRange) {
      for (size_t bits = 0; bits < (1ull << kRange); ++bits) {
        // First set all to random, will later overwrite those for `range`
        for (size_t i = 0; i < K(d); ++i) {
          in[i] = inout[i] = static_cast<T>(Random32(&rng) & 0xFF);
        }
        // Now set the current combination of {0,1} for elements in the range.
        // This is sufficient to establish correctness (arbitrary inputs could
        // be mapped to 0/1 with a comparison predicate).
        for (size_t i = 0; i < kRange; ++i) {
          in[range + i] = inout[range + i] = (bits >> i) & 1;
        }

        const size_t actual_size = SortBatch<kOrder>(d, inout.get());
        HWY_ASSERT_EQ(expected_size, actual_size);
        Validate<kOrder>(d, in.get(), inout.get());
      }
    }
  }

 public:
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;
    TestOrder<SortOrder::kAscending>(d, rng);
    TestOrder<SortOrder::kDescending>(d, rng);
  }
};

void TestAllRanges() {
  TestRanges test;
  test(int32_t(), CappedTag<int32_t, 16>());
  test(uint32_t(), CappedTag<uint32_t, 16>());
}

#else
void TestAllReverse() {}
void TestAllRanges() {}
#endif  // HWY_TARGET != HWY_SCALAR && HWY_ARCH_X86

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(SortTest);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllReverse);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllRanges);
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif
