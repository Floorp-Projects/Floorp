/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_WIN_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_WIN_H

#include "typedefs.h"
#include "audio_device.h"
#include "critical_section_wrapper.h"
#include <Windows.h>
#include <mmsystem.h>

namespace webrtc {

class AudioMixerManager
{
public:
    enum { MAX_NUMBER_MIXER_DEVICES = 40 };
    enum { MAX_NUMBER_OF_LINE_CONTROLS = 20 };
    enum { MAX_NUMBER_OF_MULTIPLE_ITEMS = 20 };
    struct SpeakerLineInfo
    {
        DWORD dwLineID;
        bool  speakerIsValid;
        DWORD dwVolumeControlID;
        bool  volumeControlIsValid;
        DWORD dwMuteControlID;
        bool  muteControlIsValid;
    };
    struct MicrophoneLineInfo
    {
        DWORD dwLineID;
        bool  microphoneIsValid;
        DWORD dwVolumeControlID;
        bool  volumeControlIsValid;
        DWORD dwMuteControlID;
        bool  muteControlIsValid;
        DWORD dwOnOffControlID;
        bool  onOffControlIsValid;
    };
public:
    WebRtc_Word32 EnumerateAll();
    WebRtc_Word32 EnumerateSpeakers();
    WebRtc_Word32 EnumerateMicrophones();
    WebRtc_Word32 OpenSpeaker(AudioDeviceModule::WindowsDeviceType device);
    WebRtc_Word32 OpenSpeaker(WebRtc_UWord16 index);
    WebRtc_Word32 OpenMicrophone(AudioDeviceModule::WindowsDeviceType device);
    WebRtc_Word32 OpenMicrophone(WebRtc_UWord16 index);
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
    UINT Devices() const;

private:
    UINT DestinationLines(UINT mixId) const;
    UINT SourceLines(UINT mixId, DWORD destId) const;
    bool GetCapabilities(UINT mixId, MIXERCAPS& caps, bool trace = false) const;
    bool GetDestinationLineInfo(UINT mixId, DWORD destId, MIXERLINE& line, bool trace = false) const;
    bool GetSourceLineInfo(UINT mixId, DWORD destId, DWORD srcId, MIXERLINE& line, bool trace = false) const;

    bool GetAllLineControls(UINT mixId, const MIXERLINE& line, MIXERCONTROL* controlArray, bool trace = false) const;
    bool GetLineControl(UINT mixId, DWORD dwControlID, MIXERCONTROL& control) const;
    bool GetControlDetails(UINT mixId, MIXERCONTROL& controlArray, bool trace = false) const;
    bool GetUnsignedControlValue(UINT mixId, DWORD dwControlID, DWORD& dwValue) const;
    bool SetUnsignedControlValue(UINT mixId, DWORD dwControlID, DWORD dwValue) const;
    bool SetBooleanControlValue(UINT mixId, DWORD dwControlID, bool value) const;
    bool GetBooleanControlValue(UINT mixId, DWORD dwControlID, bool& value) const;
    bool GetSelectedMuxSource(UINT mixId, DWORD dwControlID, DWORD cMultipleItems, UINT& index) const;

private:
    void ClearSpeakerState();
    void ClearSpeakerState(UINT idx);
    void ClearMicrophoneState();
    void ClearMicrophoneState(UINT idx);
    bool SpeakerIsValid(UINT idx) const;
    UINT ValidSpeakers() const;
    bool MicrophoneIsValid(UINT idx) const;
    UINT ValidMicrophones() const;

    void TraceStatusAndSupportFlags(DWORD fdwLine) const;
    void TraceTargetType(DWORD dwType) const;
    void TraceComponentType(DWORD dwComponentType) const;
    void TraceControlType(DWORD dwControlType) const;
    void TraceControlStatusAndSupportFlags(DWORD fdwControl) const;
    void TraceWaveInError(MMRESULT error) const;
    void TraceWaveOutError(MMRESULT error) const;
    // Converts from wide-char to UTF-8 if UNICODE is defined.
    // Does nothing if UNICODE is undefined.
    char* WideToUTF8(const TCHAR* src) const;

public:
    AudioMixerManager(const WebRtc_Word32 id);
    ~AudioMixerManager();

private:
    CriticalSectionWrapper& _critSect;
    WebRtc_Word32           _id;
    HMIXER                  _outputMixerHandle;
    UINT                    _outputMixerID;
    HMIXER                  _inputMixerHandle;
    UINT                    _inputMixerID;
    SpeakerLineInfo         _speakerState[MAX_NUMBER_MIXER_DEVICES];
    MicrophoneLineInfo      _microphoneState[MAX_NUMBER_MIXER_DEVICES];
    mutable char            _str[MAXERRORLENGTH];
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_MIXER_MANAGER_H
