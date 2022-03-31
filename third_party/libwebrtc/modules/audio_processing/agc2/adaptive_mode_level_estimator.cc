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
namespace {

using LevelEstimatorType =
    AudioProcessing::Config::GainController2::LevelEstimator;

// Combines a level estimation with the saturation protector margins.
float ComputeLevelEstimateDbfs(float level_estimate_dbfs,
                               bool use_saturation_protector,
                               float saturation_margin_db,
                               float extra_saturation_margin_db) {
  return rtc::SafeClamp<float>(
      level_estimate_dbfs +
          (use_saturation_protector
               ? (saturation_margin_db + extra_saturation_margin_db)
               : 0.f),
      -90.f, 30.f);
}

// Returns the level of given type from `vad_level`.
float GetLevel(const VadLevelAnalyzer::Result& vad_level,
               LevelEstimatorType type) {
  switch (type) {
    case LevelEstimatorType::kRms:
      return vad_level.rms_dbfs;
      break;
    case LevelEstimatorType::kPeak:
      return vad_level.peak_dbfs;
      break;
  }
}

}  // namespace

bool AdaptiveModeLevelEstimator::LevelEstimatorState::operator==(
    const AdaptiveModeLevelEstimator::LevelEstimatorState& b) const {
  return time_to_full_buffer_ms == b.time_to_full_buffer_ms &&
         level_dbfs.numerator == b.level_dbfs.numerator &&
         level_dbfs.denominator == b.level_dbfs.denominator &&
         saturation_protector == b.saturation_protector;
}

float AdaptiveModeLevelEstimator::LevelEstimatorState::Ratio::GetRatio() const {
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
      level_dbfs_(ComputeLevelEstimateDbfs(kInitialSpeechLevelEstimateDbfs,
                                           use_saturation_protector_,
                                           initial_saturation_margin_db_,
                                           extra_saturation_margin_db_)) {
  RTC_DCHECK(apm_data_dumper_);
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
  DumpDebugData();

  if (vad_level.speech_probability < kVadConfidenceThreshold) {
    return;
  }

  // Update level estimate.
  RTC_DCHECK_GE(state_.time_to_full_buffer_ms, 0);
  const bool buffer_is_full = state_.time_to_full_buffer_ms == 0;
  if (!buffer_is_full) {
    state_.time_to_full_buffer_ms -= kFrameDurationMs;
  }
  // Weighted average of levels with speech probability as weight.
  RTC_DCHECK_GT(vad_level.speech_probability, 0.f);
  const float leak_factor = buffer_is_full ? kFullBufferLeakFactor : 1.f;
  state_.level_dbfs.numerator =
      state_.level_dbfs.numerator * leak_factor +
      GetLevel(vad_level, level_estimator_type_) * vad_level.speech_probability;
  state_.level_dbfs.denominator = state_.level_dbfs.denominator * leak_factor +
                                  vad_level.speech_probability;

  const float level_dbfs = state_.level_dbfs.GetRatio();

  if (use_saturation_protector_) {
    UpdateSaturationProtectorState(vad_level.peak_dbfs, level_dbfs,
                                   state_.saturation_protector);
  }

  // Cache level estimation.
  level_dbfs_ = ComputeLevelEstimateDbfs(level_dbfs, use_saturation_protector_,
                                         state_.saturation_protector.margin_db,
                                         extra_saturation_margin_db_);
}

bool AdaptiveModeLevelEstimator::IsConfident() const {
  // Returns true if enough speech frames have been observed.
  return state_.time_to_full_buffer_ms == 0;
}

void AdaptiveModeLevelEstimator::Reset() {
  ResetLevelEstimatorState(state_);
  level_dbfs_ = ComputeLevelEstimateDbfs(
      kInitialSpeechLevelEstimateDbfs, use_saturation_protector_,
      initial_saturation_margin_db_, extra_saturation_margin_db_);
}

void AdaptiveModeLevelEstimator::ResetLevelEstimatorState(
    LevelEstimatorState& state) const {
  state.time_to_full_buffer_ms = kFullBufferSizeMs;
  state.level_dbfs.numerator = 0.f;
  state.level_dbfs.denominator = 0.f;
  ResetSaturationProtectorState(initial_saturation_margin_db_,
                                state.saturation_protector);
}

void AdaptiveModeLevelEstimator::DumpDebugData() const {
  apm_data_dumper_->DumpRaw("agc2_adaptive_level_estimate_dbfs", level_dbfs_);
  apm_data_dumper_->DumpRaw("agc2_adaptive_saturation_margin_db",
                            state_.saturation_protector.margin_db);
}

}  // namespace webrtc
