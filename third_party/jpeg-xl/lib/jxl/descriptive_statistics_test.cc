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

#include "lib/jxl/base/descriptive_statistics.h"

#include <stdio.h>
#include <stdlib.h>

#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/noise_distributions.h"

namespace jxl {
namespace {

// Assigns x to one of two streams so we can later test Assimilate.
template <typename Random>
void NotifyEither(float x, Random* rng, Stats* JXL_RESTRICT stats1,
                  Stats* JXL_RESTRICT stats2) {
  if ((*rng)() & 128) {
    stats1->Notify(x);
  } else {
    stats2->Notify(x);
  }
}

TEST(StatsTest, TestGaussian) {
  Stats stats;
  Stats stats1, stats2;
  const float mean = 5.0f;
  const float stddev = 4.0f;
  NoiseGaussian noise(stddev);
  std::mt19937 rng(129);
  for (size_t i = 0; i < 1000 * 1000; ++i) {
    const float x = noise(mean, &rng);
    stats.Notify(x);
    NotifyEither(x, &rng, &stats1, &stats2);
  }
  EXPECT_NEAR(mean, stats.Mean(), 0.01);
  EXPECT_NEAR(stddev, stats.StandardDeviation(), 0.02);
  EXPECT_NEAR(0.0, stats.Skewness(), 0.02);
  EXPECT_NEAR(0.0, stats.Kurtosis() - 3, 0.02);
  printf("%s\n", stats.ToString().c_str());

  // Same results after merging both accumulators.
  stats1.Assimilate(stats2);
  EXPECT_NEAR(mean, stats1.Mean(), 0.01);
  EXPECT_NEAR(stddev, stats1.StandardDeviation(), 0.02);
  EXPECT_NEAR(0.0, stats1.Skewness(), 0.02);
  EXPECT_NEAR(0.0, stats1.Kurtosis() - 3, 0.02);
}

TEST(StatsTest, TestUniform) {
  Stats stats;
  Stats stats1, stats2;
  NoiseUniform noise(0, 256);
  std::mt19937 rng(129), rng_split(65537);
  for (size_t i = 0; i < 1000 * 1000; ++i) {
    const float x = noise(0.0f, &rng);
    stats.Notify(x);
    NotifyEither(x, &rng_split, &stats1, &stats2);
  }
  EXPECT_NEAR(128.0, stats.Mean(), 0.05);
  EXPECT_NEAR(0.0, stats.Min(), 0.01);
  EXPECT_NEAR(256.0, stats.Max(), 0.01);
  EXPECT_NEAR(70, stats.StandardDeviation(), 10);
  // No outliers.
  EXPECT_NEAR(-1.2, stats.Kurtosis() - 3, 0.1);
  printf("%s\n", stats.ToString().c_str());

  // Same results after merging both accumulators.
  stats1.Assimilate(stats2);
  EXPECT_NEAR(128.0, stats1.Mean(), 0.05);
  EXPECT_NEAR(0.0, stats1.Min(), 0.01);
  EXPECT_NEAR(256.0, stats1.Max(), 0.01);
  EXPECT_NEAR(70, stats1.StandardDeviation(), 10);
}

TEST(StatsTest, CompareCentralMomentsAgainstTwoPass) {
  // Vary seed so the thresholds are not specific to one distribution.
  for (int rep = 0; rep < 200; ++rep) {
    // Uniform avoids outliers.
    NoiseUniform noise(0, 256);
    std::mt19937 rng(129 + 13 * rep), rng_split(65537);

    // Small count so bias (population vs sample) is visible.
    const size_t kSamples = 20;

    // First pass: compute mean
    std::vector<float> samples;
    samples.reserve(kSamples);
    double sum = 0.0;
    for (size_t i = 0; i < kSamples; ++i) {
      const float x = noise(0.0f, &rng);
      samples.push_back(x);
      sum += x;
    }
    const double mean = sum / kSamples;

    // Second pass: compute stats and moments
    Stats stats;
    Stats stats1, stats2;
    double sum2 = 0.0;
    double sum3 = 0.0;
    double sum4 = 0.0;
    for (const double x : samples) {
      const double d = x - mean;
      sum2 += d * d;
      sum3 += d * d * d;
      sum4 += d * d * d * d;

      stats.Notify(x);
      NotifyEither(x, &rng_split, &stats1, &stats2);
    }
    const double mu1 = mean;
    const double mu2 = sum2 / kSamples;
    const double mu3 = sum3 / kSamples;
    const double mu4 = sum4 / kSamples;

    // Raw central moments (note: Mu1 is zero by definition)
    EXPECT_NEAR(mu1, stats.Mu1(), 1E-13);
    EXPECT_NEAR(mu2, stats.Mu2(), 1E-11);
    EXPECT_NEAR(mu3, stats.Mu3(), 1E-9);
    EXPECT_NEAR(mu4, stats.Mu4(), 1E-6);

    // Same results after merging both accumulators.
    stats1.Assimilate(stats2);
    EXPECT_NEAR(mu1, stats1.Mu1(), 1E-13);
    EXPECT_NEAR(mu2, stats1.Mu2(), 1E-11);
    EXPECT_NEAR(mu3, stats1.Mu3(), 1E-9);
    EXPECT_NEAR(mu4, stats1.Mu4(), 1E-6);

    const double sample_variance = mu2;
    // Scaling factor for sampling bias
    const double r = (kSamples - 1.0) / kSamples;
    const double skewness = mu3 * pow(r / mu2, 1.5);
    const double kurtosis = mu4 * pow(r / mu2, 2.0);

    EXPECT_NEAR(sample_variance, stats.SampleVariance(),
                sample_variance * 1E-12);
    EXPECT_NEAR(skewness, stats.Skewness(), std::abs(skewness * 1E-11));
    EXPECT_NEAR(kurtosis, stats.Kurtosis(), kurtosis * 1E-12);
  }
}

}  // namespace
}  // namespace jxl
