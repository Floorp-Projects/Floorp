/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_sender_video.h"

#include <stdlib.h>
#include <string.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/byte_io.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "modules/rtp_rtcp/source/rtp_format_vp9.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/trace_event.h"

namespace webrtc {

namespace {
constexpr size_t kRedForFecHeaderLength = 1;
constexpr int64_t kMaxUnretransmittableFrameIntervalMs = 33 * 4;

void BuildRedPayload(const RtpPacketToSend& media_packet,
                     RtpPacketToSend* red_packet) {
  uint8_t* red_payload = red_packet->AllocatePayload(
      kRedForFecHeaderLength + media_packet.payload_size());
  RTC_DCHECK(red_payload);
  red_payload[0] = media_packet.PayloadType();

  auto media_payload = media_packet.payload();
  memcpy(&red_payload[kRedForFecHeaderLength], media_payload.data(),
         media_payload.size());
}
}  // namespace

RTPSenderVideo::RTPSenderVideo(Clock* clock,
                               RTPSender* rtp_sender,
                               FlexfecSender* flexfec_sender)
    : rtp_sender_(rtp_sender),
      clock_(clock),
      video_type_(kRtpVideoGeneric),
      retransmission_settings_(kRetransmitBaseLayer |
                               kConditionallyRetransmitHigherLayers),
      last_rotation_(kVideoRotation_0),
      red_payload_type_(-1),
      ulpfec_payload_type_(-1),
      flexfec_sender_(flexfec_sender),
      delta_fec_params_{0, 1, kFecMaskRandom},
      key_fec_params_{0, 1, kFecMaskRandom},
      fec_bitrate_(1000, RateStatistics::kBpsScale),
      video_bitrate_(1000, RateStatistics::kBpsScale) {}

RTPSenderVideo::~RTPSenderVideo() {}

void RTPSenderVideo::SetVideoCodecType(RtpVideoCodecTypes video_type) {
  video_type_ = video_type;
}

RtpVideoCodecTypes RTPSenderVideo::VideoCodecType() const {
  return video_type_;
}

// Static.
RtpUtility::Payload* RTPSenderVideo::CreateVideoPayload(
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    int8_t payload_type) {
  RtpVideoCodecTypes video_type = kRtpVideoGeneric;
  if (RtpUtility::StringCompare(payload_name, "VP8", 3)) {
    video_type = kRtpVideoVp8;
  } else if (RtpUtility::StringCompare(payload_name, "VP9", 3)) {
    video_type = kRtpVideoVp9;
  } else if (RtpUtility::StringCompare(payload_name, "H264", 4)) {
    video_type = kRtpVideoH264;
  } else if (RtpUtility::StringCompare(payload_name, "I420", 4)) {
    video_type = kRtpVideoGeneric;
  } else {
    video_type = kRtpVideoGeneric;
  }
  VideoPayload vp;
  vp.videoCodecType = video_type;
  return new RtpUtility::Payload(payload_name, PayloadUnion(vp));
}

void RTPSenderVideo::SendVideoPacket(std::unique_ptr<RtpPacketToSend> packet,
                                     StorageType storage) {
  // Remember some values about the packet before sending it away.
  size_t packet_size = packet->size();
  uint16_t seq_num = packet->SequenceNumber();
  uint32_t rtp_timestamp = packet->Timestamp();
  if (!rtp_sender_->SendToNetwork(std::move(packet), storage,
                                  RtpPacketSender::kLowPriority)) {
    RTC_LOG(LS_WARNING) << "Failed to send video packet " << seq_num;
    return;
  }
  rtc::CritScope cs(&stats_crit_);
  video_bitrate_.Update(packet_size, clock_->TimeInMilliseconds());
  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "Video::PacketNormal", "timestamp", rtp_timestamp,
                       "seqnum", seq_num);
}

void RTPSenderVideo::SendVideoPacketAsRedMaybeWithUlpfec(
    std::unique_ptr<RtpPacketToSend> media_packet,
    StorageType media_packet_storage,
    bool protect_media_packet) {
  uint32_t rtp_timestamp = media_packet->Timestamp();
  uint16_t media_seq_num = media_packet->SequenceNumber();

  std::unique_ptr<RtpPacketToSend> red_packet(
      new RtpPacketToSend(*media_packet));
  BuildRedPayload(*media_packet, red_packet.get());

  std::vector<std::unique_ptr<RedPacket>> fec_packets;
  StorageType fec_storage = kDontRetransmit;
  {
    // Only protect while creating RED and FEC packets, not when sending.
    rtc::CritScope cs(&crit_);
    red_packet->SetPayloadType(red_payload_type_);
    if (ulpfec_enabled()) {
      if (protect_media_packet) {
        ulpfec_generator_.AddRtpPacketAndGenerateFec(
            media_packet->data(), media_packet->payload_size(),
            media_packet->headers_size());
      }
      uint16_t num_fec_packets = ulpfec_generator_.NumAvailableFecPackets();
      if (num_fec_packets > 0) {
        uint16_t first_fec_sequence_number =
            rtp_sender_->AllocateSequenceNumber(num_fec_packets);
        fec_packets = ulpfec_generator_.GetUlpfecPacketsAsRed(
            red_payload_type_, ulpfec_payload_type_, first_fec_sequence_number,
            media_packet->headers_size());
        RTC_DCHECK_EQ(num_fec_packets, fec_packets.size());
        if (retransmission_settings_ & kRetransmitFECPackets)
          fec_storage = kAllowRetransmission;
      }
    }
  }
  // Send |red_packet| instead of |packet| for allocated sequence number.
  size_t red_packet_size = red_packet->size();
  if (rtp_sender_->SendToNetwork(std::move(red_packet), media_packet_storage,
                                 RtpPacketSender::kLowPriority)) {
    rtc::CritScope cs(&stats_crit_);
    video_bitrate_.Update(red_packet_size, clock_->TimeInMilliseconds());
    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                         "Video::PacketRed", "timestamp", rtp_timestamp,
                         "seqnum", media_seq_num);
  } else {
    RTC_LOG(LS_WARNING) << "Failed to send RED packet " << media_seq_num;
  }
  for (const auto& fec_packet : fec_packets) {
    // TODO(danilchap): Make ulpfec_generator_ generate RtpPacketToSend to avoid
    // reparsing them.
    std::unique_ptr<RtpPacketToSend> rtp_packet(
        new RtpPacketToSend(*media_packet));
    RTC_CHECK(rtp_packet->Parse(fec_packet->data(), fec_packet->length()));
    rtp_packet->set_capture_time_ms(media_packet->capture_time_ms());
    uint16_t fec_sequence_number = rtp_packet->SequenceNumber();
    if (rtp_sender_->SendToNetwork(std::move(rtp_packet), fec_storage,
                                   RtpPacketSender::kLowPriority)) {
      rtc::CritScope cs(&stats_crit_);
      fec_bitrate_.Update(fec_packet->length(), clock_->TimeInMilliseconds());
      TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                           "Video::PacketUlpfec", "timestamp", rtp_timestamp,
                           "seqnum", fec_sequence_number);
    } else {
      RTC_LOG(LS_WARNING) << "Failed to send ULPFEC packet "
                          << fec_sequence_number;
    }
  }
}

void RTPSenderVideo::SendVideoPacketWithFlexfec(
    std::unique_ptr<RtpPacketToSend> media_packet,
    StorageType media_packet_storage,
    bool protect_media_packet) {
  RTC_DCHECK(flexfec_sender_);

  if (protect_media_packet)
    flexfec_sender_->AddRtpPacketAndGenerateFec(*media_packet);

  SendVideoPacket(std::move(media_packet), media_packet_storage);

  if (flexfec_sender_->FecAvailable()) {
    std::vector<std::unique_ptr<RtpPacketToSend>> fec_packets =
        flexfec_sender_->GetFecPackets();
    for (auto& fec_packet : fec_packets) {
      size_t packet_length = fec_packet->size();
      uint32_t timestamp = fec_packet->Timestamp();
      uint16_t seq_num = fec_packet->SequenceNumber();
      if (rtp_sender_->SendToNetwork(std::move(fec_packet), kDontRetransmit,
                                     RtpPacketSender::kLowPriority)) {
        rtc::CritScope cs(&stats_crit_);
        fec_bitrate_.Update(packet_length, clock_->TimeInMilliseconds());
        TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                             "Video::PacketFlexfec", "timestamp", timestamp,
                             "seqnum", seq_num);
      } else {
        RTC_LOG(LS_WARNING) << "Failed to send FlexFEC packet " << seq_num;
      }
    }
  }
}

void RTPSenderVideo::SetUlpfecConfig(int red_payload_type,
                                     int ulpfec_payload_type) {
  // Sanity check. Per the definition of UlpfecConfig (see config.h),
  // a payload type of -1 means that the corresponding feature is
  // turned off.
  RTC_DCHECK_GE(red_payload_type, -1);
  RTC_DCHECK_LE(red_payload_type, 127);
  RTC_DCHECK_GE(ulpfec_payload_type, -1);
  RTC_DCHECK_LE(ulpfec_payload_type, 127);

  rtc::CritScope cs(&crit_);
  red_payload_type_ = red_payload_type;
  ulpfec_payload_type_ = ulpfec_payload_type;

  // Must not enable ULPFEC without RED.
  // TODO(brandtr): We currently support enabling RED without ULPFEC. Change
  // this when we have removed the RED/RTX send-side workaround, so that we
  // ensure that RED and ULPFEC are only enabled together.
  RTC_DCHECK(red_enabled() || !ulpfec_enabled());

  // Reset FEC parameters.
  delta_fec_params_ = FecProtectionParams{0, 1, kFecMaskRandom};
  key_fec_params_ = FecProtectionParams{0, 1, kFecMaskRandom};
}

void RTPSenderVideo::GetUlpfecConfig(int* red_payload_type,
                                     int* ulpfec_payload_type) const {
  rtc::CritScope cs(&crit_);
  *red_payload_type = red_payload_type_;
  *ulpfec_payload_type = ulpfec_payload_type_;
}

size_t RTPSenderVideo::CalculateFecPacketOverhead() const {
  if (flexfec_enabled())
    return flexfec_sender_->MaxPacketOverhead();

  size_t overhead = 0;
  if (red_enabled()) {
    // The RED overhead is due to a small header.
    overhead += kRedForFecHeaderLength;
  }
  if (ulpfec_enabled()) {
    // For ULPFEC, the overhead is the FEC headers plus RED for FEC header
    // (see above) plus anything in RTP header beyond the 12 bytes base header
    // (CSRC list, extensions...)
    // This reason for the header extensions to be included here is that
    // from an FEC viewpoint, they are part of the payload to be protected.
    // (The base RTP header is already protected by the FEC header.)
    overhead += ulpfec_generator_.MaxPacketOverhead() +
                (rtp_sender_->RtpHeaderLength() - kRtpHeaderSize);
  }
  return overhead;
}

void RTPSenderVideo::SetFecParameters(const FecProtectionParams& delta_params,
                                      const FecProtectionParams& key_params) {
  rtc::CritScope cs(&crit_);
  delta_fec_params_ = delta_params;
  key_fec_params_ = key_params;
}

rtc::Optional<uint32_t> RTPSenderVideo::FlexfecSsrc() const {
  if (flexfec_sender_) {
    return flexfec_sender_->ssrc();
  }
  return rtc::nullopt;
}

bool RTPSenderVideo::SendVideo(RtpVideoCodecTypes video_type,
                               FrameType frame_type,
                               int8_t payload_type,
                               uint32_t rtp_timestamp,
                               int64_t capture_time_ms,
                               const uint8_t* payload_data,
                               size_t payload_size,
                               const RTPFragmentationHeader* fragmentation,
                               const RTPVideoHeader* video_header,
                               int64_t expected_retransmission_time_ms,
                               const StreamId* rtpStreamId,
                               const StreamId* mId) {
  if (payload_size == 0)
    return false;

  // Create header that will be reused in all packets.
  std::unique_ptr<RtpPacketToSend> rtp_header = rtp_sender_->AllocatePacket();
  rtp_header->SetPayloadType(payload_type);
  rtp_header->SetTimestamp(rtp_timestamp);
  rtp_header->set_capture_time_ms(capture_time_ms);

  if (rtpStreamId && !rtpStreamId->empty()) {
     rtp_header->SetExtension<RtpStreamId>(*rtpStreamId);
  }
  if (mId && !mId->empty()) {
     rtp_header->SetExtension<RtpMid>(*mId);
  }

  auto last_packet = rtc::MakeUnique<RtpPacketToSend>(*rtp_header);

  size_t fec_packet_overhead;
  bool red_enabled;
  int32_t retransmission_settings;
  {
    rtc::CritScope cs(&crit_);
    // According to
    // http://www.etsi.org/deliver/etsi_ts/126100_126199/126114/12.07.00_60/
    // ts_126114v120700p.pdf Section 7.4.5:
    // The MTSI client shall add the payload bytes as defined in this clause
    // onto the last RTP packet in each group of packets which make up a key
    // frame (I-frame or IDR frame in H.264 (AVC), or an IRAP picture in H.265
    // (HEVC)). The MTSI client may also add the payload bytes onto the last RTP
    // packet in each group of packets which make up another type of frame
    // (e.g. a P-Frame) only if the current value is different from the previous
    // value sent.
    if (video_header) {
      // Set rotation when key frame or when changed (to follow standard).
      // Or when different from 0 (to follow current receiver implementation).
      VideoRotation current_rotation = video_header->rotation;
      if (frame_type == kVideoFrameKey || current_rotation != last_rotation_ ||
          current_rotation != kVideoRotation_0)
        last_packet->SetExtension<VideoOrientation>(current_rotation);
      last_rotation_ = current_rotation;
      // Report content type only for key frames.
      if (frame_type == kVideoFrameKey &&
          video_header->content_type != VideoContentType::UNSPECIFIED) {
        last_packet->SetExtension<VideoContentTypeExtension>(
            video_header->content_type);
      }
      if (video_header->video_timing.flags != TimingFrameFlags::kInvalid) {
        last_packet->SetExtension<VideoTimingExtension>(
            video_header->video_timing);
      }
    }

    // FEC settings.
    const FecProtectionParams& fec_params =
        frame_type == kVideoFrameKey ? key_fec_params_ : delta_fec_params_;
    if (flexfec_enabled())
      flexfec_sender_->SetFecParameters(fec_params);
    if (ulpfec_enabled())
      ulpfec_generator_.SetFecParameters(fec_params);

    fec_packet_overhead = CalculateFecPacketOverhead();
    red_enabled = this->red_enabled();
    retransmission_settings = retransmission_settings_;
  }

  size_t packet_capacity = rtp_sender_->MaxRtpPacketSize() -
                           fec_packet_overhead -
                           (rtp_sender_->RtxStatus() ? kRtxHeaderSize : 0);
  RTC_DCHECK_LE(packet_capacity, rtp_header->capacity());
  RTC_DCHECK_GT(packet_capacity, rtp_header->headers_size());
  RTC_DCHECK_GT(packet_capacity, last_packet->headers_size());
  size_t max_data_payload_length = packet_capacity - rtp_header->headers_size();
  RTC_DCHECK_GE(last_packet->headers_size(), rtp_header->headers_size());
  size_t last_packet_reduction_len =
      last_packet->headers_size() - rtp_header->headers_size();

  std::unique_ptr<RtpPacketizer> packetizer(RtpPacketizer::Create(
      video_type, max_data_payload_length, last_packet_reduction_len,
      video_header ? &(video_header->codecHeader) : nullptr, frame_type));

  const uint8_t temporal_id =
      video_header ? GetTemporalId(*video_header) : kNoTemporalIdx;
  StorageType storage = GetStorageType(temporal_id, retransmission_settings,
                                       expected_retransmission_time_ms);

  size_t num_packets =
      packetizer->SetPayloadData(payload_data, payload_size, fragmentation);

  if (num_packets == 0)
    return false;

  bool first_frame = first_frame_sent_();
  for (size_t i = 0; i < num_packets; ++i) {
    bool last = (i + 1) == num_packets;
    auto packet = last ? std::move(last_packet)
                       : rtc::MakeUnique<RtpPacketToSend>(*rtp_header);
    if (!packetizer->NextPacket(packet.get()))
      return false;
    RTC_DCHECK_LE(packet->payload_size(),
                  last ? max_data_payload_length - last_packet_reduction_len
                       : max_data_payload_length);
    if (!rtp_sender_->AssignSequenceNumber(packet.get()))
      return false;

    // No FEC protection for upper temporal layers, if used.
    bool protect_packet = temporal_id == 0 || temporal_id == kNoTemporalIdx;

    // Put packetization finish timestamp into extension.
    if (packet->HasExtension<VideoTimingExtension>()) {
      packet->set_packetization_finish_time_ms(clock_->TimeInMilliseconds());
      // TODO(ilnik): Due to webrtc:7859, packets with timing extensions are not
      // protected by FEC. It reduces FEC efficiency a bit. When FEC is moved
      // below the pacer, it can be re-enabled for these packets.
      // NOTE: Any RTP stream processor in the network, modifying 'network'
      // timestamps in the timing frames extension have to be an end-point for
      // FEC, otherwise recovered by FEC packets will be corrupted.
      protect_packet = false;
    }

    if (flexfec_enabled()) {
      // TODO(brandtr): Remove the FlexFEC code path when FlexfecSender
      // is wired up to PacedSender instead.
      SendVideoPacketWithFlexfec(std::move(packet), storage, protect_packet);
    } else if (red_enabled) {
      SendVideoPacketAsRedMaybeWithUlpfec(std::move(packet), storage,
                                          protect_packet);
    } else {
      SendVideoPacket(std::move(packet), storage);
    }

    if (first_frame) {
      if (i == 0) {
        RTC_LOG(LS_INFO)
            << "Sent first RTP packet of the first video frame (pre-pacer)";
      }
      if (last) {
        RTC_LOG(LS_INFO)
            << "Sent last RTP packet of the first video frame (pre-pacer)";
      }
    }
  }

  TRACE_EVENT_ASYNC_END1("webrtc", "Video", capture_time_ms, "timestamp",
                         rtp_timestamp);
  return true;
}

uint32_t RTPSenderVideo::VideoBitrateSent() const {
  rtc::CritScope cs(&stats_crit_);
  return video_bitrate_.Rate(clock_->TimeInMilliseconds()).value_or(0);
}

uint32_t RTPSenderVideo::FecOverheadRate() const {
  rtc::CritScope cs(&stats_crit_);
  return fec_bitrate_.Rate(clock_->TimeInMilliseconds()).value_or(0);
}

int RTPSenderVideo::SelectiveRetransmissions() const {
  rtc::CritScope cs(&crit_);
  return retransmission_settings_;
}

void RTPSenderVideo::SetSelectiveRetransmissions(uint8_t settings) {
  rtc::CritScope cs(&crit_);
  retransmission_settings_ = settings;
}

StorageType RTPSenderVideo::GetStorageType(
    uint8_t temporal_id,
    int32_t retransmission_settings,
    int64_t expected_retransmission_time_ms) {
  if (retransmission_settings == kRetransmitOff)
    return StorageType::kDontRetransmit;
  if (retransmission_settings == kRetransmitAllPackets)
    return StorageType::kAllowRetransmission;

  rtc::CritScope cs(&stats_crit_);
  // Media packet storage.
  if ((retransmission_settings & kConditionallyRetransmitHigherLayers) &&
      UpdateConditionalRetransmit(temporal_id,
                                  expected_retransmission_time_ms)) {
    retransmission_settings |= kRetransmitHigherLayers;
  }

  if (temporal_id == kNoTemporalIdx)
    return kAllowRetransmission;

  if ((retransmission_settings & kRetransmitBaseLayer) && temporal_id == 0)
    return kAllowRetransmission;

  if ((retransmission_settings & kRetransmitHigherLayers) && temporal_id > 0)
    return kAllowRetransmission;

  return kDontRetransmit;
}

uint8_t RTPSenderVideo::GetTemporalId(const RTPVideoHeader& header) {
  switch (header.codec) {
    case kRtpVideoVp8:
      return header.codecHeader.VP8.temporalIdx;
    case kRtpVideoVp9:
      return header.codecHeader.VP9.temporal_idx;
    default:
      return kNoTemporalIdx;
  }
}

bool RTPSenderVideo::UpdateConditionalRetransmit(
    uint8_t temporal_id,
    int64_t expected_retransmission_time_ms) {
  int64_t now_ms = clock_->TimeInMilliseconds();
  // Update stats for any temporal layer.
  TemporalLayerStats* current_layer_stats =
      &frame_stats_by_temporal_layer_[temporal_id];
  current_layer_stats->frame_rate_fp1000s.Update(1, now_ms);
  int64_t tl_frame_interval = now_ms - current_layer_stats->last_frame_time_ms;
  current_layer_stats->last_frame_time_ms = now_ms;

  // Conditional retransmit only applies to upper layers.
  if (temporal_id != kNoTemporalIdx && temporal_id > 0) {
    if (tl_frame_interval >= kMaxUnretransmittableFrameIntervalMs) {
      // Too long since a retransmittable frame in this layer, enable NACK
      // protection.
      return true;
    } else {
      // Estimate when the next frame of any lower layer will be sent.
      const int64_t kUndefined = std::numeric_limits<int64_t>::max();
      int64_t expected_next_frame_time = kUndefined;
      for (int i = temporal_id - 1; i >= 0; --i) {
        TemporalLayerStats* stats = &frame_stats_by_temporal_layer_[i];
        rtc::Optional<uint32_t> rate = stats->frame_rate_fp1000s.Rate(now_ms);
        if (rate) {
          int64_t tl_next = stats->last_frame_time_ms + 1000000 / *rate;
          if (tl_next - now_ms > -expected_retransmission_time_ms &&
              tl_next < expected_next_frame_time) {
            expected_next_frame_time = tl_next;
          }
        }
      }

      if (expected_next_frame_time == kUndefined ||
          expected_next_frame_time - now_ms > expected_retransmission_time_ms) {
        // The next frame in a lower layer is expected at a later time (or
        // unable to tell due to lack of data) than a retransmission is
        // estimated to be able to arrive, so allow this packet to be nacked.
        return true;
      }
    }
  }

  return false;
}

}  // namespace webrtc
