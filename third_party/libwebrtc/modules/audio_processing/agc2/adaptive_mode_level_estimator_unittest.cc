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

constexpr float kMinSpeechProbability = 0.f;
constexpr float kMaxSpeechProbability = 1.f;

void RunOnConstantLevel(int num_iterations,
                        const VadLevelAnalyzer::Result& vad_level,
                        AdaptiveModeLevelEstimator& level_estimator) {
  for (int i = 0; i < num_iterations; ++i) {
    level_estimator.Update(vad_level);
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

  VadLevelAnalyzer::Result vad_level{kMaxSpeechProbability, /*rms_dbfs=*/-20.f,
                                     /*peak_dbfs=*/-10.f};
  level_estimator.estimator->Update(vad_level);
  static_cast<void>(level_estimator.estimator->GetLevelDbfs());
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator, LevelShouldStabilize) {
  TestLevelEstimator level_estimator;

  constexpr float kSpeechPeakDbfs = -15.f;
  RunOnConstantLevel(100,
                     VadLevelAnalyzer::Result{kMaxSpeechProbability,
                                              /*rms_dbfs=*/kSpeechPeakDbfs -
                                                  kInitialSaturationMarginDb,
                                              kSpeechPeakDbfs},
                     *level_estimator.estimator);

  EXPECT_NEAR(
      level_estimator.estimator->GetLevelDbfs() - kExtraSaturationMarginDb,
      kSpeechPeakDbfs, 0.1f);
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator,
     EstimatorIgnoresZeroProbabilityFrames) {
  TestLevelEstimator level_estimator;

  // Run for one second of fake audio.
  constexpr float kSpeechRmsDbfs = -25.f;
  RunOnConstantLevel(100,
                     VadLevelAnalyzer::Result{kMaxSpeechProbability,
                                              /*rms_dbfs=*/kSpeechRmsDbfs -
                                                  kInitialSaturationMarginDb,
                                              /*peak_dbfs=*/kSpeechRmsDbfs},
                     *level_estimator.estimator);

  // Run for one more second, but mark as not speech.
  constexpr float kNoiseRmsDbfs = 0.f;
  RunOnConstantLevel(100,
                     VadLevelAnalyzer::Result{kMinSpeechProbability,
                                              /*rms_dbfs=*/kNoiseRmsDbfs,
                                              /*peak_dbfs=*/kNoiseRmsDbfs},
                     *level_estimator.estimator);

  // Level should not have changed.
  EXPECT_NEAR(
      level_estimator.estimator->GetLevelDbfs() - kExtraSaturationMarginDb,
      kSpeechRmsDbfs, 0.1f);
}

TEST(AutomaticGainController2AdaptiveModeLevelEstimator, TimeToAdapt) {
  TestLevelEstimator level_estimator;

  // Run for one 'window size' interval.
  constexpr float kInitialSpeechRmsDbfs = -30.f;
  RunOnConstantLevel(
      kFullBufferSizeMs / kFrameDurationMs,
      VadLevelAnalyzer::Result{
          kMaxSpeechProbability,
          /*rms_dbfs=*/kInitialSpeechRmsDbfs - kInitialSaturationMarginDb,
          /*peak_dbfs=*/kInitialSpeechRmsDbfs},
      *level_estimator.estimator);

  // Run for one half 'window size' interval. This should not be enough to
  // adapt.
  constexpr float kDifferentSpeechRmsDbfs = -10.f;
  // It should at most differ by 25% after one half 'window size' interval.
  const float kMaxDifferenceDb =
      0.25f * std::abs(kDifferentSpeechRmsDbfs - kInitialSpeechRmsDbfs);
  RunOnConstantLevel(
      static_cast<int>(kFullBufferSizeMs / kFrameDurationMs / 2),
      VadLevelAnalyzer::Result{
          kMaxSpeechProbability,
          /*rms_dbfs=*/kDifferentSpeechRmsDbfs - kInitialSaturationMarginDb,
          /*peak_dbfs=*/kDifferentSpeechRmsDbfs},
      *level_estimator.estimator);
  EXPECT_GT(std::abs(kDifferentSpeechRmsDbfs -
                     level_estimator.estimator->GetLevelDbfs()),
            kMaxDifferenceDb);

  // Run for some more time. Afterwards, we should have adapted.
  RunOnConstantLevel(
      static_cast<int>(3 * kFullBufferSizeMs / kFrameDurationMs),
      VadLevelAnalyzer::Result{
          kMaxSpeechProbability,
          /*rms_dbfs=*/kDifferentSpeechRmsDbfs - kInitialSaturationMarginDb,
          /*peak_dbfs=*/kDifferentSpeechRmsDbfs},
      *level_estimator.estimator);
  EXPECT_NEAR(
      level_estimator.estimator->GetLevelDbfs() - kExtraSaturationMarginDb,
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
      VadLevelAnalyzer::Result{
          kMaxSpeechProbability,
          /*rms_dbfs=*/kInitialSpeechRmsDbfs - kInitialSaturationMarginDb,
          /*peak_dbfs=*/kInitialSpeechRmsDbfs},
      *level_estimator.estimator);

  constexpr float kDifferentSpeechRmsDbfs = -10.f;
  // Reset and run one half window size interval.
  level_estimator.estimator->Reset();

  RunOnConstantLevel(
      kFullBufferSizeMs / kFrameDurationMs / 2,
      VadLevelAnalyzer::Result{
          kMaxSpeechProbability,
          /*rms_dbfs=*/kDifferentSpeechRmsDbfs - kInitialSaturationMarginDb,
          /*peak_dbfs=*/kDifferentSpeechRmsDbfs},
      *level_estimator.estimator);

  // The level should be close to 'kDifferentSpeechRmsDbfs'.
  const float kMaxDifferenceDb =
      0.1f * std::abs(kDifferentSpeechRmsDbfs - kInitialSpeechRmsDbfs);
  EXPECT_LT(std::abs(kDifferentSpeechRmsDbfs -
                     (level_estimator.estimator->GetLevelDbfs() -
                      kExtraSaturationMarginDb)),
            kMaxDifferenceDb);
}

}  // namespace webrtc
