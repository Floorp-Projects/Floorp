/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_IPHONE_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_IPHONE_H

#include <AudioUnit/AudioUnit.h>

#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {
class ThreadWrapper;

const uint32_t N_REC_SAMPLES_PER_SEC = 44100;
const uint32_t N_PLAY_SAMPLES_PER_SEC = 44100;

const uint32_t N_REC_CHANNELS = 1;  // default is mono recording
const uint32_t N_PLAY_CHANNELS = 1;  // default is mono playout
const uint32_t N_DEVICE_CHANNELS = 8;

const uint32_t ENGINE_REC_BUF_SIZE_IN_SAMPLES = (N_REC_SAMPLES_PER_SEC / 100);
const uint32_t ENGINE_PLAY_BUF_SIZE_IN_SAMPLES = (N_PLAY_SAMPLES_PER_SEC / 100);

// Number of 10 ms recording blocks in recording buffer
const uint16_t N_REC_BUFFERS = 20;

class AudioDeviceIPhone : public AudioDeviceGeneric {
public:
    AudioDeviceIPhone(const int32_t id);
    ~AudioDeviceIPhone();

    // Retrieve the currently utilized audio layer
    virtual int32_t
        ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const;

    // Main initializaton and termination
    virtual int32_t Init();
    virtual int32_t Terminate();
    virtual bool Initialized() const;

    // Device enumeration
    virtual int16_t PlayoutDevices();
    virtual int16_t RecordingDevices();
    virtual int32_t PlayoutDeviceName(uint16_t index,
                                      char name[kAdmMaxDeviceNameSize],
                                      char guid[kAdmMaxGuidSize]);
    virtual int32_t RecordingDeviceName(uint16_t index,
                                        char name[kAdmMaxDeviceNameSize],
                                        char guid[kAdmMaxGuidSize]);

    // Device selection
    virtual int32_t SetPlayoutDevice(uint16_t index);
    virtual int32_t
        SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device);
    virtual int32_t SetRecordingDevice(uint16_t index);
    virtual int32_t SetRecordingDevice(
        AudioDeviceModule::WindowsDeviceType device);

    // Audio transport initialization
    virtual int32_t PlayoutIsAvailable(bool& available);
    virtual int32_t InitPlayout();
    virtual bool PlayoutIsInitialized() const;
    virtual int32_t RecordingIsAvailable(bool& available);
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
    virtual int32_t WaveOutVolume(uint16_t& volumeLeft,
                                  uint16_t& volumeRight) const;

    // Audio mixer initialization
    virtual int32_t SpeakerIsAvailable(bool& available);
    virtual int32_t InitSpeaker();
    virtual bool SpeakerIsInitialized() const;
    virtual int32_t MicrophoneIsAvailable(bool& available);
    virtual int32_t InitMicrophone();
    virtual bool MicrophoneIsInitialized() const;

    // Speaker volume controls
    virtual int32_t SpeakerVolumeIsAvailable(bool& available);
    virtual int32_t SetSpeakerVolume(uint32_t volume);
    virtual int32_t SpeakerVolume(uint32_t& volume) const;
    virtual int32_t MaxSpeakerVolume(uint32_t& maxVolume) const;
    virtual int32_t MinSpeakerVolume(uint32_t& minVolume) const;
    virtual int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const;

    // Microphone volume controls
    virtual int32_t MicrophoneVolumeIsAvailable(bool& available);
    virtual int32_t SetMicrophoneVolume(uint32_t volume);
    virtual int32_t MicrophoneVolume(uint32_t& volume) const;
    virtual int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const;
    virtual int32_t MinMicrophoneVolume(uint32_t& minVolume) const;
    virtual int32_t
        MicrophoneVolumeStepSize(uint16_t& stepSize) const;

    // Microphone mute control
    virtual int32_t MicrophoneMuteIsAvailable(bool& available);
    virtual int32_t SetMicrophoneMute(bool enable);
    virtual int32_t MicrophoneMute(bool& enabled) const;

    // Speaker mute control
    virtual int32_t SpeakerMuteIsAvailable(bool& available);
    virtual int32_t SetSpeakerMute(bool enable);
    virtual int32_t SpeakerMute(bool& enabled) const;

    // Microphone boost control
    virtual int32_t MicrophoneBoostIsAvailable(bool& available);
    virtual int32_t SetMicrophoneBoost(bool enable);
    virtual int32_t MicrophoneBoost(bool& enabled) const;

    // Stereo support
    virtual int32_t StereoPlayoutIsAvailable(bool& available);
    virtual int32_t SetStereoPlayout(bool enable);
    virtual int32_t StereoPlayout(bool& enabled) const;
    virtual int32_t StereoRecordingIsAvailable(bool& available);
    virtual int32_t SetStereoRecording(bool enable);
    virtual int32_t StereoRecording(bool& enabled) const;

    // Delay information and control
    virtual int32_t
        SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                         uint16_t sizeMS);
    virtual int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,
                                        uint16_t& sizeMS) const;
    virtual int32_t PlayoutDelay(uint16_t& delayMS) const;
    virtual int32_t RecordingDelay(uint16_t& delayMS) const;

    // CPU load
    virtual int32_t CPULoad(uint16_t& load) const;

public:
    virtual bool PlayoutWarning() const;
    virtual bool PlayoutError() const;
    virtual bool RecordingWarning() const;
    virtual bool RecordingError() const;
    virtual void ClearPlayoutWarning();
    virtual void ClearPlayoutError();
    virtual void ClearRecordingWarning();
    virtual void ClearRecordingError();

public:
    virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

    // Reset Audio Deivce (for mobile devices only)
    virtual int32_t ResetAudioDevice();

    // enable or disable loud speaker (for iphone only)
    virtual int32_t SetLoudspeakerStatus(bool enable);
    virtual int32_t GetLoudspeakerStatus(bool& enabled) const;

private:
    void Lock() {
        _critSect.Enter();
    }

    void UnLock() {
        _critSect.Leave();
    }

    int32_t Id() {
        return _id;
    }

    // Init and shutdown
    int32_t InitPlayOrRecord();
    int32_t ShutdownPlayOrRecord();

    void UpdateRecordingDelay();
    void UpdatePlayoutDelay();

    static OSStatus RecordProcess(void *inRefCon,
                                  AudioUnitRenderActionFlags *ioActionFlags,
                                  const AudioTimeStamp *timeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList *ioData);

    static OSStatus PlayoutProcess(void *inRefCon,
                                   AudioUnitRenderActionFlags *ioActionFlags,
                                   const AudioTimeStamp *timeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList *ioData);

    OSStatus RecordProcessImpl(AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *timeStamp,
                               uint32_t inBusNumber,
                               uint32_t inNumberFrames);

    OSStatus PlayoutProcessImpl(uint32_t inNumberFrames,
                                AudioBufferList *ioData);

    static bool RunCapture(void* ptrThis);
    bool CaptureWorkerThread();

private:
    AudioDeviceBuffer* _ptrAudioBuffer;

    CriticalSectionWrapper& _critSect;

    ThreadWrapper* _captureWorkerThread;
    uint32_t _captureWorkerThreadId;

    int32_t _id;

    AudioUnit _auVoiceProcessing;

private:
    bool _initialized;
    bool _isShutDown;
    bool _recording;
    bool _playing;
    bool _recIsInitialized;
    bool _playIsInitialized;

    bool _recordingDeviceIsSpecified;
    bool _playoutDeviceIsSpecified;
    bool _micIsInitialized;
    bool _speakerIsInitialized;

    bool _AGC;

    // The sampling rate to use with Audio Device Buffer
    uint32_t _adbSampFreq;

    // Delay calculation
    uint32_t _recordingDelay;
    uint32_t _playoutDelay;
    uint32_t _playoutDelayMeasurementCounter;
    uint32_t _recordingDelayHWAndOS;
    uint32_t _recordingDelayMeasurementCounter;

    // Errors and warnings count
    uint16_t _playWarning;
    uint16_t _playError;
    uint16_t _recWarning;
    uint16_t _recError;

    // Playout buffer, needed for 44.0 / 44.1 kHz mismatch
    int16_t _playoutBuffer[ENGINE_PLAY_BUF_SIZE_IN_SAMPLES];
    uint32_t  _playoutBufferUsed;  // How much is filled

    // Recording buffers
    int16_t
        _recordingBuffer[N_REC_BUFFERS][ENGINE_REC_BUF_SIZE_IN_SAMPLES];
    uint32_t _recordingLength[N_REC_BUFFERS];
    uint32_t _recordingSeqNumber[N_REC_BUFFERS];
    uint32_t _recordingCurrentSeq;

    // Current total size all data in buffers, used for delay estimate
    uint32_t _recordingBufferTotalSize;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_MAIN_SOURCE_MAC_AUDIO_DEVICE_IPHONE_H_
