/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_agc.h"

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/vad_with_level.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

using AdaptiveDigitalConfig =
    AudioProcessing::Config::GainController2::AdaptiveDigital;
using NoiseEstimatorType =
    AudioProcessing::Config::GainController2::NoiseEstimator;

void DumpDebugData(const AdaptiveDigitalGainApplier::FrameInfo& info,
                   ApmDataDumper& dumper) {
  dumper.DumpRaw("agc2_vad_probability", info.vad_result.speech_probability);
  dumper.DumpRaw("agc2_vad_rms_dbfs", info.vad_result.rms_dbfs);
  dumper.DumpRaw("agc2_vad_peak_dbfs", info.vad_result.peak_dbfs);
  dumper.DumpRaw("agc2_noise_estimate_dbfs", info.input_noise_level_dbfs);
  dumper.DumpRaw("agc2_last_limiter_audio_level", info.limiter_envelope_dbfs);
}

constexpr int kGainApplierAdjacentSpeechFramesThreshold = 1;
constexpr float kMaxGainChangePerSecondDb = 3.0f;
constexpr float kMaxOutputNoiseLevelDbfs = -50.0f;

// Detects the available CPU features and applies any kill-switches.
AvailableCpuFeatures GetAllowedCpuFeatures(
    const AdaptiveDigitalConfig& config) {
  AvailableCpuFeatures features = GetAvailableCpuFeatures();
  if (!config.sse2_allowed) {
    features.sse2 = false;
  }
  if (!config.avx2_allowed) {
    features.avx2 = false;
  }
  if (!config.neon_allowed) {
    features.neon = false;
  }
  return features;
}

std::unique_ptr<NoiseLevelEstimator> CreateNoiseLevelEstimator(
    NoiseEstimatorType estimator_type,
    ApmDataDumper* apm_data_dumper) {
  switch (estimator_type) {
    case NoiseEstimatorType::kStationaryNoise:
      return CreateStationaryNoiseEstimator(apm_data_dumper);
    case NoiseEstimatorType::kNoiseFloor:
      return CreateNoiseFloorEstimator(apm_data_dumper);
  }
}

constexpr NoiseEstimatorType kDefaultNoiseLevelEstimatorType =
    NoiseEstimatorType::kNoiseFloor;

}  // namespace

AdaptiveAgc::AdaptiveAgc(ApmDataDumper* apm_data_dumper)
    : speech_level_estimator_(apm_data_dumper),
      gain_applier_(apm_data_dumper,
                    kGainApplierAdjacentSpeechFramesThreshold,
                    kMaxGainChangePerSecondDb,
                    kMaxOutputNoiseLevelDbfs),
      apm_data_dumper_(apm_data_dumper),
      noise_level_estimator_(
          CreateNoiseLevelEstimator(kDefaultNoiseLevelEstimatorType,
                                    apm_data_dumper)) {
  RTC_DCHECK(apm_data_dumper);
}

AdaptiveAgc::AdaptiveAgc(ApmDataDumper* apm_data_dumper,
                         const AdaptiveDigitalConfig& config)
    : speech_level_estimator_(
          apm_data_dumper,
          config.level_estimator,
          config.level_estimator_adjacent_speech_frames_threshold,
          config.initial_saturation_margin_db,
          config.extra_saturation_margin_db),
      vad_(config.vad_reset_period_ms,
           config.vad_probability_attack,
           GetAllowedCpuFeatures(config)),
      gain_applier_(apm_data_dumper,
                    config.gain_applier_adjacent_speech_frames_threshold,
                    config.max_gain_change_db_per_second,
                    config.max_output_noise_level_dbfs),
      apm_data_dumper_(apm_data_dumper),
      noise_level_estimator_(
          CreateNoiseLevelEstimator(config.noise_estimator, apm_data_dumper)) {
  RTC_DCHECK(apm_data_dumper);
  if (!config.use_saturation_protector) {
    RTC_LOG(LS_WARNING) << "The saturation protector cannot be disabled.";
  }
}

AdaptiveAgc::~AdaptiveAgc() = default;

void AdaptiveAgc::Process(AudioFrameView<float> frame, float limiter_envelope) {
  AdaptiveDigitalGainApplier::FrameInfo info;
  info.vad_result = vad_.AnalyzeFrame(frame);
  speech_level_estimator_.Update(info.vad_result);
  info.input_level_dbfs = speech_level_estimator_.level_dbfs();
  info.input_noise_level_dbfs = noise_level_estimator_->Analyze(frame);
  info.limiter_envelope_dbfs =
      limiter_envelope > 0 ? FloatS16ToDbfs(limiter_envelope) : -90.0f;
  info.estimate_is_confident = speech_level_estimator_.IsConfident();
  DumpDebugData(info, *apm_data_dumper_);
  gain_applier_.Process(info, frame);
}

void AdaptiveAgc::Reset() {
  speech_level_estimator_.Reset();
}

}  // namespace webrtc
