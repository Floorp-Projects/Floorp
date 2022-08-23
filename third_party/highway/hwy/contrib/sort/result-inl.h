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

#include "hwy/contrib/sort/algo-inl.h"

// Normal include guard for non-SIMD parts
#ifndef HIGHWAY_HWY_CONTRIB_SORT_RESULT_INL_H_
#define HIGHWAY_HWY_CONTRIB_SORT_RESULT_INL_H_

#include <time.h>

#include <algorithm>  // std::sort
#include <string>

#include "hwy/base.h"
#include "hwy/nanobenchmark.h"

namespace hwy {

struct Timestamp {
  Timestamp() { t = platform::Now(); }
  double t;
};

double SecondsSince(const Timestamp& t0) {
  const Timestamp t1;
  return t1.t - t0.t;
}

constexpr size_t kReps = 30;

// Returns trimmed mean (we don't want to run an out-of-L3-cache sort often
// enough for the mode to be reliable).
double SummarizeMeasurements(std::vector<double>& seconds) {
  std::sort(seconds.begin(), seconds.end());
  double sum = 0;
  int count = 0;
  for (size_t i = kReps / 4; i < seconds.size() - kReps / 2; ++i) {
    sum += seconds[i];
    count += 1;
  }
  return sum / count;
}

}  // namespace hwy
#endif  // HIGHWAY_HWY_CONTRIB_SORT_RESULT_INL_H_

// Per-target
#if defined(HIGHWAY_HWY_CONTRIB_SORT_RESULT_TOGGLE) == \
    defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CONTRIB_SORT_RESULT_TOGGLE
#undef HIGHWAY_HWY_CONTRIB_SORT_RESULT_TOGGLE
#else
#define HIGHWAY_HWY_CONTRIB_SORT_RESULT_TOGGLE
#endif

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

struct Result {
  Result() {}
  Result(const uint32_t target, const Algo algo, Dist dist, bool is128,
         size_t num, size_t num_threads, double sec, size_t sizeof_t,
         const char* type_name)
      : target(target),
        algo(algo),
        dist(dist),
        is128(is128),
        num(num),
        num_threads(num_threads),
        sec(sec),
        sizeof_t(sizeof_t),
        type_name(type_name) {}

  void Print() const {
    const double bytes = static_cast<double>(num) *
                         static_cast<double>(num_threads) *
                         static_cast<double>(sizeof_t);
    printf("%10s: %12s: %7s: %9s: %.2E %4.0f MB/s (%2zu threads)\n",
           hwy::TargetName(target), AlgoName(algo),
           is128 ? "u128" : type_name.c_str(), DistName(dist),
           static_cast<double>(num), bytes * 1E-6 / sec, num_threads);
  }

  uint32_t target;
  Algo algo;
  Dist dist;
  bool is128;
  size_t num = 0;
  size_t num_threads = 0;
  double sec = 0.0;
  size_t sizeof_t = 0;
  std::string type_name;
};

template <typename T, class Traits>
Result MakeResult(const Algo algo, Dist dist, Traits st, size_t num,
                  size_t num_threads, double sec) {
  char string100[100];
  hwy::detail::TypeName(hwy::detail::MakeTypeInfo<T>(), 1, string100);
  return Result(HWY_TARGET, algo, dist, st.Is128(), num, num_threads, sec,
                sizeof(T), string100);
}

template <class Traits, typename T>
bool VerifySort(Traits st, const InputStats<T>& input_stats, const T* out,
                size_t num, const char* caller) {
  constexpr size_t N1 = st.Is128() ? 2 : 1;
  HWY_ASSERT(num >= N1);

  InputStats<T> output_stats;
  // Ensure it matches the sort order
  for (size_t i = 0; i < num - N1; i += N1) {
    output_stats.Notify(out[i]);
    if (N1 == 2) output_stats.Notify(out[i + 1]);
    // Reverse order instead of checking !Compare1 so we accept equal keys.
    if (st.Compare1(out + i + N1, out + i)) {
      printf("%s: i=%d of %d: N1=%d %5.0f %5.0f vs. %5.0f %5.0f\n\n", caller,
             static_cast<int>(i), static_cast<int>(num), static_cast<int>(N1),
             double(out[i + 1]), double(out[i + 0]), double(out[i + N1 + 1]),
             double(out[i + N1]));
      HWY_ABORT("%d-bit sort is incorrect\n",
                static_cast<int>(sizeof(T) * 8 * N1));
    }
  }
  output_stats.Notify(out[num - N1]);
  if (N1 == 2) output_stats.Notify(out[num - N1 + 1]);

  return input_stats == output_stats;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_SORT_RESULT_TOGGLE
