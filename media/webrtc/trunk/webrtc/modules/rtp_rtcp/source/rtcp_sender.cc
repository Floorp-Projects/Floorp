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

#include <algorithm>  // min
#include <cassert>  // assert
#include <cstdlib>  // rand
#include <string.h>  // memcpy

#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/system_wrappers/interface/trace_event.h"

namespace webrtc {

using RTCPUtility::RTCPCnameInformation;

NACKStringBuilder::NACKStringBuilder() :
    _stream(""), _count(0), _consecutive(false)
{
    // Empty.
}

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

RTCPSender::RTCPSender(const int32_t id,
                       const bool audio,
                       Clock* clock,
                       ModuleRtpRtcpImpl* owner) :
    _id(id),
    _audio(audio),
    _clock(clock),
    _method(kRtcpOff),
    _rtpRtcp(*owner),
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
    _reportBlocks(),
    _csrcCNAMEs(),

    _cameraDelayMS(0),

    _lastSendReport(),
    _lastRTCPTime(),

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
    _xrSendVoIPMetric(false),
    _xrVoIPMetric(),
    _nackCount(0),
    _pliCount(0),
    _fullIntraRequestCount(0)
{
    memset(_CNAME, 0, sizeof(_CNAME));
    memset(_lastSendReport, 0, sizeof(_lastSendReport));
    memset(_lastRTCPTime, 0, sizeof(_lastRTCPTime));

    WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

RTCPSender::~RTCPSender() {
  delete [] _rembSSRC;
  delete [] _appData;

  while (!_reportBlocks.empty()) {
    std::map<uint32_t, RTCPReportBlock*>::iterator it =
        _reportBlocks.begin();
    delete it->second;
    _reportBlocks.erase(it);
  }
  while (!_csrcCNAMEs.empty()) {
    std::map<uint32_t, RTCPCnameInformation*>::iterator it =
        _csrcCNAMEs.begin();
    delete it->second;
    _csrcCNAMEs.erase(it);
  }
  delete _criticalSectionTransport;
  delete _criticalSectionRTCPSender;

  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, _id, "%s deleted", __FUNCTION__);
}

int32_t
RTCPSender::Init()
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    _method = kRtcpOff;
    _cbTransport = NULL;
    _usingNack = false;
    _sending = false;
    _sendTMMBN = false;
    _TMMBR = false;
    _IJ = false;
    _REMB = false;
    _sendREMB = false;
    last_rtp_timestamp_ = 0;
    last_frame_capture_time_ms_ = -1;
    start_timestamp_ = -1;
    _SSRC = 0;
    _remoteSSRC = 0;
    _cameraDelayMS = 0;
    _sequenceNumberFIR = 0;
    _tmmbr_Send = 0;
    _packetOH_Send = 0;
    _nextTimeToSendRTCP = 0;
    _CSRCs = 0;
    _appSend = false;
    _appSubType = 0;

    if(_appData)
    {
        delete [] _appData;
        _appData = NULL;
    }
    _appLength = 0;

    _xrSendVoIPMetric = false;

    memset(&_xrVoIPMetric, 0, sizeof(_xrVoIPMetric));
    memset(_CNAME, 0, sizeof(_CNAME));
    memset(_lastSendReport, 0, sizeof(_lastSendReport));
    memset(_lastRTCPTime, 0, sizeof(_lastRTCPTime));

    _nackCount = 0;
    _pliCount = 0;
    _fullIntraRequestCount = 0;

    return 0;
}

void
RTCPSender::ChangeUniqueId(const int32_t id)
{
    _id = id;
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
RTCPSender::SetSendingStatus(const bool sending)
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
        return SendRTCP(kRtcpBye);
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

int32_t
RTCPSender::SetRemoteSSRC( const uint32_t ssrc)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _remoteSSRC = ssrc;
    return 0;
}

int32_t
RTCPSender::SetCameraDelay(const int32_t delayMS)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    if(delayMS > 1000 || delayMS < -1000)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument, delay can't be larger than 1 sec", __FUNCTION__);
        return -1;
    }
    _cameraDelayMS = delayMS;
    return 0;
}

int32_t RTCPSender::CNAME(char cName[RTCP_CNAME_SIZE]) {
  assert(cName);
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  cName[RTCP_CNAME_SIZE - 1] = 0;
  strncpy(cName, _CNAME, RTCP_CNAME_SIZE - 1);
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
        technicaly we break the max 5% RTCP BW for video below 10 kbit/s but that should be extreamly rare


From RFC 3550

    MAX RTCP BW is 5% if the session BW
        A send report is approximately 65 bytes inc CNAME
        A report report is approximately 28 bytes

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

    if(now > _nextTimeToSendRTCP)
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

uint32_t
RTCPSender::SendTimeOfSendReport(const uint32_t sendReport)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    // This is only saved when we are the sender
    if((_lastSendReport[0] == 0) || (sendReport == 0))
    {
        return 0; // will be ignored
    } else
    {
        for(int i = 0; i < RTCP_NUMBER_OF_SR; ++i)
        {
            if( _lastSendReport[i] == sendReport)
            {
                return _lastRTCPTime[i];
            }
        }
    }
    return 0;
}

int32_t RTCPSender::AddReportBlock(const uint32_t SSRC,
                                   const RTCPReportBlock* reportBlock) {
  if (reportBlock == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s invalid argument", __FUNCTION__);
    return -1;
  }
  CriticalSectionScoped lock(_criticalSectionRTCPSender);

  if (_reportBlocks.size() >= RTCP_MAX_REPORT_BLOCKS) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s invalid argument", __FUNCTION__);
    return -1;
  }
  std::map<uint32_t, RTCPReportBlock*>::iterator it =
      _reportBlocks.find(SSRC);
  if (it != _reportBlocks.end()) {
    delete it->second;
    _reportBlocks.erase(it);
  }
  RTCPReportBlock* copyReportBlock = new RTCPReportBlock();
  memcpy(copyReportBlock, reportBlock, sizeof(RTCPReportBlock));
  _reportBlocks[SSRC] = copyReportBlock;
  return 0;
}

int32_t RTCPSender::RemoveReportBlock(const uint32_t SSRC) {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);

  std::map<uint32_t, RTCPReportBlock*>::iterator it =
      _reportBlocks.find(SSRC);

  if (it == _reportBlocks.end()) {
    return -1;
  }
  delete it->second;
  _reportBlocks.erase(it);
  return 0;
}

int32_t
RTCPSender::BuildSR(uint8_t* rtcpbuffer,
                    uint32_t& pos,
                    const uint32_t NTPsec,
                    const uint32_t NTPfrac,
                    const RTCPReportBlock* received)
{
    // sanity
    if(pos + 52 >= IP_PACKET_SIZE)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
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
    }

    _lastRTCPTime[0] = Clock::NtpToMs(NTPsec, NTPfrac);
    _lastSendReport[0] = (NTPsec << 16) + (NTPfrac >> 16);

    uint32_t freqHz = 90000; // For video
    if(_audio) {
      freqHz =  _rtpRtcp.CurrentSendFrequencyHz();
    }

    // The timestamp of this RTCP packet should be estimated as the timestamp of
    // the frame being captured at this moment. We are calculating that
    // timestamp as the last frame's timestamp + the time since the last frame
    // was captured.
    {
      // Needs protection since this method is called on the process thread.
      CriticalSectionScoped lock(_criticalSectionRTCPSender);
      RTPtime = start_timestamp_ + last_rtp_timestamp_ + (
          _clock->TimeInMilliseconds() - last_frame_capture_time_ms_) *
          (freqHz / 1000);
    }

    // Add sender data
    // Save  for our length field
    pos++;
    pos++;

    // Add our own SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;
    // NTP
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, NTPsec);
    pos += 4;
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, NTPfrac);
    pos += 4;
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, RTPtime);
    pos += 4;

    //sender's packet count
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _rtpRtcp.PacketCountSent());
    pos += 4;

    //sender's octet count
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _rtpRtcp.ByteCountSent());
    pos += 4;

    uint8_t numberOfReportBlocks = 0;
    int32_t retVal = AddReportBlocks(rtcpbuffer, pos, numberOfReportBlocks, received, NTPsec, NTPfrac);
    if(retVal < 0)
    {
        //
        return retVal ;
    }
    rtcpbuffer[posNumberOfReportBlocks] += numberOfReportBlocks;

    uint16_t len = uint16_t((pos/4) -1);
    ModuleRTPUtility::AssignUWord16ToBuffer(rtcpbuffer+2, len);
    return 0;
}


int32_t RTCPSender::BuildSDEC(uint8_t* rtcpbuffer,
                              uint32_t& pos) {
  size_t lengthCname = strlen(_CNAME);
  assert(lengthCname < RTCP_CNAME_SIZE);

  // sanity
  if(pos + 12 + lengthCname  >= IP_PACKET_SIZE) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s invalid argument", __FUNCTION__);
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
  ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
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
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, SSRC);
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
  ModuleRTPUtility::AssignUWord16ToBuffer(rtcpbuffer + SDESLengthPos,
                                          buffer_length);
  return 0;
}

int32_t
RTCPSender::BuildRR(uint8_t* rtcpbuffer,
                    uint32_t& pos,
                    const uint32_t NTPsec,
                    const uint32_t NTPfrac,
                    const RTCPReportBlock* received)
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
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    uint8_t numberOfReportBlocks = 0;
    int32_t retVal = AddReportBlocks(rtcpbuffer, pos, numberOfReportBlocks, received, NTPsec, NTPfrac);
    if(retVal < 0)
    {
        return retVal;
    }
    rtcpbuffer[posNumberOfReportBlocks] += numberOfReportBlocks;

    uint16_t len = uint16_t((pos)/4 -1);
    ModuleRTPUtility::AssignUWord16ToBuffer(rtcpbuffer+2, len);
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
    uint32_t& pos,
    const uint32_t jitterTransmissionTimeOffset)
{
    if (_reportBlocks.size() > 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "Not implemented.");
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
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer + pos,
                                            jitterTransmissionTimeOffset);
    pos += 4;
    return 0;
}

int32_t
RTCPSender::BuildPLI(uint8_t* rtcpbuffer, uint32_t& pos)
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
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
    pos += 4;
    return 0;
}

int32_t RTCPSender::BuildFIR(uint8_t* rtcpbuffer,
                             uint32_t& pos,
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
  ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
  pos += 4;

  // RFC 5104     4.3.1.2.  Semantics
  // SSRC of media source
  rtcpbuffer[pos++] = (uint8_t)0;
  rtcpbuffer[pos++] = (uint8_t)0;
  rtcpbuffer[pos++] = (uint8_t)0;
  rtcpbuffer[pos++] = (uint8_t)0;

  // Additional Feedback Control Information (FCI)
  ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _remoteSSRC);
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
RTCPSender::BuildSLI(uint8_t* rtcpbuffer, uint32_t& pos, const uint8_t pictureID)
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
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
    pos += 4;

    // Add first, number & picture ID 6 bits
    // first  = 0, 13 - bits
    // number = 0x1fff, 13 - bits only ones for now
    uint32_t sliField = (0x1fff << 6)+ (0x3f & pictureID);
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, sliField);
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
                     uint32_t& pos,
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
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
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
RTCPSender::BuildREMB(uint8_t* rtcpbuffer, uint32_t& pos)
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
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Remote SSRC must be 0
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, 0);
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
        ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _rembSSRC[i]);
        pos += 4;
    }
    TRACE_COUNTER_ID1("webrtc_rtp", "RTCPRembBitrate", _SSRC, _rembBitrate);
    return 0;
}

void
RTCPSender::SetTargetBitrate(unsigned int target_bitrate)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _tmmbr_Send = target_bitrate / 1000;
}

int32_t
RTCPSender::BuildTMMBR(uint8_t* rtcpbuffer, uint32_t& pos)
{
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
    int32_t lengthOfBoundingSet
        = _rtpRtcp.BoundingSet(tmmbrOwner, candidateSet);

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
        ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
        pos += 4;

        // RFC 5104     4.2.1.2.  Semantics

        // SSRC of media source
        rtcpbuffer[pos++]=(uint8_t)0;
        rtcpbuffer[pos++]=(uint8_t)0;
        rtcpbuffer[pos++]=(uint8_t)0;
        rtcpbuffer[pos++]=(uint8_t)0;

        // Additional Feedback Control Information (FCI)
        ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
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
RTCPSender::BuildTMMBN(uint8_t* rtcpbuffer, uint32_t& pos)
{
    TMMBRSet* boundingSet = _tmmbrHelp.BoundingSetToSend();
    if(boundingSet == NULL)
    {
        return -1;
    }
    // sanity
    if(pos + 12 + boundingSet->lengthOfSet()*8 >= IP_PACKET_SIZE)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
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
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
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
            ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, tmmbrSSRC);
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
RTCPSender::BuildAPP(uint8_t* rtcpbuffer, uint32_t& pos)
{
    // sanity
    if(_appData == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "%s invalid state", __FUNCTION__);
        return -1;
    }
    if(pos + 12 + _appLength >= IP_PACKET_SIZE)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -2;
    }
    rtcpbuffer[pos++]=(uint8_t)0x80 + _appSubType;

    // Add APP ID
    rtcpbuffer[pos++]=(uint8_t)204;

    uint16_t length = (_appLength>>2) + 2; // include SSRC and name
    rtcpbuffer[pos++]=(uint8_t)(length>>8);
    rtcpbuffer[pos++]=(uint8_t)(length);

    // Add our own SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Add our application name
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _appName);
    pos += 4;

    // Add the data
    memcpy(rtcpbuffer +pos, _appData,_appLength);
    pos += _appLength;
    return 0;
}

int32_t
RTCPSender::BuildNACK(uint8_t* rtcpbuffer,
                      uint32_t& pos,
                      const int32_t nackSize,
                      const uint16_t* nackList,
                      std::string* nackString)
{
    // sanity
    if(pos + 16 >= IP_PACKET_SIZE)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
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
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
    pos += 4;

    NACKStringBuilder stringBuilder;
    // Build NACK bitmasks and write them to the RTCP message.
    // The nack list should be sorted and not contain duplicates if one
    // wants to build the smallest rtcp nack packet.
    int numOfNackFields = 0;
    int maxNackFields = std::min<int>(kRtcpMaxNackFields,
                                      (IP_PACKET_SIZE - pos) / 4);
    int i = 0;
    while (i < nackSize && numOfNackFields < maxNackFields) {
      stringBuilder.PushNACK(nackList[i]);
      uint16_t nack = nackList[i++];
      uint16_t bitmask = 0;
      while (i < nackSize) {
        int shift = static_cast<uint16_t>(nackList[i] - nack) - 1;
        if (shift >= 0 && shift <= 15) {
          stringBuilder.PushNACK(nackList[i]);
          bitmask |= (1 << shift);
          ++i;
        } else {
          break;
        }
      }
      // Write the sequence number and the bitmask to the packet.
      assert(pos + 4 < IP_PACKET_SIZE);
      ModuleRTPUtility::AssignUWord16ToBuffer(rtcpbuffer + pos, nack);
      pos += 2;
      ModuleRTPUtility::AssignUWord16ToBuffer(rtcpbuffer + pos, bitmask);
      pos += 2;
      numOfNackFields++;
    }
    if (i != nackSize) {
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                   "Nack list to large for one packet.");
    }
    rtcpbuffer[nackSizePos] = static_cast<uint8_t>(2 + numOfNackFields);
    *nackString = stringBuilder.GetResult();
    return 0;
}

int32_t
RTCPSender::BuildBYE(uint8_t* rtcpbuffer, uint32_t& pos)
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
        ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
        pos += 4;

        // add CSRCs
        for(int i = 0; i < _CSRCs; i++)
        {
            ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _CSRC[i]);
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
        ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
        pos += 4;
    }
    return 0;
}

int32_t
RTCPSender::BuildVoIPMetric(uint8_t* rtcpbuffer, uint32_t& pos)
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
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Add a VoIP metrics block
    rtcpbuffer[pos++]=7;
    rtcpbuffer[pos++]=0;
    rtcpbuffer[pos++]=0;
    rtcpbuffer[pos++]=8;

    // Add the remote SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
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

int32_t
RTCPSender::SendRTCP(const uint32_t packetTypeFlags,
                     const int32_t nackSize,       // NACK
                     const uint16_t* nackList,     // NACK
                     const bool repeat,                  // FIR
                     const uint64_t pictureID)     // SLI & RPSI
{
    uint32_t rtcpPacketTypeFlags = packetTypeFlags;
    uint32_t pos = 0;
    uint8_t rtcpbuffer[IP_PACKET_SIZE];

    do  // only to be able to use break :) (and the critsect must be inside its own scope)
    {
        // collect the received information
        RTCPReportBlock received;
        bool hasReceived = false;
        uint32_t NTPsec = 0;
        uint32_t NTPfrac = 0;
        bool rtcpCompound = false;
        uint32_t jitterTransmissionOffset = 0;

        {
          CriticalSectionScoped lock(_criticalSectionRTCPSender);
          if(_method == kRtcpOff)
          {
              WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                           "%s invalid state", __FUNCTION__);
              return -1;
          }
          rtcpCompound = (_method == kRtcpCompound) ? true : false;
        }

        if (rtcpCompound ||
            rtcpPacketTypeFlags & kRtcpReport ||
            rtcpPacketTypeFlags & kRtcpSr ||
            rtcpPacketTypeFlags & kRtcpRr)
        {
            // get statistics from our RTPreceiver outside critsect
            if(_rtpRtcp.ReportBlockStatistics(&received.fractionLost,
                                              &received.cumulativeLost,
                                              &received.extendedHighSeqNum,
                                              &received.jitter,
                                              &jitterTransmissionOffset) == 0)
            {
                hasReceived = true;

                uint32_t lastReceivedRRNTPsecs = 0;
                uint32_t lastReceivedRRNTPfrac = 0;
                uint32_t remoteSR = 0;

                // ok even if we have not received a SR, we will send 0 in that case
                _rtpRtcp.LastReceivedNTP(lastReceivedRRNTPsecs,
                                         lastReceivedRRNTPfrac,
                                         remoteSR);

                // get our NTP as late as possible to avoid a race
                _clock->CurrentNtp(NTPsec, NTPfrac);

                // Delay since last received report
                uint32_t delaySinceLastReceivedSR = 0;
                if((lastReceivedRRNTPsecs !=0) || (lastReceivedRRNTPfrac !=0))
                {
                    // get the 16 lowest bits of seconds and the 16 higest bits of fractions
                    uint32_t now=NTPsec&0x0000FFFF;
                    now <<=16;
                    now += (NTPfrac&0xffff0000)>>16;

                    uint32_t receiveTime = lastReceivedRRNTPsecs&0x0000FFFF;
                    receiveTime <<=16;
                    receiveTime += (lastReceivedRRNTPfrac&0xffff0000)>>16;

                    delaySinceLastReceivedSR = now-receiveTime;
                }
                received.delaySinceLastSR = delaySinceLastReceivedSR;
                received.lastSR = remoteSR;
            } else
            {
                // we need to send our NTP even if we dont have received any reports
                _clock->CurrentNtp(NTPsec, NTPfrac);
            }
        }

        CriticalSectionScoped lock(_criticalSectionRTCPSender);

        if(_TMMBR ) // attach TMMBR to send and receive reports
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
        if(_sendTMMBN)  // set when having received a TMMBR
        {
            rtcpPacketTypeFlags |= kRtcpTmmbn;
            _sendTMMBN = false;
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
            if (_IJ && hasReceived)
            {
                rtcpPacketTypeFlags |= kRtcpTransmissionTimeOffset;
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
                timeToNext = (RTCP_INTERVAL_AUDIO_MS/2) + (RTCP_INTERVAL_AUDIO_MS*random/1000);
            }else
            {
                uint32_t minIntervalMs = RTCP_INTERVAL_AUDIO_MS;
                if(_sending)
                {
                    // calc bw for video 360/sendBW in kbit/s
                    uint32_t sendBitrateKbit = 0;
                    uint32_t videoRate = 0;
                    uint32_t fecRate = 0;
                    uint32_t nackRate = 0;
                    _rtpRtcp.BitrateSent(&sendBitrateKbit,
                                         &videoRate,
                                         &fecRate,
                                         &nackRate);
                    sendBitrateKbit /= 1000;
                    if(sendBitrateKbit != 0)
                    {
                        minIntervalMs = 360000/sendBitrateKbit;
                    }
                }
                if(minIntervalMs > RTCP_INTERVAL_VIDEO_MS)
                {
                    minIntervalMs = RTCP_INTERVAL_VIDEO_MS;
                }
                timeToNext = (minIntervalMs/2) + (minIntervalMs*random/1000);
            }
            _nextTimeToSendRTCP = _clock->TimeInMilliseconds() + timeToNext;
        }

        // if the data does not fitt in the packet we fill it as much as possible
        int32_t buildVal = 0;

        if(rtcpPacketTypeFlags & kRtcpSr)
        {
            if(hasReceived)
            {
                buildVal = BuildSR(rtcpbuffer, pos, NTPsec, NTPfrac, &received);
            } else
            {
                buildVal = BuildSR(rtcpbuffer, pos, NTPsec, NTPfrac);
            }
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
            buildVal = BuildSDEC(rtcpbuffer, pos);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }

        }else if(rtcpPacketTypeFlags & kRtcpRr)
        {
            if(hasReceived)
            {
                buildVal = BuildRR(rtcpbuffer, pos, NTPsec, NTPfrac,&received);
            }else
            {
                buildVal = BuildRR(rtcpbuffer, pos, NTPsec, NTPfrac);
            }
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
            // only of set
            if(_CNAME[0] != 0)
            {
                buildVal = BuildSDEC(rtcpbuffer, pos);
                if(buildVal == -1)
                {
                    return -1; // error
                }
            }
        }
        if(rtcpPacketTypeFlags & kRtcpTransmissionTimeOffset)
        {
            // If present, this RTCP packet must be placed after a
            // receiver report.
            buildVal = BuildExtendedJitterReport(rtcpbuffer,
                                                 pos,
                                                 jitterTransmissionOffset);
            if(buildVal == -1)
            {
                return -1; // error
            }
            else if(buildVal == -2)
            {
                break;  // out of buffer
            }
        }
        if(rtcpPacketTypeFlags & kRtcpPli)
        {
            buildVal = BuildPLI(rtcpbuffer, pos);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
            TRACE_EVENT_INSTANT0("webrtc_rtp", "RTCPSender::PLI");
            _pliCount++;
            TRACE_COUNTER_ID1("webrtc_rtp", "RTCP_PLICount", _SSRC, _pliCount);
        }
        if(rtcpPacketTypeFlags & kRtcpFir)
        {
            buildVal = BuildFIR(rtcpbuffer, pos, repeat);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
            TRACE_EVENT_INSTANT0("webrtc_rtp", "RTCPSender::FIR");
            _fullIntraRequestCount++;
            TRACE_COUNTER_ID1("webrtc_rtp", "RTCP_FIRCount", _SSRC,
                              _fullIntraRequestCount);
        }
        if(rtcpPacketTypeFlags & kRtcpSli)
        {
            buildVal = BuildSLI(rtcpbuffer, pos, (uint8_t)pictureID);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
        }
        if(rtcpPacketTypeFlags & kRtcpRpsi)
        {
            const int8_t payloadType = _rtpRtcp.SendPayloadType();
            if(payloadType == -1)
            {
                return -1;
            }
            buildVal = BuildRPSI(rtcpbuffer, pos, pictureID, (uint8_t)payloadType);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
        }
        if(rtcpPacketTypeFlags & kRtcpRemb)
        {
            buildVal = BuildREMB(rtcpbuffer, pos);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
            TRACE_EVENT_INSTANT0("webrtc_rtp", "RTCPSender::REMB");
        }
        if(rtcpPacketTypeFlags & kRtcpBye)
        {
            buildVal = BuildBYE(rtcpbuffer, pos);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
        }
        if(rtcpPacketTypeFlags & kRtcpApp)
        {
            buildVal = BuildAPP(rtcpbuffer, pos);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
        }
        if(rtcpPacketTypeFlags & kRtcpTmmbr)
        {
            buildVal = BuildTMMBR(rtcpbuffer, pos);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
        }
        if(rtcpPacketTypeFlags & kRtcpTmmbn)
        {
            buildVal = BuildTMMBN(rtcpbuffer, pos);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
        }
        if(rtcpPacketTypeFlags & kRtcpNack)
        {
            std::string nackString;
            buildVal = BuildNACK(rtcpbuffer, pos, nackSize, nackList,
                                 &nackString);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
            TRACE_EVENT_INSTANT1("webrtc_rtp", "RTCPSender::NACK",
                                 "nacks", TRACE_STR_COPY(nackString.c_str()));
            _nackCount++;
            TRACE_COUNTER_ID1("webrtc_rtp", "RTCP_NACKCount", _SSRC, _nackCount);
        }
        if(rtcpPacketTypeFlags & kRtcpXrVoipMetric)
        {
            buildVal = BuildVoIPMetric(rtcpbuffer, pos);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
        }
    }while (false);
    // Sanity don't send empty packets.
    if (pos == 0)
    {
        return -1;
    }
    return SendToNetwork(rtcpbuffer, (uint16_t)pos);
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
    _includeCSRCs = include;
    return 0;
}

int32_t
RTCPSender::SetCSRCs(const uint32_t arrOfCSRC[kRtpCsrcSize],
                    const uint8_t arrLength)
{
    if(arrLength > kRtpCsrcSize)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        assert(false);
        return -1;
    }

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
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
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

// called under critsect _criticalSectionRTCPSender
int32_t RTCPSender::AddReportBlocks(uint8_t* rtcpbuffer,
                                    uint32_t& pos,
                                    uint8_t& numberOfReportBlocks,
                                    const RTCPReportBlock* received,
                                    const uint32_t NTPsec,
                                    const uint32_t NTPfrac) {
  // sanity one block
  if(pos + 24 >= IP_PACKET_SIZE) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s invalid argument", __FUNCTION__);
    return -1;
  }
  numberOfReportBlocks = _reportBlocks.size();
  if (received) {
    // add our multiple RR to numberOfReportBlocks
    numberOfReportBlocks++;
  }
  if (received) {
    // answer to the one that sends to me
    _lastRTCPTime[0] = Clock::NtpToMs(NTPsec, NTPfrac);

    // Remote SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
    pos += 4;

    // fraction lost
    rtcpbuffer[pos++]=received->fractionLost;

    // cumulative loss
    ModuleRTPUtility::AssignUWord24ToBuffer(rtcpbuffer+pos,
                                            received->cumulativeLost);
    pos += 3;
    // extended highest seq_no, contain the highest sequence number received
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos,
                                            received->extendedHighSeqNum);
    pos += 4;

    //Jitter
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, received->jitter);
    pos += 4;

    // Last SR timestamp, our NTP time when we received the last report
    // This is the value that we read from the send report packet not when we
    // received it...
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, received->lastSR);
    pos += 4;

    // Delay since last received report,time since we received the report
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos,
                                            received->delaySinceLastSR);
    pos += 4;
  }
  if ((pos + _reportBlocks.size() * 24) >= IP_PACKET_SIZE) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s invalid argument", __FUNCTION__);
    return -1;
  }
  std::map<uint32_t, RTCPReportBlock*>::iterator it =
      _reportBlocks.begin();

  for (; it != _reportBlocks.end(); it++) {
    // we can have multiple report block in a conference
    uint32_t remoteSSRC = it->first;
    RTCPReportBlock* reportBlock = it->second;
    if (reportBlock) {
      // Remote SSRC
      ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, remoteSSRC);
      pos += 4;

      // fraction lost
      rtcpbuffer[pos++] = reportBlock->fractionLost;

      // cumulative loss
      ModuleRTPUtility::AssignUWord24ToBuffer(rtcpbuffer+pos,
                                              reportBlock->cumulativeLost);
      pos += 3;

      // extended highest seq_no, contain the highest sequence number received
      ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos,
                                              reportBlock->extendedHighSeqNum);
      pos += 4;

      //Jitter
      ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos,
                                              reportBlock->jitter);
      pos += 4;

      ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos,
                                              reportBlock->lastSR);
      pos += 4;

      ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos,
                                              reportBlock->delaySinceLastSR);
      pos += 4;
    }
  }
  return pos;
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
} // namespace webrtc
