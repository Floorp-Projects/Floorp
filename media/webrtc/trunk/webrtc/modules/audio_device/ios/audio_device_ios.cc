/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <AudioToolbox/AudioServices.h>  // AudioSession

#include "audio_device_ios.h"

#include "trace.h"
#include "thread_wrapper.h"

namespace webrtc {
AudioDeviceIPhone::AudioDeviceIPhone(const int32_t id)
    :
    _ptrAudioBuffer(NULL),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _captureWorkerThread(NULL),
    _captureWorkerThreadId(0),
    _id(id),
    _auVoiceProcessing(NULL),
    _initialized(false),
    _isShutDown(false),
    _recording(false),
    _playing(false),
    _recIsInitialized(false),
    _playIsInitialized(false),
    _recordingDeviceIsSpecified(false),
    _playoutDeviceIsSpecified(false),
    _micIsInitialized(false),
    _speakerIsInitialized(false),
    _AGC(false),
    _adbSampFreq(0),
    _recordingDelay(0),
    _playoutDelay(0),
    _playoutDelayMeasurementCounter(9999),
    _recordingDelayHWAndOS(0),
    _recordingDelayMeasurementCounter(9999),
    _playWarning(0),
    _playError(0),
    _recWarning(0),
    _recError(0),
    _playoutBufferUsed(0),
    _recordingCurrentSeq(0),
    _recordingBufferTotalSize(0) {
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id,
                 "%s created", __FUNCTION__);

    memset(_playoutBuffer, 0, sizeof(_playoutBuffer));
    memset(_recordingBuffer, 0, sizeof(_recordingBuffer));
    memset(_recordingLength, 0, sizeof(_recordingLength));
    memset(_recordingSeqNumber, 0, sizeof(_recordingSeqNumber));
}

AudioDeviceIPhone::~AudioDeviceIPhone() {
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s destroyed", __FUNCTION__);

    Terminate();

    delete &_critSect;
}


// ============================================================================
//                                     API
// ============================================================================

void AudioDeviceIPhone::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    _ptrAudioBuffer = audioBuffer;

    // inform the AudioBuffer about default settings for this implementation
    _ptrAudioBuffer->SetRecordingSampleRate(ENGINE_REC_BUF_SIZE_IN_SAMPLES);
    _ptrAudioBuffer->SetPlayoutSampleRate(ENGINE_PLAY_BUF_SIZE_IN_SAMPLES);
    _ptrAudioBuffer->SetRecordingChannels(N_REC_CHANNELS);
    _ptrAudioBuffer->SetPlayoutChannels(N_PLAY_CHANNELS);
}

int32_t AudioDeviceIPhone::ActiveAudioLayer(
    AudioDeviceModule::AudioLayer& audioLayer) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
    return 0;
}

int32_t AudioDeviceIPhone::Init() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (_initialized) {
        return 0;
    }

    _isShutDown = false;

    // Create and start capture thread
    if (_captureWorkerThread == NULL) {
        _captureWorkerThread
            = ThreadWrapper::CreateThread(RunCapture, this, kRealtimePriority,
                                          "CaptureWorkerThread");

        if (_captureWorkerThread == NULL) {
            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice,
                         _id, "CreateThread() error");
            return -1;
        }

        unsigned int threadID(0);
        bool res = _captureWorkerThread->Start(threadID);
        _captureWorkerThreadId = static_cast<uint32_t>(threadID);
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice,
                     _id, "CaptureWorkerThread started (res=%d)", res);
    } else {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice,
                     _id, "Thread already created");
    }
    _playWarning = 0;
    _playError = 0;
    _recWarning = 0;
    _recError = 0;

    _initialized = true;

    return 0;
}

int32_t AudioDeviceIPhone::Terminate() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    if (!_initialized) {
        return 0;
    }


    // Stop capture thread
    if (_captureWorkerThread != NULL) {
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice,
                     _id, "Stopping CaptureWorkerThread");
        bool res = _captureWorkerThread->Stop();
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice,
                     _id, "CaptureWorkerThread stopped (res=%d)", res);
        delete _captureWorkerThread;
        _captureWorkerThread = NULL;
    }

    // Shut down Audio Unit
    ShutdownPlayOrRecord();

    _isShutDown = true;
    _initialized = false;
    _speakerIsInitialized = false;
    _micIsInitialized = false;
    _playoutDeviceIsSpecified = false;
    _recordingDeviceIsSpecified = false;
    return 0;
}

bool AudioDeviceIPhone::Initialized() const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return (_initialized);
}

int32_t AudioDeviceIPhone::SpeakerIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    // speaker is always available in IOS
    available = true;
    return 0;
}

int32_t AudioDeviceIPhone::InitSpeaker() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                     _id, "  Not initialized");
        return -1;
    }

    if (_playing) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                     _id, "  Cannot init speaker when playing");
        return -1;
    }

    if (!_playoutDeviceIsSpecified) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                     _id, "  Playout device is not specified");
        return -1;
    }

    // Do nothing
    _speakerIsInitialized = true;

    return 0;
}

int32_t AudioDeviceIPhone::MicrophoneIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    available = false;

    OSStatus result = -1;
    UInt32 channel = 0;
    UInt32 size = sizeof(channel);
    result = AudioSessionGetProperty(kAudioSessionProperty_AudioInputAvailable,
                                     &size, &channel);
    if (channel != 0) {
        // API is not supported on this platform, we return available = true
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice,
                     _id, "  API call not supported on this version");
        available = true;
        return 0;
    }

    available = (channel == 0) ? false : true;

    return 0;
}

int32_t AudioDeviceIPhone::InitMicrophone() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                     _id, "  Not initialized");
        return -1;
    }

    if (_recording) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                     _id, "  Cannot init mic when recording");
        return -1;
    }

    if (!_recordingDeviceIsSpecified) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice,
                     _id, "  Recording device is not specified");
        return -1;
    }

    // Do nothing

    _micIsInitialized = true;

    return 0;
}

bool AudioDeviceIPhone::SpeakerIsInitialized() const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return _speakerIsInitialized;
}

bool AudioDeviceIPhone::MicrophoneIsInitialized() const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return _micIsInitialized;
}

int32_t AudioDeviceIPhone::SpeakerVolumeIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    available = false;  // Speaker volume not supported on iOS

    return 0;
}

int32_t AudioDeviceIPhone::SetSpeakerVolume(uint32_t volume) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetSpeakerVolume(volume=%u)", volume);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceIPhone::SpeakerVolume(uint32_t& volume) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t
    AudioDeviceIPhone::SetWaveOutVolume(uint16_t volumeLeft,
                                        uint16_t volumeRight) {
    WEBRTC_TRACE(
        kTraceModuleCall,
        kTraceAudioDevice,
        _id,
        "AudioDeviceIPhone::SetWaveOutVolume(volumeLeft=%u, volumeRight=%u)",
        volumeLeft, volumeRight);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");

    return -1;
}

int32_t
AudioDeviceIPhone::WaveOutVolume(uint16_t& /*volumeLeft*/,
                                 uint16_t& /*volumeRight*/) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t
    AudioDeviceIPhone::MaxSpeakerVolume(uint32_t& maxVolume) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceIPhone::MinSpeakerVolume(
    uint32_t& minVolume) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t
    AudioDeviceIPhone::SpeakerVolumeStepSize(uint16_t& stepSize) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceIPhone::SpeakerMuteIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    available = false;  // Speaker mute not supported on iOS

    return 0;
}

int32_t AudioDeviceIPhone::SetSpeakerMute(bool enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceIPhone::SpeakerMute(bool& enabled) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceIPhone::MicrophoneMuteIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    available = false;  // Mic mute not supported on iOS

    return 0;
}

int32_t AudioDeviceIPhone::SetMicrophoneMute(bool enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceIPhone::MicrophoneMute(bool& enabled) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t AudioDeviceIPhone::MicrophoneBoostIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    available = false;  // Mic boost not supported on iOS

    return 0;
}

int32_t AudioDeviceIPhone::SetMicrophoneBoost(bool enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetMicrophoneBoost(enable=%u)", enable);

    if (!_micIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Microphone not initialized");
        return -1;
    }

    if (enable) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  SetMicrophoneBoost cannot be enabled on this platform");
        return -1;
    }

    return 0;
}

int32_t AudioDeviceIPhone::MicrophoneBoost(bool& enabled) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    if (!_micIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Microphone not initialized");
        return -1;
    }

    enabled = false;

    return 0;
}

int32_t AudioDeviceIPhone::StereoRecordingIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    available = false;  // Stereo recording not supported on iOS

    return 0;
}

int32_t AudioDeviceIPhone::SetStereoRecording(bool enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetStereoRecording(enable=%u)", enable);

    if (enable) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     " Stereo recording is not supported on this platform");
        return -1;
    }
    return 0;
}

int32_t AudioDeviceIPhone::StereoRecording(bool& enabled) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    enabled = false;
    return 0;
}

int32_t AudioDeviceIPhone::StereoPlayoutIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    available = false;  // Stereo playout not supported on iOS

    return 0;
}

int32_t AudioDeviceIPhone::SetStereoPlayout(bool enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetStereoPlayout(enable=%u)", enable);

    if (enable) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     " Stereo playout is not supported on this platform");
        return -1;
    }
    return 0;
}

int32_t AudioDeviceIPhone::StereoPlayout(bool& enabled) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    enabled = false;
    return 0;
}

int32_t AudioDeviceIPhone::SetAGC(bool enable) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetAGC(enable=%d)", enable);

    _AGC = enable;

    return 0;
}

bool AudioDeviceIPhone::AGC() const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    return _AGC;
}

int32_t AudioDeviceIPhone::MicrophoneVolumeIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    available = false;  // Mic volume not supported on IOS

    return 0;
}

int32_t AudioDeviceIPhone::SetMicrophoneVolume(uint32_t volume) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetMicrophoneVolume(volume=%u)", volume);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t
    AudioDeviceIPhone::MicrophoneVolume(uint32_t& volume) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t
    AudioDeviceIPhone::MaxMicrophoneVolume(uint32_t& maxVolume) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t
    AudioDeviceIPhone::MinMicrophoneVolume(uint32_t& minVolume) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t
    AudioDeviceIPhone::MicrophoneVolumeStepSize(
                                            uint16_t& stepSize) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int16_t AudioDeviceIPhone::PlayoutDevices() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);

    return (int16_t)1;
}

int32_t AudioDeviceIPhone::SetPlayoutDevice(uint16_t index) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetPlayoutDevice(index=%u)", index);

    if (_playIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout already initialized");
        return -1;
    }

    if (index !=0) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  SetPlayoutDevice invalid index");
        return -1;
    }
    _playoutDeviceIsSpecified = true;

    return 0;
}

int32_t
    AudioDeviceIPhone::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "WindowsDeviceType not supported");
    return -1;
}

int32_t
    AudioDeviceIPhone::PlayoutDeviceName(uint16_t index,
                                         char name[kAdmMaxDeviceNameSize],
                                         char guid[kAdmMaxGuidSize]) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::PlayoutDeviceName(index=%u)", index);

    if (index != 0) {
        return -1;
    }
    // return empty strings
    memset(name, 0, kAdmMaxDeviceNameSize);
    if (guid != NULL) {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return 0;
}

int32_t
    AudioDeviceIPhone::RecordingDeviceName(uint16_t index,
                                           char name[kAdmMaxDeviceNameSize],
                                           char guid[kAdmMaxGuidSize]) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::RecordingDeviceName(index=%u)", index);

    if (index != 0) {
        return -1;
    }
    // return empty strings
    memset(name, 0, kAdmMaxDeviceNameSize);
    if (guid != NULL) {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return 0;
}

int16_t AudioDeviceIPhone::RecordingDevices() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    return (int16_t)1;
}

int32_t AudioDeviceIPhone::SetRecordingDevice(uint16_t index) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetRecordingDevice(index=%u)", index);

    if (_recIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording already initialized");
        return -1;
    }

    if (index !=0) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  SetRecordingDevice invalid index");
        return -1;
    }

    _recordingDeviceIsSpecified = true;

    return 0;
}

int32_t
    AudioDeviceIPhone::SetRecordingDevice(
                                        AudioDeviceModule::WindowsDeviceType) {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "WindowsDeviceType not supported");
    return -1;
}

// ----------------------------------------------------------------------------
//  SetLoudspeakerStatus
//
//  Overrides the receiver playout route to speaker instead. See
//  kAudioSessionProperty_OverrideCategoryDefaultToSpeaker in CoreAudio
//  documentation.
// ----------------------------------------------------------------------------

int32_t AudioDeviceIPhone::SetLoudspeakerStatus(bool enable) {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetLoudspeakerStatus(enable=%d)", enable);

    UInt32 doChangeDefaultRoute = enable ? 1 : 0;
    OSStatus err = AudioSessionSetProperty(
        kAudioSessionProperty_OverrideCategoryDefaultToSpeaker,
        sizeof(doChangeDefaultRoute), &doChangeDefaultRoute);

    if (err != noErr) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "Error changing default output route " \
            "(only available on iOS 3.1 or later)");
        return -1;
    }

    return 0;
}

int32_t AudioDeviceIPhone::GetLoudspeakerStatus(bool &enabled) const {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetLoudspeakerStatus(enabled=?)");

    UInt32 route(0);
    UInt32 size = sizeof(route);
    OSStatus err = AudioSessionGetProperty(
        kAudioSessionProperty_OverrideCategoryDefaultToSpeaker,
        &size, &route);
    if (err != noErr) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "Error changing default output route " \
            "(only available on iOS 3.1 or later)");
        return -1;
    }

    enabled = route == 1 ? true: false;

    return 0;
}

int32_t AudioDeviceIPhone::PlayoutIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    available = false;

    // Try to initialize the playout side
    int32_t res = InitPlayout();

    // Cancel effect of initialization
    StopPlayout();

    if (res != -1) {
        available = true;
    }

    return 0;
}

int32_t AudioDeviceIPhone::RecordingIsAvailable(bool& available) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    available = false;

    // Try to initialize the recording side
    int32_t res = InitRecording();

    // Cancel effect of initialization
    StopRecording();

    if (res != -1) {
        available = true;
    }

    return 0;
}

int32_t AudioDeviceIPhone::InitPlayout() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "  Not initialized");
        return -1;
    }

    if (_playing) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Playout already started");
        return -1;
    }

    if (_playIsInitialized) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playout already initialized");
        return 0;
    }

    if (!_playoutDeviceIsSpecified) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout device is not specified");
        return -1;
    }

    // Initialize the speaker
    if (InitSpeaker() == -1) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  InitSpeaker() failed");
    }

    _playIsInitialized = true;

    if (!_recIsInitialized) {
        // Audio init
        if (InitPlayOrRecord() == -1) {
            // todo: Handle error
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  InitPlayOrRecord() failed");
        }
    } else {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
        "  Recording already initialized - InitPlayOrRecord() not called");
    }

    return 0;
}

bool AudioDeviceIPhone::PlayoutIsInitialized() const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    return (_playIsInitialized);
}

int32_t AudioDeviceIPhone::InitRecording() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Not initialized");
        return -1;
    }

    if (_recording) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Recording already started");
        return -1;
    }

    if (_recIsInitialized) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Recording already initialized");
        return 0;
    }

    if (!_recordingDeviceIsSpecified) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording device is not specified");
        return -1;
    }

    // Initialize the microphone
    if (InitMicrophone() == -1) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  InitMicrophone() failed");
    }

    _recIsInitialized = true;

    if (!_playIsInitialized) {
        // Audio init
        if (InitPlayOrRecord() == -1) {
            // todo: Handle error
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  InitPlayOrRecord() failed");
        }
    } else {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playout already initialized - InitPlayOrRecord() " \
                     "not called");
    }

    return 0;
}

bool AudioDeviceIPhone::RecordingIsInitialized() const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    return (_recIsInitialized);
}

int32_t AudioDeviceIPhone::StartRecording() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (!_recIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording not initialized");
        return -1;
    }

    if (_recording) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Recording already started");
        return 0;
    }

    // Reset recording buffer
    memset(_recordingBuffer, 0, sizeof(_recordingBuffer));
    memset(_recordingLength, 0, sizeof(_recordingLength));
    memset(_recordingSeqNumber, 0, sizeof(_recordingSeqNumber));
    _recordingCurrentSeq = 0;
    _recordingBufferTotalSize = 0;
    _recordingDelay = 0;
    _recordingDelayHWAndOS = 0;
    // Make sure first call to update delay function will update delay
    _recordingDelayMeasurementCounter = 9999;
    _recWarning = 0;
    _recError = 0;

    if (!_playing) {
        // Start Audio Unit
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                     "  Starting Audio Unit");
        OSStatus result = AudioOutputUnitStart(_auVoiceProcessing);
        if (0 != result) {
            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                         "  Error starting Audio Unit (result=%d)", result);
            return -1;
        }
    }

    _recording = true;

    return 0;
}

int32_t AudioDeviceIPhone::StopRecording() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (!_recIsInitialized) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Recording is not initialized");
        return 0;
    }

    _recording = false;

    if (!_playing) {
        // Both playout and recording has stopped, shutdown the device
        ShutdownPlayOrRecord();
    }

    _recIsInitialized = false;
    _micIsInitialized = false;

    return 0;
}

bool AudioDeviceIPhone::Recording() const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    return (_recording);
}

int32_t AudioDeviceIPhone::StartPlayout() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    // This lock is (among other things) needed to avoid concurrency issues
    // with capture thread
    // shutting down Audio Unit
    CriticalSectionScoped lock(&_critSect);

    if (!_playIsInitialized) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout not initialized");
        return -1;
    }

    if (_playing) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playing already started");
        return 0;
    }

    // Reset playout buffer
    memset(_playoutBuffer, 0, sizeof(_playoutBuffer));
    _playoutBufferUsed = 0;
    _playoutDelay = 0;
    // Make sure first call to update delay function will update delay
    _playoutDelayMeasurementCounter = 9999;
    _playWarning = 0;
    _playError = 0;

    if (!_recording) {
        // Start Audio Unit
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                     "  Starting Audio Unit");
        OSStatus result = AudioOutputUnitStart(_auVoiceProcessing);
        if (0 != result) {
            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                         "  Error starting Audio Unit (result=%d)", result);
            return -1;
        }
    }

    _playing = true;

    return 0;
}

int32_t AudioDeviceIPhone::StopPlayout() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (!_playIsInitialized) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playout is not initialized");
        return 0;
    }

    _playing = false;

    if (!_recording) {
        // Both playout and recording has stopped, signal shutdown the device
        ShutdownPlayOrRecord();
    }

    _playIsInitialized = false;
    _speakerIsInitialized = false;

    return 0;
}

bool AudioDeviceIPhone::Playing() const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "%s", __FUNCTION__);
    return (_playing);
}

// ----------------------------------------------------------------------------
//  ResetAudioDevice
//
//  Disable playout and recording, signal to capture thread to shutdown,
//  and set enable states after shutdown to same as current.
//  In capture thread audio device will be shutdown, then started again.
// ----------------------------------------------------------------------------
int32_t AudioDeviceIPhone::ResetAudioDevice() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (!_playIsInitialized && !_recIsInitialized) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playout or recording not initialized, doing nothing");
        return 0;  // Nothing to reset
    }

    // Store the states we have before stopping to restart below
    bool initPlay = _playIsInitialized;
    bool play = _playing;
    bool initRec = _recIsInitialized;
    bool rec = _recording;

    int res(0);

    // Stop playout and recording
    WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                 "  Stopping playout and recording");
    res += StopPlayout();
    res += StopRecording();

    // Restart
    WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                 "  Restarting playout and recording (%d, %d, %d, %d)",
                 initPlay, play, initRec, rec);
    if (initPlay) res += InitPlayout();
    if (initRec)  res += InitRecording();
    if (play)     res += StartPlayout();
    if (rec)      res += StartRecording();

    if (0 != res) {
        // Logging is done in init/start/stop calls above
        return -1;
    }

    return 0;
}

int32_t AudioDeviceIPhone::PlayoutDelay(uint16_t& delayMS) const {
    delayMS = _playoutDelay;
    return 0;
}

int32_t AudioDeviceIPhone::RecordingDelay(uint16_t& delayMS) const {
    delayMS = _recordingDelay;
    return 0;
}

int32_t
    AudioDeviceIPhone::SetPlayoutBuffer(
                                    const AudioDeviceModule::BufferType type,
                                    uint16_t sizeMS) {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id,
                 "AudioDeviceIPhone::SetPlayoutBuffer(type=%u, sizeMS=%u)",
                 type, sizeMS);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

int32_t
    AudioDeviceIPhone::PlayoutBuffer(AudioDeviceModule::BufferType& type,
                                     uint16_t& sizeMS) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    type = AudioDeviceModule::kAdaptiveBufferSize;

    sizeMS = _playoutDelay;

    return 0;
}

int32_t AudioDeviceIPhone::CPULoad(uint16_t& /*load*/) const {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

bool AudioDeviceIPhone::PlayoutWarning() const {
    return (_playWarning > 0);
}

bool AudioDeviceIPhone::PlayoutError() const {
    return (_playError > 0);
}

bool AudioDeviceIPhone::RecordingWarning() const {
    return (_recWarning > 0);
}

bool AudioDeviceIPhone::RecordingError() const {
    return (_recError > 0);
}

void AudioDeviceIPhone::ClearPlayoutWarning() {
    _playWarning = 0;
}

void AudioDeviceIPhone::ClearPlayoutError() {
    _playError = 0;
}

void AudioDeviceIPhone::ClearRecordingWarning() {
    _recWarning = 0;
}

void AudioDeviceIPhone::ClearRecordingError() {
    _recError = 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================

int32_t AudioDeviceIPhone::InitPlayOrRecord() {
    WEBRTC_TRACE(kTraceModuleCall, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    OSStatus result = -1;

    // Check if already initialized
    if (NULL != _auVoiceProcessing) {
        // We already have initialized before and created any of the audio unit,
        // check that all exist
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Already initialized");
        // todo: Call AudioUnitReset() here and empty all buffers?
        return 0;
    }

    // Create Voice Processing Audio Unit
    AudioComponentDescription desc;
    AudioComponent comp;

    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_VoiceProcessingIO;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    comp = AudioComponentFindNext(NULL, &desc);
    if (NULL == comp) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Could not find audio component for Audio Unit");
        return -1;
    }

    result = AudioComponentInstanceNew(comp, &_auVoiceProcessing);
    if (0 != result) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Could not create Audio Unit instance (result=%d)",
                     result);
        return -1;
    }

    // Set preferred hardware sample rate to 16 kHz
    Float64 sampleRate(16000.0);
    result = AudioSessionSetProperty(
                         kAudioSessionProperty_PreferredHardwareSampleRate,
                         sizeof(sampleRate), &sampleRate);
    if (0 != result) {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Could not set preferred sample rate (result=%d)", result);
    }

    uint32_t voiceChat = kAudioSessionMode_VoiceChat;
    AudioSessionSetProperty(kAudioSessionProperty_Mode,
                            sizeof(voiceChat), &voiceChat);

    //////////////////////
    // Setup Voice Processing Audio Unit

    // Note: For Signal Processing AU element 0 is output bus, element 1 is
    //       input bus for global scope element is irrelevant (always use
    //       element 0)

    // Enable IO on both elements

    // todo: Below we just log and continue upon error. We might want
    //       to close AU and return error for some cases.
    // todo: Log info about setup.

    UInt32 enableIO = 1;
    result = AudioUnitSetProperty(_auVoiceProcessing,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Input,
                                  1,  // input bus
                                  &enableIO,
                                  sizeof(enableIO));
    if (0 != result) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Could not enable IO on input (result=%d)", result);
    }

    result = AudioUnitSetProperty(_auVoiceProcessing,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output,
                                  0,   // output bus
                                  &enableIO,
                                  sizeof(enableIO));
    if (0 != result) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Could not enable IO on output (result=%d)", result);
    }

    // Disable AU buffer allocation for the recorder, we allocate our own
    UInt32 flag = 0;
    result = AudioUnitSetProperty(
        _auVoiceProcessing, kAudioUnitProperty_ShouldAllocateBuffer,
        kAudioUnitScope_Output,  1, &flag, sizeof(flag));
    if (0 != result) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Could not disable AU buffer allocation (result=%d)",
                     result);
        // Should work anyway
    }

    // Set recording callback
    AURenderCallbackStruct auCbS;
    memset(&auCbS, 0, sizeof(auCbS));
    auCbS.inputProc = RecordProcess;
    auCbS.inputProcRefCon = this;
    result = AudioUnitSetProperty(_auVoiceProcessing,
                                  kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global, 1,
                                  &auCbS, sizeof(auCbS));
    if (0 != result) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  Could not set record callback for Audio Unit (result=%d)",
            result);
    }

    // Set playout callback
    memset(&auCbS, 0, sizeof(auCbS));
    auCbS.inputProc = PlayoutProcess;
    auCbS.inputProcRefCon = this;
    result = AudioUnitSetProperty(_auVoiceProcessing,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global, 0,
                                  &auCbS, sizeof(auCbS));
    if (0 != result) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  Could not set play callback for Audio Unit (result=%d)",
            result);
    }

    // Get stream format for out/0
    AudioStreamBasicDescription playoutDesc;
    UInt32 size = sizeof(playoutDesc);
    result = AudioUnitGetProperty(_auVoiceProcessing,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output, 0, &playoutDesc,
                                  &size);
    if (0 != result) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  Could not get stream format Audio Unit out/0 (result=%d)",
            result);
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  Audio Unit playout opened in sampling rate %f",
                 playoutDesc.mSampleRate);

    playoutDesc.mSampleRate = sampleRate;

    // Store the sampling frequency to use towards the Audio Device Buffer
    // todo: Add 48 kHz (increase buffer sizes). Other fs?
    if ((playoutDesc.mSampleRate > 44090.0)
        && (playoutDesc.mSampleRate < 44110.0)) {
        _adbSampFreq = 44000;
    } else if ((playoutDesc.mSampleRate > 15990.0)
               && (playoutDesc.mSampleRate < 16010.0)) {
        _adbSampFreq = 16000;
    } else if ((playoutDesc.mSampleRate > 7990.0)
               && (playoutDesc.mSampleRate < 8010.0)) {
        _adbSampFreq = 8000;
    } else {
        _adbSampFreq = 0;
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  Audio Unit out/0 opened in unknown sampling rate (%f)",
            playoutDesc.mSampleRate);
        // todo: We should bail out here.
    }

    // Set the audio device buffer sampling rate,
    // we assume we get the same for play and record
    if (_ptrAudioBuffer->SetRecordingSampleRate(_adbSampFreq) < 0) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  Could not set audio device buffer recording sampling rate (%d)",
            _adbSampFreq);
    }

    if (_ptrAudioBuffer->SetPlayoutSampleRate(_adbSampFreq) < 0) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  Could not set audio device buffer playout sampling rate (%d)",
            _adbSampFreq);
    }

    // Set stream format for in/0  (use same sampling frequency as for out/0)
    playoutDesc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger
                               | kLinearPCMFormatFlagIsPacked
                               | kLinearPCMFormatFlagIsNonInterleaved;
    playoutDesc.mBytesPerPacket = 2;
    playoutDesc.mFramesPerPacket = 1;
    playoutDesc.mBytesPerFrame = 2;
    playoutDesc.mChannelsPerFrame = 1;
    playoutDesc.mBitsPerChannel = 16;
    result = AudioUnitSetProperty(_auVoiceProcessing,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input, 0, &playoutDesc, size);
    if (0 != result) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  Could not set stream format Audio Unit in/0 (result=%d)",
            result);
    }

    // Get stream format for in/1
    AudioStreamBasicDescription recordingDesc;
    size = sizeof(recordingDesc);
    result = AudioUnitGetProperty(_auVoiceProcessing,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input, 1, &recordingDesc,
                                  &size);
    if (0 != result) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  Could not get stream format Audio Unit in/1 (result=%d)",
            result);
    }
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "  Audio Unit recording opened in sampling rate %f",
                 recordingDesc.mSampleRate);

    recordingDesc.mSampleRate = sampleRate;

    // Set stream format for out/1 (use same sampling frequency as for in/1)
    recordingDesc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger
                                 | kLinearPCMFormatFlagIsPacked
                                 | kLinearPCMFormatFlagIsNonInterleaved;

    recordingDesc.mBytesPerPacket = 2;
    recordingDesc.mFramesPerPacket = 1;
    recordingDesc.mBytesPerFrame = 2;
    recordingDesc.mChannelsPerFrame = 1;
    recordingDesc.mBitsPerChannel = 16;
    result = AudioUnitSetProperty(_auVoiceProcessing,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output, 1, &recordingDesc,
                                  size);
    if (0 != result) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
            "  Could not set stream format Audio Unit out/1 (result=%d)",
            result);
    }

    // Initialize here already to be able to get/set stream properties.
    result = AudioUnitInitialize(_auVoiceProcessing);
    if (0 != result) {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Could not init Audio Unit (result=%d)", result);
    }

    // Get hardware sample rate for logging (see if we get what we asked for)
    Float64 hardwareSampleRate = 0.0;
    size = sizeof(hardwareSampleRate);
    result = AudioSessionGetProperty(
        kAudioSessionProperty_CurrentHardwareSampleRate, &size,
        &hardwareSampleRate);
    if (0 != result) {
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
            "  Could not get current HW sample rate (result=%d)", result);
    }
    WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                 "  Current HW sample rate is %f, ADB sample rate is %d",
             hardwareSampleRate, _adbSampFreq);

    return 0;
}

int32_t AudioDeviceIPhone::ShutdownPlayOrRecord() {
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    // Close and delete AU
    OSStatus result = -1;
    if (NULL != _auVoiceProcessing) {
        result = AudioOutputUnitStop(_auVoiceProcessing);
        if (0 != result) {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "  Error stopping Audio Unit (result=%d)", result);
        }
        result = AudioComponentInstanceDispose(_auVoiceProcessing);
        if (0 != result) {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                "  Error disposing Audio Unit (result=%d)", result);
        }
        _auVoiceProcessing = NULL;
    }

    return 0;
}

// ============================================================================
//                                  Thread Methods
// ============================================================================

OSStatus
    AudioDeviceIPhone::RecordProcess(void *inRefCon,
                                     AudioUnitRenderActionFlags *ioActionFlags,
                                     const AudioTimeStamp *inTimeStamp,
                                     UInt32 inBusNumber,
                                     UInt32 inNumberFrames,
                                     AudioBufferList *ioData) {
    AudioDeviceIPhone* ptrThis = static_cast<AudioDeviceIPhone*>(inRefCon);

    return ptrThis->RecordProcessImpl(ioActionFlags,
                                      inTimeStamp,
                                      inBusNumber,
                                      inNumberFrames);
}


OSStatus
    AudioDeviceIPhone::RecordProcessImpl(
                                    AudioUnitRenderActionFlags *ioActionFlags,
                                    const AudioTimeStamp *inTimeStamp,
                                    uint32_t inBusNumber,
                                    uint32_t inNumberFrames) {
    // Setup some basic stuff
    // Use temp buffer not to lock up recording buffer more than necessary
    // todo: Make dataTmp a member variable with static size that holds
    //       max possible frames?
    int16_t* dataTmp = new int16_t[inNumberFrames];
    memset(dataTmp, 0, 2*inNumberFrames);

    AudioBufferList abList;
    abList.mNumberBuffers = 1;
    abList.mBuffers[0].mData = dataTmp;
    abList.mBuffers[0].mDataByteSize = 2*inNumberFrames;  // 2 bytes/sample
    abList.mBuffers[0].mNumberChannels = 1;

    // Get data from mic
    OSStatus res = AudioUnitRender(_auVoiceProcessing,
                                   ioActionFlags, inTimeStamp,
                                   inBusNumber, inNumberFrames, &abList);
    if (res != 0) {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Error getting rec data, error = %d", res);

        if (_recWarning > 0) {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  Pending rec warning exists");
        }
        _recWarning = 1;

        delete [] dataTmp;
        return 0;
    }

    if (_recording) {
        // Insert all data in temp buffer into recording buffers
        // There is zero or one buffer partially full at any given time,
        // all others are full or empty
        // Full means filled with noSamp10ms samples.

        const unsigned int noSamp10ms = _adbSampFreq / 100;
        unsigned int dataPos = 0;
        uint16_t bufPos = 0;
        int16_t insertPos = -1;
        unsigned int nCopy = 0;  // Number of samples to copy

        while (dataPos < inNumberFrames) {
            // Loop over all recording buffers or
            // until we find the partially full buffer
            // First choice is to insert into partially full buffer,
            // second choice is to insert into empty buffer
            bufPos = 0;
            insertPos = -1;
            nCopy = 0;
            while (bufPos < N_REC_BUFFERS) {
                if ((_recordingLength[bufPos] > 0)
                    && (_recordingLength[bufPos] < noSamp10ms)) {
                    // Found the partially full buffer
                    insertPos = static_cast<int16_t>(bufPos);
                    // Don't need to search more, quit loop
                    bufPos = N_REC_BUFFERS;
                } else if ((-1 == insertPos)
                           && (0 == _recordingLength[bufPos])) {
                    // Found an empty buffer
                    insertPos = static_cast<int16_t>(bufPos);
                }
                ++bufPos;
            }

            // Insert data into buffer
            if (insertPos > -1) {
                // We found a non-full buffer, copy data to it
                unsigned int dataToCopy = inNumberFrames - dataPos;
                unsigned int currentRecLen = _recordingLength[insertPos];
                unsigned int roomInBuffer = noSamp10ms - currentRecLen;
                nCopy = (dataToCopy < roomInBuffer ? dataToCopy : roomInBuffer);

                memcpy(&_recordingBuffer[insertPos][currentRecLen],
                       &dataTmp[dataPos], nCopy*sizeof(int16_t));
                if (0 == currentRecLen) {
                    _recordingSeqNumber[insertPos] = _recordingCurrentSeq;
                    ++_recordingCurrentSeq;
                }
                _recordingBufferTotalSize += nCopy;
                // Has to be done last to avoid interrupt problems
                // between threads
                _recordingLength[insertPos] += nCopy;
                dataPos += nCopy;
            } else {
                // Didn't find a non-full buffer
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                             "  Could not insert into recording buffer");
                if (_recWarning > 0) {
                    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                                 "  Pending rec warning exists");
                }
                _recWarning = 1;
                dataPos = inNumberFrames;  // Don't try to insert more
            }
        }
    }

    delete [] dataTmp;

    return 0;
}

OSStatus
    AudioDeviceIPhone::PlayoutProcess(void *inRefCon,
                                      AudioUnitRenderActionFlags *ioActionFlags,
                                      const AudioTimeStamp *inTimeStamp,
                                      UInt32 inBusNumber,
                                      UInt32 inNumberFrames,
                                      AudioBufferList *ioData) {
    AudioDeviceIPhone* ptrThis = static_cast<AudioDeviceIPhone*>(inRefCon);

    return ptrThis->PlayoutProcessImpl(inNumberFrames, ioData);
}

OSStatus
    AudioDeviceIPhone::PlayoutProcessImpl(uint32_t inNumberFrames,
                                          AudioBufferList *ioData) {
    // Setup some basic stuff
//    assert(sizeof(short) == 2); // Assumption for implementation

    int16_t* data =
        static_cast<int16_t*>(ioData->mBuffers[0].mData);
    unsigned int dataSizeBytes = ioData->mBuffers[0].mDataByteSize;
    unsigned int dataSize = dataSizeBytes/2;  // Number of samples
        if (dataSize != inNumberFrames) {  // Should always be the same
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "dataSize (%u) != inNumberFrames (%u)",
                     dataSize, (unsigned int)inNumberFrames);
        if (_playWarning > 0) {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  Pending play warning exists");
        }
        _playWarning = 1;
    }
    memset(data, 0, dataSizeBytes);  // Start with empty buffer


    // Get playout data from Audio Device Buffer

    if (_playing) {
        unsigned int noSamp10ms = _adbSampFreq / 100;
        // todo: Member variable and allocate when samp freq is determined
        int16_t* dataTmp = new int16_t[noSamp10ms];
        memset(dataTmp, 0, 2*noSamp10ms);
        unsigned int dataPos = 0;
        int noSamplesOut = 0;
        unsigned int nCopy = 0;

        // First insert data from playout buffer if any
        if (_playoutBufferUsed > 0) {
            nCopy = (dataSize < _playoutBufferUsed) ?
                    dataSize : _playoutBufferUsed;
            if (nCopy != _playoutBufferUsed) {
                // todo: If dataSize < _playoutBufferUsed
                //       (should normally never be)
                //       we must move the remaining data
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                             "nCopy (%u) != _playoutBufferUsed (%u)",
                             nCopy, _playoutBufferUsed);
                if (_playWarning > 0) {
                    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                                 "  Pending play warning exists");
                }
                _playWarning = 1;
            }
            memcpy(data, _playoutBuffer, 2*nCopy);
            dataPos = nCopy;
            memset(_playoutBuffer, 0, sizeof(_playoutBuffer));
            _playoutBufferUsed = 0;
        }

        // Now get the rest from Audio Device Buffer
        while (dataPos < dataSize) {
            // Update playout delay
            UpdatePlayoutDelay();

            // Ask for new PCM data to be played out using the AudioDeviceBuffer
            noSamplesOut = _ptrAudioBuffer->RequestPlayoutData(noSamp10ms);

            // Get data from Audio Device Buffer
            noSamplesOut =
                _ptrAudioBuffer->GetPlayoutData(
                    reinterpret_cast<int8_t*>(dataTmp));
            // Cast OK since only equality comparison
            if (noSamp10ms != (unsigned int)noSamplesOut) {
                // Should never happen
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                             "noSamp10ms (%u) != noSamplesOut (%d)",
                             noSamp10ms, noSamplesOut);

                if (_playWarning > 0) {
                    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                                 "  Pending play warning exists");
                }
                _playWarning = 1;
            }

            // Insert as much as fits in data buffer
            nCopy = (dataSize-dataPos) > noSamp10ms ?
                    noSamp10ms : (dataSize-dataPos);
            memcpy(&data[dataPos], dataTmp, 2*nCopy);

            // Save rest in playout buffer if any
            if (nCopy < noSamp10ms) {
                memcpy(_playoutBuffer, &dataTmp[nCopy], 2*(noSamp10ms-nCopy));
                _playoutBufferUsed = noSamp10ms - nCopy;
            }

            // Update loop/index counter, if we copied less than noSamp10ms
            // samples we shall quit loop anyway
            dataPos += noSamp10ms;
        }

        delete [] dataTmp;
    }

    return 0;
}

void AudioDeviceIPhone::UpdatePlayoutDelay() {
    ++_playoutDelayMeasurementCounter;

    if (_playoutDelayMeasurementCounter >= 100) {
        // Update HW and OS delay every second, unlikely to change

        _playoutDelay = 0;

        // HW output latency
        Float32 f32(0);
        UInt32 size = sizeof(f32);
        OSStatus result = AudioSessionGetProperty(
            kAudioSessionProperty_CurrentHardwareOutputLatency, &size, &f32);
        if (0 != result) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "error HW latency (result=%d)", result);
        }
        _playoutDelay += static_cast<int>(f32 * 1000000);

        // HW buffer duration
        f32 = 0;
        result = AudioSessionGetProperty(
            kAudioSessionProperty_CurrentHardwareIOBufferDuration, &size, &f32);
        if (0 != result) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "error HW buffer duration (result=%d)", result);
        }
        _playoutDelay += static_cast<int>(f32 * 1000000);

        // AU latency
        Float64 f64(0);
        size = sizeof(f64);
        result = AudioUnitGetProperty(_auVoiceProcessing,
            kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &f64, &size);
        if (0 != result) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "error AU latency (result=%d)", result);
        }
        _playoutDelay += static_cast<int>(f64 * 1000000);

        // To ms
        _playoutDelay = (_playoutDelay - 500) / 1000;

        // Reset counter
        _playoutDelayMeasurementCounter = 0;
    }

    // todo: Add playout buffer? (Only used for 44.1 kHz)
}

void AudioDeviceIPhone::UpdateRecordingDelay() {
    ++_recordingDelayMeasurementCounter;

    if (_recordingDelayMeasurementCounter >= 100) {
        // Update HW and OS delay every second, unlikely to change

        _recordingDelayHWAndOS = 0;

        // HW input latency
        Float32 f32(0);
        UInt32 size = sizeof(f32);
        OSStatus result = AudioSessionGetProperty(
            kAudioSessionProperty_CurrentHardwareInputLatency, &size, &f32);
        if (0 != result) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "error HW latency (result=%d)", result);
        }
        _recordingDelayHWAndOS += static_cast<int>(f32 * 1000000);

        // HW buffer duration
        f32 = 0;
        result = AudioSessionGetProperty(
            kAudioSessionProperty_CurrentHardwareIOBufferDuration, &size, &f32);
        if (0 != result) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "error HW buffer duration (result=%d)", result);
        }
        _recordingDelayHWAndOS += static_cast<int>(f32 * 1000000);

        // AU latency
        Float64 f64(0);
        size = sizeof(f64);
        result = AudioUnitGetProperty(_auVoiceProcessing,
                                      kAudioUnitProperty_Latency,
                                      kAudioUnitScope_Global, 0, &f64, &size);
        if (0 != result) {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "error AU latency (result=%d)", result);
        }
        _recordingDelayHWAndOS += static_cast<int>(f64 * 1000000);

        // To ms
        _recordingDelayHWAndOS = (_recordingDelayHWAndOS - 500) / 1000;

        // Reset counter
        _recordingDelayMeasurementCounter = 0;
    }

    _recordingDelay = _recordingDelayHWAndOS;

    // ADB recording buffer size, update every time
    // Don't count the one next 10 ms to be sent, then convert samples => ms
    const uint32_t noSamp10ms = _adbSampFreq / 100;
    if (_recordingBufferTotalSize > noSamp10ms) {
        _recordingDelay +=
            (_recordingBufferTotalSize - noSamp10ms) / (_adbSampFreq / 1000);
    }
}

bool AudioDeviceIPhone::RunCapture(void* ptrThis) {
    return static_cast<AudioDeviceIPhone*>(ptrThis)->CaptureWorkerThread();
}

bool AudioDeviceIPhone::CaptureWorkerThread() {
    if (_recording) {
        int bufPos = 0;
        unsigned int lowestSeq = 0;
        int lowestSeqBufPos = 0;
        bool foundBuf = true;
        const unsigned int noSamp10ms = _adbSampFreq / 100;

        while (foundBuf) {
            // Check if we have any buffer with data to insert
            // into the Audio Device Buffer,
            // and find the one with the lowest seq number
            foundBuf = false;
            for (bufPos = 0; bufPos < N_REC_BUFFERS; ++bufPos) {
                if (noSamp10ms == _recordingLength[bufPos]) {
                    if (!foundBuf) {
                        lowestSeq = _recordingSeqNumber[bufPos];
                        lowestSeqBufPos = bufPos;
                        foundBuf = true;
                    } else if (_recordingSeqNumber[bufPos] < lowestSeq) {
                        lowestSeq = _recordingSeqNumber[bufPos];
                        lowestSeqBufPos = bufPos;
                    }
                }
            }  // for

            // Insert data into the Audio Device Buffer if found any
            if (foundBuf) {
                // Update recording delay
                UpdateRecordingDelay();

                // Set the recorded buffer
                _ptrAudioBuffer->SetRecordedBuffer(
                    reinterpret_cast<int8_t*>(
                        _recordingBuffer[lowestSeqBufPos]),
                        _recordingLength[lowestSeqBufPos]);

                // Don't need to set the current mic level in ADB since we only
                // support digital AGC,
                // and besides we cannot get or set the IOS mic level anyway.

                // Set VQE info, use clockdrift == 0
                _ptrAudioBuffer->SetVQEData(_playoutDelay, _recordingDelay, 0);

                // Deliver recorded samples at specified sample rate, mic level
                // etc. to the observer using callback
                _ptrAudioBuffer->DeliverRecordedData();

                // Make buffer available
                _recordingSeqNumber[lowestSeqBufPos] = 0;
                _recordingBufferTotalSize -= _recordingLength[lowestSeqBufPos];
                // Must be done last to avoid interrupt problems between threads
                _recordingLength[lowestSeqBufPos] = 0;
            }
        }  // while (foundBuf)
    }  // if (_recording)

    {
        // Normal case
        // Sleep thread (5ms) to let other threads get to work
        // todo: Is 5 ms optimal? Sleep shorter if inserted into the Audio
        //       Device Buffer?
        timespec t;
        t.tv_sec = 0;
        t.tv_nsec = 5*1000*1000;
        nanosleep(&t, NULL);
    }

    return true;
}

}  // namespace webrtc

