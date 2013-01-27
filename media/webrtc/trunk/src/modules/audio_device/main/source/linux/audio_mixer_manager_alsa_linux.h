/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_ALSA_LINUX_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_ALSA_LINUX_H

#include "typedefs.h"
#include "audio_device.h"
#include "critical_section_wrapper.h"
#include "alsasymboltable_linux.h"

#include <alsa/asoundlib.h>

namespace webrtc
{

class AudioMixerManagerLinuxALSA
{
public:
    WebRtc_Word32 OpenSpeaker(char* deviceName);
    WebRtc_Word32 OpenMicrophone(char* deviceName);
    WebRtc_Word32 SetSpeakerVolume(WebRtc_UWord32 volume);
    WebRtc_Word32 SpeakerVolume(WebRtc_UWord32& volume) const;
    WebRtc_Word32 MaxSpeakerVolume(WebRtc_UWord32& maxVolume) const;
    WebRtc_Word32 MinSpeakerVolume(WebRtc_UWord32& minVolume) const;
    WebRtc_Word32 SpeakerVolumeStepSize(WebRtc_UWord16& stepSize) const;
    WebRtc_Word32 SpeakerVolumeIsAvailable(bool& available);
    WebRtc_Word32 SpeakerMuteIsAvailable(bool& available);
    WebRtc_Word32 SetSpeakerMute(bool enable);
    WebRtc_Word32 SpeakerMute(bool& enabled) const;
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
    AudioMixerManagerLinuxALSA(const WebRtc_Word32 id);
    ~AudioMixerManagerLinuxALSA();

private:
    WebRtc_Word32 LoadMicMixerElement() const;
    WebRtc_Word32 LoadSpeakerMixerElement() const;
    void GetControlName(char *controlName, char* deviceName) const;

private:
    CriticalSectionWrapper& _critSect;
    WebRtc_Word32 _id;
    mutable snd_mixer_t* _outputMixerHandle;
    char _outputMixerStr[kAdmMaxDeviceNameSize];
    mutable snd_mixer_t* _inputMixerHandle;
    char _inputMixerStr[kAdmMaxDeviceNameSize];
    mutable snd_mixer_elem_t* _outputMixerElement;
    mutable snd_mixer_elem_t* _inputMixerElement;
};

}

#endif  // MODULES_AUDIO_DEVICE_MAIN_SOURCE_LINUX_AUDIO_MIXER_MANAGER_ALSA_LINUX_H_
