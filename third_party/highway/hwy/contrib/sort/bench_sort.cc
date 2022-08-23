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

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/bench_sort.cc"
#include "hwy/foreach_target.h"

// After foreach_target
#include "hwy/contrib/sort/algo-inl.h"
#include "hwy/contrib/sort/result-inl.h"
#include "hwy/contrib/sort/vqsort.h"
#include "hwy/contrib/sort/sorting_networks-inl.h"  // SharedTraits
#include "hwy/contrib/sort/traits-inl.h"
#include "hwy/contrib/sort/traits128-inl.h"
#include "hwy/tests/test_util-inl.h"
// clang-format on

#include <stdint.h>
#include <stdio.h>
#include <string.h>  // memcpy

#include <vector>

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {
using detail::LaneTraits;
using detail::OrderAscending;
using detail::OrderDescending;
using detail::SharedTraits;

#if HWY_TARGET != HWY_SCALAR
using detail::OrderAscending128;
using detail::OrderDescending128;
using detail::Traits128;

template <class Traits, typename T>
HWY_NOINLINE void BenchPartition() {
  const SortTag<T> d;
  detail::SharedTraits<Traits> st;
  const Dist dist = Dist::kUniform8;
  double sum = 0.0;

  const size_t max_log2 = AdjustedLog2Reps(20);
  for (size_t log2 = max_log2; log2 < max_log2 + 1; ++log2) {
    const size_t num = 1ull << log2;
    auto aligned = hwy::AllocateAligned<T>(num);
    auto buf =
        hwy::AllocateAligned<T>(hwy::SortConstants::PartitionBufNum(Lanes(d)));

    std::vector<double> seconds;
    const size_t num_reps = (1ull << (14 - log2 / 2)) * kReps;
    for (size_t rep = 0; rep < num_reps; ++rep) {
      (void)GenerateInput(dist, aligned.get(), num);

      const Timestamp t0;

      detail::Partition(d, st, aligned.get(), 0, num - 1, Set(d, T(128)),
                        buf.get());
      seconds.push_back(SecondsSince(t0));
      // 'Use' the result to prevent optimizing out the partition.
      sum += static_cast<double>(aligned.get()[num / 2]);
    }

    MakeResult<T>(Algo::kVQSort, dist, st, num, 1,
                  SummarizeMeasurements(seconds))
        .Print();
  }
  HWY_ASSERT(sum != 999999);  // Prevent optimizing out
}

HWY_NOINLINE void BenchAllPartition() {
  // Not interested in benchmark results for these targets
  if (HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4 ||
      HWY_TARGET == HWY_AVX2) {
    return;
  }

  BenchPartition<LaneTraits<OrderDescending>, float>();
  BenchPartition<LaneTraits<OrderAscending>, int64_t>();
  BenchPartition<Traits128<OrderDescending128>, uint64_t>();
}

template <class Traits, typename T>
HWY_NOINLINE void BenchBase(std::vector<Result>& results) {
  // Not interested in benchmark results for these targets
  if (HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4) {
    return;
  }

  const SortTag<T> d;
  detail::SharedTraits<Traits> st;
  const Dist dist = Dist::kUniform32;

  const size_t N = Lanes(d);
  const size_t num = SortConstants::BaseCaseNum(N);
  auto keys = hwy::AllocateAligned<T>(num);
  auto buf = hwy::AllocateAligned<T>(num + N);

  std::vector<double> seconds;
  double sum = 0;                             // prevents elision
  constexpr size_t kMul = AdjustedReps(600);  // ensures long enough to measure

  for (size_t rep = 0; rep < kReps; ++rep) {
    InputStats<T> input_stats = GenerateInput(dist, keys.get(), num);

    const Timestamp t0;
    for (size_t i = 0; i < kMul; ++i) {
      detail::BaseCase(d, st, keys.get(), num, buf.get());
      sum += static_cast<double>(keys[0]);
    }
    seconds.push_back(SecondsSince(t0));
    // printf("%f\n", seconds.back());

    HWY_ASSERT(VerifySort(st, input_stats, keys.get(), num, "BenchBase"));
  }
  HWY_ASSERT(sum < 1E99);
  results.push_back(MakeResult<T>(Algo::kVQSort, dist, st, num * kMul, 1,
                                  SummarizeMeasurements(seconds)));
}

HWY_NOINLINE void BenchAllBase() {
  // Not interested in benchmark results for these targets
  if (HWY_TARGET == HWY_SSSE3) {
    return;
  }

  std::vector<Result> results;
  BenchBase<LaneTraits<OrderAscending>, float>(results);
  BenchBase<LaneTraits<OrderDescending>, int64_t>(results);
  BenchBase<Traits128<OrderAscending128>, uint64_t>(results);
  for (const Result& r : results) {
    r.Print();
  }
}

std::vector<Algo> AlgoForBench() {
  return {
#if HAVE_AVX2SORT
    Algo::kSEA,
#endif
#if HAVE_PARALLEL_IPS4O
        Algo::kParallelIPS4O,
#endif
#if HAVE_IPS4O
        Algo::kIPS4O,
#endif
#if HAVE_PDQSORT
        Algo::kPDQ,
#endif
#if HAVE_SORT512
        Algo::kSort512,
#endif
        // Algo::kStd,  // too slow to always benchmark
        // Algo::kHeap,  // too slow to always benchmark
        Algo::kVQSort,
  };
}

template <class Traits, typename T>
HWY_NOINLINE void BenchSort(size_t num) {
  SharedState shared;
  detail::SharedTraits<Traits> st;
  auto aligned = hwy::AllocateAligned<T>(num);
  for (Algo algo : AlgoForBench()) {
    for (Dist dist : AllDist()) {
      std::vector<double> seconds;
      for (size_t rep = 0; rep < kReps; ++rep) {
        InputStats<T> input_stats = GenerateInput(dist, aligned.get(), num);

        const Timestamp t0;
        Run<typename Traits::Order>(algo, aligned.get(), num, shared,
                                    /*thread=*/0);
        seconds.push_back(SecondsSince(t0));
        // printf("%f\n", seconds.back());

        HWY_ASSERT(
            VerifySort(st, input_stats, aligned.get(), num, "BenchSort"));
      }
      MakeResult<T>(algo, dist, st, num, 1, SummarizeMeasurements(seconds))
          .Print();
    }  // dist
  }    // algo
}

HWY_NOINLINE void BenchAllSort() {
  // Not interested in benchmark results for these targets
  if (HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4) {
    return;
  }

  constexpr size_t K = 1000;
  constexpr size_t M = K * K;
  (void)K;
  (void)M;
  for (size_t num : {
#if HAVE_PARALLEL_IPS4O
         100 * M,
#else
         AdjustedReps(1 * M),
#endif
       }) {
    // BenchSort<LaneTraits<OrderAscending>, float>(num);
    // BenchSort<LaneTraits<OrderDescending>, double>(num);
    // BenchSort<LaneTraits<OrderAscending>, int16_t>(num);
    BenchSort<LaneTraits<OrderDescending>, int32_t>(num);
    BenchSort<LaneTraits<OrderAscending>, int64_t>(num);
    // BenchSort<LaneTraits<OrderDescending>, uint16_t>(num);
    // BenchSort<LaneTraits<OrderDescending>, uint32_t>(num);
    // BenchSort<LaneTraits<OrderAscending>, uint64_t>(num);

    BenchSort<Traits128<OrderAscending128>, uint64_t>(num);
    // BenchSort<Traits128<OrderAscending128>, uint64_t>(num);
  }
}

#else
void BenchAllPartition() {}
void BenchAllBase() {}
void BenchAllSort() {}
#endif

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
namespace {
HWY_BEFORE_TEST(BenchSort);
HWY_EXPORT_AND_TEST_P(BenchSort, BenchAllPartition);
HWY_EXPORT_AND_TEST_P(BenchSort, BenchAllBase);
HWY_EXPORT_AND_TEST_P(BenchSort, BenchAllSort);
}  // namespace
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif  // HWY_ONCE
