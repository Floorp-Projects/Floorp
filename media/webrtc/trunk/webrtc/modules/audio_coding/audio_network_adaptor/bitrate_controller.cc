/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/bitrate_controller.h"

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace webrtc {
namespace audio_network_adaptor {

BitrateController::Config::Config(int initial_bitrate_bps,
                                  int initial_frame_length_ms)
    : initial_bitrate_bps(initial_bitrate_bps),
      initial_frame_length_ms(initial_frame_length_ms) {}

BitrateController::Config::~Config() = default;

BitrateController::BitrateController(const Config& config)
    : config_(config),
      bitrate_bps_(config_.initial_bitrate_bps),
      frame_length_ms_(config_.initial_frame_length_ms) {
  RTC_DCHECK_GT(bitrate_bps_, 0);
  RTC_DCHECK_GT(frame_length_ms_, 0);
}

void BitrateController::MakeDecision(
    const NetworkMetrics& metrics,
    AudioNetworkAdaptor::EncoderRuntimeConfig* config) {
  // Decision on |bitrate_bps| should not have been made.
  RTC_DCHECK(!config->bitrate_bps);
  if (metrics.target_audio_bitrate_bps && metrics.overhead_bytes_per_packet) {
    // Current implementation of BitrateController can only work when
    // |metrics.target_audio_bitrate_bps| includes overhead is enabled. This is
    // currently governed by the following field trial.
    RTC_DCHECK_EQ("Enabled", webrtc::field_trial::FindFullName(
                                 "WebRTC-SendSideBwe-WithOverhead"));
    if (config->frame_length_ms)
      frame_length_ms_ = *config->frame_length_ms;
    int overhead_rate_bps =
        *metrics.overhead_bytes_per_packet * 8 * 1000 / frame_length_ms_;
    bitrate_bps_ =
        std::max(0, *metrics.target_audio_bitrate_bps - overhead_rate_bps);
  }
  config->bitrate_bps = rtc::Optional<int>(bitrate_bps_);
}

}  // namespace audio_network_adaptor
}  // namespace webrtc
