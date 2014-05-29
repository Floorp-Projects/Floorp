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
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/gtest_prod_util.h"

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
  virtual int32_t TimeUntilNextProcess() OVERRIDE;

  // Process any pending tasks such as timeouts.
  virtual int32_t Process() OVERRIDE;

  // Receiver part.

  // Called when we receive an RTCP packet.
  virtual int32_t IncomingRtcpPacket(const uint8_t* incoming_packet,
                                     uint16_t incoming_packet_length) OVERRIDE;

  virtual void SetRemoteSSRC(const uint32_t ssrc);

  // Sender part.

  virtual int32_t RegisterSendPayload(const CodecInst& voice_codec) OVERRIDE;

  virtual int32_t RegisterSendPayload(const VideoCodec& video_codec) OVERRIDE;

  virtual int32_t DeRegisterSendPayload(const int8_t payload_type) OVERRIDE;

  virtual int8_t SendPayloadType() const;

  // Register RTP header extension.
  virtual int32_t RegisterSendRtpHeaderExtension(
      const RTPExtensionType type,
      const uint8_t id) OVERRIDE;

  virtual int32_t DeregisterSendRtpHeaderExtension(
      const RTPExtensionType type) OVERRIDE;

  // Get start timestamp.
  virtual uint32_t StartTimestamp() const OVERRIDE;

  // Configure start timestamp, default is a random number.
  virtual int32_t SetStartTimestamp(const uint32_t timestamp) OVERRIDE;

  virtual uint16_t SequenceNumber() const OVERRIDE;

  // Set SequenceNumber, default is a random number.
  virtual int32_t SetSequenceNumber(const uint16_t seq) OVERRIDE;

  virtual uint32_t SSRC() const OVERRIDE;

  // Configure SSRC, default is a random number.
  virtual int32_t SetSSRC(const uint32_t ssrc) OVERRIDE;

  virtual int32_t CSRCs(uint32_t arr_of_csrc[kRtpCsrcSize]) const OVERRIDE;

  virtual int32_t SetCSRCs(const uint32_t arr_of_csrc[kRtpCsrcSize],
                           const uint8_t arr_length) OVERRIDE;

  virtual int32_t SetCSRCStatus(const bool include) OVERRIDE;

  virtual uint32_t PacketCountSent() const;

  virtual int CurrentSendFrequencyHz() const;

  virtual uint32_t ByteCountSent() const;

  virtual int32_t SetRTXSendStatus(const int mode,
                                   const bool set_ssrc,
                                   const uint32_t ssrc) OVERRIDE;

  virtual int32_t RTXSendStatus(int* mode, uint32_t* ssrc,
                                int* payloadType) const OVERRIDE;


  virtual void SetRtxSendPayloadType(int payload_type) OVERRIDE;

  // Sends kRtcpByeCode when going from true to false.
  virtual int32_t SetSendingStatus(const bool sending) OVERRIDE;

  virtual bool Sending() const OVERRIDE;

  // Drops or relays media packets.
  virtual int32_t SetSendingMediaStatus(const bool sending) OVERRIDE;

  virtual bool SendingMedia() const OVERRIDE;

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
      const RTPVideoHeader* rtp_video_hdr = NULL) OVERRIDE;

  virtual bool TimeToSendPacket(uint32_t ssrc,
                                uint16_t sequence_number,
                                int64_t capture_time_ms,
                                bool retransmission) OVERRIDE;
  // Returns the number of padding bytes actually sent, which can be more or
  // less than |bytes|.
  virtual int TimeToSendPadding(int bytes) OVERRIDE;

  virtual bool GetSendSideDelay(int* avg_send_delay_ms,
                                int* max_send_delay_ms) const OVERRIDE;

  // RTCP part.

  // Get RTCP status.
  virtual RTCPMethod RTCP() const OVERRIDE;

  // Configure RTCP status i.e on/off.
  virtual int32_t SetRTCPStatus(const RTCPMethod method) OVERRIDE;

  // Set RTCP CName.
  virtual int32_t SetCNAME(const char c_name[RTCP_CNAME_SIZE]) OVERRIDE;

  // Get RTCP CName.
  virtual int32_t CNAME(char c_name[RTCP_CNAME_SIZE]) OVERRIDE;

  // Get remote CName.
  virtual int32_t RemoteCNAME(const uint32_t remote_ssrc,
                              char c_name[RTCP_CNAME_SIZE]) const OVERRIDE;

  // Get remote NTP.
  virtual int32_t RemoteNTP(uint32_t* received_ntp_secs,
                            uint32_t* received_ntp_frac,
                            uint32_t* rtcp_arrival_time_secs,
                            uint32_t* rtcp_arrival_time_frac,
                            uint32_t* rtcp_timestamp) const OVERRIDE;

  virtual int32_t AddMixedCNAME(const uint32_t ssrc,
                                const char c_name[RTCP_CNAME_SIZE]) OVERRIDE;

  virtual int32_t RemoveMixedCNAME(const uint32_t ssrc) OVERRIDE;

  // Get RoundTripTime.
  virtual int32_t RTT(const uint32_t remote_ssrc,
                      uint16_t* rtt,
                      uint16_t* avg_rtt,
                      uint16_t* min_rtt,
                      uint16_t* max_rtt) const OVERRIDE;

  // Reset RoundTripTime statistics.
  virtual int32_t ResetRTT(const uint32_t remote_ssrc) OVERRIDE;

  // Force a send of an RTCP packet.
  // Normal SR and RR are triggered via the process function.
  virtual int32_t SendRTCP(uint32_t rtcp_packet_type = kRtcpReport) OVERRIDE;

  virtual int32_t ResetSendDataCountersRTP() OVERRIDE;

  // Statistics of the amount of data sent and received.
  virtual int32_t DataCountersRTP(uint32_t* bytes_sent,
                                  uint32_t* packets_sent) const OVERRIDE;

  // Get received RTCP report, sender info.
  virtual int32_t RemoteRTCPStat(RTCPSenderInfo* sender_info) OVERRIDE;

  // Get received RTCP report, report block.
  virtual int32_t RemoteRTCPStat(
      std::vector<RTCPReportBlock>* receive_blocks) const OVERRIDE;

  // Set received RTCP report block.
  virtual int32_t AddRTCPReportBlock(
    const uint32_t ssrc, const RTCPReportBlock* receive_block) OVERRIDE;

  virtual int32_t RemoveRTCPReportBlock(const uint32_t ssrc) OVERRIDE;

  // (REMB) Receiver Estimated Max Bitrate.
  virtual bool REMB() const OVERRIDE;

  virtual int32_t SetREMBStatus(const bool enable) OVERRIDE;

  virtual int32_t SetREMBData(const uint32_t bitrate,
                              const uint8_t number_of_ssrc,
                              const uint32_t* ssrc) OVERRIDE;

  // (IJ) Extended jitter report.
  virtual bool IJ() const OVERRIDE;

  virtual int32_t SetIJStatus(const bool enable) OVERRIDE;

  // (TMMBR) Temporary Max Media Bit Rate.
  virtual bool TMMBR() const OVERRIDE;

  virtual int32_t SetTMMBRStatus(const bool enable) OVERRIDE;

  int32_t SetTMMBN(const TMMBRSet* bounding_set);

  virtual uint16_t MaxPayloadLength() const OVERRIDE;

  virtual uint16_t MaxDataPayloadLength() const OVERRIDE;

  virtual int32_t SetMaxTransferUnit(const uint16_t size) OVERRIDE;

  virtual int32_t SetTransportOverhead(
      const bool tcp,
      const bool ipv6,
      const uint8_t authentication_overhead = 0) OVERRIDE;

  // (NACK) Negative acknowledgment part.

  virtual int SelectiveRetransmissions() const OVERRIDE;

  virtual int SetSelectiveRetransmissions(uint8_t settings) OVERRIDE;

  // Send a Negative acknowledgment packet.
  virtual int32_t SendNACK(const uint16_t* nack_list,
                           const uint16_t size) OVERRIDE;

  // Store the sent packets, needed to answer to a negative acknowledgment
  // requests.
  virtual int32_t SetStorePacketsStatus(
      const bool enable, const uint16_t number_to_store) OVERRIDE;

  virtual bool StorePackets() const OVERRIDE;

  // Called on receipt of RTCP report block from remote side.
  virtual void RegisterSendChannelRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback) OVERRIDE;
  virtual RtcpStatisticsCallback*
      GetSendChannelRtcpStatisticsCallback() OVERRIDE;

  // (APP) Application specific data.
  virtual int32_t SetRTCPApplicationSpecificData(
      const uint8_t sub_type,
      const uint32_t name,
      const uint8_t* data,
      const uint16_t length) OVERRIDE;

  // (XR) VOIP metric.
  virtual int32_t SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric) OVERRIDE;

  // (XR) Receiver reference time report.
  virtual void SetRtcpXrRrtrStatus(bool enable) OVERRIDE;

  virtual bool RtcpXrRrtrStatus() const OVERRIDE;

  // Audio part.

  // Set audio packet size, used to determine when it's time to send a DTMF
  // packet in silence (CNG).
  virtual int32_t SetAudioPacketSize(
      const uint16_t packet_size_samples) OVERRIDE;

  virtual bool SendTelephoneEventActive(int8_t& telephone_event) const OVERRIDE;

  // Send a TelephoneEvent tone using RFC 2833 (4733).
  virtual int32_t SendTelephoneEventOutband(const uint8_t key,
                                            const uint16_t time_ms,
                                            const uint8_t level) OVERRIDE;

  // Set payload type for Redundant Audio Data RFC 2198.
  virtual int32_t SetSendREDPayloadType(const int8_t payload_type) OVERRIDE;

  // Get payload type for Redundant Audio Data RFC 2198.
  virtual int32_t SendREDPayloadType(int8_t& payload_type) const OVERRIDE;

  // Set status and id for header-extension-for-audio-level-indication.
  virtual int32_t SetRTPAudioLevelIndicationStatus(
      const bool enable, const uint8_t id) OVERRIDE;

  // Get status and id for header-extension-for-audio-level-indication.
  virtual int32_t GetRTPAudioLevelIndicationStatus(
      bool& enable, uint8_t& id) const OVERRIDE;

  // Store the audio level in d_bov for header-extension-for-audio-level-
  // indication.
  virtual int32_t SetAudioLevel(const uint8_t level_d_bov) OVERRIDE;

  // Video part.

  virtual RtpVideoCodecTypes SendVideoCodec() const;

  virtual int32_t SendRTCPSliceLossIndication(
      const uint8_t picture_id) OVERRIDE;

  // Set method for requestion a new key frame.
  virtual int32_t SetKeyFrameRequestMethod(
      const KeyFrameRequestMethod method) OVERRIDE;

  // Send a request for a keyframe.
  virtual int32_t RequestKeyFrame() OVERRIDE;

  virtual int32_t SetCameraDelay(const int32_t delay_ms) OVERRIDE;

  virtual void SetTargetSendBitrate(
      const std::vector<uint32_t>& stream_bitrates) OVERRIDE;

  virtual int32_t SetGenericFECStatus(
      const bool enable,
      const uint8_t payload_type_red,
      const uint8_t payload_type_fec) OVERRIDE;

  virtual int32_t GenericFECStatus(
      bool& enable,
      uint8_t& payload_type_red,
      uint8_t& payload_type_fec) OVERRIDE;

  virtual int32_t SetFecParameters(
      const FecProtectionParams* delta_params,
      const FecProtectionParams* key_params) OVERRIDE;

  virtual int32_t LastReceivedNTP(uint32_t& NTPsecs,
                                  uint32_t& NTPfrac,
                                  uint32_t& remote_sr);

  virtual bool LastReceivedXrReferenceTimeInfo(RtcpReceiveTimeInfo* info) const;

  virtual int32_t BoundingSet(bool& tmmbr_owner, TMMBRSet*& bounding_set_rec);

  virtual void BitrateSent(uint32_t* total_rate,
                           uint32_t* video_rate,
                           uint32_t* fec_rate,
                           uint32_t* nackRate) const OVERRIDE;

  virtual void RegisterVideoBitrateObserver(BitrateStatisticsObserver* observer)
      OVERRIDE;

  virtual BitrateStatisticsObserver* GetVideoBitrateObserver() const OVERRIDE;

  virtual uint32_t SendTimeOfSendReport(const uint32_t send_report);

  virtual bool SendTimeOfXrRrReport(uint32_t mid_ntp, int64_t* time_ms) const;

  // Good state of RTP receiver inform sender.
  virtual int32_t SendRTCPReferencePictureSelection(
      const uint64_t picture_id) OVERRIDE;

  virtual void RegisterSendChannelRtpStatisticsCallback(
      StreamDataCountersCallback* callback);
  virtual StreamDataCountersCallback*
      GetSendChannelRtpStatisticsCallback() const;

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

  virtual void RegisterSendFrameCountObserver(
      FrameCountObserver* observer) OVERRIDE;
  virtual FrameCountObserver* GetSendFrameCountObserver() const OVERRIDE;

 protected:
  void RegisterChildModule(RtpRtcp* module);

  void DeRegisterChildModule(RtpRtcp* module);

  bool UpdateRTCPReceiveInformationTimers();

  uint32_t BitrateReceivedNow() const;

  // Get remote SequenceNumber.
  uint16_t RemoteSequenceNumber() const;

  // Only for internal testing.
  uint32_t LastSendReport(uint32_t& last_rtcptime);

  RTPSender                 rtp_sender_;

  RTCPSender                rtcp_sender_;
  RTCPReceiver              rtcp_receiver_;

  Clock*                    clock_;

 private:
  FRIEND_TEST_ALL_PREFIXES(RtpRtcpImplTest, Rtt);
  FRIEND_TEST_ALL_PREFIXES(RtpRtcpImplTest, RttForReceiverOnly);
  int64_t RtcpReportInterval();
  void SetRtcpReceiverSsrcs(uint32_t main_ssrc);

  void set_rtt_ms(uint32_t rtt_ms);
  uint32_t rtt_ms() const;

  bool IsDefaultModule() const;

  int32_t             id_;
  const bool                audio_;
  bool                      collision_detected_;
  int64_t             last_process_time_;
  int64_t             last_bitrate_process_time_;
  int64_t             last_rtt_process_time_;
  uint16_t            packet_overhead_;

  scoped_ptr<CriticalSectionWrapper> critical_section_module_ptrs_;
  scoped_ptr<CriticalSectionWrapper> critical_section_module_ptrs_feedback_;
  ModuleRtpRtcpImpl*            default_module_;
  std::list<ModuleRtpRtcpImpl*> child_modules_;

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

  RtcpRttStats* rtt_stats_;

  // The processed RTT from RtcpRttStats.
  scoped_ptr<CriticalSectionWrapper> critical_section_rtt_;
  uint32_t rtt_ms_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RTCP_IMPL_H_
