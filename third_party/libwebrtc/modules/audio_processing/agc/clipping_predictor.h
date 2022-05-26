/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_H_
#define MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

// Frame-wise clipping prediction and clipped level step estimation. Processing
// is done in two steps: Calling `Process` analyses a frame of audio and stores
// the frame metrics and `EstimateClippedLevelStep` produces an estimate for the
// required analog gain level decrease if clipping is predicted.
class ClippingPredictor {
 public:
  virtual ~ClippingPredictor() = default;

  virtual void Reset() = 0;

  // Estimates the analog gain clipped level step for channel `channel`.
  // Returns absl::nullopt if clipping is not predicted, otherwise returns the
  // suggested decrease in the analog gain level.
  virtual absl::optional<int> EstimateClippedLevelStep(
      int channel,
      int level,
      int default_step,
      int min_mic_level,
      int max_mic_level) const = 0;

  // Analyses a frame of audio and stores the resulting metrics in `data_`.
  virtual void Process(const AudioFrameView<const float>& frame) = 0;
};

// Creates a ClippingPredictor based on crest factor-based clipping event
// prediction.
std::unique_ptr<ClippingPredictor> CreateClippingEventPredictor(
    int num_channels,
    const AudioProcessing::Config::GainController1 ::AnalogGainController::
        ClippingPredictor& config);

// Creates a ClippingPredictor based on crest factor-based peak estimation and
// fixed-step clipped level step estimation.
std::unique_ptr<ClippingPredictor> CreateFixedStepClippingPeakPredictor(
    int num_channels,
    const AudioProcessing::Config::GainController1 ::AnalogGainController::
        ClippingPredictor& config);

// Creates a ClippingPredictor based on crest factor-based peak estimation and
// adaptive-step clipped level step estimation.
std::unique_ptr<ClippingPredictor> CreateAdaptiveStepClippingPeakPredictor(
    int num_channels,
    const AudioProcessing::Config::GainController1 ::AnalogGainController::
        ClippingPredictor& config);

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AGC_CLIPPING_PREDICTOR_H_
