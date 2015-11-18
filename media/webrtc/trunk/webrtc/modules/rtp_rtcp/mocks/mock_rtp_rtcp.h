/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_MOCKS_MOCK_RTP_RTCP_H_
#define WEBRTC_MODULES_RTP_RTCP_MOCKS_MOCK_RTP_RTCP_H_

#include "testing/gmock/include/gmock/gmock.h"

#include "webrtc/modules/interface/module.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"

namespace webrtc {

class MockRtpData : public RtpData {
 public:
  MOCK_METHOD3(OnReceivedPayloadData,
               int32_t(const uint8_t* payloadData,
                       const size_t payloadSize,
                       const WebRtcRTPHeader* rtpHeader));

  MOCK_METHOD2(OnRecoveredPacket,
               bool(const uint8_t* packet, size_t packet_length));
};

class MockRtpRtcp : public RtpRtcp {
 public:
  MOCK_METHOD1(RegisterDefaultModule,
      int32_t(RtpRtcp* module));
  MOCK_METHOD0(DeRegisterDefaultModule,
      int32_t());
  MOCK_METHOD0(DefaultModuleRegistered,
      bool());
  MOCK_METHOD0(NumberChildModules,
      uint32_t());
  MOCK_METHOD1(RegisterSyncModule,
      int32_t(RtpRtcp* module));
  MOCK_METHOD0(DeRegisterSyncModule,
      int32_t());
  MOCK_METHOD2(IncomingRtcpPacket,
      int32_t(const uint8_t* incomingPacket, size_t packetLength));
  MOCK_METHOD1(SetRemoteSSRC, void(const uint32_t ssrc));
  MOCK_METHOD4(IncomingAudioNTP,
      int32_t(const uint32_t audioReceivedNTPsecs,
              const uint32_t audioReceivedNTPfrac,
              const uint32_t audioRTCPArrivalTimeSecs,
              const uint32_t audioRTCPArrivalTimeFrac));
  MOCK_METHOD0(InitSender,
      int32_t());
  MOCK_METHOD1(RegisterSendTransport,
      int32_t(Transport* outgoingTransport));
  MOCK_METHOD1(SetMaxTransferUnit,
      int32_t(const uint16_t size));
  MOCK_METHOD3(SetTransportOverhead,
      int32_t(const bool TCP, const bool IPV6,
              const uint8_t authenticationOverhead));
  MOCK_CONST_METHOD0(MaxPayloadLength,
      uint16_t());
  MOCK_CONST_METHOD0(MaxDataPayloadLength,
      uint16_t());
  MOCK_METHOD1(RegisterSendPayload,
      int32_t(const CodecInst& voiceCodec));
  MOCK_METHOD1(RegisterSendPayload,
      int32_t(const VideoCodec& videoCodec));
  MOCK_METHOD1(DeRegisterSendPayload,
      int32_t(const int8_t payloadType));
  MOCK_METHOD2(RegisterSendRtpHeaderExtension,
      int32_t(const RTPExtensionType type, const uint8_t id));
  MOCK_METHOD1(DeregisterSendRtpHeaderExtension,
      int32_t(const RTPExtensionType type));
  MOCK_CONST_METHOD0(StartTimestamp,
      uint32_t());
  MOCK_METHOD1(SetStartTimestamp, void(const uint32_t timestamp));
  MOCK_CONST_METHOD0(SequenceNumber,
      uint16_t());
  MOCK_METHOD1(SetSequenceNumber, void(const uint16_t seq));
  MOCK_METHOD2(SetRtpStateForSsrc,
               bool(uint32_t ssrc, const RtpState& rtp_state));
  MOCK_METHOD2(GetRtpStateForSsrc, bool(uint32_t ssrc, RtpState* rtp_state));
  MOCK_CONST_METHOD0(SSRC,
      uint32_t());
  MOCK_METHOD1(SetSSRC,
      void(const uint32_t ssrc));
  MOCK_CONST_METHOD1(CSRCs,
      int32_t(uint32_t arrOfCSRC[kRtpCsrcSize]));
  MOCK_METHOD1(SetCsrcs, void(const std::vector<uint32_t>& csrcs));
  MOCK_METHOD1(SetCSRCStatus,
      int32_t(const bool include));
  MOCK_METHOD1(SetRtxSendStatus, void(int modes));
  MOCK_CONST_METHOD0(RtxSendStatus, int());
  MOCK_METHOD1(SetRtxSsrc,
      void(uint32_t));
  MOCK_METHOD1(SetRtxSendPayloadType,
      void(int));
  MOCK_METHOD1(SetSendingStatus,
      int32_t(const bool sending));
  MOCK_CONST_METHOD0(Sending,
      bool());
  MOCK_METHOD1(SetSendingMediaStatus, void(const bool sending));
  MOCK_CONST_METHOD0(SendingMedia,
      bool());
  MOCK_CONST_METHOD4(BitrateSent,
      void(uint32_t* totalRate, uint32_t* videoRate, uint32_t* fecRate, uint32_t* nackRate));
  MOCK_METHOD1(RegisterVideoBitrateObserver, void(BitrateStatisticsObserver*));
  MOCK_CONST_METHOD0(GetVideoBitrateObserver, BitrateStatisticsObserver*(void));
  MOCK_CONST_METHOD1(EstimatedReceiveBandwidth,
      int(uint32_t* available_bandwidth));
  MOCK_METHOD8(SendOutgoingData,
      int32_t(const FrameType frameType,
              const int8_t payloadType,
              const uint32_t timeStamp,
              int64_t capture_time_ms,
              const uint8_t* payloadData,
              const size_t payloadSize,
              const RTPFragmentationHeader* fragmentation,
              const RTPVideoHeader* rtpVideoHdr));
  MOCK_METHOD4(TimeToSendPacket,
      bool(uint32_t ssrc, uint16_t sequence_number, int64_t capture_time_ms,
           bool retransmission));
  MOCK_METHOD1(TimeToSendPadding,
      size_t(size_t bytes));
  MOCK_CONST_METHOD2(GetSendSideDelay,
      bool(int* avg_send_delay_ms, int* max_send_delay_ms));
  MOCK_METHOD2(RegisterRtcpObservers,
      void(RtcpIntraFrameObserver* intraFrameCallback,
           RtcpBandwidthObserver* bandwidthCallback));
  MOCK_CONST_METHOD0(RTCP,
      RTCPMethod());
  MOCK_METHOD1(SetRTCPStatus, void(const RTCPMethod method));
  MOCK_METHOD1(SetCNAME,
      int32_t(const char cName[RTCP_CNAME_SIZE]));
  MOCK_CONST_METHOD2(RemoteCNAME,
      int32_t(const uint32_t remoteSSRC,
              char cName[RTCP_CNAME_SIZE]));
  MOCK_CONST_METHOD5(RemoteNTP,
      int32_t(uint32_t *ReceivedNTPsecs,
              uint32_t *ReceivedNTPfrac,
              uint32_t *RTCPArrivalTimeSecs,
              uint32_t *RTCPArrivalTimeFrac,
              uint32_t *rtcp_timestamp));
  MOCK_METHOD2(AddMixedCNAME,
      int32_t(const uint32_t SSRC,
              const char cName[RTCP_CNAME_SIZE]));
  MOCK_METHOD1(RemoveMixedCNAME,
      int32_t(const uint32_t SSRC));
  MOCK_CONST_METHOD5(RTT,
      int32_t(const uint32_t remoteSSRC,
              int64_t* RTT,
              int64_t* avgRTT,
              int64_t* minRTT,
              int64_t* maxRTT));
  MOCK_METHOD1(SendRTCP,
      int32_t(uint32_t rtcpPacketType));
  MOCK_METHOD1(SendRTCPReferencePictureSelection,
      int32_t(const uint64_t pictureID));
  MOCK_METHOD1(SendRTCPSliceLossIndication,
      int32_t(const uint8_t pictureID));
  MOCK_METHOD0(ResetSendDataCountersRTP,
      int32_t());
  MOCK_CONST_METHOD2(DataCountersRTP,
      int32_t(size_t *bytesSent, uint32_t *packetsSent));
  MOCK_CONST_METHOD2(GetSendStreamDataCounters,
      void(StreamDataCounters*, StreamDataCounters*));
  MOCK_METHOD1(RemoteRTCPStat,
      int32_t(RTCPSenderInfo* senderInfo));
  MOCK_CONST_METHOD1(RemoteRTCPStat,
      int32_t(std::vector<RTCPReportBlock>* receiveBlocks));
  MOCK_METHOD2(AddRTCPReportBlock,
      int32_t(const uint32_t SSRC, const RTCPReportBlock* receiveBlock));
  MOCK_METHOD1(RemoveRTCPReportBlock,
      int32_t(const uint32_t SSRC));
  MOCK_METHOD4(SetRTCPApplicationSpecificData,
      int32_t(const uint8_t subType, const uint32_t name, const uint8_t* data, const uint16_t length));
  MOCK_METHOD1(SetRTCPVoIPMetrics,
      int32_t(const RTCPVoIPMetric* VoIPMetric));
  MOCK_METHOD1(SetRtcpXrRrtrStatus,
      void(bool enable));
  MOCK_CONST_METHOD0(RtcpXrRrtrStatus,
      bool());
  MOCK_CONST_METHOD0(REMB,
      bool());
  MOCK_METHOD1(SetREMBStatus, void(const bool enable));
  MOCK_METHOD2(SetREMBData,
               void(const uint32_t bitrate,
                    const std::vector<uint32_t>& ssrcs));
  MOCK_CONST_METHOD0(IJ,
      bool());
  MOCK_METHOD1(SetIJStatus, void(const bool));
  MOCK_CONST_METHOD0(TMMBR,
      bool());
  MOCK_METHOD1(SetTMMBRStatus, void(const bool enable));
  MOCK_METHOD1(OnBandwidthEstimateUpdate,
      void(uint16_t bandWidthKbit));
  MOCK_CONST_METHOD0(NACK,
      NACKMethod());
  MOCK_METHOD2(SetNACKStatus,
      int32_t(const NACKMethod method, int oldestSequenceNumberToNack));
  MOCK_CONST_METHOD0(SelectiveRetransmissions,
      int());
  MOCK_METHOD1(SetSelectiveRetransmissions,
      int(uint8_t settings));
  MOCK_METHOD2(SendNACK,
      int32_t(const uint16_t* nackList, const uint16_t size));
  MOCK_METHOD2(SetStorePacketsStatus,
               void(const bool enable, const uint16_t numberToStore));
  MOCK_CONST_METHOD0(StorePackets, bool());
  MOCK_METHOD1(RegisterRtcpStatisticsCallback, void(RtcpStatisticsCallback*));
  MOCK_METHOD0(GetRtcpStatisticsCallback, RtcpStatisticsCallback*());
  MOCK_METHOD1(RegisterAudioCallback,
      int32_t(RtpAudioFeedback* messagesCallback));
  MOCK_METHOD1(SetAudioPacketSize,
      int32_t(const uint16_t packetSizeSamples));
  MOCK_METHOD3(SendTelephoneEventOutband,
      int32_t(const uint8_t key, const uint16_t time_ms, const uint8_t level));
  MOCK_METHOD1(SetSendREDPayloadType,
      int32_t(const int8_t payloadType));
  MOCK_CONST_METHOD1(SendREDPayloadType,
      int32_t(int8_t& payloadType));
  MOCK_METHOD2(SetRTPAudioLevelIndicationStatus,
      int32_t(const bool enable, const uint8_t ID));
  MOCK_CONST_METHOD2(GetRTPAudioLevelIndicationStatus,
      int32_t(bool& enable, uint8_t& ID));
  MOCK_METHOD1(SetAudioLevel,
      int32_t(const uint8_t level_dBov));
  MOCK_METHOD1(SetTargetSendBitrate,
      void(uint32_t bitrate_bps));
  MOCK_METHOD3(SetGenericFECStatus,
      int32_t(const bool enable,
              const uint8_t payloadTypeRED,
              const uint8_t payloadTypeFEC));
  MOCK_METHOD3(GenericFECStatus,
      int32_t(bool& enable, uint8_t& payloadTypeRED, uint8_t& payloadTypeFEC));
  MOCK_METHOD2(SetFecParameters,
      int32_t(const FecProtectionParams* delta_params,
              const FecProtectionParams* key_params));
  MOCK_METHOD1(SetKeyFrameRequestMethod,
      int32_t(const KeyFrameRequestMethod method));
  MOCK_METHOD0(RequestKeyFrame,
      int32_t());
  MOCK_CONST_METHOD3(Version,
      int32_t(char* version, uint32_t& remaining_buffer_in_bytes, uint32_t& position));
  MOCK_METHOD0(TimeUntilNextProcess,
        int64_t());
  MOCK_METHOD0(Process,
        int32_t());
  MOCK_METHOD1(RegisterSendFrameCountObserver,
      void(FrameCountObserver*));
  MOCK_CONST_METHOD0(GetSendFrameCountObserver,
      FrameCountObserver*(void));
  MOCK_METHOD1(RegisterSendChannelRtpStatisticsCallback,
      void(StreamDataCountersCallback*));
  MOCK_CONST_METHOD0(GetSendChannelRtpStatisticsCallback,
      StreamDataCountersCallback*(void));
  // Members.
  unsigned int remote_ssrc_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_MOCKS_MOCK_RTP_RTCP_H_
