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

#include "audio_device_generic.h"
#include "critical_section_wrapper.h"

namespace webrtc {
class ThreadWrapper;

const WebRtc_UWord32 N_REC_SAMPLES_PER_SEC = 44000;
const WebRtc_UWord32 N_PLAY_SAMPLES_PER_SEC = 44000;

const WebRtc_UWord32 N_REC_CHANNELS = 1;  // default is mono recording
const WebRtc_UWord32 N_PLAY_CHANNELS = 1;  // default is mono playout
const WebRtc_UWord32 N_DEVICE_CHANNELS = 8;

const WebRtc_UWord32 ENGINE_REC_BUF_SIZE_IN_SAMPLES = (N_REC_SAMPLES_PER_SEC
    / 100);
const WebRtc_UWord32 ENGINE_PLAY_BUF_SIZE_IN_SAMPLES = (N_PLAY_SAMPLES_PER_SEC
    / 100);

// Number of 10 ms recording blocks in recording buffer
const WebRtc_UWord16 N_REC_BUFFERS = 20;

class AudioDeviceIPhone : public AudioDeviceGeneric {
public:
    AudioDeviceIPhone(const WebRtc_Word32 id);
    ~AudioDeviceIPhone();

    // Retrieve the currently utilized audio layer
    virtual WebRtc_Word32
        ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const;

    // Main initializaton and termination
    virtual WebRtc_Word32 Init();
    virtual WebRtc_Word32 Terminate();
    virtual bool Initialized() const;

    // Device enumeration
    virtual WebRtc_Word16 PlayoutDevices();
    virtual WebRtc_Word16 RecordingDevices();
    virtual WebRtc_Word32 PlayoutDeviceName(WebRtc_UWord16 index,
                                            char name[kAdmMaxDeviceNameSize],
                                            char guid[kAdmMaxGuidSize]);
    virtual WebRtc_Word32 RecordingDeviceName(WebRtc_UWord16 index,
                                              char name[kAdmMaxDeviceNameSize],
                                              char guid[kAdmMaxGuidSize]);

    // Device selection
    virtual WebRtc_Word32 SetPlayoutDevice(WebRtc_UWord16 index);
    virtual WebRtc_Word32
        SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device);
    virtual WebRtc_Word32 SetRecordingDevice(WebRtc_UWord16 index);
    virtual WebRtc_Word32 SetRecordingDevice(
        AudioDeviceModule::WindowsDeviceType device);

    // Audio transport initialization
    virtual WebRtc_Word32 PlayoutIsAvailable(bool& available);
    virtual WebRtc_Word32 InitPlayout();
    virtual bool PlayoutIsInitialized() const;
    virtual WebRtc_Word32 RecordingIsAvailable(bool& available);
    virtual WebRtc_Word32 InitRecording();
    virtual bool RecordingIsInitialized() const;

    // Audio transport control
    virtual WebRtc_Word32 StartPlayout();
    virtual WebRtc_Word32 StopPlayout();
    virtual bool Playing() const;
    virtual WebRtc_Word32 StartRecording();
    virtual WebRtc_Word32 StopRecording();
    virtual bool Recording() const;

    // Microphone Automatic Gain Control (AGC)
    virtual WebRtc_Word32 SetAGC(bool enable);
    virtual bool AGC() const;

    // Volume control based on the Windows Wave API (Windows only)
    virtual WebRtc_Word32 SetWaveOutVolume(WebRtc_UWord16 volumeLeft,
                                           WebRtc_UWord16 volumeRight);
    virtual WebRtc_Word32 WaveOutVolume(WebRtc_UWord16& volumeLeft,
                                        WebRtc_UWord16& volumeRight) const;

    // Audio mixer initialization
    virtual WebRtc_Word32 SpeakerIsAvailable(bool& available);
    virtual WebRtc_Word32 InitSpeaker();
    virtual bool SpeakerIsInitialized() const;
    virtual WebRtc_Word32 MicrophoneIsAvailable(bool& available);
    virtual WebRtc_Word32 InitMicrophone();
    virtual bool MicrophoneIsInitialized() const;

    // Speaker volume controls
    virtual WebRtc_Word32 SpeakerVolumeIsAvailable(bool& available);
    virtual WebRtc_Word32 SetSpeakerVolume(WebRtc_UWord32 volume);
    virtual WebRtc_Word32 SpeakerVolume(WebRtc_UWord32& volume) const;
    virtual WebRtc_Word32 MaxSpeakerVolume(WebRtc_UWord32& maxVolume) const;
    virtual WebRtc_Word32 MinSpeakerVolume(WebRtc_UWord32& minVolume) const;
    virtual WebRtc_Word32 SpeakerVolumeStepSize(WebRtc_UWord16& stepSize) const;

    // Microphone volume controls
    virtual WebRtc_Word32 MicrophoneVolumeIsAvailable(bool& available);
    virtual WebRtc_Word32 SetMicrophoneVolume(WebRtc_UWord32 volume);
    virtual WebRtc_Word32 MicrophoneVolume(WebRtc_UWord32& volume) const;
    virtual WebRtc_Word32 MaxMicrophoneVolume(WebRtc_UWord32& maxVolume) const;
    virtual WebRtc_Word32 MinMicrophoneVolume(WebRtc_UWord32& minVolume) const;
    virtual WebRtc_Word32
        MicrophoneVolumeStepSize(WebRtc_UWord16& stepSize) const;

    // Microphone mute control
    virtual WebRtc_Word32 MicrophoneMuteIsAvailable(bool& available);
    virtual WebRtc_Word32 SetMicrophoneMute(bool enable);
    virtual WebRtc_Word32 MicrophoneMute(bool& enabled) const;

    // Speaker mute control
    virtual WebRtc_Word32 SpeakerMuteIsAvailable(bool& available);
    virtual WebRtc_Word32 SetSpeakerMute(bool enable);
    virtual WebRtc_Word32 SpeakerMute(bool& enabled) const;

    // Microphone boost control
    virtual WebRtc_Word32 MicrophoneBoostIsAvailable(bool& available);
    virtual WebRtc_Word32 SetMicrophoneBoost(bool enable);
    virtual WebRtc_Word32 MicrophoneBoost(bool& enabled) const;

    // Stereo support
    virtual WebRtc_Word32 StereoPlayoutIsAvailable(bool& available);
    virtual WebRtc_Word32 SetStereoPlayout(bool enable);
    virtual WebRtc_Word32 StereoPlayout(bool& enabled) const;
    virtual WebRtc_Word32 StereoRecordingIsAvailable(bool& available);
    virtual WebRtc_Word32 SetStereoRecording(bool enable);
    virtual WebRtc_Word32 StereoRecording(bool& enabled) const;

    // Delay information and control
    virtual WebRtc_Word32
        SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                         WebRtc_UWord16 sizeMS);
    virtual WebRtc_Word32 PlayoutBuffer(AudioDeviceModule::BufferType& type,
                                        WebRtc_UWord16& sizeMS) const;
    virtual WebRtc_Word32 PlayoutDelay(WebRtc_UWord16& delayMS) const;
    virtual WebRtc_Word32 RecordingDelay(WebRtc_UWord16& delayMS) const;

    // CPU load
    virtual WebRtc_Word32 CPULoad(WebRtc_UWord16& load) const;

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
    virtual WebRtc_Word32 ResetAudioDevice();

    // enable or disable loud speaker (for iphone only)
    virtual WebRtc_Word32 SetLoudspeakerStatus(bool enable);
    virtual WebRtc_Word32 GetLoudspeakerStatus(bool& enabled) const;

private:
    void Lock() {
        _critSect.Enter();
    }

    void UnLock() {
        _critSect.Leave();
    }

    WebRtc_Word32 Id() {
        return _id;
    }

    // Init and shutdown
    WebRtc_Word32 InitPlayOrRecord();
    WebRtc_Word32 ShutdownPlayOrRecord();

    void UpdateRecordingDelay();
    void UpdatePlayoutDelay();

    static OSStatus RecordProcess(void *inRefCon,
                                  AudioUnitRenderActionFlags *ioActionFlags,
                                  const AudioTimeStamp *WebRtc_Word32imeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList *ioData);

    static OSStatus PlayoutProcess(void *inRefCon,
                                   AudioUnitRenderActionFlags *ioActionFlags,
                                   const AudioTimeStamp *WebRtc_Word32imeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList *ioData);

    OSStatus RecordProcessImpl(AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *WebRtc_Word32imeStamp,
                               WebRtc_UWord32 inBusNumber,
                               WebRtc_UWord32 inNumberFrames);

    OSStatus PlayoutProcessImpl(WebRtc_UWord32 inNumberFrames,
                                AudioBufferList *ioData);

    static bool RunCapture(void* ptrThis);
    bool CaptureWorkerThread();

private:
    AudioDeviceBuffer* _ptrAudioBuffer;

    CriticalSectionWrapper& _critSect;

    ThreadWrapper* _captureWorkerThread;
    WebRtc_UWord32 _captureWorkerThreadId;

    WebRtc_Word32 _id;

    AudioUnit _auRemoteIO;

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
    WebRtc_UWord32 _adbSampFreq;

    // Delay calculation
    WebRtc_UWord32 _recordingDelay;
    WebRtc_UWord32 _playoutDelay;
    WebRtc_UWord32 _playoutDelayMeasurementCounter;
    WebRtc_UWord32 _recordingDelayHWAndOS;
    WebRtc_UWord32 _recordingDelayMeasurementCounter;

    // Errors and warnings count
    WebRtc_UWord16 _playWarning;
    WebRtc_UWord16 _playError;
    WebRtc_UWord16 _recWarning;
    WebRtc_UWord16 _recError;

    // Playout buffer, needed for 44.0 / 44.1 kHz mismatch
    WebRtc_Word16 _playoutBuffer[ENGINE_PLAY_BUF_SIZE_IN_SAMPLES];
    WebRtc_UWord32  _playoutBufferUsed;  // How much is filled

    // Recording buffers
    WebRtc_Word16
        _recordingBuffer[N_REC_BUFFERS][ENGINE_REC_BUF_SIZE_IN_SAMPLES];
    WebRtc_UWord32 _recordingLength[N_REC_BUFFERS];
    WebRtc_UWord32 _recordingSeqNumber[N_REC_BUFFERS];
    WebRtc_UWord32 _recordingCurrentSeq;

    // Current total size all data in buffers, used for delay estimate
    WebRtc_UWord32 _recordingBufferTotalSize;
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_MAIN_SOURCE_MAC_AUDIO_DEVICE_IPHONE_H_
