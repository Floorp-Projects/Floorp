/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_AUDIO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_AUDIO_H_

#include "rtp_rtcp_config.h"          // misc. defines (e.g. MAX_PACKET_LENGTH)
#include "common_types.h"             // Transport
#include "typedefs.h"

#include "dtmf_queue.h"
#include "rtp_utility.h"

#include "rtp_sender.h"

namespace webrtc {
class RTPSenderAudio: public DTMFqueue
{
public:
    RTPSenderAudio(const WebRtc_Word32 id, RtpRtcpClock* clock,
                   RTPSenderInterface* rtpSender);
    virtual ~RTPSenderAudio();

    WebRtc_Word32 RegisterAudioPayload(
        const char payloadName[RTP_PAYLOAD_NAME_SIZE],
        const WebRtc_Word8 payloadType,
        const WebRtc_UWord32 frequency,
        const WebRtc_UWord8 channels,
        const WebRtc_UWord32 rate,
        ModuleRTPUtility::Payload*& payload);

    WebRtc_Word32 SendAudio(const FrameType frameType,
                            const WebRtc_Word8 payloadType,
                            const WebRtc_UWord32 captureTimeStamp,
                            const WebRtc_UWord8* payloadData,
                            const WebRtc_UWord32 payloadSize,
                            const RTPFragmentationHeader* fragmentation);

    // set audio packet size, used to determine when it's time to send a DTMF packet in silence (CNG)
    WebRtc_Word32 SetAudioPacketSize(const WebRtc_UWord16 packetSizeSamples);

    // Set status and ID for header-extension-for-audio-level-indication.
    // Valid ID range is [1,14].
    WebRtc_Word32 SetAudioLevelIndicationStatus(const bool enable,
                                              const WebRtc_UWord8 ID);

    // Get status and ID for header-extension-for-audio-level-indication.
    WebRtc_Word32 AudioLevelIndicationStatus(bool& enable,
                                           WebRtc_UWord8& ID) const;

    // Store the audio level in dBov for header-extension-for-audio-level-indication.
    // Valid range is [0,100]. Actual value is negative.
    WebRtc_Word32 SetAudioLevel(const WebRtc_UWord8 level_dBov);

    // Send a DTMF tone using RFC 2833 (4733)
      WebRtc_Word32 SendTelephoneEvent(const WebRtc_UWord8 key,
                                   const WebRtc_UWord16 time_ms,
                                   const WebRtc_UWord8 level);

    bool SendTelephoneEventActive(WebRtc_Word8& telephoneEvent) const;

    void SetAudioFrequency(const WebRtc_UWord32 f);

    int AudioFrequency() const;

    // Set payload type for Redundant Audio Data RFC 2198
    WebRtc_Word32 SetRED(const WebRtc_Word8 payloadType);

    // Get payload type for Redundant Audio Data RFC 2198
    WebRtc_Word32 RED(WebRtc_Word8& payloadType) const;

    WebRtc_Word32 RegisterAudioCallback(RtpAudioFeedback* messagesCallback);

protected:
    WebRtc_Word32 SendTelephoneEventPacket(const bool ended,
                                         const WebRtc_UWord32 dtmfTimeStamp,
                                         const WebRtc_UWord16 duration,
                                         const bool markerBit); // set on first packet in talk burst

    bool MarkerBit(const FrameType frameType,
                   const WebRtc_Word8 payloadType);

private:
    WebRtc_Word32             _id;
    RtpRtcpClock&             _clock;
    RTPSenderInterface*     _rtpSender;
    CriticalSectionWrapper* _audioFeedbackCritsect;
    RtpAudioFeedback*   _audioFeedback;

    CriticalSectionWrapper*   _sendAudioCritsect;

    WebRtc_UWord32            _frequency;
    WebRtc_UWord16            _packetSizeSamples;

    // DTMF
    bool              _dtmfEventIsOn;
    bool              _dtmfEventFirstPacketSent;
    WebRtc_Word8      _dtmfPayloadType;
    WebRtc_UWord32    _dtmfTimestamp;
    WebRtc_UWord8     _dtmfKey;
    WebRtc_UWord32    _dtmfLengthSamples;
    WebRtc_UWord8     _dtmfLevel;
    WebRtc_Word64     _dtmfTimeLastSent;
    WebRtc_UWord32    _dtmfTimestampLastSent;

    WebRtc_Word8      _REDPayloadType;

    // VAD detection, used for markerbit
    bool              _inbandVADactive;
    WebRtc_Word8      _cngNBPayloadType;
    WebRtc_Word8      _cngWBPayloadType;
    WebRtc_Word8      _cngSWBPayloadType;
    WebRtc_Word8      _cngFBPayloadType;
    WebRtc_Word8      _lastPayloadType;

    // Audio level indication (https://datatracker.ietf.org/doc/draft-lennox-avt-rtp-audio-level-exthdr/)
    bool            _includeAudioLevelIndication;
    WebRtc_UWord8     _audioLevelIndicationID;
    WebRtc_UWord8     _audioLevel_dBov;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_AUDIO_H_
