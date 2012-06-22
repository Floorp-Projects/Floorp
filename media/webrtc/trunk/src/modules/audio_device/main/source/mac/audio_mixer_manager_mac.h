/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_MAC_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_MAC_H

#include "typedefs.h"
#include "audio_device.h"
#include "critical_section_wrapper.h"

#include <CoreAudio/CoreAudio.h>

namespace webrtc {
	
class AudioMixerManagerMac
{
public:
    WebRtc_Word32 OpenSpeaker(AudioDeviceID deviceID);
    WebRtc_Word32 OpenMicrophone(AudioDeviceID deviceID);
    WebRtc_Word32 SetSpeakerVolume(WebRtc_UWord32 volume);
    WebRtc_Word32 SpeakerVolume(WebRtc_UWord32& volume) const;
    WebRtc_Word32 MaxSpeakerVolume(WebRtc_UWord32& maxVolume) const;
    WebRtc_Word32 MinSpeakerVolume(WebRtc_UWord32& minVolume) const;
    WebRtc_Word32 SpeakerVolumeStepSize(WebRtc_UWord16& stepSize) const;
    WebRtc_Word32 SpeakerVolumeIsAvailable(bool& available);
    WebRtc_Word32 SpeakerMuteIsAvailable(bool& available);
    WebRtc_Word32 SetSpeakerMute(bool enable);
    WebRtc_Word32 SpeakerMute(bool& enabled) const;
    WebRtc_Word32 StereoPlayoutIsAvailable(bool& available);
    WebRtc_Word32 StereoRecordingIsAvailable(bool& available);
    WebRtc_Word32 MicrophoneMuteIsAvailable(bool& available);
    WebRtc_Word32 SetMicrophoneMute(bool enable);
    WebRtc_Word32 MicrophoneMute(bool& enabled) const;
    WebRtc_Word32 MicrophoneBoostIsAvailable(bool& available);
    WebRtc_Word32 SetMicrophoneBoost(bool enable);
    WebRtc_Word32 MicrophoneBoost(bool& enabled) const;
    WebRtc_Word32 MicrophoneVolumeIsAvailable(bool& available);
    WebRtc_Word32 SetMicrophoneVolume(WebRtc_UWord32 volume);
    WebRtc_Word32 MicrophoneVolume(WebRtc_UWord32& volume) const;
    WebRtc_Word32 MaxMicrophoneVolume(WebRtc_UWord32& maxVolume) const;
    WebRtc_Word32 MinMicrophoneVolume(WebRtc_UWord32& minVolume) const;
    WebRtc_Word32 MicrophoneVolumeStepSize(WebRtc_UWord16& stepSize) const;
    WebRtc_Word32 Close();
    WebRtc_Word32 CloseSpeaker();
    WebRtc_Word32 CloseMicrophone();
    bool SpeakerIsInitialized() const;
    bool MicrophoneIsInitialized() const;

public:
    AudioMixerManagerMac(const WebRtc_Word32 id);
    ~AudioMixerManagerMac();

private:
    static void logCAMsg(const TraceLevel level,
                         const TraceModule module,
                         const WebRtc_Word32 id, const char *msg,
                         const char *err);

private:
    CriticalSectionWrapper& _critSect;
    WebRtc_Word32 _id;

    AudioDeviceID _inputDeviceID;
    AudioDeviceID _outputDeviceID;

    WebRtc_UWord16 _noInputChannels;
    WebRtc_UWord16 _noOutputChannels;

};
	
} //namespace webrtc

#endif  // AUDIO_MIXER_MAC_H
