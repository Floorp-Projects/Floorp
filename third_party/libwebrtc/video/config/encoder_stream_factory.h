/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef VIDEO_CONFIG_ENCODER_STREAM_FACTORY_H_
#define VIDEO_CONFIG_ENCODER_STREAM_FACTORY_H_

#include <string>
#include <vector>

#include "api/transport/field_trial_based_config.h"
#include "api/units/data_rate.h"
#include "video/config/video_encoder_config.h"

namespace cricket {

class EncoderStreamFactory
    : public webrtc::VideoEncoderConfig::VideoStreamFactoryInterface {
 public:
  EncoderStreamFactory(std::string codec_name,
                       int max_qp,
                       bool is_screenshare,
                       bool conference_mode)
      : EncoderStreamFactory(codec_name,
                             max_qp,
                             is_screenshare,
                             conference_mode,
                             nullptr) {}

  EncoderStreamFactory(std::string codec_name,
                       int max_qp,
                       bool is_screenshare,
                       bool conference_mode,
                       const webrtc::FieldTrialsView* trials);

 private:
  std::vector<webrtc::VideoStream> CreateEncoderStreams(
      int width,
      int height,
      const webrtc::VideoEncoderConfig& encoder_config) override;

  std::vector<webrtc::VideoStream> CreateDefaultVideoStreams(
      int width,
      int height,
      const webrtc::VideoEncoderConfig& encoder_config,
      const absl::optional<webrtc::DataRate>& experimental_min_bitrate) const;

  std::vector<webrtc::VideoStream>
  CreateSimulcastOrConferenceModeScreenshareStreams(
      int width,
      int height,
      const webrtc::VideoEncoderConfig& encoder_config,
      const absl::optional<webrtc::DataRate>& experimental_min_bitrate) const;

  const std::string codec_name_;
  const int max_qp_;
  const bool is_screenshare_;
  // Allows a screenshare specific configuration, which enables temporal
  // layering and various settings.
  const bool conference_mode_;
  const webrtc::FieldTrialBasedConfig fallback_trials_;
  const webrtc::FieldTrialsView& trials_;
};

}  // namespace cricket

#endif  // VIDEO_CONFIG_ENCODER_STREAM_FACTORY_H_
