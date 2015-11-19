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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/producer_fec.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format_h264.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {
enum { REDForFECHeaderLength = 1 };

struct RtpPacket {
  uint16_t rtpHeaderLength;
  ForwardErrorCorrection::Packet* pkt;
};

RTPSenderVideo::RTPSenderVideo(Clock* clock, RTPSenderInterface* rtpSender)
    : _rtpSender(*rtpSender),
      _videoType(kRtpVideoGeneric),
      _videoCodecInformation(NULL),
      _maxBitrate(0),
      _retransmissionSettings(kRetransmitBaseLayer),

      // Generic FEC
      _fec(),
      _fecEnabled(false),
      _payloadTypeRED(-1),
      _payloadTypeFEC(-1),
      _numberFirstPartition(0),
      delta_fec_params_(),
      key_fec_params_(),
      producer_fec_(&_fec),
      _fecOverheadRate(clock, NULL),
      _videoBitrate(clock, NULL) {
  memset(&delta_fec_params_, 0, sizeof(delta_fec_params_));
  memset(&key_fec_params_, 0, sizeof(key_fec_params_));
  delta_fec_params_.max_fec_frames = key_fec_params_.max_fec_frames = 1;
  delta_fec_params_.fec_mask_type = key_fec_params_.fec_mask_type =
      kFecMaskRandom;
}

RTPSenderVideo::~RTPSenderVideo() {
  if (_videoCodecInformation) {
    delete _videoCodecInformation;
  }
}

void RTPSenderVideo::SetVideoCodecType(RtpVideoCodecTypes videoType) {
  _videoType = videoType;
}

RtpVideoCodecTypes RTPSenderVideo::VideoCodecType() const {
  return _videoType;
}

int32_t RTPSenderVideo::RegisterVideoPayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const int8_t payloadType,
    const uint32_t maxBitRate,
    RtpUtility::Payload*& payload) {
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
  payload = new RtpUtility::Payload;
  payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
  payload->typeSpecific.Video.videoCodecType = videoType;
  payload->typeSpecific.Video.maxRate = maxBitRate;
  payload->audio = false;
  return 0;
}

int32_t RTPSenderVideo::SendVideoPacket(uint8_t* data_buffer,
                                        const size_t payload_length,
                                        const size_t rtp_header_length,
                                        const uint32_t capture_timestamp,
                                        int64_t capture_time_ms,
                                        StorageType storage,
                                        bool protect) {
  if (_fecEnabled) {
    int ret = 0;
    size_t fec_overhead_sent = 0;
    size_t video_sent = 0;

    RedPacket* red_packet = producer_fec_.BuildRedPacket(
        data_buffer, payload_length, rtp_header_length, _payloadTypeRED);
    TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                         "Video::PacketRed", "timestamp", capture_timestamp,
                         "seqnum", _rtpSender.SequenceNumber());
    // Sending the media packet with RED header.
    int packet_success =
        _rtpSender.SendToNetwork(red_packet->data(),
                                 red_packet->length() - rtp_header_length,
                                 rtp_header_length,
                                 capture_time_ms,
                                 storage,
                                 PacedSender::kNormalPriority);

    ret |= packet_success;

    if (packet_success == 0) {
      video_sent += red_packet->length();
    }
    delete red_packet;
    red_packet = NULL;

    if (protect) {
      ret = producer_fec_.AddRtpPacketAndGenerateFec(
          data_buffer, payload_length, rtp_header_length);
      if (ret != 0)
        return ret;
    }

    while (producer_fec_.FecAvailable()) {
      red_packet =
          producer_fec_.GetFecPacket(_payloadTypeRED,
                                     _payloadTypeFEC,
                                     _rtpSender.IncrementSequenceNumber(),
                                     rtp_header_length);
      StorageType storage = kDontRetransmit;
      if (_retransmissionSettings & kRetransmitFECPackets) {
        storage = kAllowRetransmission;
      }
      TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                           "Video::PacketFec", "timestamp", capture_timestamp,
                           "seqnum", _rtpSender.SequenceNumber());
      // Sending FEC packet with RED header.
      int packet_success =
          _rtpSender.SendToNetwork(red_packet->data(),
                                   red_packet->length() - rtp_header_length,
                                   rtp_header_length,
                                   capture_time_ms,
                                   storage,
                                   PacedSender::kNormalPriority);

      ret |= packet_success;

      if (packet_success == 0) {
        fec_overhead_sent += red_packet->length();
      }
      delete red_packet;
      red_packet = NULL;
    }
    _videoBitrate.Update(video_sent);
    _fecOverheadRate.Update(fec_overhead_sent);
    return ret;
  }
  TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "Video::PacketNormal", "timestamp", capture_timestamp,
                       "seqnum", _rtpSender.SequenceNumber());
  int ret = _rtpSender.SendToNetwork(data_buffer,
                                     payload_length,
                                     rtp_header_length,
                                     capture_time_ms,
                                     storage,
                                     PacedSender::kNormalPriority);
  if (ret == 0) {
    _videoBitrate.Update(payload_length + rtp_header_length);
  }
  return ret;
}

int32_t RTPSenderVideo::SendRTPIntraRequest() {
  // RFC 2032
  // 5.2.1.  Full intra-frame Request (FIR) packet

  size_t length = 8;
  uint8_t data[8];
  data[0] = 0x80;
  data[1] = 192;
  data[2] = 0;
  data[3] = 1;  // length

  ByteWriter<uint32_t>::WriteBigEndian(data + 4, _rtpSender.SSRC());

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("webrtc_rtp"),
                       "Video::IntraRequest", "seqnum",
                       _rtpSender.SequenceNumber());
  return _rtpSender.SendToNetwork(
      data, 0, length, -1, kDontStore, PacedSender::kNormalPriority);
}

int32_t RTPSenderVideo::SetGenericFECStatus(const bool enable,
                                            const uint8_t payloadTypeRED,
                                            const uint8_t payloadTypeFEC) {
  _fecEnabled = enable;
  _payloadTypeRED = payloadTypeRED;
  _payloadTypeFEC = payloadTypeFEC;
  memset(&delta_fec_params_, 0, sizeof(delta_fec_params_));
  memset(&key_fec_params_, 0, sizeof(key_fec_params_));
  delta_fec_params_.max_fec_frames = key_fec_params_.max_fec_frames = 1;
  delta_fec_params_.fec_mask_type = key_fec_params_.fec_mask_type =
      kFecMaskRandom;
  return 0;
}

int32_t RTPSenderVideo::GenericFECStatus(bool& enable,
                                         uint8_t& payloadTypeRED,
                                         uint8_t& payloadTypeFEC) const {
  enable = _fecEnabled;
  payloadTypeRED = _payloadTypeRED;
  payloadTypeFEC = _payloadTypeFEC;
  return 0;
}

size_t RTPSenderVideo::FECPacketOverhead() const {
  if (_fecEnabled) {
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

int32_t RTPSenderVideo::SetFecParameters(
    const FecProtectionParams* delta_params,
    const FecProtectionParams* key_params) {
  assert(delta_params);
  assert(key_params);
  delta_fec_params_ = *delta_params;
  key_fec_params_ = *key_params;
  return 0;
}

int32_t RTPSenderVideo::SendVideo(const RtpVideoCodecTypes videoType,
                                  const FrameType frameType,
                                  const int8_t payloadType,
                                  const uint32_t captureTimeStamp,
                                  int64_t capture_time_ms,
                                  const uint8_t* payloadData,
                                  const size_t payloadSize,
                                  const RTPFragmentationHeader* fragmentation,
                                  VideoCodecInformation* codecInfo,
                                  const RTPVideoHeader* rtpHdr) {
  if (payloadSize == 0) {
    return -1;
  }

  if (frameType == kVideoFrameKey) {
    producer_fec_.SetFecParameters(&key_fec_params_, _numberFirstPartition);
  } else {
    producer_fec_.SetFecParameters(&delta_fec_params_, _numberFirstPartition);
  }

  // Default setting for number of first partition packets:
  // Will be extracted in SendVP8 for VP8 codec; other codecs use 0
  _numberFirstPartition = 0;

  return Send(videoType, frameType, payloadType, captureTimeStamp,
              capture_time_ms, payloadData, payloadSize, fragmentation, rtpHdr)
             ? 0
             : -1;
}

VideoCodecInformation* RTPSenderVideo::CodecInformationVideo() {
  return _videoCodecInformation;
}

void RTPSenderVideo::SetMaxConfiguredBitrateVideo(const uint32_t maxBitrate) {
  _maxBitrate = maxBitrate;
}

uint32_t RTPSenderVideo::MaxConfiguredBitrateVideo() const {
  return _maxBitrate;
}

bool RTPSenderVideo::Send(const RtpVideoCodecTypes videoType,
                          const FrameType frameType,
                          const int8_t payloadType,
                          const uint32_t captureTimeStamp,
                          int64_t capture_time_ms,
                          const uint8_t* payloadData,
                          const size_t payloadSize,
                          const RTPFragmentationHeader* fragmentation,
                          const RTPVideoHeader* rtpHdr) {
  // Register CVO rtp header extension at the first time when we receive a frame
  // with pending rotation.
  RTPSenderInterface::CVOMode cvo_mode = RTPSenderInterface::kCVONone;
  if (rtpHdr && rtpHdr->rotation != kVideoRotation_0) {
    cvo_mode = _rtpSender.ActivateCVORtpHeaderExtension();
  }

  uint16_t rtp_header_length = _rtpSender.RTPHeaderLength();
  size_t payload_bytes_to_send = payloadSize;
  const uint8_t* data = payloadData;
  size_t max_payload_length = _rtpSender.MaxDataPayloadLength();

  rtc::scoped_ptr<RtpPacketizer> packetizer(RtpPacketizer::Create(
      videoType, max_payload_length, &(rtpHdr->codecHeader), frameType));

  // TODO(changbin): we currently don't support to configure the codec to
  // output multiple partitions for VP8. Should remove below check after the
  // issue is fixed.
  const RTPFragmentationHeader* frag =
      (videoType == kRtpVideoVp8 || videoType == kRtpVideoVp9) ? NULL : fragmentation;

  packetizer->SetPayloadData(data, payload_bytes_to_send, frag);

  bool last = false;
  while (!last) {
    uint8_t dataBuffer[IP_PACKET_SIZE] = {0};
    size_t payload_bytes_in_packet = 0;
    if (!packetizer->NextPacket(
            &dataBuffer[rtp_header_length], &payload_bytes_in_packet, &last)) {
      return false;
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
      assert(!_rtpSender.IsRtpHeaderExtensionRegistered(
          kRtpExtensionVideoRotation));
    } else if (cvo_mode == RTPSenderInterface::kCVOActivated) {
      // Checking whether CVO header extension is registered will require taking
      // a lock. It'll be a no-op if it's not registered.
      // TODO(guoweis): For now, all packets sent will carry the CVO such that
      // the RTP header length is consistent, although the receiver side will
      // only exam the packets with market bit set.
      size_t packetSize = payloadSize + rtp_header_length;
      RtpUtility::RtpHeaderParser rtp_parser(dataBuffer, packetSize);
      RTPHeader rtp_header;
      rtp_parser.Parse(rtp_header);
      _rtpSender.UpdateVideoRotation(dataBuffer, packetSize, rtp_header,
                                     rtpHdr->rotation);
    }
    if (SendVideoPacket(dataBuffer,
                        payload_bytes_in_packet,
                        rtp_header_length,
                        captureTimeStamp,
                        capture_time_ms,
                        packetizer->GetStorageType(_retransmissionSettings),
                        packetizer->GetProtectionType() == kProtectedPacket)) {
      LOG(LS_WARNING) << packetizer->ToString()
                      << " failed to send packet number "
                      << _rtpSender.SequenceNumber();
    }
  }

  TRACE_EVENT_ASYNC_END1(
      "webrtc", "Video", capture_time_ms, "timestamp", _rtpSender.Timestamp());
  return true;
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
  return _retransmissionSettings;
}

int RTPSenderVideo::SetSelectiveRetransmissions(uint8_t settings) {
  _retransmissionSettings = settings;
  return 0;
}

}  // namespace webrtc
