/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_INTERFACE_AUDIO_CONFERENCE_MIXER_DEFINES_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_INTERFACE_AUDIO_CONFERENCE_MIXER_DEFINES_H_

#include "map_wrapper.h"
#include "module_common_types.h"
#include "typedefs.h"

namespace webrtc {
class MixHistory;

// A callback class that all mixer participants must inherit from/implement.
class MixerParticipant
{
public:
    // The implementation of this function should update audioFrame with new
    // audio every time it's called.
    //
    // If it returns -1, the frame will not be added to the mix.
    virtual WebRtc_Word32 GetAudioFrame(const WebRtc_Word32 id,
                                        AudioFrame& audioFrame) = 0;

    // mixed will be set to true if the participant was mixed this mix iteration
    WebRtc_Word32 IsMixed(bool& mixed) const;

    // This function specifies the sampling frequency needed for the AudioFrame
    // for future GetAudioFrame(..) calls.
    virtual WebRtc_Word32 NeededFrequency(const WebRtc_Word32 id) = 0;

    MixHistory* _mixHistory;
protected:
    MixerParticipant();
    virtual ~MixerParticipant();
};

// Container struct for participant statistics.
struct ParticipantStatistics
{
    WebRtc_Word32 participant;
    WebRtc_Word32 level;
};

class AudioMixerStatusReceiver
{
public:
    // Callback function that provides an array of ParticipantStatistics for the
    // participants that were mixed last mix iteration.
    virtual void MixedParticipants(
        const WebRtc_Word32 id,
        const ParticipantStatistics* participantStatistics,
        const WebRtc_UWord32 size) = 0;
    // Callback function that provides an array of the ParticipantStatistics for
    // the participants that had a positiv VAD last mix iteration.
    virtual void VADPositiveParticipants(
        const WebRtc_Word32 id,
        const ParticipantStatistics* participantStatistics,
        const WebRtc_UWord32 size) = 0;
    // Callback function that provides the audio level of the mixed audio frame
    // from the last mix iteration.
    virtual void MixedAudioLevel(
        const WebRtc_Word32  id,
        const WebRtc_UWord32 level) = 0;
protected:
    AudioMixerStatusReceiver() {}
    virtual ~AudioMixerStatusReceiver() {}
};

class AudioMixerOutputReceiver
{
public:
    // This callback function provides the mixed audio for this mix iteration.
    // Note that uniqueAudioFrames is an array of AudioFrame pointers with the
    // size according to the size parameter.
    virtual void NewMixedAudio(const WebRtc_Word32 id,
                               const AudioFrame& generalAudioFrame,
                               const AudioFrame** uniqueAudioFrames,
                               const WebRtc_UWord32 size) = 0;
protected:
    AudioMixerOutputReceiver() {}
    virtual ~AudioMixerOutputReceiver() {}
};

class AudioRelayReceiver
{
public:
    // This callback function provides the mix decision for this mix iteration.
    // mixerList is a list of elements of the type
    // [int,MixerParticipant*]
    virtual void NewAudioToRelay(const WebRtc_Word32 id,
                                 const MapWrapper& mixerList) = 0;
protected:
    AudioRelayReceiver() {}
    virtual ~AudioRelayReceiver() {}
};
} // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_INTERFACE_AUDIO_CONFERENCE_MIXER_DEFINES_H_
