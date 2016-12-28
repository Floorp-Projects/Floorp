/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtp_sender_video.h"

#include <stdlib.h>
#include <string.h>

#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/producer_fec.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp9.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {
enum { REDForFECHeaderLength = 1 };

struct RtpPacket {
  uint16_t rtpHeaderLength;
  ForwardErrorCorrection::Packet* pkt;
};

RTPSenderVideo::RTPSenderVideo(Clock* clock, RTPSenderInterface* rtpSender)
    : _rtpSender(*rtpSender),
      crit_(CriticalSectionWrapper::CreateCriticalSection()),
      _videoType(kRtpVideoGeneric),
      _maxBitrate(0),
      _retransmissionSettings(kRetransmitBaseLayer),

      // Generic FEC
      fec_(),
      fec_enabled_(false),
      red_payload_type_(-1),
      fec_payload_type_(-1),
      delta_fec_params_(),
      key_fec_params_(),
      producer_fec_(&fec_),
      _fecOverheadRate(clock, NULL),
      _videoBitrate(clock, NULL) {
  memset(&delta_fec_params_, 0, sizeof(delta_fec_params_));
  memset(&key_fec_params_, 0, sizeof(key_fec_params_));
  delta_fec_params_.max_fec_frames = key_fec_params_.max_fec_frames = 1;
  delta_fec_params_.fec_mask_type = key_fec_params_.fec_mask_type =
      kFecMaskRandom;
}

RTPSenderVideo::~RTPSenderVideo() {
}

void RTPSenderVideo::SetVideoCodecType(RtpVideoCodecTypes videoType) {
  _videoType = videoType;
}

RtpVideoCodecTypes RTPSenderVideo::VideoCodecType() const {
  return _videoType;
}

// Static.
RtpUtility::Payload* RTPSenderVideo::CreateVideoPayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const int8_t payloadType,
    const uint32_t maxBitRate) {
  RtpVideoCodecTypes videoType = kRtpVideoGeneric;
  if (RtpUtility::StringCompare(payloadName, "VP8", 3)) {
    videoType = kRtpVideoVp8;
  } else if (RtpUtility::StringCompare(payloadName, "VP9", 3)) {
    videoType = kRtpVideoVp9;
  } else if (RtpUtility::StringCompare(payloadName, "H264", 4)) {
    videoType = kRtpVideoH264;
  } else if (RtpUtility::StringCompare(payloadName, "I420", 4)) {
    videoType = kRtpVideoGeneric;
  } else {
    videoType = kRtpVideoGeneric;
  }
  RtpUtility::Payload* payload = new RtpUtility::Payload();
  payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
  payload->typeSpecific.Video.videoCodecType = videoType;
  payload->typeSpecific.Video.maxRate = maxBitRate;
  payload->audio = false;
  return payload;
}

void RTPSenderVideo::SendVideoPacket(uint8_t* data_buffer,
                                     const size_t payload_length,
                                     const size_t rtp_header_length,
                                     uint16_t seq_num,
                                     const uint32_t capture_timestamp,
                                     int64_t capture_time_ms,
                                     StorageType storage) {
  if (_rtpSender.SendToNetwork(data_buffer, payload_length, rtp_header_length,
                               capture_time_ms, storage,
                               RtpPacketSender::kLowPriority) == 0) {
    _videoBitrate.Update(payload_length + rtp_header_length);
    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                         "Video::PacketNormal", "timestamp", capture_timestamp,
                         "seqnum", seq_num);
  } else {
    LOG(LS_WARNING) << "Failed to send video packet " << seq_num;
  }
}

void RTPSenderVideo::SendVideoPacketAsRed(uint8_t* data_buffer,
                                          const size_t payload_length,
                                          const size_t rtp_header_length,
                                          uint16_t media_seq_num,
                                          const uint32_t capture_timestamp,
                                          int64_t capture_time_ms,
                                          StorageType media_packet_storage,
                                          bool protect) {
  rtc::scoped_ptr<RedPacket> red_packet;
  std::vector<RedPacket*> fec_packets;
  StorageType fec_storage = kDontRetransmit;
  uint16_t next_fec_sequence_number = 0;
  {
    // Only protect while creating RED and FEC packets, not when sending.
    CriticalSectionScoped cs(crit_.get());
    red_packet.reset(producer_fec_.BuildRedPacket(
        data_buffer, payload_length, rtp_header_length, red_payload_type_));
    if (protect) {
      producer_fec_.AddRtpPacketAndGenerateFec(data_buffer, payload_length,
                                               rtp_header_length);
    }
    uint16_t num_fec_packets = producer_fec_.NumAvailableFecPackets();
    if (num_fec_packets > 0) {
      next_fec_sequence_number =
          _rtpSender.AllocateSequenceNumber(num_fec_packets);
      fec_packets = producer_fec_.GetFecPackets(
          red_payload_type_, fec_payload_type_, next_fec_sequence_number,
          rtp_header_length);
      RTC_DCHECK_EQ(num_fec_packets, fec_packets.size());
      if (_retransmissionSettings & kRetransmitFECPackets)
        fec_storage = kAllowRetransmission;
    }
  }
  if (_rtpSender.SendToNetwork(
          red_packet->data(), red_packet->length() - rtp_header_length,
          rtp_header_length, capture_time_ms, media_packet_storage,
          RtpPacketSender::kLowPriority) == 0) {
    _videoBitrate.Update(red_packet->length());
    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                         "Video::PacketRed", "timestamp", capture_timestamp,
                         "seqnum", media_seq_num);
  } else {
    LOG(LS_WARNING) << "Failed to send RED packet " << media_seq_num;
  }
  for (RedPacket* fec_packet : fec_packets) {
    if (_rtpSender.SendToNetwork(
            fec_packet->data(), fec_packet->length() - rtp_header_length,
            rtp_header_length, capture_time_ms, fec_storage,
            RtpPacketSender::kLowPriority) == 0) {
      _fecOverheadRate.Update(fec_packet->length());
      TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                           "Video::PacketFec", "timestamp", capture_timestamp,
                           "seqnum", next_fec_sequence_number);
    } else {
      LOG(LS_WARNING) << "Failed to send FEC packet "
                      << next_fec_sequence_number;
    }
    delete fec_packet;
    ++next_fec_sequence_number;
  }
}

void RTPSenderVideo::SetGenericFECStatus(const bool enable,
                                         const uint8_t payloadTypeRED,
                                         const uint8_t payloadTypeFEC) {
  CriticalSectionScoped cs(crit_.get());
  fec_enabled_ = enable;
  red_payload_type_ = payloadTypeRED;
  fec_payload_type_ = payloadTypeFEC;
  memset(&delta_fec_params_, 0, sizeof(delta_fec_params_));
  memset(&key_fec_params_, 0, sizeof(key_fec_params_));
  delta_fec_params_.max_fec_frames = key_fec_params_.max_fec_frames = 1;
  delta_fec_params_.fec_mask_type = key_fec_params_.fec_mask_type =
      kFecMaskRandom;
}

void RTPSenderVideo::GenericFECStatus(bool* enable,
                                      uint8_t* payloadTypeRED,
                                      uint8_t* payloadTypeFEC) const {
  CriticalSectionScoped cs(crit_.get());
  *enable = fec_enabled_;
  *payloadTypeRED = red_payload_type_;
  *payloadTypeFEC = fec_payload_type_;
}

size_t RTPSenderVideo::FECPacketOverhead() const {
  CriticalSectionScoped cs(crit_.get());
  if (fec_enabled_) {
    // Overhead is FEC headers plus RED for FEC header plus anything in RTP
    // header beyond the 12 bytes base header (CSRC list, extensions...)
    // This reason for the header extensions to be included here is that
    // from an FEC viewpoint, they are part of the payload to be protected.
    // (The base RTP header is already protected by the FEC header.)
    return ForwardErrorCorrection::PacketOverhead() + REDForFECHeaderLength +
           (_rtpSender.RTPHeaderLength() - kRtpHeaderSize);
  }
  return 0;
}

void RTPSenderVideo::SetFecParameters(const FecProtectionParams* delta_params,
                                      const FecProtectionParams* key_params) {
  CriticalSectionScoped cs(crit_.get());
  RTC_DCHECK(delta_params);
  RTC_DCHECK(key_params);
  delta_fec_params_ = *delta_params;
  key_fec_params_ = *key_params;
}

int32_t RTPSenderVideo::SendVideo(const RtpVideoCodecTypes videoType,
                                  const FrameType frameType,
                                  const int8_t payloadType,
                                  const uint32_t captureTimeStamp,
                                  int64_t capture_time_ms,
                                  const uint8_t* payloadData,
                                  const size_t payloadSize,
                                  const RTPFragmentationHeader* fragmentation,
                                  const RTPVideoHeader* rtpHdr) {
  if (payloadSize == 0) {
    return -1;
  }

  rtc::scoped_ptr<RtpPacketizer> packetizer(
      RtpPacketizer::Create(videoType, _rtpSender.MaxDataPayloadLength(),
                            &(rtpHdr->codecHeader), frameType));

  StorageType storage;
  bool fec_enabled;
  {
    CriticalSectionScoped cs(crit_.get());
    FecProtectionParams* fec_params =
        frameType == kVideoFrameKey ? &key_fec_params_ : &delta_fec_params_;
    producer_fec_.SetFecParameters(fec_params, 0);
    storage = packetizer->GetStorageType(_retransmissionSettings);
    fec_enabled = fec_enabled_;
  }

  // Register CVO rtp header extension at the first time when we receive a frame
  // with pending rotation.
  RTPSenderInterface::CVOMode cvo_mode = RTPSenderInterface::kCVONone;
  if (rtpHdr && rtpHdr->rotation != kVideoRotation_0) {
    cvo_mode = _rtpSender.ActivateCVORtpHeaderExtension();
  }

  uint16_t rtp_header_length = _rtpSender.RTPHeaderLength();
  size_t payload_bytes_to_send = payloadSize;
  const uint8_t* data = payloadData;

  // TODO(changbin): we currently don't support to configure the codec to
  // output multiple partitions for VP8. Should remove below check after the
  // issue is fixed.
  const RTPFragmentationHeader* frag =
      (videoType == kRtpVideoVp8) ? NULL : fragmentation;

  packetizer->SetPayloadData(data, payload_bytes_to_send, frag);

  bool last = false;
  while (!last) {
    uint8_t dataBuffer[IP_PACKET_SIZE] = {0};
    size_t payload_bytes_in_packet = 0;
    if (!packetizer->NextPacket(&dataBuffer[rtp_header_length],
                                &payload_bytes_in_packet, &last)) {
      return -1;
    }
    // Write RTP header.
    // Set marker bit true if this is the last packet in frame.
    _rtpSender.BuildRTPheader(
        dataBuffer, payloadType, last, captureTimeStamp, capture_time_ms);
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
    // Here we are adding it to every packet of every frame at this point.
    if (!rtpHdr) {
      RTC_DCHECK(!_rtpSender.IsRtpHeaderExtensionRegistered(
          kRtpExtensionVideoRotation));
    } else if (cvo_mode == RTPSenderInterface::kCVOActivated) {
      // Checking whether CVO header extension is registered will require taking
      // a lock. It'll be a no-op if it's not registered.
      // TODO(guoweis): For now, all packets sent will carry the CVO such that
      // the RTP header length is consistent, although the receiver side will
      // only exam the packets with marker bit set.
      size_t packetSize = payloadSize + rtp_header_length;
      RtpUtility::RtpHeaderParser rtp_parser(dataBuffer, packetSize);
      RTPHeader rtp_header;
      rtp_parser.Parse(&rtp_header);
      _rtpSender.UpdateVideoRotation(dataBuffer, packetSize, rtp_header,
                                     rtpHdr->rotation);
    }
    if (fec_enabled) {
      SendVideoPacketAsRed(dataBuffer, payload_bytes_in_packet,
                           rtp_header_length, _rtpSender.SequenceNumber(),
                           captureTimeStamp, capture_time_ms, storage,
                           packetizer->GetProtectionType() == kProtectedPacket);
    } else {
      SendVideoPacket(dataBuffer, payload_bytes_in_packet, rtp_header_length,
                      _rtpSender.SequenceNumber(), captureTimeStamp,
                      capture_time_ms, storage);
    }
  }

  TRACE_EVENT_ASYNC_END1(
      "webrtc", "Video", capture_time_ms, "timestamp", _rtpSender.Timestamp());
  return 0;
}

void RTPSenderVideo::SetMaxConfiguredBitrateVideo(const uint32_t maxBitrate) {
  _maxBitrate = maxBitrate;
}

uint32_t RTPSenderVideo::MaxConfiguredBitrateVideo() const {
  return _maxBitrate;
}

void RTPSenderVideo::ProcessBitrate() {
  _videoBitrate.Process();
  _fecOverheadRate.Process();
}

uint32_t RTPSenderVideo::VideoBitrateSent() const {
  return _videoBitrate.BitrateLast();
}

uint32_t RTPSenderVideo::FecOverheadRate() const {
  return _fecOverheadRate.BitrateLast();
}

int RTPSenderVideo::SelectiveRetransmissions() const {
  CriticalSectionScoped cs(crit_.get());
  return _retransmissionSettings;
}

void RTPSenderVideo::SetSelectiveRetransmissions(uint8_t settings) {
  CriticalSectionScoped cs(crit_.get());
  _retransmissionSettings = settings;
}

}  // namespace webrtc
