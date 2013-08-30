/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_H_

#include <map>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver_help.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class RtpRtcpFeedback;
class ModuleRtpRtcpImpl;
class Trace;
class RTPReceiverAudio;
class RTPReceiverVideo;
class RTPReceiverStrategy;

class RTPReceiver : public Bitrate {
 public:
  // Callbacks passed in here may not be NULL (use Null Object callbacks if you
  // want callbacks to do nothing). This class takes ownership of the media
  // receiver but nothing else.
  RTPReceiver(const int32_t id,
              Clock* clock,
              ModuleRtpRtcpImpl* owner,
              RtpAudioFeedback* incoming_audio_messages_callback,
              RtpData* incoming_payload_callback,
              RtpFeedback* incoming_messages_callback,
              RTPReceiverStrategy* rtp_media_receiver,
              RTPPayloadRegistry* rtp_payload_registry);

  virtual ~RTPReceiver();

  RtpVideoCodecTypes VideoCodecType() const;
  uint32_t MaxConfiguredBitrate() const;

  int32_t SetPacketTimeout(const uint32_t timeout_ms);
  void PacketTimeout();

  void ProcessDeadOrAlive(const bool RTCPalive, const int64_t now);

  void ProcessBitrate();

  int32_t RegisterReceivePayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payload_type,
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate);

  int32_t DeRegisterReceivePayload(const int8_t payload_type);

  int32_t ReceivePayloadType(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const uint32_t frequency,
      const uint8_t channels,
      const uint32_t rate,
      int8_t* payload_type) const;

  int32_t IncomingRTPPacket(
      RTPHeader* rtpheader,
      const uint8_t* incoming_rtp_packet,
      const uint16_t incoming_rtp_packet_length);

  NACKMethod NACK() const ;

  // Turn negative acknowledgement requests on/off.
  int32_t SetNACKStatus(const NACKMethod method, int max_reordering_threshold);

  // Returns the last received timestamp.
  virtual uint32_t TimeStamp() const;
  int32_t LastReceivedTimeMs() const;
  virtual uint16_t SequenceNumber() const;

  int32_t EstimatedRemoteTimeStamp(uint32_t& timestamp) const;

  uint32_t SSRC() const;

  int32_t CSRCs(uint32_t array_of_csrc[kRtpCsrcSize]) const;

  int32_t Energy(uint8_t array_of_energy[kRtpCsrcSize]) const;

  // Get the currently configured SSRC filter.
  int32_t SSRCFilter(uint32_t& allowed_ssrc) const;

  // Set a SSRC to be used as a filter for incoming RTP streams.
  int32_t SetSSRCFilter(const bool enable, const uint32_t allowed_ssrc);

  int32_t Statistics(uint8_t*  fraction_lost,
                     uint32_t* cum_lost,
                     uint32_t* ext_max,
                     uint32_t* jitter,  // Will be moved from JB.
                     uint32_t* max_jitter,
                     uint32_t* jitter_transmission_time_offset,
                     bool reset) const;

  int32_t Statistics(uint8_t*  fraction_lost,
                     uint32_t* cum_lost,
                     uint32_t* ext_max,
                     uint32_t* jitter,  // Will be moved from JB.
                     uint32_t* max_jitter,
                     uint32_t* jitter_transmission_time_offset,
                     int32_t* missing,
                     bool reset) const;

  int32_t DataCounters(uint32_t* bytes_received,
                       uint32_t* packets_received) const;

  int32_t ResetStatistics();

  int32_t ResetDataCounters();

  uint16_t PacketOHReceived() const;

  uint32_t PacketCountReceived() const;

  uint32_t ByteCountReceived() const;

  int32_t RegisterRtpHeaderExtension(const RTPExtensionType type,
                                     const uint8_t id);

  int32_t DeregisterRtpHeaderExtension(const RTPExtensionType type);

  void GetHeaderExtensionMapCopy(RtpHeaderExtensionMap* map) const;

  // RTX.
  void SetRTXStatus(bool enable, uint32_t ssrc);

  void RTXStatus(bool* enable, uint32_t* ssrc, int* payload_type) const;

  void SetRtxPayloadType(int payload_type);

  virtual int8_t REDPayloadType() const;

  bool HaveNotReceivedPackets() const;

  virtual bool RetransmitOfOldPacket(const uint16_t sequence_number,
                                     const uint32_t rtp_time_stamp) const;

  void UpdateStatistics(const RTPHeader* rtp_header,
                        const uint16_t bytes,
                        const bool old_packet);

 private:
  // Returns whether RED is configured with payload_type.
  bool REDPayloadType(const int8_t payload_type) const;

  bool InOrderPacket(const uint16_t sequence_number) const;

  void CheckSSRCChanged(const RTPHeader* rtp_header);
  void CheckCSRC(const WebRtcRTPHeader* rtp_header);
  int32_t CheckPayloadChanged(const RTPHeader* rtp_header,
                              const int8_t first_payload_byte,
                              bool& isRED,
                              ModuleRTPUtility::PayloadUnion* payload);

  void UpdateNACKBitRate(int32_t bytes, uint32_t now);
  bool ProcessNACKBitRate(uint32_t now);

  RTPPayloadRegistry*             rtp_payload_registry_;
  scoped_ptr<RTPReceiverStrategy> rtp_media_receiver_;

  int32_t           id_;
  ModuleRtpRtcpImpl&      rtp_rtcp_;

  RtpFeedback*            cb_rtp_feedback_;

  CriticalSectionWrapper* critical_section_rtp_receiver_;
  mutable int64_t   last_receive_time_;
  uint16_t          last_received_payload_length_;

  uint32_t          packet_timeout_ms_;

  // SSRCs.
  uint32_t            ssrc_;
  uint8_t             num_csrcs_;
  uint32_t            current_remote_csrc_[kRtpCsrcSize];
  uint8_t             num_energy_;
  uint8_t             current_remote_energy_[kRtpCsrcSize];

  bool                      use_ssrc_filter_;
  uint32_t            ssrc_filter_;

  // Stats on received RTP packets.
  uint32_t            jitter_q4_;
  mutable uint32_t    jitter_max_q4_;
  mutable uint32_t    cumulative_loss_;
  uint32_t            jitter_q4_transmission_time_offset_;

  uint32_t            local_time_last_received_timestamp_;
  int64_t                   last_received_frame_time_ms_;
  uint32_t            last_received_timestamp_;
  uint16_t            last_received_sequence_number_;
  int32_t             last_received_transmission_time_offset_;
  uint16_t            received_seq_first_;
  uint16_t            received_seq_max_;
  uint16_t            received_seq_wraps_;

  // Current counter values.
  uint16_t            received_packet_oh_;
  uint32_t            received_byte_count_;
  uint32_t            received_old_packet_count_;
  uint32_t            received_inorder_packet_count_;

  // Counter values when we sent the last report.
  mutable uint32_t    last_report_inorder_packets_;
  mutable uint32_t    last_report_old_packets_;
  mutable uint16_t    last_report_seq_max_;
  mutable uint8_t     last_report_fraction_lost_;
  mutable uint32_t    last_report_cumulative_lost_;  // 24 bits valid.
  mutable uint32_t    last_report_extended_high_seq_num_;
  mutable uint32_t    last_report_jitter_;
  mutable uint32_t    last_report_jitter_transmission_time_offset_;

  NACKMethod nack_method_;
  int max_reordering_threshold_;

  bool rtx_;
  uint32_t ssrc_rtx_;
  int payload_type_rtx_;
};

} // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_H_
