/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "trace.h"
#include "rtp_receiver.h"

#include "rtp_rtcp_defines.h"
#include "rtp_rtcp_impl.h"
#include "critical_section_wrapper.h"

#include <cassert>
#include <string.h> //memcpy
#include <math.h>   // floor
#include <stdlib.h> // abs

namespace webrtc {

using ModuleRTPUtility::AudioPayload;
using ModuleRTPUtility::GetCurrentRTP;
using ModuleRTPUtility::Payload;
using ModuleRTPUtility::RTPPayloadParser;
using ModuleRTPUtility::StringCompare;
using ModuleRTPUtility::VideoPayload;

RTPReceiver::RTPReceiver(const WebRtc_Word32 id,
                         const bool audio,
                         RtpRtcpClock* clock,
                         RemoteBitrateEstimator* remote_bitrate,
                         ModuleRtpRtcpImpl* owner) :
    RTPReceiverAudio(id),
    RTPReceiverVideo(id, remote_bitrate, owner),
    Bitrate(clock),
    _id(id),
    _audio(audio),
    _rtpRtcp(*owner),
    _criticalSectionCbs(CriticalSectionWrapper::CreateCriticalSection()),
    _cbRtpFeedback(NULL),
    _cbRtpData(NULL),

    _criticalSectionRTPReceiver(
        CriticalSectionWrapper::CreateCriticalSection()),
    _lastReceiveTime(0),
    _lastReceivedPayloadLength(0),
    _lastReceivedPayloadType(-1),
    _lastReceivedMediaPayloadType(-1),
    _lastReceivedAudioSpecific(),
    _lastReceivedVideoSpecific(),

    _packetTimeOutMS(0),

    _redPayloadType(-1),
    _payloadTypeMap(),
    _rtpHeaderExtensionMap(),
    _SSRC(0),
    _numCSRCs(0),
    _currentRemoteCSRC(),
    _numEnergy(0),
    _currentRemoteEnergy(),
    _useSSRCFilter(false),
    _SSRCFilter(0),

    _jitterQ4(0),
    _jitterMaxQ4(0),
    _cumulativeLoss(0),
    _jitterQ4TransmissionTimeOffset(0),
    _localTimeLastReceivedTimestamp(0),
    _lastReceivedTimestamp(0),
    _lastReceivedSequenceNumber(0),
    _lastReceivedTransmissionTimeOffset(0),

    _receivedSeqFirst(0),
    _receivedSeqMax(0),
    _receivedSeqWraps(0),

    _receivedPacketOH(12), // RTP header
    _receivedByteCount(0),
    _receivedOldPacketCount(0),
    _receivedInorderPacketCount(0),

    _lastReportInorderPackets(0),
    _lastReportOldPackets(0),
    _lastReportSeqMax(0),
    _lastReportFractionLost(0),
    _lastReportCumulativeLost(0),
    _lastReportExtendedHighSeqNum(0),
    _lastReportJitter(0),
    _lastReportJitterTransmissionTimeOffset(0),

    _nackMethod(kNackOff),
    _RTX(false),
    _ssrcRTX(0) {
  memset(_currentRemoteCSRC, 0, sizeof(_currentRemoteCSRC));
  memset(_currentRemoteEnergy, 0, sizeof(_currentRemoteEnergy));
  memset(&_lastReceivedAudioSpecific, 0, sizeof(_lastReceivedAudioSpecific));

  _lastReceivedAudioSpecific.channels = 1;
  _lastReceivedVideoSpecific.maxRate = 0;
  _lastReceivedVideoSpecific.videoCodecType = kRtpNoVideo;

  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, id, "%s created", __FUNCTION__);
}

RTPReceiver::~RTPReceiver() {
  if (_cbRtpFeedback) {
    for (int i = 0; i < _numCSRCs; i++) {
      _cbRtpFeedback->OnIncomingCSRCChanged(_id,_currentRemoteCSRC[i], false);
    }
  }
  delete _criticalSectionCbs;
  delete _criticalSectionRTPReceiver;

  while (!_payloadTypeMap.empty()) {
    std::map<WebRtc_Word8, Payload*>::iterator it = _payloadTypeMap.begin();
    delete it->second;
    _payloadTypeMap.erase(it);
  }
  WEBRTC_TRACE(kTraceMemory, kTraceRtpRtcp, _id, "%s deleted", __FUNCTION__);
}

RtpVideoCodecTypes
RTPReceiver::VideoCodecType() const
{
    return _lastReceivedVideoSpecific.videoCodecType;
}

WebRtc_UWord32
RTPReceiver::MaxConfiguredBitrate() const
{
    return _lastReceivedVideoSpecific.maxRate;
}

bool
RTPReceiver::REDPayloadType(const WebRtc_Word8 payloadType) const
{
    return (_redPayloadType == payloadType)?true:false;
}

WebRtc_Word8
RTPReceiver::REDPayloadType() const
{
    return _redPayloadType;
}

    // configure a timeout value
WebRtc_Word32
RTPReceiver::SetPacketTimeout(const WebRtc_UWord32 timeoutMS)
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    _packetTimeOutMS = timeoutMS;
    return 0;
}

void RTPReceiver::PacketTimeout()
{
    bool packetTimeOut = false;
    {
        CriticalSectionScoped lock(_criticalSectionRTPReceiver);
        if(_packetTimeOutMS == 0)
        {
            // not configured
            return;
        }

        if(_lastReceiveTime == 0)
        {
            // not active
            return;
        }

        WebRtc_Word64 now = _clock.GetTimeInMS();

        if(now - _lastReceiveTime > _packetTimeOutMS)
        {
            packetTimeOut = true;
            _lastReceiveTime = 0;  // only one callback
            _lastReceivedPayloadType = -1; // makes RemotePayload return -1, which we want
            _lastReceivedMediaPayloadType = -1;
        }
    }
    CriticalSectionScoped lock(_criticalSectionCbs);
    if(packetTimeOut && _cbRtpFeedback)
    {
        _cbRtpFeedback->OnPacketTimeout(_id);
    }
}

void
RTPReceiver::ProcessDeadOrAlive(const bool RTCPalive, const WebRtc_Word64 now)
{
    if(_cbRtpFeedback == NULL)
    {
        // no callback
        return;
    }
    RTPAliveType alive = kRtpDead;

    if(_lastReceiveTime + 1000 > now)
    {
        // always alive if we have received a RTP packet the last sec
        alive = kRtpAlive;

    } else
    {
        if(RTCPalive)
        {
            if(_audio)
            {
                // alive depends on CNG
                // if last received size < 10 likely CNG
                if(_lastReceivedPayloadLength < 10) // our CNG is 9 bytes
                {
                    // potential CNG
                    // receiver need to check kRtpNoRtp against NetEq speechType kOutputPLCtoCNG
                    alive = kRtpNoRtp;
                } else
                {
                    // dead
                }
            } else
            {
                // dead for video
            }
        }else
        {
            // no RTP packet for 1 sec and no RTCP
            // dead
        }
    }


    CriticalSectionScoped lock(_criticalSectionCbs);
    if(_cbRtpFeedback)
    {
        _cbRtpFeedback->OnPeriodicDeadOrAlive(_id, alive);
    }
}

WebRtc_UWord16
RTPReceiver::PacketOHReceived() const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    return _receivedPacketOH;
}

WebRtc_UWord32
RTPReceiver::PacketCountReceived() const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    return _receivedInorderPacketCount;
}

WebRtc_UWord32
RTPReceiver::ByteCountReceived() const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    return _receivedByteCount;
}

WebRtc_Word32
RTPReceiver::RegisterIncomingRTPCallback(RtpFeedback* incomingMessagesCallback)
{
    CriticalSectionScoped lock(_criticalSectionCbs);
    _cbRtpFeedback = incomingMessagesCallback;
    return 0;
}

WebRtc_Word32
RTPReceiver::RegisterIncomingDataCallback(RtpData* incomingDataCallback)
{
    CriticalSectionScoped lock(_criticalSectionCbs);
    _cbRtpData = incomingDataCallback;
    return 0;
}

WebRtc_Word32 RTPReceiver::RegisterReceivePayload(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payloadType,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) {
  assert(payloadName);
  CriticalSectionScoped lock(_criticalSectionRTPReceiver);

  // sanity
  switch (payloadType) {
    // reserved payload types to avoid RTCP conflicts when marker bit is set
    case 64:        //  192 Full INTRA-frame request
    case 72:        //  200 Sender report
    case 73:        //  201 Receiver report
    case 74:        //  202 Source description
    case 75:        //  203 Goodbye
    case 76:        //  204 Application-defined
    case 77:        //  205 Transport layer FB message
    case 78:        //  206 Payload-specific FB message
    case 79:        //  207 Extended report
      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                   "%s invalid payloadtype:%d",
                   __FUNCTION__, payloadType);
      return -1;
    default:
      break;
  }
  size_t payloadNameLength = strlen(payloadName);

  std::map<WebRtc_Word8, Payload*>::iterator it =
      _payloadTypeMap.find(payloadType);
  if (it != _payloadTypeMap.end()) {
    // we already use this payload type
    Payload* payload = it->second;
    assert(payload);

    size_t nameLength = strlen(payload->name);

    // check if it's the same as we already have
    // if same ignore sending an error
    if (payloadNameLength == nameLength &&
        StringCompare(payload->name, payloadName, payloadNameLength)) {
      if (_audio &&
          payload->audio &&
          payload->typeSpecific.Audio.frequency == frequency &&
          payload->typeSpecific.Audio.channels == channels &&
          (payload->typeSpecific.Audio.rate == rate ||
              payload->typeSpecific.Audio.rate == 0 || rate == 0)) {
        payload->typeSpecific.Audio.rate = rate;
        // Ensure that we update the rate if new or old is zero
        return 0;
      }
      if (!_audio && !payload->audio) {
        // update maxBitrate for video
        payload->typeSpecific.Video.maxRate = rate;
        return 0;
      }
    }
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s invalid argument payloadType:%d already registered",
                 __FUNCTION__, payloadType);
    return -1;
  }
  if (_audio) {
    // remove existing item, hence search for the name
    // only for audio, for video we allow a codecs to use multiple pltypes
    std::map<WebRtc_Word8, Payload*>::iterator audio_it =
        _payloadTypeMap.begin();
    while (audio_it != _payloadTypeMap.end()) {
      Payload* payload = audio_it->second;
      size_t nameLength = strlen(payload->name);

      if (payloadNameLength == nameLength &&
          StringCompare(payload->name, payloadName, payloadNameLength)) {
        // we found the payload name in the list
        // if audio check frequency and rate
        if (payload->audio) {
          if (payload->typeSpecific.Audio.frequency == frequency &&
              (payload->typeSpecific.Audio.rate == rate ||
                  payload->typeSpecific.Audio.rate == 0 || rate == 0) &&
                  payload->typeSpecific.Audio.channels == channels) {
            // remove old setting
            delete payload;
            _payloadTypeMap.erase(audio_it);
            break;
          }
        } else if(StringCompare(payloadName,"red",3)) {
          delete payload;
          _payloadTypeMap.erase(audio_it);
          break;
        }
      }
      audio_it++;
    }
  }
  Payload* payload = NULL;

  // save the RED payload type
  // used in both audio and video
  if (StringCompare(payloadName,"red",3)) {
    _redPayloadType = payloadType;
    payload = new Payload;
    payload->audio = false;
    payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
  } else {
    if (_audio) {
      payload = RegisterReceiveAudioPayload(payloadName, payloadType,
                                            frequency, channels, rate);
    } else {
      payload = RegisterReceiveVideoPayload(payloadName, payloadType, rate);
    }
  }
  if (payload == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s filed to register payload",
                 __FUNCTION__);
    return -1;
  }
  _payloadTypeMap[payloadType] = payload;

  // Successful set of payload type, clear the value of last receivedPT,
  // since it might mean something else
  _lastReceivedPayloadType = -1;
  _lastReceivedMediaPayloadType = -1;
  return 0;
}

WebRtc_Word32 RTPReceiver::DeRegisterReceivePayload(
    const WebRtc_Word8 payloadType) {
  CriticalSectionScoped lock(_criticalSectionRTPReceiver);

  std::map<WebRtc_Word8, Payload*>::iterator it =
      _payloadTypeMap.find(payloadType);

  if (it == _payloadTypeMap.end()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s failed to find payloadType:%d",
                 __FUNCTION__, payloadType);
    return -1;
  }
  delete it->second;
  _payloadTypeMap.erase(it);
  return 0;
}

WebRtc_Word32 RTPReceiver::ReceivePayloadType(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate,
    WebRtc_Word8* payloadType) const {
  if (payloadType == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s invalid argument", __FUNCTION__);
    return -1;
  }
  size_t payloadNameLength = strlen(payloadName);

  CriticalSectionScoped lock(_criticalSectionRTPReceiver);

  std::map<WebRtc_Word8, Payload*>::const_iterator it =
      _payloadTypeMap.begin();

  while (it != _payloadTypeMap.end()) {
    Payload* payload = it->second;
    assert(payload);

    size_t nameLength = strlen(payload->name);
    if (payloadNameLength == nameLength &&
        StringCompare(payload->name, payloadName, payloadNameLength)) {
      // name match
      if( payload->audio) {
        if (rate == 0) {
          // [default] audio, check freq and channels
          if (payload->typeSpecific.Audio.frequency == frequency &&
              payload->typeSpecific.Audio.channels == channels) {
            *payloadType = it->first;
            return 0;
          }
        } else {
          // audio, check freq, channels and rate
          if( payload->typeSpecific.Audio.frequency == frequency &&
              payload->typeSpecific.Audio.channels == channels &&
              payload->typeSpecific.Audio.rate == rate) {
            // extra rate condition added
            *payloadType = it->first;
            return 0;
          }
        }
      } else {
        // video
        *payloadType = it->first;
        return 0;
      }
    }
    it++;
  }
  return -1;
}

WebRtc_Word32 RTPReceiver::ReceivePayload(
    const WebRtc_Word8 payloadType,
    char payloadName[RTP_PAYLOAD_NAME_SIZE],
    WebRtc_UWord32* frequency,
    WebRtc_UWord8* channels,
    WebRtc_UWord32* rate) const {
  CriticalSectionScoped lock(_criticalSectionRTPReceiver);

  std::map<WebRtc_Word8, Payload*>::const_iterator it =
      _payloadTypeMap.find(payloadType);

  if (it == _payloadTypeMap.end()) {
    return -1;
  }
  Payload* payload = it->second;
  assert(payload);

  if(frequency) {
    if(payload->audio) {
      *frequency = payload->typeSpecific.Audio.frequency;
    } else {
      *frequency = 90000;
    }
  }
  if (channels) {
    if(payload->audio) {
      *channels = payload->typeSpecific.Audio.channels;
    } else {
      *channels = 1;
    }
  }
  if (rate) {
    if(payload->audio) {
      *rate = payload->typeSpecific.Audio.rate;
    } else {
      assert(false);
      *rate = 0;
    }
  }
  if (payloadName) {
    payloadName[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
    strncpy(payloadName, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);
  }
  return 0;
}

WebRtc_Word32 RTPReceiver::RemotePayload(
    char payloadName[RTP_PAYLOAD_NAME_SIZE],
    WebRtc_Word8* payloadType,
    WebRtc_UWord32* frequency,
    WebRtc_UWord8* channels) const {
  if(_lastReceivedPayloadType == -1) {
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                 "%s invalid state", __FUNCTION__);
    return -1;
  }
  std::map<WebRtc_Word8, Payload*>::const_iterator it =
      _payloadTypeMap.find(_lastReceivedPayloadType);

  if (it == _payloadTypeMap.end()) {
    return -1;
  }
  Payload* payload = it->second;
  assert(payload);
  payloadName[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payloadName, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);

  if (payloadType) {
    *payloadType = _lastReceivedPayloadType;
  }
  if (frequency) {
    if (payload->audio) {
      *frequency = payload->typeSpecific.Audio.frequency;
    } else {
      *frequency = 90000;
    }
  }
  if (channels) {
    if (payload->audio) {
      *channels = payload->typeSpecific.Audio.channels;
    } else {
      *channels = 1;
    }
  }
  return 0;
}

WebRtc_Word32
RTPReceiver::RegisterRtpHeaderExtension(const RTPExtensionType type,
                                        const WebRtc_UWord8 id)
{
    CriticalSectionScoped cs(_criticalSectionRTPReceiver);
    return _rtpHeaderExtensionMap.Register(type, id);
}

WebRtc_Word32
RTPReceiver::DeregisterRtpHeaderExtension(const RTPExtensionType type)
{
    CriticalSectionScoped cs(_criticalSectionRTPReceiver);
    return _rtpHeaderExtensionMap.Deregister(type);
}

void RTPReceiver::GetHeaderExtensionMapCopy(RtpHeaderExtensionMap* map) const
{
    CriticalSectionScoped cs(_criticalSectionRTPReceiver);
    _rtpHeaderExtensionMap.GetCopy(map);
}

NACKMethod
RTPReceiver::NACK() const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    return _nackMethod;
}

    // Turn negative acknowledgement requests on/off
WebRtc_Word32
RTPReceiver::SetNACKStatus(const NACKMethod method)
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    _nackMethod = method;
    return 0;
}

void RTPReceiver::SetRTXStatus(const bool enable,
                               const WebRtc_UWord32 SSRC) {
  CriticalSectionScoped lock(_criticalSectionRTPReceiver);
  _RTX = enable;
  _ssrcRTX = SSRC;
}

void RTPReceiver::RTXStatus(bool* enable, WebRtc_UWord32* SSRC) const {
  CriticalSectionScoped lock(_criticalSectionRTPReceiver);
  *enable = _RTX;
  *SSRC = _ssrcRTX;
}

WebRtc_UWord32
RTPReceiver::SSRC() const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    return _SSRC;
}

    // Get remote CSRC
WebRtc_Word32
RTPReceiver::CSRCs( WebRtc_UWord32 arrOfCSRC[kRtpCsrcSize]) const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);

    assert(_numCSRCs <= kRtpCsrcSize);

    if(_numCSRCs >0)
    {
        memcpy(arrOfCSRC, _currentRemoteCSRC, sizeof(WebRtc_UWord32)*_numCSRCs);
    }
    return _numCSRCs;
}

WebRtc_Word32
RTPReceiver::Energy( WebRtc_UWord8 arrOfEnergy[kRtpCsrcSize]) const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);

    assert(_numEnergy <= kRtpCsrcSize);

    if(_numEnergy >0)
    {
        memcpy(arrOfEnergy, _currentRemoteEnergy, sizeof(WebRtc_UWord8)*_numCSRCs);
    }
    return _numEnergy;
}

WebRtc_Word32 RTPReceiver::IncomingRTPPacket(
    WebRtcRTPHeader* rtp_header,
    const WebRtc_UWord8* packet,
    const WebRtc_UWord16 packet_length) {
  // rtp_header contains the parsed RTP header.
  // Adjust packet length w r t RTP padding.
  int length = packet_length - rtp_header->header.paddingLength;

  // length sanity
  if ((length - rtp_header->header.headerLength) < 0) {
     WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                  "%s invalid argument",
                  __FUNCTION__);
     return -1;
  }
  if (_RTX) {
    if (_ssrcRTX == rtp_header->header.ssrc) {
      // Sanity check.
      if (rtp_header->header.headerLength + 2 > packet_length) {
        return -1;
      }
      rtp_header->header.ssrc = _SSRC;
      rtp_header->header.sequenceNumber =
          (packet[rtp_header->header.headerLength] << 8) +
          packet[1 + rtp_header->header.headerLength];
      // Count the RTX header as part of the RTP header.
      rtp_header->header.headerLength += 2;
    }
  }
  if (_useSSRCFilter) {
    if (rtp_header->header.ssrc != _SSRCFilter) {
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                   "%s drop packet due to SSRC filter",
                   __FUNCTION__);
      return -1;
    }
  }
  if (_lastReceiveTime == 0) {
    // trigger only once
    CriticalSectionScoped lock(_criticalSectionCbs);
    if (_cbRtpFeedback) {
      if (length - rtp_header->header.headerLength == 0) {
        // keepalive packet
        _cbRtpFeedback->OnReceivedPacket(_id, kPacketKeepAlive);
      } else {
        _cbRtpFeedback->OnReceivedPacket(_id, kPacketRtp);
      }
    }
  }
  WebRtc_Word8 first_payload_byte = 0;
  if (length > 0) {
    first_payload_byte = packet[rtp_header->header.headerLength];
  }
  // trigger our callbacks
  CheckSSRCChanged(rtp_header);

  bool is_red = false;
  VideoPayload video_specific;
  video_specific.maxRate = 0;
  video_specific.videoCodecType = kRtpNoVideo;

  AudioPayload audio_specific;
  audio_specific.channels = 0;
  audio_specific.frequency = 0;

  if (CheckPayloadChanged(rtp_header,
                          first_payload_byte,
                          is_red,
                          audio_specific,
                          video_specific) == -1) {
    if (length - rtp_header->header.headerLength == 0)
    {
      // ok keepalive packet
      WEBRTC_TRACE(kTraceStream, kTraceRtpRtcp, _id,
                   "%s received keepalive",
                   __FUNCTION__);
      return 0;
    }
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                 "%s received invalid payloadtype",
                 __FUNCTION__);
    return -1;
  }
  CheckCSRC(rtp_header);

  const WebRtc_UWord8* payload_data =
      packet + rtp_header->header.headerLength;

  WebRtc_UWord16 payload_data_length =
      static_cast<WebRtc_UWord16>(length - rtp_header->header.headerLength);

  WebRtc_Word32 retVal = 0;
  if(_audio) {
    retVal = ParseAudioCodecSpecific(rtp_header,
                                     payload_data,
                                     payload_data_length,
                                     audio_specific,
                                     is_red);
  } else {
    retVal = ParseVideoCodecSpecific(rtp_header,
                                     payload_data,
                                     payload_data_length,
                                     video_specific.videoCodecType,
                                     is_red,
                                     packet,
                                     packet_length,
                                     _clock.GetTimeInMS());
  }
  if(retVal < 0) {
    return retVal;
  }

  CriticalSectionScoped lock(_criticalSectionRTPReceiver);

  // this compare to _receivedSeqMax
  // we store the last received after we have done the callback
  bool old_packet = RetransmitOfOldPacket(rtp_header->header.sequenceNumber,
                                          rtp_header->header.timestamp);

  // this updates _receivedSeqMax and other members
  UpdateStatistics(rtp_header, payload_data_length, old_packet);

  // Need to be updated after RetransmitOfOldPacket &
  // RetransmitOfOldPacketUpdateStatistics
  _lastReceiveTime = _clock.GetTimeInMS();
  _lastReceivedPayloadLength = payload_data_length;

  if (!old_packet) {
    if (_lastReceivedTimestamp != rtp_header->header.timestamp) {
      _lastReceivedTimestamp = rtp_header->header.timestamp;
    }
    _lastReceivedSequenceNumber = rtp_header->header.sequenceNumber;
    _lastReceivedTransmissionTimeOffset =
        rtp_header->extension.transmissionTimeOffset;
  }
  return retVal;
}

// must not have critsect when called
WebRtc_Word32
RTPReceiver::CallbackOfReceivedPayloadData(const WebRtc_UWord8* payloadData,
                                           const WebRtc_UWord16 payloadSize,
                                           const WebRtcRTPHeader* rtpHeader)
{
    CriticalSectionScoped lock(_criticalSectionCbs);
    if(_cbRtpData)
    {
        return _cbRtpData->OnReceivedPayloadData(payloadData, payloadSize, rtpHeader);
    }
    return -1;
}

// we already have the _criticalSectionRTPReceiver critsect when we call this
void
RTPReceiver::UpdateStatistics(const WebRtcRTPHeader* rtpHeader,
                              const WebRtc_UWord16 bytes,
                              const bool oldPacket)
{
    WebRtc_UWord32 freq = 90000;
    if(_audio)
    {
        freq = AudioFrequency();
    }

    Bitrate::Update(bytes);

    _receivedByteCount += bytes;

    if (_receivedSeqMax == 0 && _receivedSeqWraps == 0)
    {
        // First received report
        _receivedSeqFirst = rtpHeader->header.sequenceNumber;
        _receivedSeqMax = rtpHeader->header.sequenceNumber;
        _receivedInorderPacketCount = 1;
        _localTimeLastReceivedTimestamp =
            GetCurrentRTP(&_clock, freq); //time in samples
        return;
    }

    // count only the new packets received
    if(InOrderPacket(rtpHeader->header.sequenceNumber))
    {
        const WebRtc_UWord32 RTPtime =
            GetCurrentRTP(&_clock, freq); //time in samples
        _receivedInorderPacketCount++;

        // wrong if we use RetransmitOfOldPacket
        WebRtc_Word32 seqDiff = rtpHeader->header.sequenceNumber - _receivedSeqMax;
        if (seqDiff < 0)
        {
            // Wrap around detected
            _receivedSeqWraps++;
        }
        // new max
        _receivedSeqMax = rtpHeader->header.sequenceNumber;

        if (rtpHeader->header.timestamp != _lastReceivedTimestamp &&
            _receivedInorderPacketCount > 1)
        {
            WebRtc_Word32 timeDiffSamples = (RTPtime - _localTimeLastReceivedTimestamp) -
                                          (rtpHeader->header.timestamp - _lastReceivedTimestamp);

            timeDiffSamples = abs(timeDiffSamples);

            // libJingle sometimes deliver crazy jumps in TS for the same stream
            // If this happen don't update jitter value
            if(timeDiffSamples < 450000)  // Use 5 secs video frequency as border
            {
                // note we calculate in Q4 to avoid using float
                WebRtc_Word32 jitterDiffQ4 = (timeDiffSamples << 4) - _jitterQ4;
                _jitterQ4 += ((jitterDiffQ4 + 8) >> 4);
            }

            // Extended jitter report, RFC 5450.
            // Actual network jitter, excluding the source-introduced jitter.
            WebRtc_Word32 timeDiffSamplesExt =
                (RTPtime - _localTimeLastReceivedTimestamp) -
                ((rtpHeader->header.timestamp +
                  rtpHeader->extension.transmissionTimeOffset) -
                (_lastReceivedTimestamp +
                 _lastReceivedTransmissionTimeOffset));

            timeDiffSamplesExt = abs(timeDiffSamplesExt);

            if(timeDiffSamplesExt < 450000)  // Use 5 secs video freq as border
            {
                // note we calculate in Q4 to avoid using float
                WebRtc_Word32 jitterDiffQ4TransmissionTimeOffset =
                    (timeDiffSamplesExt << 4) - _jitterQ4TransmissionTimeOffset;
                _jitterQ4TransmissionTimeOffset +=
                    ((jitterDiffQ4TransmissionTimeOffset + 8) >> 4);
            }
        }
        _localTimeLastReceivedTimestamp = RTPtime;
    } else
    {
        if(oldPacket)
        {
            _receivedOldPacketCount++;
        }else
        {
            _receivedInorderPacketCount++;
        }
    }

    WebRtc_UWord16 packetOH = rtpHeader->header.headerLength + rtpHeader->header.paddingLength;

    // our measured overhead
    // filter from RFC 5104     4.2.1.2
    // avg_OH (new) = 15/16*avg_OH (old) + 1/16*pckt_OH,
    _receivedPacketOH =  (15*_receivedPacketOH + packetOH) >> 4;
}

// we already have the _criticalSectionRTPReceiver critsect when we call this
bool RTPReceiver::RetransmitOfOldPacket(
    const WebRtc_UWord16 sequenceNumber,
    const WebRtc_UWord32 rtpTimeStamp) const {
  if (InOrderPacket(sequenceNumber)) {
    return false;
  }
  WebRtc_UWord32 frequencyKHz = 90;  // Video frequency.
  if (_audio) {
    frequencyKHz = AudioFrequency() / 1000;
  }
  WebRtc_Word64 timeDiffMS = _clock.GetTimeInMS() - _lastReceiveTime;
  // Diff in time stamp since last received in order.
  WebRtc_Word32 rtpTimeStampDiffMS = static_cast<WebRtc_Word32>(
      rtpTimeStamp - _lastReceivedTimestamp) / frequencyKHz;

  WebRtc_UWord16 minRTT = 0;
  WebRtc_Word32 maxDelayMs = 0;
  _rtpRtcp.RTT(_SSRC, NULL, NULL, &minRTT, NULL);
  if (minRTT == 0) {
    float jitter = _jitterQ4 >> 4;  // Jitter variance in samples.
    // Jitter standard deviation in samples.
    float jitterStd = sqrt(jitter);
    // 2 times the std deviation => 95% confidence.
    // And transform to ms by dividing by the frequency in kHz.
    maxDelayMs = static_cast<WebRtc_Word32>((2 * jitterStd) / frequencyKHz);

    // Min maxDelayMs is 1.
    if (maxDelayMs == 0) {
      maxDelayMs = 1; 
    }
  } else {
    maxDelayMs = (minRTT / 3) + 1;
  }
  if (timeDiffMS > rtpTimeStampDiffMS + maxDelayMs) {
    return true;
  }
  return false;
}

bool
RTPReceiver::InOrderPacket(const WebRtc_UWord16 sequenceNumber) const
{
    if(_receivedSeqMax >= sequenceNumber)
    {
        if(!(_receivedSeqMax > 0xff00 && sequenceNumber < 0x0ff ))//detect wrap around
        {
            if(_receivedSeqMax - NACK_PACKETS_MAX_SIZE > sequenceNumber)
            {
                // we have a restart of the remote side
            }else
            {
                // we received a retransmit of a packet we already have
                return false;
            }
        }
    }else
    {
        // check for a wrap
        if(sequenceNumber > 0xff00 && _receivedSeqMax < 0x0ff )//detect wrap around
        {
            if(_receivedSeqMax - NACK_PACKETS_MAX_SIZE > sequenceNumber)
            {
                // we have a restart of the remote side
            }else
            {
                // we received a retransmit of a packet we already have
                return false;
            }
        }
    }
    return true;
}

WebRtc_UWord16
RTPReceiver::SequenceNumber() const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    return _lastReceivedSequenceNumber;
}

WebRtc_UWord32
RTPReceiver::TimeStamp() const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    return _lastReceivedTimestamp;
}

WebRtc_UWord32 RTPReceiver::PayloadTypeToPayload(
    const WebRtc_UWord8 payloadType,
    Payload*& payload) const {

  std::map<WebRtc_Word8, Payload*>::const_iterator it =
      _payloadTypeMap.find(payloadType);

  // check that this is a registered payload type
  if (it == _payloadTypeMap.end()) {
    return -1;
  }
  payload = it->second;
  return 0;
}

// timeStamp of the last incoming packet that is the first packet of its frame
WebRtc_Word32
RTPReceiver::EstimatedRemoteTimeStamp(WebRtc_UWord32& timestamp) const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    WebRtc_UWord32 freq = 90000;
    if(_audio)
    {
        freq = AudioFrequency();
    }
    if(_localTimeLastReceivedTimestamp == 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "%s invalid state", __FUNCTION__);
        return -1;
    }
    //time in samples
    WebRtc_UWord32 diff = GetCurrentRTP(&_clock, freq)
        - _localTimeLastReceivedTimestamp;

    timestamp = _lastReceivedTimestamp + diff;
    return 0;
}

    // get the currently configured SSRC filter
WebRtc_Word32
RTPReceiver::SSRCFilter(WebRtc_UWord32& allowedSSRC) const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);
    if(_useSSRCFilter)
    {
        allowedSSRC = _SSRCFilter;
        return 0;
    }
    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id, "%s invalid state", __FUNCTION__);
    return -1;
}

    // set a SSRC to be used as a filter for incoming RTP streams
WebRtc_Word32
RTPReceiver::SetSSRCFilter(const bool enable, const WebRtc_UWord32 allowedSSRC)
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);

    _useSSRCFilter = enable;
    if(enable)
    {
        _SSRCFilter = allowedSSRC;
    } else
    {
        _SSRCFilter = 0;
    }
    return 0;
}

// no criticalsection when called
void RTPReceiver::CheckSSRCChanged(const WebRtcRTPHeader* rtpHeader) {
  bool newSSRC = false;
  bool reInitializeDecoder = false;
  char payloadName[RTP_PAYLOAD_NAME_SIZE];
  WebRtc_UWord32 frequency = 90000; // default video freq
  WebRtc_UWord8 channels = 1;
  WebRtc_UWord32 rate = 0;

  {
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);

    if (_SSRC != rtpHeader->header.ssrc ||
        (_lastReceivedPayloadType == -1 && _SSRC == 0)) {
      // we need the _payloadType to make the call if the remote SSRC is 0
      newSSRC = true;

      // reset last report
      ResetStatistics();

      _lastReceivedTimestamp      = 0;
      _lastReceivedSequenceNumber = 0;
      _lastReceivedTransmissionTimeOffset = 0;

      if (_SSRC) {  // do we have a SSRC? then the stream is restarted
        //  if we have the same codec? reinit decoder
        if (rtpHeader->header.payloadType == _lastReceivedPayloadType) {
          reInitializeDecoder = true;

          std::map<WebRtc_Word8, Payload*>::iterator it =
              _payloadTypeMap.find(rtpHeader->header.payloadType);

          if (it == _payloadTypeMap.end()) {
            return;
          }
          Payload* payload = it->second;
          assert(payload);
          payloadName[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
          strncpy(payloadName, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);
          if(payload->audio) {
            frequency = payload->typeSpecific.Audio.frequency;
            channels =  payload->typeSpecific.Audio.channels;
            rate = payload->typeSpecific.Audio.rate;
          } else {
            frequency = 90000;
          }
        }
      }
      _SSRC = rtpHeader->header.ssrc;
    }
  }
  if(newSSRC) {
    // we need to get this to our RTCP sender and receiver
    // need to do this outside critical section
    _rtpRtcp.SetRemoteSSRC(rtpHeader->header.ssrc);
  }
  CriticalSectionScoped lock(_criticalSectionCbs);
  if(_cbRtpFeedback) {
    if(newSSRC) {
      _cbRtpFeedback->OnIncomingSSRCChanged(_id, rtpHeader->header.ssrc);
    }
    if(reInitializeDecoder) {
      if (-1 == _cbRtpFeedback->OnInitializeDecoder(_id,
          rtpHeader->header.payloadType, payloadName, frequency, channels,
          rate)) {  // new stream same codec
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                     "Failed to create decoder for payload type:%d",
                     rtpHeader->header.payloadType);
      }
    }
  }
}

// no criticalsection when called
WebRtc_Word32 RTPReceiver::CheckPayloadChanged(
    const WebRtcRTPHeader* rtpHeader,
    const WebRtc_Word8 firstPayloadByte,
    bool& isRED,
    AudioPayload& audioSpecificPayload,
    VideoPayload& videoSpecificPayload) {
  bool reInitializeDecoder = false;

  char payloadName[RTP_PAYLOAD_NAME_SIZE];
  WebRtc_Word8 payloadType = rtpHeader->header.payloadType;

  {
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);

    if (payloadType != _lastReceivedPayloadType) {
      if (REDPayloadType(payloadType)) {
        // get the real codec payload type
        payloadType = firstPayloadByte & 0x7f;
        isRED = true;

        if (REDPayloadType(payloadType)) {
            // Invalid payload type, traced by caller. If we proceeded here,
            // this would be set as |_lastReceivedPayloadType|, and we would no
            // longer catch corrupt packets at this level.
            return -1;
        }

        //when we receive RED we need to check the real payload type
        if (payloadType == _lastReceivedPayloadType) {
          if(_audio)
          {
            memcpy(&audioSpecificPayload, &_lastReceivedAudioSpecific,
                   sizeof(_lastReceivedAudioSpecific));
          } else {
            memcpy(&videoSpecificPayload, &_lastReceivedVideoSpecific,
                   sizeof(_lastReceivedVideoSpecific));
          }
          return 0;
        }
      }
      if (_audio) {
        if (TelephoneEventPayloadType(payloadType)) {
          // don't do callbacks for DTMF packets
          isRED = false;
          return 0;
        }
        // frequency is updated for CNG
        if (CNGPayloadType(payloadType, audioSpecificPayload.frequency)) {
          // don't do callbacks for DTMF packets
          isRED = false;
          return 0;
        }
      }
      std::map<WebRtc_Word8, ModuleRTPUtility::Payload*>::iterator it =
          _payloadTypeMap.find(payloadType);

      // check that this is a registered payload type
      if (it == _payloadTypeMap.end()) {
        return -1;
      }
      Payload* payload = it->second;
      assert(payload);
      payloadName[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
      strncpy(payloadName, payload->name, RTP_PAYLOAD_NAME_SIZE - 1);
      _lastReceivedPayloadType = payloadType;

      reInitializeDecoder = true;

      if(payload->audio) {
        memcpy(&_lastReceivedAudioSpecific, &(payload->typeSpecific.Audio),
               sizeof(_lastReceivedAudioSpecific));
        memcpy(&audioSpecificPayload, &(payload->typeSpecific.Audio),
               sizeof(_lastReceivedAudioSpecific));
      } else {
        memcpy(&_lastReceivedVideoSpecific, &(payload->typeSpecific.Video),
               sizeof(_lastReceivedVideoSpecific));
        memcpy(&videoSpecificPayload, &(payload->typeSpecific.Video),
               sizeof(_lastReceivedVideoSpecific));

        if (_lastReceivedVideoSpecific.videoCodecType == kRtpFecVideo)
        {
          // Only reset the decoder on media packets.
          reInitializeDecoder = false;
        } else {
          if (_lastReceivedMediaPayloadType == _lastReceivedPayloadType) {
            // Only reset the decoder if the media codec type has changed.
            reInitializeDecoder = false;
          }
          _lastReceivedMediaPayloadType = _lastReceivedPayloadType;
        }
      }
      if (reInitializeDecoder) {
        // reset statistics
        ResetStatistics();
      }
    } else {
      if(_audio)
      {
        memcpy(&audioSpecificPayload, &_lastReceivedAudioSpecific,
               sizeof(_lastReceivedAudioSpecific));
      } else
      {
        memcpy(&videoSpecificPayload, &_lastReceivedVideoSpecific,
               sizeof(_lastReceivedVideoSpecific));
      }
      isRED = false;
    }
  }   // end critsect
  if (reInitializeDecoder) {
    CriticalSectionScoped lock(_criticalSectionCbs);
    if (_cbRtpFeedback) {
      // create new decoder instance
      if(_audio) {
        if (-1 == _cbRtpFeedback->OnInitializeDecoder(_id, payloadType,
            payloadName, audioSpecificPayload.frequency,
            audioSpecificPayload.channels, audioSpecificPayload.rate)) {
          WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                       "Failed to create audio decoder for payload type:%d",
                       payloadType);
          return -1; // Wrong payload type
        }
      } else {
        if (-1 == _cbRtpFeedback->OnInitializeDecoder(_id, payloadType,
            payloadName, 90000, 1, 0)) {
          WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                       "Failed to create video decoder for payload type:%d",
                       payloadType);
          return -1; // Wrong payload type
        }
      }
    }
  }
  return 0;
}

// no criticalsection when called
void RTPReceiver::CheckCSRC(const WebRtcRTPHeader* rtpHeader) {
  WebRtc_Word32 numCSRCsDiff = 0;
  WebRtc_UWord32 oldRemoteCSRC[kRtpCsrcSize];
  WebRtc_UWord8 oldNumCSRCs = 0;

  {
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);

    if (TelephoneEventPayloadType(rtpHeader->header.payloadType)) {
      // Don't do this for DTMF packets
      return;
    }
    _numEnergy = rtpHeader->type.Audio.numEnergy;
    if (rtpHeader->type.Audio.numEnergy > 0 &&
        rtpHeader->type.Audio.numEnergy <= kRtpCsrcSize) {
      memcpy(_currentRemoteEnergy,
             rtpHeader->type.Audio.arrOfEnergy,
             rtpHeader->type.Audio.numEnergy);
    }
    oldNumCSRCs  = _numCSRCs;
    if (oldNumCSRCs > 0) {
      // Make a copy of old.
      memcpy(oldRemoteCSRC, _currentRemoteCSRC,
             _numCSRCs * sizeof(WebRtc_UWord32));
    }
    const WebRtc_UWord8 numCSRCs = rtpHeader->header.numCSRCs;
    if ((numCSRCs > 0) && (numCSRCs <= kRtpCsrcSize)) {
      // Copy new
      memcpy(_currentRemoteCSRC,
             rtpHeader->header.arrOfCSRCs,
             numCSRCs * sizeof(WebRtc_UWord32));
    }
    if (numCSRCs > 0 || oldNumCSRCs > 0) {
      numCSRCsDiff = numCSRCs - oldNumCSRCs;
      _numCSRCs = numCSRCs;  // Update stored CSRCs.
    } else {
      // No change.
      return;
    }
  }  // End scoped CriticalSection.

  CriticalSectionScoped lock(_criticalSectionCbs);
  if (_cbRtpFeedback == NULL) {
    return;
  }
  bool haveCalledCallback = false;
  // Search for new CSRC in old array.
  for (WebRtc_UWord8 i = 0; i < rtpHeader->header.numCSRCs; ++i) {
    const WebRtc_UWord32 csrc = rtpHeader->header.arrOfCSRCs[i];

    bool foundMatch = false;
    for (WebRtc_UWord8 j = 0; j < oldNumCSRCs; ++j) {
      if (csrc == oldRemoteCSRC[j]) {  // old list
        foundMatch = true;
        break;
      }
    }
    if (!foundMatch && csrc) {
      // Didn't find it, report it as new.
      haveCalledCallback = true;
      _cbRtpFeedback->OnIncomingCSRCChanged(_id, csrc, true);
    }
  }
  // Search for old CSRC in new array.
  for (WebRtc_UWord8 i = 0; i < oldNumCSRCs; ++i) {
    const WebRtc_UWord32 csrc = oldRemoteCSRC[i];

    bool foundMatch = false;
    for (WebRtc_UWord8 j = 0; j < rtpHeader->header.numCSRCs; ++j) {
      if (csrc == rtpHeader->header.arrOfCSRCs[j]) {
        foundMatch = true;
        break;
      }
    }
    if (!foundMatch && csrc) {
      // Did not find it, report as removed.
      haveCalledCallback = true;
      _cbRtpFeedback->OnIncomingCSRCChanged(_id, csrc, false);
    }
  }
  if (!haveCalledCallback) {
    // If the CSRC list contain non-unique entries we will endup here.
    // Using CSRC 0 to signal this event, not interop safe, other
    // implementations might have CSRC 0 as avalid value.
    if (numCSRCsDiff > 0) {
      _cbRtpFeedback->OnIncomingCSRCChanged(_id, 0, true);
    } else if (numCSRCsDiff < 0) {
      _cbRtpFeedback->OnIncomingCSRCChanged(_id, 0, false);
    }
  }
}

WebRtc_Word32
RTPReceiver::ResetStatistics()
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);

    _lastReportInorderPackets = 0;
    _lastReportOldPackets = 0;
    _lastReportSeqMax = 0;
    _lastReportFractionLost = 0;
    _lastReportCumulativeLost = 0;
    _lastReportExtendedHighSeqNum = 0;
    _lastReportJitter = 0;
    _lastReportJitterTransmissionTimeOffset = 0;
    _jitterQ4 = 0;
    _jitterMaxQ4 = 0;
    _cumulativeLoss = 0;
    _jitterQ4TransmissionTimeOffset = 0;
    _receivedSeqWraps = 0;
    _receivedSeqMax = 0;
    _receivedSeqFirst = 0;
    _receivedByteCount = 0;
    _receivedOldPacketCount = 0;
    _receivedInorderPacketCount = 0;
    return 0;
}

WebRtc_Word32
RTPReceiver::ResetDataCounters()
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);

    _receivedByteCount = 0;
    _receivedOldPacketCount = 0;
    _receivedInorderPacketCount = 0;
    _lastReportInorderPackets = 0;

    return 0;
}

WebRtc_Word32
RTPReceiver::Statistics(WebRtc_UWord8  *fraction_lost,
                       WebRtc_UWord32 *cum_lost,
                       WebRtc_UWord32 *ext_max,
                       WebRtc_UWord32 *jitter,
                       WebRtc_UWord32 *max_jitter,
                       WebRtc_UWord32 *jitter_transmission_time_offset,
                       bool reset) const
{
    WebRtc_Word32 missing;
    return Statistics(fraction_lost,
                      cum_lost,
                      ext_max,
                      jitter,
                      max_jitter,
                      jitter_transmission_time_offset,
                      &missing,
                      reset);
}

WebRtc_Word32
RTPReceiver::Statistics(WebRtc_UWord8  *fraction_lost,
                        WebRtc_UWord32 *cum_lost,
                        WebRtc_UWord32 *ext_max,
                        WebRtc_UWord32 *jitter,
                        WebRtc_UWord32 *max_jitter,
                        WebRtc_UWord32 *jitter_transmission_time_offset,
                        WebRtc_Word32  *missing,
                        bool reset) const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);

    if (missing == NULL)
    {
        return -1;
    }
    if(_receivedSeqFirst == 0 && _receivedByteCount == 0)
    {
        // we have not received anything
        // -1 required by RTCP sender
        return -1;
    }
    if(!reset)
    {
        if(_lastReportInorderPackets == 0)
        {
            // no report
            return -1;
        }
        // just get last report
        if(fraction_lost)
        {
            *fraction_lost = _lastReportFractionLost;
        }
        if(cum_lost)
        {
            *cum_lost = _lastReportCumulativeLost;  // 24 bits valid
        }
        if(ext_max)
        {
            *ext_max = _lastReportExtendedHighSeqNum;
        }
        if(jitter)
        {
            *jitter =_lastReportJitter;
        }
        if(max_jitter)
        {
            // note that the internal jitter value is in Q4
            // and needs to be scaled by 1/16
            *max_jitter = (_jitterMaxQ4 >> 4);
        }
        if(jitter_transmission_time_offset)
        {
            *jitter_transmission_time_offset =
               _lastReportJitterTransmissionTimeOffset;
        }
        return 0;
    }

    if (_lastReportInorderPackets == 0)
    {
        // First time we send a report
        _lastReportSeqMax = _receivedSeqFirst-1;
    }
    /*
    *   calc fraction lost
    */
    WebRtc_UWord16 expSinceLast = (_receivedSeqMax - _lastReportSeqMax);

    if(_lastReportSeqMax > _receivedSeqMax)
    {
        // can we assume that the seqNum can't go decrease over a full RTCP period ?
        expSinceLast = 0;
    }

    // number of received RTP packets since last report, counts all packets but not re-transmissions
    WebRtc_UWord32 recSinceLast = _receivedInorderPacketCount - _lastReportInorderPackets;

    if(_nackMethod == kNackOff)
    {
        // this is needed for re-ordered packets
        WebRtc_UWord32 oldPackets = _receivedOldPacketCount - _lastReportOldPackets;
        recSinceLast += oldPackets;
    }else
    {
        // with NACK we don't know the expected retransmitions during the last second
        // we know how many "old" packets we have received we just count the numer of
        // old received to estimate the loss but it still does not guarantee an exact number
        // since we run this based on time triggered by sending of a RTP packet this
        // should have a minimum effect

        // with NACK we don't count old packets as received since they are re-transmitted
        // we use RTT to decide if a packet is re-ordered or re-transmitted
    }

    *missing = 0;
    if(expSinceLast > recSinceLast)
    {
        *missing = (expSinceLast - recSinceLast);
    }
    WebRtc_UWord8 fractionLost = 0;
    if(expSinceLast)
    {
        // scale 0 to 255, where 255 is 100% loss
        fractionLost = (WebRtc_UWord8) ((255 * (*missing)) / expSinceLast);
    }
    if(fraction_lost)
    {
        *fraction_lost = fractionLost;
    }
    // we need a counter for cumulative loss too
    _cumulativeLoss += *missing;

    if(_jitterQ4 > _jitterMaxQ4)
    {
        _jitterMaxQ4 = _jitterQ4;
    }
    if(cum_lost)
    {
        *cum_lost =  _cumulativeLoss;
    }
    if(ext_max)
    {
        *ext_max = (_receivedSeqWraps<<16) + _receivedSeqMax;
    }
    if(jitter)
    {
        // note that the internal jitter value is in Q4
        // and needs to be scaled by 1/16
        *jitter = (_jitterQ4 >> 4);
    }
    if(max_jitter)
    {
        // note that the internal jitter value is in Q4
        // and needs to be scaled by 1/16
        *max_jitter = (_jitterMaxQ4 >> 4);
    }
    if(jitter_transmission_time_offset)
    {
        // note that the internal jitter value is in Q4
        // and needs to be scaled by 1/16
        *jitter_transmission_time_offset =
            (_jitterQ4TransmissionTimeOffset >> 4);
    }
    if(reset)
    {
        // store this report
        _lastReportFractionLost = fractionLost;
        _lastReportCumulativeLost = _cumulativeLoss;  // 24 bits valid
        _lastReportExtendedHighSeqNum = (_receivedSeqWraps<<16) + _receivedSeqMax;
        _lastReportJitter  = (_jitterQ4 >> 4);
        _lastReportJitterTransmissionTimeOffset =
            (_jitterQ4TransmissionTimeOffset >> 4);

        // only for report blocks in RTCP SR and RR
        _lastReportInorderPackets = _receivedInorderPacketCount;
        _lastReportOldPackets = _receivedOldPacketCount;
        _lastReportSeqMax = _receivedSeqMax;
    }
    return 0;
}

WebRtc_Word32
RTPReceiver::DataCounters(WebRtc_UWord32 *bytesReceived,
                          WebRtc_UWord32 *packetsReceived) const
{
    CriticalSectionScoped lock(_criticalSectionRTPReceiver);

    if(bytesReceived)
    {
        *bytesReceived = _receivedByteCount;
    }
    if(packetsReceived)
    {
        *packetsReceived = _receivedOldPacketCount + _receivedInorderPacketCount;
    }
    return 0;
}

void
RTPReceiver::ProcessBitrate()
{
    CriticalSectionScoped cs(_criticalSectionRTPReceiver);

    Bitrate::Process();
}
} // namespace webrtc
