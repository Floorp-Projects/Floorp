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

#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/pacing/include/paced_sender.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/bitrate.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_header_extension.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_packet_history.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/modules/rtp_rtcp/source/ssrc_database.h"
#include "webrtc/modules/rtp_rtcp/source/video_codec_information.h"

#define MAX_INIT_RTP_SEQ_NUMBER 32767  // 2^15 -1.

namespace webrtc {

class BitrateAggregator;
class CriticalSectionWrapper;
class RTPSenderAudio;
class RTPSenderVideo;

class RTPSenderInterface {
 public:
  RTPSenderInterface() {}
  virtual ~RTPSenderInterface() {}

  virtual uint32_t SSRC() const = 0;
  virtual uint32_t Timestamp() const = 0;

  virtual int32_t BuildRTPheader(uint8_t* data_buffer,
                                 const int8_t payload_type,
                                 const bool marker_bit,
                                 const uint32_t capture_timestamp,
                                 int64_t capture_time_ms,
                                 const bool timestamp_provided = true,
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

class RTPSender : public RTPSenderInterface {
 public:
  RTPSender(const int32_t id, const bool audio, Clock *clock,
            Transport *transport, RtpAudioFeedback *audio_feedback,
            PacedSender *paced_sender,
            BitrateStatisticsObserver* bitrate_callback,
            FrameCountObserver* frame_count_observer,
            SendSideDelayObserver* send_side_delay_observer);
  virtual ~RTPSender();

  void ProcessBitrate();

  virtual uint16_t ActualSendBitrateKbit() const OVERRIDE;

  uint32_t VideoBitrateSent() const;
  uint32_t FecOverheadRate() const;
  uint32_t NackOverheadRate() const;

  // Returns true if the statistics have been calculated, and false if no frame
  // was sent within the statistics window.
  bool GetSendSideDelay(int* avg_send_delay_ms, int* max_send_delay_ms) const;

  void SetTargetBitrate(uint32_t bitrate);
  uint32_t GetTargetBitrate();

  virtual uint16_t MaxDataPayloadLength() const
      OVERRIDE;  // with RTP and FEC headers.

  int32_t RegisterPayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payload_type, const uint32_t frequency,
      const uint8_t channels, const uint32_t rate);

  int32_t DeRegisterSendPayload(const int8_t payload_type);

  void SetSendPayloadType(int8_t payload_type);

  int8_t SendPayloadType() const;

  int SendPayloadFrequency() const;

  void SetSendingStatus(bool enabled);

  void SetSendingMediaStatus(const bool enabled);
  bool SendingMedia() const;

  void GetDataCounters(StreamDataCounters* rtp_stats,
                       StreamDataCounters* rtx_stats) const;

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

  int32_t SendOutgoingData(const FrameType frame_type,
                           const int8_t payload_type,
                           const uint32_t timestamp,
                           int64_t capture_time_ms,
                           const uint8_t* payload_data,
                           const uint32_t payload_size,
                           const RTPFragmentationHeader* fragmentation,
                           VideoCodecInformation* codec_info = NULL,
                           const RTPVideoTypeHeader* rtp_type_hdr = NULL);

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

  uint8_t BuildTransmissionTimeOffsetExtension(uint8_t *data_buffer) const;
  uint8_t BuildAudioLevelExtension(uint8_t* data_buffer) const;
  uint8_t BuildAbsoluteSendTimeExtension(uint8_t* data_buffer) const;

  bool UpdateAudioLevel(uint8_t *rtp_packet,
                        const uint16_t rtp_packet_length,
                        const RTPHeader &rtp_header,
                        const bool is_voiced,
                        const uint8_t dBov) const;

  bool TimeToSendPacket(uint16_t sequence_number, int64_t capture_time_ms,
                        bool retransmission);
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
  void SetRTXStatus(int mode);

  void RTXStatus(int* mode, uint32_t* ssrc, int* payload_type) const;

  uint32_t RtxSsrc() const;
  void SetRtxSsrc(uint32_t ssrc);

  void SetRtxPayloadType(int payloadType);

  // Functions wrapping RTPSenderInterface.
  virtual int32_t BuildRTPheader(
      uint8_t* data_buffer,
      const int8_t payload_type,
      const bool marker_bit,
      const uint32_t capture_timestamp,
      int64_t capture_time_ms,
      const bool timestamp_provided = true,
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

  int SendPadData(uint32_t timestamp,
                  int64_t capture_time_ms,
                  int32_t bytes);

  // Called on update of RTP statistics.
  void RegisterRtpStatisticsCallback(StreamDataCountersCallback* callback);
  StreamDataCountersCallback* GetRtpStatisticsCallback() const;

  uint32_t BitrateSent() const;

  void SetRtpState(const RtpState& rtp_state);
  RtpState GetRtpState() const;
  void SetRtxRtpState(const RtpState& rtp_state);
  RtpState GetRtxRtpState() const;

 protected:
  int32_t CheckPayloadType(const int8_t payload_type,
                           RtpVideoCodecTypes *video_type);

 private:
  // Maps capture time in milliseconds to send-side delay in milliseconds.
  // Send-side delay is the difference between transmission time and capture
  // time.
  typedef std::map<int64_t, int> SendDelayMap;

  int CreateRTPHeader(uint8_t* header, int8_t payload_type,
                      uint32_t ssrc, bool marker_bit,
                      uint32_t timestamp, uint16_t sequence_number,
                      const uint32_t* csrcs, uint8_t csrcs_length) const;

  void UpdateNACKBitRate(const uint32_t bytes, const uint32_t now);

  bool PrepareAndSendPacket(uint8_t* buffer,
                            uint16_t length,
                            int64_t capture_time_ms,
                            bool send_over_rtx,
                            bool is_retransmit);

  // Return the number of bytes sent.
  int TrySendRedundantPayloads(int bytes);
  int TrySendPadData(int bytes);

  int BuildPaddingPacket(uint8_t* packet, int header_length, int32_t bytes);

  void BuildRtxPacket(uint8_t* buffer, uint16_t* length,
                      uint8_t* buffer_rtx);

  bool SendPacketToNetwork(const uint8_t *packet, uint32_t size);

  void UpdateDelayStatistics(int64_t capture_time_ms, int64_t now_ms);

  void UpdateTransmissionTimeOffset(uint8_t *rtp_packet,
                                    const uint16_t rtp_packet_length,
                                    const RTPHeader &rtp_header,
                                    const int64_t time_diff_ms) const;
  void UpdateAbsoluteSendTime(uint8_t *rtp_packet,
                              const uint16_t rtp_packet_length,
                              const RTPHeader &rtp_header,
                              const int64_t now_ms) const;

  void UpdateRtpStats(const uint8_t* buffer,
                      uint32_t size,
                      const RTPHeader& header,
                      bool is_rtx,
                      bool is_retransmit);
  bool IsFecPacket(const uint8_t* buffer, const RTPHeader& header) const;

  Clock* clock_;
  int64_t clock_delta_ms_;

  scoped_ptr<BitrateAggregator> bitrates_;
  Bitrate total_bitrate_sent_;

  int32_t id_;
  const bool audio_configured_;
  RTPSenderAudio *audio_;
  RTPSenderVideo *video_;

  PacedSender *paced_sender_;
  int64_t last_capture_time_ms_sent_;
  CriticalSectionWrapper *send_critsect_;

  Transport *transport_;
  bool sending_media_ GUARDED_BY(send_critsect_);

  uint16_t max_payload_length_;
  uint16_t packet_over_head_;

  int8_t payload_type_ GUARDED_BY(send_critsect_);
  std::map<int8_t, RtpUtility::Payload*> payload_type_map_;

  RtpHeaderExtensionMap rtp_header_extension_map_;
  int32_t transmission_time_offset_;
  uint32_t absolute_send_time_;

  // NACK
  uint32_t nack_byte_count_times_[NACK_BYTECOUNT_SIZE];
  int32_t nack_byte_count_[NACK_BYTECOUNT_SIZE];
  Bitrate nack_bitrate_;

  RTPPacketHistory packet_history_;

  // Statistics
  scoped_ptr<CriticalSectionWrapper> statistics_crit_;
  SendDelayMap send_delays_ GUARDED_BY(statistics_crit_);
  std::map<FrameType, uint32_t> frame_counts_ GUARDED_BY(statistics_crit_);
  StreamDataCounters rtp_stats_ GUARDED_BY(statistics_crit_);
  StreamDataCounters rtx_rtp_stats_ GUARDED_BY(statistics_crit_);
  StreamDataCountersCallback* rtp_stats_callback_ GUARDED_BY(statistics_crit_);
  FrameCountObserver* const frame_count_observer_;
  SendSideDelayObserver* const send_side_delay_observer_;

  // RTP variables
  bool start_timestamp_forced_ GUARDED_BY(send_critsect_);
  uint32_t start_timestamp_ GUARDED_BY(send_critsect_);
  SSRCDatabase& ssrc_db_ GUARDED_BY(send_critsect_);
  uint32_t remote_ssrc_ GUARDED_BY(send_critsect_);
  bool sequence_number_forced_ GUARDED_BY(send_critsect_);
  uint16_t sequence_number_ GUARDED_BY(send_critsect_);
  uint16_t sequence_number_rtx_ GUARDED_BY(send_critsect_);
  bool ssrc_forced_ GUARDED_BY(send_critsect_);
  uint32_t ssrc_ GUARDED_BY(send_critsect_);
  uint32_t timestamp_ GUARDED_BY(send_critsect_);
  int64_t capture_time_ms_ GUARDED_BY(send_critsect_);
  int64_t last_timestamp_time_ms_ GUARDED_BY(send_critsect_);
  bool media_has_been_sent_ GUARDED_BY(send_critsect_);
  bool last_packet_marker_bit_ GUARDED_BY(send_critsect_);
  uint8_t num_csrcs_ GUARDED_BY(send_critsect_);
  uint32_t csrcs_[kRtpCsrcSize] GUARDED_BY(send_critsect_);
  bool include_csrcs_ GUARDED_BY(send_critsect_);
  int rtx_ GUARDED_BY(send_critsect_);
  uint32_t ssrc_rtx_ GUARDED_BY(send_critsect_);
  int payload_type_rtx_ GUARDED_BY(send_critsect_);

  // Note: Don't access this variable directly, always go through
  // SetTargetBitrateKbps or GetTargetBitrateKbps. Also remember
  // that by the time the function returns there is no guarantee
  // that the target bitrate is still valid.
  scoped_ptr<CriticalSectionWrapper> target_bitrate_critsect_;
  uint32_t target_bitrate_ GUARDED_BY(target_bitrate_critsect_);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
