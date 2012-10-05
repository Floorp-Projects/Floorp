/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtcp_sender.h"

#include <cassert>  // assert
#include <cstdlib>  // rand
#include <string.h>  // memcpy

#include "common_types.h"
#include "modules/remote_bitrate_estimator/remote_rate_control.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

using RTCPUtility::RTCPCnameInformation;

RTCPSender::RTCPSender(const WebRtc_Word32 id,
                       const bool audio,
                       RtpRtcpClock* clock,
                       ModuleRtpRtcpImpl* owner) :
    _id(id),
    _audio(audio),
    _clock(*clock),
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
    _xrVoIPMetric()
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
    std::map<WebRtc_UWord32, RTCPReportBlock*>::iterator it =
        _reportBlocks.begin();
    delete it->second;
    _reportBlocks.erase(it);
  }
  while (!_csrcCNAMEs.empty()) {
    std::map<WebRtc_UWord32, RTCPCnameInformation*>::iterator it =
        _csrcCNAMEs.begin();
    delete it->second;
    _csrcCNAMEs.erase(it);
  }
  delete _criticalSectionTransport;
  delete _criticalSectionRTCPSender;

  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, _id, "%s deleted", __FUNCTION__);
}

WebRtc_Word32
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
    _SSRC = 0;
    _remoteSSRC = 0;
    _cameraDelayMS = 0;
    _sequenceNumberFIR = 0;
    _tmmbr_Send = 0;
    _packetOH_Send = 0;
    //_remoteRateControl.Reset();
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
    return 0;
}

void
RTCPSender::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
}

WebRtc_Word32
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

WebRtc_Word32
RTCPSender::SetRTCPStatus(const RTCPMethod method)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    if(method != kRtcpOff)
    {
        if(_audio)
        {
            _nextTimeToSendRTCP = _clock.GetTimeInMS() + (RTCP_INTERVAL_AUDIO_MS/2);
        } else
        {
            _nextTimeToSendRTCP = _clock.GetTimeInMS() + (RTCP_INTERVAL_VIDEO_MS/2);
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

WebRtc_Word32
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

WebRtc_Word32
RTCPSender::SetREMBStatus(const bool enable)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _REMB = enable;
    return 0;
}

WebRtc_Word32
RTCPSender::SetREMBData(const WebRtc_UWord32 bitrate,
                        const WebRtc_UWord8 numberOfSSRC,
                        const WebRtc_UWord32* SSRC)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _rembBitrate = bitrate;
 
    if(_sizeRembSSRC < numberOfSSRC)
    {
        delete [] _rembSSRC;
        _rembSSRC = new WebRtc_UWord32[numberOfSSRC];
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

WebRtc_Word32
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

WebRtc_Word32
RTCPSender::SetIJStatus(const bool enable)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _IJ = enable;
    return 0;
}

void
RTCPSender::SetSSRC( const WebRtc_UWord32 ssrc)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    if(_SSRC != 0)
    {
        // not first SetSSRC, probably due to a collision
        // schedule a new RTCP report
        // make sure that we send a RTP packet
        _nextTimeToSendRTCP = _clock.GetTimeInMS() + 100;
    }
    _SSRC = ssrc;
}

WebRtc_Word32
RTCPSender::SetRemoteSSRC( const WebRtc_UWord32 ssrc)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    _remoteSSRC = ssrc;
    //_remoteRateControl.Reset();
    return 0;
}

WebRtc_Word32
RTCPSender::SetCameraDelay(const WebRtc_Word32 delayMS)
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

WebRtc_Word32 RTCPSender::CNAME(char cName[RTCP_CNAME_SIZE]) {
  assert(cName);
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  cName[RTCP_CNAME_SIZE - 1] = 0;
  strncpy(cName, _CNAME, RTCP_CNAME_SIZE - 1);
  return 0;
}

WebRtc_Word32 RTCPSender::SetCNAME(const char cName[RTCP_CNAME_SIZE]) {
  if (!cName)
    return -1;

  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  _CNAME[RTCP_CNAME_SIZE - 1] = 0;
  strncpy(_CNAME, cName, RTCP_CNAME_SIZE - 1);
  return 0;
}

WebRtc_Word32 RTCPSender::AddMixedCNAME(const WebRtc_UWord32 SSRC,
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

WebRtc_Word32 RTCPSender::RemoveMixedCNAME(const WebRtc_UWord32 SSRC) {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);
  std::map<WebRtc_UWord32, RTCPCnameInformation*>::iterator it =
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

    WebRtc_Word64 now = _clock.GetTimeInMS();

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

WebRtc_UWord32
RTCPSender::LastSendReport( WebRtc_UWord32& lastRTCPTime)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);

    lastRTCPTime = _lastRTCPTime[0];
    return _lastSendReport[0];
}

WebRtc_UWord32
RTCPSender::SendTimeOfSendReport(const WebRtc_UWord32 sendReport)
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

WebRtc_Word32 RTCPSender::AddReportBlock(const WebRtc_UWord32 SSRC,
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
  RTCPReportBlock* copyReportBlock = new RTCPReportBlock();
  memcpy(copyReportBlock, reportBlock, sizeof(RTCPReportBlock));
  _reportBlocks[SSRC] = copyReportBlock;
  return 0;
}

WebRtc_Word32 RTCPSender::RemoveReportBlock(const WebRtc_UWord32 SSRC) {
  CriticalSectionScoped lock(_criticalSectionRTCPSender);

  std::map<WebRtc_UWord32, RTCPReportBlock*>::iterator it =
      _reportBlocks.find(SSRC);

  if (it == _reportBlocks.end()) {
    return -1;
  }
  delete it->second;
  _reportBlocks.erase(it);
  return 0;
}

WebRtc_Word32
RTCPSender::BuildSR(WebRtc_UWord8* rtcpbuffer,
                    WebRtc_UWord32& pos,
                    const WebRtc_UWord32 NTPsec,
                    const WebRtc_UWord32 NTPfrac,
                    const RTCPReportBlock* received)
{
    // sanity
    if(pos + 52 >= IP_PACKET_SIZE)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -2;
    }
    WebRtc_UWord32 RTPtime;
    WebRtc_UWord32 BackTimedNTPsec;
    WebRtc_UWord32 BackTimedNTPfrac;

    WebRtc_UWord32 posNumberOfReportBlocks = pos;
    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80;

    // Sender report
    rtcpbuffer[pos++]=(WebRtc_UWord8)200;

    for(int i = (RTCP_NUMBER_OF_SR-2); i >= 0; i--)
    {
        // shift old
        _lastSendReport[i+1] = _lastSendReport[i];
        _lastRTCPTime[i+1] =_lastRTCPTime[i];
    }

    _lastRTCPTime[0] = ModuleRTPUtility::ConvertNTPTimeToMS(NTPsec, NTPfrac); // before video cam compensation

    if(_cameraDelayMS >= 0)
    {
        // fraction of a second as an unsigned word32 4.294 967 296E9
        WebRtc_UWord32 cameraDelayFixFrac =  (WebRtc_UWord32)_cameraDelayMS* 4294967; // note camera delay can't be larger than +/-1000ms
        if(NTPfrac > cameraDelayFixFrac)
        {
            // no problem just reduce the fraction part
            BackTimedNTPfrac = NTPfrac - cameraDelayFixFrac;
            BackTimedNTPsec = NTPsec;
        } else
        {
            // we need to reduce the sec and add that sec to the frac
            BackTimedNTPsec = NTPsec - 1;
            BackTimedNTPfrac = 0xffffffff - (cameraDelayFixFrac - NTPfrac);
        }
    } else
    {
        // fraction of a second as an unsigned word32 4.294 967 296E9
        WebRtc_UWord32 cameraDelayFixFrac =  (WebRtc_UWord32)(-_cameraDelayMS)* 4294967; // note camera delay can't be larger than +/-1000ms
        if(NTPfrac > 0xffffffff - cameraDelayFixFrac)
        {
            // we need to add the sec and add that sec to the frac
            BackTimedNTPsec = NTPsec + 1;
            BackTimedNTPfrac = cameraDelayFixFrac + NTPfrac; // this will wrap but that is ok
        } else
        {
            // no problem just add the fraction part
            BackTimedNTPsec = NTPsec;
            BackTimedNTPfrac = NTPfrac + cameraDelayFixFrac;
        }
    }
    _lastSendReport[0] = (BackTimedNTPsec <<16) + (BackTimedNTPfrac >> 16);

    // RTP timestamp
    // This should have a ramdom start value added
    // RTP is counted from NTP not the acctual RTP
    // This reflects the perfect RTP time
    // we solve this by initiating RTP to our NTP :)

    WebRtc_UWord32 freqHz = 90000; // For video
    if(_audio)
    {
        freqHz =  _rtpRtcp.CurrentSendFrequencyHz();
        RTPtime = ModuleRTPUtility::GetCurrentRTP(&_clock, freqHz);
    }
    else // video 
    {
        // used to be (WebRtc_UWord32)(((float)BackTimedNTPfrac/(float)FRAC)* 90000)
        WebRtc_UWord32 tmp = 9*(BackTimedNTPfrac/429496);
        RTPtime = BackTimedNTPsec*freqHz + tmp;
    }

    
    

    // Add sender data
    // Save  for our length field
    pos++;
    pos++;

    // Add our own SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;
    // NTP
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, BackTimedNTPsec);
    pos += 4;
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, BackTimedNTPfrac);
    pos += 4;
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, RTPtime);
    pos += 4;

    //sender's packet count
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _rtpRtcp.PacketCountSent());
    pos += 4;

    //sender's octet count
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _rtpRtcp.ByteCountSent());
    pos += 4;

    WebRtc_UWord8 numberOfReportBlocks = 0;
    WebRtc_Word32 retVal = AddReportBlocks(rtcpbuffer, pos, numberOfReportBlocks, received, NTPsec, NTPfrac);
    if(retVal < 0)
    {
        //
        return retVal ;
    }
    rtcpbuffer[posNumberOfReportBlocks] += numberOfReportBlocks;

    WebRtc_UWord16 len = WebRtc_UWord16((pos/4) -1);
    ModuleRTPUtility::AssignUWord16ToBuffer(rtcpbuffer+2, len);
    return 0;
}


WebRtc_Word32 RTCPSender::BuildSDEC(WebRtc_UWord8* rtcpbuffer,
                                    WebRtc_UWord32& pos) {
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
  rtcpbuffer[pos++] = static_cast<WebRtc_UWord8>(0x80 + 1 + _csrcCNAMEs.size());
  rtcpbuffer[pos++] = static_cast<WebRtc_UWord8>(202);

  // handle SDES length later on
  WebRtc_UWord32 SDESLengthPos = pos;
  pos++;
  pos++;

  // Add our own SSRC
  ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
  pos += 4;

  // CNAME = 1
  rtcpbuffer[pos++] = static_cast<WebRtc_UWord8>(1);

  //
  rtcpbuffer[pos++] = static_cast<WebRtc_UWord8>(lengthCname);

  WebRtc_UWord16 SDESLength = 10;

  memcpy(&rtcpbuffer[pos], _CNAME, lengthCname);
  pos += lengthCname;
  SDESLength += (WebRtc_UWord16)lengthCname;

  WebRtc_UWord16 padding = 0;
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

  std::map<WebRtc_UWord32, RTCPUtility::RTCPCnameInformation*>::iterator it =
      _csrcCNAMEs.begin();

  for(; it != _csrcCNAMEs.end(); it++) {
    RTCPCnameInformation* cname = it->second;
    WebRtc_UWord32 SSRC = it->first;

    // Add SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, SSRC);
    pos += 4;

    // CNAME = 1
    rtcpbuffer[pos++] = static_cast<WebRtc_UWord8>(1);

    size_t length = strlen(cname->name);
    assert(length < RTCP_CNAME_SIZE);

    rtcpbuffer[pos++]= static_cast<WebRtc_UWord8>(length);
    SDESLength += 6;

    memcpy(&rtcpbuffer[pos],cname->name, length);

    pos += length;
    SDESLength += length;
    WebRtc_UWord16 padding = 0;

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
  WebRtc_UWord16 buffer_length = (SDESLength / 4) - 1;
  ModuleRTPUtility::AssignUWord16ToBuffer(rtcpbuffer + SDESLengthPos,
                                          buffer_length);
  return 0;
}

WebRtc_Word32
RTCPSender::BuildRR(WebRtc_UWord8* rtcpbuffer,
                    WebRtc_UWord32& pos,
                    const WebRtc_UWord32 NTPsec,
                    const WebRtc_UWord32 NTPfrac,
                    const RTCPReportBlock* received)
{
    // sanity one block
    if(pos + 32 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    WebRtc_UWord32 posNumberOfReportBlocks = pos;

    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80;
    rtcpbuffer[pos++]=(WebRtc_UWord8)201;

    // Save  for our length field
    pos++;
    pos++;

    // Add our own SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    WebRtc_UWord8 numberOfReportBlocks = 0;
    WebRtc_Word32 retVal = AddReportBlocks(rtcpbuffer, pos, numberOfReportBlocks, received, NTPsec, NTPfrac);
    if(retVal < 0)
    {
        return retVal;
    }
    rtcpbuffer[posNumberOfReportBlocks] += numberOfReportBlocks;

    WebRtc_UWord16 len = WebRtc_UWord16((pos)/4 -1);
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

WebRtc_Word32
RTCPSender::BuildExtendedJitterReport(
    WebRtc_UWord8* rtcpbuffer,
    WebRtc_UWord32& pos,
    const WebRtc_UWord32 jitterTransmissionTimeOffset)
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
    WebRtc_UWord8 RC = 1;
    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + RC;
    rtcpbuffer[pos++]=(WebRtc_UWord8)195;

    // Used fixed length of 2
    rtcpbuffer[pos++]=(WebRtc_UWord8)0;
    rtcpbuffer[pos++]=(WebRtc_UWord8)(1);

    // Add inter-arrival jitter
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer + pos,
                                            jitterTransmissionTimeOffset);
    pos += 4;
    return 0;
}

WebRtc_Word32
RTCPSender::BuildPLI(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos)
{
    // sanity
    if(pos + 12 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    // add picture loss indicator
    WebRtc_UWord8 FMT = 1;
    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + FMT;
    rtcpbuffer[pos++]=(WebRtc_UWord8)206;

    //Used fixed length of 2
    rtcpbuffer[pos++]=(WebRtc_UWord8)0;
    rtcpbuffer[pos++]=(WebRtc_UWord8)(2);

    // Add our own SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
    pos += 4;
    return 0;
}

WebRtc_Word32 RTCPSender::BuildFIR(WebRtc_UWord8* rtcpbuffer,
                                   WebRtc_UWord32& pos,
                                   bool repeat) {
  // sanity
  if(pos + 20 >= IP_PACKET_SIZE)  {
    return -2;
  }
  if (!repeat) {
    _sequenceNumberFIR++;   // do not increase if repetition
  }

  // add full intra request indicator
  WebRtc_UWord8 FMT = 4;
  rtcpbuffer[pos++] = (WebRtc_UWord8)0x80 + FMT;
  rtcpbuffer[pos++] = (WebRtc_UWord8)206;

  //Length of 4
  rtcpbuffer[pos++] = (WebRtc_UWord8)0;
  rtcpbuffer[pos++] = (WebRtc_UWord8)(4);

  // Add our own SSRC
  ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _SSRC);
  pos += 4;

  // RFC 5104     4.3.1.2.  Semantics
  // SSRC of media source
  rtcpbuffer[pos++] = (WebRtc_UWord8)0;
  rtcpbuffer[pos++] = (WebRtc_UWord8)0;
  rtcpbuffer[pos++] = (WebRtc_UWord8)0;
  rtcpbuffer[pos++] = (WebRtc_UWord8)0;

  // Additional Feedback Control Information (FCI)
  ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer + pos, _remoteSSRC);
  pos += 4;

  rtcpbuffer[pos++] = (WebRtc_UWord8)(_sequenceNumberFIR);
  rtcpbuffer[pos++] = (WebRtc_UWord8)0;
  rtcpbuffer[pos++] = (WebRtc_UWord8)0;
  rtcpbuffer[pos++] = (WebRtc_UWord8)0;
  return 0;
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |            First        |        Number           | PictureID |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
WebRtc_Word32
RTCPSender::BuildSLI(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos, const WebRtc_UWord8 pictureID)
{
    // sanity
    if(pos + 16 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    // add slice loss indicator
    WebRtc_UWord8 FMT = 2;
    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + FMT;
    rtcpbuffer[pos++]=(WebRtc_UWord8)206;

    //Used fixed length of 3
    rtcpbuffer[pos++]=(WebRtc_UWord8)0;
    rtcpbuffer[pos++]=(WebRtc_UWord8)(3);

    // Add our own SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
    pos += 4;

    // Add first, number & picture ID 6 bits
    // first  = 0, 13 - bits
    // number = 0x1fff, 13 - bits only ones for now
    WebRtc_UWord32 sliField = (0x1fff << 6)+ (0x3f & pictureID);
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
WebRtc_Word32
RTCPSender::BuildRPSI(WebRtc_UWord8* rtcpbuffer,
                     WebRtc_UWord32& pos,
                     const WebRtc_UWord64 pictureID,
                     const WebRtc_UWord8 payloadType)
{
    // sanity
    if(pos + 24 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    // add Reference Picture Selection Indication
    WebRtc_UWord8 FMT = 3;
    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + FMT;
    rtcpbuffer[pos++]=(WebRtc_UWord8)206;

    // calc length
    WebRtc_UWord32 bitsRequired = 7;
    WebRtc_UWord8 bytesRequired = 1;
    while((pictureID>>bitsRequired) > 0)
    {
        bitsRequired += 7;
        bytesRequired++;
    }

    WebRtc_UWord8 size = 3;
    if(bytesRequired > 6)
    {
        size = 5;
    } else if(bytesRequired > 2)
    {
        size = 4;
    }
    rtcpbuffer[pos++]=(WebRtc_UWord8)0;
    rtcpbuffer[pos++]=size;

    // Add our own SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
    pos += 4;

    // calc padding length
    WebRtc_UWord8 paddingBytes = 4-((2+bytesRequired)%4);
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
        rtcpbuffer[pos] = 0x80 | WebRtc_UWord8(pictureID >> (i*7));
        pos++;
    }
    // add last byte of picture ID
    rtcpbuffer[pos] = WebRtc_UWord8(pictureID & 0x7f);
    pos++;

    // add padding
    for(int j = 0; j <paddingBytes; j++)
    {
        rtcpbuffer[pos] = 0;
        pos++;
    }
    return 0;
}

WebRtc_Word32
RTCPSender::BuildREMB(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos)
{
    // sanity
    if(pos + 20 + 4 * _lengthRembSSRC >= IP_PACKET_SIZE)
    {
        return -2;
    }
    // add application layer feedback
    WebRtc_UWord8 FMT = 15;
    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + FMT;
    rtcpbuffer[pos++]=(WebRtc_UWord8)206;

    rtcpbuffer[pos++]=(WebRtc_UWord8)0;
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
    WebRtc_UWord8 brExp = 0;
    for(WebRtc_UWord32 i=0; i<64; i++)
    {
        if(_rembBitrate <= ((WebRtc_UWord32)262143 << i))
        {
            brExp = i;
            break;
        }
    }
    const WebRtc_UWord32 brMantissa = (_rembBitrate >> brExp);
    rtcpbuffer[pos++]=(WebRtc_UWord8)((brExp << 2) + ((brMantissa >> 16) & 0x03));
    rtcpbuffer[pos++]=(WebRtc_UWord8)(brMantissa >> 8);
    rtcpbuffer[pos++]=(WebRtc_UWord8)(brMantissa);

    for (int i = 0; i < _lengthRembSSRC; i++) 
    { 
        ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _rembSSRC[i]);
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

WebRtc_Word32
RTCPSender::BuildTMMBR(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos)
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
    WebRtc_Word32 lengthOfBoundingSet
        = _rtpRtcp.BoundingSet(tmmbrOwner, candidateSet);

    if(lengthOfBoundingSet > 0)
    {
        for (WebRtc_Word32 i = 0; i < lengthOfBoundingSet; i++)
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
        WebRtc_UWord8 FMT = 3;
        rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + FMT;
        rtcpbuffer[pos++]=(WebRtc_UWord8)205;

        //Length of 4
        rtcpbuffer[pos++]=(WebRtc_UWord8)0;
        rtcpbuffer[pos++]=(WebRtc_UWord8)(4);

        // Add our own SSRC
        ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
        pos += 4;

        // RFC 5104     4.2.1.2.  Semantics

        // SSRC of media source
        rtcpbuffer[pos++]=(WebRtc_UWord8)0;
        rtcpbuffer[pos++]=(WebRtc_UWord8)0;
        rtcpbuffer[pos++]=(WebRtc_UWord8)0;
        rtcpbuffer[pos++]=(WebRtc_UWord8)0;

        // Additional Feedback Control Information (FCI)
        ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
        pos += 4;

        WebRtc_UWord32 bitRate = _tmmbr_Send*1000;
        WebRtc_UWord32 mmbrExp = 0;
        for(WebRtc_UWord32 i=0;i<64;i++)
        {
            if(bitRate <= ((WebRtc_UWord32)131071 << i))
            {
                mmbrExp = i;
                break;
            }
        }
        WebRtc_UWord32 mmbrMantissa = (bitRate >> mmbrExp);

        rtcpbuffer[pos++]=(WebRtc_UWord8)((mmbrExp << 2) + ((mmbrMantissa >> 15) & 0x03));
        rtcpbuffer[pos++]=(WebRtc_UWord8)(mmbrMantissa >> 7);
        rtcpbuffer[pos++]=(WebRtc_UWord8)((mmbrMantissa << 1) + ((_packetOH_Send >> 8)& 0x01));
        rtcpbuffer[pos++]=(WebRtc_UWord8)(_packetOH_Send);
    }
    return 0;
}

WebRtc_Word32
RTCPSender::BuildTMMBN(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos)
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
    WebRtc_UWord8 FMT = 4;
    // add TMMBN indicator
    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + FMT;
    rtcpbuffer[pos++]=(WebRtc_UWord8)205;

    //Add length later
    int posLength = pos;
    pos++;
    pos++;

    // Add our own SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // RFC 5104     4.2.2.2.  Semantics

    // SSRC of media source
    rtcpbuffer[pos++]=(WebRtc_UWord8)0;
    rtcpbuffer[pos++]=(WebRtc_UWord8)0;
    rtcpbuffer[pos++]=(WebRtc_UWord8)0;
    rtcpbuffer[pos++]=(WebRtc_UWord8)0;

    // Additional Feedback Control Information (FCI)
    int numBoundingSet = 0;
    for(WebRtc_UWord32 n=0; n< boundingSet->lengthOfSet(); n++)
    {
        if (boundingSet->Tmmbr(n) > 0)
        {
            WebRtc_UWord32 tmmbrSSRC = boundingSet->Ssrc(n);
            ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, tmmbrSSRC);
            pos += 4;

            WebRtc_UWord32 bitRate = boundingSet->Tmmbr(n) * 1000;
            WebRtc_UWord32 mmbrExp = 0;
            for(int i=0; i<64; i++)
            {
                if(bitRate <=  ((WebRtc_UWord32)131071 << i))
                {
                    mmbrExp = i;
                    break;
                }
            }
            WebRtc_UWord32 mmbrMantissa = (bitRate >> mmbrExp);
            WebRtc_UWord32 measuredOH = boundingSet->PacketOH(n);

            rtcpbuffer[pos++]=(WebRtc_UWord8)((mmbrExp << 2) + ((mmbrMantissa >> 15) & 0x03));
            rtcpbuffer[pos++]=(WebRtc_UWord8)(mmbrMantissa >> 7);
            rtcpbuffer[pos++]=(WebRtc_UWord8)((mmbrMantissa << 1) + ((measuredOH >> 8)& 0x01));
            rtcpbuffer[pos++]=(WebRtc_UWord8)(measuredOH);
            numBoundingSet++;
        }
    }
    WebRtc_UWord16 length= (WebRtc_UWord16)(2+2*numBoundingSet);
    rtcpbuffer[posLength++]=(WebRtc_UWord8)(length>>8);
    rtcpbuffer[posLength]=(WebRtc_UWord8)(length);
    return 0;
}

WebRtc_Word32
RTCPSender::BuildAPP(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos)
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
    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + _appSubType;

    // Add APP ID
    rtcpbuffer[pos++]=(WebRtc_UWord8)204;

    WebRtc_UWord16 length = (_appLength>>2) + 2; // include SSRC and name
    rtcpbuffer[pos++]=(WebRtc_UWord8)(length>>8);
    rtcpbuffer[pos++]=(WebRtc_UWord8)(length);

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

WebRtc_Word32
RTCPSender::BuildNACK(WebRtc_UWord8* rtcpbuffer,
                      WebRtc_UWord32& pos,
                      const WebRtc_Word32 nackSize,
                      const WebRtc_UWord16* nackList)
{
    // sanity
    if(pos + 16 >= IP_PACKET_SIZE)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id, "%s invalid argument", __FUNCTION__);
        return -2;
    }

    // int size, WebRtc_UWord16* nackList
    // add nack list
    WebRtc_UWord8 FMT = 1;
    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + FMT;
    rtcpbuffer[pos++]=(WebRtc_UWord8)205;

    rtcpbuffer[pos++]=(WebRtc_UWord8) 0;
    int nackSizePos = pos;
    rtcpbuffer[pos++]=(WebRtc_UWord8)(3); //setting it to one kNACK signal as default

    // Add our own SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
    pos += 4;

    // Add the remote SSRC
    ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _remoteSSRC);
    pos += 4;

    // add the list
    int i = 0;
    int numOfNackFields = 0;
    while(nackSize > i && numOfNackFields < 253)
    {
        WebRtc_UWord16 nack = nackList[i];
        // put dow our sequence number
        ModuleRTPUtility::AssignUWord16ToBuffer(rtcpbuffer+pos, nack);
        pos += 2;

        i++;
        numOfNackFields++;
        if(nackSize > i)
        {
            bool moreThan16Away = (WebRtc_UWord16(nack+16) < nackList[i])?true: false;
            if(!moreThan16Away)
            {
                // check for a wrap
                if(WebRtc_UWord16(nack+16) > 0xff00 && nackList[i] < 0x0fff)
                {
                    // wrap
                    moreThan16Away = true;
                }
            }
            if(moreThan16Away)
            {
                // next is more than 16 away
                rtcpbuffer[pos++]=(WebRtc_UWord8)0;
                rtcpbuffer[pos++]=(WebRtc_UWord8)0;
            } else
            {
                // build our bitmask
                WebRtc_UWord16 bitmask = 0;

                bool within16Away = (WebRtc_UWord16(nack+16) > nackList[i])?true: false;
                if(within16Away)
                {
                   // check for a wrap
                    if(WebRtc_UWord16(nack+16) > 0xff00 && nackList[i] < 0x0fff)
                    {
                        // wrap
                        within16Away = false;
                    }
                }

                while( nackSize > i && within16Away)
                {
                    WebRtc_Word16 shift = (nackList[i]-nack)-1;
                    assert(!(shift > 15) && !(shift < 0));

                    bitmask += (1<< shift);
                    i++;
                    if(nackSize > i)
                    {
                        within16Away = (WebRtc_UWord16(nack+16) > nackList[i])?true: false;
                        if(within16Away)
                        {
                            // check for a wrap
                            if(WebRtc_UWord16(nack+16) > 0xff00 && nackList[i] < 0x0fff)
                            {
                                // wrap
                                within16Away = false;
                            }
                        }
                    }
                }
                ModuleRTPUtility::AssignUWord16ToBuffer(rtcpbuffer+pos, bitmask);
                pos += 2;
            }
            // sanity do we have room from one more 4 byte block?
            if(pos + 4 >= IP_PACKET_SIZE)
            {
                return -2;
            }
        } else
        {
            // no more in the list
            rtcpbuffer[pos++]=(WebRtc_UWord8)0;
            rtcpbuffer[pos++]=(WebRtc_UWord8)0;
        }
    }
    rtcpbuffer[nackSizePos]=(WebRtc_UWord8)(2+numOfNackFields);
    return 0;
}

WebRtc_Word32
RTCPSender::BuildBYE(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos)
{
    // sanity
    if(pos + 8 >= IP_PACKET_SIZE)
    {
        return -2;
    }
    if(_includeCSRCs)
    {
        // Add a bye packet
        rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + 1 + _CSRCs;  // number of SSRC+CSRCs
        rtcpbuffer[pos++]=(WebRtc_UWord8)203;

        // length
        rtcpbuffer[pos++]=(WebRtc_UWord8)0;
        rtcpbuffer[pos++]=(WebRtc_UWord8)(1 + _CSRCs);

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
        rtcpbuffer[pos++]=(WebRtc_UWord8)0x80 + 1;  // number of SSRC+CSRCs
        rtcpbuffer[pos++]=(WebRtc_UWord8)203;

        // length
        rtcpbuffer[pos++]=(WebRtc_UWord8)0;
        rtcpbuffer[pos++]=(WebRtc_UWord8)1;

        // Add our own SSRC
        ModuleRTPUtility::AssignUWord32ToBuffer(rtcpbuffer+pos, _SSRC);
        pos += 4;
    }
    return 0;
}

WebRtc_Word32
RTCPSender::BuildVoIPMetric(WebRtc_UWord8* rtcpbuffer, WebRtc_UWord32& pos)
{
    // sanity
    if(pos + 44 >= IP_PACKET_SIZE)
    {
        return -2;
    }

    // Add XR header
    rtcpbuffer[pos++]=(WebRtc_UWord8)0x80;
    rtcpbuffer[pos++]=(WebRtc_UWord8)207;

    WebRtc_UWord32 XRLengthPos = pos;

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

    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.burstDuration >> 8);
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.burstDuration);
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.gapDuration >> 8);
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.gapDuration);

    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.roundTripDelay >> 8);
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.roundTripDelay);
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.endSystemDelay >> 8);
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.endSystemDelay);

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
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.JBnominal >> 8);
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.JBnominal);

    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.JBmax >> 8);
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.JBmax);
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.JBabsMax >> 8);
    rtcpbuffer[pos++] = (WebRtc_UWord8)(_xrVoIPMetric.JBabsMax);

    rtcpbuffer[XRLengthPos]=(WebRtc_UWord8)(0);
    rtcpbuffer[XRLengthPos+1]=(WebRtc_UWord8)(10);
    return 0;
}

WebRtc_Word32
RTCPSender::SendRTCP(const WebRtc_UWord32 packetTypeFlags,
                     const WebRtc_Word32 nackSize,       // NACK
                     const WebRtc_UWord16* nackList,     // NACK
                     const bool repeat,                  // FIR
                     const WebRtc_UWord64 pictureID)     // SLI & RPSI
{
    WebRtc_UWord32 rtcpPacketTypeFlags = packetTypeFlags;
    WebRtc_UWord32 pos = 0;
    WebRtc_UWord8 rtcpbuffer[IP_PACKET_SIZE];

    do  // only to be able to use break :) (and the critsect must be inside its own scope)
    {
        // collect the received information
        RTCPReportBlock received;
        bool hasReceived = false;
        WebRtc_UWord32 NTPsec = 0;
        WebRtc_UWord32 NTPfrac = 0;
        bool rtcpCompound = false;
        WebRtc_UWord32 jitterTransmissionOffset = 0;

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

                WebRtc_UWord32 lastReceivedRRNTPsecs = 0;
                WebRtc_UWord32 lastReceivedRRNTPfrac = 0;
                WebRtc_UWord32 remoteSR = 0;

                // ok even if we have not received a SR, we will send 0 in that case
                _rtpRtcp.LastReceivedNTP(lastReceivedRRNTPsecs,
                                         lastReceivedRRNTPfrac,
                                         remoteSR);

                // get our NTP as late as possible to avoid a race
                _clock.CurrentNTP(NTPsec, NTPfrac);

                // Delay since last received report
                WebRtc_UWord32 delaySinceLastReceivedSR = 0;
                if((lastReceivedRRNTPsecs !=0) || (lastReceivedRRNTPfrac !=0))
                {
                    // get the 16 lowest bits of seconds and the 16 higest bits of fractions
                    WebRtc_UWord32 now=NTPsec&0x0000FFFF;
                    now <<=16;
                    now += (NTPfrac&0xffff0000)>>16;

                    WebRtc_UWord32 receiveTime = lastReceivedRRNTPsecs&0x0000FFFF;
                    receiveTime <<=16;
                    receiveTime += (lastReceivedRRNTPfrac&0xffff0000)>>16;

                    delaySinceLastReceivedSR = now-receiveTime;
                }
                received.delaySinceLastSR = delaySinceLastReceivedSR;
                received.lastSR = remoteSR;
            } else
            {
                // we need to send our NTP even if we dont have received any reports
                _clock.CurrentNTP(NTPsec, NTPfrac);
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
            WebRtc_Word32 random = rand() % 1000;
            WebRtc_Word32 timeToNext = RTCP_INTERVAL_AUDIO_MS;

            if(_audio)
            {
                timeToNext = (RTCP_INTERVAL_AUDIO_MS/2) + (RTCP_INTERVAL_AUDIO_MS*random/1000);
            }else
            {
                WebRtc_UWord32 minIntervalMs = RTCP_INTERVAL_AUDIO_MS;
                if(_sending)
                {
                    // calc bw for video 360/sendBW in kbit/s
                    WebRtc_UWord32 sendBitrateKbit = 0;
                    WebRtc_UWord32 videoRate = 0;
                    WebRtc_UWord32 fecRate = 0;
                    WebRtc_UWord32 nackRate = 0;
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
            _nextTimeToSendRTCP = _clock.GetTimeInMS() + timeToNext;
        }

        // if the data does not fitt in the packet we fill it as much as possible
        WebRtc_Word32 buildVal = 0;

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
        }
        if(rtcpPacketTypeFlags & kRtcpSli)
        {
            buildVal = BuildSLI(rtcpbuffer, pos, (WebRtc_UWord8)pictureID);
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
            const WebRtc_Word8 payloadType = _rtpRtcp.SendPayloadType();
            if(payloadType == -1)
            {
                return -1;
            }
            buildVal = BuildRPSI(rtcpbuffer, pos, pictureID, (WebRtc_UWord8)payloadType);
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
            buildVal = BuildNACK(rtcpbuffer, pos, nackSize, nackList);
            if(buildVal == -1)
            {
                return -1; // error

            }else if(buildVal == -2)
            {
                break;  // out of buffer
            }
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
    return SendToNetwork(rtcpbuffer, (WebRtc_UWord16)pos);
}

WebRtc_Word32
RTCPSender::SendToNetwork(const WebRtc_UWord8* dataBuffer,
                          const WebRtc_UWord16 length)
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

WebRtc_Word32
RTCPSender::SetCSRCStatus(const bool include)
{
    _includeCSRCs = include;
    return 0;
}

WebRtc_Word32
RTCPSender::SetCSRCs(const WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize],
                    const WebRtc_UWord8 arrLength)
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

WebRtc_Word32
RTCPSender::SetApplicationSpecificData(const WebRtc_UWord8 subType,
                                       const WebRtc_UWord32 name,
                                       const WebRtc_UWord8* data,
                                       const WebRtc_UWord16 length)
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
    _appData = new WebRtc_UWord8[length];
    _appLength = length;
    memcpy(_appData, data, length);
    return 0;
}

WebRtc_Word32
RTCPSender::SetRTCPVoIPMetrics(const RTCPVoIPMetric* VoIPMetric)
{
    CriticalSectionScoped lock(_criticalSectionRTCPSender);
    memcpy(&_xrVoIPMetric, VoIPMetric, sizeof(RTCPVoIPMetric));

    _xrSendVoIPMetric = true;
    return 0;
}

// called under critsect _criticalSectionRTCPSender
WebRtc_Word32 RTCPSender::AddReportBlocks(WebRtc_UWord8* rtcpbuffer,
                                          WebRtc_UWord32& pos,
                                          WebRtc_UWord8& numberOfReportBlocks,
                                          const RTCPReportBlock* received,
                                          const WebRtc_UWord32 NTPsec,
                                          const WebRtc_UWord32 NTPfrac) {
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
    _lastRTCPTime[0] = ModuleRTPUtility::ConvertNTPTimeToMS(NTPsec, NTPfrac);

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
  std::map<WebRtc_UWord32, RTCPReportBlock*>::iterator it =
      _reportBlocks.begin();

  for (; it != _reportBlocks.end(); it++) {
    // we can have multiple report block in a conference
    WebRtc_UWord32 remoteSSRC = it->first;
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
WebRtc_Word32
RTCPSender::SetTMMBN(const TMMBRSet* boundingSet,
                     const WebRtc_UWord32 maxBitrateKbit)
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
