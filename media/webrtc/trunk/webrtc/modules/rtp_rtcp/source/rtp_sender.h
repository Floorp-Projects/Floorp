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

  enum CVOMode {
    kCVONone,
    kCVOInactive,  // CVO rtp header extension is registered but haven't
                   // received any frame with rotation pending.
    kCVOActivated,  // CVO rtp header extension will be present in the rtp
                    // packets.
  };

  virtual uint32_t SSRC() const = 0;
  virtual uint32_t Timestamp() const = 0;

  virtual int32_t BuildRTPheader(uint8_t* data_buffer,
                                 int8_t payload_type,
                                 bool marker_bit,
                                 uint32_t capture_timestamp,
                                 int64_t capture_time_ms,
                                 bool timestamp_provided = true,
                                 bool inc_sequence_number = true) = 0;

  virtual size_t RTPHeaderLength() const = 0;
  virtual uint16_t IncrementSequenceNumber() = 0;
  virtual uint16_t SequenceNumber() const = 0;
  virtual size_t MaxPayloadLength() const = 0;
  virtual size_t MaxDataPayloadLength() const = 0;
  virtual uint16_t PacketOverHead() const = 0;
  virtual uint16_t ActualSendBitrateKbit() const = 0;

  virtual int32_t SendToNetwork(
      uint8_t *data_buffer, size_t payload_length, size_t rtp_header_length,
      int64_t capture_time_ms, StorageType storage,
      PacedSender::Priority priority) = 0;

  virtual bool UpdateVideoRotation(uint8_t* rtp_packet,
                                   size_t rtp_packet_length,
                                   const RTPHeader& rtp_header,
                                   VideoRotation rotation) const = 0;
  virtual bool IsRtpHeaderExtensionRegistered(RTPExtensionType type) = 0;
  virtual CVOMode ActivateCVORtpHeaderExtension() = 0;
};

class RTPSender : public RTPSenderInterface {
 public:
  RTPSender(int32_t id,
            bool audio,
            Clock* clock,
            Transport* transport,
            RtpAudioFeedback* audio_feedback,
            PacedSender* paced_sender,
            BitrateStatisticsObserver* bitrate_callback,
            FrameCountObserver* frame_count_observer,
            SendSideDelayObserver* send_side_delay_observer);
  virtual ~RTPSender();

  void ProcessBitrate();

  uint16_t ActualSendBitrateKbit() const override;

  uint32_t VideoBitrateSent() const;
  uint32_t FecOverheadRate() const;
  uint32_t NackOverheadRate() const;

  // Returns true if the statistics have been calculated, and false if no frame
  // was sent within the statistics window.
  bool GetSendSideDelay(int* avg_send_delay_ms, int* max_send_delay_ms) const;

  void SetTargetBitrate(uint32_t bitrate);
  uint32_t GetTargetBitrate();

  // Includes size of RTP and FEC headers.
  size_t MaxDataPayloadLength() const override;

  int32_t RegisterPayload(
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int8_t payload_type, const uint32_t frequency,
      const uint8_t channels, const uint32_t rate);

  int32_t DeRegisterSendPayload(const int8_t payload_type);

  void SetSendPayloadType(int8_t payload_type);

  int8_t SendPayloadType() const;

  int SendPayloadFrequency() const;

  void SetSendingStatus(bool enabled);

  void SetSendingMediaStatus(bool enabled);
  bool SendingMedia() const;

  void GetDataCounters(StreamDataCounters* rtp_stats,
                       StreamDataCounters* rtx_stats) const;

  void ResetDataCounters();

  uint32_t StartTimestamp() const;
  void SetStartTimestamp(uint32_t timestamp, bool force);

  uint32_t GenerateNewSSRC();
  void SetSSRC(uint32_t ssrc);

  uint16_t SequenceNumber() const override;
  void SetSequenceNumber(uint16_t seq);

  void SetCsrcs(const std::vector<uint32_t>& csrcs);

  int32_t SetMaxPayloadLength(size_t length, uint16_t packet_over_head);

  int32_t SendOutgoingData(FrameType frame_type,
                           int8_t payload_type,
                           uint32_t timestamp,
                           int64_t capture_time_ms,
                           const uint8_t* payload_data,
                           size_t payload_size,
                           const RTPFragmentationHeader* fragmentation,
                           VideoCodecInformation* codec_info = NULL,
                           const RTPVideoHeader* rtp_hdr = NULL);

  // RTP header extension
  int32_t SetTransmissionTimeOffset(int32_t transmission_time_offset);
  int32_t SetAbsoluteSendTime(uint32_t absolute_send_time);
  void SetVideoRotation(VideoRotation rotation);
  int32_t SetTransportSequenceNumber(uint16_t sequence_number);
  int32_t SetRID(const char* rid);

  int32_t RegisterRtpHeaderExtension(RTPExtensionType type, uint8_t id);
  virtual bool IsRtpHeaderExtensionRegistered(RTPExtensionType type) override;
  int32_t DeregisterRtpHeaderExtension(RTPExtensionType type);

  size_t RtpHeaderExtensionTotalLength() const;

  uint16_t BuildRTPHeaderExtension(uint8_t* data_buffer, bool marker_bit) const;

  uint8_t BuildTransmissionTimeOffsetExtension(uint8_t *data_buffer) const;
  uint8_t BuildAudioLevelExtension(uint8_t* data_buffer) const;
  uint8_t BuildAbsoluteSendTimeExtension(uint8_t* data_buffer) const;
  uint8_t BuildVideoRotationExtension(uint8_t* data_buffer) const;
  uint8_t BuildTransportSequenceNumberExtension(uint8_t* data_buffer) const;
  uint8_t BuildRIDExtension(uint8_t* data_buffer) const;

  bool UpdateAudioLevel(uint8_t* rtp_packet,
                        size_t rtp_packet_length,
                        const RTPHeader& rtp_header,
                        bool is_voiced,
                        uint8_t dBov) const;

  virtual bool UpdateVideoRotation(uint8_t* rtp_packet,
                                   size_t rtp_packet_length,
                                   const RTPHeader& rtp_header,
                                   VideoRotation rotation) const override;

  bool TimeToSendPacket(uint16_t sequence_number, int64_t capture_time_ms,
                        bool retransmission);
  size_t TimeToSendPadding(size_t bytes);

  // NACK.
  int SelectiveRetransmissions() const;
  int SetSelectiveRetransmissions(uint8_t settings);
  void OnReceivedNACK(const std::list<uint16_t>& nack_sequence_numbers,
                      int64_t avg_rtt);

  void SetStorePacketsStatus(bool enable, uint16_t number_to_store);

  bool StorePackets() const;

  int32_t ReSendPacket(uint16_t packet_id, int64_t min_resend_time = 0);

  bool ProcessNACKBitRate(uint32_t now);

  // RTX.
  void SetRtxStatus(int mode);
  int RtxStatus() const;

  uint32_t RtxSsrc() const;
  void SetRtxSsrc(uint32_t ssrc);

  void SetRtxPayloadType(int payloadType);

  // Functions wrapping RTPSenderInterface.
  int32_t BuildRTPheader(uint8_t* data_buffer,
                         int8_t payload_type,
                         bool marker_bit,
                         uint32_t capture_timestamp,
                         int64_t capture_time_ms,
                         const bool timestamp_provided = true,
                         const bool inc_sequence_number = true) override;

  size_t RTPHeaderLength() const override;
  uint16_t IncrementSequenceNumber() override;
  size_t MaxPayloadLength() const override;
  uint16_t PacketOverHead() const override;

  // Current timestamp.
  uint32_t Timestamp() const override;
  uint32_t SSRC() const override;

  int32_t SendToNetwork(uint8_t* data_buffer,
                        size_t payload_length,
                        size_t rtp_header_length,
                        int64_t capture_time_ms,
                        StorageType storage,
                        PacedSender::Priority priority) override;

  // Audio.

  // Send a DTMF tone using RFC 2833 (4733).
  int32_t SendTelephoneEvent(uint8_t key, uint16_t time_ms, uint8_t level);

  // Set audio packet size, used to determine when it's time to send a DTMF
  // packet in silence (CNG).
  int32_t SetAudioPacketSize(uint16_t packet_size_samples);

  // Store the audio level in d_bov for
  // header-extension-for-audio-level-indication.
  int32_t SetAudioLevel(uint8_t level_d_bov);

  // Set payload type for Redundant Audio Data RFC 2198.
  int32_t SetRED(int8_t payload_type);

  // Get payload type for Redundant Audio Data RFC 2198.
  int32_t RED(int8_t *payload_type) const;

  // Video.
  VideoCodecInformation *CodecInformationVideo();

  RtpVideoCodecTypes VideoCodecType() const;

  uint32_t MaxConfiguredBitrateVideo() const;

  int32_t SendRTPIntraRequest();

  // FEC.
  int32_t SetGenericFECStatus(bool enable,
                              uint8_t payload_type_red,
                              uint8_t payload_type_fec);

  int32_t GenericFECStatus(bool *enable, uint8_t *payload_type_red,
                           uint8_t *payload_type_fec) const;

  int32_t SetFecParameters(const FecProtectionParams *delta_params,
                           const FecProtectionParams *key_params);

  size_t SendPadData(uint32_t timestamp,
                     int64_t capture_time_ms,
                     size_t bytes);

  // Called on update of RTP statistics.
  void RegisterRtpStatisticsCallback(StreamDataCountersCallback* callback);
  StreamDataCountersCallback* GetRtpStatisticsCallback() const;

  uint32_t BitrateSent() const;

  void SetRtpState(const RtpState& rtp_state);
  RtpState GetRtpState() const;
  void SetRtxRtpState(const RtpState& rtp_state);
  RtpState GetRtxRtpState() const;
  CVOMode ActivateCVORtpHeaderExtension() override;

 protected:
  int32_t CheckPayloadType(int8_t payload_type, RtpVideoCodecTypes* video_type);

 private:
  // Maps capture time in milliseconds to send-side delay in milliseconds.
  // Send-side delay is the difference between transmission time and capture
  // time.
  typedef std::map<int64_t, int> SendDelayMap;

  size_t CreateRtpHeader(uint8_t* header,
                         int8_t payload_type,
                         uint32_t ssrc,
                         bool marker_bit,
                         uint32_t timestamp,
                         uint16_t sequence_number,
                         const std::vector<uint32_t>& csrcs) const;

  void UpdateNACKBitRate(uint32_t bytes, int64_t now);

  bool PrepareAndSendPacket(uint8_t* buffer,
                            size_t length,
                            int64_t capture_time_ms,
                            bool send_over_rtx,
                            bool is_retransmit);

  // Return the number of bytes sent.  Note that both of these functions may
  // return a larger value that their argument.
  size_t TrySendRedundantPayloads(size_t bytes);
  size_t TrySendPadData(size_t bytes);

  size_t BuildPaddingPacket(uint8_t* packet, size_t header_length);

  void BuildRtxPacket(uint8_t* buffer, size_t* length,
                      uint8_t* buffer_rtx);

  bool SendPacketToNetwork(const uint8_t *packet, size_t size);

  void UpdateDelayStatistics(int64_t capture_time_ms, int64_t now_ms);

  // Find the byte position of the RTP extension as indicated by |type| in
  // |rtp_packet|. Return false if such extension doesn't exist.
  bool FindHeaderExtensionPosition(RTPExtensionType type,
                                   const uint8_t* rtp_packet,
                                   size_t rtp_packet_length,
                                   const RTPHeader& rtp_header,
                                   size_t* position) const;

  void UpdateTransmissionTimeOffset(uint8_t* rtp_packet,
                                    size_t rtp_packet_length,
                                    const RTPHeader& rtp_header,
                                    int64_t time_diff_ms) const;
  void UpdateAbsoluteSendTime(uint8_t* rtp_packet,
                              size_t rtp_packet_length,
                              const RTPHeader& rtp_header,
                              int64_t now_ms) const;

  void UpdateRtpStats(const uint8_t* buffer,
                      size_t packet_length,
                      const RTPHeader& header,
                      bool is_rtx,
                      bool is_retransmit);
  bool IsFecPacket(const uint8_t* buffer, const RTPHeader& header) const;

  Clock* clock_;
  int64_t clock_delta_ms_;

  rtc::scoped_ptr<BitrateAggregator> bitrates_;
  Bitrate total_bitrate_sent_;

  int32_t id_;

  const bool audio_configured_;
  rtc::scoped_ptr<RTPSenderAudio> audio_;
  rtc::scoped_ptr<RTPSenderVideo> video_;

  PacedSender *paced_sender_;
  int64_t last_capture_time_ms_sent_;
  rtc::scoped_ptr<CriticalSectionWrapper> send_critsect_;

  Transport *transport_;
  bool sending_media_ GUARDED_BY(send_critsect_);

  size_t max_payload_length_;
  uint16_t packet_over_head_;

  int8_t payload_type_ GUARDED_BY(send_critsect_);
  std::map<int8_t, RtpUtility::Payload*> payload_type_map_;

  RtpHeaderExtensionMap rtp_header_extension_map_;
  int32_t transmission_time_offset_;
  uint32_t absolute_send_time_;
  VideoRotation rotation_;
  CVOMode cvo_mode_;
  uint16_t transport_sequence_number_;
  char* rid_;

  // NACK
  uint32_t nack_byte_count_times_[NACK_BYTECOUNT_SIZE];
  size_t nack_byte_count_[NACK_BYTECOUNT_SIZE];
  Bitrate nack_bitrate_;

  RTPPacketHistory packet_history_;

  // Statistics
  rtc::scoped_ptr<CriticalSectionWrapper> statistics_crit_;
  SendDelayMap send_delays_ GUARDED_BY(statistics_crit_);
  FrameCounts frame_counts_ GUARDED_BY(statistics_crit_);
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
  std::vector<uint32_t> csrcs_ GUARDED_BY(send_critsect_);
  int rtx_ GUARDED_BY(send_critsect_);
  uint32_t ssrc_rtx_ GUARDED_BY(send_critsect_);
  int payload_type_rtx_ GUARDED_BY(send_critsect_);

  // Note: Don't access this variable directly, always go through
  // SetTargetBitrateKbps or GetTargetBitrateKbps. Also remember
  // that by the time the function returns there is no guarantee
  // that the target bitrate is still valid.
  rtc::scoped_ptr<CriticalSectionWrapper> target_bitrate_critsect_;
  uint32_t target_bitrate_ GUARDED_BY(target_bitrate_critsect_);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_H_
