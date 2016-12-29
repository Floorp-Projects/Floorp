/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
//  Unit tests for intelligibility enhancer.
//

#include <math.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_processing/intelligibility/intelligibility_enhancer.h"

namespace webrtc {

namespace {

// Target output for ERB create test. Generated with matlab.
const float kTestCenterFreqs[] = {
    13.169f, 26.965f, 41.423f, 56.577f, 72.461f, 89.113f, 106.57f, 124.88f,
    144.08f, 164.21f, 185.34f, 207.5f,  230.75f, 255.16f, 280.77f, 307.66f,
    335.9f,  365.56f, 396.71f, 429.44f, 463.84f, 500.f};
const float kTestFilterBank[][2] = {{0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.f},
                                    {0.055556f, 0.2f},
                                    {0, 0.2f},
                                    {0, 0.2f},
                                    {0, 0.2f},
                                    {0, 0.2f}};
static_assert(arraysize(kTestCenterFreqs) == arraysize(kTestFilterBank),
              "Test filterbank badly initialized.");

// Target output for gain solving test. Generated with matlab.
const size_t kTestStartFreq = 12;  // Lowest integral frequency for ERBs.
const float kTestZeroVar[] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f,
                              1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f,
                              0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
static_assert(arraysize(kTestCenterFreqs) == arraysize(kTestZeroVar),
              "Variance test data badly initialized.");
const float kTestNonZeroVarLambdaTop[] = {
    1.f,     1.f,     1.f,     1.f,     1.f,     1.f,     1.f,     1.f,
    1.f,     1.f,     1.f,     0.f,     0.f,     0.0351f, 0.0636f, 0.0863f,
    0.1037f, 0.1162f, 0.1236f, 0.1251f, 0.1189f, 0.0993f};
static_assert(arraysize(kTestCenterFreqs) ==
                  arraysize(kTestNonZeroVarLambdaTop),
              "Variance test data badly initialized.");
const float kMaxTestError = 0.005f;

// Enhancer initialization parameters.
const int kSamples = 2000;
const int kSampleRate = 1000;
const int kNumChannels = 1;
const int kFragmentSize = kSampleRate / 100;

}  // namespace

using std::vector;
using intelligibility::VarianceArray;

class IntelligibilityEnhancerTest : public ::testing::Test {
 protected:
  IntelligibilityEnhancerTest()
      : clear_data_(kSamples), noise_data_(kSamples), orig_data_(kSamples) {
    config_.sample_rate_hz = kSampleRate;
    enh_.reset(new IntelligibilityEnhancer(config_));
  }

  bool CheckUpdate(VarianceArray::StepType step_type) {
    config_.sample_rate_hz = kSampleRate;
    config_.var_type = step_type;
    enh_.reset(new IntelligibilityEnhancer(config_));
    float* clear_cursor = &clear_data_[0];
    float* noise_cursor = &noise_data_[0];
    for (int i = 0; i < kSamples; i += kFragmentSize) {
      enh_->AnalyzeCaptureAudio(&noise_cursor, kSampleRate, kNumChannels);
      enh_->ProcessRenderAudio(&clear_cursor, kSampleRate, kNumChannels);
      clear_cursor += kFragmentSize;
      noise_cursor += kFragmentSize;
    }
    for (int i = 0; i < kSamples; i++) {
      if (std::fabs(clear_data_[i] - orig_data_[i]) > kMaxTestError) {
        return true;
      }
    }
    return false;
  }

  IntelligibilityEnhancer::Config config_;
  rtc::scoped_ptr<IntelligibilityEnhancer> enh_;
  vector<float> clear_data_;
  vector<float> noise_data_;
  vector<float> orig_data_;
};

// For each class of generated data, tests that render stream is
// updated when it should be for each variance update method.
TEST_F(IntelligibilityEnhancerTest, TestRenderUpdate) {
  vector<VarianceArray::StepType> step_types;
  step_types.push_back(VarianceArray::kStepInfinite);
  step_types.push_back(VarianceArray::kStepDecaying);
  step_types.push_back(VarianceArray::kStepWindowed);
  step_types.push_back(VarianceArray::kStepBlocked);
  step_types.push_back(VarianceArray::kStepBlockBasedMovingAverage);
  std::fill(noise_data_.begin(), noise_data_.end(), 0.0f);
  std::fill(orig_data_.begin(), orig_data_.end(), 0.0f);
  for (auto step_type : step_types) {
    std::fill(clear_data_.begin(), clear_data_.end(), 0.0f);
    EXPECT_FALSE(CheckUpdate(step_type));
  }
  std::srand(1);
  auto float_rand = []() { return std::rand() * 2.f / RAND_MAX - 1; };
  std::generate(noise_data_.begin(), noise_data_.end(), float_rand);
  for (auto step_type : step_types) {
    EXPECT_FALSE(CheckUpdate(step_type));
  }
  for (auto step_type : step_types) {
    std::generate(clear_data_.begin(), clear_data_.end(), float_rand);
    orig_data_ = clear_data_;
    EXPECT_TRUE(CheckUpdate(step_type));
  }
}

// Tests ERB bank creation, comparing against matlab output.
TEST_F(IntelligibilityEnhancerTest, TestErbCreation) {
  ASSERT_EQ(arraysize(kTestCenterFreqs), enh_->bank_size_);
  for (size_t i = 0; i < enh_->bank_size_; ++i) {
    EXPECT_NEAR(kTestCenterFreqs[i], enh_->center_freqs_[i], kMaxTestError);
    ASSERT_EQ(arraysize(kTestFilterBank[0]), enh_->freqs_);
    for (size_t j = 0; j < enh_->freqs_; ++j) {
      EXPECT_NEAR(kTestFilterBank[i][j], enh_->filter_bank_[i][j],
                  kMaxTestError);
    }
  }
}

// Tests analytic solution for optimal gains, comparing
// against matlab output.
TEST_F(IntelligibilityEnhancerTest, TestSolveForGains) {
  ASSERT_EQ(kTestStartFreq, enh_->start_freq_);
  vector<float> sols(enh_->bank_size_);
  float lambda = -0.001f;
  for (size_t i = 0; i < enh_->bank_size_; i++) {
    enh_->filtered_clear_var_[i] = 0.0f;
    enh_->filtered_noise_var_[i] = 0.0f;
    enh_->rho_[i] = 0.02f;
  }
  enh_->SolveForGainsGivenLambda(lambda, enh_->start_freq_, &sols[0]);
  for (size_t i = 0; i < enh_->bank_size_; i++) {
    EXPECT_NEAR(kTestZeroVar[i], sols[i], kMaxTestError);
  }
  for (size_t i = 0; i < enh_->bank_size_; i++) {
    enh_->filtered_clear_var_[i] = static_cast<float>(i + 1);
    enh_->filtered_noise_var_[i] = static_cast<float>(enh_->bank_size_ - i);
  }
  enh_->SolveForGainsGivenLambda(lambda, enh_->start_freq_, &sols[0]);
  for (size_t i = 0; i < enh_->bank_size_; i++) {
    EXPECT_NEAR(kTestNonZeroVarLambdaTop[i], sols[i], kMaxTestError);
  }
  lambda = -1.0;
  enh_->SolveForGainsGivenLambda(lambda, enh_->start_freq_, &sols[0]);
  for (size_t i = 0; i < enh_->bank_size_; i++) {
    EXPECT_NEAR(kTestZeroVar[i], sols[i], kMaxTestError);
  }
}

}  // namespace webrtc
