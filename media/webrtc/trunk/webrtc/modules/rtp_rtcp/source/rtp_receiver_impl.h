/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_IMPL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_IMPL_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver_strategy.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RtpReceiverImpl : public RtpReceiver {
 public:
  // Callbacks passed in here may not be NULL (use Null Object callbacks if you
  // want callbacks to do nothing). This class takes ownership of the media
  // receiver but nothing else.
  RtpReceiverImpl(int32_t id,
                  Clock* clock,
                  RtpAudioFeedback* incoming_audio_messages_callback,
                  RtpFeedback* incoming_messages_callback,
                  RTPPayloadRegistry* rtp_payload_registry,
                  RTPReceiverStrategy* rtp_media_receiver);

  virtual ~RtpReceiverImpl();

  RTPReceiverStrategy* GetMediaReceiver() const;

  int32_t RegisterReceivePayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payload_type,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate);

  int32_t DeRegisterReceivePayload(const int8_t payload_type);

  bool IncomingRtpPacket(
      const RTPHeader& rtp_header,
      const uint8_t* payload,
      int payload_length,
      PayloadUnion payload_specific,
      bool in_order);

  NACKMethod NACK() const;

  // Turn negative acknowledgement requests on/off.
  void SetNACKStatus(const NACKMethod method);

  // Returns the last received timestamp.
  bool Timestamp(uint32_t* timestamp) const;
  bool LastReceivedTimeMs(int64_t* receive_time_ms) const;

  uint32_t SSRC() const;

  int32_t CSRCs(uint32_t array_of_csrc[kRtpCsrcSize]) const;

  int32_t Energy(uint8_t array_of_energy[kRtpCsrcSize]) const;

  // RTX.
  void SetRTXStatus(bool enable, uint32_t ssrc);

  void RTXStatus(bool* enable, uint32_t* ssrc, int* payload_type) const;

  void SetRtxPayloadType(int payload_type);

  TelephoneEventHandler* GetTelephoneEventHandler();

 private:
  bool HaveReceivedFrame() const;

  RtpVideoCodecTypes VideoCodecType() const;

  void CheckSSRCChanged(const RTPHeader& rtp_header);
  void CheckCSRC(const WebRtcRTPHeader& rtp_header);
  int32_t CheckPayloadChanged(const RTPHeader& rtp_header,
                              const int8_t first_payload_byte,
                              bool& is_red,
                              PayloadUnion* payload,
                              bool* should_reset_statistics);

  Clock* clock_;
  RTPPayloadRegistry* rtp_payload_registry_;
  scoped_ptr<RTPReceiverStrategy> rtp_media_receiver_;

  int32_t id_;

  RtpFeedback* cb_rtp_feedback_;

  scoped_ptr<CriticalSectionWrapper> critical_section_rtp_receiver_;
  int64_t last_receive_time_;
  uint16_t last_received_payload_length_;

  // SSRCs.
  uint32_t ssrc_;
  uint8_t num_csrcs_;
  uint32_t current_remote_csrc_[kRtpCsrcSize];

  uint32_t last_received_timestamp_;
  int64_t last_received_frame_time_ms_;
  uint16_t last_received_sequence_number_;

  NACKMethod nack_method_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_IMPL_H_
