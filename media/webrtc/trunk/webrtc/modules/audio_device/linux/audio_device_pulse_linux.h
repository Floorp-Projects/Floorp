/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_PULSE_LINUX_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_PULSE_LINUX_H

#include "audio_device_generic.h"
#include "audio_mixer_manager_pulse_linux.h"
#include "critical_section_wrapper.h"

#include <pulse/pulseaudio.h>

// We define this flag if it's missing from our headers, because we want to be
// able to compile against old headers but still use PA_STREAM_ADJUST_LATENCY
// if run against a recent version of the library.
#ifndef PA_STREAM_ADJUST_LATENCY
#define PA_STREAM_ADJUST_LATENCY 0x2000U
#endif
#ifndef PA_STREAM_START_MUTED
#define PA_STREAM_START_MUTED 0x1000U
#endif

// Set this constant to 0 to disable latency reading
const WebRtc_UWord32 WEBRTC_PA_REPORT_LATENCY = 1;

// Constants from implementation by Tristan Schmelcher [tschmelcher@google.com]

// First PulseAudio protocol version that supports PA_STREAM_ADJUST_LATENCY.
const WebRtc_UWord32 WEBRTC_PA_ADJUST_LATENCY_PROTOCOL_VERSION = 13;

// Some timing constants for optimal operation. See
// https://tango.0pointer.de/pipermail/pulseaudio-discuss/2008-January/001170.html
// for a good explanation of some of the factors that go into this.

// Playback.

// For playback, there is a round-trip delay to fill the server-side playback
// buffer, so setting too low of a latency is a buffer underflow risk. We will
// automatically increase the latency if a buffer underflow does occur, but we
// also enforce a sane minimum at start-up time. Anything lower would be
// virtually guaranteed to underflow at least once, so there's no point in
// allowing lower latencies.
const WebRtc_UWord32 WEBRTC_PA_PLAYBACK_LATENCY_MINIMUM_MSECS = 20;

// Every time a playback stream underflows, we will reconfigure it with target
// latency that is greater by this amount.
const WebRtc_UWord32 WEBRTC_PA_PLAYBACK_LATENCY_INCREMENT_MSECS = 20;

// We also need to configure a suitable request size. Too small and we'd burn
// CPU from the overhead of transfering small amounts of data at once. Too large
// and the amount of data remaining in the buffer right before refilling it
// would be a buffer underflow risk. We set it to half of the buffer size.
const WebRtc_UWord32 WEBRTC_PA_PLAYBACK_REQUEST_FACTOR = 2;

// Capture.

// For capture, low latency is not a buffer overflow risk, but it makes us burn
// CPU from the overhead of transfering small amounts of data at once, so we set
// a recommended value that we use for the kLowLatency constant (but if the user
// explicitly requests something lower then we will honour it).
// 1ms takes about 6-7% CPU. 5ms takes about 5%. 10ms takes about 4.x%.
const WebRtc_UWord32 WEBRTC_PA_LOW_CAPTURE_LATENCY_MSECS = 10;

// There is a round-trip delay to ack the data to the server, so the
// server-side buffer needs extra space to prevent buffer overflow. 20ms is
// sufficient, but there is no penalty to making it bigger, so we make it huge.
// (750ms is libpulse's default value for the _total_ buffer size in the
// kNoLatencyRequirements case.)
const WebRtc_UWord32 WEBRTC_PA_CAPTURE_BUFFER_EXTRA_MSECS = 750;

const WebRtc_UWord32 WEBRTC_PA_MSECS_PER_SEC = 1000;

// Init _configuredLatencyRec/Play to this value to disable latency requirements
const WebRtc_Word32 WEBRTC_PA_NO_LATENCY_REQUIREMENTS = -1;

// Set this const to 1 to account for peeked and used data in latency calculation
const WebRtc_UWord32 WEBRTC_PA_CAPTURE_BUFFER_LATENCY_ADJUSTMENT = 0;

namespace webrtc
{
class EventWrapper;
class ThreadWrapper;

class AudioDeviceLinuxPulse: public AudioDeviceGeneric
{
public:
    AudioDeviceLinuxPulse(const WebRtc_Word32 id);
    ~AudioDeviceLinuxPulse();

    static bool PulseAudioIsSupported();

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
    virtual WebRtc_Word32 PlayoutDeviceName(
        WebRtc_UWord16 index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]);
    virtual WebRtc_Word32 RecordingDeviceName(
        WebRtc_UWord16 index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize]);

    // Device selection
    virtual WebRtc_Word32 SetPlayoutDevice(WebRtc_UWord16 index);
    virtual WebRtc_Word32 SetPlayoutDevice(
        AudioDeviceModule::WindowsDeviceType device);
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
    virtual WebRtc_Word32 MicrophoneVolumeStepSize(
        WebRtc_UWord16& stepSize) const;

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

private:
    void Lock()
    {
        _critSect.Enter();
    }
    ;
    void UnLock()
    {
        _critSect.Leave();
    }
    ;
    void WaitForOperationCompletion(pa_operation* paOperation) const;
    void WaitForSuccess(pa_operation* paOperation) const;

private:
    static void PaContextStateCallback(pa_context *c, void *pThis);
    static void PaSinkInfoCallback(pa_context *c, const pa_sink_info *i,
                                   int eol, void *pThis);
    static void PaSourceInfoCallback(pa_context *c, const pa_source_info *i,
                                     int eol, void *pThis);
    static void PaServerInfoCallback(pa_context *c, const pa_server_info *i,
                                     void *pThis);
    static void PaStreamStateCallback(pa_stream *p, void *pThis);
    void PaContextStateCallbackHandler(pa_context *c);
    void PaSinkInfoCallbackHandler(const pa_sink_info *i, int eol);
    void PaSourceInfoCallbackHandler(const pa_source_info *i, int eol);
    void PaServerInfoCallbackHandler(const pa_server_info *i);
    void PaStreamStateCallbackHandler(pa_stream *p);

    void EnableWriteCallback();
    void DisableWriteCallback();
    static void PaStreamWriteCallback(pa_stream *unused, size_t buffer_space,
                                      void *pThis);
    void PaStreamWriteCallbackHandler(size_t buffer_space);
    static void PaStreamUnderflowCallback(pa_stream *unused, void *pThis);
    void PaStreamUnderflowCallbackHandler();
    void EnableReadCallback();
    void DisableReadCallback();
    static void PaStreamReadCallback(pa_stream *unused1, size_t unused2,
                                     void *pThis);
    void PaStreamReadCallbackHandler();
    static void PaStreamOverflowCallback(pa_stream *unused, void *pThis);
    void PaStreamOverflowCallbackHandler();
    WebRtc_Word32 LatencyUsecs(pa_stream *stream);
    WebRtc_Word32 ReadRecordedData(const void* bufferData, size_t bufferSize);
    WebRtc_Word32 ProcessRecordedData(WebRtc_Word8 *bufferData,
                                      WebRtc_UWord32 bufferSizeInSamples,
                                      WebRtc_UWord32 recDelay);

    WebRtc_Word32 CheckPulseAudioVersion();
    WebRtc_Word32 InitSamplingFrequency();
    WebRtc_Word32 GetDefaultDeviceInfo(bool recDevice, char* name,
                                       WebRtc_UWord16& index);
    WebRtc_Word32 InitPulseAudio();
    WebRtc_Word32 TerminatePulseAudio();

    void PaLock();
    void PaUnLock();

    static bool RecThreadFunc(void*);
    static bool PlayThreadFunc(void*);
    bool RecThreadProcess();
    bool PlayThreadProcess();

private:
    AudioDeviceBuffer* _ptrAudioBuffer;

    CriticalSectionWrapper& _critSect;
    EventWrapper& _timeEventRec;
    EventWrapper& _timeEventPlay;
    EventWrapper& _recStartEvent;
    EventWrapper& _playStartEvent;

    ThreadWrapper* _ptrThreadPlay;
    ThreadWrapper* _ptrThreadRec;
    WebRtc_UWord32 _recThreadID;
    WebRtc_UWord32 _playThreadID;
    WebRtc_Word32 _id;

    AudioMixerManagerLinuxPulse _mixerManager;

    WebRtc_UWord16 _inputDeviceIndex;
    WebRtc_UWord16 _outputDeviceIndex;
    bool _inputDeviceIsSpecified;
    bool _outputDeviceIsSpecified;

    WebRtc_Word32 sampling_rate_hz;
    WebRtc_UWord8 _recChannels;
    WebRtc_UWord8 _playChannels;

    AudioDeviceModule::BufferType _playBufType;

private:
    bool _initialized;
    bool _recording;
    bool _playing;
    bool _recIsInitialized;
    bool _playIsInitialized;
    bool _startRec;
    bool _stopRec;
    bool _startPlay;
    bool _stopPlay;
    bool _AGC;
    bool update_speaker_volume_at_startup_;

private:
    WebRtc_UWord16 _playBufDelayFixed; // fixed playback delay

    WebRtc_UWord32 _sndCardPlayDelay;
    WebRtc_UWord32 _sndCardRecDelay;

    WebRtc_Word32 _writeErrors;
    WebRtc_UWord16 _playWarning;
    WebRtc_UWord16 _playError;
    WebRtc_UWord16 _recWarning;
    WebRtc_UWord16 _recError;

    WebRtc_UWord16 _deviceIndex;
    WebRtc_Word16 _numPlayDevices;
    WebRtc_Word16 _numRecDevices;
    char* _playDeviceName;
    char* _recDeviceName;
    char* _playDisplayDeviceName;
    char* _recDisplayDeviceName;
    char _paServerVersion[32];

    WebRtc_Word8* _playBuffer;
    size_t _playbackBufferSize;
    size_t _playbackBufferUnused;
    size_t _tempBufferSpace;
    WebRtc_Word8* _recBuffer;
    size_t _recordBufferSize;
    size_t _recordBufferUsed;
    const void* _tempSampleData;
    size_t _tempSampleDataSize;
    WebRtc_Word32 _configuredLatencyPlay;
    WebRtc_Word32 _configuredLatencyRec;

    // PulseAudio
    WebRtc_UWord16 _paDeviceIndex;
    bool _paStateChanged;

    pa_threaded_mainloop* _paMainloop;
    pa_mainloop_api* _paMainloopApi;
    pa_context* _paContext;

    pa_stream* _recStream;
    pa_stream* _playStream;
    WebRtc_UWord32 _recStreamFlags;
    WebRtc_UWord32 _playStreamFlags;
    pa_buffer_attr _playBufferAttr;
    pa_buffer_attr _recBufferAttr;
};

}

#endif  // MODULES_AUDIO_DEVICE_MAIN_SOURCE_LINUX_AUDIO_DEVICE_PULSE_LINUX_H_
