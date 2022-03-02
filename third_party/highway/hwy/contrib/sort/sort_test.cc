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
#define HWY_TARGET_INCLUDE "hwy/contrib/sort/sort_test.cc"
#include "hwy/foreach_target.h"

#include "hwy/contrib/sort/vqsort.h"
// After foreach_target
#include "hwy/contrib/sort/algo-inl.h"
#include "hwy/contrib/sort/result-inl.h"
#include "hwy/contrib/sort/vqsort-inl.h"  // BaseCase
#include "hwy/tests/test_util-inl.h"
// clang-format on

#include <stdint.h>
#include <stdio.h>
#include <string.h>  // memcpy

#include <algorithm>  // std::max
#include <vector>

#undef VQSORT_TEST_IMPL
#if (HWY_TARGET == HWY_SCALAR) || (defined(_MSC_VER) && !HWY_IS_DEBUG_BUILD)
// Scalar does not implement these, and MSVC non-debug builds time out.
#define VQSORT_TEST_IMPL 0
#else
#define VQSORT_TEST_IMPL 1
#endif

#undef VQSORT_TEST_SORT
// MSVC non-debug builds time out.
#if defined(_MSC_VER) && !HWY_IS_DEBUG_BUILD
#define VQSORT_TEST_SORT 0
#else
#define VQSORT_TEST_SORT 1
#endif

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

#if VQSORT_TEST_IMPL || VQSORT_TEST_SORT
using detail::LaneTraits;
using detail::OrderAscending;
using detail::OrderAscending128;
using detail::OrderDescending;
using detail::OrderDescending128;
using detail::SharedTraits;
using detail::Traits128;
#endif

#if !VQSORT_TEST_IMPL
static void TestAllMedian() {}
static void TestAllBaseCase() {}
static void TestAllPartition() {}
static void TestAllGenerator() {}
#else

template <class Traits>
static HWY_NOINLINE void TestMedian3() {
  using T = uint64_t;
  using D = CappedTag<T, 1>;
  SharedTraits<Traits> st;
  const D d;
  using V = Vec<D>;
  for (uint32_t bits = 0; bits < 8; ++bits) {
    const V v0 = Set(d, T{(bits & (1u << 0)) ? 1u : 0u});
    const V v1 = Set(d, T{(bits & (1u << 1)) ? 1u : 0u});
    const V v2 = Set(d, T{(bits & (1u << 2)) ? 1u : 0u});
    const T m = GetLane(detail::MedianOf3(st, v0, v1, v2));
    // If at least half(rounded up) of bits are 1, so is the median.
    const size_t count = PopCount(bits);
    HWY_ASSERT_EQ((count >= 2) ? static_cast<T>(1) : 0, m);
  }
}

HWY_NOINLINE void TestAllMedian() {
  TestMedian3<LaneTraits<OrderAscending> >();
}

template <class Traits, typename T>
static HWY_NOINLINE void TestBaseCaseAscDesc() {
  SharedTraits<Traits> st;
  const SortTag<T> d;
  const size_t N = Lanes(d);
  const size_t base_case_num = SortConstants::BaseCaseNum(N);
  const size_t N1 = st.LanesPerKey();

  constexpr int kDebug = 0;
  auto aligned_keys = hwy::AllocateAligned<T>(N + base_case_num + N);
  auto buf = hwy::AllocateAligned<T>(base_case_num + 2 * N);

  std::vector<size_t> lengths;
  lengths.push_back(HWY_MAX(1, N1));
  lengths.push_back(3 * N1);
  lengths.push_back(base_case_num / 2);
  lengths.push_back(base_case_num / 2 + N1);
  lengths.push_back(base_case_num - N1);
  lengths.push_back(base_case_num);

  std::vector<size_t> misalignments;
  misalignments.push_back(0);
  misalignments.push_back(1);
  if (N >= 6) misalignments.push_back(N / 2 - 1);
  misalignments.push_back(N / 2);
  misalignments.push_back(N / 2 + 1);
  misalignments.push_back(HWY_MIN(2 * N / 3 + 3, size_t{N - 1}));

  for (bool asc : {false, true}) {
    for (size_t len : lengths) {
      for (size_t misalign : misalignments) {
        T* HWY_RESTRICT keys = aligned_keys.get() + misalign;
        if (kDebug) {
          printf("============%s asc %d N1 %d len %d misalign %d\n",
                 hwy::TypeName(T(), 1).c_str(), asc, static_cast<int>(N1),
                 static_cast<int>(len), static_cast<int>(misalign));
        }

        for (size_t i = 0; i < misalign; ++i) {
          aligned_keys[i] = hwy::LowestValue<T>();
        }
        InputStats<T> input_stats;
        for (size_t i = 0; i < len; ++i) {
          keys[i] =
              asc ? static_cast<T>(T(i) + 1) : static_cast<T>(T(len) - T(i));
          input_stats.Notify(keys[i]);
          if (kDebug >= 2) printf("%3zu: %f\n", i, double(keys[i]));
        }
        for (size_t i = len; i < base_case_num + N; ++i) {
          keys[i] = hwy::LowestValue<T>();
        }

        detail::BaseCase(d, st, keys, len, buf.get());

        if (kDebug >= 2) {
          printf("out>>>>>>\n");
          for (size_t i = 0; i < len; ++i) {
            printf("%3zu: %f\n", i, double(keys[i]));
          }
        }

        HWY_ASSERT(VerifySort(st, input_stats, keys, len, "BaseAscDesc"));
        for (size_t i = 0; i < misalign; ++i) {
          if (aligned_keys[i] != hwy::LowestValue<T>())
            HWY_ABORT("Overrun misalign at %d\n", static_cast<int>(i));
        }
        for (size_t i = len; i < base_case_num + N; ++i) {
          if (keys[i] != hwy::LowestValue<T>())
            HWY_ABORT("Overrun right at %d\n", static_cast<int>(i));
        }
      }  // misalign
    }    // len
  }      // asc
}

template <class Traits, typename T>
static HWY_NOINLINE void TestBaseCase01() {
  SharedTraits<Traits> st;
  const SortTag<T> d;
  const size_t N = Lanes(d);
  const size_t base_case_num = SortConstants::BaseCaseNum(N);
  const size_t N1 = st.LanesPerKey();

  constexpr int kDebug = 0;
  auto keys = hwy::AllocateAligned<T>(base_case_num + N);
  auto buf = hwy::AllocateAligned<T>(base_case_num + 2 * N);

  std::vector<size_t> lengths;
  lengths.push_back(HWY_MAX(1, N1));
  lengths.push_back(3 * N1);
  lengths.push_back(base_case_num / 2);
  lengths.push_back(base_case_num / 2 + N1);
  lengths.push_back(base_case_num - N1);
  lengths.push_back(base_case_num);

  for (size_t len : lengths) {
    if (kDebug) {
      printf("============%s 01 N1 %d len %d\n", hwy::TypeName(T(), 1).c_str(),
             static_cast<int>(N1), static_cast<int>(len));
    }
    const uint64_t kMaxBits = AdjustedLog2Reps(HWY_MIN(len, size_t{14}));
    for (uint64_t bits = 0; bits < ((1ull << kMaxBits) - 1); ++bits) {
      InputStats<T> input_stats;
      for (size_t i = 0; i < len; ++i) {
        keys[i] = (i < 64 && (bits & (1ull << i))) ? 1 : 0;
        input_stats.Notify(keys[i]);
        if (kDebug >= 2) printf("%3zu: %f\n", i, double(keys[i]));
      }
      for (size_t i = len; i < base_case_num + N; ++i) {
        keys[i] = hwy::LowestValue<T>();
      }

      detail::BaseCase(d, st, keys.get(), len, buf.get());

      if (kDebug >= 2) {
        printf("out>>>>>>\n");
        for (size_t i = 0; i < len; ++i) {
          printf("%3zu: %f\n", i, double(keys[i]));
        }
      }

      HWY_ASSERT(VerifySort(st, input_stats, keys.get(), len, "Base01"));
      for (size_t i = len; i < base_case_num + N; ++i) {
        if (keys[i] != hwy::LowestValue<T>())
          HWY_ABORT("Overrun right at %d\n", static_cast<int>(i));
      }
    }  // bits
  }    // len
}

template <class Traits, typename T>
static HWY_NOINLINE void TestBaseCase() {
  TestBaseCaseAscDesc<Traits, T>();
  TestBaseCase01<Traits, T>();
}

HWY_NOINLINE void TestAllBaseCase() {
  // Workaround for stack overflow on MSVC debug.
#if defined(_MSC_VER) && HWY_IS_DEBUG_BUILD && (HWY_TARGET == HWY_AVX3)
  return;
#endif

  TestBaseCase<LaneTraits<OrderAscending>, int32_t>();
  TestBaseCase<LaneTraits<OrderDescending>, int64_t>();
  TestBaseCase<Traits128<OrderAscending128>, uint64_t>();
  TestBaseCase<Traits128<OrderDescending128>, uint64_t>();
}

template <class Traits, typename T>
static HWY_NOINLINE void VerifyPartition(Traits st, T* HWY_RESTRICT keys,
                                         size_t left, size_t border,
                                         size_t right, const size_t N1,
                                         const T* pivot) {
  /* for (size_t i = left; i < right; ++i) {
     if (i == border) printf("--\n");
     printf("%4zu: %3d\n", i, keys[i]);
   }*/

  HWY_ASSERT(left % N1 == 0);
  HWY_ASSERT(border % N1 == 0);
  HWY_ASSERT(right % N1 == 0);
  const bool asc = typename Traits::Order().IsAscending();
  for (size_t i = left; i < border; i += N1) {
    if (st.Compare1(pivot, keys + i)) {
      HWY_ABORT(
          "%s: asc %d left[%d] piv %.0f %.0f compares before %.0f %.0f "
          "border %d",
          hwy::TypeName(T(), 1).c_str(), asc, static_cast<int>(i),
          double(pivot[1]), double(pivot[0]), double(keys[i + 1]),
          double(keys[i + 0]), static_cast<int>(border));
    }
  }
  for (size_t i = border; i < right; i += N1) {
    if (!st.Compare1(pivot, keys + i)) {
      HWY_ABORT(
          "%s: asc %d right[%d] piv %.0f %.0f compares after %.0f %.0f "
          "border %d",
          hwy::TypeName(T(), 1).c_str(), asc, static_cast<int>(i),
          double(pivot[1]), double(pivot[0]), double(keys[i + 1]),
          double(keys[i]), static_cast<int>(border));
    }
  }
}

template <class Traits, typename T>
static HWY_NOINLINE void TestPartition() {
  const SortTag<T> d;
  SharedTraits<Traits> st;
  const bool asc = typename Traits::Order().IsAscending();
  const size_t N = Lanes(d);
  constexpr int kDebug = 0;
  const size_t base_case_num = SortConstants::BaseCaseNum(N);
  // left + len + align
  const size_t total = 32 + (base_case_num + 4 * HWY_MAX(N, 4)) + 2 * N;
  auto aligned_keys = hwy::AllocateAligned<T>(total);
  auto buf = hwy::AllocateAligned<T>(SortConstants::PartitionBufNum(N));

  const size_t N1 = st.LanesPerKey();
  for (bool in_asc : {false, true}) {
    for (int left_i : {0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 15, 22, 28, 29, 30, 31}) {
      const size_t left = static_cast<size_t>(left_i) & ~(N1 - 1);
      for (size_t ofs : {N, N + 1, N + 2, N + 3, 2 * N, 2 * N + 1, 2 * N + 2,
                         2 * N + 3, 3 * N - 1, 4 * N - 3, 4 * N - 2}) {
        const size_t len = (base_case_num + ofs) & ~(N1 - 1);
        for (T pivot1 :
             {T(0), T(len / 3), T(len / 2), T(2 * len / 3), T(len)}) {
          const T pivot2[2] = {pivot1, 0};
          const auto pivot = st.SetKey(d, pivot2);
          for (size_t misalign = 0; misalign < N;
               misalign += st.LanesPerKey()) {
            T* HWY_RESTRICT keys = aligned_keys.get() + misalign;
            const size_t right = left + len;
            if (kDebug) {
              printf(
                  "=========%s asc %d left %d len %d right %d piv %.0f %.0f\n",
                  hwy::TypeName(T(), 1).c_str(), asc, static_cast<int>(left),
                  static_cast<int>(len), static_cast<int>(right),
                  double(pivot2[1]), double(pivot2[0]));
            }

            for (size_t i = 0; i < misalign; ++i) {
              aligned_keys[i] = hwy::LowestValue<T>();
            }
            for (size_t i = 0; i < left; ++i) {
              keys[i] = hwy::LowestValue<T>();
            }
            for (size_t i = left; i < right; ++i) {
              keys[i] = static_cast<T>(in_asc ? T(i + 1) - static_cast<T>(left)
                                              : static_cast<T>(right) - T(i));
              if (kDebug >= 2) printf("%3zu: %f\n", i, double(keys[i]));
            }
            for (size_t i = right; i < total - misalign; ++i) {
              keys[i] = hwy::LowestValue<T>();
            }

            size_t border =
                detail::Partition(d, st, keys, left, right, pivot, buf.get());

            if (kDebug >= 2) {
              printf("out>>>>>>\n");
              for (size_t i = left; i < right; ++i) {
                printf("%3zu: %f\n", i, double(keys[i]));
              }
              for (size_t i = right; i < total - misalign; ++i) {
                printf("%3zu: sentinel %f\n", i, double(keys[i]));
              }
            }

            VerifyPartition(st, keys, left, border, right, N1, pivot2);
            for (size_t i = 0; i < misalign; ++i) {
              if (aligned_keys[i] != hwy::LowestValue<T>())
                HWY_ABORT("Overrun misalign at %d\n", static_cast<int>(i));
            }
            for (size_t i = 0; i < left; ++i) {
              if (keys[i] != hwy::LowestValue<T>())
                HWY_ABORT("Overrun left at %d\n", static_cast<int>(i));
            }
            for (size_t i = right; i < total - misalign; ++i) {
              if (keys[i] != hwy::LowestValue<T>())
                HWY_ABORT("Overrun right at %d\n", static_cast<int>(i));
            }
          }  // misalign
        }    // pivot
      }      // len
    }        // left
  }          // asc
}

HWY_NOINLINE void TestAllPartition() {
  TestPartition<LaneTraits<OrderAscending>, int16_t>();
  TestPartition<LaneTraits<OrderDescending>, int32_t>();
  TestPartition<LaneTraits<OrderAscending>, int64_t>();
  TestPartition<LaneTraits<OrderDescending>, float>();
#if HWY_HAVE_FLOAT64
  TestPartition<LaneTraits<OrderDescending>, double>();
#endif
  TestPartition<Traits128<OrderAscending128>, uint64_t>();
  TestPartition<Traits128<OrderDescending128>, uint64_t>();
}

// (used for sample selection for choosing a pivot)
template <typename TU>
static HWY_NOINLINE void TestRandomGenerator() {
  static_assert(!hwy::IsSigned<TU>(), "");
  SortTag<TU> du;
  const size_t N = Lanes(du);

  detail::Generator rng(&N, N);

  const size_t lanes_per_block = HWY_MAX(64 / sizeof(TU), N);  // power of two

  for (uint32_t num_blocks = 2; num_blocks < 100000;
       num_blocks = 3 * num_blocks / 2) {
    // Generate some numbers and ensure all are in range
    uint64_t sum = 0;
    constexpr size_t kReps = 10000;
    for (size_t rep = 0; rep < kReps; ++rep) {
      const uint32_t bits = rng() & 0xFFFFFFFF;
      const size_t index = detail::RandomChunkIndex(num_blocks, bits);
      HWY_ASSERT(((index + 1) * lanes_per_block) <=
                 num_blocks * lanes_per_block);

      sum += index;
    }

    // Also ensure the mean is near the middle of the range
    const double expected = (num_blocks - 1) / 2.0;
    const double actual = double(sum) / kReps;
    HWY_ASSERT(0.9 * expected <= actual && actual <= 1.1 * expected);
  }
}

HWY_NOINLINE void TestAllGenerator() {
  TestRandomGenerator<uint32_t>();
  TestRandomGenerator<uint64_t>();
}

#endif  // VQSORT_TEST_IMPL

#if !VQSORT_TEST_SORT
static void TestAllSort() {}
#else

// Remembers input, and compares results to that of a reference algorithm.
template <class Traits, typename T>
class CompareResults {
 public:
  void SetInput(const T* in, size_t num) {
    copy_.resize(num);
    memcpy(copy_.data(), in, num * sizeof(T));
  }

  bool Verify(const T* output) {
#if HAVE_PDQSORT
    const Algo reference = Algo::kPDQ;
#else
    const Algo reference = Algo::kStd;
#endif
    SharedState shared;
    using Order = typename Traits::Order;
    Run<Order>(reference, copy_.data(), copy_.size(), shared,
               /*thread=*/0);

    for (size_t i = 0; i < copy_.size(); ++i) {
      if (copy_[i] != output[i]) {
        fprintf(stderr, "Asc %d mismatch at %d: %A %A\n", Order().IsAscending(),
                static_cast<int>(i), double(copy_[i]), double(output[i]));
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<T> copy_;
};

std::vector<Algo> AlgoForTest() {
  return {
#if HAVE_AVX2SORT
    Algo::kSEA,
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
        Algo::kHeap, Algo::kVQSort,
  };
}

template <class Traits, typename T>
void TestSort(size_t num) {
  // TODO(janwas): fix
  if (HWY_TARGET == HWY_SSSE3) return;
// Workaround for stack overflow on clang-cl (/F 8388608 does not help).
#if defined(_MSC_VER) && HWY_IS_DEBUG_BUILD && (HWY_TARGET == HWY_AVX3)
  return;
#endif

  SharedState shared;
  SharedTraits<Traits> st;

  constexpr size_t kMaxMisalign = 16;
  auto aligned = hwy::AllocateAligned<T>(kMaxMisalign + num + kMaxMisalign);
  for (Algo algo : AlgoForTest()) {
#if HAVE_IPS4O
    if (st.Is128() && (algo == Algo::kIPS4O || algo == Algo::kParallelIPS4O)) {
      continue;
    }
#endif
    for (Dist dist : AllDist()) {
      for (size_t misalign : {size_t{0}, size_t{st.LanesPerKey()},
                              size_t{3 * st.LanesPerKey()}, kMaxMisalign / 2}) {
        T* keys = aligned.get() + misalign;

        // Set up red zones before/after the keys to sort
        for (size_t i = 0; i < misalign; ++i) {
          aligned[i] = hwy::LowestValue<T>();
        }
        for (size_t i = 0; i < kMaxMisalign; ++i) {
          keys[num + i] = hwy::HighestValue<T>();
        }
#if HWY_IS_MSAN
        __msan_poison(aligned.get(), misalign * sizeof(T));
        __msan_poison(keys + num, kMaxMisalign * sizeof(T));
#endif
        InputStats<T> input_stats = GenerateInput(dist, keys, num);

        CompareResults<Traits, T> compare;
        compare.SetInput(keys, num);

        Run<typename Traits::Order>(algo, keys, num, shared, /*thread=*/0);
        HWY_ASSERT(compare.Verify(keys));
        HWY_ASSERT(VerifySort(st, input_stats, keys, num, "TestSort"));

        // Check red zones
#if HWY_IS_MSAN
        __msan_unpoison(aligned.get(), misalign * sizeof(T));
        __msan_unpoison(keys + num, kMaxMisalign * sizeof(T));
#endif
        for (size_t i = 0; i < misalign; ++i) {
          if (aligned[i] != hwy::LowestValue<T>())
            HWY_ABORT("Overrun left at %d\n", static_cast<int>(i));
        }
        for (size_t i = num; i < num + kMaxMisalign; ++i) {
          if (keys[i] != hwy::HighestValue<T>())
            HWY_ABORT("Overrun right at %d\n", static_cast<int>(i));
        }
      }  // misalign
    }    // dist
  }      // algo
}

void TestAllSort() {
  const size_t num = 15 * 1000;

  TestSort<LaneTraits<OrderAscending>, int16_t>(num);
  TestSort<LaneTraits<OrderDescending>, uint16_t>(num);

  TestSort<LaneTraits<OrderDescending>, int32_t>(num);
  TestSort<LaneTraits<OrderDescending>, uint32_t>(num);

  TestSort<LaneTraits<OrderAscending>, int64_t>(num);
  TestSort<LaneTraits<OrderAscending>, uint64_t>(num);

  // WARNING: for float types, SIMD comparisons will flush denormals to zero,
  // causing mismatches with scalar sorts. In this test, we avoid generating
  // denormal inputs.
  TestSort<LaneTraits<OrderAscending>, float>(num);
#if HWY_HAVE_FLOAT64  // protects algo-inl's GenerateRandom
  if (Sorter::HaveFloat64()) {
    TestSort<LaneTraits<OrderDescending>, double>(num);
  }
#endif

  TestSort<Traits128<OrderAscending128>, uint64_t>(num);
  TestSort<Traits128<OrderAscending128>, uint64_t>(num);
}

#endif  // VQSORT_TEST_SORT

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
namespace {
HWY_BEFORE_TEST(SortTest);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllMedian);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllBaseCase);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllPartition);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllGenerator);
HWY_EXPORT_AND_TEST_P(SortTest, TestAllSort);
}  // namespace
}  // namespace hwy

// Ought not to be necessary, but without this, no tests run on RVV.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif  // HWY_ONCE
