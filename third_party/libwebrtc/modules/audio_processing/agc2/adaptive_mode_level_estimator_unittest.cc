/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_mode_level_estimator.h"

#include <memory>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr float kInitialSaturationMarginDb = 20.f;
constexpr float kExtraSaturationMarginDb = 2.f;

void RunOnConstantLevel(int num_iterations,
                        VadWithLevel::LevelAndProbability vad_data,
                        AdaptiveModeLevelEstimator& level_estimator) {
  for (int i = 0; i < num_iterations; ++i) {
    level_estimator.UpdateEstimation(vad_data);  // By copy
  }
}

struct TestLevelEstimator {
  TestLevelEstimator()
      : data_dumper(0),
        estimator(std::make_unique<AdaptiveModeLevelEstimator>(
            &data_dumper,
            AudioProcessing::Config::GainController2::LevelEstimator::kRms,
            /*use_saturation_protector=*/true,
            kInitialSaturationMarginDb,
            kExtraSaturationMarginDb)) {}
  ApmDataDumper data_dumper;
  std::unique_ptr<AdaptiveModeLevelEstimator> estimator;
};

}  // namespace

TEST(AutomaticGainController2AdaptiveModeLevelEstimator,
     EstimatorShouldNotCrash) {
  TestLevelEstimator level_estimator;

  VadWithLevel::LevelAndProbability vad_data(1.f, -20.f, -10.f);
  level_estimator.estimator->UpdateEstimation(vad_data);
  static_cast<void>(level_estimator.estimator->LatestLevelEstimate());
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator, LevelShouldStabilize) {
  TestLevelEstimator level_estimator;

  constexpr float kSpeechPeakDbfs = -15.f;
  RunOnConstantLevel(
      100,
      VadWithLevel::LevelAndProbability(
          1.f, kSpeechPeakDbfs - kInitialSaturationMarginDb, kSpeechPeakDbfs),
      *level_estimator.estimator);

  EXPECT_NEAR(level_estimator.estimator->LatestLevelEstimate() -
                  kExtraSaturationMarginDb,
              kSpeechPeakDbfs, 0.1f);
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator,
     EstimatorIgnoresZeroProbabilityFrames) {
  TestLevelEstimator level_estimator;

  // Run for one second of fake audio.
  constexpr float kSpeechRmsDbfs = -25.f;
  RunOnConstantLevel(
      100,
      VadWithLevel::LevelAndProbability(
          1.f, kSpeechRmsDbfs - kInitialSaturationMarginDb, kSpeechRmsDbfs),
      *level_estimator.estimator);

  // Run for one more second, but mark as not speech.
  constexpr float kNoiseRmsDbfs = 0.f;
  RunOnConstantLevel(
      100, VadWithLevel::LevelAndProbability(0.f, kNoiseRmsDbfs, kNoiseRmsDbfs),
      *level_estimator.estimator);

  // Level should not have changed.
  EXPECT_NEAR(level_estimator.estimator->LatestLevelEstimate() -
                  kExtraSaturationMarginDb,
              kSpeechRmsDbfs, 0.1f);
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator, TimeToAdapt) {
  TestLevelEstimator level_estimator;

  // Run for one 'window size' interval.
  constexpr float kInitialSpeechRmsDbfs = -30.f;
  RunOnConstantLevel(
      kFullBufferSizeMs / kFrameDurationMs,
      VadWithLevel::LevelAndProbability(
          1.f, kInitialSpeechRmsDbfs - kInitialSaturationMarginDb,
          kInitialSpeechRmsDbfs),
      *level_estimator.estimator);

  // Run for one half 'window size' interval. This should not be enough to
  // adapt.
  constexpr float kDifferentSpeechRmsDbfs = -10.f;
  // It should at most differ by 25% after one half 'window size' interval.
  const float kMaxDifferenceDb =
      0.25 * std::abs(kDifferentSpeechRmsDbfs - kInitialSpeechRmsDbfs);
  RunOnConstantLevel(
      static_cast<int>(kFullBufferSizeMs / kFrameDurationMs / 2),
      VadWithLevel::LevelAndProbability(
          1.f, kDifferentSpeechRmsDbfs - kInitialSaturationMarginDb,
          kDifferentSpeechRmsDbfs),
      *level_estimator.estimator);
  EXPECT_GT(std::abs(kDifferentSpeechRmsDbfs -
                     level_estimator.estimator->LatestLevelEstimate()),
            kMaxDifferenceDb);

  // Run for some more time. Afterwards, we should have adapted.
  RunOnConstantLevel(
      static_cast<int>(3 * kFullBufferSizeMs / kFrameDurationMs),
      VadWithLevel::LevelAndProbability(
          1.f, kDifferentSpeechRmsDbfs - kInitialSaturationMarginDb,
          kDifferentSpeechRmsDbfs),
      *level_estimator.estimator);
  EXPECT_NEAR(level_estimator.estimator->LatestLevelEstimate() -
                  kExtraSaturationMarginDb,
              kDifferentSpeechRmsDbfs, kMaxDifferenceDb * 0.5f);
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator,
     ResetGivesFastAdaptation) {
  TestLevelEstimator level_estimator;

  // Run the level estimator for one window size interval. This gives time to
  // adapt.
  constexpr float kInitialSpeechRmsDbfs = -30.f;
  RunOnConstantLevel(
      kFullBufferSizeMs / kFrameDurationMs,
      VadWithLevel::LevelAndProbability(
          1.f, kInitialSpeechRmsDbfs - kInitialSaturationMarginDb,
          kInitialSpeechRmsDbfs),
      *level_estimator.estimator);

  constexpr float kDifferentSpeechRmsDbfs = -10.f;
  // Reset and run one half window size interval.
  level_estimator.estimator->Reset();

  RunOnConstantLevel(
      kFullBufferSizeMs / kFrameDurationMs / 2,
      VadWithLevel::LevelAndProbability(
          1.f, kDifferentSpeechRmsDbfs - kInitialSaturationMarginDb,
          kDifferentSpeechRmsDbfs),
      *level_estimator.estimator);

  // The level should be close to 'kDifferentSpeechRmsDbfs'.
  const float kMaxDifferenceDb =
      0.1f * std::abs(kDifferentSpeechRmsDbfs - kInitialSpeechRmsDbfs);
  EXPECT_LT(std::abs(kDifferentSpeechRmsDbfs -
                     (level_estimator.estimator->LatestLevelEstimate() -
                      kExtraSaturationMarginDb)),
            kMaxDifferenceDb);
}

}  // namespace webrtc
