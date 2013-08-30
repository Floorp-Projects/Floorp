/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_

#include <list>
#include <vector>

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_payload_registry.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

#ifdef MATLAB
class MatlabPlot;
#endif

namespace webrtc {

class ModuleRtpRtcpImpl : public RtpRtcp {
 public:
  explicit ModuleRtpRtcpImpl(const RtpRtcp::Configuration& configuration);

  virtual ~ModuleRtpRtcpImpl();

  // Returns the number of milliseconds until the module want a worker thread to
  // call Process.
  virtual int32_t TimeUntilNextProcess();

  // Process any pending tasks such as timeouts.
  virtual int32_t Process();

  // Receiver part.

  // Configure a timeout value.
  virtual int32_t SetPacketTimeout(const uint32_t rtp_timeout_ms,
                                   const uint32_t rtcp_timeout_ms);

  // Set periodic dead or alive notification.
  virtual int32_t SetPeriodicDeadOrAliveStatus(
      const bool enable,
      const uint8_t sample_time_seconds);

  // Get periodic dead or alive notification status.
  virtual int32_t PeriodicDeadOrAliveStatus(
      bool& enable,
      uint8_t& sample_time_seconds);

  virtual int32_t RegisterReceivePayload(const CodecInst& voice_codec);

  virtual int32_t RegisterReceivePayload(const VideoCodec& video_codec);

  virtual int32_t ReceivePayloadType(const CodecInst& voice_codec,
                                     int8_t* pl_type);

  virtual int32_t ReceivePayloadType(const VideoCodec& video_codec,
                                     int8_t* pl_type);

  virtual int32_t DeRegisterReceivePayload(
      const int8_t payload_type);

  // Get the currently configured SSRC filter.
  virtual int32_t SSRCFilter(uint32_t& allowed_ssrc) const;

  // Set a SSRC to be used as a filter for incoming RTP streams.
  virtual int32_t SetSSRCFilter(const bool enable,
                                const uint32_t allowed_ssrc);

  // Get last received remote timestamp.
  virtual uint32_t RemoteTimestamp() const;

  // Get the local time of the last received remote timestamp.
  virtual int64_t LocalTimeOfRemoteTimeStamp() const;

  // Get the current estimated remote timestamp.
  virtual int32_t EstimatedRemoteTimeStamp(
      uint32_t& timestamp) const;

  virtual uint32_t RemoteSSRC() const;

  virtual int32_t RemoteCSRCs(
      uint32_t arr_of_csrc[kRtpCsrcSize]) const;

  virtual int32_t SetRTXReceiveStatus(const bool enable,
                                      const uint32_t ssrc);

  virtual int32_t RTXReceiveStatus(bool* enable, uint32_t* ssrc,
                                   int* payloadType) const;

  virtual void SetRtxReceivePayloadType(int payload_type);

  // Called when we receive an RTP packet.
  virtual int32_t IncomingRtpPacket(const uint8_t* incoming_packet,
                                    const uint16_t packet_length,
                                    const RTPHeader& parsed_rtp_header);

  // Called when we receive an RTCP packet.
  virtual int32_t IncomingRtcpPacket(const uint8_t* incoming_packet,
                                     uint16_t incoming_packet_length);

  // Sender part.

  virtual int32_t RegisterSendPayload(const CodecInst& voice_codec);

  virtual int32_t RegisterSendPayload(const VideoCodec& video_codec);

  virtual int32_t DeRegisterSendPayload(const int8_t payload_type);

  virtual int8_t SendPayloadType() const;

  // Register RTP header extension.
  virtual int32_t RegisterSendRtpHeaderExtension(
      const RTPExtensionType type,
      const uint8_t id);

  virtual int32_t DeregisterSendRtpHeaderExtension(
      const RTPExtensionType type);

  // Get start timestamp.
  virtual uint32_t StartTimestamp() const;

  // Configure start timestamp, default is a random number.
  virtual int32_t SetStartTimestamp(const uint32_t timestamp);

  virtual uint16_t SequenceNumber() const;

  // Set SequenceNumber, default is a random number.
  virtual int32_t SetSequenceNumber(const uint16_t seq);

  virtual uint32_t SSRC() const;

  // Configure SSRC, default is a random number.
  virtual int32_t SetSSRC(const uint32_t ssrc);

  virtual int32_t CSRCs(uint32_t arr_of_csrc[kRtpCsrcSize]) const;

  virtual int32_t SetCSRCs(const uint32_t arr_of_csrc[kRtpCsrcSize],
                           const uint8_t arr_length);

  virtual int32_t SetCSRCStatus(const bool include);

  virtual uint32_t PacketCountSent() const;

  virtual int CurrentSendFrequencyHz() const;

  virtual uint32_t ByteCountSent() const;

  virtual int32_t SetRTXSendStatus(const RtxMode mode,
                                   const bool set_ssrc,
                                   const uint32_t ssrc);

  virtual int32_t RTXSendStatus(RtxMode* mode, uint32_t* ssrc,
                                int* payloadType) const;


  virtual void SetRtxSendPayloadType(int payload_type);

  // Sends kRtcpByeCode when going from true to false.
  virtual int32_t SetSendingStatus(const bool sending);

  virtual bool Sending() const;

  // Drops or relays media packets.
  virtual int32_t SetSendingMediaStatus(const bool sending);

  virtual bool SendingMedia() const;

  // Used by the codec module to deliver a video or audio frame for
  // packetization.
  virtual int32_t SendOutgoingData(
      const FrameType frame_type,
      const int8_t payload_type,
      const uint32_t time_stamp,
      int64_t capture_time_ms,
      const uint8_t* payload_data,
      const uint32_t payload_size,
      const RTPFragmentationHeader* fragmentation = NULL,
      const RTPVideoHeader* rtp_video_hdr = NULL);

  virtual bool TimeToSendPacket(uint32_t ssrc, uint16_t sequence_number,
                                int64_t capture_time_ms);
  // Returns the number of padding bytes actually sent, which can be more or
  // less than |bytes|.
  virtual int TimeToSendPadding(int bytes);
  // RTCP part.

  // Get RTCP status.
  virtual RTCPMethod RTCP() const;

  // Configure RTCP status i.e on/off.
  virtual int32_t SetRTCPStatus(const RTCPMethod method);

  // Set RTCP CName.
  virtual int32_t SetCNAME(const char c_name[RTCP_CNAME_SIZE]);

  // Get RTCP CName.
  virtual int32_t CNAME(char c_name[RTCP_CNAME_SIZE]);

  // Get remote CName.
  virtual int32_t RemoteCNAME(const uint32_t remote_ssrc,
                              char c_name[RTCP_CNAME_SIZE]) const;

  // Get remote NTP.
  virtual int32_t RemoteNTP(uint32_t* received_ntp_secs,
                            uint32_t* received_ntp_frac,
                            uint32_t* rtcp_arrival_time_secs,
                            uint32_t* rtcp_arrival_time_frac,
                            uint32_t* rtcp_timestamp) const;

  virtual int32_t AddMixedCNAME(const uint32_t ssrc,
                                const char c_name[RTCP_CNAME_SIZE]);

  virtual int32_t RemoveMixedCNAME(const uint32_t ssrc);

  // Get RoundTripTime.
  virtual int32_t RTT(const uint32_t remote_ssrc,
                      uint16_t* rtt,
                      uint16_t* avg_rtt,
                      uint16_t* min_rtt,
                      uint16_t* max_rtt) const;

  // Reset RoundTripTime statistics.
  virtual int32_t ResetRTT(const uint32_t remote_ssrc);

  virtual void SetRtt(uint32_t rtt);

  // Force a send of an RTCP packet.
  // Normal SR and RR are triggered via the process function.
  virtual int32_t SendRTCP(uint32_t rtcp_packet_type = kRtcpReport);

  // Statistics of our locally created statistics of the received RTP stream.
  virtual int32_t StatisticsRTP(uint8_t*  fraction_lost,
                                uint32_t* cum_lost,
                                uint32_t* ext_max,
                                uint32_t* jitter,
                                uint32_t* max_jitter = NULL) const;

  // Reset RTP statistics.
  virtual int32_t ResetStatisticsRTP();

  virtual int32_t ResetReceiveDataCountersRTP();

  virtual int32_t ResetSendDataCountersRTP();

  // Statistics of the amount of data sent and received.
  virtual int32_t DataCountersRTP(uint32_t* bytes_sent,
                                  uint32_t* packets_sent,
                                  uint32_t* bytes_received,
                                  uint32_t* packets_received) const;

  virtual int32_t ReportBlockStatistics(
      uint8_t* fraction_lost,
      uint32_t* cum_lost,
      uint32_t* ext_max,
      uint32_t* jitter,
      uint32_t* jitter_transmission_time_offset);

  // Get received RTCP report, sender info.
  virtual int32_t RemoteRTCPStat(RTCPSenderInfo* sender_info);

  // Get received RTCP report, report block.
  virtual int32_t RemoteRTCPStat(
      std::vector<RTCPReportBlock>* receive_blocks) const;

  // Set received RTCP report block.
  virtual int32_t AddRTCPReportBlock(
    const uint32_t ssrc, const RTCPReportBlock* receive_block);

  virtual int32_t RemoveRTCPReportBlock(const uint32_t ssrc);

  // (REMB) Receiver Estimated Max Bitrate.
  virtual bool REMB() const;

  virtual int32_t SetREMBStatus(const bool enable);

  virtual int32_t SetREMBData(const uint32_t bitrate,
                              const uint8_t number_of_ssrc,
                              const uint32_t* ssrc);

  // (IJ) Extended jitter report.
  virtual bool IJ() const;

  virtual int32_t SetIJStatus(const bool enable);

  // (TMMBR) Temporary Max Media Bit Rate.
  virtual bool TMMBR() const;

  virtual int32_t SetTMMBRStatus(const bool enable);

  int32_t SetTMMBN(const TMMBRSet* bounding_set);

  virtual uint16_t MaxPayloadLength() const;

  virtual uint16_t MaxDataPayloadLength() const;

  virtual int32_t SetMaxTransferUnit(const uint16_t size);

  virtual int32_t SetTransportOverhead(
      const bool tcp,
      const bool ipv6,
      const uint8_t authentication_overhead = 0);

  // (NACK) Negative acknowledgment part.

  // Is Negative acknowledgment requests on/off?
  virtual NACKMethod NACK() const;

  // Turn negative acknowledgment requests on/off.
  virtual int32_t SetNACKStatus(const NACKMethod method,
                                int max_reordering_threshold);

  virtual int SelectiveRetransmissions() const;

  virtual int SetSelectiveRetransmissions(uint8_t settings);

  // Send a Negative acknowledgment packet.
  virtual int32_t SendNACK(const uint16_t* nack_list, const uint16_t size);

  // Store the sent packets, needed to answer to a negative acknowledgment
  // requests.
  virtual int32_t SetStorePacketsStatus(
      const bool enable, const uint16_t number_to_store);

  // (APP) Application specific data.
  virtual int32_t SetRTCPApplicationSpecificData(
      const uint8_t sub_type,
      const uint32_t name,
      const uint8_t* data,
      const uint16_t length);

  // (XR) VOIP metric.
  virtual int32_t SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric);

  // Audio part.

  // Set audio packet size, used to determine when it's time to send a DTMF
  // packet in silence (CNG).
  virtual int32_t SetAudioPacketSize(
      const uint16_t packet_size_samples);

  // Forward DTMFs to decoder for playout.
  virtual int SetTelephoneEventForwardToDecoder(bool forward_to_decoder);

  // Is forwarding of outband telephone events turned on/off?
  virtual bool TelephoneEventForwardToDecoder() const;

  virtual bool SendTelephoneEventActive(int8_t& telephone_event) const;

  // Send a TelephoneEvent tone using RFC 2833 (4733).
  virtual int32_t SendTelephoneEventOutband(const uint8_t key,
                                            const uint16_t time_ms,
                                            const uint8_t level);

  // Set payload type for Redundant Audio Data RFC 2198.
  virtual int32_t SetSendREDPayloadType(const int8_t payload_type);

  // Get payload type for Redundant Audio Data RFC 2198.
  virtual int32_t SendREDPayloadType(int8_t& payload_type) const;

  // Set status and id for header-extension-for-audio-level-indication.
  virtual int32_t SetRTPAudioLevelIndicationStatus(
      const bool enable, const uint8_t id);

  // Get status and id for header-extension-for-audio-level-indication.
  virtual int32_t GetRTPAudioLevelIndicationStatus(
      bool& enable, uint8_t& id) const;

  // Store the audio level in d_bov for header-extension-for-audio-level-
  // indication.
  virtual int32_t SetAudioLevel(const uint8_t level_d_bov);

  // Video part.

  virtual RtpVideoCodecTypes ReceivedVideoCodec() const;

  virtual RtpVideoCodecTypes SendVideoCodec() const;

  virtual int32_t SendRTCPSliceLossIndication(
      const uint8_t picture_id);

  // Set method for requestion a new key frame.
  virtual int32_t SetKeyFrameRequestMethod(
      const KeyFrameRequestMethod method);

  // Send a request for a keyframe.
  virtual int32_t RequestKeyFrame();

  virtual int32_t SetCameraDelay(const int32_t delay_ms);

  virtual void SetTargetSendBitrate(const uint32_t bitrate);

  virtual int32_t SetGenericFECStatus(
      const bool enable,
      const uint8_t payload_type_red,
      const uint8_t payload_type_fec);

  virtual int32_t GenericFECStatus(
      bool& enable,
      uint8_t& payload_type_red,
      uint8_t& payload_type_fec);

  virtual int32_t SetFecParameters(
      const FecProtectionParams* delta_params,
      const FecProtectionParams* key_params);

  virtual int32_t LastReceivedNTP(uint32_t& NTPsecs,
                                  uint32_t& NTPfrac,
                                  uint32_t& remote_sr);

  virtual int32_t BoundingSet(bool& tmmbr_owner, TMMBRSet*& bounding_set_rec);

  virtual void BitrateSent(uint32_t* total_rate,
                           uint32_t* video_rate,
                           uint32_t* fec_rate,
                           uint32_t* nackRate) const;

  virtual void SetRemoteSSRC(const uint32_t ssrc);

  virtual uint32_t SendTimeOfSendReport(const uint32_t send_report);

  // Good state of RTP receiver inform sender.
  virtual int32_t SendRTCPReferencePictureSelection(
      const uint64_t picture_id);

  void OnReceivedTMMBR();

  // Bad state of RTP receiver request a keyframe.
  void OnRequestIntraFrame();

  // Received a request for a new SLI.
  void OnReceivedSliceLossIndication(const uint8_t picture_id);

  // Received a new reference frame.
  void OnReceivedReferencePictureSelectionIndication(
      const uint64_t picture_id);

  void OnReceivedNACK(const std::list<uint16_t>& nack_sequence_numbers);

  void OnRequestSendReport();

 protected:
  void RegisterChildModule(RtpRtcp* module);

  void DeRegisterChildModule(RtpRtcp* module);

  bool UpdateRTCPReceiveInformationTimers();

  void ProcessDeadOrAliveTimer();

  uint32_t BitrateReceivedNow() const;

  // Get remote SequenceNumber.
  uint16_t RemoteSequenceNumber() const;

  // Only for internal testing.
  uint32_t LastSendReport(uint32_t& last_rtcptime);

  RTPPayloadRegistry        rtp_payload_registry_;

  RTPSender                 rtp_sender_;
  scoped_ptr<RTPReceiver>   rtp_receiver_;

  RTCPSender                rtcp_sender_;
  RTCPReceiver              rtcp_receiver_;

  Clock*                    clock_;

 private:
  int64_t RtcpReportInterval();

  RTPReceiverAudio*         rtp_telephone_event_handler_;

  int32_t             id_;
  const bool                audio_;
  bool                      collision_detected_;
  int64_t             last_process_time_;
  int64_t             last_bitrate_process_time_;
  int64_t             last_packet_timeout_process_time_;
  int64_t             last_rtt_process_time_;
  uint16_t            packet_overhead_;

  scoped_ptr<CriticalSectionWrapper> critical_section_module_ptrs_;
  scoped_ptr<CriticalSectionWrapper> critical_section_module_ptrs_feedback_;
  ModuleRtpRtcpImpl*            default_module_;
  std::list<ModuleRtpRtcpImpl*> child_modules_;

  // Dead or alive.
  bool                  dead_or_alive_active_;
  uint32_t        dead_or_alive_timeout_ms_;
  int64_t         dead_or_alive_last_timer_;
  // Send side
  NACKMethod            nack_method_;
  uint32_t        nack_last_time_sent_full_;
  uint16_t        nack_last_seq_number_sent_;

  bool                  simulcast_;
  VideoCodec            send_video_codec_;
  KeyFrameRequestMethod key_frame_req_method_;

  RemoteBitrateEstimator* remote_bitrate_;

#ifdef MATLAB
  MatlabPlot*           plot1_;
#endif

  RtcpRttObserver* rtt_observer_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_
