/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_

#include <map>

#include "typedefs.h"
#include "rtcp_utility.h"
#include "rtp_utility.h"
#include "rtp_rtcp_defines.h"
#include "scoped_ptr.h"
#include "tmmbr_help.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"

namespace webrtc {

class ModuleRtpRtcpImpl; 

class RTCPSender
{
public:
    RTCPSender(const WebRtc_Word32 id, const bool audio,
               RtpRtcpClock* clock, ModuleRtpRtcpImpl* owner);
    virtual ~RTCPSender();

    void ChangeUniqueId(const WebRtc_Word32 id);

    WebRtc_Word32 Init();

    WebRtc_Word32 RegisterSendTransport(Transport* outgoingTransport);

    RTCPMethod Status() const;
    WebRtc_Word32 SetRTCPStatus(const RTCPMethod method);

    bool Sending() const;
    WebRtc_Word32 SetSendingStatus(const bool enabled); // combine the functions

    WebRtc_Word32 SetNackStatus(const bool enable);

    void SetStartTimestamp(uint32_t start_timestamp);

    void SetLastRtpTime(uint32_t rtp_timestamp,
                        int64_t capture_time_ms);

    void SetSSRC( const WebRtc_UWord32 ssrc);

    WebRtc_Word32 SetRemoteSSRC( const WebRtc_UWord32 ssrc);

    WebRtc_Word32 SetCameraDelay(const WebRtc_Word32 delayMS);

    WebRtc_Word32 CNAME(char cName[RTCP_CNAME_SIZE]);
    WebRtc_Word32 SetCNAME(const char cName[RTCP_CNAME_SIZE]);

    WebRtc_Word32 AddMixedCNAME(const WebRtc_UWord32 SSRC,
                                const char cName[RTCP_CNAME_SIZE]);

    WebRtc_Word32 RemoveMixedCNAME(const WebRtc_UWord32 SSRC);

    WebRtc_UWord32 SendTimeOfSendReport(const WebRtc_UWord32 sendReport);

    bool TimeToSendRTCPReport(const bool sendKeyframeBeforeRTP = false) const;

    WebRtc_UWord32 LastSendReport(WebRtc_UWord32& lastRTCPTime);

    WebRtc_Word32 SendRTCP(const WebRtc_UWord32 rtcpPacketTypeFlags,
                           const WebRtc_Word32 nackSize = 0,
                           const WebRtc_UWord16* nackList = 0,
                           const bool repeat = false,
                           const WebRtc_UWord64 pictureID = 0);

    WebRtc_Word32 AddReportBlock(const WebRtc_UWord32 SSRC,
                                 const RTCPReportBlock* receiveBlock);

    WebRtc_Word32 RemoveReportBlock(const WebRtc_UWord32 SSRC);

    /*
    *  REMB
    */
    bool REMB() const;

    WebRtc_Word32 SetREMBStatus(const bool enable);

    WebRtc_Word32 SetREMBData(const WebRtc_UWord32 bitrate,
                              const WebRtc_UWord8 numberOfSSRC,
                              const WebRtc_UWord32* SSRC);

    /*
    *   TMMBR
    */
    bool TMMBR() const;

    WebRtc_Word32 SetTMMBRStatus(const bool enable);

    WebRtc_Word32 SetTMMBN(const TMMBRSet* boundingSet,
                           const WebRtc_UWord32 maxBitrateKbit);

    /*
    *   Extended jitter report
    */
    bool IJ() const;

    WebRtc_Word32 SetIJStatus(const bool enable);

    /*
    *
    */

    WebRtc_Word32 SetApplicationSpecificData(const WebRtc_UWord8 subType,
                                             const WebRtc_UWord32 name,
                                             const WebRtc_UWord8* data,
                                             const WebRtc_UWord16 length);

    WebRtc_Word32 SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric);

    WebRtc_Word32 SetCSRCs(const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize],
                           const WebRtc_UWord8 arrLength);

    WebRtc_Word32 SetCSRCStatus(const bool include);

    void SetTargetBitrate(unsigned int target_bitrate);

private:
    WebRtc_Word32 SendToNetwork(const WebRtc_UWord8* dataBuffer,
                                const WebRtc_UWord16 length);

    void UpdatePacketRate();

    WebRtc_Word32 AddReportBlocks(WebRtc_UWord8* rtcpbuffer,
                                WebRtc_UWord32& pos,
                                WebRtc_UWord8& numberOfReportBlocks,
                                const RTCPReportBlock* received,
                                const WebRtc_UWord32 NTPsec,
                                const WebRtc_UWord32 NTPfrac);

    WebRtc_Word32 BuildSR(WebRtc_UWord8* rtcpbuffer,
                        WebRtc_UWord32& pos,
                        const WebRtc_UWord32 NTPsec,
                        const WebRtc_UWord32 NTPfrac,
                        const RTCPReportBlock* received = NULL);

    WebRtc_Word32 BuildRR(WebRtc_UWord8* rtcpbuffer,
                        WebRtc_UWord32& pos,
                        const WebRtc_UWord32 NTPsec,
                        const WebRtc_UWord32 NTPfrac,
                        const RTCPReportBlock* received = NULL);

    WebRtc_Word32 BuildExtendedJitterReport(
        WebRtc_UWord8* rtcpbuffer,
        WebRtc_UWord32& pos,
        const WebRtc_UWord32 jitterTransmissionTimeOffset);

    WebRtc_Word32 BuildSDEC(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos);
    WebRtc_Word32 BuildPLI(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos);
    WebRtc_Word32 BuildREMB(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos);
    WebRtc_Word32 BuildTMMBR(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos);
    WebRtc_Word32 BuildTMMBN(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos);
    WebRtc_Word32 BuildAPP(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos);
    WebRtc_Word32 BuildVoIPMetric(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos);
    WebRtc_Word32 BuildBYE(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos);
    WebRtc_Word32 BuildFIR(WebRtc_UWord8* rtcpbuffer,
                           WebRtc_UWord32& pos,
                           bool repeat);
    WebRtc_Word32 BuildSLI(WebRtc_UWord8* rtcpbuffer,
                         WebRtc_UWord32& pos,
                         const WebRtc_UWord8 pictureID);
    WebRtc_Word32 BuildRPSI(WebRtc_UWord8* rtcpbuffer,
                         WebRtc_UWord32& pos,
                         const WebRtc_UWord64 pictureID,
                         const WebRtc_UWord8 payloadType);

    WebRtc_Word32 BuildNACK(WebRtc_UWord8* rtcpbuffer,
                          WebRtc_UWord32& pos,
                          const WebRtc_Word32 nackSize,
                          const WebRtc_UWord16* nackList);

private:
    WebRtc_Word32            _id;
    const bool               _audio;
    RtpRtcpClock&            _clock;
    RTCPMethod               _method;

    ModuleRtpRtcpImpl&      _rtpRtcp;

    CriticalSectionWrapper* _criticalSectionTransport;
    Transport*              _cbTransport;

    CriticalSectionWrapper* _criticalSectionRTCPSender;
    bool                    _usingNack;
    bool                    _sending;
    bool                    _sendTMMBN;
    bool                    _REMB;
    bool                    _sendREMB;
    bool                    _TMMBR;
    bool                    _IJ;

    WebRtc_Word64        _nextTimeToSendRTCP;

    uint32_t start_timestamp_;
    uint32_t last_rtp_timestamp_;
    int64_t last_frame_capture_time_ms_;
    WebRtc_UWord32 _SSRC;
    WebRtc_UWord32 _remoteSSRC;  // SSRC that we receive on our RTP channel
    char _CNAME[RTCP_CNAME_SIZE];

    std::map<WebRtc_UWord32, RTCPReportBlock*> _reportBlocks;
    std::map<WebRtc_UWord32, RTCPUtility::RTCPCnameInformation*> _csrcCNAMEs;

    WebRtc_Word32         _cameraDelayMS;

    // Sent
    WebRtc_UWord32        _lastSendReport[RTCP_NUMBER_OF_SR];  // allow packet loss and RTT above 1 sec
    WebRtc_UWord32        _lastRTCPTime[RTCP_NUMBER_OF_SR];

    // send CSRCs
    WebRtc_UWord8         _CSRCs;
    WebRtc_UWord32        _CSRC[kRtpCsrcSize];
    bool                _includeCSRCs;

    // Full intra request
    WebRtc_UWord8         _sequenceNumberFIR;

    // REMB    
    WebRtc_UWord8       _lengthRembSSRC;
    WebRtc_UWord8       _sizeRembSSRC;
    WebRtc_UWord32*     _rembSSRC;
    WebRtc_UWord32      _rembBitrate;

    TMMBRHelp           _tmmbrHelp;
    WebRtc_UWord32      _tmmbr_Send;
    WebRtc_UWord32      _packetOH_Send;

    // APP
    bool                 _appSend;
    WebRtc_UWord8        _appSubType;
    WebRtc_UWord32       _appName;
    WebRtc_UWord8*       _appData;
    WebRtc_UWord16       _appLength;

    // XR VoIP metric
    bool                _xrSendVoIPMetric;
    RTCPVoIPMetric      _xrVoIPMetric;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_
