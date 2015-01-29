/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_IMPL_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_IMPL_H

#include "webrtc/modules/audio_device/audio_device_buffer.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

namespace webrtc
{

class AudioDeviceGeneric;
class AudioDeviceUtility;
class CriticalSectionWrapper;

class AudioDeviceModuleImpl : public AudioDeviceModule
{
public:
    enum PlatformType
    {
        kPlatformNotSupported = 0,
        kPlatformWin32 = 1,
        kPlatformWinCe = 2,
        kPlatformLinux = 3,
        kPlatformMac = 4,
        kPlatformAndroid = 5,
        kPlatformIOS = 6
    };

    int32_t CheckPlatform();
    int32_t CreatePlatformSpecificObjects();
    int32_t AttachAudioBuffer();

    AudioDeviceModuleImpl(const int32_t id, const AudioLayer audioLayer);
    virtual ~AudioDeviceModuleImpl();

public: // RefCountedModule
    virtual int32_t ChangeUniqueId(const int32_t id) OVERRIDE;
    virtual int32_t TimeUntilNextProcess() OVERRIDE;
    virtual int32_t Process() OVERRIDE;

public:
    // Factory methods (resource allocation/deallocation)
    static AudioDeviceModule* Create(
        const int32_t id,
        const AudioLayer audioLayer = kPlatformDefaultAudio);

    // Retrieve the currently utilized audio layer
    virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const OVERRIDE;

    // Error handling
    virtual ErrorCode LastError() const OVERRIDE;
    virtual int32_t RegisterEventObserver(
        AudioDeviceObserver* eventCallback) OVERRIDE;

    // Full-duplex transportation of PCM audio
    virtual int32_t RegisterAudioCallback(
        AudioTransport* audioCallback) OVERRIDE;

    // Main initializaton and termination
    virtual int32_t Init() OVERRIDE;
    virtual int32_t Terminate() OVERRIDE;
    virtual bool Initialized() const OVERRIDE;

    // Device enumeration
    virtual int16_t PlayoutDevices() OVERRIDE;
    virtual int16_t RecordingDevices() OVERRIDE;
    virtual int32_t PlayoutDeviceName(
        uint16_t index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]) OVERRIDE;
    virtual int32_t RecordingDeviceName(
        uint16_t index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]) OVERRIDE;

    // Device selection
    virtual int32_t SetPlayoutDevice(uint16_t index) OVERRIDE;
    virtual int32_t SetPlayoutDevice(WindowsDeviceType device) OVERRIDE;
    virtual int32_t SetRecordingDevice(uint16_t index) OVERRIDE;
    virtual int32_t SetRecordingDevice(WindowsDeviceType device) OVERRIDE;

    // Audio transport initialization
    virtual int32_t PlayoutIsAvailable(bool* available) OVERRIDE;
    virtual int32_t InitPlayout() OVERRIDE;
    virtual bool PlayoutIsInitialized() const OVERRIDE;
    virtual int32_t RecordingIsAvailable(bool* available) OVERRIDE;
    virtual int32_t InitRecording() OVERRIDE;
    virtual bool RecordingIsInitialized() const OVERRIDE;

    // Audio transport control
    virtual int32_t StartPlayout() OVERRIDE;
    virtual int32_t StopPlayout() OVERRIDE;
    virtual bool Playing() const OVERRIDE;
    virtual int32_t StartRecording() OVERRIDE;
    virtual int32_t StopRecording() OVERRIDE;
    virtual bool Recording() const OVERRIDE;

    // Microphone Automatic Gain Control (AGC)
    virtual int32_t SetAGC(bool enable) OVERRIDE;
    virtual bool AGC() const OVERRIDE;

    // Volume control based on the Windows Wave API (Windows only)
    virtual int32_t SetWaveOutVolume(uint16_t volumeLeft,
                                     uint16_t volumeRight) OVERRIDE;
    virtual int32_t WaveOutVolume(uint16_t* volumeLeft,
                                  uint16_t* volumeRight) const OVERRIDE;

    // Audio mixer initialization
    virtual int32_t InitSpeaker() OVERRIDE;
    virtual bool SpeakerIsInitialized() const OVERRIDE;
    virtual int32_t InitMicrophone() OVERRIDE;
    virtual bool MicrophoneIsInitialized() const OVERRIDE;

    // Speaker volume controls
    virtual int32_t SpeakerVolumeIsAvailable(bool* available) OVERRIDE;
    virtual int32_t SetSpeakerVolume(uint32_t volume) OVERRIDE;
    virtual int32_t SpeakerVolume(uint32_t* volume) const OVERRIDE;
    virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const OVERRIDE;
    virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const OVERRIDE;
    virtual int32_t SpeakerVolumeStepSize(
        uint16_t* stepSize) const OVERRIDE;

    // Microphone volume controls
    virtual int32_t MicrophoneVolumeIsAvailable(bool* available) OVERRIDE;
    virtual int32_t SetMicrophoneVolume(uint32_t volume) OVERRIDE;
    virtual int32_t MicrophoneVolume(uint32_t* volume) const OVERRIDE;
    virtual int32_t MaxMicrophoneVolume(
        uint32_t* maxVolume) const OVERRIDE;
    virtual int32_t MinMicrophoneVolume(
        uint32_t* minVolume) const OVERRIDE;
    virtual int32_t MicrophoneVolumeStepSize(
        uint16_t* stepSize) const OVERRIDE;

    // Speaker mute control
    virtual int32_t SpeakerMuteIsAvailable(bool* available) OVERRIDE;
    virtual int32_t SetSpeakerMute(bool enable) OVERRIDE;
    virtual int32_t SpeakerMute(bool* enabled) const OVERRIDE;

    // Microphone mute control
    virtual int32_t MicrophoneMuteIsAvailable(bool* available) OVERRIDE;
    virtual int32_t SetMicrophoneMute(bool enable) OVERRIDE;
    virtual int32_t MicrophoneMute(bool* enabled) const OVERRIDE;

    // Microphone boost control
    virtual int32_t MicrophoneBoostIsAvailable(bool* available) OVERRIDE;
    virtual int32_t SetMicrophoneBoost(bool enable) OVERRIDE;
    virtual int32_t MicrophoneBoost(bool* enabled) const OVERRIDE;

    // Stereo support
    virtual int32_t StereoPlayoutIsAvailable(bool* available) const OVERRIDE;
    virtual int32_t SetStereoPlayout(bool enable) OVERRIDE;
    virtual int32_t StereoPlayout(bool* enabled) const OVERRIDE;
    virtual int32_t StereoRecordingIsAvailable(bool* available) const OVERRIDE;
    virtual int32_t SetStereoRecording(bool enable) OVERRIDE;
    virtual int32_t StereoRecording(bool* enabled) const OVERRIDE;
    virtual int32_t SetRecordingChannel(const ChannelType channel) OVERRIDE;
    virtual int32_t RecordingChannel(ChannelType* channel) const OVERRIDE;

    // Delay information and control
    virtual int32_t SetPlayoutBuffer(const BufferType type,
                                           uint16_t sizeMS = 0) OVERRIDE;
    virtual int32_t PlayoutBuffer(BufferType* type,
                                        uint16_t* sizeMS) const OVERRIDE;
    virtual int32_t PlayoutDelay(uint16_t* delayMS) const OVERRIDE;
    virtual int32_t RecordingDelay(uint16_t* delayMS) const OVERRIDE;

    // CPU load
    virtual int32_t CPULoad(uint16_t* load) const OVERRIDE;

    // Recording of raw PCM data
    virtual int32_t StartRawOutputFileRecording(
        const char pcmFileNameUTF8[kAdmMaxFileNameSize]) OVERRIDE;
    virtual int32_t StopRawOutputFileRecording() OVERRIDE;
    virtual int32_t StartRawInputFileRecording(
        const char pcmFileNameUTF8[kAdmMaxFileNameSize]) OVERRIDE;
    virtual int32_t StopRawInputFileRecording() OVERRIDE;

    // Native sample rate controls (samples/sec)
    virtual int32_t SetRecordingSampleRate(
        const uint32_t samplesPerSec) OVERRIDE;
    virtual int32_t RecordingSampleRate(
        uint32_t* samplesPerSec) const OVERRIDE;
    virtual int32_t SetPlayoutSampleRate(
        const uint32_t samplesPerSec) OVERRIDE;
    virtual int32_t PlayoutSampleRate(
        uint32_t* samplesPerSec) const OVERRIDE;

    // Mobile device specific functions
    virtual int32_t ResetAudioDevice() OVERRIDE;
    virtual int32_t SetLoudspeakerStatus(bool enable) OVERRIDE;
    virtual int32_t GetLoudspeakerStatus(bool* enabled) const OVERRIDE;

    virtual int32_t EnableBuiltInAEC(bool enable) OVERRIDE;
    virtual bool BuiltInAECIsEnabled() const OVERRIDE;

public:
    int32_t Id() {return _id;}

private:
    PlatformType Platform() const;
    AudioLayer PlatformAudioLayer() const;

private:
    CriticalSectionWrapper&     _critSect;
    CriticalSectionWrapper&     _critSectEventCb;
    CriticalSectionWrapper&     _critSectAudioCb;

    AudioDeviceObserver*        _ptrCbAudioDeviceObserver;

    AudioDeviceUtility*         _ptrAudioDeviceUtility;
    AudioDeviceGeneric*         _ptrAudioDevice;

    AudioDeviceBuffer           _audioDeviceBuffer;

    int32_t               _id;
    AudioLayer                  _platformAudioLayer;
    uint32_t              _lastProcessTime;
    PlatformType                _platformType;
    bool                        _initialized;
    mutable ErrorCode           _lastError;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_INTERFACE_AUDIO_DEVICE_IMPL_H_
