/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_DIGITAL_GAIN_APPLIER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_DIGITAL_GAIN_APPLIER_H_

#include "modules/audio_processing/agc2/gain_applier.h"
#include "modules/audio_processing/agc2/vad_with_level.h"
#include "modules/audio_processing/include/audio_frame_view.h"

namespace webrtc {

class ApmDataDumper;

// Part of the adaptive digital controller that applies a digital adaptive gain.
// The gain is updated towards a target. The logic decides when gain updates are
// allowed, it controls the adaptation speed and caps the target based on the
// estimated noise level and the speech level estimate confidence.
class AdaptiveDigitalGainApplier {
 public:
  // Information about a frame to process.
  struct FrameInfo {
    float input_level_dbfs;        // Estimated speech plus noise level.
    float input_noise_level_dbfs;  // Estimated noise level.
    VadLevelAnalyzer::Result vad_result;
    float limiter_envelope_dbfs;  // Envelope level from the limiter.
    bool estimate_is_confident;
  };

  explicit AdaptiveDigitalGainApplier(ApmDataDumper* apm_data_dumper);
  // Ctor. `adjacent_speech_frames_threshold` indicates how many speech frames
  // are required before a gain increase is allowed.
  AdaptiveDigitalGainApplier(ApmDataDumper* apm_data_dumper,
                             int adjacent_speech_frames_threshold);
  AdaptiveDigitalGainApplier(const AdaptiveDigitalGainApplier&) = delete;
  AdaptiveDigitalGainApplier& operator=(const AdaptiveDigitalGainApplier&) =
      delete;

  // Analyzes `info`, updates the digital gain and applies it to `frame`.
  void Process(const FrameInfo& info, AudioFrameView<float> frame);

 private:
  ApmDataDumper* const apm_data_dumper_;
  GainApplier gain_applier_;

  const int adjacent_speech_frames_threshold_;

  int calls_since_last_gain_log_;
  int frames_to_gain_increase_allowed_;
  float last_gain_db_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC2_ADAPTIVE_DIGITAL_GAIN_APPLIER_H_
