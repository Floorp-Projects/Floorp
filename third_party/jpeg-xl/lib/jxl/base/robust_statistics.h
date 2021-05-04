// Copyright (c) the JPEG XL Project
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

#ifndef LIB_JXL_BASE_ROBUST_STATISTICS_H_
#define LIB_JXL_BASE_ROBUST_STATISTICS_H_

// Robust statistics: Mode, Median, MedianAbsoluteDeviation.

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"

namespace jxl {

template <typename T>
T Geomean(const T* items, size_t count) {
  double product = 1.0;
  for (size_t i = 0; i < count; ++i) {
    product *= items[i];
  }
  return static_cast<T>(std::pow(product, 1.0 / count));
}

// Round up for integers
template <class T, typename std::enable_if<
                       std::numeric_limits<T>::is_integer>::type* = nullptr>
inline T Half(T x) {
  return (x + 1) / 2;
}

// Mul is faster than div.
template <class T, typename std::enable_if<
                       !std::numeric_limits<T>::is_integer>::type* = nullptr>
inline T Half(T x) {
  return x * 0.5;
}

// Returns the median value. Side effect: values <= median will appear before,
// values >= median after the middle index.
// Guarantees average speed O(num_values).
template <typename T>
T Median(T* samples, const size_t num_samples) {
  JXL_ASSERT(num_samples != 0);
  std::nth_element(samples, samples + num_samples / 2, samples + num_samples);
  T result = samples[num_samples / 2];
  // If even size, find largest element in the partially sorted vector to
  // use as second element to average with
  if ((num_samples & 1) == 0) {
    T biggest = *std::max_element(samples, samples + num_samples / 2);
    result = Half(result + biggest);
  }
  return result;
}

template <typename T>
T Median(std::vector<T>* samples) {
  return Median(samples->data(), samples->size());
}

template <typename T>
static inline T Median3(const T a, const T b, const T c) {
  return std::max(std::min(a, b), std::min(c, std::max(a, b)));
}

template <typename T>
static inline T Median5(const T a, const T b, const T c, const T d, const T e) {
  return Median3(e, std::max(std::min(a, b), std::min(c, d)),
                 std::min(std::max(a, b), std::max(c, d)));
}

// Returns a robust measure of variability.
template <typename T>
T MedianAbsoluteDeviation(const T* samples, const size_t num_samples,
                          const T median) {
  JXL_ASSERT(num_samples != 0);
  std::vector<T> abs_deviations;
  abs_deviations.reserve(num_samples);
  for (size_t i = 0; i < num_samples; ++i) {
    abs_deviations.push_back(std::abs(samples[i] - median));
  }
  return Median(&abs_deviations);
}

template <typename T>
T MedianAbsoluteDeviation(const std::vector<T>& samples, const T median) {
  return MedianAbsoluteDeviation(samples.data(), samples.size(), median);
}

// Half{Range/Sample}Mode are implementations of "Robust estimators of the mode
// and skewness of continuous data". The mode is less affected by outliers in
// highly-skewed distributions than the median.

// Robust estimator of the mode for data given as sorted values.
// O(N*logN), N=num_values.
class HalfSampleMode {
 public:
  // Returns mode. "sorted" must be in ascending order.
  template <typename T>
  T operator()(const T* const JXL_RESTRICT sorted,
               const size_t num_values) const {
    int64_t center = num_values / 2;
    int64_t width = num_values;

    // Zoom in on modal intervals of decreasing width. Stop before we reach
    // width=1, i.e. single values, for which there is no "slope".
    while (width > 2) {
      // Round up so we can still reach the outer edges of odd widths.
      width = Half(width);

      center = CenterOfIntervalWithMinSlope(sorted, num_values, center, width);
    }

    return sorted[center];  // mode := middle value in modal interval.
  }

 private:
  // Returns center of the densest region [c-radius, c+radius].
  template <typename T>
  static JXL_INLINE int64_t CenterOfIntervalWithMinSlope(
      const T* JXL_RESTRICT sorted, const int64_t total_values,
      const int64_t center, const int64_t width) {
    const int64_t radius = Half(width);

    auto compute_slope = [radius, total_values, sorted](
                             int64_t c, int64_t* actual_center = nullptr) {
      // For symmetry, check 2*radius+1 values, i.e. [min, max].
      const int64_t min = std::max(c - radius, int64_t(0));
      const int64_t max = std::min(c + radius, total_values - 1);
      JXL_ASSERT(min < max);
      JXL_ASSERT(sorted[min] <=
                 sorted[max] + std::numeric_limits<float>::epsilon());
      const float dx = max - min + 1;
      const float slope = (sorted[max] - sorted[min]) / dx;

      if (actual_center != nullptr) {
        // c may be out of bounds, so return center of the clamped bounds.
        *actual_center = Half(min + max);
      }
      return slope;
    };

    // First find min_slope for all centers.
    float min_slope = std::numeric_limits<float>::max();
    for (int64_t c = center - radius; c <= center + radius; ++c) {
      min_slope = std::min(min_slope, compute_slope(c));
    }

    // Candidates := centers with slope ~= min_slope.
    std::vector<int64_t> candidates;
    for (int64_t c = center - radius; c <= center + radius; ++c) {
      int64_t actual_center;
      const float slope = compute_slope(c, &actual_center);
      if (slope <= min_slope * 1.001f) {
        candidates.push_back(actual_center);
      }
    }

    // Keep the median.
    JXL_ASSERT(!candidates.empty());
    if (candidates.size() == 1) return candidates[0];
    return Median(&candidates);
  }
};

// Robust estimator of the mode for data given as a CDF.
// O(N*logN), N=num_bins.
class HalfRangeMode {
 public:
  // Returns mode expressed as a histogram bin index. "cdf" must be weakly
  // monotonically increasing, e.g. from std::partial_sum.
  int operator()(const uint32_t* JXL_RESTRICT cdf,
                 const size_t num_bins) const {
    int center = num_bins / 2;
    int width = num_bins;

    // Zoom in on modal intervals of decreasing width. Stop before we reach
    // width=1, i.e. original bins, because those are noisy.
    while (width > 2) {
      // Round up so we can still reach the outer edges of odd widths.
      width = Half(width);

      center = CenterOfIntervalWithMaxDensity(cdf, num_bins, center, width);
    }

    return center;  // mode := midpoint of modal interval.
  }

 private:
  // Returns center of the densest interval [c-radius, c+radius].
  static JXL_INLINE int CenterOfIntervalWithMaxDensity(
      const uint32_t* JXL_RESTRICT cdf, const int total_bins, const int center,
      const int width) {
    const int radius = Half(width);

    auto compute_density = [radius, total_bins, cdf](
                               int c, int* actual_center = nullptr) {
      // For symmetry, check 2*radius+1 bins, i.e. [min, max].
      const int min = std::max(c - radius, 1);  // for -1 below
      const int max = std::min(c + radius, total_bins - 1);
      JXL_ASSERT(min < max);
      JXL_ASSERT(cdf[min] <= cdf[max - 1]);
      const int num_bins = max - min + 1;
      // Sum over [min, max] == CDF(max) - CDF(min-1).
      const float density = float(cdf[max] - cdf[min - 1]) / num_bins;

      if (actual_center != nullptr) {
        // c may be out of bounds, so take center of the clamped bounds.
        *actual_center = Half(min + max);
      }
      return density;
    };

    // First find max_density for all centers.
    float max_density = 0.0f;
    for (int c = center - radius; c <= center + radius; ++c) {
      max_density = std::max(max_density, compute_density(c));
    }

    // Candidates := centers with density ~= max_density.
    std::vector<int> candidates;
    for (int c = center - radius; c <= center + radius; ++c) {
      int actual_center;
      const float density = compute_density(c, &actual_center);
      if (density >= max_density * 0.999f) {
        candidates.push_back(actual_center);
      }
    }

    // Keep the median.
    JXL_ASSERT(!candidates.empty());
    if (candidates.size() == 1) return candidates[0];
    return Median(&candidates);
  }
};

// Sorts integral values in ascending order. About 3x faster than std::sort for
// input distributions with very few unique values.
template <class T>
void CountingSort(T* begin, T* end) {
  // Unique values and their frequency (similar to flat_map).
  using Unique = std::pair<T, int>;
  std::vector<Unique> unique;
  for (const T* p = begin; p != end; ++p) {
    const T value = *p;
    const auto pos =
        std::find_if(unique.begin(), unique.end(),
                     [value](const Unique& u) { return u.first == value; });
    if (pos == unique.end()) {
      unique.push_back(std::make_pair(*p, 1));
    } else {
      ++pos->second;
    }
  }

  // Sort in ascending order of value (pair.first).
  std::sort(unique.begin(), unique.end());

  // Write that many copies of each unique value to the array.
  T* JXL_RESTRICT p = begin;
  for (const auto& value_count : unique) {
    std::fill(p, p + value_count.second, value_count.first);
    p += value_count.second;
  }
  JXL_ASSERT(p == end);
}

struct Bivariate {
  Bivariate(float x, float y) : x(x), y(y) {}
  float x;
  float y;
};

class Line {
 public:
  constexpr Line(const float slope, const float intercept)
      : slope_(slope), intercept_(intercept) {}

  constexpr float slope() const { return slope_; }
  constexpr float intercept() const { return intercept_; }

  // Robust line fit using Siegel's repeated-median algorithm.
  explicit Line(const std::vector<Bivariate>& points) {
    const size_t N = points.size();
    // This straightforward N^2 implementation is OK for small N.
    JXL_ASSERT(N < 10 * 1000);

    // One for every point i.
    std::vector<float> medians;
    medians.reserve(N);

    // One for every j != i. Never cleared to avoid reallocation.
    std::vector<float> slopes(N - 1);

    for (size_t i = 0; i < N; ++i) {
      // Index within slopes[] (avoids the hole where j == i).
      size_t idx_slope = 0;

      for (size_t j = 0; j < N; ++j) {
        if (j == i) continue;

        const float dy = points[j].y - points[i].y;
        const float dx = points[j].x - points[i].x;
        JXL_ASSERT(std::abs(dx) > 1E-7f);  // x must be distinct
        slopes[idx_slope++] = dy / dx;
      }
      JXL_ASSERT(idx_slope == N - 1);

      const float median = Median(&slopes);
      medians.push_back(median);
    }

    slope_ = Median(&medians);

    // Solve for intercept, overwriting medians[].
    for (size_t i = 0; i < N; ++i) {
      medians[i] = points[i].y - slope_ * points[i].x;
    }
    intercept_ = Median(&medians);
  }

  constexpr float operator()(float x) const { return x * slope_ + intercept_; }

 private:
  float slope_;
  float intercept_;
};

static inline void EvaluateQuality(const Line& line,
                                   const std::vector<Bivariate>& points,
                                   float* JXL_RESTRICT max_l1,
                                   float* JXL_RESTRICT median_abs_deviation) {
  // For computing median_abs_deviation.
  std::vector<float> abs_deviations;
  abs_deviations.reserve(points.size());

  *max_l1 = 0.0f;
  for (const Bivariate& point : points) {
    const float l1 = std::abs(line(point.x) - point.y);
    *max_l1 = std::max(*max_l1, l1);
    abs_deviations.push_back(l1);
  }

  *median_abs_deviation = Median(&abs_deviations);
}

}  // namespace jxl

#endif  // LIB_JXL_BASE_ROBUST_STATISTICS_H_
