/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_IMPL_H

#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"

#include "webrtc/voice_engine/shared_data.h"

namespace webrtc {

class VoERTP_RTCPImpl : public VoERTP_RTCP {
 public:
  // RTCP
  int SetRTCPStatus(int channel, bool enable) override;

  int GetRTCPStatus(int channel, bool& enabled) override;

  int SetRTCP_CNAME(int channel, const char cName[256]) override;

  int GetRemoteRTCP_CNAME(int channel, char cName[256]) override;

  int GetRemoteRTCPReceiverInfo(int channel,
                                uint32_t& NTPHigh,
                                uint32_t& NTPLow,
                                uint32_t& receivedPacketCount,
                                uint64_t& receivedOctetCount,
                                uint32_t& jitter,
                                uint16_t& fractionLost,
                                uint32_t& cumulativeLost,
                                int32_t& rttMs) override;

  // SSRC
  int SetLocalSSRC(int channel, unsigned int ssrc) override;

  int GetLocalSSRC(int channel, unsigned int& ssrc) override;

  int GetRemoteSSRC(int channel, unsigned int& ssrc) override;

  // RTP Header Extension for Client-to-Mixer Audio Level Indication
  int SetSendAudioLevelIndicationStatus(int channel,
                                        bool enable,
                                        unsigned char id) override;
  int SetReceiveAudioLevelIndicationStatus(int channel,
                                           bool enable,
                                           unsigned char id) override;

  // RTP Header Extension for Absolute Sender Time
  int SetSendAbsoluteSenderTimeStatus(int channel,
                                      bool enable,
                                      unsigned char id) override;
  int SetReceiveAbsoluteSenderTimeStatus(int channel,
                                         bool enable,
                                         unsigned char id) override;

  // Statistics
  int GetRTPStatistics(int channel,
                       unsigned int& averageJitterMs,
                       unsigned int& maxJitterMs,
                       unsigned int& discardedPackets,
                       unsigned int& cumulativeLost) override;

  int GetRTCPStatistics(int channel, CallStatistics& stats) override;

  int GetRemoteRTCPReportBlocks(
      int channel,
      std::vector<ReportBlock>* report_blocks) override;

  // RED
  int SetREDStatus(int channel, bool enable, int redPayloadtype = -1) override;

  int GetREDStatus(int channel, bool& enabled, int& redPayloadtype) override;

  // NACK
  int SetNACKStatus(int channel, bool enable, int maxNoPackets) override;

 protected:
  VoERTP_RTCPImpl(voe::SharedData* shared);
  ~VoERTP_RTCPImpl() override;

 private:
  voe::SharedData* _shared;
};

}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_IMPL_H
