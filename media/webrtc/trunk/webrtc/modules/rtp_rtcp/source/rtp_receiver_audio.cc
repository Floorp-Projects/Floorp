/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_receiver_audio.h"

#include <cassert> //assert
#include <cstring> // memcpy()
#include <math.h>    // pow()

#include "critical_section_wrapper.h"
#include "rtp_receiver.h"
#include "trace.h"

namespace webrtc {
RTPReceiverAudio::RTPReceiverAudio(const WebRtc_Word32 id,
                                   RTPReceiver* parent,
                                   RtpAudioFeedback* incomingMessagesCallback)
  : _id(id),
    _parent(parent),
    _criticalSectionRtpReceiverAudio(
        CriticalSectionWrapper::CreateCriticalSection()),
    _lastReceivedFrequency(8000),
    _telephoneEvent(false),
    _telephoneEventForwardToDecoder(false),
    _telephoneEventDetectEndOfTone(false),
    _telephoneEventPayloadType(-1),
    _cngNBPayloadType(-1),
    _cngWBPayloadType(-1),
    _cngSWBPayloadType(-1),
    _cngFBPayloadType(-1),
    _cngPayloadType(-1),
    _G722PayloadType(-1),
    _lastReceivedG722(false),
    _cbAudioFeedback(incomingMessagesCallback)
{
  last_payload_.Audio.channels = 1;
}

WebRtc_UWord32
RTPReceiverAudio::AudioFrequency() const
{
    CriticalSectionScoped lock(_criticalSectionRtpReceiverAudio.get());
    if(_lastReceivedG722)
    {
        return 8000;
    }
    return _lastReceivedFrequency;
}

// Outband TelephoneEvent(DTMF) detection
WebRtc_Word32
RTPReceiverAudio::SetTelephoneEventStatus(const bool enable,
                                          const bool forwardToDecoder,
                                          const bool detectEndOfTone)
{
    CriticalSectionScoped lock(_criticalSectionRtpReceiverAudio.get());
    _telephoneEvent= enable;
    _telephoneEventDetectEndOfTone = detectEndOfTone;
    _telephoneEventForwardToDecoder = forwardToDecoder;
    return 0;
}

 // Is outband TelephoneEvent(DTMF) turned on/off?
bool
RTPReceiverAudio::TelephoneEvent() const
{
    CriticalSectionScoped lock(_criticalSectionRtpReceiverAudio.get());
    return _telephoneEvent;
}

// Is forwarding of outband telephone events turned on/off?
bool
RTPReceiverAudio::TelephoneEventForwardToDecoder() const
{
    CriticalSectionScoped lock(_criticalSectionRtpReceiverAudio.get());
    return _telephoneEventForwardToDecoder;
}

bool
RTPReceiverAudio::TelephoneEventPayloadType(const WebRtc_Word8 payloadType) const
{
    CriticalSectionScoped lock(_criticalSectionRtpReceiverAudio.get());
    return (_telephoneEventPayloadType == payloadType)?true:false;
}

bool
RTPReceiverAudio::CNGPayloadType(const WebRtc_Word8 payloadType,
                                 WebRtc_UWord32* frequency,
                                 bool* cngPayloadTypeHasChanged)
{
    CriticalSectionScoped lock(_criticalSectionRtpReceiverAudio.get());
    *cngPayloadTypeHasChanged = false;

    //  We can have four CNG on 8000Hz, 16000Hz, 32000Hz and 48000Hz.
    if(_cngNBPayloadType == payloadType)
    {
        *frequency = 8000;
        if ((_cngPayloadType != -1) &&(_cngPayloadType !=_cngNBPayloadType))
            *cngPayloadTypeHasChanged = true;

        _cngPayloadType = _cngNBPayloadType;
        return true;
    } else if(_cngWBPayloadType == payloadType)
    {
        // if last received codec is G.722 we must use frequency 8000
        if(_lastReceivedG722)
        {
            *frequency = 8000;
        } else
        {
            *frequency = 16000;
        }
        if ((_cngPayloadType != -1) &&(_cngPayloadType !=_cngWBPayloadType))
            *cngPayloadTypeHasChanged = true;
        _cngPayloadType = _cngWBPayloadType;
        return true;
    }else if(_cngSWBPayloadType == payloadType)
    {
        *frequency = 32000;
        if ((_cngPayloadType != -1) &&(_cngPayloadType !=_cngSWBPayloadType))
            *cngPayloadTypeHasChanged = true;
        _cngPayloadType = _cngSWBPayloadType;
        return true;
    }else if(_cngFBPayloadType == payloadType)
    {
        *frequency = 48000;
        if ((_cngPayloadType != -1) &&(_cngPayloadType !=_cngFBPayloadType))
            *cngPayloadTypeHasChanged = true;
        _cngPayloadType = _cngFBPayloadType;
        return true;
    }else
    {
        //  not CNG
        if(_G722PayloadType == payloadType)
        {
            _lastReceivedG722 = true;
        }else
        {
            _lastReceivedG722 = false;
        }
    }
    return false;
}

/*
   Sample based or frame based codecs based on RFC 3551

   NOTE! There is one error in the RFC, stating G.722 uses 8 bits/samples.
   The correct rate is 4 bits/sample.

   name of                              sampling              default
   encoding  sample/frame  bits/sample      rate  ms/frame  ms/packet

   Sample based audio codecs
   DVI4      sample        4                var.                   20
   G722      sample        4              16,000                   20
   G726-40   sample        5               8,000                   20
   G726-32   sample        4               8,000                   20
   G726-24   sample        3               8,000                   20
   G726-16   sample        2               8,000                   20
   L8        sample        8                var.                   20
   L16       sample        16               var.                   20
   PCMA      sample        8                var.                   20
   PCMU      sample        8                var.                   20

   Frame based audio codecs
   G723      frame         N/A             8,000        30         30
   G728      frame         N/A             8,000       2.5         20
   G729      frame         N/A             8,000        10         20
   G729D     frame         N/A             8,000        10         20
   G729E     frame         N/A             8,000        10         20
   GSM       frame         N/A             8,000        20         20
   GSM-EFR   frame         N/A             8,000        20         20
   LPC       frame         N/A             8,000        20         20
   MPA       frame         N/A              var.      var.

   G7221     frame         N/A
*/

ModuleRTPUtility::Payload* RTPReceiverAudio::CreatePayloadType(
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const WebRtc_Word8 payloadType,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) {
  CriticalSectionScoped lock(_criticalSectionRtpReceiverAudio.get());

  if (ModuleRTPUtility::StringCompare(payloadName, "telephone-event", 15)) {
    _telephoneEventPayloadType = payloadType;
  }
  if (ModuleRTPUtility::StringCompare(payloadName, "cn", 2)) {
    //  we can have three CNG on 8000Hz, 16000Hz and 32000Hz
    if(frequency == 8000){
      _cngNBPayloadType = payloadType;
    } else if(frequency == 16000) {
      _cngWBPayloadType = payloadType;
    } else if(frequency == 32000) {
      _cngSWBPayloadType = payloadType;
    } else if(frequency == 48000) {
      _cngFBPayloadType = payloadType;
    } else {
      assert(false);
      return NULL;
    }
  }

  ModuleRTPUtility::Payload* payload = new ModuleRTPUtility::Payload;
  payload->name[RTP_PAYLOAD_NAME_SIZE - 1] = 0;
  strncpy(payload->name, payloadName, RTP_PAYLOAD_NAME_SIZE - 1);
  payload->typeSpecific.Audio.frequency = frequency;
  payload->typeSpecific.Audio.channels = channels;
  payload->typeSpecific.Audio.rate = rate;
  payload->audio = true;
  return payload;
}

void RTPReceiverAudio::SendTelephoneEvents(
    WebRtc_UWord8 numberOfNewEvents,
    WebRtc_UWord8 newEvents[MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS],
    WebRtc_UWord8 numberOfRemovedEvents,
    WebRtc_UWord8 removedEvents[MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS]) {

    // Copy these variables since we can't hold the critsect when we call the
    // callback. _cbAudioFeedback and _id are immutable though.
    bool telephoneEvent;
    bool telephoneEventDetectEndOfTone;
    {
      CriticalSectionScoped lock(_criticalSectionRtpReceiverAudio.get());
      telephoneEvent = _telephoneEvent;
      telephoneEventDetectEndOfTone = _telephoneEventDetectEndOfTone;
    }
    if (telephoneEvent) {
        for (int n = 0; n < numberOfNewEvents; ++n) {
            _cbAudioFeedback->OnReceivedTelephoneEvent(
                _id, newEvents[n], false);
        }
        if (telephoneEventDetectEndOfTone) {
            for (int n = 0; n < numberOfRemovedEvents; ++n) {
                _cbAudioFeedback->OnReceivedTelephoneEvent(
                    _id, removedEvents[n], true);
            }
        }
    }
}

WebRtc_Word32 RTPReceiverAudio::ParseRtpPacket(
    WebRtcRTPHeader* rtpHeader,
    const ModuleRTPUtility::PayloadUnion& specificPayload,
    const bool isRed,
    const WebRtc_UWord8* packet,
    const WebRtc_UWord16 packetLength,
    const WebRtc_Word64 timestampMs) {

    const WebRtc_UWord8* payloadData =
        ModuleRTPUtility::GetPayloadData(rtpHeader, packet);
    const WebRtc_UWord16 payloadDataLength =
        ModuleRTPUtility::GetPayloadDataLength(rtpHeader, packetLength);

    return ParseAudioCodecSpecific(rtpHeader, payloadData, payloadDataLength,
                                   specificPayload.Audio, isRed);
}

WebRtc_Word32 RTPReceiverAudio::GetFrequencyHz() const {
  return AudioFrequency();
}

RTPAliveType RTPReceiverAudio::ProcessDeadOrAlive(
    WebRtc_UWord16 lastPayloadLength) const {

    // Our CNG is 9 bytes; if it's a likely CNG the receiver needs to check
    // kRtpNoRtp against NetEq speechType kOutputPLCtoCNG.
    if(lastPayloadLength < 10)  // our CNG is 9 bytes
    {
        return kRtpNoRtp;
    } else
    {
        return kRtpDead;
    }
}

bool RTPReceiverAudio::PayloadIsCompatible(
    const ModuleRTPUtility::Payload& payload,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) const {
  return
      payload.audio &&
      payload.typeSpecific.Audio.frequency == frequency &&
      payload.typeSpecific.Audio.channels == channels &&
      (payload.typeSpecific.Audio.rate == rate ||
          payload.typeSpecific.Audio.rate == 0 || rate == 0);
}

void RTPReceiverAudio::UpdatePayloadRate(
    ModuleRTPUtility::Payload* payload,
    const WebRtc_UWord32 rate) const {
  payload->typeSpecific.Audio.rate = rate;
}

void RTPReceiverAudio::PossiblyRemoveExistingPayloadType(
    ModuleRTPUtility::PayloadTypeMap* payloadTypeMap,
    const char payloadName[RTP_PAYLOAD_NAME_SIZE],
    const size_t payloadNameLength,
    const WebRtc_UWord32 frequency,
    const WebRtc_UWord8 channels,
    const WebRtc_UWord32 rate) const {
  ModuleRTPUtility::PayloadTypeMap::iterator audio_it = payloadTypeMap->begin();
  while (audio_it != payloadTypeMap->end()) {
    ModuleRTPUtility::Payload* payload = audio_it->second;
    size_t nameLength = strlen(payload->name);

    if (payloadNameLength == nameLength &&
        ModuleRTPUtility::StringCompare(payload->name,
                                        payloadName, payloadNameLength)) {
      // we found the payload name in the list
      // if audio check frequency and rate
      if (payload->audio) {
        if (payload->typeSpecific.Audio.frequency == frequency &&
            (payload->typeSpecific.Audio.rate == rate ||
                payload->typeSpecific.Audio.rate == 0 || rate == 0) &&
                payload->typeSpecific.Audio.channels == channels) {
          // remove old setting
          delete payload;
          payloadTypeMap->erase(audio_it);
          break;
        }
      } else if(ModuleRTPUtility::StringCompare(payloadName,"red",3)) {
        delete payload;
        payloadTypeMap->erase(audio_it);
        break;
      }
    }
    audio_it++;
  }
}

void RTPReceiverAudio::CheckPayloadChanged(
    const WebRtc_Word8 payloadType,
    ModuleRTPUtility::PayloadUnion* specificPayload,
    bool* shouldResetStatistics,
    bool* shouldDiscardChanges) {
  *shouldDiscardChanges = false;
  *shouldResetStatistics = false;

  if (TelephoneEventPayloadType(payloadType)) {
    // Don't do callbacks for DTMF packets.
    *shouldDiscardChanges = true;
    return;
  }
  // frequency is updated for CNG
  bool cngPayloadTypeHasChanged = false;
  bool isCngPayloadType = CNGPayloadType(
      payloadType, &specificPayload->Audio.frequency,
      &cngPayloadTypeHasChanged);

  *shouldResetStatistics = cngPayloadTypeHasChanged;

  if (isCngPayloadType) {
    // Don't do callbacks for DTMF packets.
    *shouldDiscardChanges = true;
    return;
  }
}

WebRtc_Word32 RTPReceiverAudio::InvokeOnInitializeDecoder(
      RtpFeedback* callback,
      const WebRtc_Word32 id,
      const WebRtc_Word8 payloadType,
      const char payloadName[RTP_PAYLOAD_NAME_SIZE],
      const ModuleRTPUtility::PayloadUnion& specificPayload) const {
  if (-1 == callback->OnInitializeDecoder(
      id, payloadType, payloadName, specificPayload.Audio.frequency,
      specificPayload.Audio.channels, specificPayload.Audio.rate)) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, id,
                 "Failed to create video decoder for payload type:%d",
                 payloadType);
    return -1;
  }
  return 0;
}

// we are not allowed to have any critsects when calling CallbackOfReceivedPayloadData
WebRtc_Word32
RTPReceiverAudio::ParseAudioCodecSpecific(WebRtcRTPHeader* rtpHeader,
                                          const WebRtc_UWord8* payloadData,
                                          const WebRtc_UWord16 payloadLength,
                                          const ModuleRTPUtility::AudioPayload& audioSpecific,
                                          const bool isRED)
{
    WebRtc_UWord8 newEvents[MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS];
    WebRtc_UWord8 removedEvents[MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS];
    WebRtc_UWord8 numberOfNewEvents = 0;
    WebRtc_UWord8 numberOfRemovedEvents = 0;

    if(payloadLength == 0)
    {
        return 0;
    }

    bool telephoneEventPacket = TelephoneEventPayloadType(rtpHeader->header.payloadType);
    if(telephoneEventPacket)
    {
        CriticalSectionScoped lock(_criticalSectionRtpReceiverAudio.get());

        // RFC 4733 2.3
        /*
            0                   1                   2                   3
            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |     event     |E|R| volume    |          duration             |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        */
        if(payloadLength % 4 != 0)
        {
            return -1;
        }
        WebRtc_UWord8 numberOfEvents = payloadLength / 4;

        // sanity
        if(numberOfEvents >= MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS)
        {
            numberOfEvents = MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS;
        }
        for (int n = 0; n < numberOfEvents; n++)
        {
            bool end = (payloadData[(4*n)+1] & 0x80)? true:false;

            std::set<WebRtc_UWord8>::iterator event =
                _telephoneEventReported.find(payloadData[4*n]);

            if(event != _telephoneEventReported.end())
            {
                // we have already seen this event
                if(end)
                {
                    removedEvents[numberOfRemovedEvents]= payloadData[4*n];
                    numberOfRemovedEvents++;
                    _telephoneEventReported.erase(payloadData[4*n]);
                }
            }else
            {
                if(end)
                {
                    // don't add if it's a end of a tone
                }else
                {
                    newEvents[numberOfNewEvents] = payloadData[4*n];
                    numberOfNewEvents++;
                    _telephoneEventReported.insert(payloadData[4*n]);
                }
            }
        }

        // RFC 4733 2.5.1.3 & 2.5.2.3 Long-Duration Events
        // should not be a problem since we don't care about the duration

        // RFC 4733 See 2.5.1.5. & 2.5.2.4.  Multiple Events in a Packet
    }

    // This needs to be called without locks held.
    SendTelephoneEvents(numberOfNewEvents, newEvents, numberOfRemovedEvents,
                        removedEvents);

    {
        CriticalSectionScoped lock(_criticalSectionRtpReceiverAudio.get());

        if(! telephoneEventPacket )
        {
            _lastReceivedFrequency = audioSpecific.frequency;
        }

        // Check if this is a CNG packet, receiver might want to know
        WebRtc_UWord32 ignored;
        bool alsoIgnored;
        if(CNGPayloadType(rtpHeader->header.payloadType, &ignored, &alsoIgnored))
        {
            rtpHeader->type.Audio.isCNG=true;
            rtpHeader->frameType = kAudioFrameCN;
        }else
        {
            rtpHeader->frameType = kAudioFrameSpeech;
            rtpHeader->type.Audio.isCNG=false;
        }

        // check if it's a DTMF event, hence something we can playout
        if(telephoneEventPacket)
        {
            if(!_telephoneEventForwardToDecoder)
            {
                // don't forward event to decoder
                return 0;
            }
            std::set<WebRtc_UWord8>::iterator first =
                _telephoneEventReported.begin();
            if(first != _telephoneEventReported.end() && *first > 15)
            {
                // don't forward non DTMF events
                return 0;
            }
        }
    }
    if(isRED && !(payloadData[0] & 0x80))
    {
        // we recive only one frame packed in a RED packet remove the RED wrapper
        rtpHeader->header.payloadType = payloadData[0];

        // only one frame in the RED strip the one byte to help NetEq
        return _parent->CallbackOfReceivedPayloadData(payloadData+1,
                                                      payloadLength-1,
                                                      rtpHeader);
    }

    rtpHeader->type.Audio.channel = audioSpecific.channels;
    return _parent->CallbackOfReceivedPayloadData(
        payloadData, payloadLength, rtpHeader);
}
} // namespace webrtc
