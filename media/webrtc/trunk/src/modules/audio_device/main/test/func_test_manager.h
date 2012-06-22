/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_FUNC_TEST_MANAGER_H
#define WEBRTC_AUDIO_DEVICE_FUNC_TEST_MANAGER_H

#include "../source/audio_device_utility.h"

#include <string>

#include "typedefs.h"
#include "audio_device.h"
#include "audio_device_test_defines.h"
#include "file_wrapper.h"
#include "list_wrapper.h"
#include "resampler.h"

#if defined(MAC_IPHONE) || defined(ANDROID)
#define USE_SLEEP_AS_PAUSE
#else
//#define USE_SLEEP_AS_PAUSE
#endif

// Sets the default pause time if using sleep as pause
#define DEFAULT_PAUSE_TIME 5000

#if defined(USE_SLEEP_AS_PAUSE)
#define PAUSE(a) AudioDeviceUtility::Sleep(a);
#else
#define PAUSE(a) AudioDeviceUtility::WaitForKey();
#endif

#define SLEEP(a) AudioDeviceUtility::Sleep(a);

#define ADM_AUDIO_LAYER AudioDeviceModule::kPlatformDefaultAudio
//#define ADM_AUDIO_LAYER AudioDeviceModule::kLinuxPulseAudio

enum TestType
{
    TTInvalid = -1,
    TTAll = 0,
    TTAudioLayerSelection = 1,
    TTDeviceEnumeration = 2,
    TTDeviceSelection = 3,
    TTAudioTransport = 4,
    TTSpeakerVolume = 5,
    TTMicrophoneVolume = 6,
    TTSpeakerMute = 7,
    TTMicrophoneMute = 8,
    TTMicrophoneBoost = 9,
    TTMicrophoneAGC = 10,
    TTLoopback = 11,
    TTDeviceRemoval = 13,
    TTMobileAPI = 14,
    TTTest = 66,
};

class ProcessThread;

namespace webrtc
{

class AudioDeviceModule;
class AudioEventObserver;
class AudioTransport;

// ----------------------------------------------------------------------------
//  AudioEventObserver
// ----------------------------------------------------------------------------

class AudioEventObserver: public AudioDeviceObserver
{
public:
    virtual void OnErrorIsReported(const ErrorCode error);
    virtual void OnWarningIsReported(const WarningCode warning);
    AudioEventObserver(AudioDeviceModule* audioDevice);
    ~AudioEventObserver();
public:
    ErrorCode _error;
    WarningCode _warning;
private:
    AudioDeviceModule* _audioDevice;
};

// ----------------------------------------------------------------------------
//  AudioTransport
// ----------------------------------------------------------------------------

class AudioTransportImpl: public AudioTransport
{
public:
    virtual WebRtc_Word32
        RecordedDataIsAvailable(const void* audioSamples,
                                const WebRtc_UWord32 nSamples,
                                const WebRtc_UWord8 nBytesPerSample,
                                const WebRtc_UWord8 nChannels,
                                const WebRtc_UWord32 samplesPerSec,
                                const WebRtc_UWord32 totalDelayMS,
                                const WebRtc_Word32 clockDrift,
                                const WebRtc_UWord32 currentMicLevel,
                                WebRtc_UWord32& newMicLevel);

    virtual WebRtc_Word32 NeedMorePlayData(const WebRtc_UWord32 nSamples,
                                           const WebRtc_UWord8 nBytesPerSample,
                                           const WebRtc_UWord8 nChannels,
                                           const WebRtc_UWord32 samplesPerSec,
                                           void* audioSamples,
                                           WebRtc_UWord32& nSamplesOut);

    AudioTransportImpl(AudioDeviceModule* audioDevice);
    ~AudioTransportImpl();

public:
    WebRtc_Word32 SetFilePlayout(bool enable, const char* fileName = NULL);
    void SetFullDuplex(bool enable);
    void SetSpeakerVolume(bool enable)
    {
        _speakerVolume = enable;
    }
    ;
    void SetSpeakerMute(bool enable)
    {
        _speakerMute = enable;
    }
    ;
    void SetMicrophoneMute(bool enable)
    {
        _microphoneMute = enable;
    }
    ;
    void SetMicrophoneVolume(bool enable)
    {
        _microphoneVolume = enable;
    }
    ;
    void SetMicrophoneBoost(bool enable)
    {
        _microphoneBoost = enable;
    }
    ;
    void SetLoopbackMeasurements(bool enable)
    {
        _loopBackMeasurements = enable;
    }
    ;
    void SetMicrophoneAGC(bool enable)
    {
        _microphoneAGC = enable;
    }
    ;

private:
    AudioDeviceModule* _audioDevice;

    bool _playFromFile;
    bool _fullDuplex;
    bool _speakerVolume;
    bool _speakerMute;
    bool _microphoneVolume;
    bool _microphoneMute;
    bool _microphoneBoost;
    bool _microphoneAGC;
    bool _loopBackMeasurements;

    FileWrapper& _playFile;

    WebRtc_UWord32 _recCount;
    WebRtc_UWord32 _playCount;

    ListWrapper _audioList;

    Resampler _resampler;
};

// ----------------------------------------------------------------------------
//  FuncTestManager
// ----------------------------------------------------------------------------

class FuncTestManager
{
public:
    FuncTestManager();
    ~FuncTestManager();
    WebRtc_Word32 Init();
    WebRtc_Word32 Close();
    WebRtc_Word32 DoTest(const TestType testType);
private:
    WebRtc_Word32 TestAudioLayerSelection();
    WebRtc_Word32 TestDeviceEnumeration();
    WebRtc_Word32 TestDeviceSelection();
    WebRtc_Word32 TestAudioTransport();
    WebRtc_Word32 TestSpeakerVolume();
    WebRtc_Word32 TestMicrophoneVolume();
    WebRtc_Word32 TestSpeakerMute();
    WebRtc_Word32 TestMicrophoneMute();
    WebRtc_Word32 TestMicrophoneBoost();
    WebRtc_Word32 TestLoopback();
    WebRtc_Word32 TestDeviceRemoval();
    WebRtc_Word32 TestExtra();
    WebRtc_Word32 TestMicrophoneAGC();
    WebRtc_Word32 SelectPlayoutDevice();
    WebRtc_Word32 SelectRecordingDevice();
    WebRtc_Word32 TestAdvancedMBAPI();
private:
    // Paths to where the resource files to be used for this test are located.
    std::string _resourcePath;
    std::string _playoutFile48;
    std::string _playoutFile44;
    std::string _playoutFile16;
    std::string _playoutFile8;

    ProcessThread* _processThread;
    AudioDeviceModule* _audioDevice;
    AudioEventObserver* _audioEventObserver;
    AudioTransportImpl* _audioTransport;
};

} // namespace webrtc

#endif  // #ifndef WEBRTC_AUDIO_DEVICE_FUNC_TEST_MANAGER_H
