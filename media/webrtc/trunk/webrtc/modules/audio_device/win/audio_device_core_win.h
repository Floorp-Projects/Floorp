/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_CORE_WIN_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_CORE_WIN_H

#if (_MSC_VER >= 1400)  // only include for VS 2005 and higher

#include "audio_device_generic.h"

#pragma once
#include <wmcodecdsp.h>      // CLSID_CWMAudioAEC
                             // (must be before audioclient.h)
#include <Audioclient.h>     // WASAPI
#include <Audiopolicy.h>
#include <avrt.h>            // Avrt
#include <endpointvolume.h>
#include <mediaobj.h>        // IMediaObject
#include <Mmdeviceapi.h>     // MMDevice

#include "critical_section_wrapper.h"
#include "scoped_refptr.h"

// Use Multimedia Class Scheduler Service (MMCSS) to boost the thread priority
#pragma comment( lib, "avrt.lib" )
// AVRT function pointers
typedef BOOL (WINAPI *PAvRevertMmThreadCharacteristics)(HANDLE);
typedef HANDLE (WINAPI *PAvSetMmThreadCharacteristicsA)(LPCSTR, LPDWORD);
typedef BOOL (WINAPI *PAvSetMmThreadPriority)(HANDLE, AVRT_PRIORITY);

namespace webrtc {

const float MAX_CORE_SPEAKER_VOLUME = 255.0f;
const float MIN_CORE_SPEAKER_VOLUME = 0.0f;
const float MAX_CORE_MICROPHONE_VOLUME = 255.0f;
const float MIN_CORE_MICROPHONE_VOLUME = 0.0f;
const WebRtc_UWord16 CORE_SPEAKER_VOLUME_STEP_SIZE = 1;
const WebRtc_UWord16 CORE_MICROPHONE_VOLUME_STEP_SIZE = 1;

// Utility class which initializes COM in the constructor (STA or MTA),
// and uninitializes COM in the destructor.
class ScopedCOMInitializer {
 public:
  // Enum value provided to initialize the thread as an MTA instead of STA.
  enum SelectMTA { kMTA };

  // Constructor for STA initialization.
  ScopedCOMInitializer() {
    Initialize(COINIT_APARTMENTTHREADED);
  }

  // Constructor for MTA initialization.
  explicit ScopedCOMInitializer(SelectMTA mta) {
    Initialize(COINIT_MULTITHREADED);
  }

  ScopedCOMInitializer::~ScopedCOMInitializer() {
    if (SUCCEEDED(hr_))
      CoUninitialize();
  }

  bool succeeded() const { return SUCCEEDED(hr_); }
 
 private:
  void Initialize(COINIT init) {
    hr_ = CoInitializeEx(NULL, init);
  }

  HRESULT hr_;

  ScopedCOMInitializer(const ScopedCOMInitializer&);
  void operator=(const ScopedCOMInitializer&);
};


class AudioDeviceWindowsCore : public AudioDeviceGeneric
{
public:
    AudioDeviceWindowsCore(const WebRtc_Word32 id);
    ~AudioDeviceWindowsCore();

    static bool CoreAudioIsSupported();

    // Retrieve the currently utilized audio layer
    virtual WebRtc_Word32 ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const;

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
    virtual WebRtc_Word32 SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType device);
    virtual WebRtc_Word32 SetRecordingDevice(WebRtc_UWord16 index);
    virtual WebRtc_Word32 SetRecordingDevice(AudioDeviceModule::WindowsDeviceType device);

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
    virtual WebRtc_Word32 SetWaveOutVolume(WebRtc_UWord16 volumeLeft, WebRtc_UWord16 volumeRight);
    virtual WebRtc_Word32 WaveOutVolume(WebRtc_UWord16& volumeLeft, WebRtc_UWord16& volumeRight) const;

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
    virtual WebRtc_Word32 MicrophoneVolumeStepSize(WebRtc_UWord16& stepSize) const;

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
    virtual WebRtc_Word32 SetPlayoutBuffer(const AudioDeviceModule::BufferType type, WebRtc_UWord16 sizeMS);
    virtual WebRtc_Word32 PlayoutBuffer(AudioDeviceModule::BufferType& type, WebRtc_UWord16& sizeMS) const;
    virtual WebRtc_Word32 PlayoutDelay(WebRtc_UWord16& delayMS) const;
    virtual WebRtc_Word32 RecordingDelay(WebRtc_UWord16& delayMS) const;

    // CPU load
    virtual WebRtc_Word32 CPULoad(WebRtc_UWord16& load) const;

    virtual int32_t EnableBuiltInAEC(bool enable);
    virtual bool BuiltInAECIsEnabled() const;

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

private:    // avrt function pointers
    PAvRevertMmThreadCharacteristics    _PAvRevertMmThreadCharacteristics;
    PAvSetMmThreadCharacteristicsA      _PAvSetMmThreadCharacteristicsA;
    PAvSetMmThreadPriority              _PAvSetMmThreadPriority;
    HMODULE                             _avrtLibrary;
    bool                                _winSupportAvrt;

private:    // thread functions
    DWORD InitCaptureThreadPriority();
    void RevertCaptureThreadPriority();
    static DWORD WINAPI WSAPICaptureThread(LPVOID context);
    DWORD DoCaptureThread();

    static DWORD WINAPI WSAPICaptureThreadPollDMO(LPVOID context);
    DWORD DoCaptureThreadPollDMO();

    static DWORD WINAPI WSAPIRenderThread(LPVOID context);
    DWORD DoRenderThread();

    static DWORD WINAPI GetCaptureVolumeThread(LPVOID context);
    DWORD DoGetCaptureVolumeThread();

    static DWORD WINAPI SetCaptureVolumeThread(LPVOID context);
    DWORD DoSetCaptureVolumeThread();

    void _SetThreadName(DWORD dwThreadID, LPCSTR szThreadName);
    void _Lock() { _critSect.Enter(); };
    void _UnLock() { _critSect.Leave(); };

private:
    WebRtc_Word32 Id() {return _id;}

private:
    int SetDMOProperties();

    int SetBoolProperty(IPropertyStore* ptrPS,
                        REFPROPERTYKEY key,
                        VARIANT_BOOL value);

    int SetVtI4Property(IPropertyStore* ptrPS,
                        REFPROPERTYKEY key,
                        LONG value);

    WebRtc_Word32 _EnumerateEndpointDevicesAll(EDataFlow dataFlow) const;
    void _TraceCOMError(HRESULT hr) const;

    WebRtc_Word32 _RefreshDeviceList(EDataFlow dir);
    WebRtc_Word16 _DeviceListCount(EDataFlow dir);
    WebRtc_Word32 _GetDefaultDeviceName(EDataFlow dir, ERole role, LPWSTR szBuffer, int bufferLen);
    WebRtc_Word32 _GetListDeviceName(EDataFlow dir, int index, LPWSTR szBuffer, int bufferLen);
    WebRtc_Word32 _GetDeviceName(IMMDevice* pDevice, LPWSTR pszBuffer, int bufferLen);
    WebRtc_Word32 _GetListDeviceID(EDataFlow dir, int index, LPWSTR szBuffer, int bufferLen);
    WebRtc_Word32 _GetDefaultDeviceID(EDataFlow dir, ERole role, LPWSTR szBuffer, int bufferLen);
    WebRtc_Word32 _GetDefaultDeviceIndex(EDataFlow dir, ERole role, int* index);
    WebRtc_Word32 _GetDeviceID(IMMDevice* pDevice, LPWSTR pszBuffer, int bufferLen);
    WebRtc_Word32 _GetDefaultDevice(EDataFlow dir, ERole role, IMMDevice** ppDevice);
    WebRtc_Word32 _GetListDevice(EDataFlow dir, int index, IMMDevice** ppDevice);

    void _Get44kHzDrift();

    // Converts from wide-char to UTF-8 if UNICODE is defined.
    // Does nothing if UNICODE is undefined.
    char* WideToUTF8(const TCHAR* src) const;

    WebRtc_Word32 InitRecordingDMO();

private:
    ScopedCOMInitializer                    _comInit;
    AudioDeviceBuffer*                      _ptrAudioBuffer;
    CriticalSectionWrapper&                 _critSect;
    CriticalSectionWrapper&                 _volumeMutex;
    WebRtc_Word32                           _id;

private:  // MMDevice
    IMMDeviceEnumerator*                    _ptrEnumerator;
    IMMDeviceCollection*                    _ptrRenderCollection;
    IMMDeviceCollection*                    _ptrCaptureCollection;
    IMMDevice*                              _ptrDeviceOut;
    IMMDevice*                              _ptrDeviceIn;

private:  // WASAPI
    IAudioClient*                           _ptrClientOut;
    IAudioClient*                           _ptrClientIn;
    IAudioRenderClient*                     _ptrRenderClient;
    IAudioCaptureClient*                    _ptrCaptureClient;
    IAudioEndpointVolume*                   _ptrCaptureVolume;
    ISimpleAudioVolume*                     _ptrRenderSimpleVolume;

    // DirectX Media Object (DMO) for the built-in AEC.
    scoped_refptr<IMediaObject>             _dmo;
    scoped_refptr<IMediaBuffer>             _mediaBuffer;
    bool                                    _builtInAecEnabled;

    HANDLE                                  _hRenderSamplesReadyEvent;
    HANDLE                                  _hPlayThread;
    HANDLE                                  _hRenderStartedEvent;
    HANDLE                                  _hShutdownRenderEvent;

    HANDLE                                  _hCaptureSamplesReadyEvent;
    HANDLE                                  _hRecThread;
    HANDLE                                  _hCaptureStartedEvent;
    HANDLE                                  _hShutdownCaptureEvent;

    HANDLE                                  _hGetCaptureVolumeThread;
    HANDLE                                  _hSetCaptureVolumeThread;
    HANDLE                                  _hSetCaptureVolumeEvent;

    HANDLE                                  _hMmTask;

    UINT                                    _playAudioFrameSize;
    WebRtc_UWord32                          _playSampleRate;
    WebRtc_UWord32                          _devicePlaySampleRate;
    WebRtc_UWord32                          _playBlockSize;
    WebRtc_UWord32                          _devicePlayBlockSize;
    WebRtc_UWord32                          _playChannels;
    WebRtc_UWord32                          _sndCardPlayDelay;
    UINT64                                  _writtenSamples;
    LONGLONG                                _playAcc;

    UINT                                    _recAudioFrameSize;
    WebRtc_UWord32                          _recSampleRate;
    WebRtc_UWord32                          _recBlockSize;
    WebRtc_UWord32                          _recChannels;
    UINT64                                  _readSamples;
    WebRtc_UWord32                          _sndCardRecDelay;

    float                                   _sampleDriftAt48kHz;
    float                                   _driftAccumulator;

    WebRtc_UWord16                          _recChannelsPrioList[2];
    WebRtc_UWord16                          _playChannelsPrioList[2];

    LARGE_INTEGER                           _perfCounterFreq;
    double                                  _perfCounterFactor;
    float                                   _avgCPULoad;

private:
    bool                                    _initialized;
    bool                                    _recording;
    bool                                    _playing;
    bool                                    _recIsInitialized;
    bool                                    _playIsInitialized;
    bool                                    _speakerIsInitialized;
    bool                                    _microphoneIsInitialized;

    bool                                    _usingInputDeviceIndex;
    bool                                    _usingOutputDeviceIndex;
    AudioDeviceModule::WindowsDeviceType    _inputDevice;
    AudioDeviceModule::WindowsDeviceType    _outputDevice;
    WebRtc_UWord16                          _inputDeviceIndex;
    WebRtc_UWord16                          _outputDeviceIndex;

    bool                                    _AGC;

    WebRtc_UWord16                          _playWarning;
    WebRtc_UWord16                          _playError;
    WebRtc_UWord16                          _recWarning;
    WebRtc_UWord16                          _recError;

    AudioDeviceModule::BufferType           _playBufType;
    WebRtc_UWord16                          _playBufDelay;
    WebRtc_UWord16                          _playBufDelayFixed;

    WebRtc_UWord16                          _newMicLevel;

    mutable char                            _str[512];
};

#endif    // #if (_MSC_VER >= 1400)

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_CORE_WIN_H

