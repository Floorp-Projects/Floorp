/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/payload_router.h"

#include "modules/rtp_rtcp/include/rtp_rtcp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/random.h"
#include "rtc_base/timeutils.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {

namespace {
// Map information from info into rtp.
void CopyCodecSpecific(const CodecSpecificInfo* info, RTPVideoHeader* rtp) {
  RTC_DCHECK(info);
  switch (info->codecType) {
    case kVideoCodecVP8: {
      rtp->codec = kRtpVideoVp8;
      rtp->codecHeader.VP8.InitRTPVideoHeaderVP8();
      rtp->codecHeader.VP8.pictureId = info->codecSpecific.VP8.pictureId;
      rtp->codecHeader.VP8.nonReference = info->codecSpecific.VP8.nonReference;
      rtp->codecHeader.VP8.temporalIdx = info->codecSpecific.VP8.temporalIdx;
      rtp->codecHeader.VP8.layerSync = info->codecSpecific.VP8.layerSync;
      rtp->codecHeader.VP8.tl0PicIdx = info->codecSpecific.VP8.tl0PicIdx;
      rtp->codecHeader.VP8.keyIdx = info->codecSpecific.VP8.keyIdx;
      rtp->simulcastIdx = info->codecSpecific.VP8.simulcastIdx;
      return;
    }
    case kVideoCodecVP9: {
      rtp->codec = kRtpVideoVp9;
      rtp->codecHeader.VP9.InitRTPVideoHeaderVP9();
      rtp->codecHeader.VP9.inter_pic_predicted =
          info->codecSpecific.VP9.inter_pic_predicted;
      rtp->codecHeader.VP9.flexible_mode =
          info->codecSpecific.VP9.flexible_mode;
      rtp->codecHeader.VP9.ss_data_available =
          info->codecSpecific.VP9.ss_data_available;
      rtp->codecHeader.VP9.picture_id = info->codecSpecific.VP9.picture_id;
      rtp->codecHeader.VP9.tl0_pic_idx = info->codecSpecific.VP9.tl0_pic_idx;
      rtp->codecHeader.VP9.temporal_idx = info->codecSpecific.VP9.temporal_idx;
      rtp->codecHeader.VP9.spatial_idx = info->codecSpecific.VP9.spatial_idx;
      rtp->codecHeader.VP9.temporal_up_switch =
          info->codecSpecific.VP9.temporal_up_switch;
      rtp->codecHeader.VP9.inter_layer_predicted =
          info->codecSpecific.VP9.inter_layer_predicted;
      rtp->codecHeader.VP9.gof_idx = info->codecSpecific.VP9.gof_idx;
      rtp->codecHeader.VP9.num_spatial_layers =
          info->codecSpecific.VP9.num_spatial_layers;

      if (info->codecSpecific.VP9.ss_data_available) {
        rtp->codecHeader.VP9.spatial_layer_resolution_present =
            info->codecSpecific.VP9.spatial_layer_resolution_present;
        if (info->codecSpecific.VP9.spatial_layer_resolution_present) {
          for (size_t i = 0; i < info->codecSpecific.VP9.num_spatial_layers;
               ++i) {
            rtp->codecHeader.VP9.width[i] = info->codecSpecific.VP9.width[i];
            rtp->codecHeader.VP9.height[i] = info->codecSpecific.VP9.height[i];
          }
        }
        rtp->codecHeader.VP9.gof.CopyGofInfoVP9(info->codecSpecific.VP9.gof);
      }

      rtp->codecHeader.VP9.num_ref_pics = info->codecSpecific.VP9.num_ref_pics;
      for (int i = 0; i < info->codecSpecific.VP9.num_ref_pics; ++i)
        rtp->codecHeader.VP9.pid_diff[i] = info->codecSpecific.VP9.p_diff[i];
      return;
    }
    case kVideoCodecH264:
      rtp->codec = kRtpVideoH264;
      rtp->codecHeader.H264.packetization_mode =
          info->codecSpecific.H264.packetization_mode;
      return;
    case kVideoCodecGeneric:
      rtp->codec = kRtpVideoGeneric;
      rtp->simulcastIdx = info->codecSpecific.generic.simulcast_idx;
      return;
    default:
      return;
  }
}

}  // namespace

// Currently only used if forced fallback for VP8 is enabled.
// Consider adding tl0idx and set for VP8 and VP9.
// Make picture id not codec specific.
class PayloadRouter::RtpPayloadParams final {
 public:
  RtpPayloadParams(const uint32_t ssrc, const RtpPayloadState* state)
      : ssrc_(ssrc) {
    Random random(rtc::TimeMicros());
    state_.picture_id =
        state ? state->picture_id : (random.Rand<int16_t>() & 0x7FFF);
  }
  ~RtpPayloadParams() {}

  void Set(RTPVideoHeader* rtp_video_header) {
    if (rtp_video_header->codec == kRtpVideoVp8 &&
        rtp_video_header->codecHeader.VP8.pictureId != kNoPictureId) {
      rtp_video_header->codecHeader.VP8.pictureId = state_.picture_id;
      state_.picture_id = (state_.picture_id + 1) & 0x7FFF;
    }
  }

  uint32_t ssrc() const { return ssrc_; }

  RtpPayloadState state() const { return state_; }

 private:
  const uint32_t ssrc_;
  RtpPayloadState state_;
};

PayloadRouter::PayloadRouter(const std::vector<RtpRtcp*>& rtp_modules,
                             const std::vector<uint32_t>& ssrcs,
                             int payload_type,
                             const std::map<uint32_t, RtpPayloadState>& states)
    : active_(false),
      rtp_modules_(rtp_modules),
      payload_type_(payload_type),
      forced_fallback_enabled_((webrtc::field_trial::IsEnabled(
          "WebRTC-VP8-Forced-Fallback-Encoder-v2"))) {
  RTC_DCHECK_EQ(ssrcs.size(), rtp_modules.size());
  // SSRCs are assumed to be sorted in the same order as |rtp_modules|.
  for (uint32_t ssrc : ssrcs) {
    // Restore state if it previously existed.
    const RtpPayloadState* state = nullptr;
    auto it = states.find(ssrc);
    if (it != states.end()) {
      state = &it->second;
    }
    params_.push_back(RtpPayloadParams(ssrc, state));
  }
}

PayloadRouter::~PayloadRouter() {}

void PayloadRouter::SetActive(bool active) {
  rtc::CritScope lock(&crit_);
  if (active_ == active)
    return;
  active_ = active;

  for (auto& module : rtp_modules_) {
    module->SetSendingStatus(active_);
    module->SetSendingMediaStatus(active_);
  }
}

bool PayloadRouter::IsActive() {
  rtc::CritScope lock(&crit_);
  return active_ && !rtp_modules_.empty();
}

std::map<uint32_t, RtpPayloadState> PayloadRouter::GetRtpPayloadStates() const {
  rtc::CritScope lock(&crit_);
  std::map<uint32_t, RtpPayloadState> payload_states;
  for (const auto& param : params_) {
    payload_states[param.ssrc()] = param.state();
  }
  return payload_states;
}

EncodedImageCallback::Result PayloadRouter::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  rtc::CritScope lock(&crit_);
  RTC_DCHECK(!rtp_modules_.empty());
  if (!active_)
    return Result(Result::ERROR_SEND_FAILED);

  RTPVideoHeader rtp_video_header;
  memset(&rtp_video_header, 0, sizeof(RTPVideoHeader));
  if (codec_specific_info)
    CopyCodecSpecific(codec_specific_info, &rtp_video_header);
  rtp_video_header.rotation = encoded_image.rotation_;
  rtp_video_header.content_type = encoded_image.content_type_;
  if (encoded_image.timing_.flags != TimingFrameFlags::kInvalid &&
      encoded_image.timing_.flags != TimingFrameFlags::kNotTriggered) {
    rtp_video_header.video_timing.encode_start_delta_ms =
        VideoSendTiming::GetDeltaCappedMs(
            encoded_image.capture_time_ms_,
            encoded_image.timing_.encode_start_ms);
    rtp_video_header.video_timing.encode_finish_delta_ms =
        VideoSendTiming::GetDeltaCappedMs(
            encoded_image.capture_time_ms_,
            encoded_image.timing_.encode_finish_ms);
    rtp_video_header.video_timing.packetization_finish_delta_ms = 0;
    rtp_video_header.video_timing.pacer_exit_delta_ms = 0;
    rtp_video_header.video_timing.network_timestamp_delta_ms = 0;
    rtp_video_header.video_timing.network2_timestamp_delta_ms = 0;
    rtp_video_header.video_timing.flags = encoded_image.timing_.flags;
  } else {
    rtp_video_header.video_timing.flags = TimingFrameFlags::kInvalid;
  }
  rtp_video_header.playout_delay = encoded_image.playout_delay_;

  int stream_index = rtp_video_header.simulcastIdx;
  RTC_DCHECK_LT(stream_index, rtp_modules_.size());
  if (forced_fallback_enabled_) {
    // Sets picture id. The SW and HW encoder have separate picture id
    // sequences, set picture id to not cause sequence discontinuties at encoder
    // changes.
    params_[stream_index].Set(&rtp_video_header);
  }
  uint32_t frame_id;
  bool send_result = rtp_modules_[stream_index]->SendOutgoingData(
      encoded_image._frameType, payload_type_, encoded_image._timeStamp,
      encoded_image.capture_time_ms_, encoded_image._buffer,
      encoded_image._length, fragmentation, &rtp_video_header, &frame_id);
  if (!send_result)
    return Result(Result::ERROR_SEND_FAILED);

  return Result(Result::OK, frame_id);
}

void PayloadRouter::OnBitrateAllocationUpdated(
    const BitrateAllocation& bitrate) {
  rtc::CritScope lock(&crit_);
  if (IsActive()) {
    if (rtp_modules_.size() == 1) {
      // If spatial scalability is enabled, it is covered by a single stream.
      rtp_modules_[0]->SetVideoBitrateAllocation(bitrate);
    } else {
      // Simulcast is in use, split the BitrateAllocation into one struct per
      // rtp stream, moving over the temporal layer allocation.
      for (size_t si = 0; si < rtp_modules_.size(); ++si) {
        // Don't send empty TargetBitrate messages on streams not being relayed.
        if (!bitrate.IsSpatialLayerUsed(si))
          break;

        BitrateAllocation layer_bitrate;
        for (int tl = 0; tl < kMaxTemporalStreams; ++tl) {
          if (bitrate.HasBitrate(si, tl))
            layer_bitrate.SetBitrate(0, tl, bitrate.GetBitrate(si, tl));
        }
        rtp_modules_[si]->SetVideoBitrateAllocation(layer_bitrate);
      }
    }
  }
}

}  // namespace webrtc
