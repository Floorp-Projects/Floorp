/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/saturation_protector.h"

#include <algorithm>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr float kInitialMarginDb = 20.f;

float RunOnConstantLevel(int num_iterations,
                         float speech_peak_dbfs,
                         float speech_level_dbfs,
                         SaturationProtector* saturation_protector) {
  float last_margin = saturation_protector->margin_db();
  float max_difference = 0.f;
  for (int i = 0; i < num_iterations; ++i) {
    saturation_protector->UpdateMargin(speech_peak_dbfs, speech_level_dbfs);
    const float new_margin = saturation_protector->margin_db();
    max_difference =
        std::max(max_difference, std::abs(new_margin - last_margin));
    last_margin = new_margin;
    saturation_protector->DebugDumpEstimate();
  }
  return max_difference;
}
}  // namespace

TEST(AutomaticGainController2SaturationProtector, ProtectorShouldNotCrash) {
  ApmDataDumper apm_data_dumper(0);
  SaturationProtector saturation_protector(&apm_data_dumper, kInitialMarginDb);
  saturation_protector.UpdateMargin(/*speech_peak_dbfs=*/-10.f,
                                    /*speech_level_dbfs=*/-20.f);
  static_cast<void>(saturation_protector.margin_db());
  saturation_protector.DebugDumpEstimate();
}

// Check that the estimate converges to the ratio between peaks and
// level estimator values after a while.
TEST(AutomaticGainController2SaturationProtector,
     ProtectorEstimatesCrestRatio) {
  ApmDataDumper apm_data_dumper(0);
  SaturationProtector saturation_protector(&apm_data_dumper, kInitialMarginDb);

  constexpr float kPeakLevel = -20.f;
  const float kCrestFactor = kInitialMarginDb + 1.f;
  const float kSpeechLevel = kPeakLevel - kCrestFactor;
  const float kMaxDifference = 0.5 * std::abs(kInitialMarginDb - kCrestFactor);

  static_cast<void>(RunOnConstantLevel(2000, kPeakLevel, kSpeechLevel,
                                       &saturation_protector));

  EXPECT_NEAR(saturation_protector.margin_db(), kCrestFactor, kMaxDifference);
}

TEST(AutomaticGainController2SaturationProtector, ProtectorChangesSlowly) {
  ApmDataDumper apm_data_dumper(0);
  SaturationProtector saturation_protector(&apm_data_dumper, kInitialMarginDb);

  constexpr float kPeakLevel = -20.f;
  const float kCrestFactor = kInitialMarginDb - 5.f;
  const float kOtherCrestFactor = kInitialMarginDb;
  const float kSpeechLevel = kPeakLevel - kCrestFactor;
  const float kOtherSpeechLevel = kPeakLevel - kOtherCrestFactor;

  constexpr int kNumIterations = 1000;
  float max_difference = RunOnConstantLevel(
      kNumIterations, kPeakLevel, kSpeechLevel, &saturation_protector);

  max_difference =
      std::max(RunOnConstantLevel(kNumIterations, kPeakLevel, kOtherSpeechLevel,
                                  &saturation_protector),
               max_difference);

  constexpr float kMaxChangeSpeedDbPerSecond = 0.5;  // 1 db / 2 seconds.

  EXPECT_LE(max_difference,
            kMaxChangeSpeedDbPerSecond / 1000 * kFrameDurationMs);
}

TEST(AutomaticGainController2SaturationProtector,
     ProtectorAdaptsToDelayedChanges) {
  ApmDataDumper apm_data_dumper(0);
  SaturationProtector saturation_protector(&apm_data_dumper, kInitialMarginDb);

  constexpr int kDelayIterations = kFullBufferSizeMs / kFrameDurationMs;
  constexpr float kInitialSpeechLevelDbfs = -30;
  constexpr float kLaterSpeechLevelDbfs = -15;

  // First run on initial level.
  float max_difference = RunOnConstantLevel(
      kDelayIterations, kInitialSpeechLevelDbfs + kInitialMarginDb,
      kInitialSpeechLevelDbfs, &saturation_protector);

  // Then peak changes, but not RMS.
  max_difference =
      std::max(RunOnConstantLevel(
                   kDelayIterations, kLaterSpeechLevelDbfs + kInitialMarginDb,
                   kInitialSpeechLevelDbfs, &saturation_protector),
               max_difference);

  // Then both change.
  max_difference =
      std::max(RunOnConstantLevel(kDelayIterations,
                                  kLaterSpeechLevelDbfs + kInitialMarginDb,
                                  kLaterSpeechLevelDbfs, &saturation_protector),
               max_difference);

  // The saturation protector expects that the RMS changes roughly
  // 'kFullBufferSizeMs' after peaks change. This is to account for
  // delay introduces by the level estimator. Therefore, the input
  // above is 'normal' and 'expected', and shouldn't influence the
  // margin by much.
  const float total_difference =
      std::abs(saturation_protector.margin_db() - kInitialMarginDb);

  EXPECT_LE(total_difference, 0.05f);
  EXPECT_LE(max_difference, 0.01f);
}

}  // namespace webrtc
