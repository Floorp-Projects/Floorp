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
#include <sstream>
#include <string>

#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/tmmbr_help.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ModuleRtpRtcpImpl;
class RTCPReceiver;

class NACKStringBuilder
{
public:
    NACKStringBuilder();
    ~NACKStringBuilder();

    void PushNACK(uint16_t nack);
    std::string GetResult();

private:
    std::ostringstream _stream;
    int _count;
    uint16_t _prevNack;
    bool _consecutive;
};

class RTCPSender
{
public:
 struct FeedbackState {
   explicit FeedbackState(ModuleRtpRtcpImpl* module);
   FeedbackState();

   uint8_t send_payload_type;
   uint32_t frequency_hz;
   uint32_t packet_count_sent;
   uint32_t byte_count_sent;
   uint32_t send_bitrate;

   uint32_t last_rr_ntp_secs;
   uint32_t last_rr_ntp_frac;
   uint32_t remote_sr;

   bool has_last_xr_rr;
   RtcpReceiveTimeInfo last_xr_rr;

   // Used when generating TMMBR.
   ModuleRtpRtcpImpl* module;
 };
    RTCPSender(const int32_t id, const bool audio,
               Clock* clock,
               ReceiveStatistics* receive_statistics);
    virtual ~RTCPSender();

    void ChangeUniqueId(const int32_t id);

    int32_t Init();

    int32_t RegisterSendTransport(Transport* outgoingTransport);

    RTCPMethod Status() const;
    int32_t SetRTCPStatus(const RTCPMethod method);

    bool Sending() const;
    int32_t SetSendingStatus(const FeedbackState& feedback_state,
                             bool enabled);  // combine the functions

    int32_t SetNackStatus(const bool enable);

    void SetStartTimestamp(uint32_t start_timestamp);

    void SetLastRtpTime(uint32_t rtp_timestamp,
                        int64_t capture_time_ms);

    void SetSSRC( const uint32_t ssrc);

    void SetRemoteSSRC(uint32_t ssrc);

    int32_t SetCameraDelay(const int32_t delayMS);

    int32_t CNAME(char cName[RTCP_CNAME_SIZE]);
    int32_t SetCNAME(const char cName[RTCP_CNAME_SIZE]);

    int32_t AddMixedCNAME(const uint32_t SSRC,
                          const char cName[RTCP_CNAME_SIZE]);

    int32_t RemoveMixedCNAME(const uint32_t SSRC);

    bool GetSendReportMetadata(const uint32_t sendReport,
                               uint32_t *timeOfSend,
                               uint32_t *packetCount,
                               uint64_t *octetCount);

    bool SendTimeOfXrRrReport(uint32_t mid_ntp, int64_t* time_ms) const;

    bool TimeToSendRTCPReport(const bool sendKeyframeBeforeRTP = false) const;

    uint32_t LastSendReport(uint32_t& lastRTCPTime);

    int32_t SendRTCP(
        const FeedbackState& feedback_state,
        uint32_t rtcpPacketTypeFlags,
        int32_t nackSize = 0,
        const uint16_t* nackList = 0,
        bool repeat = false,
        uint64_t pictureID = 0);

    int32_t AddExternalReportBlock(
            uint32_t SSRC,
            const RTCPReportBlock* receiveBlock);

    int32_t RemoveExternalReportBlock(uint32_t SSRC);

    /*
    *  REMB
    */
    bool REMB() const;

    int32_t SetREMBStatus(const bool enable);

    int32_t SetREMBData(const uint32_t bitrate,
                        const uint8_t numberOfSSRC,
                        const uint32_t* SSRC);

    /*
    *   TMMBR
    */
    bool TMMBR() const;

    int32_t SetTMMBRStatus(const bool enable);

    int32_t SetTMMBN(const TMMBRSet* boundingSet,
                     const uint32_t maxBitrateKbit);

    /*
    *   Extended jitter report
    */
    bool IJ() const;

    int32_t SetIJStatus(const bool enable);

    /*
    *
    */

    int32_t SetApplicationSpecificData(const uint8_t subType,
                                       const uint32_t name,
                                       const uint8_t* data,
                                       const uint16_t length);

    int32_t SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric);

    void SendRtcpXrReceiverReferenceTime(bool enable);

    bool RtcpXrReceiverReferenceTime() const;

    int32_t SetCSRCs(const uint32_t arrOfCSRC[kRtpCsrcSize],
                     const uint8_t arrLength);

    int32_t SetCSRCStatus(const bool include);

    void SetTargetBitrate(unsigned int target_bitrate);

private:
    int32_t SendToNetwork(const uint8_t* dataBuffer, const uint16_t length);

    void UpdatePacketRate();

    int32_t WriteAllReportBlocksToBuffer(uint8_t* rtcpbuffer,
                            int pos,
                            uint8_t& numberOfReportBlocks,
                            const uint32_t NTPsec,
                            const uint32_t NTPfrac);

    int32_t WriteReportBlocksToBuffer(
        uint8_t* rtcpbuffer,
        int32_t position,
        const std::map<uint32_t, RTCPReportBlock*>& report_blocks);

    int32_t AddReportBlock(
        uint32_t SSRC,
        std::map<uint32_t, RTCPReportBlock*>* report_blocks,
        const RTCPReportBlock* receiveBlock);

    bool PrepareReport(const FeedbackState& feedback_state,
                       StreamStatistician* statistician,
                       RTCPReportBlock* report_block,
                       uint32_t* ntp_secs, uint32_t* ntp_frac);

    int32_t BuildSR(const FeedbackState& feedback_state,
                    uint8_t* rtcpbuffer,
                    int& pos,
                    uint32_t NTPsec,
                    uint32_t NTPfrac);

    int32_t BuildRR(uint8_t* rtcpbuffer,
                    int& pos,
                    const uint32_t NTPsec,
                    const uint32_t NTPfrac);

    int PrepareRTCP(
        const FeedbackState& feedback_state,
        uint32_t packetTypeFlags,
        int32_t nackSize,
        const uint16_t* nackList,
        bool repeat,
        uint64_t pictureID,
        uint8_t* rtcp_buffer,
        int buffer_size);

    bool ShouldSendReportBlocks(uint32_t rtcp_packet_type) const;

    int32_t BuildExtendedJitterReport(
        uint8_t* rtcpbuffer,
        int& pos,
        const uint32_t jitterTransmissionTimeOffset);

    int32_t BuildSDEC(uint8_t* rtcpbuffer, int& pos);
    int32_t BuildPLI(uint8_t* rtcpbuffer, int& pos);
    int32_t BuildREMB(uint8_t* rtcpbuffer, int& pos);
    int32_t BuildTMMBR(ModuleRtpRtcpImpl* module,
                       uint8_t* rtcpbuffer,
                       int& pos);
    int32_t BuildTMMBN(uint8_t* rtcpbuffer, int& pos);
    int32_t BuildAPP(uint8_t* rtcpbuffer, int& pos);
    int32_t BuildVoIPMetric(uint8_t* rtcpbuffer, int& pos);
    int32_t BuildBYE(uint8_t* rtcpbuffer, int& pos);
    int32_t BuildFIR(uint8_t* rtcpbuffer, int& pos, bool repeat);
    int32_t BuildSLI(uint8_t* rtcpbuffer,
                     int& pos,
                     const uint8_t pictureID);
    int32_t BuildRPSI(uint8_t* rtcpbuffer,
                      int& pos,
                      const uint64_t pictureID,
                      const uint8_t payloadType);

    int32_t BuildNACK(uint8_t* rtcpbuffer,
                      int& pos,
                      const int32_t nackSize,
                      const uint16_t* nackList,
                          std::string* nackString);

    int32_t BuildReceiverReferenceTime(uint8_t* buffer,
                                       int& pos,
                                       uint32_t ntp_sec,
                                       uint32_t ntp_frac);
    int32_t BuildDlrr(uint8_t* buffer,
                      int& pos,
                      const RtcpReceiveTimeInfo& info);

private:
    int32_t            _id;
    const bool               _audio;
    Clock*                   _clock;
    RTCPMethod               _method;

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

    int64_t        _nextTimeToSendRTCP;

    uint32_t start_timestamp_;
    uint32_t last_rtp_timestamp_;
    int64_t last_frame_capture_time_ms_;
    uint32_t _SSRC;
    uint32_t _remoteSSRC;  // SSRC that we receive on our RTP channel
    char _CNAME[RTCP_CNAME_SIZE];


    ReceiveStatistics* receive_statistics_;
    std::map<uint32_t, RTCPReportBlock*> internal_report_blocks_;
    std::map<uint32_t, RTCPReportBlock*> external_report_blocks_;
    std::map<uint32_t, RTCPUtility::RTCPCnameInformation*> _csrcCNAMEs;

    int32_t         _cameraDelayMS;

    // Sent
    uint32_t        _lastSendReport[RTCP_NUMBER_OF_SR];  // allow packet loss and RTT above 1 sec
    uint32_t        _lastRTCPTime[RTCP_NUMBER_OF_SR];
    uint32_t        _lastSRPacketCount[RTCP_NUMBER_OF_SR];
    uint64_t        _lastSROctetCount[RTCP_NUMBER_OF_SR];

    // Sent XR receiver reference time report.
    // <mid ntp (mid 32 bits of the 64 bits NTP timestamp), send time in ms>.
    std::map<uint32_t, int64_t> last_xr_rr_;

    // send CSRCs
    uint8_t         _CSRCs;
    uint32_t        _CSRC[kRtpCsrcSize];
    bool                _includeCSRCs;

    // Full intra request
    uint8_t         _sequenceNumberFIR;

    // REMB    
    uint8_t       _lengthRembSSRC;
    uint8_t       _sizeRembSSRC;
    uint32_t*     _rembSSRC;
    uint32_t      _rembBitrate;

    TMMBRHelp           _tmmbrHelp;
    uint32_t      _tmmbr_Send;
    uint32_t      _packetOH_Send;

    // APP
    bool                 _appSend;
    uint8_t        _appSubType;
    uint32_t       _appName;
    uint8_t*       _appData;
    uint16_t       _appLength;

    // True if sending of XR Receiver reference time report is enabled.
    bool xrSendReceiverReferenceTimeEnabled_;

    // XR VoIP metric
    bool                _xrSendVoIPMetric;
    RTCPVoIPMetric      _xrVoIPMetric;

    // Counters
    uint32_t      _nackCount;
    uint32_t      _pliCount;
    uint32_t      _fullIntraRequestCount;
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_
