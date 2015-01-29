/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_sender.h"

#include <assert.h>  // assert
#include <stdlib.h>  // rand
#include <string.h>  // memcpy

#include <algorithm>  // min

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

using RTCPUtility::RTCPCnameInformation;

NACKStringBuilder::NACKStringBuilder() :
    _stream(""), _count(0), _consecutive(false)
{
    // Empty.
}

NACKStringBuilder::~NACKStringBuilder() {}

void NACKStringBuilder::PushNACK(uint16_t nack)
{
    if (_count == 0)
    {
        _stream << nack;
    } else if (nack == _prevNack + 1)
    {
        _consecutive = true;
    } else
    {
        if (_consecutive)
        {
            _stream << "-" << _prevNack;
            _consecutive = false;
        }
        _stream << "," << nack;
    }
    _count++;
    _prevNack = nack;
}

std::string NACKStringBuilder::GetResult()
{
    if (_consecutive)
    {
        _stream << "-" << _prevNack;
        _consecutive = false;
    }
    return _stream.str();
}

RTCPSender::FeedbackState::FeedbackState()
    : send_payload_type(0),
      frequency_hz(0),
      packets_sent(0),
      media_bytes_sent(0),
      send_bitrate(0),
      last_rr_ntp_secs(0),
      last_rr_ntp_frac(0),
      remote_sr(0),
      has_last_xr_rr(false) {}

RTCPSender::RTCPSender(const int32_t id,
                       const bool audio,
                       Clock* clock,
                       ReceiveStatistics* receive_statistics) :
    _id(id),
    _audio(audio),
    _clock(clock),
    _method(kRtcpOff),
    _criticalSectionTransport(CriticalSectionWrapper::CreateCriticalSection()),
    _cbTransport(NULL),

    _criticalSectionRTCPSender(CriticalSectionWrapper::CreateCriticalSection()),
    _usingNack(false),
    _sending(false),
    _sendTMMBN(false),
    _REMB(false),
    _sendREMB(false),
    _TMMBR(false),
    _IJ(false),
    _nextTimeToSendRTCP(0),
    start_timestamp_(0),
    last_rtp_timestamp_(0),
    last_frame_capture_time_ms_(-1),
    _SSRC(0),
    _remoteSSRC(0),
    _CNAME(),
    receive_statistics_(receive_statistics),
    internal_report_blocks_(),
    external_report_blocks_(),
    _csrcCNAMEs(),

    _cameraDelayMS(0),

    _lastSendReport(),
    _lastRTCPTime(),
    _lastSRPacketCount(),
    _lastSROctetCount(),

    last_xr_rr_(),

    _CSRCs(0),
    _CSRC(),
    _includeCSRCs(true),

    _sequenceNumberFIR(0),

    _lengthRembSSRC(0),
    _sizeRembSSRC(0),
    _rembSSRC(NULL),
    _rembBitrate(0),

    _tmmbrHelp(),
    _tmmbr_Send(0),
    _packetOH_Send(0),

    _appSend(false),
    _appSubType(0),
    _appName(),
    _appData(NULL),
    _appLength(0),

    xrSendReceiverReferenceTimeEnabled_(false),
    _xrSendVoIPMetric(false),
    _xrVoIPMetric()
{
    memset(_CNAME, 0, sizeof(_CNAME));
    memset(_lastSendReport, 0, sizeof(_lastSendReport));
    memset(_lastRTCPTime, 0, sizeof(_lastRTCPTime));
    memset(_lastSRPacketCount, 0, sizeof(_lastSRPacketCount));
    memset(_lastSROctetCount, 0, sizeof(_lastSROctetCount));
}

RTCPSender::~RTCPSender() {
  delete [] _rembSSRC;
  delete [] _appData;

  while (!internal_report_blocks_.empty()) {
    delete internal_report_blocks_.begin()->second;
    internal_report_blocks_.erase(internal_report_blocks_.begin());
  }
  while (!external_report_blocks_.empty()) {
    std::map<uint32_t, RTCPReportBlock*>::iterator it =
        external_report_blocks_.begin();
    delete it->second;
    external_report_blocks_.erase(it);
  }
  while (!_csrcCNAMEs.empty()) {
    std::map<uint32_t, RTCPCnameInformation*>::iterator it =
        _csrcCNAMEs.begin();
    delete it->second;
    _csrcCNAMEs.erase(it);
  }
  delete _criticalSectionTransport;
  delete _criticalSectionRTCPSender;
}

int32_t
RTCPSender::RegisterSendTransport(Transport* outgoingTransport)
{
    CriticalSectionScoped lock(_criticalSectionTransport);
    _cbTransport = outgoingTransport;
    return 0;
}

RTCPMethod
RTCPSender::Status() const
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    return _method;
}

int32_t
RTCPSender::SetRTCPStatus(const RTCPMethod method)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    if(method != kRtcpOff)
    {
        if(_audio)
        {
            _nextTimeToSendRTCP = _clock->TimeInMilliseconds() +
                (RTCP_INTERVAL_AUDIO_MS/2);
        } else
        {
            _nextTimeToSendRTCP = _clock->TimeInMilliseconds() +
                (RTCP_INTERVAL_VIDEO_MS/2);
        }
    }
    _method = method;
    return 0;
}

bool
RTCPSender::Sending() const
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    return _sending;
}

int32_t
RTCPSender::SetSendingStatus(const FeedbackState& feedback_state, bool sending)
{
    bool sendRTCPBye = false;
    {
        CriticalSectionScoped lock(_criticalSectionRTCPSender);

        if(_method != kRtcpOff)
        {
            if(sending == false && _sending == true)
            {
                // Trigger RTCP bye
                sendRTCPBye = true;
            }
        }
        _sending = sending;
    }
    if(sendRTCPBye)
    {
        return SendRTCP(feedback_state, kRtcpBye);
    }
    return 0;
}

bool
RTCPSender::REMB() const
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    return _REMB;
}

int32_t
RTCPSender::SetREMBStatus(const bool enable)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _REMB = enable;
    return 0;
}

int32_t
RTCPSender::SetREMBData(const uint32_t bitrate,
                        const uint8_t numberOfSSRC,
                        const uint32_t* SSRC)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _rembBitrate = bitrate;

    if(_sizeRembSSRC < numberOfSSRC)
    {
        delete [] _rembSSRC;
        _rembSSRC = new uint32_t[numberOfSSRC];
        _sizeRembSSRC = numberOfSSRC;
    }

    _lengthRembSSRC = numberOfSSRC;
    for (int i = 0; i < numberOfSSRC; i++)
    {
        _rembSSRC[i] = SSRC[i];
    }
    _sendREMB = true;
    // Send a REMB immediately if we have a new REMB. The frequency of REMBs is
    // throttled by the caller.
    _nextTimeToSendRTCP = _clock->TimeInMilliseconds();
    return 0;
}

bool
RTCPSender::TMMBR() const
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    return _TMMBR;
}

int32_t
RTCPSender::SetTMMBRStatus(const bool enable)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _TMMBR = enable;
    return 0;
}

bool
RTCPSender::IJ() const
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    return _IJ;
}

int32_t
RTCPSender::SetIJStatus(const bool enable)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _IJ = enable;
    return 0;
}

void RTCPSender::SetStartTimestamp(uint32_t start_timestamp) {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  start_timestamp_ = start_timestamp;
}

void RTCPSender::SetLastRtpTime(uint32_t rtp_timestamp,
                                int64_t capture_time_ms) {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  last_rtp_timestamp_ = rtp_timestamp;
  if (capture_time_ms < 0) {
    // We don't currently get a capture time from VoiceEngine.
    last_frame_capture_time_ms_ = _clock->TimeInMilliseconds();
  } else {
    last_frame_capture_time_ms_ = capture_time_ms;
  }
}

void
RTCPSender::SetSSRC( const uint32_t ssrc)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    if(_SSRC != 0)
    {
        // not first SetSSRC, probably due to a collision
        // schedule a new RTCP report
        // make sure that we send a RTP packet
        _nextTimeToSendRTCP = _clock->TimeInMilliseconds() + 100;
    }
    _SSRC = ssrc;
}

void RTCPSender::SetRemoteSSRC(uint32_t ssrc)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _remoteSSRC = ssrc;
}

int32_t
RTCPSender::SetCameraDelay(const int32_t delayMS)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    if(delayMS > 1000 || delayMS < -1000)
    {
        LOG(LS_WARNING) << "Delay can't be larger than 1 second: "
                        << delayMS << " ms";
        return -1;
    }
    _cameraDelayMS = delayMS;
    return 0;
}

int32_t RTCPSender::SetCNAME(const char cName[RTCP_CNAME_SIZE]) {
  if (!cName)
    return -1;

  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  _CNAME[RTCP_CNAME_SIZE - 1] = 0;
  strncpy(_CNAME, cName, RTCP_CNAME_SIZE - 1);
  return 0;
}

int32_t RTCPSender::AddMixedCNAME(const uint32_t SSRC,
                                  const char cName[RTCP_CNAME_SIZE]) {
  assert(cName);
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  if (_csrcCNAMEs.size() >= kRtpCsrcSize) {
    return -1;
  }
  RTCPCnameInformation* ptr = new RTCPCnameInformation();
  ptr->name[RTCP_CNAME_SIZE - 1] = 0;
  strncpy(ptr->name, cName, RTCP_CNAME_SIZE - 1);
  _csrcCNAMEs[SSRC] = ptr;
  return 0;
}

int32_t RTCPSender::RemoveMixedCNAME(const uint32_t SSRC) {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  std::map<uint32_t, RTCPCnameInformation*>::iterator it =
      _csrcCNAMEs.find(SSRC);

  if (it == _csrcCNAMEs.end()) {
    return -1;
  }
  delete it->second;
  _csrcCNAMEs.erase(it);
  return 0;
}

bool
RTCPSender::TimeToSendRTCPReport(const bool sendKeyframeBeforeRTP) const
{
/*
    For audio we use a fix 5 sec interval

    For video we use 1 sec interval fo a BW smaller than 360 kbit/s,
        technicaly we break the max 5% RTCP BW for video below 10 kbit/s but
        that should be extremely rare


From RFC 3550

    MAX RTCP BW is 5% if the session BW
        A send report is approximately 65 bytes inc CNAME
        A receiver report is approximately 28 bytes

    The RECOMMENDED value for the reduced minimum in seconds is 360
      divided by the session bandwidth in kilobits/second.  This minimum
      is smaller than 5 seconds for bandwidths greater than 72 kb/s.

    If the participant has not yet sent an RTCP packet (the variable
      initial is true), the constant Tmin is set to 2.5 seconds, else it
      is set to 5 seconds.

    The interval between RTCP packets is varied randomly over the
      range [0.5,1.5] times the calculated interval to avoid unintended
      synchronization of all participants

    if we send
    If the participant is a sender (we_sent true), the constant C is
      set to the average RTCP packet size (avg_rtcp_size) divided by 25%
      of the RTCP bandwidth (rtcp_bw), and the constant n is set to the
      number of senders.

    if we receive only
      If we_sent is not true, the constant C is set
      to the average RTCP packet size divided by 75% of the RTCP
      bandwidth.  The constant n is set to the number of receivers
      (members - senders).  If the number of senders is greater than
      25%, senders and receivers are treated together.

    reconsideration NOT required for peer-to-peer
      "timer reconsideration" is
      employed.  This algorithm implements a simple back-off mechanism
      which causes users to hold back RTCP packet transmission if the
      group sizes are increasing.

      n = number of members
      C = avg_size/(rtcpBW/4)

   3. The deterministic calculated interval Td is set to max(Tmin, n*C).

   4. The calculated interval T is set to a number uniformly distributed
      between 0.5 and 1.5 times the deterministic calculated interval.

   5. The resulting value of T is divided by e-3/2=1.21828 to compensate
      for the fact that the timer reconsideration algorithm converges to
      a value of the RTCP bandwidth below the intended average
*/

    int64_t now = _clock->TimeInMilliseconds();

    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    if(_method == kRtcpOff)
    {
        return false;
    }

    if(!_audio && sendKeyframeBeforeRTP)
    {
        // for video key-frames we want to send the RTCP before the large key-frame
        // if we have a 100 ms margin
        now += RTCP_SEND_BEFORE_KEY_FRAME_MS;
    }

    if(now >= _nextTimeToSendRTCP)
    {
        return true;

    } else if(now < 0x0000ffff && _nextTimeToSendRTCP > 0xffff0000) // 65 sec margin
    {
        // wrap
        return true;
    }
    return false;
}

uint32_t
RTCPSender::LastSendReport( uint32_t& lastRTCPTime)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    lastRTCPTime = _lastRTCPTime[0];
    return _lastSendReport[0];
}

bool
RTCPSender::GetSendReportMetadata(const uint32_t sendReport,
                                  uint32_t *timeOfSend,
                                  uint32_t *packetCount,
                                  uint64_t *octetCount)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    // This is only saved when we are the sender
    if((_lastSendReport[0] == 0) || (sendReport == 0))
    {
        return false;
    } else
    {
        for(int i = 0; i < RTCP_NUMBER_OF_SR; ++i)
        {
            if( _lastSendReport[i] == sendReport)
            {
                *timeOfSend = _lastRTCPTime[i];
                *packetCount = _lastSRPacketCount[i];
                *octetCount = _lastSROctetCount[i];
                return true;
            }
        }
    }
    return false;
}

bool RTCPSender::SendTimeOfXrRrReport(uint32_t mid_ntp,
                                      int64_t* time_ms) const {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);

  if (last_xr_rr_.empty()) {
    return false;
  }
  std::map<uint32_t, int64_t>::const_iterator it = last_xr_rr_.find(mid_ntp);
  if (it == last_xr_rr_.end()) {
    return false;
  }
  *time_ms = it->second;
  return true;
}

void RTCPSender::GetPacketTypeCounter(
    RtcpPacketTypeCounter* packet_counter) const {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  *packet_counter = packet_type_counter_;
}

int32_t RTCPSender::AddExternalReportBlock(
    uint32_t SSRC,
    const RTCPReportBlock* reportBlock) {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  return AddReportBlock(SSRC, &external_report_blocks_, reportBlock);
}

int32_t RTCPSender::AddReportBlock(
    uint32_t SSRC,
    std::map<uint32_t, RTCPReportBlock*>* report_blocks,
    const RTCPReportBlock* reportBlock) {
  assert(reportBlock);

  if (report_blocks->size() >= RTCP_MAX_REPORT_BLOCKS) {
    LOG(LS_WARNING) << "Too many report blocks.";
    return -1;
  }
  std::map<uint32_t, RTCPReportBlock*>::iterator it =
      report_blocks->find(SSRC);
  if (it != report_blocks->end()) {
    delete it->second;
    report_blocks->erase(it);
  }
  RTCPReportBlock* copyReportBlock = new RTCPReportBlock();
  memcpy(copyReportBlock, reportBlock, sizeof(RTCPReportBlock));
  (*report_blocks)[SSRC] = copyReportBlock;
  return 0;
}

int32_t RTCPSender::RemoveExternalReportBlock(uint32_t SSRC) {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);

  std::map<uint32_t, RTCPReportBlock*>::iterator it =
      external_report_blocks_.find(SSRC);

  if (it == external_report_blocks_.end()) {
    return -1;
  }
  delete it->second;
  external_report_blocks_.erase(it);
  return 0;
}

int32_t RTCPSender::BuildSR(const FeedbackState& feedback_state,
                            uint8_t* rtcpbuffer,
                            int& pos,
                            uint32_t NTPsec,
                            uint32_t NTPfrac)
{
    // sanity
    if(pos + 52 >= IP_PACKET_SIZE)
    {
        LOG(LS_WARNING) << "Failed to build Sender Report.";
        return -2;
    }
    uint32_t RTPtime;

    uint32_t posNumberOfReportBlocks = pos;
    rtcpbuffer[pos++]=(uint8_t)0x80;

    // Sender report
    rtcpbuffer[pos++]=(uint8_t)200;

    for(int i = (RTCP_NUMBER_OF_SR-2); i >= 0; i--)
    {
        // shift old
        _lastSendReport[i+1] = _lastSendReport[i];
        _lastRTCPTime[i+1] =_lastRTCPTime[i];
        _lastSRPacketCount[i+1] = _lastSRPacketCount[i];
        _lastSROctetCount[i+1] = _lastSROctetCount[i];
    }

    _lastRTCPTime[0] = Clock::NtpToMs(NTPsec, NTPfrac);
    _lastSendReport[0] = (NTPsec << 16) + (NTPfrac >> 16);
    _lastSRPacketCount[0] = feedback_state.packets_sent;
    _lastSROctetCount[0] = feedback_state.media_bytes_sent;

    // The timestamp of this RTCP packet should be estimated as the timestamp of
    // the frame being captured at this moment. We are calculating that
    // timestamp as the last frame's timestamp + the time since the last frame
    // was captured.
    RTPtime = start_timestamp_ + last_rtp_timestamp_ +
              (_clock->TimeInMilliseconds() - last_frame_capture_time_ms_) *
                  (feedback_state.frequency_hz / 1000);

    // Add sender data
    // Save  for our length field
    pos++;
    pos++;

    // Add our own SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
    pos += 4;
    // NTP
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, NTPsec);
    pos += 4;
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, NTPfrac);
    pos += 4;
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, RTPtime);
    pos += 4;

    //sender's packet count
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos,
                                      feedback_state.packets_sent);
    pos += 4;

    //sender's octet count
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos,
                                      feedback_state.media_bytes_sent);
    pos += 4;

    uint8_t numberOfReportBlocks = 0;
    int32_t retVal = WriteAllReportBlocksToBuffer(rtcpbuffer, pos,
                                                  numberOfReportBlocks,
                                                  NTPsec, NTPfrac);
    if(retVal < 0)
    {
        //
        return retVal ;
    }
    pos = retVal;
    rtcpbuffer[posNumberOfReportBlocks] += numberOfReportBlocks;

    uint16_t len = uint16_t((pos/4) -1);
    RtpUtility::AssignUWord16ToBuffer(rtcpbuffer + 2, len);
    return 0;
}


int32_t RTCPSender::BuildSDEC(uint8_t* rtcpbuffer, int& pos) {
  size_t lengthCname = strlen(_CNAME);
  assert(lengthCname < RTCP_CNAME_SIZE);

  // sanity
  if(pos + 12 + lengthCname  >= IP_PACKET_SIZE) {
    LOG(LS_WARNING) << "Failed to build SDEC.";
    return -2;
  }
  // SDEC Source Description

  // We always need to add SDES CNAME
  rtcpbuffer[pos++] = static_cast<uint8_t>(0x80 + 1 + _csrcCNAMEs.size());
  rtcpbuffer[pos++] = static_cast<uint8_t>(202);

  // handle SDES length later on
  uint32_t SDESLengthPos = pos;
  pos++;
  pos++;

  // Add our own SSRC
  RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
  pos += 4;

  // CNAME = 1
  rtcpbuffer[pos++] = static_cast<uint8_t>(1);

  //
  rtcpbuffer[pos++] = static_cast<uint8_t>(lengthCname);

  uint16_t SDESLength = 10;

  memcpy(&rtcpbuffer[pos], _CNAME, lengthCname);
  pos += lengthCname;
  SDESLength += (uint16_t)lengthCname;

  uint16_t padding = 0;
  // We must have a zero field even if we have an even multiple of 4 bytes
  if ((pos % 4) == 0) {
    padding++;
    rtcpbuffer[pos++]=0;
  }
  while ((pos % 4) != 0) {
    padding++;
    rtcpbuffer[pos++]=0;
  }
  SDESLength += padding;

  std::map<uint32_t, RTCPUtility::RTCPCnameInformation*>::iterator it =
      _csrcCNAMEs.begin();

  for(; it != _csrcCNAMEs.end(); it++) {
    RTCPCnameInformation* cname = it->second;
    uint32_t SSRC = it->first;

    // Add SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, SSRC);
    pos += 4;

    // CNAME = 1
    rtcpbuffer[pos++] = static_cast<uint8_t>(1);

    size_t length = strlen(cname->name);
    assert(length < RTCP_CNAME_SIZE);

    rtcpbuffer[pos++]= static_cast<uint8_t>(length);
    SDESLength += 6;

    memcpy(&rtcpbuffer[pos],cname->name, length);

    pos += length;
    SDESLength += length;
    uint16_t padding = 0;

    // We must have a zero field even if we have an even multiple of 4 bytes
    if((pos % 4) == 0){
      padding++;
      rtcpbuffer[pos++]=0;
    }
    while((pos % 4) != 0){
      padding++;
      rtcpbuffer[pos++] = 0;
    }
    SDESLength += padding;
  }
  // in 32-bit words minus one and we don't count the header
  uint16_t buffer_length = (SDESLength / 4) - 1;
  RtpUtility::AssignUWord16ToBuffer(rtcpbuffer + SDESLengthPos, buffer_length);
  return 0;
}

int32_t
RTCPSender::BuildRR(uint8_t* rtcpbuffer,
                    int& pos,
                    const uint32_t NTPsec,
                    const uint32_t NTPfrac)
{
    // sanity one block
    if(pos + 32 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    uint32_t posNumberOfReportBlocks = pos;

    rtcpbuffer[pos++]=(uint8_t)0x80;
    rtcpbuffer[pos++]=(uint8_t)201;

    // Save  for our length field
    pos++;
    pos++;

    // Add our own SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
    pos += 4;

    uint8_t numberOfReportBlocks = 0;
    int retVal = WriteAllReportBlocksToBuffer(rtcpbuffer, pos,
                                              numberOfReportBlocks,
                                              NTPsec, NTPfrac);
    if(retVal < 0)
    {
        return pos;
    }
    pos = retVal;
    rtcpbuffer[posNumberOfReportBlocks] += numberOfReportBlocks;

    uint16_t len = uint16_t((pos)/4 -1);
    RtpUtility::AssignUWord16ToBuffer(rtcpbuffer + 2, len);
    return 0;
}

// From RFC 5450: Transmission Time Offsets in RTP Streams.
//        0                   1                   2                   3
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   hdr |V=2|P|    RC   |   PT=IJ=195   |             length            |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |                      inter-arrival jitter                     |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       .                                                               .
//       .                                                               .
//       .                                                               .
//       |                      inter-arrival jitter                     |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  If present, this RTCP packet must be placed after a receiver report
//  (inside a compound RTCP packet), and MUST have the same value for RC
//  (reception report count) as the receiver report.

int32_t
RTCPSender::BuildExtendedJitterReport(
    uint8_t* rtcpbuffer,
    int& pos,
    const uint32_t jitterTransmissionTimeOffset)
{
    if (external_report_blocks_.size() > 0)
    {
        // TODO(andresp): Remove external report blocks since they are not
        // supported.
        LOG(LS_ERROR) << "Handling of external report blocks not implemented.";
        return 0;
    }

    // sanity
    if(pos + 8 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    // add picture loss indicator
    uint8_t RC = 1;
    rtcpbuffer[pos++]=(uint8_t)0x80 + RC;
    rtcpbuffer[pos++]=(uint8_t)195;

    // Used fixed length of 2
    rtcpbuffer[pos++]=(uint8_t)0;
    rtcpbuffer[pos++]=(uint8_t)(1);

    // Add inter-arrival jitter
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos,
                                      jitterTransmissionTimeOffset);
    pos += 4;
    return 0;
}

int32_t
RTCPSender::BuildPLI(uint8_t* rtcpbuffer, int& pos)
{
    // sanity
    if(pos + 12 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    // add picture loss indicator
    uint8_t FMT = 1;
    rtcpbuffer[pos++]=(uint8_t)0x80 + FMT;
    rtcpbuffer[pos++]=(uint8_t)206;

    //Used fixed length of 2
    rtcpbuffer[pos++]=(uint8_t)0;
    rtcpbuffer[pos++]=(uint8_t)(2);

    // Add our own SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _remoteSSRC);
    pos += 4;
    return 0;
}

int32_t RTCPSender::BuildFIR(uint8_t* rtcpbuffer,
                             int& pos,
                             bool repeat) {
  // sanity
  if(pos + 20 >= IP_PACKET_SIZE)  {
    return -2;
  }
  if (!repeat) {
    _sequenceNumberFIR++;   // do not increase if repetition
  }

  // add full intra request indicator
  uint8_t FMT = 4;
  rtcpbuffer[pos++] = (uint8_t)0x80 + FMT;
  rtcpbuffer[pos++] = (uint8_t)206;

  //Length of 4
  rtcpbuffer[pos++] = (uint8_t)0;
  rtcpbuffer[pos++] = (uint8_t)(4);

  // Add our own SSRC
  RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
  pos += 4;

  // RFC 5104     4.3.1.2.  Semantics
  // SSRC of media source
  rtcpbuffer[pos++] = (uint8_t)0;
  rtcpbuffer[pos++] = (uint8_t)0;
  rtcpbuffer[pos++] = (uint8_t)0;
  rtcpbuffer[pos++] = (uint8_t)0;

  // Additional Feedback Control Information (FCI)
  RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _remoteSSRC);
  pos += 4;

  rtcpbuffer[pos++] = (uint8_t)(_sequenceNumberFIR);
  rtcpbuffer[pos++] = (uint8_t)0;
  rtcpbuffer[pos++] = (uint8_t)0;
  rtcpbuffer[pos++] = (uint8_t)0;
  return 0;
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            First        |        Number           | PictureID |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
int32_t
RTCPSender::BuildSLI(uint8_t* rtcpbuffer, int& pos, const uint8_t pictureID)
{
    // sanity
    if(pos + 16 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    // add slice loss indicator
    uint8_t FMT = 2;
    rtcpbuffer[pos++]=(uint8_t)0x80 + FMT;
    rtcpbuffer[pos++]=(uint8_t)206;

    //Used fixed length of 3
    rtcpbuffer[pos++]=(uint8_t)0;
    rtcpbuffer[pos++]=(uint8_t)(3);

    // Add our own SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _remoteSSRC);
    pos += 4;

    // Add first, number & picture ID 6 bits
    // first  = 0, 13 - bits
    // number = 0x1fff, 13 - bits only ones for now
    uint32_t sliField = (0x1fff << 6)+ (0x3f & pictureID);
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, sliField);
    pos += 4;
    return 0;
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      PB       |0| Payload Type|    Native RPSI bit string     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   defined per codec          ...                | Padding (0) |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
/*
*    Note: not generic made for VP8
*/
int32_t
RTCPSender::BuildRPSI(uint8_t* rtcpbuffer,
                     int& pos,
                     const uint64_t pictureID,
                     const uint8_t payloadType)
{
    // sanity
    if(pos + 24 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    // add Reference Picture Selection Indication
    uint8_t FMT = 3;
    rtcpbuffer[pos++]=(uint8_t)0x80 + FMT;
    rtcpbuffer[pos++]=(uint8_t)206;

    // calc length
    uint32_t bitsRequired = 7;
    uint8_t bytesRequired = 1;
    while((pictureID>>bitsRequired) > 0)
    {
        bitsRequired += 7;
        bytesRequired++;
    }

    uint8_t size = 3;
    if(bytesRequired > 6)
    {
        size = 5;
    } else if(bytesRequired > 2)
    {
        size = 4;
    }
    rtcpbuffer[pos++]=(uint8_t)0;
    rtcpbuffer[pos++]=size;

    // Add our own SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _remoteSSRC);
    pos += 4;

    // calc padding length
    uint8_t paddingBytes = 4-((2+bytesRequired)%4);
    if(paddingBytes == 4)
    {
        paddingBytes = 0;
    }
    // add padding length in bits
    rtcpbuffer[pos] = paddingBytes*8; // padding can be 0, 8, 16 or 24
    pos++;

    // add payload type
    rtcpbuffer[pos] = payloadType;
    pos++;

    // add picture ID
    for(int i = bytesRequired-1; i > 0; i--)
    {
        rtcpbuffer[pos] = 0x80 | uint8_t(pictureID >> (i*7));
        pos++;
    }
    // add last byte of picture ID
    rtcpbuffer[pos] = uint8_t(pictureID & 0x7f);
    pos++;

    // add padding
    for(int j = 0; j <paddingBytes; j++)
    {
        rtcpbuffer[pos] = 0;
        pos++;
    }
    return 0;
}

int32_t
RTCPSender::BuildREMB(uint8_t* rtcpbuffer, int& pos)
{
    // sanity
    if(pos + 20 + 4 * _lengthRembSSRC >= IP_PACKET_SIZE)
    {
        return -2;
    }
    // add application layer feedback
    uint8_t FMT = 15;
    rtcpbuffer[pos++]=(uint8_t)0x80 + FMT;
    rtcpbuffer[pos++]=(uint8_t)206;

    rtcpbuffer[pos++]=(uint8_t)0;
    rtcpbuffer[pos++]=_lengthRembSSRC + 4;

    // Add our own SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
    pos += 4;

    // Remote SSRC must be 0
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, 0);
    pos += 4;

    rtcpbuffer[pos++]='R';
    rtcpbuffer[pos++]='E';
    rtcpbuffer[pos++]='M';
    rtcpbuffer[pos++]='B';

    rtcpbuffer[pos++] = _lengthRembSSRC;
    // 6 bit Exp
    // 18 bit mantissa
    uint8_t brExp = 0;
    for(uint32_t i=0; i<64; i++)
    {
        if(_rembBitrate <= ((uint32_t)262143 << i))
        {
            brExp = i;
            break;
        }
    }
    const uint32_t brMantissa = (_rembBitrate >> brExp);
    rtcpbuffer[pos++]=(uint8_t)((brExp << 2) + ((brMantissa >> 16) & 0x03));
    rtcpbuffer[pos++]=(uint8_t)(brMantissa >> 8);
    rtcpbuffer[pos++]=(uint8_t)(brMantissa);

    for (int i = 0; i < _lengthRembSSRC; i++)
    {
      RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _rembSSRC[i]);
        pos += 4;
    }
    return 0;
}

void
RTCPSender::SetTargetBitrate(unsigned int target_bitrate)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _tmmbr_Send = target_bitrate / 1000;
}

int32_t RTCPSender::BuildTMMBR(ModuleRtpRtcpImpl* rtp_rtcp_module,
                               uint8_t* rtcpbuffer,
                               int& pos) {
    if (rtp_rtcp_module == NULL)
      return -1;
    // Before sending the TMMBR check the received TMMBN, only an owner is allowed to raise the bitrate
    // If the sender is an owner of the TMMBN -> send TMMBR
    // If not an owner but the TMMBR would enter the TMMBN -> send TMMBR

    // get current bounding set from RTCP receiver
    bool tmmbrOwner = false;
    // store in candidateSet, allocates one extra slot
    TMMBRSet* candidateSet = _tmmbrHelp.CandidateSet();

    // holding _criticalSectionRTCPSender while calling RTCPreceiver which
    // will accuire _criticalSectionRTCPReceiver is a potental deadlock but
    // since RTCPreceiver is not doing the reverse we should be fine
    int32_t lengthOfBoundingSet =
        rtp_rtcp_module->BoundingSet(tmmbrOwner, candidateSet);

    if(lengthOfBoundingSet > 0)
    {
        for (int32_t i = 0; i < lengthOfBoundingSet; i++)
        {
            if( candidateSet->Tmmbr(i) == _tmmbr_Send &&
                candidateSet->PacketOH(i) == _packetOH_Send)
            {
                // do not send the same tuple
                return 0;
            }
        }
        if(!tmmbrOwner)
        {
            // use received bounding set as candidate set
            // add current tuple
            candidateSet->SetEntry(lengthOfBoundingSet,
                                   _tmmbr_Send,
                                   _packetOH_Send,
                                   _SSRC);
            int numCandidates = lengthOfBoundingSet+ 1;

            // find bounding set
            TMMBRSet* boundingSet = NULL;
            int numBoundingSet = _tmmbrHelp.FindTMMBRBoundingSet(boundingSet);
            if(numBoundingSet > 0 || numBoundingSet <= numCandidates)
            {
                tmmbrOwner = _tmmbrHelp.IsOwner(_SSRC, numBoundingSet);
            }
            if(!tmmbrOwner)
            {
                // did not enter bounding set, no meaning to send this request
                return 0;
            }
        }
    }

    if(_tmmbr_Send)
    {
        // sanity
        if(pos + 20 >= IP_PACKET_SIZE)
        {
            return -2;
        }
        // add TMMBR indicator
        uint8_t FMT = 3;
        rtcpbuffer[pos++]=(uint8_t)0x80 + FMT;
        rtcpbuffer[pos++]=(uint8_t)205;

        //Length of 4
        rtcpbuffer[pos++]=(uint8_t)0;
        rtcpbuffer[pos++]=(uint8_t)(4);

        // Add our own SSRC
        RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
        pos += 4;

        // RFC 5104     4.2.1.2.  Semantics

        // SSRC of media source
        rtcpbuffer[pos++]=(uint8_t)0;
        rtcpbuffer[pos++]=(uint8_t)0;
        rtcpbuffer[pos++]=(uint8_t)0;
        rtcpbuffer[pos++]=(uint8_t)0;

        // Additional Feedback Control Information (FCI)
        RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _remoteSSRC);
        pos += 4;

        uint32_t bitRate = _tmmbr_Send*1000;
        uint32_t mmbrExp = 0;
        for(uint32_t i=0;i<64;i++)
        {
            if(bitRate <= ((uint32_t)131071 << i))
            {
                mmbrExp = i;
                break;
            }
        }
        uint32_t mmbrMantissa = (bitRate >> mmbrExp);

        rtcpbuffer[pos++]=(uint8_t)((mmbrExp << 2) + ((mmbrMantissa >> 15) & 0x03));
        rtcpbuffer[pos++]=(uint8_t)(mmbrMantissa >> 7);
        rtcpbuffer[pos++]=(uint8_t)((mmbrMantissa << 1) + ((_packetOH_Send >> 8)& 0x01));
        rtcpbuffer[pos++]=(uint8_t)(_packetOH_Send);
    }
    return 0;
}

int32_t
RTCPSender::BuildTMMBN(uint8_t* rtcpbuffer, int& pos)
{
    TMMBRSet* boundingSet = _tmmbrHelp.BoundingSetToSend();
    if(boundingSet == NULL)
    {
        return -1;
    }
    // sanity
    if(pos + 12 + boundingSet->lengthOfSet()*8 >= IP_PACKET_SIZE)
    {
        LOG(LS_WARNING) << "Failed to build TMMBN.";
        return -2;
    }
    uint8_t FMT = 4;
    // add TMMBN indicator
    rtcpbuffer[pos++]=(uint8_t)0x80 + FMT;
    rtcpbuffer[pos++]=(uint8_t)205;

    //Add length later
    int posLength = pos;
    pos++;
    pos++;

    // Add our own SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
    pos += 4;

    // RFC 5104     4.2.2.2.  Semantics

    // SSRC of media source
    rtcpbuffer[pos++]=(uint8_t)0;
    rtcpbuffer[pos++]=(uint8_t)0;
    rtcpbuffer[pos++]=(uint8_t)0;
    rtcpbuffer[pos++]=(uint8_t)0;

    // Additional Feedback Control Information (FCI)
    int numBoundingSet = 0;
    for(uint32_t n=0; n< boundingSet->lengthOfSet(); n++)
    {
        if (boundingSet->Tmmbr(n) > 0)
        {
            uint32_t tmmbrSSRC = boundingSet->Ssrc(n);
            RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, tmmbrSSRC);
            pos += 4;

            uint32_t bitRate = boundingSet->Tmmbr(n) * 1000;
            uint32_t mmbrExp = 0;
            for(int i=0; i<64; i++)
            {
                if(bitRate <=  ((uint32_t)131071 << i))
                {
                    mmbrExp = i;
                    break;
                }
            }
            uint32_t mmbrMantissa = (bitRate >> mmbrExp);
            uint32_t measuredOH = boundingSet->PacketOH(n);

            rtcpbuffer[pos++]=(uint8_t)((mmbrExp << 2) + ((mmbrMantissa >> 15) & 0x03));
            rtcpbuffer[pos++]=(uint8_t)(mmbrMantissa >> 7);
            rtcpbuffer[pos++]=(uint8_t)((mmbrMantissa << 1) + ((measuredOH >> 8)& 0x01));
            rtcpbuffer[pos++]=(uint8_t)(measuredOH);
            numBoundingSet++;
        }
    }
    uint16_t length= (uint16_t)(2+2*numBoundingSet);
    rtcpbuffer[posLength++]=(uint8_t)(length>>8);
    rtcpbuffer[posLength]=(uint8_t)(length);
    return 0;
}

int32_t
RTCPSender::BuildAPP(uint8_t* rtcpbuffer, int& pos)
{
    // sanity
    if(_appData == NULL)
    {
        LOG(LS_WARNING) << "Failed to build app specific.";
        return -1;
    }
    if(pos + 12 + _appLength >= IP_PACKET_SIZE)
    {
        LOG(LS_WARNING) << "Failed to build app specific.";
        return -2;
    }
    rtcpbuffer[pos++]=(uint8_t)0x80 + _appSubType;

    // Add APP ID
    rtcpbuffer[pos++]=(uint8_t)204;

    uint16_t length = (_appLength>>2) + 2; // include SSRC and name
    rtcpbuffer[pos++]=(uint8_t)(length>>8);
    rtcpbuffer[pos++]=(uint8_t)(length);

    // Add our own SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
    pos += 4;

    // Add our application name
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _appName);
    pos += 4;

    // Add the data
    memcpy(rtcpbuffer +pos, _appData,_appLength);
    pos += _appLength;
    return 0;
}

int32_t
RTCPSender::BuildNACK(uint8_t* rtcpbuffer,
                      int& pos,
                      const int32_t nackSize,
                      const uint16_t* nackList,
                      std::string* nackString)
{
    // sanity
    if(pos + 16 >= IP_PACKET_SIZE)
    {
        LOG(LS_WARNING) << "Failed to build NACK.";
        return -2;
    }

    // int size, uint16_t* nackList
    // add nack list
    uint8_t FMT = 1;
    rtcpbuffer[pos++]=(uint8_t)0x80 + FMT;
    rtcpbuffer[pos++]=(uint8_t)205;

    rtcpbuffer[pos++]=(uint8_t) 0;
    int nackSizePos = pos;
    rtcpbuffer[pos++]=(uint8_t)(3); //setting it to one kNACK signal as default

    // Add our own SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _remoteSSRC);
    pos += 4;

    // Build NACK bitmasks and write them to the RTCP message.
    // The nack list should be sorted and not contain duplicates if one
    // wants to build the smallest rtcp nack packet.
    int numOfNackFields = 0;
    int maxNackFields = std::min<int>(kRtcpMaxNackFields,
                                      (IP_PACKET_SIZE - pos) / 4);
    int i = 0;
    while (i < nackSize && numOfNackFields < maxNackFields) {
      uint16_t nack = nackList[i++];
      uint16_t bitmask = 0;
      while (i < nackSize) {
        int shift = static_cast<uint16_t>(nackList[i] - nack) - 1;
        if (shift >= 0 && shift <= 15) {
          bitmask |= (1 << shift);
          ++i;
        } else {
          break;
        }
      }
      // Write the sequence number and the bitmask to the packet.
      assert(pos + 4 < IP_PACKET_SIZE);
      RtpUtility::AssignUWord16ToBuffer(rtcpbuffer + pos, nack);
      pos += 2;
      RtpUtility::AssignUWord16ToBuffer(rtcpbuffer + pos, bitmask);
      pos += 2;
      numOfNackFields++;
    }
    rtcpbuffer[nackSizePos] = static_cast<uint8_t>(2 + numOfNackFields);

    if (i != nackSize) {
      LOG(LS_WARNING) << "Nack list too large for one packet.";
    }

    // Report stats.
    NACKStringBuilder stringBuilder;
    for (int idx = 0; idx < i; ++idx) {
      stringBuilder.PushNACK(nackList[idx]);
      nack_stats_.ReportRequest(nackList[idx]);
    }
    *nackString = stringBuilder.GetResult();
    packet_type_counter_.nack_requests = nack_stats_.requests();
    packet_type_counter_.unique_nack_requests = nack_stats_.unique_requests();
    return 0;
}

int32_t
RTCPSender::BuildBYE(uint8_t* rtcpbuffer, int& pos)
{
    // sanity
    if(pos + 8 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    if(_includeCSRCs)
    {
        // Add a bye packet
        rtcpbuffer[pos++]=(uint8_t)0x80 + 1 + _CSRCs;  // number of SSRC+CSRCs
        rtcpbuffer[pos++]=(uint8_t)203;

        // length
        rtcpbuffer[pos++]=(uint8_t)0;
        rtcpbuffer[pos++]=(uint8_t)(1 + _CSRCs);

        // Add our own SSRC
        RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
        pos += 4;

        // add CSRCs
        for(int i = 0; i < _CSRCs; i++)
        {
          RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _CSRC[i]);
            pos += 4;
        }
    } else
    {
        // Add a bye packet
        rtcpbuffer[pos++]=(uint8_t)0x80 + 1;  // number of SSRC+CSRCs
        rtcpbuffer[pos++]=(uint8_t)203;

        // length
        rtcpbuffer[pos++]=(uint8_t)0;
        rtcpbuffer[pos++]=(uint8_t)1;

        // Add our own SSRC
        RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
        pos += 4;
    }
    return 0;
}

int32_t RTCPSender::BuildReceiverReferenceTime(uint8_t* buffer,
                                               int& pos,
                                               uint32_t ntp_sec,
                                               uint32_t ntp_frac) {
  const int kRrTimeBlockLength = 20;
  if (pos + kRrTimeBlockLength >= IP_PACKET_SIZE) {
    return -2;
  }

  if (last_xr_rr_.size() >= RTCP_NUMBER_OF_SR) {
    last_xr_rr_.erase(last_xr_rr_.begin());
  }
  last_xr_rr_.insert(std::pair<uint32_t, int64_t>(
      RTCPUtility::MidNtp(ntp_sec, ntp_frac),
      Clock::NtpToMs(ntp_sec, ntp_frac)));

  // Add XR header.
  buffer[pos++] = 0x80;
  buffer[pos++] = 207;
  buffer[pos++] = 0;  // XR packet length.
  buffer[pos++] = 4;  // XR packet length.

  // Add our own SSRC.
  RtpUtility::AssignUWord32ToBuffer(buffer + pos, _SSRC);
  pos += 4;

  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |     BT=4      |   reserved    |       block length = 2        |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |              NTP timestamp, most significant word             |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |             NTP timestamp, least significant word             |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // Add Receiver Reference Time Report block.
  buffer[pos++] = 4;  // BT.
  buffer[pos++] = 0;  // Reserved.
  buffer[pos++] = 0;  // Block length.
  buffer[pos++] = 2;  // Block length.

  // NTP timestamp.
  RtpUtility::AssignUWord32ToBuffer(buffer + pos, ntp_sec);
  pos += 4;
  RtpUtility::AssignUWord32ToBuffer(buffer + pos, ntp_frac);
  pos += 4;

  return 0;
}

int32_t RTCPSender::BuildDlrr(uint8_t* buffer,
                              int& pos,
                              const RtcpReceiveTimeInfo& info) {
  const int kDlrrBlockLength = 24;
  if (pos + kDlrrBlockLength >= IP_PACKET_SIZE) {
    return -2;
  }

  // Add XR header.
  buffer[pos++] = 0x80;
  buffer[pos++] = 207;
  buffer[pos++] = 0;  // XR packet length.
  buffer[pos++] = 5;  // XR packet length.

  // Add our own SSRC.
  RtpUtility::AssignUWord32ToBuffer(buffer + pos, _SSRC);
  pos += 4;

  //   0                   1                   2                   3
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |     BT=5      |   reserved    |         block length          |
  //  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  //  |                 SSRC_1 (SSRC of first receiver)               | sub-
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  //  |                         last RR (LRR)                         |   1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |                   delay since last RR (DLRR)                  |
  //  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  //  |                 SSRC_2 (SSRC of second receiver)              | sub-
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  //  :                               ...                             :   2

  // Add DLRR sub block.
  buffer[pos++] = 5;  // BT.
  buffer[pos++] = 0;  // Reserved.
  buffer[pos++] = 0;  // Block length.
  buffer[pos++] = 3;  // Block length.

  // NTP timestamp.
  RtpUtility::AssignUWord32ToBuffer(buffer + pos, info.sourceSSRC);
  pos += 4;
  RtpUtility::AssignUWord32ToBuffer(buffer + pos, info.lastRR);
  pos += 4;
  RtpUtility::AssignUWord32ToBuffer(buffer + pos, info.delaySinceLastRR);
  pos += 4;

  return 0;
}

int32_t
RTCPSender::BuildVoIPMetric(uint8_t* rtcpbuffer, int& pos)
{
    // sanity
    if(pos + 44 >= IP_PACKET_SIZE)
    {
        return -2;
    }

    // Add XR header
    rtcpbuffer[pos++]=(uint8_t)0x80;
    rtcpbuffer[pos++]=(uint8_t)207;

    uint32_t XRLengthPos = pos;

    // handle length later on
    pos++;
    pos++;

    // Add our own SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
    pos += 4;

    // Add a VoIP metrics block
    rtcpbuffer[pos++]=7;
    rtcpbuffer[pos++]=0;
    rtcpbuffer[pos++]=0;
    rtcpbuffer[pos++]=8;

    // Add the remote SSRC
    RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _remoteSSRC);
    pos += 4;

    rtcpbuffer[pos++] = _xrVoIPMetric.lossRate;
    rtcpbuffer[pos++] = _xrVoIPMetric.discardRate;
    rtcpbuffer[pos++] = _xrVoIPMetric.burstDensity;
    rtcpbuffer[pos++] = _xrVoIPMetric.gapDensity;

    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.burstDuration >> 8);
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.burstDuration);
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.gapDuration >> 8);
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.gapDuration);

    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.roundTripDelay >> 8);
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.roundTripDelay);
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.endSystemDelay >> 8);
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.endSystemDelay);

    rtcpbuffer[pos++] = _xrVoIPMetric.signalLevel;
    rtcpbuffer[pos++] = _xrVoIPMetric.noiseLevel;
    rtcpbuffer[pos++] = _xrVoIPMetric.RERL;
    rtcpbuffer[pos++] = _xrVoIPMetric.Gmin;

    rtcpbuffer[pos++] = _xrVoIPMetric.Rfactor;
    rtcpbuffer[pos++] = _xrVoIPMetric.extRfactor;
    rtcpbuffer[pos++] = _xrVoIPMetric.MOSLQ;
    rtcpbuffer[pos++] = _xrVoIPMetric.MOSCQ;

    rtcpbuffer[pos++] = _xrVoIPMetric.RXconfig;
    rtcpbuffer[pos++] = 0; // reserved
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.JBnominal >> 8);
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.JBnominal);

    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.JBmax >> 8);
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.JBmax);
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.JBabsMax >> 8);
    rtcpbuffer[pos++] = (uint8_t)(_xrVoIPMetric.JBabsMax);

    rtcpbuffer[XRLengthPos]=(uint8_t)(0);
    rtcpbuffer[XRLengthPos+1]=(uint8_t)(10);
    return 0;
}

int32_t RTCPSender::SendRTCP(const FeedbackState& feedback_state,
                             uint32_t packetTypeFlags,
                             int32_t nackSize,
                             const uint16_t* nackList,
                             bool repeat,
                             uint64_t pictureID) {
  {
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    if(_method == kRtcpOff)
    {
        LOG(LS_WARNING) << "Can't send rtcp if it is disabled.";
        return -1;
    }
  }
  uint8_t rtcp_buffer[IP_PACKET_SIZE];
  int rtcp_length = PrepareRTCP(feedback_state,
                                packetTypeFlags,
                                nackSize,
                                nackList,
                                repeat,
                                pictureID,
                                rtcp_buffer,
                                IP_PACKET_SIZE);
  if (rtcp_length < 0) {
    return -1;
  }
  // Sanity don't send empty packets.
  if (rtcp_length == 0)
  {
      return -1;
  }
  return SendToNetwork(rtcp_buffer, static_cast<uint16_t>(rtcp_length));
}

int RTCPSender::PrepareRTCP(const FeedbackState& feedback_state,
                            uint32_t packetTypeFlags,
                            int32_t nackSize,
                            const uint16_t* nackList,
                            bool repeat,
                            uint64_t pictureID,
                            uint8_t* rtcp_buffer,
                            int buffer_size) {
  uint32_t rtcpPacketTypeFlags = packetTypeFlags;
  // Collect the received information.
  uint32_t NTPsec = 0;
  uint32_t NTPfrac = 0;
  uint32_t jitterTransmissionOffset = 0;
  int position = 0;

  CriticalSectionScoped lock(_criticalSectionRTCPSender);

  if(_TMMBR )  // Attach TMMBR to send and receive reports.
  {
      rtcpPacketTypeFlags |= kRtcpTmmbr;
  }
  if(_appSend)
  {
      rtcpPacketTypeFlags |= kRtcpApp;
      _appSend = false;
  }
  if(_REMB && _sendREMB)
  {
      // Always attach REMB to SR if that is configured. Note that REMB is
      // only sent on one of the RTP modules in the REMB group.
      rtcpPacketTypeFlags |= kRtcpRemb;
  }
  if(_xrSendVoIPMetric)
  {
      rtcpPacketTypeFlags |= kRtcpXrVoipMetric;
      _xrSendVoIPMetric = false;
  }
  if(_sendTMMBN)  // Set when having received a TMMBR.
  {
      rtcpPacketTypeFlags |= kRtcpTmmbn;
      _sendTMMBN = false;
  }
  if (rtcpPacketTypeFlags & kRtcpReport)
  {
      if (xrSendReceiverReferenceTimeEnabled_ && !_sending)
      {
          rtcpPacketTypeFlags |= kRtcpXrReceiverReferenceTime;
      }
      if (feedback_state.has_last_xr_rr)
      {
          rtcpPacketTypeFlags |= kRtcpXrDlrrReportBlock;
      }
  }
  if(_method == kRtcpCompound)
  {
      if(_sending)
      {
          rtcpPacketTypeFlags |= kRtcpSr;
      } else
      {
          rtcpPacketTypeFlags |= kRtcpRr;
      }
  } else if(_method == kRtcpNonCompound)
  {
      if(rtcpPacketTypeFlags & kRtcpReport)
      {
          if(_sending)
          {
              rtcpPacketTypeFlags |= kRtcpSr;
          } else
          {
              rtcpPacketTypeFlags |= kRtcpRr;
          }
      }
  }
  if( rtcpPacketTypeFlags & kRtcpRr ||
      rtcpPacketTypeFlags & kRtcpSr)
  {
      // generate next time to send a RTCP report
      // seeded from RTP constructor
      int32_t random = rand() % 1000;
      int32_t timeToNext = RTCP_INTERVAL_AUDIO_MS;

      if(_audio)
      {
          timeToNext = (RTCP_INTERVAL_AUDIO_MS/2) +
              (RTCP_INTERVAL_AUDIO_MS*random/1000);
      }else
      {
          uint32_t minIntervalMs = RTCP_INTERVAL_AUDIO_MS;
          if(_sending)
          {
            // Calculate bandwidth for video; 360 / send bandwidth in kbit/s.
            uint32_t send_bitrate_kbit = feedback_state.send_bitrate / 1000;
            if (send_bitrate_kbit != 0)
              minIntervalMs = 360000 / send_bitrate_kbit;
          }
          if(minIntervalMs > RTCP_INTERVAL_VIDEO_MS)
          {
              minIntervalMs = RTCP_INTERVAL_VIDEO_MS;
          }
          timeToNext = (minIntervalMs/2) + (minIntervalMs*random/1000);
      }
      _nextTimeToSendRTCP = _clock->TimeInMilliseconds() + timeToNext;
  }

  // If the data does not fit in the packet we fill it as much as possible.
  int32_t buildVal = 0;

  // We need to send our NTP even if we haven't received any reports.
  _clock->CurrentNtp(NTPsec, NTPfrac);
  if (ShouldSendReportBlocks(rtcpPacketTypeFlags)) {
    StatisticianMap statisticians =
        receive_statistics_->GetActiveStatisticians();
    if (!statisticians.empty()) {
      StatisticianMap::const_iterator it;
      int i;
      for (it = statisticians.begin(), i = 0; it != statisticians.end();
           ++it, ++i) {
        RTCPReportBlock report_block;
        if (PrepareReport(
                feedback_state, it->second, &report_block, &NTPsec, &NTPfrac))
          AddReportBlock(it->first, &internal_report_blocks_, &report_block);
      }
      if (_IJ && !statisticians.empty()) {
        rtcpPacketTypeFlags |= kRtcpTransmissionTimeOffset;
      }
    }
  }

  if(rtcpPacketTypeFlags & kRtcpSr)
  {
    buildVal = BuildSR(feedback_state, rtcp_buffer, position, NTPsec, NTPfrac);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
      buildVal = BuildSDEC(rtcp_buffer, position);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }else if(rtcpPacketTypeFlags & kRtcpRr)
  {
      buildVal = BuildRR(rtcp_buffer, position, NTPsec, NTPfrac);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
      // only of set
      if(_CNAME[0] != 0)
      {
          buildVal = BuildSDEC(rtcp_buffer, position);
          if (buildVal == -1) {
            return -1;
          }
      }
  }
  if(rtcpPacketTypeFlags & kRtcpTransmissionTimeOffset)
  {
      // If present, this RTCP packet must be placed after a
      // receiver report.
      buildVal = BuildExtendedJitterReport(rtcp_buffer,
                                           position,
                                           jitterTransmissionOffset);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }
  if(rtcpPacketTypeFlags & kRtcpPli)
  {
      buildVal = BuildPLI(rtcp_buffer, position);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
      TRACE_EVENT_INSTANT0("webrtc_rtp", "RTCPSender::PLI");
      ++packet_type_counter_.pli_packets;
      TRACE_COUNTER_ID1("webrtc_rtp", "RTCP_PLICount", _SSRC,
                        packet_type_counter_.pli_packets);
  }
  if(rtcpPacketTypeFlags & kRtcpFir)
  {
      buildVal = BuildFIR(rtcp_buffer, position, repeat);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
      TRACE_EVENT_INSTANT0("webrtc_rtp", "RTCPSender::FIR");
      ++packet_type_counter_.fir_packets;
      TRACE_COUNTER_ID1("webrtc_rtp", "RTCP_FIRCount", _SSRC,
                        packet_type_counter_.fir_packets);
  }
  if(rtcpPacketTypeFlags & kRtcpSli)
  {
      buildVal = BuildSLI(rtcp_buffer, position, (uint8_t)pictureID);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }
  if(rtcpPacketTypeFlags & kRtcpRpsi)
  {
      const int8_t payloadType = feedback_state.send_payload_type;
      if (payloadType == -1) {
        return -1;
      }
      buildVal = BuildRPSI(rtcp_buffer, position, pictureID,
                           (uint8_t)payloadType);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }
  if(rtcpPacketTypeFlags & kRtcpRemb)
  {
      buildVal = BuildREMB(rtcp_buffer, position);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
      TRACE_EVENT_INSTANT0("webrtc_rtp", "RTCPSender::REMB");
  }
  if(rtcpPacketTypeFlags & kRtcpBye)
  {
      buildVal = BuildBYE(rtcp_buffer, position);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }
  if(rtcpPacketTypeFlags & kRtcpApp)
  {
      buildVal = BuildAPP(rtcp_buffer, position);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }
  if(rtcpPacketTypeFlags & kRtcpTmmbr)
  {
      buildVal = BuildTMMBR(feedback_state.module, rtcp_buffer, position);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }
  if(rtcpPacketTypeFlags & kRtcpTmmbn)
  {
      buildVal = BuildTMMBN(rtcp_buffer, position);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }
  if(rtcpPacketTypeFlags & kRtcpNack)
  {
      std::string nackString;
      buildVal = BuildNACK(rtcp_buffer, position, nackSize, nackList,
                           &nackString);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
      TRACE_EVENT_INSTANT1("webrtc_rtp", "RTCPSender::NACK",
                           "nacks", TRACE_STR_COPY(nackString.c_str()));
      ++packet_type_counter_.nack_packets;
      TRACE_COUNTER_ID1("webrtc_rtp", "RTCP_NACKCount", _SSRC,
                        packet_type_counter_.nack_packets);
  }
  if(rtcpPacketTypeFlags & kRtcpXrVoipMetric)
  {
      buildVal = BuildVoIPMetric(rtcp_buffer, position);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }
  if (rtcpPacketTypeFlags & kRtcpXrReceiverReferenceTime)
  {
      buildVal = BuildReceiverReferenceTime(rtcp_buffer,
                                            position,
                                            NTPsec,
                                            NTPfrac);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }
  if (rtcpPacketTypeFlags & kRtcpXrDlrrReportBlock)
  {
      buildVal = BuildDlrr(rtcp_buffer, position, feedback_state.last_xr_rr);
      if (buildVal == -1) {
        return -1;
      } else if (buildVal == -2) {
        return position;
      }
  }
  return position;
}

bool RTCPSender::ShouldSendReportBlocks(uint32_t rtcp_packet_type) const {
  return Status() == kRtcpCompound ||
      (rtcp_packet_type & kRtcpReport) ||
      (rtcp_packet_type & kRtcpSr) ||
      (rtcp_packet_type & kRtcpRr);
}

bool RTCPSender::PrepareReport(const FeedbackState& feedback_state,
                               StreamStatistician* statistician,
                               RTCPReportBlock* report_block,
                               uint32_t* ntp_secs, uint32_t* ntp_frac) {
  // Do we have receive statistics to send?
  RtcpStatistics stats;
  if (!statistician->GetStatistics(&stats, true))
    return false;
  report_block->fractionLost = stats.fraction_lost;
  report_block->cumulativeLost = stats.cumulative_lost;
  report_block->extendedHighSeqNum =
      stats.extended_max_sequence_number;
  report_block->jitter = stats.jitter;

  // get our NTP as late as possible to avoid a race
  _clock->CurrentNtp(*ntp_secs, *ntp_frac);

  // Delay since last received report
  uint32_t delaySinceLastReceivedSR = 0;
  if ((feedback_state.last_rr_ntp_secs != 0) ||
      (feedback_state.last_rr_ntp_frac != 0)) {
    // get the 16 lowest bits of seconds and the 16 higest bits of fractions
    uint32_t now=*ntp_secs&0x0000FFFF;
    now <<=16;
    now += (*ntp_frac&0xffff0000)>>16;

    uint32_t receiveTime = feedback_state.last_rr_ntp_secs&0x0000FFFF;
    receiveTime <<=16;
    receiveTime += (feedback_state.last_rr_ntp_frac&0xffff0000)>>16;

    delaySinceLastReceivedSR = now-receiveTime;
  }
  report_block->delaySinceLastSR = delaySinceLastReceivedSR;
  report_block->lastSR = feedback_state.remote_sr;
  return true;
}

int32_t
RTCPSender::SendToNetwork(const uint8_t* dataBuffer,
                          const uint16_t length)
{
    CriticalSectionScoped lock(_criticalSectionTransport);
    if(_cbTransport)
    {
        if(_cbTransport->SendRTCPPacket(_id, dataBuffer, length) > 0)
        {
            return 0;
        }
    }
    return -1;
}

int32_t
RTCPSender::SetCSRCStatus(const bool include)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _includeCSRCs = include;
    return 0;
}

int32_t
RTCPSender::SetCSRCs(const uint32_t arrOfCSRC[kRtpCsrcSize],
                    const uint8_t arrLength)
{
    assert(arrLength <= kRtpCsrcSize);
    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    for(int i = 0; i < arrLength;i++)
    {
        _CSRC[i] = arrOfCSRC[i];
    }
    _CSRCs = arrLength;
    return 0;
}

int32_t
RTCPSender::SetApplicationSpecificData(const uint8_t subType,
                                       const uint32_t name,
                                       const uint8_t* data,
                                       const uint16_t length)
{
    if(length %4 != 0)
    {
        LOG(LS_ERROR) << "Failed to SetApplicationSpecificData.";
        return -1;
    }
    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    if(_appData)
    {
        delete [] _appData;
    }

    _appSend = true;
    _appSubType = subType;
    _appName = name;
    _appData = new uint8_t[length];
    _appLength = length;
    memcpy(_appData, data, length);
    return 0;
}

int32_t
RTCPSender::SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    memcpy(&_xrVoIPMetric, VoIPMetric, sizeof(RTCPVoIPMetric));

    _xrSendVoIPMetric = true;
    return 0;
}

void RTCPSender::SendRtcpXrReceiverReferenceTime(bool enable) {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  xrSendReceiverReferenceTimeEnabled_ = enable;
}

bool RTCPSender::RtcpXrReceiverReferenceTime() const {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  return xrSendReceiverReferenceTimeEnabled_;
}

// called under critsect _criticalSectionRTCPSender
int32_t RTCPSender::WriteAllReportBlocksToBuffer(
    uint8_t* rtcpbuffer,
    int pos,
    uint8_t& numberOfReportBlocks,
    const uint32_t NTPsec,
    const uint32_t NTPfrac) {
  numberOfReportBlocks = external_report_blocks_.size();
  numberOfReportBlocks += internal_report_blocks_.size();
  if ((pos + numberOfReportBlocks * 24) >= IP_PACKET_SIZE) {
    LOG(LS_WARNING) << "Can't fit all report blocks.";
    return -1;
  }
  pos = WriteReportBlocksToBuffer(rtcpbuffer, pos, internal_report_blocks_);
  while (!internal_report_blocks_.empty()) {
    delete internal_report_blocks_.begin()->second;
    internal_report_blocks_.erase(internal_report_blocks_.begin());
  }
  pos = WriteReportBlocksToBuffer(rtcpbuffer, pos, external_report_blocks_);
  return pos;
}

int32_t RTCPSender::WriteReportBlocksToBuffer(
    uint8_t* rtcpbuffer,
    int32_t position,
    const std::map<uint32_t, RTCPReportBlock*>& report_blocks) {
  std::map<uint32_t, RTCPReportBlock*>::const_iterator it =
      report_blocks.begin();
  for (; it != report_blocks.end(); it++) {
    uint32_t remoteSSRC = it->first;
    RTCPReportBlock* reportBlock = it->second;
    if (reportBlock) {
      // Remote SSRC
      RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + position, remoteSSRC);
      position += 4;

      // fraction lost
      rtcpbuffer[position++] = reportBlock->fractionLost;

      // cumulative loss
      RtpUtility::AssignUWord24ToBuffer(rtcpbuffer + position,
                                        reportBlock->cumulativeLost);
      position += 3;

      // extended highest seq_no, contain the highest sequence number received
      RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + position,
                                        reportBlock->extendedHighSeqNum);
      position += 4;

      // Jitter
      RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + position,
                                        reportBlock->jitter);
      position += 4;

      RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + position,
                                        reportBlock->lastSR);
      position += 4;

      RtpUtility::AssignUWord32ToBuffer(rtcpbuffer + position,
                                        reportBlock->delaySinceLastSR);
      position += 4;
    }
  }
  return position;
}

// no callbacks allowed inside this function
int32_t
RTCPSender::SetTMMBN(const TMMBRSet* boundingSet,
                     const uint32_t maxBitrateKbit)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    if (0 == _tmmbrHelp.SetTMMBRBoundingSetToSend(boundingSet, maxBitrateKbit))
    {
        _sendTMMBN = true;
        return 0;
    }
    return -1;
}
}  // namespace webrtc
