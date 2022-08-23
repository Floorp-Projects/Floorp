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

// Normal include guard for target-independent parts
#ifndef HIGHWAY_HWY_CONTRIB_SORT_ALGO_INL_H_
#define HIGHWAY_HWY_CONTRIB_SORT_ALGO_INL_H_

#include <stdint.h>
#include <string.h>  // memcpy

#include <algorithm>
#include <cmath>  // std::abs
#include <vector>

#include "hwy/base.h"
#include "hwy/contrib/sort/vqsort.h"

// Third-party algorithms
#define HAVE_AVX2SORT 0
#define HAVE_IPS4O 0
#define HAVE_PARALLEL_IPS4O (HAVE_IPS4O && 1)
#define HAVE_PDQSORT 0
#define HAVE_SORT512 0

#if HAVE_AVX2SORT
HWY_PUSH_ATTRIBUTES("avx2,avx")
#include "avx2sort.h"
HWY_POP_ATTRIBUTES
#endif
#if HAVE_IPS4O
#include "third_party/ips4o/include/ips4o.hpp"
#include "third_party/ips4o/include/ips4o/thread_pool.hpp"
#endif
#if HAVE_PDQSORT
#include "third_party/boost/allowed/sort/sort.hpp"
#endif
#if HAVE_SORT512
#include "sort512.h"
#endif

namespace hwy {

enum class Dist { kUniform8, kUniform16, kUniform32 };

std::vector<Dist> AllDist() {
  return {/*Dist::kUniform8,*/ Dist::kUniform16, Dist::kUniform32};
}

const char* DistName(Dist dist) {
  switch (dist) {
    case Dist::kUniform8:
      return "uniform8";
    case Dist::kUniform16:
      return "uniform16";
    case Dist::kUniform32:
      return "uniform32";
  }
  return "unreachable";
}

template <typename T>
class InputStats {
 public:
  void Notify(T value) {
    min_ = std::min(min_, value);
    max_ = std::max(max_, value);
    sumf_ += static_cast<double>(value);
    count_ += 1;
  }

  bool operator==(const InputStats& other) const {
    if (count_ != other.count_) {
      HWY_ABORT("count %d vs %d\n", static_cast<int>(count_),
                static_cast<int>(other.count_));
    }

    if (min_ != other.min_ || max_ != other.max_) {
      HWY_ABORT("minmax %f/%f vs %f/%f\n", double(min_), double(max_),
                double(other.min_), double(other.max_));
    }

    // Sum helps detect duplicated/lost values
    if (sumf_ != other.sumf_) {
      // Allow some tolerance because kUniform32 * num can exceed double
      // precision.
      const double mul = 1E-9;  // prevent destructive cancellation
      const double err = std::abs(sumf_ * mul - other.sumf_ * mul);
      if (err > 1E-3) {
        HWY_ABORT("Sum mismatch %.15e %.15e (%f) min %g max %g\n", sumf_,
                  other.sumf_, err, double(min_), double(max_));
      }
    }

    return true;
  }

 private:
  T min_ = hwy::HighestValue<T>();
  T max_ = hwy::LowestValue<T>();
  double sumf_ = 0.0;
  size_t count_ = 0;
};

enum class Algo {
#if HAVE_AVX2SORT
  kSEA,
#endif
#if HAVE_IPS4O
  kIPS4O,
#endif
#if HAVE_PARALLEL_IPS4O
  kParallelIPS4O,
#endif
#if HAVE_PDQSORT
  kPDQ,
#endif
#if HAVE_SORT512
  kSort512,
#endif
  kStd,
  kVQSort,
  kHeap,
};

const char* AlgoName(Algo algo) {
  switch (algo) {
#if HAVE_AVX2SORT
    case Algo::kSEA:
      return "sea";
#endif
#if HAVE_IPS4O
    case Algo::kIPS4O:
      return "ips4o";
#endif
#if HAVE_PARALLEL_IPS4O
    case Algo::kParallelIPS4O:
      return "par_ips4o";
#endif
#if HAVE_PDQSORT
    case Algo::kPDQ:
      return "pdq";
#endif
#if HAVE_SORT512
    case Algo::kSort512:
      return "sort512";
#endif
    case Algo::kStd:
      return "std";
    case Algo::kVQSort:
      return "vq";
    case Algo::kHeap:
      return "heap";
  }
  return "unreachable";
}

}  // namespace hwy
#endif  // HIGHWAY_HWY_CONTRIB_SORT_ALGO_INL_H_

// Per-target
#if defined(HIGHWAY_HWY_CONTRIB_SORT_ALGO_TOGGLE) == \
    defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CONTRIB_SORT_ALGO_TOGGLE
#undef HIGHWAY_HWY_CONTRIB_SORT_ALGO_TOGGLE
#else
#define HIGHWAY_HWY_CONTRIB_SORT_ALGO_TOGGLE
#endif

#include "hwy/contrib/sort/traits-inl.h"
#include "hwy/contrib/sort/traits128-inl.h"
#include "hwy/contrib/sort/vqsort-inl.h"  // HeapSort
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

class Xorshift128Plus {
  static HWY_INLINE uint64_t SplitMix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }

 public:
  // Generates two vectors of 64-bit seeds via SplitMix64 and stores into
  // `seeds`. Generating these afresh in each ChoosePivot is too expensive.
  template <class DU64>
  static void GenerateSeeds(DU64 du64, TFromD<DU64>* HWY_RESTRICT seeds) {
    seeds[0] = SplitMix64(0x9E3779B97F4A7C15ull);
    for (size_t i = 1; i < 2 * Lanes(du64); ++i) {
      seeds[i] = SplitMix64(seeds[i - 1]);
    }
  }

  // Need to pass in the state because vector cannot be class members.
  template <class DU64>
  static Vec<DU64> RandomBits(DU64 /* tag */, Vec<DU64>& state0,
                              Vec<DU64>& state1) {
    Vec<DU64> s1 = state0;
    Vec<DU64> s0 = state1;
    const Vec<DU64> bits = Add(s1, s0);
    state0 = s0;
    s1 = Xor(s1, ShiftLeft<23>(s1));
    state1 = Xor(s1, Xor(s0, Xor(ShiftRight<18>(s1), ShiftRight<5>(s0))));
    return bits;
  }
};

template <typename T, class DU64, HWY_IF_NOT_FLOAT(T)>
Vec<DU64> RandomValues(DU64 du64, Vec<DU64>& s0, Vec<DU64>& s1,
                       const Vec<DU64> mask) {
  const Vec<DU64> bits = Xorshift128Plus::RandomBits(du64, s0, s1);
  return And(bits, mask);
}

// Important to avoid denormals, which are flushed to zero by SIMD but not
// scalar sorts, and NaN, which may be ordered differently in scalar vs. SIMD.
template <typename T, class DU64, HWY_IF_FLOAT(T)>
Vec<DU64> RandomValues(DU64 du64, Vec<DU64>& s0, Vec<DU64>& s1,
                       const Vec<DU64> mask) {
  const Vec<DU64> bits = Xorshift128Plus::RandomBits(du64, s0, s1);
  const Vec<DU64> values = And(bits, mask);
#if HWY_TARGET == HWY_SCALAR  // Cannot repartition u64 to i32
  const RebindToSigned<DU64> di;
#else
  const Repartition<MakeSigned<T>, DU64> di;
#endif
  const RebindToFloat<decltype(di)> df;
  // Avoid NaN/denormal by converting from (range-limited) integer.
  const Vec<DU64> no_nan =
      And(values, Set(du64, MantissaMask<MakeUnsigned<T>>()));
  return BitCast(du64, ConvertTo(df, BitCast(di, no_nan)));
}

template <class DU64>
Vec<DU64> MaskForDist(DU64 du64, const Dist dist, size_t sizeof_t) {
  switch (sizeof_t) {
    case 2:
      return Set(du64, (dist == Dist::kUniform8) ? 0x00FF00FF00FF00FFull
                                                 : 0xFFFFFFFFFFFFFFFFull);
    case 4:
      return Set(du64, (dist == Dist::kUniform8)    ? 0x000000FF000000FFull
                       : (dist == Dist::kUniform16) ? 0x0000FFFF0000FFFFull
                                                    : 0xFFFFFFFFFFFFFFFFull);
    case 8:
      return Set(du64, (dist == Dist::kUniform8)    ? 0x00000000000000FFull
                       : (dist == Dist::kUniform16) ? 0x000000000000FFFFull
                                                    : 0x00000000FFFFFFFFull);
    default:
      HWY_ABORT("Logic error");
      return Zero(du64);
  }
}

template <typename T>
InputStats<T> GenerateInput(const Dist dist, T* v, size_t num) {
  SortTag<uint64_t> du64;
  using VU64 = Vec<decltype(du64)>;
  const size_t N64 = Lanes(du64);
  auto buf = hwy::AllocateAligned<uint64_t>(2 * N64);
  Xorshift128Plus::GenerateSeeds(du64, buf.get());
  auto s0 = Load(du64, buf.get());
  auto s1 = Load(du64, buf.get() + N64);

  const VU64 mask = MaskForDist(du64, dist, sizeof(T));

  const Repartition<T, decltype(du64)> d;
  const size_t N = Lanes(d);
  size_t i = 0;
  for (; i + N <= num; i += N) {
    const VU64 bits = RandomValues<T>(du64, s0, s1, mask);
#if HWY_ARCH_RVV
    // v may not be 64-bit aligned
    StoreU(bits, du64, buf.get());
    memcpy(v + i, buf.get(), N64 * sizeof(uint64_t));
#else
    StoreU(bits, du64, reinterpret_cast<uint64_t*>(v + i));
#endif
  }
  if (i < num) {
    const VU64 bits = RandomValues<T>(du64, s0, s1, mask);
    StoreU(bits, du64, buf.get());
    memcpy(v + i, buf.get(), (num - i) * sizeof(T));
  }

  InputStats<T> input_stats;
  for (size_t i = 0; i < num; ++i) {
    input_stats.Notify(v[i]);
  }
  return input_stats;
}

struct ThreadLocal {
  Sorter sorter;
};

struct SharedState {
#if HAVE_PARALLEL_IPS4O
  ips4o::StdThreadPool pool{
      HWY_MIN(16, static_cast<int>(std::thread::hardware_concurrency() / 2))};
#endif
  std::vector<ThreadLocal> tls{1};
};

template <class Order, typename T>
void Run(Algo algo, T* HWY_RESTRICT inout, size_t num, SharedState& shared,
         size_t thread) {
  using detail::HeapSort;
  using detail::LaneTraits;
  using detail::SharedTraits;

  switch (algo) {
#if HAVE_AVX2SORT
    case Algo::kSEA:
      return avx2::quicksort(inout, static_cast<int>(num));
#endif

#if HAVE_IPS4O
    case Algo::kIPS4O:
      if (Order().IsAscending()) {
        return ips4o::sort(inout, inout + num, std::less<T>());
      } else {
        return ips4o::sort(inout, inout + num, std::greater<T>());
      }
#endif

#if HAVE_PARALLEL_IPS4O
    case Algo::kParallelIPS4O:
      if (Order().IsAscending()) {
        return ips4o::parallel::sort(inout, inout + num, std::less<T>());
      } else {
        return ips4o::parallel::sort(inout, inout + num, std::greater<T>());
      }
#endif

#if HAVE_SORT512
    case Algo::kSort512:
      HWY_ABORT("not supported");
      //    return Sort512::Sort(inout, num);
#endif

#if HAVE_PDQSORT
    case Algo::kPDQ:
      if (Order().IsAscending()) {
        return boost::sort::pdqsort_branchless(inout, inout + num,
                                               std::less<T>());
      } else {
        return boost::sort::pdqsort_branchless(inout, inout + num,
                                               std::greater<T>());
      }
#endif

    case Algo::kStd:
      if (Order().IsAscending()) {
        return std::sort(inout, inout + num, std::less<T>());
      } else {
        return std::sort(inout, inout + num, std::greater<T>());
      }

    case Algo::kVQSort:
      return shared.tls[thread].sorter(inout, num, Order());

    case Algo::kHeap:
      HWY_ASSERT(sizeof(T) < 16);
      if (Order().IsAscending()) {
        const SharedTraits<LaneTraits<detail::OrderAscending>> st;
        return HeapSort(st, inout, num);
      } else {
        const SharedTraits<LaneTraits<detail::OrderDescending>> st;
        return HeapSort(st, inout, num);
      }

    default:
      HWY_ABORT("Not implemented");
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_SORT_ALGO_TOGGLE
