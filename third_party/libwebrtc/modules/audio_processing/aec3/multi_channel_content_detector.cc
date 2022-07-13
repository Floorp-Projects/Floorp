
/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/multi_channel_content_detector.h"

#include <cmath>

namespace webrtc {

namespace {

// Compares the left and right channels in the render `frame` to determine
// whether the signal is a proper stereo signal. To allow for differences
// introduced by hardware drivers, a threshold `detection_threshold` is used for
// the detection.
bool IsProperStereo(const std::vector<std::vector<std::vector<float>>>& frame,
                    float detection_threshold) {
  if (frame[0].size() < 2) {
    return false;
  }

  for (size_t band = 0; band < frame.size(); ++band) {
    for (size_t k = 0; k < frame[band][0].size(); ++k) {
      if (std::fabs(frame[band][0][k] - frame[band][1][k]) >
          detection_threshold) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

MultiChannelContentDetector::MultiChannelContentDetector(
    bool detect_stereo_content,
    int num_render_input_channels,
    float detection_threshold)
    : detect_stereo_content_(detect_stereo_content),
      detection_threshold_(detection_threshold),
      proper_multichannel_content_detected_(!detect_stereo_content &&
                                            num_render_input_channels > 1) {}

bool MultiChannelContentDetector::UpdateDetection(
    const std::vector<std::vector<std::vector<float>>>& frame) {
  bool previous_proper_multichannel_content_detected_ =
      proper_multichannel_content_detected_;
  if (detect_stereo_content_ && !proper_multichannel_content_detected_) {
    proper_multichannel_content_detected_ =
        IsProperStereo(frame, detection_threshold_);
  }
  return previous_proper_multichannel_content_detected_ !=
         proper_multichannel_content_detected_;
}

}  // namespace webrtc
