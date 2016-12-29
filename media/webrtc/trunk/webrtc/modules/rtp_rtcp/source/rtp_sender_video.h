/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_

#include <list>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"
#include "webrtc/modules/rtp_rtcp/source/producer_fec.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/video_codec_information.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;
struct RtpPacket;

class RTPSenderVideo {
 public:
  RTPSenderVideo(Clock* clock, RTPSenderInterface* rtpSender);
  virtual ~RTPSenderVideo();

  virtual RtpVideoCodecTypes VideoCodecType() const;

  size_t FECPacketOverhead() const;

  static RtpUtility::Payload* CreateVideoPayload(
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payloadType,
      const uint32_t maxBitRate);

  int32_t SendVideo(const RtpVideoCodecTypes videoType,
                    const FrameType frameType,
                    const int8_t payloadType,
                    const uint32_t captureTimeStamp,
                    int64_t capture_time_ms,
                    const uint8_t* payloadData,
                    const size_t payloadSize,
                    const RTPFragmentationHeader* fragmentation,
                    const RTPVideoHeader* rtpHdr);

  int32_t SendRTPIntraRequest();

  void SetVideoCodecType(RtpVideoCodecTypes type);

  void SetMaxConfiguredBitrateVideo(const uint32_t maxBitrate);

  uint32_t MaxConfiguredBitrateVideo() const;

  // FEC
  void SetGenericFECStatus(const bool enable,
                           const uint8_t payloadTypeRED,
                           const uint8_t payloadTypeFEC);

  void GenericFECStatus(bool* enable,
                        uint8_t* payloadTypeRED,
                        uint8_t* payloadTypeFEC) const;

  void SetFecParameters(const FecProtectionParams* delta_params,
                        const FecProtectionParams* key_params);

  void ProcessBitrate();

  uint32_t VideoBitrateSent() const;
  uint32_t FecOverheadRate() const;

  int SelectiveRetransmissions() const;
  void SetSelectiveRetransmissions(uint8_t settings);

 private:
  void SendVideoPacket(uint8_t* dataBuffer,
                       const size_t payloadLength,
                       const size_t rtpHeaderLength,
                       uint16_t seq_num,
                       const uint32_t capture_timestamp,
                       int64_t capture_time_ms,
                       StorageType storage);

  void SendVideoPacketAsRed(uint8_t* dataBuffer,
                            const size_t payloadLength,
                            const size_t rtpHeaderLength,
                            uint16_t video_seq_num,
                            const uint32_t capture_timestamp,
                            int64_t capture_time_ms,
                            StorageType media_packet_storage,
                            bool protect);

  RTPSenderInterface& _rtpSender;

  // Should never be held when calling out of this class.
  const rtc::scoped_ptr<CriticalSectionWrapper> crit_;

  RtpVideoCodecTypes _videoType;
  uint32_t _maxBitrate;
  int32_t _retransmissionSettings GUARDED_BY(crit_);

  // FEC
  ForwardErrorCorrection fec_;
  bool fec_enabled_ GUARDED_BY(crit_);
  int8_t red_payload_type_ GUARDED_BY(crit_);
  int8_t fec_payload_type_ GUARDED_BY(crit_);
  FecProtectionParams delta_fec_params_ GUARDED_BY(crit_);
  FecProtectionParams key_fec_params_ GUARDED_BY(crit_);
  ProducerFec producer_fec_ GUARDED_BY(crit_);

  // Bitrate used for FEC payload, RED headers, RTP headers for FEC packets
  // and any padding overhead.
  Bitrate _fecOverheadRate;
  // Bitrate used for video payload and RTP headers
  Bitrate _videoBitrate;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
