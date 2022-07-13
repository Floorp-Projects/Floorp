/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_MULTI_CHANNEL_CONTENT_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_MULTI_CHANNEL_CONTENT_DETECTOR_H_

#include <stddef.h>

#include <vector>

namespace webrtc {

// Analyzes audio content to determine whether the contained audio is proper
// multichannel, or only upmixed mono. To allow for differences introduced by
// hardware drivers, a threshold `detection_threshold` is used for the
// detection.
class MultiChannelContentDetector {
 public:
  MultiChannelContentDetector(bool detect_stereo_content,
                              int num_render_input_channels,
                              float detection_threshold);

  // Compares the left and right channels in the render `frame` to determine
  // whether the signal is a proper multichannel signal. Returns a bool
  // indicating whether a change in the multichannel was detected.
  bool UpdateDetection(
      const std::vector<std::vector<std::vector<float>>>& frame);

  bool IsMultiChannelContentDetected() const {
    return proper_multichannel_content_detected_;
  }

 private:
  const bool detect_stereo_content_;
  const float detection_threshold_;
  bool proper_multichannel_content_detected_;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_PROCESSING_AEC3_MULTI_CHANNEL_CONTENT_DETECTOR_H_
