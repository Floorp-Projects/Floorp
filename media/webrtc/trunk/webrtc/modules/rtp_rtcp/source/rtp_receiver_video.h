/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;
class ModuleRtpRtcpImpl;
class ReceiverFEC;
class RTPReceiver;
class RTPPayloadRegistry;

class RTPReceiverVideo : public RTPReceiverStrategy {
 public:
  RTPReceiverVideo(const int32_t id,
                   const RTPPayloadRegistry* rtp_payload_registry,
                   RtpData* data_callback);

  virtual ~RTPReceiverVideo();

  int32_t ParseRtpPacket(
      WebRtcRTPHeader* rtp_header,
      const ModuleRTPUtility::PayloadUnion& specific_payload,
      const bool is_red,
      const uint8_t* packet,
      const uint16_t packet_length,
      const int64_t timestamp,
      const bool is_first_packet);

  int32_t GetFrequencyHz() const;

  RTPAliveType ProcessDeadOrAlive(uint16_t last_payload_length) const;

  bool ShouldReportCsrcChanges(uint8_t payload_type) const;

  int32_t OnNewPayloadTypeCreated(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payload_type,
      const uint32_t frequency);

  int32_t InvokeOnInitializeDecoder(
      RtpFeedback* callback,
      const int32_t id,
      const int8_t payload_type,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const ModuleRTPUtility::PayloadUnion& specific_payload) const;

  virtual int32_t ReceiveRecoveredPacketCallback(
      WebRtcRTPHeader* rtp_header,
      const uint8_t* payload_data,
      const uint16_t payload_data_length);

  void SetPacketOverHead(uint16_t packet_over_head);

 protected:
  int32_t SetCodecType(const RtpVideoCodecTypes video_type,
                       WebRtcRTPHeader* rtp_header) const;

  int32_t ParseVideoCodecSpecificSwitch(
      WebRtcRTPHeader* rtp_header,
      const uint8_t* payload_data,
      const uint16_t payload_data_length,
      const RtpVideoCodecTypes video_type,
      const bool is_first_packet);

  int32_t ReceiveGenericCodec(WebRtcRTPHeader* rtp_header,
                              const uint8_t* payload_data,
                              const uint16_t payload_data_length);

  int32_t ReceiveVp8Codec(WebRtcRTPHeader* rtp_header,
                          const uint8_t* payload_data,
                          const uint16_t payload_data_length);

  int32_t BuildRTPheader(const WebRtcRTPHeader* rtp_header,
                         uint8_t* data_buffer) const;

 private:
  int32_t ParseVideoCodecSpecific(
      WebRtcRTPHeader* rtp_header,
      const uint8_t* payload_data,
      const uint16_t payload_data_length,
      const RtpVideoCodecTypes video_type,
      const bool is_red,
      const uint8_t* incoming_rtp_packet,
      const uint16_t incoming_rtp_packet_size,
      const int64_t now_ms,
      const bool is_first_packet);

  int32_t id_;
  const RTPPayloadRegistry* rtp_rtp_payload_registry_;

  CriticalSectionWrapper* critical_section_receiver_video_;

  // FEC
  bool current_fec_frame_decoded_;
  ReceiverFEC* receive_fec_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_VIDEO_H_
