/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/clipping_predictor.h"

#include <tuple>

#include "rtc_base/checks.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Eq;
using ::testing::Optional;

constexpr int kSampleRateHz = 32000;
constexpr int kNumChannels = 1;
constexpr int kSamplesPerChannel = kSampleRateHz / 100;
constexpr int kWindowLength = 5;
constexpr int kReferenceWindowLength = 5;
constexpr int kReferenceWindowDelay = 5;
constexpr int kMaxMicLevel = 255;
constexpr int kMinMicLevel = 12;
constexpr int kDefaultClippedLevelStep = 15;

using ClippingPredictorConfig = AudioProcessing::Config::GainController1 ::
    AnalogGainController::ClippingPredictor;

void CallProcess(int num_calls,
                 const AudioFrameView<const float>& frame,
                 ClippingPredictor& predictor) {
  for (int i = 0; i < num_calls; ++i) {
    predictor.Process(frame);
  }
}

// Creates and processes an audio frame with a non-zero (approx. 4.15dB) crest
// factor.
void ProcessNonZeroCrestFactorAudio(int num_calls,
                                    int num_channels,
                                    float peak_ratio,
                                    ClippingPredictor& predictor) {
  RTC_DCHECK_GT(num_calls, 0);
  RTC_DCHECK_GT(num_channels, 0);
  RTC_DCHECK_LE(peak_ratio, 1.f);
  std::vector<float*> audio(num_channels);
  std::vector<float> audio_data(num_channels * kSamplesPerChannel, 0.f);
  for (int channel = 0; channel < num_channels; ++channel) {
    audio[channel] = &audio_data[channel * kSamplesPerChannel];
    for (int sample = 0; sample < kSamplesPerChannel; sample += 10) {
      audio[channel][sample] = 0.1f * peak_ratio * 32767.f;
      audio[channel][sample + 1] = 0.2f * peak_ratio * 32767.f;
      audio[channel][sample + 2] = 0.3f * peak_ratio * 32767.f;
      audio[channel][sample + 3] = 0.4f * peak_ratio * 32767.f;
      audio[channel][sample + 4] = 0.5f * peak_ratio * 32767.f;
      audio[channel][sample + 5] = 0.6f * peak_ratio * 32767.f;
      audio[channel][sample + 6] = 0.7f * peak_ratio * 32767.f;
      audio[channel][sample + 7] = 0.8f * peak_ratio * 32767.f;
      audio[channel][sample + 8] = 0.9f * peak_ratio * 32767.f;
      audio[channel][sample + 9] = 1.f * peak_ratio * 32767.f;
    }
  }
  auto frame = AudioFrameView<const float>(audio.data(), num_channels,
                                           kSamplesPerChannel);
  CallProcess(num_calls, frame, predictor);
}

void CheckChannelEstimatesWithValue(int num_channels,
                                    int level,
                                    int default_step,
                                    int min_mic_level,
                                    int max_mic_level,
                                    const ClippingPredictor& predictor,
                                    int expected) {
  for (int i = 0; i < num_channels; ++i) {
    EXPECT_THAT(predictor.EstimateClippedLevelStep(
                    i, level, default_step, min_mic_level, max_mic_level),
                Optional(Eq(expected)));
  }
}

void CheckChannelEstimatesWithoutValue(int num_channels,
                                       int level,
                                       int default_step,
                                       int min_mic_level,
                                       int max_mic_level,
                                       const ClippingPredictor& predictor) {
  for (int i = 0; i < num_channels; ++i) {
    EXPECT_EQ(predictor.EstimateClippedLevelStep(i, level, default_step,
                                                 min_mic_level, max_mic_level),
              absl::nullopt);
  }
}

// Creates and processes an audio frame with a zero crest factor.
void ProcessZeroCrestFactorAudio(int num_calls,
                                 int num_channels,
                                 float peak_ratio,
                                 ClippingPredictor& predictor) {
  RTC_DCHECK_GT(num_calls, 0);
  RTC_DCHECK_GT(num_channels, 0);
  RTC_DCHECK_LE(peak_ratio, 1.f);
  std::vector<float*> audio(num_channels);
  std::vector<float> audio_data(num_channels * kSamplesPerChannel, 0.f);
  for (int channel = 0; channel < num_channels; ++channel) {
    audio[channel] = &audio_data[channel * kSamplesPerChannel];
    for (int sample = 0; sample < kSamplesPerChannel; ++sample) {
      audio[channel][sample] = peak_ratio * 32767.f;
    }
  }
  auto frame = AudioFrameView<const float>(audio.data(), num_channels,
                                           kSamplesPerChannel);
  CallProcess(num_calls, frame, predictor);
}

class ClippingPredictorParameterization
    : public ::testing::TestWithParam<std::tuple<int, int, int, int>> {
 protected:
  int num_channels() const { return std::get<0>(GetParam()); }
  int window_length() const { return std::get<1>(GetParam()); }
  int reference_window_length() const { return std::get<2>(GetParam()); }
  int reference_window_delay() const { return std::get<3>(GetParam()); }
};

class ClippingEventPredictorParameterization
    : public ::testing::TestWithParam<std::tuple<float, float>> {
 protected:
  float clipping_threshold() const { return std::get<0>(GetParam()); }
  float crest_factor_margin() const { return std::get<1>(GetParam()); }
};

class ClippingPeakPredictorParameterization
    : public ::testing::TestWithParam<std::tuple<bool, float>> {
 protected:
  float adaptive_step_estimation() const { return std::get<0>(GetParam()); }
  float clipping_threshold() const { return std::get<1>(GetParam()); }
};

TEST_P(ClippingPredictorParameterization,
       CheckClippingEventPredictorEstimateAfterCrestFactorDrop) {
  if (reference_window_length() + reference_window_delay() > window_length()) {
    ClippingPredictorConfig config;
    config.window_length = window_length();
    config.reference_window_length = reference_window_length();
    config.reference_window_delay = reference_window_delay();
    config.clipping_threshold = -1.0f;
    config.crest_factor_margin = 0.5f;
    auto predictor = CreateClippingEventPredictor(num_channels(), config);
    ProcessNonZeroCrestFactorAudio(
        reference_window_length() + reference_window_delay() - window_length(),
        num_channels(), /*peak_ratio=*/0.99f, *predictor);
    CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                      kDefaultClippedLevelStep, kMinMicLevel,
                                      kMaxMicLevel, *predictor);
    ProcessZeroCrestFactorAudio(window_length(), num_channels(),
                                /*peak_ratio=*/0.99f, *predictor);
    CheckChannelEstimatesWithValue(
        num_channels(), /*level=*/255, kDefaultClippedLevelStep, kMinMicLevel,
        kMaxMicLevel, *predictor, kDefaultClippedLevelStep);
  }
}

TEST_P(ClippingPredictorParameterization,
       CheckClippingEventPredictorNoEstimateAfterConstantCrestFactor) {
  if (reference_window_length() + reference_window_delay() > window_length()) {
    ClippingPredictorConfig config;
    config.window_length = window_length();
    config.reference_window_length = reference_window_length();
    config.reference_window_delay = reference_window_delay();
    config.clipping_threshold = -1.0f;
    config.crest_factor_margin = 0.5f;
    auto predictor = CreateClippingEventPredictor(num_channels(), config);
    ProcessNonZeroCrestFactorAudio(
        reference_window_length() + reference_window_delay() - window_length(),
        num_channels(), /*peak_ratio=*/0.99f, *predictor);
    CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                      kDefaultClippedLevelStep, kMinMicLevel,
                                      kMaxMicLevel, *predictor);
    ProcessNonZeroCrestFactorAudio(window_length(), num_channels(),
                                   /*peak_ratio=*/0.99f, *predictor);
    CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                      kDefaultClippedLevelStep, kMinMicLevel,
                                      kMaxMicLevel, *predictor);
  }
}

TEST_P(ClippingPredictorParameterization,
       CheckClippingPeakPredictorEstimateAfterHighCrestFactor) {
  if (reference_window_length() + reference_window_delay() > window_length()) {
    ClippingPredictorConfig config;
    config.window_length = window_length();
    config.reference_window_length = reference_window_length();
    config.reference_window_delay = reference_window_delay();
    config.clipping_threshold = -1.0f;
    auto predictor =
        CreateAdaptiveStepClippingPeakPredictor(num_channels(), config);
    ProcessNonZeroCrestFactorAudio(
        reference_window_length() + reference_window_delay() - window_length(),
        num_channels(), /*peak_ratio=*/0.99f, *predictor);
    CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                      kDefaultClippedLevelStep, kMinMicLevel,
                                      kMaxMicLevel, *predictor);
    ProcessNonZeroCrestFactorAudio(window_length(), num_channels(),
                                   /*peak_ratio=*/0.99f, *predictor);
    CheckChannelEstimatesWithValue(
        num_channels(), /*level=*/255, kDefaultClippedLevelStep, kMinMicLevel,
        kMaxMicLevel, *predictor, kDefaultClippedLevelStep);
  }
}

TEST_P(ClippingPredictorParameterization,
       CheckClippingPeakPredictorNoEstimateAfterLowCrestFactor) {
  if (reference_window_length() + reference_window_delay() > window_length()) {
    ClippingPredictorConfig config;
    config.window_length = window_length();
    config.reference_window_length = reference_window_length();
    config.reference_window_delay = reference_window_delay();
    config.clipping_threshold = -1.0f;
    auto predictor =
        CreateAdaptiveStepClippingPeakPredictor(num_channels(), config);
    ProcessZeroCrestFactorAudio(
        reference_window_length() + reference_window_delay() - window_length(),
        num_channels(), /*peak_ratio=*/0.99f, *predictor);
    CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                      kDefaultClippedLevelStep, kMinMicLevel,
                                      kMaxMicLevel, *predictor);
    ProcessNonZeroCrestFactorAudio(window_length(), num_channels(),
                                   /*peak_ratio=*/0.99f, *predictor);
    CheckChannelEstimatesWithoutValue(num_channels(), /*level=*/255,
                                      kDefaultClippedLevelStep, kMinMicLevel,
                                      kMaxMicLevel, *predictor);
  }
}

INSTANTIATE_TEST_SUITE_P(GainController1ClippingPredictor,
                         ClippingPredictorParameterization,
                         ::testing::Combine(::testing::Values(1, 5),
                                            ::testing::Values(1, 5, 10),
                                            ::testing::Values(1, 5),
                                            ::testing::Values(0, 1, 5)));

TEST_P(ClippingEventPredictorParameterization,
       CheckEstimateAfterCrestFactorDrop) {
  ClippingPredictorConfig config;
  config.window_length = kWindowLength;
  config.reference_window_length = kReferenceWindowLength;
  config.reference_window_delay = kReferenceWindowDelay;
  config.clipping_threshold = clipping_threshold();
  config.crest_factor_margin = crest_factor_margin();
  auto predictor = CreateClippingEventPredictor(kNumChannels, config);
  ProcessNonZeroCrestFactorAudio(kReferenceWindowLength, kNumChannels,
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  ProcessZeroCrestFactorAudio(kWindowLength, kNumChannels, /*peak_ratio=*/0.99f,
                              *predictor);
  if (clipping_threshold() < 20 * std::log10f(0.99f) &&
      crest_factor_margin() < 4.15f) {
    CheckChannelEstimatesWithValue(
        kNumChannels, /*level=*/255, kDefaultClippedLevelStep, kMinMicLevel,
        kMaxMicLevel, *predictor, kDefaultClippedLevelStep);
  } else {
    CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                      kDefaultClippedLevelStep, kMinMicLevel,
                                      kMaxMicLevel, *predictor);
  }
}

INSTANTIATE_TEST_SUITE_P(GainController1ClippingPredictor,
                         ClippingEventPredictorParameterization,
                         ::testing::Combine(::testing::Values(-1.0f, 0.0f),
                                            ::testing::Values(3.0f, 4.16f)));

TEST_P(ClippingPeakPredictorParameterization,
       CheckEstimateAfterHighCrestFactor) {
  ClippingPredictorConfig config;
  config.window_length = kWindowLength;
  config.reference_window_length = kReferenceWindowLength;
  config.reference_window_delay = kReferenceWindowDelay;
  config.clipping_threshold = clipping_threshold();
  auto predictor =
      adaptive_step_estimation()
          ? CreateAdaptiveStepClippingPeakPredictor(kNumChannels, config)
          : CreateFixedStepClippingPeakPredictor(kNumChannels, config);
  ProcessNonZeroCrestFactorAudio(kReferenceWindowLength, kNumChannels,
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  ProcessZeroCrestFactorAudio(kWindowLength, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  if (clipping_threshold() < 20 * std::log10(0.99f)) {
    if (adaptive_step_estimation()) {
      CheckChannelEstimatesWithValue(kNumChannels, /*level=*/255,
                                     kDefaultClippedLevelStep, kMinMicLevel,
                                     kMaxMicLevel, *predictor,
                                     /*expected=*/17);
    } else {
      CheckChannelEstimatesWithValue(
          kNumChannels, /*level=*/255, kDefaultClippedLevelStep, kMinMicLevel,
          kMaxMicLevel, *predictor, kDefaultClippedLevelStep);
    }
  } else {
    CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                      kDefaultClippedLevelStep, kMinMicLevel,
                                      kMaxMicLevel, *predictor);
  }
}

INSTANTIATE_TEST_SUITE_P(GainController1ClippingPredictor,
                         ClippingPeakPredictorParameterization,
                         ::testing::Combine(::testing::Values(true, false),
                                            ::testing::Values(-1.0f, 0.0f)));

TEST(ClippingEventPredictorTest, CheckEstimateAfterReset) {
  ClippingPredictorConfig config;
  config.window_length = kWindowLength;
  config.reference_window_length = kReferenceWindowLength;
  config.reference_window_delay = kReferenceWindowDelay;
  config.clipping_threshold = -1.0f;
  config.crest_factor_margin = 3.0f;
  auto predictor = CreateClippingEventPredictor(kNumChannels, config);
  ProcessNonZeroCrestFactorAudio(kReferenceWindowLength, kNumChannels,
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  predictor->Reset();
  ProcessZeroCrestFactorAudio(kWindowLength, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
}

TEST(ClippingPeakPredictorTest, CheckNoEstimateAfterReset) {
  ClippingPredictorConfig config;
  config.window_length = kWindowLength;
  config.reference_window_length = kReferenceWindowLength;
  config.reference_window_delay = kReferenceWindowDelay;
  config.clipping_threshold = -1.0f;
  config.crest_factor_margin = 3.0f;
  auto predictor =
      CreateAdaptiveStepClippingPeakPredictor(kNumChannels, config);
  ProcessNonZeroCrestFactorAudio(kReferenceWindowLength, kNumChannels,
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  predictor->Reset();
  ProcessZeroCrestFactorAudio(kWindowLength, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
}

TEST(ClippingPeakPredictorTest, CheckAdaptiveStepEstimate) {
  ClippingPredictorConfig config;
  config.window_length = kWindowLength;
  config.reference_window_length = kReferenceWindowLength;
  config.reference_window_delay = kReferenceWindowDelay;
  config.clipping_threshold = -1.0f;
  auto predictor =
      CreateAdaptiveStepClippingPeakPredictor(kNumChannels, config);
  ProcessNonZeroCrestFactorAudio(kReferenceWindowLength, kNumChannels,
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  ProcessZeroCrestFactorAudio(kWindowLength, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithValue(kNumChannels, /*level=*/255,
                                 kDefaultClippedLevelStep, kMinMicLevel,
                                 kMaxMicLevel, *predictor, /*expected=*/17);
}

TEST(ClippingPeakPredictorTest, CheckFixedStepEstimate) {
  ClippingPredictorConfig config;
  config.window_length = kWindowLength;
  config.reference_window_length = kReferenceWindowLength;
  config.reference_window_delay = kReferenceWindowDelay;
  config.clipping_threshold = -1.0f;
  auto predictor = CreateFixedStepClippingPeakPredictor(kNumChannels, config);
  ProcessNonZeroCrestFactorAudio(kReferenceWindowLength, kNumChannels,
                                 /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithoutValue(kNumChannels, /*level=*/255,
                                    kDefaultClippedLevelStep, kMinMicLevel,
                                    kMaxMicLevel, *predictor);
  ProcessZeroCrestFactorAudio(kWindowLength, kNumChannels,
                              /*peak_ratio=*/0.99f, *predictor);
  CheckChannelEstimatesWithValue(
      kNumChannels, /*level=*/255, kDefaultClippedLevelStep, kMinMicLevel,
      kMaxMicLevel, *predictor, kDefaultClippedLevelStep);
}

}  // namespace
}  // namespace webrtc
