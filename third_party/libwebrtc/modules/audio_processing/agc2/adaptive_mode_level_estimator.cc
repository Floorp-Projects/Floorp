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

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {

float AdaptiveModeLevelEstimator::State::Ratio::GetRatio() const {
  RTC_DCHECK_NE(denominator, 0.f);
  return numerator / denominator;
}

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper)
    : AdaptiveModeLevelEstimator(
          apm_data_dumper,
          AudioProcessing::Config::GainController2::LevelEstimator::kRms,
          kDefaultUseSaturationProtector,
          kDefaultInitialSaturationMarginDb,
          kDefaultExtraSaturationMarginDb) {}

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper,
    AudioProcessing::Config::GainController2::LevelEstimator level_estimator,
    bool use_saturation_protector,
    float extra_saturation_margin_db)
    : AdaptiveModeLevelEstimator(apm_data_dumper,
                                 level_estimator,
                                 use_saturation_protector,
                                 kDefaultInitialSaturationMarginDb,
                                 extra_saturation_margin_db) {}

AdaptiveModeLevelEstimator::AdaptiveModeLevelEstimator(
    ApmDataDumper* apm_data_dumper,
    AudioProcessing::Config::GainController2::LevelEstimator level_estimator,
    bool use_saturation_protector,
    float initial_saturation_margin_db,
    float extra_saturation_margin_db)
    : apm_data_dumper_(apm_data_dumper),
      level_estimator_type_(level_estimator),
      use_saturation_protector_(use_saturation_protector),
      initial_saturation_margin_db_(initial_saturation_margin_db),
      extra_saturation_margin_db_(extra_saturation_margin_db),
      last_level_dbfs_(absl::nullopt) {
  Reset();
}

void AdaptiveModeLevelEstimator::Update(
    const VadLevelAnalyzer::Result& vad_level) {
  RTC_DCHECK_GT(vad_level.rms_dbfs, -150.f);
  RTC_DCHECK_LT(vad_level.rms_dbfs, 50.f);
  RTC_DCHECK_GT(vad_level.peak_dbfs, -150.f);
  RTC_DCHECK_LT(vad_level.peak_dbfs, 50.f);
  RTC_DCHECK_GE(vad_level.speech_probability, 0.f);
  RTC_DCHECK_LE(vad_level.speech_probability, 1.f);

  if (vad_level.speech_probability < kVadConfidenceThreshold) {
    DebugDumpEstimate();
    return;
  }

  // Update the state.
  RTC_DCHECK_GE(state_.time_to_full_buffer_ms, 0);
  const bool buffer_is_full = state_.time_to_full_buffer_ms == 0;
  if (!buffer_is_full) {
    state_.time_to_full_buffer_ms -= kFrameDurationMs;
  }

  // Read level estimation.
  float level_dbfs = 0.f;
  using LevelEstimatorType =
      AudioProcessing::Config::GainController2::LevelEstimator;
  switch (level_estimator_type_) {
    case LevelEstimatorType::kRms:
      level_dbfs = vad_level.rms_dbfs;
      break;
    case LevelEstimatorType::kPeak:
      level_dbfs = vad_level.peak_dbfs;
      break;
  }

  // Update level estimation (average level weighted by speech probability).
  RTC_DCHECK_GT(vad_level.speech_probability, 0.f);
  const float leak_factor = buffer_is_full ? kFullBufferLeakFactor : 1.f;
  state_.level_dbfs.numerator = state_.level_dbfs.numerator * leak_factor +
                                level_dbfs * vad_level.speech_probability;
  state_.level_dbfs.denominator = state_.level_dbfs.denominator * leak_factor +
                                  vad_level.speech_probability;

  // Cache level estimation.
  last_level_dbfs_ = state_.level_dbfs.GetRatio();

  if (use_saturation_protector_) {
    UpdateSaturationProtectorState(
        /*speech_peak_dbfs=*/vad_level.peak_dbfs,
        /*speech_level_dbfs=*/last_level_dbfs_.value(),
        state_.saturation_protector);
  }

  DebugDumpEstimate();
}

float AdaptiveModeLevelEstimator::GetLevelDbfs() const {
  float level_dbfs = last_level_dbfs_.value_or(kInitialSpeechLevelEstimateDbfs);
  if (use_saturation_protector_) {
    level_dbfs += state_.saturation_protector.margin_db;
    level_dbfs += extra_saturation_margin_db_;
  }
  return rtc::SafeClamp<float>(level_dbfs, -90.f, 30.f);
}

bool AdaptiveModeLevelEstimator::IsConfident() const {
  // Returns true if enough speech frames have been observed.
  return state_.time_to_full_buffer_ms == 0;
}

void AdaptiveModeLevelEstimator::Reset() {
  ResetState(state_);
  last_level_dbfs_ = absl::nullopt;
}

void AdaptiveModeLevelEstimator::ResetState(State& state) {
  state.time_to_full_buffer_ms = kFullBufferSizeMs;
  state.level_dbfs.numerator = 0.f;
  state.level_dbfs.denominator = 0.f;
  ResetSaturationProtectorState(initial_saturation_margin_db_,
                                state.saturation_protector);
}

void AdaptiveModeLevelEstimator::DebugDumpEstimate() {
  if (apm_data_dumper_) {
    apm_data_dumper_->DumpRaw("agc2_adaptive_level_estimate_dbfs",
                              GetLevelDbfs());
    apm_data_dumper_->DumpRaw("agc2_adaptive_saturation_margin_db",
                              state_.saturation_protector.margin_db);
  }
}

}  // namespace webrtc
