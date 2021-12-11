/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_REMOVER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_REMOVER_H_

#include <vector>

#include "api/optional.h"
#include "modules/audio_processing/aec3/echo_path_variability.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"

namespace webrtc {

// Class for removing the echo from the capture signal.
class EchoRemover {
 public:
  static EchoRemover* Create(const EchoCanceller3Config& config,
                             int sample_rate_hz);
  virtual ~EchoRemover() = default;

  // Get current metrics.
  virtual void GetMetrics(EchoControl::Metrics* metrics) const = 0;

  // Removes the echo from a block of samples from the capture signal. The
  // supplied render signal is assumed to be pre-aligned with the capture
  // signal.
  virtual void ProcessCapture(
      const rtc::Optional<size_t>& echo_path_delay_samples,
      const EchoPathVariability& echo_path_variability,
      bool capture_signal_saturation,
      const RenderBuffer& render_buffer,
      std::vector<std::vector<float>>* capture) = 0;

  // Updates the status on whether echo leakage is detected in the output of the
  // echo remover.
  virtual void UpdateEchoLeakageStatus(bool leakage_detected) = 0;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_ECHO_REMOVER_H_
