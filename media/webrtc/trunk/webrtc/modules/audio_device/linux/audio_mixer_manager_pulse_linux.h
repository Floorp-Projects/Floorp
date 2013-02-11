/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_PULSE_LINUX_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_PULSE_LINUX_H

#include "typedefs.h"
#include "audio_device.h"
#include "critical_section_wrapper.h"
#include "pulseaudiosymboltable_linux.h"

#include <stdint.h>
#include <pulse/pulseaudio.h>

#ifndef UINT32_MAX
#define UINT32_MAX  ((uint32_t)-1)
#endif

namespace webrtc
{

class AudioMixerManagerLinuxPulse
{
public:
    WebRtc_Word32 SetPlayStream(pa_stream* playStream);
    WebRtc_Word32 SetRecStream(pa_stream* recStream);
    WebRtc_Word32 OpenSpeaker(WebRtc_UWord16 deviceIndex);
    WebRtc_Word32 OpenMicrophone(WebRtc_UWord16 deviceIndex);
    WebRtc_Word32 SetSpeakerVolume(WebRtc_UWord32 volume);
    WebRtc_Word32 SpeakerVolume(WebRtc_UWord32& volume) const;
    WebRtc_Word32 MaxSpeakerVolume(WebRtc_UWord32& maxVolume) const;
    WebRtc_Word32 MinSpeakerVolume(WebRtc_UWord32& minVolume) const;
    WebRtc_Word32 SpeakerVolumeStepSize(WebRtc_UWord16& stepSize) const;
    WebRtc_Word32 SpeakerVolumeIsAvailable(bool& available);
    WebRtc_Word32 SpeakerMuteIsAvailable(bool& available);
    WebRtc_Word32 SetSpeakerMute(bool enable);
    WebRtc_Word32 StereoPlayoutIsAvailable(bool& available);
    WebRtc_Word32 StereoRecordingIsAvailable(bool& available);
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
    WebRtc_Word32 SetPulseAudioObjects(pa_threaded_mainloop* mainloop,
                                       pa_context* context);
    WebRtc_Word32 Close();
    WebRtc_Word32 CloseSpeaker();
    WebRtc_Word32 CloseMicrophone();
    bool SpeakerIsInitialized() const;
    bool MicrophoneIsInitialized() const;

public:
    AudioMixerManagerLinuxPulse(const WebRtc_Word32 id);
    ~AudioMixerManagerLinuxPulse();

private:
    static void PaSinkInfoCallback(pa_context *c, const pa_sink_info *i,
                                   int eol, void *pThis);
    static void PaSinkInputInfoCallback(pa_context *c,
                                        const pa_sink_input_info *i, int eol,
                                        void *pThis);
    static void PaSourceInfoCallback(pa_context *c, const pa_source_info *i,
                                     int eol, void *pThis);
    static void
        PaSetVolumeCallback(pa_context* /*c*/, int success, void* /*pThis*/);
    void PaSinkInfoCallbackHandler(const pa_sink_info *i, int eol);
    void PaSinkInputInfoCallbackHandler(const pa_sink_input_info *i, int eol);
    void PaSourceInfoCallbackHandler(const pa_source_info *i, int eol);

    void ResetCallbackVariables() const;
    void WaitForOperationCompletion(pa_operation* paOperation) const;
    void PaLock() const;
    void PaUnLock() const;

    bool GetSinkInputInfo() const;
    bool GetSinkInfoByIndex(int device_index)const ;
    bool GetSourceInfoByIndex(int device_index) const;

private:
    CriticalSectionWrapper& _critSect;
    WebRtc_Word32 _id;
    WebRtc_Word16 _paOutputDeviceIndex;
    WebRtc_Word16 _paInputDeviceIndex;

    pa_stream* _paPlayStream;
    pa_stream* _paRecStream;

    pa_threaded_mainloop* _paMainloop;
    pa_context* _paContext;

    mutable WebRtc_UWord32 _paVolume;
    mutable WebRtc_UWord32 _paMute;
    mutable WebRtc_UWord32 _paVolSteps;
    bool _paSpeakerMute;
    mutable WebRtc_UWord32 _paSpeakerVolume;
    mutable WebRtc_UWord8 _paChannels;
    bool _paObjectsSet;
    mutable bool _callbackValues;
};

}

#endif  // MODULES_AUDIO_DEVICE_MAIN_SOURCE_LINUX_AUDIO_MIXER_MANAGER_PULSE_LINUX_H_
