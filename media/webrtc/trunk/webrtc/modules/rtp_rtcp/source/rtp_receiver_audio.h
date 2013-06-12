/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_AUDIO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_AUDIO_H_

#include <set>

#include "rtp_receiver.h"
#include "rtp_receiver_strategy.h"
#include "rtp_rtcp_defines.h"
#include "rtp_utility.h"
#include "scoped_ptr.h"
#include "typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;
class RTPReceiver;

// Handles audio RTP packets. This class is thread-safe.
class RTPReceiverAudio : public RTPReceiverStrategy
{
public:
    RTPReceiverAudio(const WebRtc_Word32 id,
                     RTPReceiver* parent,
                     RtpAudioFeedback* incomingMessagesCallback);

    WebRtc_UWord32 AudioFrequency() const;

    // Outband TelephoneEvent (DTMF) detection
    WebRtc_Word32 SetTelephoneEventStatus(const bool enable,
                                        const bool forwardToDecoder,
                                        const bool detectEndOfTone);

    // Is outband DTMF(AVT) turned on/off?
    bool TelephoneEvent() const ;

    // Is forwarding of outband telephone events turned on/off?
    bool TelephoneEventForwardToDecoder() const ;

    // Is TelephoneEvent configured with payload type payloadType
    bool TelephoneEventPayloadType(const WebRtc_Word8 payloadType) const;

    // Returns true if CNG is configured with payload type payloadType. If so,
    // the frequency and cngPayloadTypeHasChanged are filled in.
    bool CNGPayloadType(const WebRtc_Word8 payloadType,
                        WebRtc_UWord32* frequency,
                        bool* cngPayloadTypeHasChanged);

    WebRtc_Word32 ParseRtpPacket(
        WebRtcRTPHeader* rtpHeader,
        const ModuleRTPUtility::PayloadUnion& specificPayload,
        const bool isRed,
        const WebRtc_UWord8* packet,
        const WebRtc_UWord16 packetLength,
        const WebRtc_Word64 timestampMs);

    WebRtc_Word32 GetFrequencyHz() const;

    RTPAliveType ProcessDeadOrAlive(WebRtc_UWord16 lastPayloadLength) const;

    bool PayloadIsCompatible(
        const ModuleRTPUtility::Payload& payload,
        const WebRtc_UWord32 frequency,
        const WebRtc_UWord8 channels,
        const WebRtc_UWord32 rate) const;

    void UpdatePayloadRate(
        ModuleRTPUtility::Payload* payload,
        const WebRtc_UWord32 rate) const;

    ModuleRTPUtility::Payload* CreatePayloadType(
          const char payloadName[RTP_PAYLOAD_NAME_SIZE],
          const WebRtc_Word8 payloadType,
          const WebRtc_UWord32 frequency,
          const WebRtc_UWord8 channels,
          const WebRtc_UWord32 rate);

    WebRtc_Word32 InvokeOnInitializeDecoder(
          RtpFeedback* callback,
          const WebRtc_Word32 id,
          const WebRtc_Word8 payloadType,
          const char payloadName[RTP_PAYLOAD_NAME_SIZE],
          const ModuleRTPUtility::PayloadUnion& specificPayload) const;

    // We do not allow codecs to have multiple payload types for audio, so we
    // need to override the default behavior (which is to do nothing).
    void PossiblyRemoveExistingPayloadType(
        ModuleRTPUtility::PayloadTypeMap* payloadTypeMap,
        const char payloadName[RTP_PAYLOAD_NAME_SIZE],
        const size_t payloadNameLength,
        const WebRtc_UWord32 frequency,
        const WebRtc_UWord8 channels,
        const WebRtc_UWord32 rate) const;

    // We need to look out for special payload types here and sometimes reset
    // statistics. In addition we sometimes need to tweak the frequency.
    void CheckPayloadChanged(
        const WebRtc_Word8 payloadType,
        ModuleRTPUtility::PayloadUnion* specificPayload,
        bool* shouldResetStatistics,
        bool* shouldDiscardChanges);
private:
    void SendTelephoneEvents(
        WebRtc_UWord8 numberOfNewEvents,
        WebRtc_UWord8 newEvents[MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS],
        WebRtc_UWord8 numberOfRemovedEvents,
        WebRtc_UWord8 removedEvents[MAX_NUMBER_OF_PARALLEL_TELEPHONE_EVENTS]);

    WebRtc_Word32 ParseAudioCodecSpecific(
        WebRtcRTPHeader* rtpHeader,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord16 payloadLength,
        const ModuleRTPUtility::AudioPayload& audioSpecific,
        const bool isRED);

    WebRtc_Word32                      _id;
    RTPReceiver*                       _parent;
    scoped_ptr<CriticalSectionWrapper> _criticalSectionRtpReceiverAudio;

    WebRtc_UWord32                     _lastReceivedFrequency;

    bool                    _telephoneEvent;
    bool                    _telephoneEventForwardToDecoder;
    bool                    _telephoneEventDetectEndOfTone;
    WebRtc_Word8            _telephoneEventPayloadType;
    std::set<WebRtc_UWord8> _telephoneEventReported;

    WebRtc_Word8              _cngNBPayloadType;
    WebRtc_Word8              _cngWBPayloadType;
    WebRtc_Word8              _cngSWBPayloadType;
    WebRtc_Word8              _cngFBPayloadType;
    WebRtc_Word8              _cngPayloadType;

    // G722 is special since it use the wrong number of RTP samples in timestamp VS. number of samples in the frame
    WebRtc_Word8              _G722PayloadType;
    bool                      _lastReceivedG722;

    RtpAudioFeedback*         _cbAudioFeedback;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_RECEIVER_AUDIO_H_
