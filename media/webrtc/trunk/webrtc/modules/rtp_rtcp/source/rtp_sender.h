/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_

#include <assert.h>
#include <math.h>

#include <map>

#include "webrtc/common_types.h"
#include "webrtc/modules/pacing/include/paced_sender.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/modules/rtp_rtcp/source/ssrc_database.h"
#include "webrtc/modules/rtp_rtcp/source/video_codec_information.h"

#define MAX_INIT_RTP_SEQ_NUMBER 32767  // 2^15 -1.

namespace webrtc {

class CriticalSectionWrapper;
class RTPPacketHistory;
class RTPSenderAudio;
class RTPSenderVideo;

class RTPSenderInterface {
 public:
  RTPSenderInterface() {}
  virtual ~RTPSenderInterface() {}

  virtual uint32_t SSRC() const = 0;
  virtual uint32_t Timestamp() const = 0;

  virtual int32_t BuildRTPheader(
      uint8_t *data_buffer, const int8_t payload_type,
      const bool marker_bit, const uint32_t capture_time_stamp,
      int64_t capture_time_ms,
      const bool time_stamp_provided = true,
      const bool inc_sequence_number = true) = 0;

  virtual uint16_t RTPHeaderLength() const = 0;
  virtual uint16_t IncrementSequenceNumber() = 0;
  virtual uint16_t SequenceNumber() const = 0;
  virtual uint16_t MaxPayloadLength() const = 0;
  virtual uint16_t MaxDataPayloadLength() const = 0;
  virtual uint16_t PacketOverHead() const = 0;
  virtual uint16_t ActualSendBitrateKbit() const = 0;

  virtual int32_t SendToNetwork(
      uint8_t *data_buffer, int payload_length, int rtp_header_length,
      int64_t capture_time_ms, StorageType storage,
      PacedSender::Priority priority) = 0;
};

class RTPSender : public Bitrate, public RTPSenderInterface {
 public:
  RTPSender(const int32_t id, const bool audio, Clock *clock,
            Transport *transport, RtpAudioFeedback *audio_feedback,
            PacedSender *paced_sender);
  virtual ~RTPSender();

  void ProcessBitrate();

  virtual uint16_t ActualSendBitrateKbit() const OVERRIDE;

  uint32_t VideoBitrateSent() const;
  uint32_t FecOverheadRate() const;
  uint32_t NackOverheadRate() const;

  void SetTargetSendBitrate(const uint32_t bits);

  virtual uint16_t MaxDataPayloadLength() const
      OVERRIDE;  // with RTP and FEC headers.

  int32_t RegisterPayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payload_type, const uint32_t frequency,
      const uint8_t channels, const uint32_t rate);

  int32_t DeRegisterSendPayload(const int8_t payload_type);

  int8_t SendPayloadType() const;

  int SendPayloadFrequency() const;

  void SetSendingStatus(bool enabled);

  void SetSendingMediaStatus(const bool enabled);
  bool SendingMedia() const;

  // Number of sent RTP packets.
  uint32_t Packets() const;

  // Number of sent RTP bytes.
  uint32_t Bytes() const;

  void ResetDataCounters();

  uint32_t StartTimestamp() const;
  void SetStartTimestamp(uint32_t timestamp, bool force);

  uint32_t GenerateNewSSRC();
  void SetSSRC(const uint32_t ssrc);

  virtual uint16_t SequenceNumber() const OVERRIDE;
  void SetSequenceNumber(uint16_t seq);

  int32_t CSRCs(uint32_t arr_of_csrc[kRtpCsrcSize]) const;

  void SetCSRCStatus(const bool include);

  void SetCSRCs(const uint32_t arr_of_csrc[kRtpCsrcSize],
                const uint8_t arr_length);

  int32_t SetMaxPayloadLength(const uint16_t length,
                              const uint16_t packet_over_head);

  int32_t SendOutgoingData(
      const FrameType frame_type, const int8_t payload_type,
      const uint32_t time_stamp, int64_t capture_time_ms,
      const uint8_t *payload_data, const uint32_t payload_size,
      const RTPFragmentationHeader *fragmentation,
      VideoCodecInformation *codec_info = NULL,
      const RTPVideoTypeHeader * rtp_type_hdr = NULL);

  int BuildPaddingPacket(uint8_t* packet, int header_length, int32_t bytes);
  int SendPadData(int payload_type, uint32_t timestamp, int64_t capture_time_ms,
                  int32_t bytes, StorageType store,
                  bool force_full_size_packets, bool only_pad_after_markerbit);
  // RTP header extension
  int32_t SetTransmissionTimeOffset(
      const int32_t transmission_time_offset);
  int32_t SetAbsoluteSendTime(
      const uint32_t absolute_send_time);

  int32_t RegisterRtpHeaderExtension(const RTPExtensionType type,
                                     const uint8_t id);

  int32_t DeregisterRtpHeaderExtension(const RTPExtensionType type);

  uint16_t RtpHeaderExtensionTotalLength() const;

  uint16_t BuildRTPHeaderExtension(uint8_t* data_buffer) const;

  uint8_t BuildTransmissionTimeOffsetExtension(
      uint8_t *data_buffer) const;
  uint8_t BuildAbsoluteSendTimeExtension(
      uint8_t* data_buffer) const;

  bool UpdateTransmissionTimeOffset(uint8_t *rtp_packet,
                                    const uint16_t rtp_packet_length,
                                    const RTPHeader &rtp_header,
                                    const int64_t time_diff_ms) const;
  bool UpdateAbsoluteSendTime(uint8_t *rtp_packet,
                              const uint16_t rtp_packet_length,
                              const RTPHeader &rtp_header,
                              const int64_t now_ms) const;

  bool TimeToSendPacket(uint16_t sequence_number, int64_t capture_time_ms);
  int TimeToSendPadding(int bytes);

  // NACK.
  int SelectiveRetransmissions() const;
  int SetSelectiveRetransmissions(uint8_t settings);
  void OnReceivedNACK(const std::list<uint16_t>& nack_sequence_numbers,
                      const uint16_t avg_rtt);

  void SetStorePacketsStatus(const bool enable,
                             const uint16_t number_to_store);

  bool StorePackets() const;

  int32_t ReSendPacket(uint16_t packet_id, uint32_t min_resend_time = 0);

  bool ProcessNACKBitRate(const uint32_t now);

  // RTX.
  void SetRTXStatus(RtxMode mode, bool set_ssrc, uint32_t ssrc);

  void RTXStatus(RtxMode* mode, uint32_t* ssrc, int* payload_type) const;

  void SetRtxPayloadType(int payloadType);

  // Functions wrapping RTPSenderInterface.
  virtual int32_t BuildRTPheader(
      uint8_t *data_buffer, const int8_t payload_type,
      const bool marker_bit, const uint32_t capture_time_stamp,
      int64_t capture_time_ms,
      const bool time_stamp_provided = true,
      const bool inc_sequence_number = true) OVERRIDE;

  virtual uint16_t RTPHeaderLength() const OVERRIDE;
  virtual uint16_t IncrementSequenceNumber() OVERRIDE;
  virtual uint16_t MaxPayloadLength() const OVERRIDE;
  virtual uint16_t PacketOverHead() const OVERRIDE;

  // Current timestamp.
  virtual uint32_t Timestamp() const OVERRIDE;
  virtual uint32_t SSRC() const OVERRIDE;

  virtual int32_t SendToNetwork(
      uint8_t *data_buffer, int payload_length, int rtp_header_length,
      int64_t capture_time_ms, StorageType storage,
      PacedSender::Priority priority) OVERRIDE;

  // Audio.

  // Send a DTMF tone using RFC 2833 (4733).
  int32_t SendTelephoneEvent(const uint8_t key,
                             const uint16_t time_ms,
                             const uint8_t level);

  bool SendTelephoneEventActive(int8_t *telephone_event) const;

  // Set audio packet size, used to determine when it's time to send a DTMF
  // packet in silence (CNG).
  int32_t SetAudioPacketSize(const uint16_t packet_size_samples);

  // Set status and ID for header-extension-for-audio-level-indication.
  int32_t SetAudioLevelIndicationStatus(const bool enable, const uint8_t ID);

  // Get status and ID for header-extension-for-audio-level-indication.
  int32_t AudioLevelIndicationStatus(bool *enable, uint8_t *id) const;

  // Store the audio level in d_bov for
  // header-extension-for-audio-level-indication.
  int32_t SetAudioLevel(const uint8_t level_d_bov);

  // Set payload type for Redundant Audio Data RFC 2198.
  int32_t SetRED(const int8_t payload_type);

  // Get payload type for Redundant Audio Data RFC 2198.
  int32_t RED(int8_t *payload_type) const;

  // Video.
  VideoCodecInformation *CodecInformationVideo();

  RtpVideoCodecTypes VideoCodecType() const;

  uint32_t MaxConfiguredBitrateVideo() const;

  int32_t SendRTPIntraRequest();

  // FEC.
  int32_t SetGenericFECStatus(const bool enable,
                              const uint8_t payload_type_red,
                              const uint8_t payload_type_fec);

  int32_t GenericFECStatus(bool *enable, uint8_t *payload_type_red,
                           uint8_t *payload_type_fec) const;

  int32_t SetFecParameters(const FecProtectionParams *delta_params,
                           const FecProtectionParams *key_params);

 protected:
  int32_t CheckPayloadType(const int8_t payload_type,
                           RtpVideoCodecTypes *video_type);

 private:
  int CreateRTPHeader(uint8_t* header, int8_t payload_type,
                      uint32_t ssrc, bool marker_bit,
                      uint32_t timestamp, uint16_t sequence_number,
                      const uint32_t* csrcs, uint8_t csrcs_length) const;

  void UpdateNACKBitRate(const uint32_t bytes, const uint32_t now);

  bool SendPaddingAccordingToBitrate(int8_t payload_type,
                                     uint32_t capture_timestamp,
                                     int64_t capture_time_ms);

  void BuildRtxPacket(uint8_t* buffer, uint16_t* length,
                      uint8_t* buffer_rtx);

  bool SendPacketToNetwork(const uint8_t *packet, uint32_t size);

  int32_t id_;
  const bool audio_configured_;
  RTPSenderAudio *audio_;
  RTPSenderVideo *video_;

  PacedSender *paced_sender_;
  CriticalSectionWrapper *send_critsect_;

  Transport *transport_;
  bool sending_media_;

  uint16_t max_payload_length_;
  uint16_t target_send_bitrate_;
  uint16_t packet_over_head_;

  int8_t payload_type_;
  std::map<int8_t, ModuleRTPUtility::Payload *> payload_type_map_;

  RtpHeaderExtensionMap rtp_header_extension_map_;
  int32_t transmission_time_offset_;
  uint32_t absolute_send_time_;

  // NACK
  uint32_t nack_byte_count_times_[NACK_BYTECOUNT_SIZE];
  int32_t nack_byte_count_[NACK_BYTECOUNT_SIZE];
  Bitrate nack_bitrate_;

  RTPPacketHistory *packet_history_;

  // Statistics
  scoped_ptr<CriticalSectionWrapper> statistics_crit_;
  uint32_t packets_sent_;
  uint32_t payload_bytes_sent_;

  // RTP variables
  bool start_time_stamp_forced_;
  uint32_t start_time_stamp_;
  SSRCDatabase &ssrc_db_;
  uint32_t remote_ssrc_;
  bool sequence_number_forced_;
  uint16_t sequence_number_;
  uint16_t sequence_number_rtx_;
  bool ssrc_forced_;
  uint32_t ssrc_;
  uint32_t timestamp_;
  int64_t capture_time_ms_;
  bool last_packet_marker_bit_;
  uint8_t num_csrcs_;
  uint32_t csrcs_[kRtpCsrcSize];
  bool include_csrcs_;
  RtxMode rtx_;
  uint32_t ssrc_rtx_;
  int payload_type_rtx_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
