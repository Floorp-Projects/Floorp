/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_receiver.h"

#include <assert.h> //assert
#include <string.h> //memset

#include <algorithm>

#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {
using namespace RTCPUtility;
using namespace RTCPHelp;

// The number of RTCP time intervals needed to trigger a timeout.
const int kRrTimeoutIntervals = 3;

RTCPReceiver::RTCPReceiver(const int32_t id, Clock* clock,
                           ModuleRtpRtcpImpl* owner)
    : TMMBRHelp(),
    _id(id),
    _clock(clock),
    _method(kRtcpOff),
    _lastReceived(0),
    _rtpRtcp(*owner),
      _criticalSectionFeedbacks(
          CriticalSectionWrapper::CreateCriticalSection()),
    _cbRtcpFeedback(NULL),
    _cbRtcpBandwidthObserver(NULL),
    _cbRtcpIntraFrameObserver(NULL),
    _criticalSectionRTCPReceiver(
        CriticalSectionWrapper::CreateCriticalSection()),
    main_ssrc_(0),
    _remoteSSRC(0),
    _remoteSenderInfo(),
    _lastReceivedSRNTPsecs(0),
    _lastReceivedSRNTPfrac(0),
    _lastReceivedXRNTPsecs(0),
    _lastReceivedXRNTPfrac(0),
    xr_rr_rtt_ms_(0),
    _receivedInfoMap(),
    _packetTimeOutMS(0),
    _lastReceivedRrMs(0),
    _lastIncreasedSequenceNumberMs(0),
    stats_callback_(NULL) {
    memset(&_remoteSenderInfo, 0, sizeof(_remoteSenderInfo));
    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

RTCPReceiver::~RTCPReceiver() {
  delete _criticalSectionRTCPReceiver;
  delete _criticalSectionFeedbacks;

  while (!_receivedReportBlockMap.empty()) {
    std::map<uint32_t, RTCPReportBlockInformation*>::iterator first =
        _receivedReportBlockMap.begin();
    delete first->second;
    _receivedReportBlockMap.erase(first);
  }
  while (!_receivedInfoMap.empty()) {
    std::map<uint32_t, RTCPReceiveInformation*>::iterator first =
        _receivedInfoMap.begin();
    delete first->second;
    _receivedInfoMap.erase(first);
  }
  while (!_receivedCnameMap.empty()) {
    std::map<uint32_t, RTCPCnameInformation*>::iterator first =
        _receivedCnameMap.begin();
    delete first->second;
    _receivedCnameMap.erase(first);
  }
  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, _id,
               "%s deleted", __FUNCTION__);
}

void
RTCPReceiver::ChangeUniqueId(const int32_t id)
{
    _id = id;
}

RTCPMethod
RTCPReceiver::Status() const
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    return _method;
}

int32_t
RTCPReceiver::SetRTCPStatus(const RTCPMethod method)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    _method = method;
    return 0;
}

int64_t
RTCPReceiver::LastReceived()
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    return _lastReceived;
}

int64_t
RTCPReceiver::LastReceivedReceiverReport() const {
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    int64_t last_received_rr = -1;
    for (ReceivedInfoMap::const_iterator it = _receivedInfoMap.begin();
         it != _receivedInfoMap.end(); ++it) {
      if (it->second->lastTimeReceived > last_received_rr) {
        last_received_rr = it->second->lastTimeReceived;
      }
    }
    return last_received_rr;
}

int32_t
RTCPReceiver::SetRemoteSSRC( const uint32_t ssrc)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    // new SSRC reset old reports
    memset(&_remoteSenderInfo, 0, sizeof(_remoteSenderInfo));
    _lastReceivedSRNTPsecs = 0;
    _lastReceivedSRNTPfrac = 0;

    _remoteSSRC = ssrc;
    return 0;
}

uint32_t RTCPReceiver::RemoteSSRC() const {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
  return _remoteSSRC;
}

void RTCPReceiver::RegisterRtcpObservers(
    RtcpIntraFrameObserver* intra_frame_callback,
    RtcpBandwidthObserver* bandwidth_callback,
    RtcpFeedback* feedback_callback) {
  CriticalSectionScoped lock(_criticalSectionFeedbacks);
  _cbRtcpIntraFrameObserver = intra_frame_callback;
  _cbRtcpBandwidthObserver = bandwidth_callback;
  _cbRtcpFeedback = feedback_callback;
}

void RTCPReceiver::SetSsrcs(uint32_t main_ssrc,
                            const std::set<uint32_t>& registered_ssrcs) {
  uint32_t old_ssrc = 0;
  {
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    old_ssrc = main_ssrc_;
    main_ssrc_ = main_ssrc;
    registered_ssrcs_ = registered_ssrcs;
  }
  {
    CriticalSectionScoped lock(_criticalSectionFeedbacks);
    if (_cbRtcpIntraFrameObserver && old_ssrc != main_ssrc) {
      _cbRtcpIntraFrameObserver->OnLocalSsrcChanged(old_ssrc, main_ssrc);
    }
  }
}

int32_t RTCPReceiver::ResetRTT(const uint32_t remoteSSRC) {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
  RTCPReportBlockInformation* reportBlock =
      GetReportBlockInformation(remoteSSRC);
  if (reportBlock == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "\tfailed to GetReportBlockInformation(%u)", remoteSSRC);
    return -1;
  }
  reportBlock->RTT = 0;
  reportBlock->avgRTT = 0;
  reportBlock->minRTT = 0;
  reportBlock->maxRTT = 0;
  return 0;
}

int32_t RTCPReceiver::RTT(uint32_t remoteSSRC,
                          uint16_t* RTT,
                          uint16_t* avgRTT,
                          uint16_t* minRTT,
                          uint16_t* maxRTT) const {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  RTCPReportBlockInformation* reportBlock =
      GetReportBlockInformation(remoteSSRC);

  if (reportBlock == NULL) {
    return -1;
  }
  if (RTT) {
    *RTT = reportBlock->RTT;
  }
  if (avgRTT) {
    *avgRTT = reportBlock->avgRTT;
  }
  if (minRTT) {
    *minRTT = reportBlock->minRTT;
  }
  if (maxRTT) {
    *maxRTT = reportBlock->maxRTT;
  }
  return 0;
}

bool RTCPReceiver::GetAndResetXrRrRtt(uint16_t* rtt_ms) {
  assert(rtt_ms);
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
  if (xr_rr_rtt_ms_ == 0) {
    return false;
  }
  *rtt_ms = xr_rr_rtt_ms_;
  xr_rr_rtt_ms_ = 0;
  return true;
}

int32_t
RTCPReceiver::NTP(uint32_t *ReceivedNTPsecs,
                  uint32_t *ReceivedNTPfrac,
                  uint32_t *RTCPArrivalTimeSecs,
                  uint32_t *RTCPArrivalTimeFrac,
                  uint32_t *rtcp_timestamp) const
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    if(ReceivedNTPsecs)
    {
        *ReceivedNTPsecs = _remoteSenderInfo.NTPseconds; // NTP from incoming SendReport
    }
    if(ReceivedNTPfrac)
    {
        *ReceivedNTPfrac = _remoteSenderInfo.NTPfraction;
    }
    if(RTCPArrivalTimeFrac)
    {
        *RTCPArrivalTimeFrac = _lastReceivedSRNTPfrac; // local NTP time when we received a RTCP packet with a send block
    }
    if(RTCPArrivalTimeSecs)
    {
        *RTCPArrivalTimeSecs = _lastReceivedSRNTPsecs;
    }
    if (rtcp_timestamp) {
      *rtcp_timestamp = _remoteSenderInfo.RTPtimeStamp;
    }
    return 0;
}

bool RTCPReceiver::LastReceivedXrReferenceTimeInfo(
    RtcpReceiveTimeInfo* info) const {
  assert(info);
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
  if (_lastReceivedXRNTPsecs == 0 && _lastReceivedXRNTPfrac == 0) {
    return false;
  }

  info->sourceSSRC = _remoteXRReceiveTimeInfo.sourceSSRC;
  info->lastRR = _remoteXRReceiveTimeInfo.lastRR;

  // Get the delay since last received report (RFC 3611).
  uint32_t receive_time = RTCPUtility::MidNtp(_lastReceivedXRNTPsecs,
                                              _lastReceivedXRNTPfrac);

  uint32_t ntp_sec = 0;
  uint32_t ntp_frac = 0;
  _clock->CurrentNtp(ntp_sec, ntp_frac);
  uint32_t now = RTCPUtility::MidNtp(ntp_sec, ntp_frac);

  info->delaySinceLastRR = now - receive_time;
  return true;
}

int32_t
RTCPReceiver::SenderInfoReceived(RTCPSenderInfo* senderInfo) const
{
    if(senderInfo == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -1;
    }
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    if(_lastReceivedSRNTPsecs == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "%s No received SR", __FUNCTION__);
        return -1;
    }
    memcpy(senderInfo, &(_remoteSenderInfo), sizeof(RTCPSenderInfo));
    return 0;
}

// statistics
// we can get multiple receive reports when we receive the report from a CE
int32_t RTCPReceiver::StatisticsReceived(
    std::vector<RTCPReportBlock>* receiveBlocks) const {
  assert(receiveBlocks);
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPReportBlockInformation*>::const_iterator it =
      _receivedReportBlockMap.begin();

  while (it != _receivedReportBlockMap.end()) {
    receiveBlocks->push_back(it->second->remoteReceiveBlock);
    it++;
  }
  return 0;
}

int32_t
RTCPReceiver::IncomingRTCPPacket(RTCPPacketInformation& rtcpPacketInformation,
                                 RTCPUtility::RTCPParserV2* rtcpParser)
{
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    _lastReceived = _clock->TimeInMilliseconds();

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser->Begin();
    while (pktType != RTCPUtility::kRtcpNotValidCode)
    {
        // Each "case" is responsible for iterate the parser to the
        // next top level packet.
        switch (pktType)
        {
        case RTCPUtility::kRtcpSrCode:
        case RTCPUtility::kRtcpRrCode:
            HandleSenderReceiverReport(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpSdesCode:
            HandleSDES(*rtcpParser);
            break;
        case RTCPUtility::kRtcpXrHeaderCode:
            HandleXrHeader(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpXrReceiverReferenceTimeCode:
            HandleXrReceiveReferenceTime(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpXrDlrrReportBlockCode:
            HandleXrDlrrReportBlock(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpXrVoipMetricCode:
            HandleXRVOIPMetric(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpByeCode:
            HandleBYE(*rtcpParser);
            break;
        case RTCPUtility::kRtcpRtpfbNackCode:
            HandleNACK(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpRtpfbTmmbrCode:
            HandleTMMBR(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpRtpfbTmmbnCode:
            HandleTMMBN(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpRtpfbSrReqCode:
            HandleSR_REQ(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpPsfbPliCode:
            HandlePLI(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpPsfbSliCode:
            HandleSLI(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpPsfbRpsiCode:
            HandleRPSI(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpExtendedIjCode:
            HandleIJ(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpPsfbFirCode:
            HandleFIR(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpPsfbAppCode:
            HandlePsfbApp(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpAppCode:
            // generic application messages
            HandleAPP(*rtcpParser, rtcpPacketInformation);
            break;
        case RTCPUtility::kRtcpAppItemCode:
            // generic application messages
            HandleAPPItem(*rtcpParser, rtcpPacketInformation);
            break;
        default:
            rtcpParser->Iterate();
            break;
        }
        pktType = rtcpParser->PacketType();
    }
    return 0;
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSenderReceiverReport(RTCPUtility::RTCPParserV2& rtcpParser,
                                         RTCPPacketInformation& rtcpPacketInformation)
{
    RTCPUtility::RTCPPacketTypes rtcpPacketType = rtcpParser.PacketType();
    const RTCPUtility::RTCPPacket& rtcpPacket   = rtcpParser.Packet();

    assert((rtcpPacketType == RTCPUtility::kRtcpRrCode) || (rtcpPacketType == RTCPUtility::kRtcpSrCode));

    // SR.SenderSSRC
    // The synchronization source identifier for the originator of this SR packet

    // rtcpPacket.RR.SenderSSRC
    // The source of the packet sender, same as of SR? or is this a CE?

    const uint32_t remoteSSRC = (rtcpPacketType == RTCPUtility::kRtcpRrCode) ? rtcpPacket.RR.SenderSSRC:rtcpPacket.SR.SenderSSRC;
    const uint8_t  numberOfReportBlocks = (rtcpPacketType == RTCPUtility::kRtcpRrCode) ? rtcpPacket.RR.NumberOfReportBlocks:rtcpPacket.SR.NumberOfReportBlocks;

    rtcpPacketInformation.remoteSSRC = remoteSSRC;

    RTCPReceiveInformation* ptrReceiveInfo = CreateReceiveInformation(remoteSSRC);
    if (!ptrReceiveInfo)
    {
        rtcpParser.Iterate();
        return;
    }

    if (rtcpPacketType == RTCPUtility::kRtcpSrCode)
    {
        TRACE_EVENT_INSTANT2("webrtc_rtp", "SR",
                             "remote_ssrc", remoteSSRC,
                             "ssrc", main_ssrc_);

        if (_remoteSSRC == remoteSSRC) // have I received RTP packets from this party
        {
            // only signal that we have received a SR when we accept one
            rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpSr;

            rtcpPacketInformation.ntp_secs = rtcpPacket.SR.NTPMostSignificant;
            rtcpPacketInformation.ntp_frac = rtcpPacket.SR.NTPLeastSignificant;
            rtcpPacketInformation.rtp_timestamp = rtcpPacket.SR.RTPTimestamp;

            // We will only store the send report from one source, but
            // we will store all the receive block

            // Save the NTP time of this report
            _remoteSenderInfo.NTPseconds = rtcpPacket.SR.NTPMostSignificant;
            _remoteSenderInfo.NTPfraction = rtcpPacket.SR.NTPLeastSignificant;
            _remoteSenderInfo.RTPtimeStamp = rtcpPacket.SR.RTPTimestamp;
            _remoteSenderInfo.sendPacketCount = rtcpPacket.SR.SenderPacketCount;
            _remoteSenderInfo.sendOctetCount = rtcpPacket.SR.SenderOctetCount;

            _clock->CurrentNtp(_lastReceivedSRNTPsecs, _lastReceivedSRNTPfrac);
        }
        else
        {
            rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRr;
        }
    } else
    {
        TRACE_EVENT_INSTANT2("webrtc_rtp", "RR",
                             "remote_ssrc", remoteSSRC,
                             "ssrc", main_ssrc_);

        rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRr;
    }
    UpdateReceiveInformation(*ptrReceiveInfo);

    rtcpPacketType = rtcpParser.Iterate();

    while (rtcpPacketType == RTCPUtility::kRtcpReportBlockItemCode)
    {
        HandleReportBlock(rtcpPacket, rtcpPacketInformation, remoteSSRC, numberOfReportBlocks);
        rtcpPacketType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void RTCPReceiver::HandleReportBlock(
    const RTCPUtility::RTCPPacket& rtcpPacket,
    RTCPPacketInformation& rtcpPacketInformation,
    const uint32_t remoteSSRC,
    const uint8_t numberOfReportBlocks)
    EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPReceiver) {
  // This will be called once per report block in the RTCP packet.
  // We filter out all report blocks that are not for us.
  // Each packet has max 31 RR blocks.
  //
  // We can calc RTT if we send a send report and get a report block back.

  // |rtcpPacket.ReportBlockItem.SSRC| is the SSRC identifier of the source to
  // which the information in this reception report block pertains.

  // Filter out all report blocks that are not for us.
  if (registered_ssrcs_.find(rtcpPacket.ReportBlockItem.SSRC) ==
      registered_ssrcs_.end()) {
    // This block is not for us ignore it.
    return;
  }

  // To avoid problem with acquiring _criticalSectionRTCPSender while holding
  // _criticalSectionRTCPReceiver.
  _criticalSectionRTCPReceiver->Leave();
  uint32_t sendTimeMS =
      _rtpRtcp.SendTimeOfSendReport(rtcpPacket.ReportBlockItem.LastSR);
  _criticalSectionRTCPReceiver->Enter();

  RTCPReportBlockInformation* reportBlock =
      CreateReportBlockInformation(remoteSSRC);
  if (reportBlock == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "\tfailed to CreateReportBlockInformation(%u)", remoteSSRC);
    return;
  }

  _lastReceivedRrMs = _clock->TimeInMilliseconds();
  const RTCPPacketReportBlockItem& rb = rtcpPacket.ReportBlockItem;
  reportBlock->remoteReceiveBlock.remoteSSRC = remoteSSRC;
  reportBlock->remoteReceiveBlock.sourceSSRC = rb.SSRC;
  reportBlock->remoteReceiveBlock.fractionLost = rb.FractionLost;
  reportBlock->remoteReceiveBlock.cumulativeLost =
      rb.CumulativeNumOfPacketsLost;
  if (rb.ExtendedHighestSequenceNumber >
      reportBlock->remoteReceiveBlock.extendedHighSeqNum) {
    // We have successfully delivered new RTP packets to the remote side after
    // the last RR was sent from the remote side.
    _lastIncreasedSequenceNumberMs = _lastReceivedRrMs;
  }
  reportBlock->remoteReceiveBlock.extendedHighSeqNum =
      rb.ExtendedHighestSequenceNumber;
  reportBlock->remoteReceiveBlock.jitter = rb.Jitter;
  reportBlock->remoteReceiveBlock.delaySinceLastSR = rb.DelayLastSR;
  reportBlock->remoteReceiveBlock.lastSR = rb.LastSR;

  if (rtcpPacket.ReportBlockItem.Jitter > reportBlock->remoteMaxJitter) {
    reportBlock->remoteMaxJitter = rtcpPacket.ReportBlockItem.Jitter;
  }

  uint32_t delaySinceLastSendReport =
      rtcpPacket.ReportBlockItem.DelayLastSR;

  // local NTP time when we received this
  uint32_t lastReceivedRRNTPsecs = 0;
  uint32_t lastReceivedRRNTPfrac = 0;

  _clock->CurrentNtp(lastReceivedRRNTPsecs, lastReceivedRRNTPfrac);

  // time when we received this in MS
  uint32_t receiveTimeMS = Clock::NtpToMs(lastReceivedRRNTPsecs,
                                          lastReceivedRRNTPfrac);

  // Estimate RTT
  uint32_t d = (delaySinceLastSendReport & 0x0000ffff) * 1000;
  d /= 65536;
  d += ((delaySinceLastSendReport & 0xffff0000) >> 16) * 1000;

  int32_t RTT = 0;

  if (sendTimeMS > 0) {
    RTT = receiveTimeMS - d - sendTimeMS;
    if (RTT <= 0) {
      RTT = 1;
    }
    if (RTT > reportBlock->maxRTT) {
      // store max RTT
      reportBlock->maxRTT = (uint16_t) RTT;
    }
    if (reportBlock->minRTT == 0) {
      // first RTT
      reportBlock->minRTT = (uint16_t) RTT;
    } else if (RTT < reportBlock->minRTT) {
      // Store min RTT
      reportBlock->minRTT = (uint16_t) RTT;
    }
    // store last RTT
    reportBlock->RTT = (uint16_t) RTT;

    // store average RTT
    if (reportBlock->numAverageCalcs != 0) {
      float ac = static_cast<float> (reportBlock->numAverageCalcs);
      float newAverage = ((ac / (ac + 1)) * reportBlock->avgRTT)
          + ((1 / (ac + 1)) * RTT);
      reportBlock->avgRTT = static_cast<int> (newAverage + 0.5f);
    } else {
      // first RTT
      reportBlock->avgRTT = (uint16_t) RTT;
    }
    reportBlock->numAverageCalcs++;
  }

  TRACE_COUNTER_ID1("webrtc_rtp", "RR_RTT", rb.SSRC, RTT);

  rtcpPacketInformation.AddReportInfo(*reportBlock);
}

RTCPReportBlockInformation*
RTCPReceiver::CreateReportBlockInformation(uint32_t remoteSSRC) {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPReportBlockInformation*>::iterator it =
      _receivedReportBlockMap.find(remoteSSRC);

  RTCPReportBlockInformation* ptrReportBlockInfo = NULL;
  if (it != _receivedReportBlockMap.end()) {
    ptrReportBlockInfo = it->second;
  } else {
    ptrReportBlockInfo = new RTCPReportBlockInformation;
    _receivedReportBlockMap[remoteSSRC] = ptrReportBlockInfo;
  }
  return ptrReportBlockInfo;
}

RTCPReportBlockInformation*
RTCPReceiver::GetReportBlockInformation(uint32_t remoteSSRC) const {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPReportBlockInformation*>::const_iterator it =
      _receivedReportBlockMap.find(remoteSSRC);

  if (it == _receivedReportBlockMap.end()) {
    return NULL;
  }
  return it->second;
}

RTCPCnameInformation*
RTCPReceiver::CreateCnameInformation(uint32_t remoteSSRC) {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPCnameInformation*>::iterator it =
      _receivedCnameMap.find(remoteSSRC);

  if (it != _receivedCnameMap.end()) {
    return it->second;
  }
  RTCPCnameInformation* cnameInfo = new RTCPCnameInformation;
  memset(cnameInfo->name, 0, RTCP_CNAME_SIZE);
  _receivedCnameMap[remoteSSRC] = cnameInfo;
  return cnameInfo;
}

RTCPCnameInformation*
RTCPReceiver::GetCnameInformation(uint32_t remoteSSRC) const {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPCnameInformation*>::const_iterator it =
      _receivedCnameMap.find(remoteSSRC);

  if (it == _receivedCnameMap.end()) {
    return NULL;
  }
  return it->second;
}

RTCPReceiveInformation*
RTCPReceiver::CreateReceiveInformation(uint32_t remoteSSRC) {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPReceiveInformation*>::iterator it =
      _receivedInfoMap.find(remoteSSRC);

  if (it != _receivedInfoMap.end()) {
    return it->second;
  }
  RTCPReceiveInformation* receiveInfo = new RTCPReceiveInformation;
  _receivedInfoMap[remoteSSRC] = receiveInfo;
  return receiveInfo;
}

RTCPReceiveInformation*
RTCPReceiver::GetReceiveInformation(uint32_t remoteSSRC) {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPReceiveInformation*>::iterator it =
      _receivedInfoMap.find(remoteSSRC);
  if (it == _receivedInfoMap.end()) {
    return NULL;
  }
  return it->second;
}

void RTCPReceiver::UpdateReceiveInformation(
    RTCPReceiveInformation& receiveInformation) {
  // Update that this remote is alive
  receiveInformation.lastTimeReceived = _clock->TimeInMilliseconds();
}

bool RTCPReceiver::RtcpRrTimeout(int64_t rtcp_interval_ms) {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
  if (_lastReceivedRrMs == 0)
    return false;

  int64_t time_out_ms = kRrTimeoutIntervals * rtcp_interval_ms;
  if (_clock->TimeInMilliseconds() > _lastReceivedRrMs + time_out_ms) {
    // Reset the timer to only trigger one log.
    _lastReceivedRrMs = 0;
    return true;
  }
  return false;
}

bool RTCPReceiver::RtcpRrSequenceNumberTimeout(int64_t rtcp_interval_ms) {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
  if (_lastIncreasedSequenceNumberMs == 0)
    return false;

  int64_t time_out_ms = kRrTimeoutIntervals * rtcp_interval_ms;
  if (_clock->TimeInMilliseconds() > _lastIncreasedSequenceNumberMs +
      time_out_ms) {
    // Reset the timer to only trigger one log.
    _lastIncreasedSequenceNumberMs = 0;
    return true;
  }
  return false;
}

bool RTCPReceiver::UpdateRTCPReceiveInformationTimers() {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  bool updateBoundingSet = false;
  int64_t timeNow = _clock->TimeInMilliseconds();

  std::map<uint32_t, RTCPReceiveInformation*>::iterator receiveInfoIt =
      _receivedInfoMap.begin();

  while (receiveInfoIt != _receivedInfoMap.end()) {
    RTCPReceiveInformation* receiveInfo = receiveInfoIt->second;
    if (receiveInfo == NULL) {
      return updateBoundingSet;
    }
    // time since last received rtcp packet
    // when we dont have a lastTimeReceived and the object is marked
    // readyForDelete it's removed from the map
    if (receiveInfo->lastTimeReceived) {
      /// use audio define since we don't know what interval the remote peer is
      // using
      if ((timeNow - receiveInfo->lastTimeReceived) >
          5 * RTCP_INTERVAL_AUDIO_MS) {
        // no rtcp packet for the last five regular intervals, reset limitations
        receiveInfo->TmmbrSet.clearSet();
        // prevent that we call this over and over again
        receiveInfo->lastTimeReceived = 0;
        // send new TMMBN to all channels using the default codec
        updateBoundingSet = true;
      }
      receiveInfoIt++;
    } else if (receiveInfo->readyForDelete) {
      // store our current receiveInfoItem
      std::map<uint32_t, RTCPReceiveInformation*>::iterator
      receiveInfoItemToBeErased = receiveInfoIt;
      receiveInfoIt++;
      delete receiveInfoItemToBeErased->second;
      _receivedInfoMap.erase(receiveInfoItemToBeErased);
    } else {
      receiveInfoIt++;
    }
  }
  return updateBoundingSet;
}

int32_t RTCPReceiver::BoundingSet(bool &tmmbrOwner, TMMBRSet* boundingSetRec) {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPReceiveInformation*>::iterator receiveInfoIt =
      _receivedInfoMap.find(_remoteSSRC);

  if (receiveInfoIt == _receivedInfoMap.end()) {
    return -1;
  }
  RTCPReceiveInformation* receiveInfo = receiveInfoIt->second;
  if (receiveInfo == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s failed to get RTCPReceiveInformation",
                 __FUNCTION__);
    return -1;
  }
  if (receiveInfo->TmmbnBoundingSet.lengthOfSet() > 0) {
    boundingSetRec->VerifyAndAllocateSet(
        receiveInfo->TmmbnBoundingSet.lengthOfSet() + 1);
    for(uint32_t i=0; i< receiveInfo->TmmbnBoundingSet.lengthOfSet();
        i++) {
      if(receiveInfo->TmmbnBoundingSet.Ssrc(i) == main_ssrc_) {
        // owner of bounding set
        tmmbrOwner = true;
      }
      boundingSetRec->SetEntry(i,
                               receiveInfo->TmmbnBoundingSet.Tmmbr(i),
                               receiveInfo->TmmbnBoundingSet.PacketOH(i),
                               receiveInfo->TmmbnBoundingSet.Ssrc(i));
    }
  }
  return receiveInfo->TmmbnBoundingSet.lengthOfSet();
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSDES(RTCPUtility::RTCPParserV2& rtcpParser)
{
    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpSdesChunkCode)
    {
        HandleSDESChunk(rtcpParser);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void RTCPReceiver::HandleSDESChunk(RTCPUtility::RTCPParserV2& rtcpParser) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
  RTCPCnameInformation* cnameInfo =
      CreateCnameInformation(rtcpPacket.CName.SenderSSRC);
  assert(cnameInfo);

  cnameInfo->name[RTCP_CNAME_SIZE - 1] = 0;
  strncpy(cnameInfo->name, rtcpPacket.CName.CName, RTCP_CNAME_SIZE - 1);
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleNACK(RTCPUtility::RTCPParserV2& rtcpParser,
                         RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
    if (main_ssrc_ != rtcpPacket.NACK.MediaSSRC)
    {
        // Not to us.
        rtcpParser.Iterate();
        return;
    }
    rtcpPacketInformation.ResetNACKPacketIdArray();

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpRtpfbNackItemCode)
    {
        HandleNACKItem(rtcpPacket, rtcpPacketInformation);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleNACKItem(const RTCPUtility::RTCPPacket& rtcpPacket,
                             RTCPPacketInformation& rtcpPacketInformation)
{
    rtcpPacketInformation.AddNACKPacket(rtcpPacket.NACKItem.PacketID);

    uint16_t bitMask = rtcpPacket.NACKItem.BitMask;
    if(bitMask)
    {
        for(int i=1; i <= 16; ++i)
        {
            if(bitMask & 0x01)
            {
                rtcpPacketInformation.AddNACKPacket(rtcpPacket.NACKItem.PacketID + i);
            }
            bitMask = bitMask >>1;
        }
    }

    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpNack;
}

// no need for critsect we have _criticalSectionRTCPReceiver
void RTCPReceiver::HandleBYE(RTCPUtility::RTCPParserV2& rtcpParser) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

  // clear our lists
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
  std::map<uint32_t, RTCPReportBlockInformation*>::iterator
      reportBlockInfoIt = _receivedReportBlockMap.find(
          rtcpPacket.BYE.SenderSSRC);

  if (reportBlockInfoIt != _receivedReportBlockMap.end()) {
    delete reportBlockInfoIt->second;
    _receivedReportBlockMap.erase(reportBlockInfoIt);
  }
  //  we can't delete it due to TMMBR
  std::map<uint32_t, RTCPReceiveInformation*>::iterator receiveInfoIt =
      _receivedInfoMap.find(rtcpPacket.BYE.SenderSSRC);

  if (receiveInfoIt != _receivedInfoMap.end()) {
    receiveInfoIt->second->readyForDelete = true;
  }

  std::map<uint32_t, RTCPCnameInformation*>::iterator cnameInfoIt =
      _receivedCnameMap.find(rtcpPacket.BYE.SenderSSRC);

  if (cnameInfoIt != _receivedCnameMap.end()) {
    delete cnameInfoIt->second;
    _receivedCnameMap.erase(cnameInfoIt);
  }
  xr_rr_rtt_ms_ = 0;
  rtcpParser.Iterate();
}

void RTCPReceiver::HandleXrHeader(
    RTCPUtility::RTCPParserV2& parser,
    RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& packet = parser.Packet();

  rtcpPacketInformation.xr_originator_ssrc = packet.XR.OriginatorSSRC;

  parser.Iterate();
}

void RTCPReceiver::HandleXrReceiveReferenceTime(
    RTCPUtility::RTCPParserV2& parser,
    RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& packet = parser.Packet();

  _remoteXRReceiveTimeInfo.sourceSSRC =
      rtcpPacketInformation.xr_originator_ssrc;

  _remoteXRReceiveTimeInfo.lastRR = RTCPUtility::MidNtp(
      packet.XRReceiverReferenceTimeItem.NTPMostSignificant,
      packet.XRReceiverReferenceTimeItem.NTPLeastSignificant);

  _clock->CurrentNtp(_lastReceivedXRNTPsecs, _lastReceivedXRNTPfrac);

  rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpXrReceiverReferenceTime;

  parser.Iterate();
}

void RTCPReceiver::HandleXrDlrrReportBlock(
    RTCPUtility::RTCPParserV2& parser,
    RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& packet = parser.Packet();
  // Iterate through sub-block(s), if any.
  RTCPUtility::RTCPPacketTypes packet_type = parser.Iterate();

  while (packet_type == RTCPUtility::kRtcpXrDlrrReportBlockItemCode) {
    HandleXrDlrrReportBlockItem(packet, rtcpPacketInformation);
    packet_type = parser.Iterate();
  }
}

void RTCPReceiver::HandleXrDlrrReportBlockItem(
    const RTCPUtility::RTCPPacket& packet,
    RTCPPacketInformation& rtcpPacketInformation)
    EXCLUSIVE_LOCKS_REQUIRED(_criticalSectionRTCPReceiver) {
  if (registered_ssrcs_.find(packet.XRDLRRReportBlockItem.SSRC) ==
      registered_ssrcs_.end()) {
    // Not to us.
    return;
  }

  rtcpPacketInformation.xr_dlrr_item = true;

  // To avoid problem with acquiring _criticalSectionRTCPSender while holding
  // _criticalSectionRTCPReceiver.
  _criticalSectionRTCPReceiver->Leave();

  int64_t send_time_ms;
  bool found = _rtpRtcp.SendTimeOfXrRrReport(
      packet.XRDLRRReportBlockItem.LastRR, &send_time_ms);

  _criticalSectionRTCPReceiver->Enter();

  if (!found) {
    return;
  }

  // The DelayLastRR field is in units of 1/65536 sec.
  uint32_t delay_rr_ms =
      (((packet.XRDLRRReportBlockItem.DelayLastRR & 0x0000ffff) * 1000) >> 16) +
      (((packet.XRDLRRReportBlockItem.DelayLastRR & 0xffff0000) >> 16) * 1000);

  int32_t rtt = _clock->CurrentNtpInMilliseconds() - delay_rr_ms - send_time_ms;

  xr_rr_rtt_ms_ = static_cast<uint16_t>(std::max(rtt, 1));

  rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpXrDlrrReportBlock;
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleXRVOIPMetric(RTCPUtility::RTCPParserV2& rtcpParser,
                                 RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

    if(rtcpPacket.XRVOIPMetricItem.SSRC == main_ssrc_)
    {
        // Store VoIP metrics block if it's about me
        // from OriginatorSSRC do we filter it?
        // rtcpPacket.XR.OriginatorSSRC;

        RTCPVoIPMetric receivedVoIPMetrics;
        receivedVoIPMetrics.burstDensity = rtcpPacket.XRVOIPMetricItem.burstDensity;
        receivedVoIPMetrics.burstDuration = rtcpPacket.XRVOIPMetricItem.burstDuration;
        receivedVoIPMetrics.discardRate = rtcpPacket.XRVOIPMetricItem.discardRate;
        receivedVoIPMetrics.endSystemDelay = rtcpPacket.XRVOIPMetricItem.endSystemDelay;
        receivedVoIPMetrics.extRfactor = rtcpPacket.XRVOIPMetricItem.extRfactor;
        receivedVoIPMetrics.gapDensity = rtcpPacket.XRVOIPMetricItem.gapDensity;
        receivedVoIPMetrics.gapDuration = rtcpPacket.XRVOIPMetricItem.gapDuration;
        receivedVoIPMetrics.Gmin = rtcpPacket.XRVOIPMetricItem.Gmin;
        receivedVoIPMetrics.JBabsMax = rtcpPacket.XRVOIPMetricItem.JBabsMax;
        receivedVoIPMetrics.JBmax = rtcpPacket.XRVOIPMetricItem.JBmax;
        receivedVoIPMetrics.JBnominal = rtcpPacket.XRVOIPMetricItem.JBnominal;
        receivedVoIPMetrics.lossRate = rtcpPacket.XRVOIPMetricItem.lossRate;
        receivedVoIPMetrics.MOSCQ = rtcpPacket.XRVOIPMetricItem.MOSCQ;
        receivedVoIPMetrics.MOSLQ = rtcpPacket.XRVOIPMetricItem.MOSLQ;
        receivedVoIPMetrics.noiseLevel = rtcpPacket.XRVOIPMetricItem.noiseLevel;
        receivedVoIPMetrics.RERL = rtcpPacket.XRVOIPMetricItem.RERL;
        receivedVoIPMetrics.Rfactor = rtcpPacket.XRVOIPMetricItem.Rfactor;
        receivedVoIPMetrics.roundTripDelay = rtcpPacket.XRVOIPMetricItem.roundTripDelay;
        receivedVoIPMetrics.RXconfig = rtcpPacket.XRVOIPMetricItem.RXconfig;
        receivedVoIPMetrics.signalLevel = rtcpPacket.XRVOIPMetricItem.signalLevel;

        rtcpPacketInformation.AddVoIPMetric(&receivedVoIPMetrics);

        rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpXrVoipMetric; // received signal
    }
    rtcpParser.Iterate();
}

// no need for critsect we have _criticalSectionRTCPReceiver
void RTCPReceiver::HandlePLI(RTCPUtility::RTCPParserV2& rtcpParser,
                             RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
  if (main_ssrc_ == rtcpPacket.PLI.MediaSSRC) {
    TRACE_EVENT_INSTANT0("webrtc_rtp", "PLI");

    // Received a signal that we need to send a new key frame.
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpPli;
  }
  rtcpParser.Iterate();
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleTMMBR(RTCPUtility::RTCPParserV2& rtcpParser,
                          RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    uint32_t senderSSRC = rtcpPacket.TMMBR.SenderSSRC;
    RTCPReceiveInformation* ptrReceiveInfo = GetReceiveInformation(senderSSRC);
    if (ptrReceiveInfo == NULL)
    {
        // This remote SSRC must be saved before.
        rtcpParser.Iterate();
        return;
    }
    if(rtcpPacket.TMMBR.MediaSSRC)
    {
        // rtcpPacket.TMMBR.MediaSSRC SHOULD be 0 if same as SenderSSRC
        // in relay mode this is a valid number
        senderSSRC = rtcpPacket.TMMBR.MediaSSRC;
    }

    // Use packet length to calc max number of TMMBR blocks
    // each TMMBR block is 8 bytes
    ptrdiff_t maxNumOfTMMBRBlocks = rtcpParser.LengthLeft() / 8;

    // sanity
    if(maxNumOfTMMBRBlocks > 200) // we can't have more than what's in one packet
    {
        assert(false);
        rtcpParser.Iterate();
        return;
    }
    ptrReceiveInfo->VerifyAndAllocateTMMBRSet((uint32_t)maxNumOfTMMBRBlocks);

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpRtpfbTmmbrItemCode)
    {
        HandleTMMBRItem(*ptrReceiveInfo, rtcpPacket, rtcpPacketInformation, senderSSRC);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleTMMBRItem(RTCPReceiveInformation& receiveInfo,
                              const RTCPUtility::RTCPPacket& rtcpPacket,
                              RTCPPacketInformation& rtcpPacketInformation,
                              const uint32_t senderSSRC)
{
    if (main_ssrc_ == rtcpPacket.TMMBRItem.SSRC &&
        rtcpPacket.TMMBRItem.MaxTotalMediaBitRate > 0)
    {
        receiveInfo.InsertTMMBRItem(senderSSRC, rtcpPacket.TMMBRItem,
                                    _clock->TimeInMilliseconds());
        rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpTmmbr;
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleTMMBN(RTCPUtility::RTCPParserV2& rtcpParser,
                          RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
    RTCPReceiveInformation* ptrReceiveInfo = GetReceiveInformation(rtcpPacket.TMMBN.SenderSSRC);
    if (ptrReceiveInfo == NULL)
    {
        // This remote SSRC must be saved before.
        rtcpParser.Iterate();
        return;
    }
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpTmmbn;
    // Use packet length to calc max number of TMMBN blocks
    // each TMMBN block is 8 bytes
    ptrdiff_t maxNumOfTMMBNBlocks = rtcpParser.LengthLeft() / 8;

    // sanity
    if(maxNumOfTMMBNBlocks > 200) // we cant have more than what's in one packet
    {
        assert(false);
        rtcpParser.Iterate();
        return;
    }

    ptrReceiveInfo->VerifyAndAllocateBoundingSet((uint32_t)maxNumOfTMMBNBlocks);

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpRtpfbTmmbnItemCode)
    {
        HandleTMMBNItem(*ptrReceiveInfo, rtcpPacket);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSR_REQ(RTCPUtility::RTCPParserV2& rtcpParser,
                           RTCPPacketInformation& rtcpPacketInformation)
{
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpSrReq;
    rtcpParser.Iterate();
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleTMMBNItem(RTCPReceiveInformation& receiveInfo,
                              const RTCPUtility::RTCPPacket& rtcpPacket)
{
    receiveInfo.TmmbnBoundingSet.AddEntry(
        rtcpPacket.TMMBNItem.MaxTotalMediaBitRate,
        rtcpPacket.TMMBNItem.MeasuredOverhead,
        rtcpPacket.TMMBNItem.SSRC);
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSLI(RTCPUtility::RTCPParserV2& rtcpParser,
                        RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpPsfbSliItemCode)
    {
        HandleSLIItem(rtcpPacket, rtcpPacketInformation);
        pktType = rtcpParser.Iterate();
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleSLIItem(const RTCPUtility::RTCPPacket& rtcpPacket,
                            RTCPPacketInformation& rtcpPacketInformation)
{
    // in theory there could be multiple slices lost
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpSli; // received signal that we need to refresh a slice
    rtcpPacketInformation.sliPictureId = rtcpPacket.SLIItem.PictureId;
}

void
RTCPReceiver::HandleRPSI(RTCPUtility::RTCPParserV2& rtcpParser,
                         RTCPHelp::RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    if(pktType == RTCPUtility::kRtcpPsfbRpsiCode)
    {
        rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRpsi; // received signal that we have a confirmed reference picture
        if(rtcpPacket.RPSI.NumberOfValidBits%8 != 0)
        {
            // to us unknown
            // continue
            rtcpParser.Iterate();
            return;
        }
        rtcpPacketInformation.rpsiPictureId = 0;

        // convert NativeBitString to rpsiPictureId
        uint8_t numberOfBytes = rtcpPacket.RPSI.NumberOfValidBits /8;
        for(uint8_t n = 0; n < (numberOfBytes-1); n++)
        {
            rtcpPacketInformation.rpsiPictureId += (rtcpPacket.RPSI.NativeBitString[n] & 0x7f);
            rtcpPacketInformation.rpsiPictureId <<= 7; // prepare next
        }
        rtcpPacketInformation.rpsiPictureId += (rtcpPacket.RPSI.NativeBitString[numberOfBytes-1] & 0x7f);
    }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void RTCPReceiver::HandlePsfbApp(RTCPUtility::RTCPParserV2& rtcpParser,
                                 RTCPPacketInformation& rtcpPacketInformation) {
  RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
  if (pktType == RTCPUtility::kRtcpPsfbRembCode) {
    pktType = rtcpParser.Iterate();
    if (pktType == RTCPUtility::kRtcpPsfbRembItemCode) {
      HandleREMBItem(rtcpParser, rtcpPacketInformation);
      rtcpParser.Iterate();
    }
  }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void
RTCPReceiver::HandleIJ(RTCPUtility::RTCPParserV2& rtcpParser,
                       RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
    while (pktType == RTCPUtility::kRtcpExtendedIjItemCode)
    {
        HandleIJItem(rtcpPacket, rtcpPacketInformation);
        pktType = rtcpParser.Iterate();
    }
}

void
RTCPReceiver::HandleIJItem(const RTCPUtility::RTCPPacket& rtcpPacket,
                           RTCPPacketInformation& rtcpPacketInformation)
{
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpTransmissionTimeOffset;
    rtcpPacketInformation.interArrivalJitter =
    rtcpPacket.ExtendedJitterReportItem.Jitter;
}

void RTCPReceiver::HandleREMBItem(
    RTCPUtility::RTCPParserV2& rtcpParser,
    RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
  rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpRemb;
  rtcpPacketInformation.receiverEstimatedMaxBitrate =
      rtcpPacket.REMBItem.BitRate;
}

// no need for critsect we have _criticalSectionRTCPReceiver
void RTCPReceiver::HandleFIR(RTCPUtility::RTCPParserV2& rtcpParser,
                             RTCPPacketInformation& rtcpPacketInformation) {
  const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();
  RTCPReceiveInformation* ptrReceiveInfo =
      GetReceiveInformation(rtcpPacket.FIR.SenderSSRC);

  RTCPUtility::RTCPPacketTypes pktType = rtcpParser.Iterate();
  while (pktType == RTCPUtility::kRtcpPsfbFirItemCode) {
    HandleFIRItem(ptrReceiveInfo, rtcpPacket, rtcpPacketInformation);
    pktType = rtcpParser.Iterate();
  }
}

// no need for critsect we have _criticalSectionRTCPReceiver
void RTCPReceiver::HandleFIRItem(RTCPReceiveInformation* receiveInfo,
                                 const RTCPUtility::RTCPPacket& rtcpPacket,
                                 RTCPPacketInformation& rtcpPacketInformation) {
  // Is it our sender that is requested to generate a new keyframe
  if (main_ssrc_ != rtcpPacket.FIRItem.SSRC) {
    return;
  }
  // rtcpPacket.FIR.MediaSSRC SHOULD be 0 but we ignore to check it
  // we don't know who this originate from
  if (receiveInfo) {
    // check if we have reported this FIRSequenceNumber before
    if (rtcpPacket.FIRItem.CommandSequenceNumber !=
        receiveInfo->lastFIRSequenceNumber) {
      int64_t now = _clock->TimeInMilliseconds();
      // sanity; don't go crazy with the callbacks
      if ((now - receiveInfo->lastFIRRequest) > RTCP_MIN_FRAME_LENGTH_MS) {
        receiveInfo->lastFIRRequest = now;
        receiveInfo->lastFIRSequenceNumber =
            rtcpPacket.FIRItem.CommandSequenceNumber;
        // received signal that we need to send a new key frame
        rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpFir;
      }
    }
  } else {
    // received signal that we need to send a new key frame
    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpFir;
  }
}

void
RTCPReceiver::HandleAPP(RTCPUtility::RTCPParserV2& rtcpParser,
                        RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    rtcpPacketInformation.rtcpPacketTypeFlags |= kRtcpApp;
    rtcpPacketInformation.applicationSubType = rtcpPacket.APP.SubType;
    rtcpPacketInformation.applicationName = rtcpPacket.APP.Name;

    rtcpParser.Iterate();
}

void
RTCPReceiver::HandleAPPItem(RTCPUtility::RTCPParserV2& rtcpParser,
                           RTCPPacketInformation& rtcpPacketInformation)
{
    const RTCPUtility::RTCPPacket& rtcpPacket = rtcpParser.Packet();

    rtcpPacketInformation.AddApplicationData(rtcpPacket.APP.Data, rtcpPacket.APP.Size);

    rtcpParser.Iterate();
}

int32_t RTCPReceiver::UpdateTMMBR() {
  int32_t numBoundingSet = 0;
  uint32_t bitrate = 0;
  uint32_t accNumCandidates = 0;

  int32_t size = TMMBRReceived(0, 0, NULL);
  if (size > 0) {
    TMMBRSet* candidateSet = VerifyAndAllocateCandidateSet(size);
    // Get candidate set from receiver.
    accNumCandidates = TMMBRReceived(size, accNumCandidates, candidateSet);
  } else {
    // Candidate set empty.
    VerifyAndAllocateCandidateSet(0);  // resets candidate set
  }
  // Find bounding set
  TMMBRSet* boundingSet = NULL;
  numBoundingSet = FindTMMBRBoundingSet(boundingSet);
  if (numBoundingSet == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                 "Failed to find TMMBR bounding set.");
    return -1;
  }
  // Set bounding set
  // Inform remote clients about the new bandwidth
  // inform the remote client
  _rtpRtcp.SetTMMBN(boundingSet);

  // might trigger a TMMBN
  if (numBoundingSet == 0) {
    // owner of max bitrate request has timed out
    // empty bounding set has been sent
    return 0;
  }
  // Get net bitrate from bounding set depending on sent packet rate
  if (CalcMinBitRate(&bitrate)) {
    // we have a new bandwidth estimate on this channel
    CriticalSectionScoped lock(_criticalSectionFeedbacks);
    if (_cbRtcpBandwidthObserver) {
        _cbRtcpBandwidthObserver->OnReceivedEstimatedBitrate(bitrate * 1000);
      WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id,
                   "Set TMMBR request:%d kbps", bitrate);
    }
  }
  return 0;
}

void RTCPReceiver::RegisterRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  CriticalSectionScoped cs(_criticalSectionFeedbacks);
  if (callback != NULL)
    assert(stats_callback_ == NULL);
  stats_callback_ = callback;
}

RtcpStatisticsCallback* RTCPReceiver::GetRtcpStatisticsCallback() {
  CriticalSectionScoped cs(_criticalSectionFeedbacks);
  return stats_callback_;
}

// Holding no Critical section
void RTCPReceiver::TriggerCallbacksFromRTCPPacket(
    RTCPPacketInformation& rtcpPacketInformation) {
  // Process TMMBR and REMB first to avoid multiple callbacks
  // to OnNetworkChanged.
  if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpTmmbr) {
    WEBRTC_TRACE(kTraceStateInfo, kTraceRtpRtcp, _id,
                 "SIG [RTCP] Incoming TMMBR to id:%d", _id);

    // Might trigger a OnReceivedBandwidthEstimateUpdate.
    UpdateTMMBR();
  }
  unsigned int local_ssrc = 0;
  {
    // We don't want to hold this critsect when triggering the callbacks below.
    CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
    local_ssrc = main_ssrc_;
  }
  if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSrReq) {
    _rtpRtcp.OnRequestSendReport();
  }
  if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpNack) {
    if (rtcpPacketInformation.nackSequenceNumbers.size() > 0) {
      WEBRTC_TRACE(kTraceStateInfo, kTraceRtpRtcp, _id,
                   "SIG [RTCP] Incoming NACK length:%d",
                   rtcpPacketInformation.nackSequenceNumbers.size());
      _rtpRtcp.OnReceivedNACK(rtcpPacketInformation.nackSequenceNumbers);
    }
  }
  {
    CriticalSectionScoped lock(_criticalSectionFeedbacks);

    // We need feedback that we have received a report block(s) so that we
    // can generate a new packet in a conference relay scenario, one received
    // report can generate several RTCP packets, based on number relayed/mixed
    // a send report block should go out to all receivers.
    if (_cbRtcpIntraFrameObserver) {
      if ((rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpPli) ||
          (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpFir)) {
        if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpPli) {
          WEBRTC_TRACE(kTraceStateInfo, kTraceRtpRtcp, _id,
                       "SIG [RTCP] Incoming PLI from SSRC:0x%x",
                       rtcpPacketInformation.remoteSSRC);
        } else {
          WEBRTC_TRACE(kTraceStateInfo, kTraceRtpRtcp, _id,
                       "SIG [RTCP] Incoming FIR from SSRC:0x%x",
                       rtcpPacketInformation.remoteSSRC);
        }
        _cbRtcpIntraFrameObserver->OnReceivedIntraFrameRequest(local_ssrc);
      }
      if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSli) {
        _cbRtcpIntraFrameObserver->OnReceivedSLI(
            local_ssrc, rtcpPacketInformation.sliPictureId);
      }
      if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRpsi) {
        _cbRtcpIntraFrameObserver->OnReceivedRPSI(
            local_ssrc, rtcpPacketInformation.rpsiPictureId);
      }
    }
    if (_cbRtcpBandwidthObserver) {
      if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRemb) {
        WEBRTC_TRACE(kTraceStateInfo, kTraceRtpRtcp, _id,
                     "SIG [RTCP] Incoming REMB:%d",
                     rtcpPacketInformation.receiverEstimatedMaxBitrate);
        _cbRtcpBandwidthObserver->OnReceivedEstimatedBitrate(
            rtcpPacketInformation.receiverEstimatedMaxBitrate);
      }
      if (rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSr ||
          rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpRr) {
        int64_t now = _clock->TimeInMilliseconds();
        _cbRtcpBandwidthObserver->OnReceivedRtcpReceiverReport(
            rtcpPacketInformation.report_blocks,
            rtcpPacketInformation.rtt,
            now);
      }
    }
    if(_cbRtcpFeedback) {
      if(!(rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpSr)) {
        _cbRtcpFeedback->OnReceiveReportReceived(_id,
            rtcpPacketInformation.remoteSSRC);
      }
      if(rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpXrVoipMetric) {
        _cbRtcpFeedback->OnXRVoIPMetricReceived(_id,
            rtcpPacketInformation.VoIPMetric);
      }
      if(rtcpPacketInformation.rtcpPacketTypeFlags & kRtcpApp) {
        _cbRtcpFeedback->OnApplicationDataReceived(_id,
            rtcpPacketInformation.applicationSubType,
            rtcpPacketInformation.applicationName,
            rtcpPacketInformation.applicationLength,
            rtcpPacketInformation.applicationData);
      }
    }
  }

  {
    CriticalSectionScoped cs(_criticalSectionFeedbacks);
    if (stats_callback_) {
      for (ReportBlockList::const_iterator it =
          rtcpPacketInformation.report_blocks.begin();
          it != rtcpPacketInformation.report_blocks.end();
          ++it) {
        RtcpStatistics stats;
        stats.cumulative_lost = it->cumulativeLost;
        stats.extended_max_sequence_number = it->extendedHighSeqNum;
        stats.fraction_lost = it->fractionLost;
        stats.jitter = it->jitter;

        stats_callback_->StatisticsUpdated(stats, local_ssrc);
      }
    }
  }
}

int32_t RTCPReceiver::CNAME(const uint32_t remoteSSRC,
                            char cName[RTCP_CNAME_SIZE]) const {
  assert(cName);

  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);
  RTCPCnameInformation* cnameInfo = GetCnameInformation(remoteSSRC);
  if (cnameInfo == NULL) {
    return -1;
  }
  cName[RTCP_CNAME_SIZE - 1] = 0;
  strncpy(cName, cnameInfo->name, RTCP_CNAME_SIZE - 1);
  return 0;
}

// no callbacks allowed inside this function
int32_t RTCPReceiver::TMMBRReceived(const uint32_t size,
                                    const uint32_t accNumCandidates,
                                    TMMBRSet* candidateSet) const {
  CriticalSectionScoped lock(_criticalSectionRTCPReceiver);

  std::map<uint32_t, RTCPReceiveInformation*>::const_iterator
      receiveInfoIt = _receivedInfoMap.begin();
  if (receiveInfoIt == _receivedInfoMap.end()) {
    return -1;
  }
  uint32_t num = accNumCandidates;
  if (candidateSet) {
    while( num < size && receiveInfoIt != _receivedInfoMap.end()) {
      RTCPReceiveInformation* receiveInfo = receiveInfoIt->second;
      if (receiveInfo == NULL) {
        return 0;
      }
      for (uint32_t i = 0;
           (num < size) && (i < receiveInfo->TmmbrSet.lengthOfSet()); i++) {
        if (receiveInfo->GetTMMBRSet(i, num, candidateSet,
                                     _clock->TimeInMilliseconds()) == 0) {
          num++;
        }
      }
      receiveInfoIt++;
    }
  } else {
    while (receiveInfoIt != _receivedInfoMap.end()) {
      RTCPReceiveInformation* receiveInfo = receiveInfoIt->second;
      if(receiveInfo == NULL) {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                     "%s failed to get RTCPReceiveInformation",
                     __FUNCTION__);
        return -1;
      }
      num += receiveInfo->TmmbrSet.lengthOfSet();
      receiveInfoIt++;
    }
  }
  return num;
}

}  // namespace webrtc
