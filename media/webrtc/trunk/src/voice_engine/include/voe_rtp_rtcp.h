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
//  - Forward Error Correction (FEC).
//  - Writing RTP and RTCP packets to binary files for off-line analysis of
//    the call quality.
//  - Inserting extra RTP packets into active audio stream.
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
#include "common_types.h"

namespace webrtc {

class VoiceEngine;

// VoERTPObserver
class WEBRTC_DLLEXPORT VoERTPObserver
{
public:
    virtual void OnIncomingCSRCChanged(
        const int channel, const unsigned int CSRC, const bool added) = 0;

    virtual void OnIncomingSSRCChanged(
        const int channel, const unsigned int SSRC) = 0;

protected:
    virtual ~VoERTPObserver() {}
};

// VoERTCPObserver
class WEBRTC_DLLEXPORT VoERTCPObserver
{
public:
    virtual void OnApplicationDataReceived(
        const int channel, const unsigned char subType,
        const unsigned int name, const unsigned char* data,
        const unsigned short dataLengthInBytes) = 0;

protected:
    virtual ~VoERTCPObserver() {}
};

// CallStatistics
struct CallStatistics
{
    unsigned short fractionLost;
    unsigned int cumulativeLost;
    unsigned int extendedMax;
    unsigned int jitterSamples;
    int rttMs;
    int bytesSent;
    int packetsSent;
    int bytesReceived;
    int packetsReceived;
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

    // Registers an instance of a VoERTPObserver derived class for a specified
    // |channel|. It will allow the user to observe callbacks related to the
    // RTP protocol such as changes in the incoming SSRC.
    virtual int RegisterRTPObserver(int channel, VoERTPObserver& observer) = 0;

    // Deregisters an instance of a VoERTPObserver derived class for a
    // specified |channel|.
    virtual int DeRegisterRTPObserver(int channel) = 0;

    // Registers an instance of a VoERTCPObserver derived class for a specified
    // |channel|.
    virtual int RegisterRTCPObserver(
        int channel, VoERTCPObserver& observer) = 0;

    // Deregisters an instance of a VoERTCPObserver derived class for a
    // specified |channel|.
    virtual int DeRegisterRTCPObserver(int channel) = 0;

    // Sets the local RTP synchronization source identifier (SSRC) explicitly.
    virtual int SetLocalSSRC(int channel, unsigned int ssrc) = 0;

    // Gets the local RTP SSRC of a specified |channel|.
    virtual int GetLocalSSRC(int channel, unsigned int& ssrc) = 0;

    // Gets the SSRC of the incoming RTP packets.
    virtual int GetRemoteSSRC(int channel, unsigned int& ssrc) = 0;

    // Sets the status of rtp-audio-level-indication on a specific |channel|.
    virtual int SetRTPAudioLevelIndicationStatus(
        int channel, bool enable, unsigned char ID = 1) = 0;

    // Sets the status of rtp-audio-level-indication on a specific |channel|.
    virtual int GetRTPAudioLevelIndicationStatus(
        int channel, bool& enabled, unsigned char& ID) = 0;

    // Gets the CSRCs of the incoming RTP packets.
    virtual int GetRemoteCSRCs(int channel, unsigned int arrCSRC[15]) = 0;

    // Sets the RTCP status on a specific |channel|.
    virtual int SetRTCPStatus(int channel, bool enable) = 0;

    // Gets the RTCP status on a specific |channel|.
    virtual int GetRTCPStatus(int channel, bool& enabled) = 0;

    // Sets the canonical name (CNAME) parameter for RTCP reports on a
    // specific |channel|.
    virtual int SetRTCP_CNAME(int channel, const char cName[256]) = 0;

    // Gets the canonical name (CNAME) parameter for RTCP reports on a
    // specific |channel|.
    virtual int GetRTCP_CNAME(int channel, char cName[256]) = 0;

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

    // Gets the sender info part of the last received RTCP Sender Report (SR)
    // on a specified |channel|.
    virtual int GetRemoteRTCPSenderInfo(
        int channel, SenderInfo* sender_info) = 0;

    // Gets the report block parts of the last received RTCP Sender Report (SR),
    // or RTCP Receiver Report (RR) on a specified |channel|. Each vector
    // element also contains the SSRC of the sender in addition to a report
    // block.
    virtual int GetRemoteRTCPReportBlocks(
        int channel, std::vector<ReportBlock>* receive_blocks) = 0;

    // Sends an RTCP APP packet on a specific |channel|.
    virtual int SendApplicationDefinedRTCPPacket(
        int channel, const unsigned char subType, unsigned int name,
        const char* data, unsigned short dataLengthInBytes) = 0;

    // Sets the Forward Error Correction (FEC) status on a specific |channel|.
    virtual int SetFECStatus(
        int channel, bool enable, int redPayloadtype = -1) = 0;

    // Gets the FEC status on a specific |channel|.
    virtual int GetFECStatus(
        int channel, bool& enabled, int& redPayloadtype) = 0;

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

    // Sends an extra RTP packet using an existing/active RTP session.
    // It is possible to set the payload type, marker bit and payload
    // of the extra RTP
    virtual int InsertExtraRTPPacket(
        int channel, unsigned char payloadType, bool markerBit,
        const char* payloadData, unsigned short payloadSize) = 0;

protected:
    VoERTP_RTCP() {}
    virtual ~VoERTP_RTCP() {}
};

}  // namespace webrtc

#endif  // #ifndef WEBRTC_VOICE_ENGINE_VOE_RTP_RTCP_H
