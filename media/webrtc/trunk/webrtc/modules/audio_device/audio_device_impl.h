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

#include "audio_device.h"
#include "audio_device_buffer.h"

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
    virtual int32_t ChangeUniqueId(const int32_t id);
    virtual int32_t TimeUntilNextProcess();
    virtual int32_t Process();

public:
    // Factory methods (resource allocation/deallocation)
    static AudioDeviceModule* Create(
        const int32_t id,
        const AudioLayer audioLayer = kPlatformDefaultAudio);

    // Retrieve the currently utilized audio layer
    virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const;

    // Error handling
    virtual ErrorCode LastError() const;
    virtual int32_t RegisterEventObserver(
        AudioDeviceObserver* eventCallback);

    // Full-duplex transportation of PCM audio
    virtual int32_t RegisterAudioCallback(
        AudioTransport* audioCallback);

    // Main initializaton and termination
    virtual int32_t Init();
    virtual int32_t Terminate();
    virtual bool Initialized() const;

    // Device enumeration
    virtual int16_t PlayoutDevices();
    virtual int16_t RecordingDevices();
    virtual int32_t PlayoutDeviceName(
        uint16_t index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]);
    virtual int32_t RecordingDeviceName(
        uint16_t index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]);

    // Device selection
    virtual int32_t SetPlayoutDevice(uint16_t index);
    virtual int32_t SetPlayoutDevice(WindowsDeviceType device);
    virtual int32_t SetRecordingDevice(uint16_t index);
    virtual int32_t SetRecordingDevice(WindowsDeviceType device);

    // Audio transport initialization
    virtual int32_t PlayoutIsAvailable(bool* available);
    virtual int32_t InitPlayout();
    virtual bool PlayoutIsInitialized() const;
    virtual int32_t RecordingIsAvailable(bool* available);
    virtual int32_t InitRecording();
    virtual bool RecordingIsInitialized() const;

    // Audio transport control
    virtual int32_t StartPlayout();
    virtual int32_t StopPlayout();
    virtual bool Playing() const;
    virtual int32_t StartRecording();
    virtual int32_t StopRecording();
    virtual bool Recording() const;

    // Microphone Automatic Gain Control (AGC)
    virtual int32_t SetAGC(bool enable);
    virtual bool AGC() const;

    // Volume control based on the Windows Wave API (Windows only)
    virtual int32_t SetWaveOutVolume(uint16_t volumeLeft, uint16_t volumeRight);
    virtual int32_t WaveOutVolume(uint16_t* volumeLeft,
                                  uint16_t* volumeRight) const;

    // Audio mixer initialization
    virtual int32_t SpeakerIsAvailable(bool* available);
    virtual int32_t InitSpeaker();
    virtual bool SpeakerIsInitialized() const;
    virtual int32_t MicrophoneIsAvailable(bool* available);
    virtual int32_t InitMicrophone();
    virtual bool MicrophoneIsInitialized() const;

    // Speaker volume controls
    virtual int32_t SpeakerVolumeIsAvailable(bool* available);
    virtual int32_t SetSpeakerVolume(uint32_t volume);
    virtual int32_t SpeakerVolume(uint32_t* volume) const;
    virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const;
    virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const;
    virtual int32_t SpeakerVolumeStepSize(
        uint16_t* stepSize) const;

    // Microphone volume controls
    virtual int32_t MicrophoneVolumeIsAvailable(bool* available);
    virtual int32_t SetMicrophoneVolume(uint32_t volume);
    virtual int32_t MicrophoneVolume(uint32_t* volume) const;
    virtual int32_t MaxMicrophoneVolume(
        uint32_t* maxVolume) const;
    virtual int32_t MinMicrophoneVolume(
        uint32_t* minVolume) const;
    virtual int32_t MicrophoneVolumeStepSize(
        uint16_t* stepSize) const;

    // Speaker mute control
    virtual int32_t SpeakerMuteIsAvailable(bool* available);
    virtual int32_t SetSpeakerMute(bool enable);
    virtual int32_t SpeakerMute(bool* enabled) const;

    // Microphone mute control
    virtual int32_t MicrophoneMuteIsAvailable(bool* available);
    virtual int32_t SetMicrophoneMute(bool enable);
    virtual int32_t MicrophoneMute(bool* enabled) const;

    // Microphone boost control
    virtual int32_t MicrophoneBoostIsAvailable(bool* available);
    virtual int32_t SetMicrophoneBoost(bool enable);
    virtual int32_t MicrophoneBoost(bool* enabled) const;

    // Stereo support
    virtual int32_t StereoPlayoutIsAvailable(bool* available) const;
    virtual int32_t SetStereoPlayout(bool enable);
    virtual int32_t StereoPlayout(bool* enabled) const;
    virtual int32_t StereoRecordingIsAvailable(bool* available) const;
    virtual int32_t SetStereoRecording(bool enable);
    virtual int32_t StereoRecording(bool* enabled) const;
    virtual int32_t SetRecordingChannel(const ChannelType channel);
    virtual int32_t RecordingChannel(ChannelType* channel) const;

    // Delay information and control
    virtual int32_t SetPlayoutBuffer(const BufferType type,
                                           uint16_t sizeMS = 0);
    virtual int32_t PlayoutBuffer(BufferType* type,
                                        uint16_t* sizeMS) const;
    virtual int32_t PlayoutDelay(uint16_t* delayMS) const;
    virtual int32_t RecordingDelay(uint16_t* delayMS) const;

    // CPU load
    virtual int32_t CPULoad(uint16_t* load) const;

    // Recording of raw PCM data
    virtual int32_t StartRawOutputFileRecording(
        const char pcmFileNameUTF8[kAdmMaxFileNameSize]);
    virtual int32_t StopRawOutputFileRecording();
    virtual int32_t StartRawInputFileRecording(
        const char pcmFileNameUTF8[kAdmMaxFileNameSize]);
    virtual int32_t StopRawInputFileRecording();

    // Native sample rate controls (samples/sec)
    virtual int32_t SetRecordingSampleRate(
        const uint32_t samplesPerSec);
    virtual int32_t RecordingSampleRate(
        uint32_t* samplesPerSec) const;
    virtual int32_t SetPlayoutSampleRate(
        const uint32_t samplesPerSec);
    virtual int32_t PlayoutSampleRate(
        uint32_t* samplesPerSec) const;

    // Mobile device specific functions
    virtual int32_t ResetAudioDevice();
    virtual int32_t SetLoudspeakerStatus(bool enable);
    virtual int32_t GetLoudspeakerStatus(bool* enabled) const;

    virtual int32_t EnableBuiltInAEC(bool enable);
    virtual bool BuiltInAECIsEnabled() const;

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
