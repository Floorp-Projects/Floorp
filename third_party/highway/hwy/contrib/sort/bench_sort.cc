// Copyright 2021 Google LLC
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>  // memcpy

#include <vector>

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/bench_sort.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep

// After foreach_target
#include "hwy/contrib/sort/algo-inl.h"
#include "hwy/contrib/sort/result-inl.h"
#include "hwy/contrib/sort/sorting_networks-inl.h"  // SharedTraits
#include "hwy/contrib/sort/traits-inl.h"
#include "hwy/contrib/sort/traits128-inl.h"
#include "hwy/tests/test_util-inl.h"
// clang-format on

// Mode for larger sorts because M1 is able to access more than the per-core
// share of L2, so 1M elements might still be in cache.
#define SORT_100M 0

HWY_BEFORE_NAMESPACE();
namespace hwy {
// Defined within HWY_ONCE, used by BenchAllSort.
extern int64_t first_sort_target;

namespace HWY_NAMESPACE {
namespace {
using detail::TraitsLane;
using detail::OrderAscending;
using detail::OrderDescending;
using detail::SharedTraits;

#if VQSORT_ENABLED || HWY_IDE
using detail::OrderAscending128;
using detail::Traits128;

template <class Traits>
HWY_NOINLINE void BenchPartition() {
  using LaneType = typename Traits::LaneType;
  using KeyType = typename Traits::KeyType;
  const SortTag<LaneType> d;
  detail::SharedTraits<Traits> st;
  const Dist dist = Dist::kUniform8;
  double sum = 0.0;

  detail::Generator rng(&sum, 123);  // for ChoosePivot

  const size_t max_log2 = AdjustedLog2Reps(20);
  for (size_t log2 = max_log2; log2 < max_log2 + 1; ++log2) {
    const size_t num_lanes = 1ull << log2;
    const size_t num_keys = num_lanes / st.LanesPerKey();
    auto aligned = hwy::AllocateAligned<LaneType>(num_lanes);
    auto buf = hwy::AllocateAligned<LaneType>(
        HWY_MAX(hwy::SortConstants::PartitionBufNum(Lanes(d)),
                hwy::SortConstants::PivotBufNum(sizeof(LaneType), Lanes(d))));

    std::vector<double> seconds;
    const size_t num_reps = (1ull << (14 - log2 / 2)) * 30;
    for (size_t rep = 0; rep < num_reps; ++rep) {
      (void)GenerateInput(dist, aligned.get(), num_lanes);

      // The pivot value can influence performance. Do exactly what vqsort will
      // do so that the performance (influenced by prefetching and branch
      // prediction) is likely to predict the actual performance inside vqsort.
      const auto pivot = detail::ChoosePivot(d, st, aligned.get(), 0, num_lanes,
                                             buf.get(), rng);

      const Timestamp t0;
      detail::Partition(d, st, aligned.get(), 0, num_lanes - 1, pivot,
                        buf.get());
      seconds.push_back(SecondsSince(t0));
      // 'Use' the result to prevent optimizing out the partition.
      sum += static_cast<double>(aligned.get()[num_lanes / 2]);
    }

    Result(Algo::kVQSort, dist, num_keys, 1, SummarizeMeasurements(seconds),
           sizeof(KeyType), st.KeyString())
        .Print();
  }
  HWY_ASSERT(sum != 999999);  // Prevent optimizing out
}

HWY_NOINLINE void BenchAllPartition() {
  // Not interested in benchmark results for these targets
  if (HWY_TARGET == HWY_SSSE3) {
    return;
  }

  BenchPartition<TraitsLane<OrderDescending<float>>>();
  BenchPartition<TraitsLane<OrderDescending<int32_t>>>();
  BenchPartition<TraitsLane<OrderDescending<int64_t>>>();
  BenchPartition<Traits128<OrderAscending128>>();
  // BenchPartition<Traits128<OrderDescending128>>();
  // BenchPartition<Traits128<OrderAscendingKV128>>();
}

template <class Traits>
HWY_NOINLINE void BenchBase(std::vector<Result>& results) {
  // Not interested in benchmark results for these targets
  if (HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4) {
    return;
  }

  using LaneType = typename Traits::LaneType;
  using KeyType = typename Traits::KeyType;
  const SortTag<LaneType> d;
  detail::SharedTraits<Traits> st;
  const Dist dist = Dist::kUniform32;

  const size_t N = Lanes(d);
  const size_t num_lanes = SortConstants::BaseCaseNum(N);
  const size_t num_keys = num_lanes / st.LanesPerKey();
  auto keys = hwy::AllocateAligned<LaneType>(num_lanes);
  auto buf = hwy::AllocateAligned<LaneType>(num_lanes + N);

  std::vector<double> seconds;
  double sum = 0;                             // prevents elision
  constexpr size_t kMul = AdjustedReps(600);  // ensures long enough to measure

  for (size_t rep = 0; rep < 30; ++rep) {
    InputStats<LaneType> input_stats =
        GenerateInput(dist, keys.get(), num_lanes);

    const Timestamp t0;
    for (size_t i = 0; i < kMul; ++i) {
      detail::BaseCase(d, st, keys.get(), keys.get() + num_lanes, num_lanes,
                       buf.get());
      sum += static_cast<double>(keys[0]);
    }
    seconds.push_back(SecondsSince(t0));
    // printf("%f\n", seconds.back());

    HWY_ASSERT(VerifySort(st, input_stats, keys.get(), num_lanes, "BenchBase"));
  }
  HWY_ASSERT(sum < 1E99);
  results.emplace_back(Algo::kVQSort, dist, num_keys * kMul, 1,
                       SummarizeMeasurements(seconds), sizeof(KeyType),
                       st.KeyString());
}

HWY_NOINLINE void BenchAllBase() {
  // Not interested in benchmark results for these targets
  if (HWY_TARGET == HWY_SSSE3) {
    return;
  }

  std::vector<Result> results;
  BenchBase<TraitsLane<OrderAscending<float>>>(results);
  BenchBase<TraitsLane<OrderDescending<int64_t>>>(results);
  BenchBase<Traits128<OrderAscending128>>(results);
  for (const Result& r : results) {
    r.Print();
  }
}

#else
void BenchAllPartition() {}
void BenchAllBase() {}
#endif  // VQSORT_ENABLED

std::vector<Algo> AlgoForBench() {
  return {
#if HAVE_AVX2SORT
    Algo::kSEA,
#endif
#if HAVE_PARALLEL_IPS4O
        Algo::kParallelIPS4O,
#elif HAVE_IPS4O
        Algo::kIPS4O,
#endif
#if HAVE_PDQSORT
        Algo::kPDQ,
#endif
#if HAVE_SORT512
        Algo::kSort512,
#endif
// Only include if we're compiling for the target it supports.
#if HAVE_VXSORT && ((VXSORT_AVX3 && HWY_TARGET == HWY_AVX3) || \
                    (!VXSORT_AVX3 && HWY_TARGET == HWY_AVX2))
        Algo::kVXSort,
#endif

#if !HAVE_PARALLEL_IPS4O
#if !SORT_100M
        // These are 10-20x slower, but that's OK for the default size when we
        // are not testing the parallel nor 100M modes.
        Algo::kStd, Algo::kHeap,
#endif

        Algo::kVQSort,  // only ~4x slower, but not required for Table 1a
#endif
  };
}

template <class Traits>
HWY_NOINLINE void BenchSort(size_t num_keys) {
  if (first_sort_target == 0) first_sort_target = HWY_TARGET;

  SharedState shared;
  detail::SharedTraits<Traits> st;
  using Order = typename Traits::Order;
  using LaneType = typename Traits::LaneType;
  using KeyType = typename Traits::KeyType;
  const size_t num_lanes = num_keys * st.LanesPerKey();
  auto aligned = hwy::AllocateAligned<LaneType>(num_lanes);

  const size_t reps = num_keys > 1000 * 1000 ? 10 : 30;

  for (Algo algo : AlgoForBench()) {
    // Other algorithms don't depend on the vector instructions, so only run
    // them for the first target.
#if !HAVE_VXSORT
    if (algo != Algo::kVQSort && HWY_TARGET != first_sort_target) {
      continue;
    }
#endif

    for (Dist dist : AllDist()) {
      std::vector<double> seconds;
      for (size_t rep = 0; rep < reps; ++rep) {
        InputStats<LaneType> input_stats =
            GenerateInput(dist, aligned.get(), num_lanes);

        const Timestamp t0;
        Run<Order>(algo, reinterpret_cast<KeyType*>(aligned.get()), num_keys,
                   shared, /*thread=*/0);
        seconds.push_back(SecondsSince(t0));
        // printf("%f\n", seconds.back());

        HWY_ASSERT(
            VerifySort(st, input_stats, aligned.get(), num_lanes, "BenchSort"));
      }
      Result(algo, dist, num_keys, 1, SummarizeMeasurements(seconds),
             sizeof(KeyType), st.KeyString())
          .Print();
    }  // dist
  }    // algo
}

HWY_NOINLINE void BenchAllSort() {
  // Not interested in benchmark results for these targets
  if (HWY_TARGET == HWY_SSSE3 || HWY_TARGET == HWY_SSE4 ||
      HWY_TARGET == HWY_EMU128) {
    return;
  }
  // Only enable EMU128 on x86 - it's slow on emulators.
  if (!HWY_ARCH_X86 && (HWY_TARGET == HWY_EMU128)) return;

  constexpr size_t K = 1000;
  constexpr size_t M = K * K;
  (void)K;
  (void)M;
  for (size_t num_keys : {
#if HAVE_PARALLEL_IPS4O || SORT_100M
         100 * M,
#else
        1 * M,
#endif
       }) {
    BenchSort<TraitsLane<OrderAscending<float>>>(num_keys);
    // BenchSort<TraitsLane<OrderDescending<double>>>(num_keys);
    // BenchSort<TraitsLane<OrderAscending<int16_t>>>(num_keys);
    BenchSort<TraitsLane<OrderDescending<int32_t>>>(num_keys);
    BenchSort<TraitsLane<OrderAscending<int64_t>>>(num_keys);
    // BenchSort<TraitsLane<OrderDescending<uint16_t>>>(num_keys);
    // BenchSort<TraitsLane<OrderDescending<uint32_t>>>(num_keys);
    // BenchSort<TraitsLane<OrderAscending<uint64_t>>>(num_keys);

#if !HAVE_VXSORT && VQSORT_ENABLED
    BenchSort<Traits128<OrderAscending128>>(num_keys);
    // BenchSort<Traits128<OrderAscendingKV128>>(num_keys);
#endif
  }
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
int64_t first_sort_target = 0;  // none run yet
namespace {
HWY_BEFORE_TEST(BenchSort);
HWY_EXPORT_AND_TEST_P(BenchSort, BenchAllPartition);
HWY_EXPORT_AND_TEST_P(BenchSort, BenchAllBase);
HWY_EXPORT_AND_TEST_P(BenchSort, BenchAllSort);
}  // namespace
}  // namespace hwy

#endif  // HWY_ONCE
