/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_device_impl.h"
#include "audio_device_config.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "system_wrappers/interface/ref_count.h"

#include <assert.h>
#include <string.h>

#if defined(_WIN32)
    #include "audio_device_utility_win.h"
    #include "audio_device_wave_win.h"
 #if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    #include "audio_device_core_win.h"
 #endif
#elif defined(WEBRTC_ANDROID_OPENSLES)
    #include <stdlib.h>
    #include "audio_device_utility_android.h"
    #include "audio_device_opensles_android.h"
#elif defined(WEBRTC_ANDROID)
    #include <stdlib.h>
    #include "audio_device_utility_android.h"
    #include "audio_device_jni_android.h"
#elif defined(WEBRTC_LINUX)
    #include "audio_device_utility_linux.h"
 #if defined(LINUX_ALSA)
    #include "audio_device_alsa_linux.h"
 #endif
 #if defined(LINUX_PULSE)
    #include "audio_device_pulse_linux.h"
 #endif
#elif defined(WEBRTC_IOS)
    #include "audio_device_utility_ios.h"
    #include "audio_device_ios.h"
#elif defined(WEBRTC_MAC)
    #include "audio_device_utility_mac.h"
    #include "audio_device_mac.h"
#endif
#include "audio_device_dummy.h"
#include "audio_device_utility_dummy.h"
#include "critical_section_wrapper.h"
#include "trace.h"

#define CHECK_INITIALIZED()         \
{                                   \
    if (!_initialized) {            \
        return -1;                  \
    };                              \
}

#define CHECK_INITIALIZED_BOOL()    \
{                                   \
    if (!_initialized) {            \
        return false;               \
    };                              \
}

namespace webrtc
{

AudioDeviceModule* CreateAudioDeviceModule(
    WebRtc_Word32 id, AudioDeviceModule::AudioLayer audioLayer) {
  return AudioDeviceModuleImpl::Create(id, audioLayer);
}


// ============================================================================
//                                   Static methods
// ============================================================================

// ----------------------------------------------------------------------------
//  AudioDeviceModule::Create()
// ----------------------------------------------------------------------------

AudioDeviceModule* AudioDeviceModuleImpl::Create(const WebRtc_Word32 id,
                                                 const AudioLayer audioLayer)
{

    // Create the generic ref counted (platform independent) implementation.
    RefCountImpl<AudioDeviceModuleImpl>* audioDevice =
        new RefCountImpl<AudioDeviceModuleImpl>(id, audioLayer);

    // Ensure that the current platform is supported.
    if (audioDevice->CheckPlatform() == -1)
    {
        delete audioDevice;
        return NULL;
    }

    // Create the platform-dependent implementation.
    if (audioDevice->CreatePlatformSpecificObjects() == -1)
    {
        delete audioDevice;
        return NULL;
    }

    // Ensure that the generic audio buffer can communicate with the
    // platform-specific parts.
    if (audioDevice->AttachAudioBuffer() == -1)
    {
        delete audioDevice;
        return NULL;
    }

    WebRtcSpl_Init();

    return audioDevice;
}

// ============================================================================
//                            Construction & Destruction
// ============================================================================

// ----------------------------------------------------------------------------
//  AudioDeviceModuleImpl - ctor
// ----------------------------------------------------------------------------

AudioDeviceModuleImpl::AudioDeviceModuleImpl(const WebRtc_Word32 id, const AudioLayer audioLayer) :
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _critSectEventCb(*CriticalSectionWrapper::CreateCriticalSection()),
    _critSectAudioCb(*CriticalSectionWrapper::CreateCriticalSection()),
    _ptrCbAudioDeviceObserver(NULL),
    _ptrAudioDeviceUtility(NULL),
    _ptrAudioDevice(NULL),
    _id(id),
    _platformAudioLayer(audioLayer),
    _lastProcessTime(AudioDeviceUtility::GetTimeInMS()),
    _platformType(kPlatformNotSupported),
    _initialized(false),
    _lastError(kAdmErrNone)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id, "%s created", __FUNCTION__);
}

// ----------------------------------------------------------------------------
//  CheckPlatform
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::CheckPlatform()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    // Ensure that the current platform is supported
    //
    PlatformType platform(kPlatformNotSupported);

#if defined(_WIN32)
    platform = kPlatformWin32;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "current platform is WIN32");
#elif defined(WEBRTC_ANDROID)
    platform = kPlatformAndroid;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "current platform is ANDROID");
#elif defined(WEBRTC_LINUX)
    platform = kPlatformLinux;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "current platform is LINUX");
#elif defined(WEBRTC_IOS)
    platform = kPlatformIOS;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "current platform is IOS");
#elif defined(WEBRTC_MAC)
    platform = kPlatformMac;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "current platform is MAC");
#endif

    if (platform == kPlatformNotSupported)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id, "current platform is not supported => this module will self destruct!");
        return -1;
    }

    // Store valid output results
    //
    _platformType = platform;

    return 0;
}


// ----------------------------------------------------------------------------
//  CreatePlatformSpecificObjects
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::CreatePlatformSpecificObjects()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    AudioDeviceGeneric* ptrAudioDevice(NULL);
    AudioDeviceUtility* ptrAudioDeviceUtility(NULL);

#if defined(WEBRTC_DUMMY_AUDIO_BUILD)
    ptrAudioDevice = new AudioDeviceDummy(Id());
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Dummy Audio APIs will be utilized");

    if (ptrAudioDevice != NULL)
    {
        ptrAudioDeviceUtility = new AudioDeviceUtilityDummy(Id());
    }
#else
    const AudioLayer audioLayer(PlatformAudioLayer());

    // Create the *Windows* implementation of the Audio Device
    //
#if defined(_WIN32)
    if ((audioLayer == kWindowsWaveAudio)
#if !defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
        // Wave audio is default if Core audio is not supported in this build
        || (audioLayer == kPlatformDefaultAudio)
#endif
        )
    {
        // create *Windows Wave Audio* implementation
        ptrAudioDevice = new AudioDeviceWindowsWave(Id());
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Windows Wave APIs will be utilized");
    }
#if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    if ((audioLayer == kWindowsCoreAudio) ||
        (audioLayer == kPlatformDefaultAudio)
        )
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "attempting to use the Windows Core Audio APIs...");

        if (AudioDeviceWindowsCore::CoreAudioIsSupported())
        {
            // create *Windows Core Audio* implementation
            ptrAudioDevice = new AudioDeviceWindowsCore(Id());
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Windows Core Audio APIs will be utilized");
        }
        else
        {
            // create *Windows Wave Audio* implementation
            ptrAudioDevice = new AudioDeviceWindowsWave(Id());
            if (ptrAudioDevice != NULL)
            {
                // Core Audio was not supported => revert to Windows Wave instead
                _platformAudioLayer = kWindowsWaveAudio;  // modify the state set at construction
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "Windows Core Audio is *not* supported => Wave APIs will be utilized instead");
            }
        }
    }
#endif // defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    if (ptrAudioDevice != NULL)
    {
        // Create the Windows implementation of the Device Utility.
        // This class is independent of the selected audio layer
        // for Windows.
        //
        ptrAudioDeviceUtility = new AudioDeviceUtilityWindows(Id());
    }
#endif  // #if defined(_WIN32)

    // Create the *Android OpenSLES* implementation of the Audio Device
    //
#if defined(WEBRTC_ANDROID_OPENSLES)
    if (audioLayer == kPlatformDefaultAudio)
    {
        // Create *Android OpenELSE Audio* implementation
        ptrAudioDevice = new AudioDeviceAndroidOpenSLES(Id());
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "Android OpenSLES Audio APIs will be utilized");
    }

    if (ptrAudioDevice != NULL)
    {
        // Create the Android implementation of the Device Utility.
        ptrAudioDeviceUtility = new AudioDeviceUtilityAndroid(Id());
    }
    // END #if defined(WEBRTC_ANDROID_OPENSLES)

    // Create the *Android Java* implementation of the Audio Device
    //
#elif defined(WEBRTC_ANDROID)
    if (audioLayer == kPlatformDefaultAudio)
    {
        // Create *Android JNI Audio* implementation
        ptrAudioDevice = new AudioDeviceAndroidJni(Id());
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Android JNI Audio APIs will be utilized");
    }

    if (ptrAudioDevice != NULL)
    {
        // Create the Android implementation of the Device Utility.
        ptrAudioDeviceUtility = new AudioDeviceUtilityAndroid(Id());
    }
    // END #if defined(WEBRTC_ANDROID)

    // Create the *Linux* implementation of the Audio Device
    //
#elif defined(WEBRTC_LINUX)
    if ((audioLayer == kLinuxPulseAudio) || (audioLayer == kPlatformDefaultAudio))
    {
#if defined(LINUX_PULSE)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "attempting to use the Linux PulseAudio APIs...");

        if (AudioDeviceLinuxPulse::PulseAudioIsSupported())
        {
            // create *Linux PulseAudio* implementation
            ptrAudioDevice = new AudioDeviceLinuxPulse(Id());
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Linux PulseAudio APIs will be utilized");
        }
        else
        {
#endif
#if defined(LINUX_ALSA)
            // create *Linux ALSA Audio* implementation
            ptrAudioDevice = new AudioDeviceLinuxALSA(Id());
            if (ptrAudioDevice != NULL)
            {
                // Pulse Audio was not supported => revert to ALSA instead
                _platformAudioLayer = kLinuxAlsaAudio;  // modify the state set at construction
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "Linux PulseAudio is *not* supported => ALSA APIs will be utilized instead");
            }
#endif
#if defined(LINUX_PULSE)
        }
#endif
    }
    else if (audioLayer == kLinuxAlsaAudio)
    {
#if defined(LINUX_ALSA)
        // create *Linux ALSA Audio* implementation
        ptrAudioDevice = new AudioDeviceLinuxALSA(Id());
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Linux ALSA APIs will be utilized");
#endif
    }

    if (ptrAudioDevice != NULL)
    {
        // Create the Linux implementation of the Device Utility.
        // This class is independent of the selected audio layer
        // for Linux.
        //
        ptrAudioDeviceUtility = new AudioDeviceUtilityLinux(Id());
    }
#endif  // #if defined(WEBRTC_LINUX)

    // Create the *iPhone* implementation of the Audio Device
    //
#if defined(WEBRTC_IOS)
    if (audioLayer == kPlatformDefaultAudio)
    {
        // Create *iPhone Audio* implementation
        ptrAudioDevice = new AudioDeviceIPhone(Id());
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "iPhone Audio APIs will be utilized");
    }

    if (ptrAudioDevice != NULL)
    {
        // Create the Mac implementation of the Device Utility.
        ptrAudioDeviceUtility = new AudioDeviceUtilityIPhone(Id());
    }
    // END #if defined(WEBRTC_IOS)

    // Create the *Mac* implementation of the Audio Device
    //
#elif defined(WEBRTC_MAC)
    if (audioLayer == kPlatformDefaultAudio)
    {
        // Create *Mac Audio* implementation
        ptrAudioDevice = new AudioDeviceMac(Id());
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Mac OS X Audio APIs will be utilized");
    }

    if (ptrAudioDevice != NULL)
    {
        // Create the Mac implementation of the Device Utility.
        ptrAudioDeviceUtility = new AudioDeviceUtilityMac(Id());
    }
#endif  // WEBRTC_MAC

    // Create the *Dummy* implementation of the Audio Device
    // Available for all platforms
    //
    if (audioLayer == kDummyAudio)
    {
        // Create *Dummy Audio* implementation
        assert(!ptrAudioDevice);
        ptrAudioDevice = new AudioDeviceDummy(Id());
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Dummy Audio APIs will be utilized");

        if (ptrAudioDevice != NULL)
        {
            ptrAudioDeviceUtility = new AudioDeviceUtilityDummy(Id());
        }
    }
#endif  // if defined(WEBRTC_DUMMY_AUDIO_BUILD)

    if (ptrAudioDevice == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id, "unable to create the platform specific audio device implementation");
        return -1;
    }

    if (ptrAudioDeviceUtility == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id, "unable to create the platform specific audio device utility");
        return -1;
    }

    // Store valid output pointers
    //
    _ptrAudioDevice = ptrAudioDevice;
    _ptrAudioDeviceUtility = ptrAudioDeviceUtility;

    return 0;
}

// ----------------------------------------------------------------------------
//  AttachAudioBuffer
//
//  Install "bridge" between the platform implemetation and the generic
//  implementation. The "child" shall set the native sampling rate and the
//  number of channels in this function call.
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::AttachAudioBuffer()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    _audioDeviceBuffer.SetId(_id);
    _ptrAudioDevice->AttachAudioBuffer(&_audioDeviceBuffer);
    return 0;
}

// ----------------------------------------------------------------------------
//  ~AudioDeviceModuleImpl - dtor
// ----------------------------------------------------------------------------

AudioDeviceModuleImpl::~AudioDeviceModuleImpl()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s destroyed", __FUNCTION__);

    if (_ptrAudioDevice)
    {
        delete _ptrAudioDevice;
        _ptrAudioDevice = NULL;
    }

    if (_ptrAudioDeviceUtility)
    {
        delete _ptrAudioDeviceUtility;
        _ptrAudioDeviceUtility = NULL;
    }

    delete &_critSect;
    delete &_critSectEventCb;
    delete &_critSectAudioCb;
}

// ============================================================================
//                                  Module
// ============================================================================

// ----------------------------------------------------------------------------
//  Module::ChangeUniqueId
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
    return 0;
}

// ----------------------------------------------------------------------------
//  Module::TimeUntilNextProcess
//
//  Returns the number of milliseconds until the module want a worker thread
//  to call Process().
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::TimeUntilNextProcess()
{
    WebRtc_UWord32 now = AudioDeviceUtility::GetTimeInMS();
    WebRtc_Word32 deltaProcess = kAdmMaxIdleTimeProcess - (now - _lastProcessTime);
    return (deltaProcess);
}

// ----------------------------------------------------------------------------
//  Module::Process
//
//  Check for posted error and warning reports. Generate callbacks if
//  new reports exists.
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::Process()
{

    _lastProcessTime = AudioDeviceUtility::GetTimeInMS();

    // kPlayoutWarning
    if (_ptrAudioDevice->PlayoutWarning())
    {
        CriticalSectionScoped lock(&_critSectEventCb);
        if (_ptrCbAudioDeviceObserver)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "=> OnWarningIsReported(kPlayoutWarning)");
            _ptrCbAudioDeviceObserver->OnWarningIsReported(AudioDeviceObserver::kPlayoutWarning);
        }
        _ptrAudioDevice->ClearPlayoutWarning();
    }

    // kPlayoutError
    if (_ptrAudioDevice->PlayoutError())
    {
        CriticalSectionScoped lock(&_critSectEventCb);
        if (_ptrCbAudioDeviceObserver)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "=> OnErrorIsReported(kPlayoutError)");
            _ptrCbAudioDeviceObserver->OnErrorIsReported(AudioDeviceObserver::kPlayoutError);
        }
        _ptrAudioDevice->ClearPlayoutError();
    }

    // kRecordingWarning
    if (_ptrAudioDevice->RecordingWarning())
    {
        CriticalSectionScoped lock(&_critSectEventCb);
        if (_ptrCbAudioDeviceObserver)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "=> OnWarningIsReported(kRecordingWarning)");
            _ptrCbAudioDeviceObserver->OnWarningIsReported(AudioDeviceObserver::kRecordingWarning);
        }
        _ptrAudioDevice->ClearRecordingWarning();
    }

    // kRecordingError
    if (_ptrAudioDevice->RecordingError())
    {
        CriticalSectionScoped lock(&_critSectEventCb);
        if (_ptrCbAudioDeviceObserver)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "=> OnErrorIsReported(kRecordingError)");
            _ptrCbAudioDeviceObserver->OnErrorIsReported(AudioDeviceObserver::kRecordingError);
        }
        _ptrAudioDevice->ClearRecordingError();
    }

    return 0;
}

// ============================================================================
//                                    Public API
// ============================================================================

// ----------------------------------------------------------------------------
//  ActiveAudioLayer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::ActiveAudioLayer(AudioLayer* audioLayer) const
{

    AudioLayer activeAudio;

    if (_ptrAudioDevice->ActiveAudioLayer(activeAudio) == -1)
    {
        return -1;
    }

    *audioLayer = activeAudio;

    if (*audioLayer == AudioDeviceModule::kWindowsWaveAudio)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: kWindowsWaveAudio");
    }
    else if (*audioLayer == AudioDeviceModule::kWindowsCoreAudio)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: kWindowsCoreAudio");
    }
    else if (*audioLayer == AudioDeviceModule::kLinuxAlsaAudio)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: kLinuxAlsaAudio");
    }
    else
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: NOT_SUPPORTED");
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  LastError
// ----------------------------------------------------------------------------

AudioDeviceModule::ErrorCode AudioDeviceModuleImpl::LastError() const
{
    return _lastError;
}

// ----------------------------------------------------------------------------
//  Init
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::Init()
{

    if (_initialized)
        return 0;

    if (!_ptrAudioDeviceUtility)
        return -1;

    if (!_ptrAudioDevice)
        return -1;

    _ptrAudioDeviceUtility->Init();

    if (_ptrAudioDevice->Init() == -1)
    {
        return -1;
    }

    _initialized = true;
    return 0;
}

// ----------------------------------------------------------------------------
//  Terminate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::Terminate()
{

    if (!_initialized)
        return 0;

    if (_ptrAudioDevice->Terminate() == -1)
    {
        return -1;
    }

    _initialized = false;
    return 0;
}

// ----------------------------------------------------------------------------
//  Initialized
// ----------------------------------------------------------------------------

bool AudioDeviceModuleImpl::Initialized() const
{

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: %d", _initialized);
    return (_initialized);
}

// ----------------------------------------------------------------------------
//  SpeakerIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SpeakerIsAvailable(bool* available)
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->SpeakerIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", available);
    return (0);
}

// ----------------------------------------------------------------------------
//  InitSpeaker
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::InitSpeaker()
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->InitSpeaker());
}

// ----------------------------------------------------------------------------
//  MicrophoneIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MicrophoneIsAvailable(bool* available)
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->MicrophoneIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", *available);
    return (0);
}

// ----------------------------------------------------------------------------
//  InitMicrophone
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::InitMicrophone()
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->InitMicrophone());
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SpeakerVolumeIsAvailable(bool* available)
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->SpeakerVolumeIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", *available);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetSpeakerVolume(WebRtc_UWord32 volume)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetSpeakerVolume(volume));
}

// ----------------------------------------------------------------------------
//  SpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SpeakerVolume(WebRtc_UWord32* volume) const
{
    CHECK_INITIALIZED();

    WebRtc_UWord32 level(0);

    if (_ptrAudioDevice->SpeakerVolume(level) == -1)
    {
        return -1;
    }

    *volume = level;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: volume=%u", *volume);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetWaveOutVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetWaveOutVolume(WebRtc_UWord16 volumeLeft, WebRtc_UWord16 volumeRight)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetWaveOutVolume(volumeLeft, volumeRight));
}

// ----------------------------------------------------------------------------
//  WaveOutVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::WaveOutVolume(WebRtc_UWord16* volumeLeft, WebRtc_UWord16* volumeRight) const
{
    CHECK_INITIALIZED();

    WebRtc_UWord16 volLeft(0);
    WebRtc_UWord16 volRight(0);

    if (_ptrAudioDevice->WaveOutVolume(volLeft, volRight) == -1)
    {
        return -1;
    }

    *volumeLeft = volLeft;
    *volumeRight = volRight;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "outputs: volumeLeft=%u, volumeRight=%u",
        *volumeLeft, *volumeRight);

    return (0);
}

// ----------------------------------------------------------------------------
//  SpeakerIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceModuleImpl::SpeakerIsInitialized() const
{
    CHECK_INITIALIZED_BOOL();

    bool isInitialized = _ptrAudioDevice->SpeakerIsInitialized();

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: %d", isInitialized);
    return (isInitialized);
}

// ----------------------------------------------------------------------------
//  MicrophoneIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceModuleImpl::MicrophoneIsInitialized() const
{
    CHECK_INITIALIZED_BOOL();

    bool isInitialized = _ptrAudioDevice->MicrophoneIsInitialized();

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: %d", isInitialized);
    return (isInitialized);
}

// ----------------------------------------------------------------------------
//  MaxSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MaxSpeakerVolume(WebRtc_UWord32* maxVolume) const
{
    CHECK_INITIALIZED();

    WebRtc_UWord32 maxVol(0);

    if (_ptrAudioDevice->MaxSpeakerVolume(maxVol) == -1)
    {
        return -1;
    }

    *maxVolume = maxVol;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: maxVolume=%d", *maxVolume);
    return (0);
}

// ----------------------------------------------------------------------------
//  MinSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MinSpeakerVolume(WebRtc_UWord32* minVolume) const
{
    CHECK_INITIALIZED();

    WebRtc_UWord32 minVol(0);

    if (_ptrAudioDevice->MinSpeakerVolume(minVol) == -1)
    {
        return -1;
    }

    *minVolume = minVol;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: minVolume=%u", *minVolume);
    return (0);
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeStepSize
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SpeakerVolumeStepSize(WebRtc_UWord16* stepSize) const
{
    CHECK_INITIALIZED();

    WebRtc_UWord16 delta(0);

    if (_ptrAudioDevice->SpeakerVolumeStepSize(delta) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to retrieve the speaker-volume step size");
        return -1;
    }

    *stepSize = delta;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: stepSize=%u", *stepSize);
    return (0);
}

// ----------------------------------------------------------------------------
//  SpeakerMuteIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SpeakerMuteIsAvailable(bool* available)
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->SpeakerMuteIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", *available);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetSpeakerMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetSpeakerMute(bool enable)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetSpeakerMute(enable));
}

// ----------------------------------------------------------------------------
//  SpeakerMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SpeakerMute(bool* enabled) const
{
    CHECK_INITIALIZED();

    bool muted(false);

    if (_ptrAudioDevice->SpeakerMute(muted) == -1)
    {
        return -1;
    }

    *enabled = muted;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: enabled=%u", *enabled);
    return (0);
}

// ----------------------------------------------------------------------------
//  MicrophoneMuteIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MicrophoneMuteIsAvailable(bool* available)
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->MicrophoneMuteIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", *available);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetMicrophoneMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetMicrophoneMute(bool enable)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetMicrophoneMute(enable));
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MicrophoneMute(bool* enabled) const
{
    CHECK_INITIALIZED();

    bool muted(false);

    if (_ptrAudioDevice->MicrophoneMute(muted) == -1)
    {
        return -1;
    }

    *enabled = muted;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: enabled=%u", *enabled);
    return (0);
}

// ----------------------------------------------------------------------------
//  MicrophoneBoostIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MicrophoneBoostIsAvailable(bool* available)
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->MicrophoneBoostIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", *available);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetMicrophoneBoost
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetMicrophoneBoost(bool enable)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetMicrophoneBoost(enable));
}

// ----------------------------------------------------------------------------
//  MicrophoneBoost
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MicrophoneBoost(bool* enabled) const
{
    CHECK_INITIALIZED();

    bool onOff(false);

    if (_ptrAudioDevice->MicrophoneBoost(onOff) == -1)
    {
        return -1;
    }

    *enabled = onOff;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: enabled=%u", *enabled);
    return (0);
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MicrophoneVolumeIsAvailable(bool* available)
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->MicrophoneVolumeIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", *available);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetMicrophoneVolume(WebRtc_UWord32 volume)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetMicrophoneVolume(volume));
}

// ----------------------------------------------------------------------------
//  MicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MicrophoneVolume(WebRtc_UWord32* volume) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    CHECK_INITIALIZED();

    WebRtc_UWord32 level(0);

    if (_ptrAudioDevice->MicrophoneVolume(level) == -1)
    {
        return -1;
    }

    *volume = level;

    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "output: volume=%u", *volume);
    return (0);
}

// ----------------------------------------------------------------------------
//  StereoRecordingIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StereoRecordingIsAvailable(bool* available) const
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->StereoRecordingIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", *available);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetStereoRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetStereoRecording(bool enable)
{
    CHECK_INITIALIZED();

    if (_ptrAudioDevice->RecordingIsInitialized())
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "recording in stereo is not supported");
        return -1;
    }

    if (_ptrAudioDevice->SetStereoRecording(enable) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to enable stereo recording");
        return -1;
    }

    WebRtc_Word8 nChannels(1);
    if (enable)
    {
        nChannels = 2;
    }
    _audioDeviceBuffer.SetRecordingChannels(nChannels);

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StereoRecording(bool* enabled) const
{
    CHECK_INITIALIZED();

    bool stereo(false);

    if (_ptrAudioDevice->StereoRecording(stereo) == -1)
    {
        return -1;
    }

    *enabled = stereo;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: enabled=%u", *enabled);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetRecordingChannel
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetRecordingChannel(const ChannelType channel)
{
    if (channel == kChannelBoth)
    {
    }
    else if (channel == kChannelLeft)
    {
    }
    else
    {
    }
    CHECK_INITIALIZED();

    bool stereo(false);

    if (_ptrAudioDevice->StereoRecording(stereo) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "recording in stereo is not supported");
        return -1;
    }

    return (_audioDeviceBuffer.SetRecordingChannel(channel));
}

// ----------------------------------------------------------------------------
//  RecordingChannel
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::RecordingChannel(ChannelType* channel) const
{
    CHECK_INITIALIZED();

    ChannelType chType;

    if (_audioDeviceBuffer.RecordingChannel(chType) == -1)
    {
        return -1;
    }

    *channel = chType;

    if (*channel == kChannelBoth)
    {
    }
    else if (*channel == kChannelLeft)
    {
    }
    else
    {
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  StereoPlayoutIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StereoPlayoutIsAvailable(bool* available) const
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->StereoPlayoutIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", *available);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetStereoPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetStereoPlayout(bool enable)
{
    CHECK_INITIALIZED();

    if (_ptrAudioDevice->PlayoutIsInitialized())
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "unable to set stereo mode while playing side is initialized");
        return -1;
    }

    if (_ptrAudioDevice->SetStereoPlayout(enable))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "stereo playout is not supported");
        return -1;
    }

    WebRtc_Word8 nChannels(1);
    if (enable)
    {
        nChannels = 2;
    }
    _audioDeviceBuffer.SetPlayoutChannels(nChannels);

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StereoPlayout(bool* enabled) const
{
    CHECK_INITIALIZED();

    bool stereo(false);

    if (_ptrAudioDevice->StereoPlayout(stereo) == -1)
    {
        return -1;
    }

   *enabled = stereo;

   WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: enabled=%u", *enabled);
   return (0);
}

// ----------------------------------------------------------------------------
//  SetAGC
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetAGC(bool enable)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetAGC(enable));
}

// ----------------------------------------------------------------------------
//  AGC
// ----------------------------------------------------------------------------

bool AudioDeviceModuleImpl::AGC() const
{
    CHECK_INITIALIZED_BOOL();
    return (_ptrAudioDevice->AGC());
}

// ----------------------------------------------------------------------------
//  PlayoutIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::PlayoutIsAvailable(bool* available)
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->PlayoutIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", *available);
    return (0);
}

// ----------------------------------------------------------------------------
//  RecordingIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::RecordingIsAvailable(bool* available)
{
    CHECK_INITIALIZED();

    bool isAvailable(0);

    if (_ptrAudioDevice->RecordingIsAvailable(isAvailable) == -1)
    {
        return -1;
    }

    *available = isAvailable;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: available=%d", *available);
    return (0);
}

// ----------------------------------------------------------------------------
//  MaxMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MaxMicrophoneVolume(WebRtc_UWord32* maxVolume) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    CHECK_INITIALIZED();

    WebRtc_UWord32 maxVol(0);

    if (_ptrAudioDevice->MaxMicrophoneVolume(maxVol) == -1)
    {
        return -1;
    }

    *maxVolume = maxVol;

    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "output: maxVolume=%d", *maxVolume);
    return (0);
}

// ----------------------------------------------------------------------------
//  MinMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MinMicrophoneVolume(WebRtc_UWord32* minVolume) const
{
    CHECK_INITIALIZED();

    WebRtc_UWord32 minVol(0);

    if (_ptrAudioDevice->MinMicrophoneVolume(minVol) == -1)
    {
        return -1;
    }

    *minVolume = minVol;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: minVolume=%u", *minVolume);
    return (0);
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeStepSize
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::MicrophoneVolumeStepSize(WebRtc_UWord16* stepSize) const
{
    CHECK_INITIALIZED();

    WebRtc_UWord16 delta(0);

    if (_ptrAudioDevice->MicrophoneVolumeStepSize(delta) == -1)
    {
        return -1;
    }

    *stepSize = delta;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: stepSize=%u", *stepSize);
    return (0);
}

// ----------------------------------------------------------------------------
//  PlayoutDevices
// ----------------------------------------------------------------------------

WebRtc_Word16 AudioDeviceModuleImpl::PlayoutDevices()
{
    CHECK_INITIALIZED();

    WebRtc_UWord16 nPlayoutDevices = _ptrAudioDevice->PlayoutDevices();

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: #playout devices=%d", nPlayoutDevices);
    return ((WebRtc_Word16)(nPlayoutDevices));
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice I (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetPlayoutDevice(WebRtc_UWord16 index)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetPlayoutDevice(index));
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice II (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetPlayoutDevice(WindowsDeviceType device)
{
    if (device == kDefaultDevice)
    {
    }
    else
    {
    }
    CHECK_INITIALIZED();

    return (_ptrAudioDevice->SetPlayoutDevice(device));
}

// ----------------------------------------------------------------------------
//  PlayoutDeviceName
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::PlayoutDeviceName(
    WebRtc_UWord16 index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{
    CHECK_INITIALIZED();

    if (name == NULL)
    {
        _lastError = kAdmErrArgument;
        return -1;
    }

    if (_ptrAudioDevice->PlayoutDeviceName(index, name, guid) == -1)
    {
        return -1;
    }

    if (name != NULL)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: name=%s", name);
    }
    if (guid != NULL)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: guid=%s", guid);
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  RecordingDeviceName
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::RecordingDeviceName(
    WebRtc_UWord16 index,
    char name[kAdmMaxDeviceNameSize],
    char guid[kAdmMaxGuidSize])
{
    CHECK_INITIALIZED();

    if (name == NULL)
    {
        _lastError = kAdmErrArgument;
        return -1;
    }

    if (_ptrAudioDevice->RecordingDeviceName(index, name, guid) == -1)
    {
        return -1;
    }

    if (name != NULL)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: name=%s", name);
    }
    if (guid != NULL)
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: guid=%s", guid);
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  RecordingDevices
// ----------------------------------------------------------------------------

WebRtc_Word16 AudioDeviceModuleImpl::RecordingDevices()
{
    CHECK_INITIALIZED();

    WebRtc_UWord16 nRecordingDevices = _ptrAudioDevice->RecordingDevices();

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                 "output: #recording devices=%d", nRecordingDevices);
    return ((WebRtc_Word16)nRecordingDevices);
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice I (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetRecordingDevice(WebRtc_UWord16 index)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetRecordingDevice(index));
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetRecordingDevice(WindowsDeviceType device)
{
    if (device == kDefaultDevice)
    {
    }
    else
    {
    }
    CHECK_INITIALIZED();

    return (_ptrAudioDevice->SetRecordingDevice(device));
}

// ----------------------------------------------------------------------------
//  InitPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::InitPlayout()
{
    CHECK_INITIALIZED();
    _audioDeviceBuffer.InitPlayout();
    return (_ptrAudioDevice->InitPlayout());
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::InitRecording()
{
    CHECK_INITIALIZED();
    _audioDeviceBuffer.InitRecording();
    return (_ptrAudioDevice->InitRecording());
}

// ----------------------------------------------------------------------------
//  PlayoutIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceModuleImpl::PlayoutIsInitialized() const
{
    CHECK_INITIALIZED_BOOL();
    return (_ptrAudioDevice->PlayoutIsInitialized());
}

// ----------------------------------------------------------------------------
//  RecordingIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceModuleImpl::RecordingIsInitialized() const
{
    CHECK_INITIALIZED_BOOL();
    return (_ptrAudioDevice->RecordingIsInitialized());
}

// ----------------------------------------------------------------------------
//  StartPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StartPlayout()
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->StartPlayout());
}

// ----------------------------------------------------------------------------
//  StopPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StopPlayout()
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->StopPlayout());
}

// ----------------------------------------------------------------------------
//  Playing
// ----------------------------------------------------------------------------

bool AudioDeviceModuleImpl::Playing() const
{
    CHECK_INITIALIZED_BOOL();
    return (_ptrAudioDevice->Playing());
}

// ----------------------------------------------------------------------------
//  StartRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StartRecording()
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->StartRecording());
}
// ----------------------------------------------------------------------------
//  StopRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StopRecording()
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->StopRecording());
}

// ----------------------------------------------------------------------------
//  Recording
// ----------------------------------------------------------------------------

bool AudioDeviceModuleImpl::Recording() const
{
    CHECK_INITIALIZED_BOOL();
    return (_ptrAudioDevice->Recording());
}

// ----------------------------------------------------------------------------
//  RegisterEventObserver
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::RegisterEventObserver(AudioDeviceObserver* eventCallback)
{

    CriticalSectionScoped lock(&_critSectEventCb);
    _ptrCbAudioDeviceObserver = eventCallback;

    return 0;
}

// ----------------------------------------------------------------------------
//  RegisterAudioCallback
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::RegisterAudioCallback(AudioTransport* audioCallback)
{

    CriticalSectionScoped lock(&_critSectAudioCb);
    _audioDeviceBuffer.RegisterAudioCallback(audioCallback);

    return 0;
}

// ----------------------------------------------------------------------------
//  StartRawInputFileRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StartRawInputFileRecording(
    const char pcmFileNameUTF8[kAdmMaxFileNameSize])
{
    CHECK_INITIALIZED();

    if (NULL == pcmFileNameUTF8)
    {
        return -1;
    }

    return (_audioDeviceBuffer.StartInputFileRecording(pcmFileNameUTF8));
}

// ----------------------------------------------------------------------------
//  StopRawInputFileRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StopRawInputFileRecording()
{
    CHECK_INITIALIZED();

    return (_audioDeviceBuffer.StopInputFileRecording());
}

// ----------------------------------------------------------------------------
//  StartRawOutputFileRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StartRawOutputFileRecording(
    const char pcmFileNameUTF8[kAdmMaxFileNameSize])
{
    CHECK_INITIALIZED();

    if (NULL == pcmFileNameUTF8)
    {
        return -1;
    }

    return (_audioDeviceBuffer.StartOutputFileRecording(pcmFileNameUTF8));
}

// ----------------------------------------------------------------------------
//  StopRawOutputFileRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::StopRawOutputFileRecording()
{
    CHECK_INITIALIZED();

    return (_audioDeviceBuffer.StopOutputFileRecording());

    return 0;
}

// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetPlayoutBuffer(const BufferType type, WebRtc_UWord16 sizeMS)
{
    CHECK_INITIALIZED();

    if (_ptrAudioDevice->PlayoutIsInitialized())
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "unable to modify the playout buffer while playing side is initialized");
        return -1;
    }

    WebRtc_Word32 ret(0);

    if (kFixedBufferSize == type)
    {
        if (sizeMS < kAdmMinPlayoutBufferSizeMs || sizeMS > kAdmMaxPlayoutBufferSizeMs)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "size parameter is out of range");
            return -1;
        }
    }

    if ((ret = _ptrAudioDevice->SetPlayoutBuffer(type, sizeMS)) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to set the playout buffer (error: %d)", LastError());
    }

    return ret;
}

// ----------------------------------------------------------------------------
//  PlayoutBuffer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::PlayoutBuffer(BufferType* type, WebRtc_UWord16* sizeMS) const
{
    CHECK_INITIALIZED();

    BufferType bufType;
    WebRtc_UWord16 size(0);

    if (_ptrAudioDevice->PlayoutBuffer(bufType, size) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to retrieve the buffer type and size");
        return -1;
    }

    *type = bufType;
    *sizeMS = size;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: type=%u, sizeMS=%u", *type, *sizeMS);
    return (0);
}

// ----------------------------------------------------------------------------
//  PlayoutDelay
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::PlayoutDelay(WebRtc_UWord16* delayMS) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    CHECK_INITIALIZED();

    WebRtc_UWord16 delay(0);

    if (_ptrAudioDevice->PlayoutDelay(delay) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to retrieve the playout delay");
        return -1;
    }

    *delayMS = delay;

    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "output: delayMS=%u", *delayMS);
    return (0);
}

// ----------------------------------------------------------------------------
//  RecordingDelay
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::RecordingDelay(WebRtc_UWord16* delayMS) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    CHECK_INITIALIZED();

    WebRtc_UWord16 delay(0);

    if (_ptrAudioDevice->RecordingDelay(delay) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to retrieve the recording delay");
        return -1;
    }

    *delayMS = delay;

    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "output: delayMS=%u", *delayMS);
    return (0);
}

// ----------------------------------------------------------------------------
//  CPULoad
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::CPULoad(WebRtc_UWord16* load) const
{
    CHECK_INITIALIZED();

    WebRtc_UWord16 cpuLoad(0);

    if (_ptrAudioDevice->CPULoad(cpuLoad) == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to retrieve the CPU load");
        return -1;
    }

    *load = cpuLoad;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: load=%u", *load);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetRecordingSampleRate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetRecordingSampleRate(const WebRtc_UWord32 samplesPerSec)
{
    CHECK_INITIALIZED();

    if (_ptrAudioDevice->SetRecordingSampleRate(samplesPerSec) != 0)
    {
        return -1;
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  RecordingSampleRate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::RecordingSampleRate(WebRtc_UWord32* samplesPerSec) const
{
    CHECK_INITIALIZED();

    WebRtc_Word32 sampleRate = _audioDeviceBuffer.RecordingSampleRate();

    if (sampleRate == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to retrieve the sample rate");
        return -1;
    }

    *samplesPerSec = sampleRate;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: samplesPerSec=%u", *samplesPerSec);
    return (0);
}

// ----------------------------------------------------------------------------
//  SetPlayoutSampleRate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetPlayoutSampleRate(const WebRtc_UWord32 samplesPerSec)
{
    CHECK_INITIALIZED();

    if (_ptrAudioDevice->SetPlayoutSampleRate(samplesPerSec) != 0)
    {
        return -1;
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  PlayoutSampleRate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::PlayoutSampleRate(WebRtc_UWord32* samplesPerSec) const
{
    CHECK_INITIALIZED();

    WebRtc_Word32 sampleRate = _audioDeviceBuffer.PlayoutSampleRate();

    if (sampleRate == -1)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "failed to retrieve the sample rate");
        return -1;
    }

    *samplesPerSec = sampleRate;

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: samplesPerSec=%u", *samplesPerSec);
    return (0);
}

// ----------------------------------------------------------------------------
//  ResetAudioDevice
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::ResetAudioDevice()
{
    CHECK_INITIALIZED();


    if (_ptrAudioDevice->ResetAudioDevice() == -1)
    {
        return -1;
    }

    return (0);
}

// ----------------------------------------------------------------------------
//  SetLoudspeakerStatus
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::SetLoudspeakerStatus(bool enable)
{
    CHECK_INITIALIZED();

    if (_ptrAudioDevice->SetLoudspeakerStatus(enable) != 0)
    {
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  GetLoudspeakerStatus
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceModuleImpl::GetLoudspeakerStatus(bool* enabled) const
{
    CHECK_INITIALIZED();

    if (_ptrAudioDevice->GetLoudspeakerStatus(*enabled) != 0)
    {
        return -1;
    }

    return 0;
}

int32_t AudioDeviceModuleImpl::EnableBuiltInAEC(bool enable)
{
    CHECK_INITIALIZED();

    return _ptrAudioDevice->EnableBuiltInAEC(enable);
}

bool AudioDeviceModuleImpl::BuiltInAECIsEnabled() const
{
    CHECK_INITIALIZED_BOOL();

    return _ptrAudioDevice->BuiltInAECIsEnabled();
}

// ============================================================================
//                                 Private Methods
// ============================================================================

// ----------------------------------------------------------------------------
//  Platform
// ----------------------------------------------------------------------------

AudioDeviceModuleImpl::PlatformType AudioDeviceModuleImpl::Platform() const
{
    return _platformType;
}

// ----------------------------------------------------------------------------
//  PlatformAudioLayer
// ----------------------------------------------------------------------------

AudioDeviceModule::AudioLayer AudioDeviceModuleImpl::PlatformAudioLayer() const
{

    switch (_platformAudioLayer)
    {
    case kPlatformDefaultAudio:
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                     "output: kPlatformDefaultAudio");
        break;
    case kWindowsWaveAudio:
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                     "output: kWindowsWaveAudio");
        break;
    case kWindowsCoreAudio:
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                     "output: kWindowsCoreAudio");
        break;
    case kLinuxAlsaAudio:
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                     "output: kLinuxAlsaAudio");
        break;
    case kDummyAudio:
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                     "output: kDummyAudio");
        break;
    default:
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "output: INVALID");
        break;
    }

    return _platformAudioLayer;
}

}  // namespace webrtc
