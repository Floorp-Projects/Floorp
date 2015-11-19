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

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/tmmbr_help.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ModuleRtpRtcpImpl;
class RTCPReceiver;

class NACKStringBuilder {
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

class RTCPSender {
public:
 struct FeedbackState {
   FeedbackState();

   uint8_t send_payload_type;
   uint32_t frequency_hz;
   uint32_t packets_sent;
   size_t media_bytes_sent;
   uint32_t send_bitrate;

   uint32_t last_rr_ntp_secs;
   uint32_t last_rr_ntp_frac;
   uint32_t remote_sr;

   bool has_last_xr_rr;
   RtcpReceiveTimeInfo last_xr_rr;

   // Used when generating TMMBR.
   ModuleRtpRtcpImpl* module;
 };
 RTCPSender(int32_t id,
            bool audio,
            Clock* clock,
            ReceiveStatistics* receive_statistics,
            RtcpPacketTypeCounterObserver* packet_type_counter_observer);
    virtual ~RTCPSender();

    int32_t RegisterSendTransport(Transport* outgoingTransport);

    RTCPMethod Status() const;
    void SetRTCPStatus(RTCPMethod method);

    bool Sending() const;
    int32_t SetSendingStatus(const FeedbackState& feedback_state,
                             bool enabled);  // combine the functions

    int32_t SetNackStatus(bool enable);

    void SetStartTimestamp(uint32_t start_timestamp);

    void SetLastRtpTime(uint32_t rtp_timestamp,
                        int64_t capture_time_ms);

    void SetSSRC(uint32_t ssrc);

    void SetRemoteSSRC(uint32_t ssrc);

    int32_t SetCNAME(const char cName[RTCP_CNAME_SIZE]);

    int32_t AddMixedCNAME(uint32_t SSRC, const char cName[RTCP_CNAME_SIZE]);

    int32_t RemoveMixedCNAME(uint32_t SSRC);

    bool GetSendReportMetadata(const uint32_t sendReport,
                               uint64_t *timeOfSend,
                               uint32_t *packetCount,
                               uint64_t *octetCount);

    bool SendTimeOfXrRrReport(uint32_t mid_ntp, int64_t* time_ms) const;

    bool TimeToSendRTCPReport(bool sendKeyframeBeforeRTP = false) const;

    uint32_t LastSendReport(int64_t& lastRTCPTime);

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

    void SetREMBStatus(bool enable);

    void SetREMBData(uint32_t bitrate, const std::vector<uint32_t>& ssrcs);

    /*
    *   TMMBR
    */
    bool TMMBR() const;

    void SetTMMBRStatus(bool enable);

    int32_t SetTMMBN(const TMMBRSet* boundingSet, uint32_t maxBitrateKbit);

    /*
    *   Extended jitter report
    */
    bool IJ() const;

    void SetIJStatus(bool enable);

    /*
    *
    */

    int32_t SetApplicationSpecificData(uint8_t subType,
                                       uint32_t name,
                                       const uint8_t* data,
                                       uint16_t length);

    int32_t SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric);

    void SendRtcpXrReceiverReferenceTime(bool enable);

    bool RtcpXrReceiverReferenceTime() const;

    void SetCsrcs(const std::vector<uint32_t>& csrcs);

    void SetTargetBitrate(unsigned int target_bitrate);

private:
 int32_t SendToNetwork(const uint8_t* dataBuffer, size_t length);

 int32_t WriteAllReportBlocksToBuffer(uint8_t* rtcpbuffer,
                                      int pos,
                                      uint8_t& numberOfReportBlocks,
                                      uint32_t NTPsec,
                                      uint32_t NTPfrac)
     EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);

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
                    uint32_t NTPfrac)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);

    int32_t BuildRR(uint8_t* rtcpbuffer,
                    int& pos,
                    uint32_t NTPsec,
                    uint32_t NTPfrac)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);

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

    int32_t BuildExtendedJitterReport(uint8_t* rtcpbuffer,
                                      int& pos,
                                      uint32_t jitterTransmissionTimeOffset)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);

    int32_t BuildSDEC(uint8_t* rtcpbuffer, int& pos)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildPLI(uint8_t* rtcpbuffer, int& pos)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildREMB(uint8_t* rtcpbuffer, int& pos)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildTMMBR(ModuleRtpRtcpImpl* module, uint8_t* rtcpbuffer, int& pos)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildTMMBN(uint8_t* rtcpbuffer, int& pos)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildAPP(uint8_t* rtcpbuffer, int& pos)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildVoIPMetric(uint8_t* rtcpbuffer, int& pos)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildBYE(uint8_t* rtcpbuffer, int& pos)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildFIR(uint8_t* rtcpbuffer, int& pos, bool repeat)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildSLI(uint8_t* rtcpbuffer, int& pos, uint8_t pictureID)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildRPSI(uint8_t* rtcpbuffer,
                      int& pos,
                      uint64_t pictureID,
                      uint8_t payloadType)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);

    int32_t BuildNACK(uint8_t* rtcpbuffer,
                      int& pos,
                      int32_t nackSize,
                      const uint16_t* nackList,
                      std::string* nackString)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildReceiverReferenceTime(uint8_t* buffer,
                                       int& pos,
                                       uint32_t ntp_sec,
                                       uint32_t ntp_frac)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);
    int32_t BuildDlrr(uint8_t* buffer,
                      int& pos,
                      const RtcpReceiveTimeInfo& info)
        EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPSender);

private:
    const int32_t _id;
    const bool _audio;
    Clock* const _clock;
    RTCPMethod _method GUARDED_BY(_criticalSectionRTCPSender);

    CriticalSectionWrapper* _criticalSectionTransport;
    Transport* _cbTransport GUARDED_BY(_criticalSectionTransport);

    CriticalSectionWrapper* _criticalSectionRTCPSender;
    bool _usingNack GUARDED_BY(_criticalSectionRTCPSender);
    bool _sending GUARDED_BY(_criticalSectionRTCPSender);
    bool _sendTMMBN GUARDED_BY(_criticalSectionRTCPSender);
    bool _REMB GUARDED_BY(_criticalSectionRTCPSender);
    bool _sendREMB GUARDED_BY(_criticalSectionRTCPSender);
    bool _TMMBR GUARDED_BY(_criticalSectionRTCPSender);
    bool _IJ GUARDED_BY(_criticalSectionRTCPSender);

    int64_t _nextTimeToSendRTCP GUARDED_BY(_criticalSectionRTCPSender);

    uint32_t start_timestamp_ GUARDED_BY(_criticalSectionRTCPSender);
    uint32_t last_rtp_timestamp_ GUARDED_BY(_criticalSectionRTCPSender);
    int64_t last_frame_capture_time_ms_ GUARDED_BY(_criticalSectionRTCPSender);
    uint32_t _SSRC GUARDED_BY(_criticalSectionRTCPSender);
    // SSRC that we receive on our RTP channel
    uint32_t _remoteSSRC GUARDED_BY(_criticalSectionRTCPSender);
    char _CNAME[RTCP_CNAME_SIZE] GUARDED_BY(_criticalSectionRTCPSender);

    ReceiveStatistics* receive_statistics_
        GUARDED_BY(_criticalSectionRTCPSender);
    std::map<uint32_t, RTCPReportBlock*> internal_report_blocks_
        GUARDED_BY(_criticalSectionRTCPSender);
    std::map<uint32_t, RTCPReportBlock*> external_report_blocks_
        GUARDED_BY(_criticalSectionRTCPSender);
    std::map<uint32_t, RTCPUtility::RTCPCnameInformation*> _csrcCNAMEs
        GUARDED_BY(_criticalSectionRTCPSender);

    // Sent
    uint32_t _lastSendReport[RTCP_NUMBER_OF_SR] GUARDED_BY(
        _criticalSectionRTCPSender);  // allow packet loss and RTT above 1 sec
    int64_t _lastRTCPTime[RTCP_NUMBER_OF_SR] GUARDED_BY(
        _criticalSectionRTCPSender);
    uint32_t        _lastSRPacketCount[RTCP_NUMBER_OF_SR] GUARDED_BY(
        _criticalSectionRTCPSender); 
    uint64_t        _lastSROctetCount[RTCP_NUMBER_OF_SR] GUARDED_BY(
        _criticalSectionRTCPSender);

    // Sent XR receiver reference time report.
    // <mid ntp (mid 32 bits of the 64 bits NTP timestamp), send time in ms>.
    std::map<uint32_t, int64_t> last_xr_rr_
        GUARDED_BY(_criticalSectionRTCPSender);

    // send CSRCs
    std::vector<uint32_t> csrcs_ GUARDED_BY(_criticalSectionRTCPSender);

    // Full intra request
    uint8_t _sequenceNumberFIR GUARDED_BY(_criticalSectionRTCPSender);

    // REMB
    uint32_t _rembBitrate GUARDED_BY(_criticalSectionRTCPSender);
    std::vector<uint32_t> remb_ssrcs_ GUARDED_BY(_criticalSectionRTCPSender);

    TMMBRHelp _tmmbrHelp GUARDED_BY(_criticalSectionRTCPSender);
    uint32_t _tmmbr_Send GUARDED_BY(_criticalSectionRTCPSender);
    uint32_t _packetOH_Send GUARDED_BY(_criticalSectionRTCPSender);

    // APP
    bool _appSend GUARDED_BY(_criticalSectionRTCPSender);
    uint8_t _appSubType GUARDED_BY(_criticalSectionRTCPSender);
    uint32_t _appName GUARDED_BY(_criticalSectionRTCPSender);
    uint8_t* _appData GUARDED_BY(_criticalSectionRTCPSender);
    uint16_t _appLength GUARDED_BY(_criticalSectionRTCPSender);

    // True if sending of XR Receiver reference time report is enabled.
    bool xrSendReceiverReferenceTimeEnabled_
        GUARDED_BY(_criticalSectionRTCPSender);

    // XR VoIP metric
    bool _xrSendVoIPMetric GUARDED_BY(_criticalSectionRTCPSender);
    RTCPVoIPMetric _xrVoIPMetric GUARDED_BY(_criticalSectionRTCPSender);

    RtcpPacketTypeCounterObserver* const packet_type_counter_observer_;
    RtcpPacketTypeCounter packet_type_counter_
        GUARDED_BY(_criticalSectionRTCPSender);

    RTCPUtility::NackStats nack_stats_ GUARDED_BY(_criticalSectionRTCPSender);
};
}  // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTCP_SENDER_H_
