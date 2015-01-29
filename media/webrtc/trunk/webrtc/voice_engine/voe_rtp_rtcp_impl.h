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

class VoERTP_RTCPImpl : public VoERTP_RTCP
{
public:
    // RTCP
    virtual int SetRTCPStatus(int channel, bool enable);

    virtual int GetRTCPStatus(int channel, bool& enabled);

    virtual int SetRTCP_CNAME(int channel, const char cName[256]);

    virtual int GetRemoteRTCP_CNAME(int channel, char cName[256]);

    virtual int GetRemoteRTCPReceiverInfo(int channel,
                                          uint32_t& NTPHigh,
                                          uint32_t& NTPLow,
                                          uint32_t& receivedPacketCount,
                                          uint64_t& receivedOctetCount,
                                          uint32_t& jitter,
                                          uint16_t& fractionLost,
                                          uint32_t& cumulativeLost,
                                          int32_t& rttMs);

    // SSRC
    virtual int SetLocalSSRC(int channel, unsigned int ssrc);

    virtual int GetLocalSSRC(int channel, unsigned int& ssrc);

    virtual int GetRemoteSSRC(int channel, unsigned int& ssrc);

    // RTP Header Extension for Client-to-Mixer Audio Level Indication
    virtual int SetSendAudioLevelIndicationStatus(int channel,
                                                  bool enable,
                                                  unsigned char id);
    virtual int SetReceiveAudioLevelIndicationStatus(int channel,
                                                     bool enable,
                                                     unsigned char id);

    // RTP Header Extension for Absolute Sender Time
    virtual int SetSendAbsoluteSenderTimeStatus(int channel,
                                                bool enable,
                                                unsigned char id);
    virtual int SetReceiveAbsoluteSenderTimeStatus(int channel,
                                                   bool enable,
                                                   unsigned char id);

    // Statistics
    virtual int GetRTPStatistics(int channel,
                                 unsigned int& averageJitterMs,
                                 unsigned int& maxJitterMs,
                                 unsigned int& discardedPackets,
                                 unsigned int& cumulativeLost);

    virtual int GetRTCPStatistics(int channel, CallStatistics& stats);

    virtual int GetRemoteRTCPReportBlocks(
        int channel, std::vector<ReportBlock>* report_blocks);

    // RED
    virtual int SetREDStatus(int channel,
                             bool enable,
                             int redPayloadtype = -1);

    virtual int GetREDStatus(int channel, bool& enabled, int& redPayloadtype);

    //NACK
    virtual int SetNACKStatus(int channel,
                              bool enable,
                              int maxNoPackets);

    // Store RTP and RTCP packets and dump to file (compatible with rtpplay)
    virtual int StartRTPDump(int channel,
                             const char fileNameUTF8[1024],
                             RTPDirections direction = kRtpIncoming);

    virtual int StopRTPDump(int channel,
                            RTPDirections direction = kRtpIncoming);

    virtual int RTPDumpIsActive(int channel,
                                RTPDirections direction = kRtpIncoming);

    virtual int SetVideoEngineBWETarget(int channel, ViENetwork* vie_network,
                                        int video_channel);
protected:
    VoERTP_RTCPImpl(voe::SharedData* shared);
    virtual ~VoERTP_RTCPImpl();

private:
    voe::SharedData* _shared;
};

}  // namespace webrtc

#endif    // WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_IMPL_H
