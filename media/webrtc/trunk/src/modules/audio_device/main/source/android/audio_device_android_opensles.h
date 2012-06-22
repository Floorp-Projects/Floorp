/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_ANDROID_OPENSLES_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_ANDROID_OPENSLES_H

#include "audio_device_generic.h"
#include "critical_section_wrapper.h"

#include <jni.h> // For accessing AudioDeviceAndroid.java
#include <stdio.h>
#include <stdlib.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

namespace webrtc
{
class EventWrapper;

const WebRtc_UWord32 N_MAX_INTERFACES = 3;
const WebRtc_UWord32 N_MAX_OUTPUT_DEVICES = 6;
const WebRtc_UWord32 N_MAX_INPUT_DEVICES = 3;

const WebRtc_UWord32 N_REC_SAMPLES_PER_SEC = 16000;//44000;  // Default fs
const WebRtc_UWord32 N_PLAY_SAMPLES_PER_SEC = 16000;//44000; // Default fs

const WebRtc_UWord32 N_REC_CHANNELS = 1; // default is mono recording
const WebRtc_UWord32 N_PLAY_CHANNELS = 1; // default is mono playout

const WebRtc_UWord32 REC_BUF_SIZE_IN_SAMPLES = 480; // Handle max 10 ms @ 48 kHz
const WebRtc_UWord32 PLAY_BUF_SIZE_IN_SAMPLES = 480;

// Number of the buffers in playout queue
const WebRtc_UWord16 N_PLAY_QUEUE_BUFFERS = 2;
// Number of buffers in recording queue
const WebRtc_UWord16 N_REC_QUEUE_BUFFERS = 2;
// Number of 10 ms recording blocks in rec buffer
const WebRtc_UWord16 N_REC_BUFFERS = 20;

class ThreadWrapper;

class AudioDeviceAndroidOpenSLES: public AudioDeviceGeneric
{
public:
    AudioDeviceAndroidOpenSLES(const WebRtc_Word32 id);
    ~AudioDeviceAndroidOpenSLES();

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
    virtual WebRtc_Word32
            PlayoutDeviceName(WebRtc_UWord16 index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]);
    virtual WebRtc_Word32
            RecordingDeviceName(WebRtc_UWord16 index,
                                char name[kAdmMaxDeviceNameSize],
                                char guid[kAdmMaxGuidSize]);

    // Device selection
    virtual WebRtc_Word32 SetPlayoutDevice(WebRtc_UWord16 index);
    virtual WebRtc_Word32
            SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device);
    virtual WebRtc_Word32 SetRecordingDevice(WebRtc_UWord16 index);
    virtual WebRtc_Word32
            SetRecordingDevice(AudioDeviceModule::WindowsDeviceType device);

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
    SLPlayItf playItf;
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

    // Speaker mute control
    virtual WebRtc_Word32 SpeakerMuteIsAvailable(bool& available);
    virtual WebRtc_Word32 SetSpeakerMute(bool enable);
    virtual WebRtc_Word32 SpeakerMute(bool& enabled) const;

    // Microphone mute control
    virtual WebRtc_Word32 MicrophoneMuteIsAvailable(bool& available);
    virtual WebRtc_Word32 SetMicrophoneMute(bool enable);
    virtual WebRtc_Word32 MicrophoneMute(bool& enabled) const;

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

    // Error and warning information
    virtual bool PlayoutWarning() const;
    virtual bool PlayoutError() const;
    virtual bool RecordingWarning() const;
    virtual bool RecordingError() const;
    virtual void ClearPlayoutWarning();
    virtual void ClearPlayoutError();
    virtual void ClearRecordingWarning();
    virtual void ClearRecordingError();

    // Attach audio buffer
    virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

    // Speaker audio routing
    virtual WebRtc_Word32 SetLoudspeakerStatus(bool enable);
    virtual WebRtc_Word32 GetLoudspeakerStatus(bool& enable) const;

private:
    // Lock
    void Lock()
    {
        _critSect.Enter();
    };
    void UnLock()
    {
        _critSect.Leave();
    };

    static void PlayerSimpleBufferQueueCallback(
            SLAndroidSimpleBufferQueueItf queueItf,
            void *pContext);
    void PlayerSimpleBufferQueueCallbackHandler(
            SLAndroidSimpleBufferQueueItf queueItf);
    static void RecorderSimpleBufferQueueCallback(
            SLAndroidSimpleBufferQueueItf queueItf,
            void *pContext);
    void RecorderSimpleBufferQueueCallbackHandler(
            SLAndroidSimpleBufferQueueItf queueItf);
    void CheckErr(SLresult res);

    // Delay updates
    void UpdateRecordingDelay();
    void UpdatePlayoutDelay(WebRtc_UWord32 nSamplePlayed);

    // Init
    WebRtc_Word32 InitSampleRate();

    // Threads
    static bool RecThreadFunc(void*);
    static bool PlayThreadFunc(void*);
    bool RecThreadProcess();
    bool PlayThreadProcess();

    // Misc
    AudioDeviceBuffer* _ptrAudioBuffer;
    CriticalSectionWrapper& _critSect;
    WebRtc_Word32 _id;

    // audio unit
    SLObjectItf _slEngineObject;

    // playout device
    SLObjectItf _slPlayer;
    SLEngineItf _slEngine;
    SLPlayItf _slPlayerPlay;
    SLAndroidSimpleBufferQueueItf _slPlayerSimpleBufferQueue;
    SLObjectItf _slOutputMixObject;
    SLVolumeItf _slSpeakerVolume;

    // recording device
    SLObjectItf _slRecorder;
    SLRecordItf _slRecorderRecord;
    SLAudioIODeviceCapabilitiesItf _slAudioIODeviceCapabilities;
    SLAndroidSimpleBufferQueueItf _slRecorderSimpleBufferQueue;
    SLDeviceVolumeItf _slMicVolume;

    WebRtc_UWord32 _micDeviceId;
    WebRtc_UWord32 _recQueueSeq;

    // Events
    EventWrapper& _timeEventRec;
    // Threads
    ThreadWrapper* _ptrThreadRec;
    WebRtc_UWord32 _recThreadID;
    // TODO(xians), remove the following flag
    bool _recThreadIsInitialized;

    // Playout buffer
    WebRtc_Word8 _playQueueBuffer[N_PLAY_QUEUE_BUFFERS][2
            * PLAY_BUF_SIZE_IN_SAMPLES];
    WebRtc_UWord32 _playQueueSeq;
    // Recording buffer
    WebRtc_Word8 _recQueueBuffer[N_REC_QUEUE_BUFFERS][2
            * REC_BUF_SIZE_IN_SAMPLES];
    WebRtc_Word8 _recBuffer[N_REC_BUFFERS][2*REC_BUF_SIZE_IN_SAMPLES];
    WebRtc_UWord32 _recLength[N_REC_BUFFERS];
    WebRtc_UWord32 _recSeqNumber[N_REC_BUFFERS];
    WebRtc_UWord32 _recCurrentSeq;
    // Current total size all data in buffers, used for delay estimate
    WebRtc_UWord32 _recBufferTotalSize;

    // States
    bool _recordingDeviceIsSpecified;
    bool _playoutDeviceIsSpecified;
    bool _initialized;
    bool _recording;
    bool _playing;
    bool _recIsInitialized;
    bool _playIsInitialized;
    bool _micIsInitialized;
    bool _speakerIsInitialized;

    // Warnings and errors
    WebRtc_UWord16 _playWarning;
    WebRtc_UWord16 _playError;
    WebRtc_UWord16 _recWarning;
    WebRtc_UWord16 _recError;

    // Delay
    WebRtc_UWord16 _playoutDelay;
    WebRtc_UWord16 _recordingDelay;

    // AGC state
    bool _AGC;

    // The sampling rate to use with Audio Device Buffer
    WebRtc_UWord32 _adbSampleRate;
    // Stored device properties
    WebRtc_UWord32 _samplingRateIn; // Sampling frequency for Mic
    WebRtc_UWord32 _samplingRateOut; // Sampling frequency for Speaker
    WebRtc_UWord32 _maxSpeakerVolume; // The maximum speaker volume value
    WebRtc_UWord32 _minSpeakerVolume; // The minimum speaker volume value
    bool _loudSpeakerOn;
};

} // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_ANDROID_OPENSLES_H
