/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/experiments/encoder_info_settings.h"

#include <stdio.h>

#include "rtc_base/experiments/field_trial_list.h"
#include "rtc_base/logging.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {

std::vector<VideoEncoder::ResolutionBitrateLimits> ToResolutionBitrateLimits(
    const std::vector<EncoderInfoSettings::BitrateLimit>& limits) {
  std::vector<VideoEncoder::ResolutionBitrateLimits> result;
  for (const auto& limit : limits) {
    result.push_back(VideoEncoder::ResolutionBitrateLimits(
        limit.frame_size_pixels, limit.min_start_bitrate_bps,
        limit.min_bitrate_bps, limit.max_bitrate_bps));
  }
  return result;
}

}  // namespace

EncoderInfoSettings::EncoderInfoSettings(std::string name)
    : requested_resolution_alignment_("requested_resolution_alignment"),
      apply_alignment_to_all_simulcast_layers_(
          "apply_alignment_to_all_simulcast_layers") {
  FieldTrialStructList<BitrateLimit> bitrate_limits(
      {FieldTrialStructMember(
           "frame_size_pixels",
           [](BitrateLimit* b) { return &b->frame_size_pixels; }),
       FieldTrialStructMember(
           "min_start_bitrate_bps",
           [](BitrateLimit* b) { return &b->min_start_bitrate_bps; }),
       FieldTrialStructMember(
           "min_bitrate_bps",
           [](BitrateLimit* b) { return &b->min_bitrate_bps; }),
       FieldTrialStructMember(
           "max_bitrate_bps",
           [](BitrateLimit* b) { return &b->max_bitrate_bps; })},
      {});

  ParseFieldTrial({&bitrate_limits, &requested_resolution_alignment_,
                   &apply_alignment_to_all_simulcast_layers_},
                  field_trial::FindFullName(name));

  resolution_bitrate_limits_ = ToResolutionBitrateLimits(bitrate_limits.Get());
}

absl::optional<int> EncoderInfoSettings::requested_resolution_alignment()
    const {
  if (requested_resolution_alignment_ &&
      requested_resolution_alignment_.Value() < 1) {
    RTC_LOG(LS_WARNING) << "Unsupported alignment value, ignored.";
    return absl::nullopt;
  }
  return requested_resolution_alignment_.GetOptional();
}

EncoderInfoSettings::~EncoderInfoSettings() {}

SimulcastEncoderAdapterEncoderInfoSettings::
    SimulcastEncoderAdapterEncoderInfoSettings()
    : EncoderInfoSettings(
          "WebRTC-SimulcastEncoderAdapter-GetEncoderInfoOverride") {}

LibvpxVp9EncoderInfoSettings::LibvpxVp9EncoderInfoSettings()
    : EncoderInfoSettings("WebRTC-LibvpxVp9Encoder-GetEncoderInfoOverride") {}

}  // namespace webrtc
