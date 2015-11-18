/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Callbacks for RTP and RTCP events such as modified SSRC or CSRC.
//  - SSRC handling.
//  - Transmission of RTCP sender reports.
//  - Obtaining RTCP data from incoming RTCP sender reports.
//  - RTP and RTCP statistics (jitter, packet loss, RTT etc.).
//  - Redundant Coding (RED)
//  - Writing RTP and RTCP packets to binary files for off-line analysis of
//    the call quality.
//
// Usage example, omitting error checking:
//
//  using namespace webrtc;
//  VoiceEngine* voe = VoiceEngine::Create();
//  VoEBase* base = VoEBase::GetInterface(voe);
//  VoERTP_RTCP* rtp_rtcp  = VoERTP_RTCP::GetInterface(voe);
//  base->Init();
//  int ch = base->CreateChannel();
//  ...
//  rtp_rtcp->SetLocalSSRC(ch, 12345);
//  ...
//  base->DeleteChannel(ch);
//  base->Terminate();
//  base->Release();
//  rtp_rtcp->Release();
//  VoiceEngine::Delete(voe);
//
#ifndef WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_H
#define WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_H

#include <vector>
#include "webrtc/common_types.h"

namespace webrtc {

class ViENetwork;
class VoiceEngine;

// VoERTPObserver
class WEBRTC_DLLEXPORT VoERTPObserver
{
public:
    virtual void OnIncomingCSRCChanged(
        int channel, unsigned int CSRC, bool added) = 0;

    virtual void OnIncomingSSRCChanged(
        int channel, unsigned int SSRC) = 0;

protected:
    virtual ~VoERTPObserver() {}
};

// CallStatistics
struct CallStatistics
{
    unsigned short fractionLost;
    unsigned int cumulativeLost;
    unsigned int extendedMax;
    unsigned int jitterSamples;
    int64_t rttMs;
    size_t bytesSent;
    int packetsSent;
    size_t bytesReceived;
    int packetsReceived;
    // The capture ntp time (in local timebase) of the first played out audio
    // frame.
    int64_t capture_start_ntp_time_ms_;
};

// See section 6.4.1 in http://www.ietf.org/rfc/rfc3550.txt for details.
struct SenderInfo {
  uint32_t NTP_timestamp_high;
  uint32_t NTP_timestamp_low;
  uint32_t RTP_timestamp;
  uint32_t sender_packet_count;
  uint32_t sender_octet_count;
};

// See section 6.4.2 in http://www.ietf.org/rfc/rfc3550.txt for details.
struct ReportBlock {
  uint32_t sender_SSRC; // SSRC of sender
  uint32_t source_SSRC;
  uint8_t fraction_lost;
  uint32_t cumulative_num_packets_lost;
  uint32_t extended_highest_sequence_number;
  uint32_t interarrival_jitter;
  uint32_t last_SR_timestamp;
  uint32_t delay_since_last_SR;
};

// VoERTP_RTCP
class WEBRTC_DLLEXPORT VoERTP_RTCP
{
public:

    // Factory for the VoERTP_RTCP sub-API. Increases an internal
    // reference counter if successful. Returns NULL if the API is not
    // supported or if construction fails.
    static VoERTP_RTCP* GetInterface(VoiceEngine* voiceEngine);

    // Releases the VoERTP_RTCP sub-API and decreases an internal
    // reference counter. Returns the new reference count. This value should
    // be zero for all sub-API:s before the VoiceEngine object can be safely
    // deleted.
    virtual int Release() = 0;

    // Sets the local RTP synchronization source identifier (SSRC) explicitly.
    virtual int SetLocalSSRC(int channel, unsigned int ssrc) = 0;

    // Gets the local RTP SSRC of a specified |channel|.
    virtual int GetLocalSSRC(int channel, unsigned int& ssrc) = 0;

    // Gets the SSRC of the incoming RTP packets.
    virtual int GetRemoteSSRC(int channel, unsigned int& ssrc) = 0;

    // Sets the status of rtp-audio-level-indication on a specific |channel|.
    virtual int SetSendAudioLevelIndicationStatus(int channel,
                                                  bool enable,
                                                  unsigned char id = 1) = 0;

    // Sets the status of receiving rtp-audio-level-indication on a specific
    // |channel|.
    virtual int SetReceiveAudioLevelIndicationStatus(int channel,
                                                     bool enable,
                                                     unsigned char id = 1) {
      // TODO(wu): Remove default implementation once talk is updated.
      return 0;
    }

    // Sets the status of sending absolute sender time on a specific |channel|.
    virtual int SetSendAbsoluteSenderTimeStatus(int channel,
                                                bool enable,
                                                unsigned char id) = 0;

    // Sets status of receiving absolute sender time on a specific |channel|.
    virtual int SetReceiveAbsoluteSenderTimeStatus(int channel,
                                                   bool enable,
                                                   unsigned char id) = 0;

    // Sets the RTCP status on a specific |channel|.
    virtual int SetRTCPStatus(int channel, bool enable) = 0;

    // Gets the RTCP status on a specific |channel|.
    virtual int GetRTCPStatus(int channel, bool& enabled) = 0;

    // Sets the canonical name (CNAME) parameter for RTCP reports on a
    // specific |channel|.
    virtual int SetRTCP_CNAME(int channel, const char cName[256]) = 0;

    // TODO(holmer): Remove this API once it has been removed from
    // fakewebrtcvoiceengine.h.
    virtual int GetRTCP_CNAME(int channel, char cName[256]) {
      return -1;
    }

    // Gets the canonical name (CNAME) parameter for incoming RTCP reports
    // on a specific channel.
    virtual int GetRemoteRTCP_CNAME(int channel, char cName[256]) = 0;

    // Gets RTCP data from incoming RTCP Sender Reports.
    virtual int GetRemoteRTCPData(
        int channel, unsigned int& NTPHigh, unsigned int& NTPLow,
        unsigned int& timestamp, unsigned int& playoutTimestamp,
        unsigned int* jitter = NULL, unsigned short* fractionLost = NULL) = 0;

    // Gets RTP statistics for a specific |channel|.
    virtual int GetRTPStatistics(
        int channel, unsigned int& averageJitterMs, unsigned int& maxJitterMs,
        unsigned int& discardedPackets) = 0;

    // Gets RTCP statistics for a specific |channel|.
    virtual int GetRTCPStatistics(int channel, CallStatistics& stats) = 0;

    // Gets the report block parts of the last received RTCP Sender Report (SR),
    // or RTCP Receiver Report (RR) on a specified |channel|. Each vector
    // element also contains the SSRC of the sender in addition to a report
    // block.
    virtual int GetRemoteRTCPReportBlocks(
        int channel, std::vector<ReportBlock>* receive_blocks) = 0;

    // Sets the Redundant Coding (RED) status on a specific |channel|.
    // TODO(minyue): Make SetREDStatus() pure virtual when fakewebrtcvoiceengine
    // in talk is ready.
    virtual int SetREDStatus(
        int channel, bool enable, int redPayloadtype = -1) { return -1; }

    // Gets the RED status on a specific |channel|.
    // TODO(minyue): Make GetREDStatus() pure virtual when fakewebrtcvoiceengine
    // in talk is ready.
    virtual int GetREDStatus(
        int channel, bool& enabled, int& redPayloadtype) { return -1; }

    // Sets the Forward Error Correction (FEC) status on a specific |channel|.
    // TODO(minyue): Remove SetFECStatus() when SetFECStatus() is replaced by
    // SetREDStatus() in fakewebrtcvoiceengine.
    virtual int SetFECStatus(
        int channel, bool enable, int redPayloadtype = -1) {
      return SetREDStatus(channel, enable, redPayloadtype);
    };

    // Gets the FEC status on a specific |channel|.
    // TODO(minyue): Remove GetFECStatus() when GetFECStatus() is replaced by
    // GetREDStatus() in fakewebrtcvoiceengine.
    virtual int GetFECStatus(
        int channel, bool& enabled, int& redPayloadtype) {
      return SetREDStatus(channel, enabled, redPayloadtype);
    }

    // This function enables Negative Acknowledgment (NACK) using RTCP,
    // implemented based on RFC 4585. NACK retransmits RTP packets if lost on
    // the network. This creates a lossless transport at the expense of delay.
    // If using NACK, NACK should be enabled on both endpoints in a call.
    virtual int SetNACKStatus(int channel,
                              bool enable,
                              int maxNoPackets) = 0;

    // Enables capturing of RTP packets to a binary file on a specific
    // |channel| and for a given |direction|. The file can later be replayed
    // using e.g. RTP Tools rtpplay since the binary file format is
    // compatible with the rtpdump format.
    virtual int StartRTPDump(
        int channel, const char fileNameUTF8[1024],
        RTPDirections direction = kRtpIncoming) = 0;

    // Disables capturing of RTP packets to a binary file on a specific
    // |channel| and for a given |direction|.
    virtual int StopRTPDump(
        int channel, RTPDirections direction = kRtpIncoming) = 0;

    // Gets the the current RTP capturing state for the specified
    // |channel| and |direction|.
    virtual int RTPDumpIsActive(
        int channel, RTPDirections direction = kRtpIncoming) = 0;

    // Sets video engine channel to receive incoming audio packets for
    // aggregated bandwidth estimation. Takes ownership of the ViENetwork
    // interface.
    virtual int SetVideoEngineBWETarget(int channel, ViENetwork* vie_network,
                                        int video_channel) {
      return 0;
    }

    // Will be removed. Don't use.
    virtual int RegisterRTPObserver(int channel,
            VoERTPObserver& observer) { return -1; };
    virtual int DeRegisterRTPObserver(int channel) { return -1; };
    virtual int GetRemoteCSRCs(int channel,
            unsigned int arrCSRC[15]) { return -1; };
    virtual int InsertExtraRTPPacket(
            int channel, unsigned char payloadType, bool markerBit,
            const char* payloadData, unsigned short payloadSize) { return -1; };
    virtual int GetRemoteRTCPSenderInfo(
            int channel, SenderInfo* sender_info) { return -1; };
    virtual int SendApplicationDefinedRTCPPacket(
            int channel, unsigned char subType, unsigned int name,
            const char* data, unsigned short dataLengthInBytes) { return -1; };
    virtual int GetLastRemoteTimeStamp(int channel,
            uint32_t* lastRemoteTimeStamp) { return -1; };

protected:
    VoERTP_RTCP() {}
    virtual ~VoERTP_RTCP() {}
};

}  // namespace webrtc

#endif  // #ifndef WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_H
