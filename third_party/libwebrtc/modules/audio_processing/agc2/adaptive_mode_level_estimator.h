/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_MODE_LEVEL_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_MODE_LEVEL_ESTIMATOR_H_

#include <stddef.h>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/saturation_protector.h"
#include "modules/audio_processing/agc2/vad_with_level.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {
class ApmDataDumper;

class AdaptiveModeLevelEstimator {
 public:
  explicit AdaptiveModeLevelEstimator(ApmDataDumper* apm_data_dumper);
  AdaptiveModeLevelEstimator(const AdaptiveModeLevelEstimator&) = delete;
  AdaptiveModeLevelEstimator& operator=(const AdaptiveModeLevelEstimator&) =
      delete;
  // Deprecated ctor.
  AdaptiveModeLevelEstimator(
      ApmDataDumper* apm_data_dumper,
      AudioProcessing::Config::GainController2::LevelEstimator level_estimator,
      bool use_saturation_protector,
      float extra_saturation_margin_db);
  // TODO(crbug.com/webrtc/7494): Replace ctor above with the one below.
  AdaptiveModeLevelEstimator(
      ApmDataDumper* apm_data_dumper,
      AudioProcessing::Config::GainController2::LevelEstimator level_estimator,
      bool use_saturation_protector,
      float initial_saturation_margin_db,
      float extra_saturation_margin_db);
  void UpdateEstimation(const VadLevelAnalyzer::Result& vad_level);
  float LatestLevelEstimate() const;
  void Reset();
  bool LevelEstimationIsConfident() const {
    return buffer_size_ms_ >= kFullBufferSizeMs;
  }

 private:
  void DebugDumpEstimate();

  const AudioProcessing::Config::GainController2::LevelEstimator
      level_estimator_;
  const bool use_saturation_protector_;
  const float extra_saturation_margin_db_;
  size_t buffer_size_ms_ = 0;
  float last_estimate_with_offset_dbfs_ = kInitialSpeechLevelEstimateDbfs;
  float estimate_numerator_ = 0.f;
  float estimate_denominator_ = 0.f;
  SaturationProtector saturation_protector_;
  ApmDataDumper* const apm_data_dumper_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_MODE_LEVEL_ESTIMATOR_H_
