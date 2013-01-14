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
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
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
  RTPReceiver(const WebRtc_Word32 id,
              const bool audio,
              RtpRtcpClock* clock,
              ModuleRtpRtcpImpl* owner,
              RtpAudioFeedback* incoming_messages_callback);

  virtual ~RTPReceiver();

  RtpVideoCodecTypes VideoCodecType() const;
  WebRtc_UWord32 MaxConfiguredBitrate() const;

  WebRtc_Word32 SetPacketTimeout(const WebRtc_UWord32 timeout_ms);
  void PacketTimeout();

  void ProcessDeadOrAlive(const bool RTCPalive, const WebRtc_Word64 now);

  void ProcessBitrate();

  WebRtc_Word32 RegisterIncomingDataCallback(RtpData* incoming_data_callback);
  WebRtc_Word32 RegisterIncomingRTPCallback(
      RtpFeedback* incoming_messages_callback);

  WebRtc_Word32 RegisterReceivePayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_Word8 payload_type,
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate);

  WebRtc_Word32 DeRegisterReceivePayload(const WebRtc_Word8 payload_type);

  WebRtc_Word32 ReceivePayloadType(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const WebRtc_UWord32 frequency,
      const WebRtc_UWord8 channels,
      const WebRtc_UWord32 rate,
      WebRtc_Word8* payload_type) const;

  WebRtc_Word32 ReceivePayload(const WebRtc_Word8 payload_type,
                               char payload_name[RTP_PAYLOAD_NAME_SIZE],
                               WebRtc_UWord32* frequency,
                               WebRtc_UWord8* channels,
                               WebRtc_UWord32* rate) const;

  WebRtc_Word32 RemotePayload(char payload_name[RTP_PAYLOAD_NAME_SIZE],
                              WebRtc_Word8* payload_type,
                              WebRtc_UWord32* frequency,
                              WebRtc_UWord8* channels) const;

  WebRtc_Word32 IncomingRTPPacket(
      WebRtcRTPHeader* rtpheader,
      const WebRtc_UWord8* incoming_rtp_packet,
      const WebRtc_UWord16 incoming_rtp_packet_length);

  NACKMethod NACK() const ;

  // Turn negative acknowledgement requests on/off.
  WebRtc_Word32 SetNACKStatus(const NACKMethod method);

  // Returns the last received timestamp.
  virtual WebRtc_UWord32 TimeStamp() const;
  int32_t LastReceivedTimeMs() const;
  virtual WebRtc_UWord16 SequenceNumber() const;

  WebRtc_Word32 EstimatedRemoteTimeStamp(WebRtc_UWord32& timestamp) const;

  WebRtc_UWord32 SSRC() const;

  WebRtc_Word32 CSRCs(WebRtc_UWord32 array_of_csrc[kRtpCsrcSize]) const;

  WebRtc_Word32 Energy(WebRtc_UWord8 array_of_energy[kRtpCsrcSize]) const;

  // Get the currently configured SSRC filter.
  WebRtc_Word32 SSRCFilter(WebRtc_UWord32& allowed_ssrc) const;

  // Set a SSRC to be used as a filter for incoming RTP streams.
  WebRtc_Word32 SetSSRCFilter(const bool enable,
                              const WebRtc_UWord32 allowed_ssrc);

  WebRtc_Word32 Statistics(WebRtc_UWord8*  fraction_lost,
                           WebRtc_UWord32* cum_lost,
                           WebRtc_UWord32* ext_max,
                           WebRtc_UWord32* jitter,  // Will be moved from JB.
                           WebRtc_UWord32* max_jitter,
                           WebRtc_UWord32* jitter_transmission_time_offset,
                           bool reset) const;

  WebRtc_Word32 Statistics(WebRtc_UWord8*  fraction_lost,
                           WebRtc_UWord32* cum_lost,
                           WebRtc_UWord32* ext_max,
                           WebRtc_UWord32* jitter,  // Will be moved from JB.
                           WebRtc_UWord32* max_jitter,
                           WebRtc_UWord32* jitter_transmission_time_offset,
                           WebRtc_Word32* missing,
                           bool reset) const;

  WebRtc_Word32 DataCounters(WebRtc_UWord32* bytes_received,
                             WebRtc_UWord32* packets_received) const;

  WebRtc_Word32 ResetStatistics();

  WebRtc_Word32 ResetDataCounters();

  WebRtc_UWord16 PacketOHReceived() const;

  WebRtc_UWord32 PacketCountReceived() const;

  WebRtc_UWord32 ByteCountReceived() const;

  WebRtc_Word32 RegisterRtpHeaderExtension(const RTPExtensionType type,
                                           const WebRtc_UWord8 id);

  WebRtc_Word32 DeregisterRtpHeaderExtension(const RTPExtensionType type);

  void GetHeaderExtensionMapCopy(RtpHeaderExtensionMap* map) const;

  virtual WebRtc_UWord32 PayloadTypeToPayload(
      const WebRtc_UWord8 payload_type,
      ModuleRTPUtility::Payload*& payload) const;

  // RTX.
  void SetRTXStatus(const bool enable, const WebRtc_UWord32 ssrc);

  void RTXStatus(bool* enable, WebRtc_UWord32* ssrc) const;

  RTPReceiverAudio* GetAudioReceiver() const {
    return rtp_receiver_audio_;
  }

  virtual WebRtc_Word32 CallbackOfReceivedPayloadData(
      const WebRtc_UWord8* payload_data,
      const WebRtc_UWord16 payload_size,
      const WebRtcRTPHeader* rtp_header);

  virtual WebRtc_Word8 REDPayloadType() const;

  bool HaveNotReceivedPackets() const;
 protected:

  virtual bool RetransmitOfOldPacket(const WebRtc_UWord16 sequence_number,
                                     const WebRtc_UWord32 rtp_time_stamp) const;

  void UpdateStatistics(const WebRtcRTPHeader* rtp_header,
                        const WebRtc_UWord16 bytes,
                        const bool old_packet);

 private:
  // Returns whether RED is configured with payload_type.
  bool REDPayloadType(const WebRtc_Word8 payload_type) const;

  bool InOrderPacket(const WebRtc_UWord16 sequence_number) const;

  void CheckSSRCChanged(const WebRtcRTPHeader* rtp_header);
  void CheckCSRC(const WebRtcRTPHeader* rtp_header);
  WebRtc_Word32 CheckPayloadChanged(const WebRtcRTPHeader* rtp_header,
                                    const WebRtc_Word8 first_payload_byte,
                                    bool& isRED,
                                    ModuleRTPUtility::PayloadUnion* payload);

  void UpdateNACKBitRate(WebRtc_Word32 bytes, WebRtc_UWord32 now);
  bool ProcessNACKBitRate(WebRtc_UWord32 now);

 private:
  RTPReceiverAudio*       rtp_receiver_audio_;
  RTPReceiverVideo*       rtp_receiver_video_;
  RTPReceiverStrategy*    rtp_media_receiver_;

  WebRtc_Word32           id_;
  ModuleRtpRtcpImpl&      rtp_rtcp_;

  CriticalSectionWrapper* critical_section_cbs_;
  RtpFeedback*            cb_rtp_feedback_;
  RtpData*                cb_rtp_data_;

  CriticalSectionWrapper* critical_section_rtp_receiver_;
  mutable WebRtc_Word64   last_receive_time_;
  WebRtc_UWord16          last_received_payload_length_;
  WebRtc_Word8            last_received_payload_type_;
  WebRtc_Word8            last_received_media_payload_type_;

  WebRtc_UWord32          packet_timeout_ms_;
  WebRtc_Word8            red_payload_type_;

  ModuleRTPUtility::PayloadTypeMap payload_type_map_;
  RtpHeaderExtensionMap            rtp_header_extension_map_;

  // SSRCs.
  WebRtc_UWord32            ssrc_;
  WebRtc_UWord8             num_csrcs_;
  WebRtc_UWord32            current_remote_csrc_[kRtpCsrcSize];
  WebRtc_UWord8             num_energy_;
  WebRtc_UWord8             current_remote_energy_[kRtpCsrcSize];

  bool                      use_ssrc_filter_;
  WebRtc_UWord32            ssrc_filter_;

  // Stats on received RTP packets.
  WebRtc_UWord32            jitter_q4_;
  mutable WebRtc_UWord32    jitter_max_q4_;
  mutable WebRtc_UWord32    cumulative_loss_;
  WebRtc_UWord32            jitter_q4_transmission_time_offset_;

  WebRtc_UWord32            local_time_last_received_timestamp_;
  int64_t                   last_received_frame_time_ms_;
  WebRtc_UWord32            last_received_timestamp_;
  WebRtc_UWord16            last_received_sequence_number_;
  WebRtc_Word32             last_received_transmission_time_offset_;
  WebRtc_UWord16            received_seq_first_;
  WebRtc_UWord16            received_seq_max_;
  WebRtc_UWord16            received_seq_wraps_;

  // Current counter values.
  WebRtc_UWord16            received_packet_oh_;
  WebRtc_UWord32            received_byte_count_;
  WebRtc_UWord32            received_old_packet_count_;
  WebRtc_UWord32            received_inorder_packet_count_;

  // Counter values when we sent the last report.
  mutable WebRtc_UWord32    last_report_inorder_packets_;
  mutable WebRtc_UWord32    last_report_old_packets_;
  mutable WebRtc_UWord16    last_report_seq_max_;
  mutable WebRtc_UWord8     last_report_fraction_lost_;
  mutable WebRtc_UWord32    last_report_cumulative_lost_;  // 24 bits valid.
  mutable WebRtc_UWord32    last_report_extended_high_seq_num_;
  mutable WebRtc_UWord32    last_report_jitter_;
  mutable WebRtc_UWord32    last_report_jitter_transmission_time_offset_;

  NACKMethod nack_method_;

  bool rtx_;
  WebRtc_UWord32 ssrc_rtx_;
};

} // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_H_
