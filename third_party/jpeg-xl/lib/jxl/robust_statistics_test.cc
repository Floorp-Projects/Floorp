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

#include "lib/jxl/base/robust_statistics.h"

#include <stdio.h>

#include <numeric>  // partial_sum
#include <random>

#include "gtest/gtest.h"
#include "lib/jxl/noise_distributions.h"

namespace jxl {
namespace {

TEST(RobustStatisticsTest, TestMode) {
  // Enough to populate bins. We have to sort this many values.
  constexpr size_t kReps = 15000;
  constexpr size_t kBins = 101;

  std::mt19937 rng(65537);

  // Place Poisson mean at 1/10, 2/10 .. 9/10 of the bin range.
  for (int frac = 1; frac < 10; ++frac) {
    printf("===========================frac %d\n", frac);

    NoisePoisson noise(frac * kBins / 10);
    std::vector<float> values;
    values.reserve(kReps);

    uint32_t bins[kBins] = {0};

    std::uniform_real_distribution<float> jitter(-1E-3f, 1E-3f);
    for (size_t rep = 0; rep < kReps; ++rep) {
      // Scale back to integer, add jitter to avoid too many repeated values.
      const float poisson = noise(0.0f, &rng) * 1E3f + jitter(rng);

      values.push_back(poisson);

      const int idx_bin = static_cast<int>(poisson);
      if (idx_bin < static_cast<ssize_t>(kBins)) {
        bins[idx_bin] += 1;
      }  // else skip instead of clamping to avoid bias
    }

    // // Print histogram
    // for (const uint32_t b : bins) {
    //   printf("%u\n", b);
    // }

    // (Smoothed) argmax and median for verification
    float smoothed[kBins];
    smoothed[0] = bins[0];
    smoothed[kBins - 1] = bins[kBins - 1];
    for (size_t i = 1; i < kBins - 1; ++i) {
      smoothed[i] = (2 * bins[i] + bins[i - 1] + bins[i + 1]) * 0.25f;
    }
    const float argmax =
        std::max_element(smoothed, smoothed + kBins) - smoothed;
    const float median = Median(&values);

    std::sort(values.begin(), values.end());
    const float hsm = HalfSampleMode()(values.data(), values.size());

    uint32_t cdf[kBins];
    std::partial_sum(bins, bins + kBins, cdf);
    const int hrm = HalfRangeMode()(cdf, kBins);

    const auto is_near = [](const float expected, const float actual) {
      return std::abs(expected - actual) <= 1.0f + 1E-5f;
    };
    EXPECT_TRUE(is_near(hsm, argmax) || is_near(hsm, median));
    EXPECT_TRUE(is_near(hrm, argmax) || is_near(hrm, median));

    printf("hsm %.1f hrm %d argmax %.1f median %f\n", hsm, hrm, argmax, median);
    const int center = static_cast<int>(argmax);
    printf("%d %d %d %d %d\n", bins[center - 2], bins[center - 1], bins[center],
           bins[center + 1], bins[center + 2]);
  }
}

// Ensures Median3/5 return the same results as Median.
TEST(RobustStatisticsTest, TestMedian) {
  std::vector<float> v3(3), v5(5);

  std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
  std::mt19937 rng(129);

#ifdef NDEBUG
  constexpr size_t kReps = 100000;
#else
  constexpr size_t kReps = 100;
#endif
  for (size_t i = 0; i < kReps; ++i) {
    v3[0] = dist(rng);
    v3[1] = dist(rng);
    v3[2] = dist(rng);
    for (size_t j = 0; j < 5; ++j) {
      v5[j] = dist(rng);
    }

    JXL_ASSERT(Median(&v3) == Median3(v3[0], v3[1], v3[2]));
    JXL_ASSERT(Median(&v5) == Median5(v5[0], v5[1], v5[2], v5[3], v5[4]));
  }
}

template <class Noise>
void TestLine(const Noise& noise, float max_l1_limit, float mad_limit) {
  std::vector<Bivariate> points;
  Line perfect(0.6f, 2.0f);

  // Random spacing of X (must be unique)
  float x = -100.0f;
  std::mt19937_64 rng(129);
  std::uniform_real_distribution<float> x_dist(1E-6f, 10.0f);
  for (size_t ix = 0; ix < 500; ++ix) {
    x += x_dist(rng);
    const float y = noise(perfect(x), &rng);
    points.emplace_back(x, y);
    // printf("%f,%f\n", x, y);
  }

  Line est(points);
  float max_l1, mad;
  EvaluateQuality(est, points, &max_l1, &mad);
  printf("x %f  slope=%.2f b=%.2f  max_l1 %f mad %f\n", x, est.slope(),
         est.intercept(), max_l1, mad);

  EXPECT_LE(max_l1, max_l1_limit);
  EXPECT_LE(mad, mad_limit);
}

TEST(RobustStatisticsTest, CleanLine) {
  const NoiseNone noise;
  TestLine(noise, 1E-6, 1E-7);
}
TEST(RobustStatisticsTest, Uniform) {
  const NoiseUniform noise(-100.0f, 100.0f);
  TestLine(noise, 107, 53);
}
TEST(RobustStatisticsTest, Gauss) {
  const NoiseGaussian noise(10.0f);
  TestLine(noise, 37, 7);
}

}  // namespace
}  // namespace jxl
