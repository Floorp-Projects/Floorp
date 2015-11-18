/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_RTP_RTCP_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_RTP_RTCP_IMPL_H_

#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/vie_ref_count.h"

namespace webrtc {

class ViESharedData;

class ViERTP_RTCPImpl
    : public ViERTP_RTCP,
      public ViERefCount {
 public:
  // Implements ViERTP_RTCP.
  virtual int Release();
  virtual int SetLocalSSRC(const int video_channel,
                           const unsigned int SSRC,
                           const StreamType usage,
                           const unsigned char simulcast_idx);
  virtual int GetLocalSSRC(const int video_channel,
                           unsigned int& SSRC) const;  // NOLINT
  virtual int SetRemoteSSRCType(const int video_channel,
                                const StreamType usage,
                                const unsigned int SSRC) const;
  virtual int GetRemoteSSRC(const int video_channel,
                            unsigned int& SSRC) const;  // NOLINT
  virtual int GetRemoteCSRCs(const int video_channel,
                             unsigned int CSRCs[kRtpCsrcSize]) const;
  virtual int SetRtxSendPayloadType(const int video_channel,
                                    const uint8_t payload_type);
  virtual int SetRtxReceivePayloadType(const int video_channel,
                                       const uint8_t payload_type);
  virtual int SetStartSequenceNumber(const int video_channel,
                                     uint16_t sequence_number);
  void SetRtpStateForSsrc(int video_channel,
                          uint32_t ssrc,
                          const RtpState& rtp_state) override;
  RtpState GetRtpStateForSsrc(int video_channel, uint32_t ssrc) override;
  virtual int SetRTCPStatus(const int video_channel,
                            const ViERTCPMode rtcp_mode);
  virtual int GetRTCPStatus(const int video_channel,
                            ViERTCPMode& rtcp_mode) const;
  virtual int SetRTCPCName(const int video_channel,
                           const char rtcp_cname[KMaxRTCPCNameLength]);
  virtual int GetRemoteRTCPCName(const int video_channel,
                                 char rtcp_cname[KMaxRTCPCNameLength]) const;
  virtual int GetRemoteRTCPReceiverInfo(const int video_channel,
                                        uint32_t& NTPHigh,
                                        uint32_t& NTPLow,
                                        uint32_t& receivedPacketCount,
                                        uint64_t& receivedOctetCount,
                                        uint32_t* jitter,
                                        uint16_t* fractionLost,
                                        uint32_t* cumulativeLost,
                                        int32_t* rttMs) const;
  virtual int SendApplicationDefinedRTCPPacket(
      const int video_channel,
      const unsigned char sub_type,
      unsigned int name,
      const char* data,
      uint16_t data_length_in_bytes);
  virtual int SetNACKStatus(const int video_channel, const bool enable);
  virtual int SetFECStatus(const int video_channel, const bool enable,
                           const unsigned char payload_typeRED,
                           const unsigned char payload_typeFEC);
  virtual int SetHybridNACKFECStatus(const int video_channel, const bool enable,
                                     const unsigned char payload_typeRED,
                                     const unsigned char payload_typeFEC);
  virtual int SetSenderBufferingMode(int video_channel,
                                     int target_delay_ms);
  virtual int SetReceiverBufferingMode(int video_channel,
                                       int target_delay_ms);
  virtual int SetKeyFrameRequestMethod(const int video_channel,
                                       const ViEKeyFrameRequestMethod method);
  virtual int SetTMMBRStatus(const int video_channel, const bool enable);
  virtual int SetRembStatus(int video_channel, bool sender, bool receiver);
  virtual int SetSendTimestampOffsetStatus(int video_channel,
                                           bool enable,
                                           int id);
  virtual int SetReceiveTimestampOffsetStatus(int video_channel,
                                              bool enable,
                                              int id);
  virtual int SetSendAbsoluteSendTimeStatus(int video_channel,
                                            bool enable,
                                            int id);
  virtual int SetReceiveAbsoluteSendTimeStatus(int video_channel,
                                               bool enable,
                                               int id);
  virtual int SetSendVideoRotationStatus(int video_channel,
                                         bool enable,
                                         int id);
  virtual int SetReceiveVideoRotationStatus(int video_channel,
                                            bool enable,
                                            int id);
  virtual int SetRtcpXrRrtrStatus(int video_channel, bool enable);
  virtual int SetTransmissionSmoothingStatus(int video_channel, bool enable);
  virtual int SetMinTransmitBitrate(int video_channel,
                                    int min_transmit_bitrate_kbps);
  virtual int SetReservedTransmitBitrate(
      int video_channel, unsigned int reserved_transmit_bitrate_bps);
  virtual int GetReceiveChannelRtcpStatistics(const int video_channel,
                                              RtcpStatistics& basic_stats,
                                              int64_t& rtt_ms) const;
  virtual int GetSendChannelRtcpStatistics(const int video_channel,
                                           RtcpStatistics& basic_stats,
                                           int64_t& rtt_ms) const;
  virtual int GetRtpStatistics(const int video_channel,
                               StreamDataCounters& sent,
                               StreamDataCounters& received) const;
  virtual int GetSendRtcpPacketTypeCounter(
      int video_channel,
      RtcpPacketTypeCounter* packet_counter) const;
  virtual int GetReceiveRtcpPacketTypeCounter(
      int video_channel,
      RtcpPacketTypeCounter* packet_counter) const;
  virtual int GetRemoteRTCPSenderInfo(const int video_channel,
                                      SenderInfo* sender_info) const;
  virtual int GetBandwidthUsage(const int video_channel,
                                unsigned int& total_bitrate_sent,
                                unsigned int& video_bitrate_sent,
                                unsigned int& fec_bitrate_sent,
                                unsigned int& nackBitrateSent) const;
  virtual int GetEstimatedSendBandwidth(
      const int video_channel,
      unsigned int* estimated_bandwidth) const;
  virtual int GetEstimatedReceiveBandwidth(
      const int video_channel,
      unsigned int* estimated_bandwidth) const;
  virtual int GetPacerQueuingDelayMs(const int video_channel,
                                     int64_t* delay_ms) const;
  virtual int StartRTPDump(const int video_channel,
                           const char file_nameUTF8[1024],
                           RTPDirections direction);
  virtual int StopRTPDump(const int video_channel, RTPDirections direction);
  virtual int RegisterRTPObserver(const int video_channel,
                                  ViERTPObserver& observer);
  virtual int DeregisterRTPObserver(const int video_channel);

  virtual int RegisterSendChannelRtcpStatisticsCallback(
      int channel, RtcpStatisticsCallback* callback);
  virtual int DeregisterSendChannelRtcpStatisticsCallback(
      int channel, RtcpStatisticsCallback* callback);
  virtual int RegisterReceiveChannelRtcpStatisticsCallback(
      int channel, RtcpStatisticsCallback* callback);
  virtual int DeregisterReceiveChannelRtcpStatisticsCallback(
      int channel, RtcpStatisticsCallback* callback);
  virtual int RegisterSendChannelRtpStatisticsCallback(
      int channel, StreamDataCountersCallback* callback);
  virtual int DeregisterSendChannelRtpStatisticsCallback(
      int channel, StreamDataCountersCallback* callback);
  virtual int RegisterReceiveChannelRtpStatisticsCallback(
      int channel, StreamDataCountersCallback* callback);
  virtual int DeregisterReceiveChannelRtpStatisticsCallback(
      int channel, StreamDataCountersCallback* callback);
  virtual int RegisterSendBitrateObserver(
      int channel, BitrateStatisticsObserver* callback);
  virtual int DeregisterSendBitrateObserver(
      int channel, BitrateStatisticsObserver* callback);
  virtual int RegisterSendFrameCountObserver(
      int channel, FrameCountObserver* callback);
  virtual int DeregisterSendFrameCountObserver(
      int channel, FrameCountObserver* callback);
  int RegisterRtcpPacketTypeCounterObserver(
      int video_channel,
      RtcpPacketTypeCounterObserver* observer) override;

 protected:
  explicit ViERTP_RTCPImpl(ViESharedData* shared_data);
  virtual ~ViERTP_RTCPImpl();

 private:
  ViESharedData* shared_data_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_RTP_RTCP_IMPL_H_
