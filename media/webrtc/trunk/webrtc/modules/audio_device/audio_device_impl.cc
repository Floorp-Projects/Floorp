/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/modules/audio_device/audio_device_impl.h"
#include "webrtc/system_wrappers/include/ref_count.h"
#include "webrtc/system_wrappers/include/tick_util.h"

#include <assert.h>
#include <string.h>

#if defined(WEBRTC_DUMMY_AUDIO_BUILD)
// do not include platform specific headers
#elif defined(_WIN32)
    #include "audio_device_wave_win.h"
 #if defined(WEBRTC_WINDOWS_CORE_AUDIO_BUILD)
    #include "audio_device_core_win.h"
 #endif
#elif defined(WEBRTC_ANDROID_OPENSLES)
#include <stdlib.h>
#include <dlfcn.h>
#include "webrtc/modules/audio_device/android/audio_device_template.h"
#include "webrtc/modules/audio_device/android/audio_manager.h"
#include "webrtc/modules/audio_device/android/audio_record_jni.h"
#include "webrtc/modules/audio_device/android/audio_track_jni.h"
#include "webrtc/modules/audio_device/android/opensles_player.h"
#elif defined(WEBRTC_AUDIO_SNDIO)
#include "audio_device_utility_sndio.h"
#include "audio_device_sndio.h"
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
 #if defined(LINUX_ALSA)
   #include "audio_device_alsa_linux.h"
 #endif
#if defined(LINUX_PULSE)
    #include "audio_device_pulse_linux.h"
#endif
#elif defined(WEBRTC_IOS)
    #include "audio_device_ios.h"
#elif defined(WEBRTC_MAC)
    #include "audio_device_mac.h"
#endif

#if defined(WEBRTC_DUMMY_FILE_DEVICES)
#include "webrtc/modules/audio_device/dummy/file_audio_device_factory.h"
#endif

#include "webrtc/modules/audio_device/dummy/audio_device_dummy.h"
#include "webrtc/modules/audio_device/dummy/file_audio_device.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"

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
    int32_t id, AudioDeviceModule::AudioLayer audioLayer) {
  return AudioDeviceModuleImpl::Create(id, audioLayer);
}

// ============================================================================
//                                   Static methods
// ============================================================================

// ----------------------------------------------------------------------------
//  AudioDeviceModule::Create()
// ----------------------------------------------------------------------------

AudioDeviceModule* AudioDeviceModuleImpl::Create(const int32_t id,
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

AudioDeviceModuleImpl::AudioDeviceModuleImpl(const int32_t id, const AudioLayer audioLayer) :
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _critSectEventCb(*CriticalSectionWrapper::CreateCriticalSection()),
    _critSectAudioCb(*CriticalSectionWrapper::CreateCriticalSection()),
    _ptrCbAudioDeviceObserver(NULL),
    _ptrAudioDevice(NULL),
    _id(id),
    _platformAudioLayer(audioLayer),
    _lastProcessTime(TickTime::MillisecondTimestamp()),
    _platformType(kPlatformNotSupported),
    _initialized(false),
    _lastError(kAdmErrNone)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id, "%s created", __FUNCTION__);
}

// ----------------------------------------------------------------------------
//  CheckPlatform
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::CheckPlatform()
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
#elif defined(WEBRTC_AUDIO_SNDIO)
    platform = kPlatformSndio;
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "current platform is POSIX using SNDIO");
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
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

int32_t AudioDeviceModuleImpl::CreatePlatformSpecificObjects()
{
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    AudioDeviceGeneric* ptrAudioDevice(NULL);

#if defined(WEBRTC_DUMMY_AUDIO_BUILD)
    ptrAudioDevice = new AudioDeviceDummy(Id());
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Dummy Audio APIs will be utilized");
#elif defined(WEBRTC_DUMMY_FILE_DEVICES)
    ptrAudioDevice = FileAudioDeviceFactory::CreateFileAudioDevice(Id());
    WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                 "Will use file-playing dummy device.");
#else
    AudioLayer audioLayer(PlatformAudioLayer());

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
#endif  // #if defined(_WIN32)

#if defined(WEBRTC_ANDROID)
    // Create an Android audio manager.
    _audioManagerAndroid.reset(new AudioManager());
    // Select best possible combination of audio layers.
    if (audioLayer == kPlatformDefaultAudio) {
      // Use Java-based audio in both directions when low-latency output
      // is not supported.
      audioLayer =  kAndroidJavaAudio;
      //Check to see if low latency output is allowed for this device.
      if (_audioManagerAndroid->IsLowLatencyPlayoutSupported()) {
        // Always use OpenSL ES for output on devices that supports the
        // low-latency output audio path.
        // Check if the OpenSLES library is available before going further.
        void* opensles_lib = dlopen("libOpenSLES.so", RTLD_LAZY);
        if (opensles_lib) {
          // That worked, close for now and proceed normally.
          dlclose(opensles_lib);
          audioLayer = kAndroidJavaInputAndOpenSLESOutputAudio;
        }
      }
    }

    AudioManager* audio_manager = _audioManagerAndroid.get();
    if (audioLayer == kAndroidJavaAudio) {
      // Java audio for both input and output audio.
      ptrAudioDevice = new AudioDeviceTemplate<AudioRecordJni, AudioTrackJni>(
          audioLayer, audio_manager);
    } else if (audioLayer == kAndroidJavaInputAndOpenSLESOutputAudio) {
      // Java audio for input and OpenSL ES for output audio (i.e. mixed APIs).
      // This combination provides low-latency output audio and at the same
      // time support for HW AEC using the AudioRecord Java API.
      ptrAudioDevice = new AudioDeviceTemplate<AudioRecordJni, OpenSLESPlayer>(
          audioLayer, audio_manager);
    } else {
      // Invalid audio layer.
      ptrAudioDevice = NULL;
    }
    // END #if defined(WEBRTC_ANDROID)
#elif defined(WEBRTC_AUDIO_SNDIO)
    ptrAudioDevice = new AudioDeviceSndio(Id());
    if (ptrAudioDevice != NULL)
    {
      WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "attempting to use the Sndio audio API...");
      _platformAudioLayer = kSndioAudio;
      // Create the sndio implementation of the Device Utility.
      ptrAudioDeviceUtility = new AudioDeviceUtilitySndio(Id());
    }
#elif defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)
    // Create the *Linux* implementation of the Audio Device
    //
    if ((audioLayer == kLinuxPulseAudio) || (audioLayer == kPlatformDefaultAudio))
    {
#if defined(LINUX_PULSE)
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "attempting to use the Linux PulseAudio APIs...");

        // create *Linux PulseAudio* implementation
        AudioDeviceLinuxPulse* pulseDevice = new AudioDeviceLinuxPulse(Id());
        if (pulseDevice->Init() != -1)
        {
            ptrAudioDevice = pulseDevice;
            WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "Linux PulseAudio APIs will be utilized");
        }
        else
        {
            delete pulseDevice;
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
#endif  // #if defined(WEBRTC_LINUX) || defined(WEBRTC_BSD)

    // Create the *iPhone* implementation of the Audio Device
    //
#if defined(WEBRTC_IOS)
    if (audioLayer == kPlatformDefaultAudio)
    {
        // Create iOS Audio Device implementation.
      ptrAudioDevice = new AudioDeviceIOS();
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "iPhone Audio APIs will be utilized");
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
    }
#endif  // if defined(WEBRTC_DUMMY_AUDIO_BUILD)

    if (ptrAudioDevice == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id, "unable to create the platform specific audio device implementation");
        return -1;
    }

    // Store valid output pointers
    //
    _ptrAudioDevice = ptrAudioDevice;

    return 0;
}

// ----------------------------------------------------------------------------
//  AttachAudioBuffer
//
//  Install "bridge" between the platform implemetation and the generic
//  implementation. The "child" shall set the native sampling rate and the
//  number of channels in this function call.
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::AttachAudioBuffer()
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

    delete &_critSect;
    delete &_critSectEventCb;
    delete &_critSectAudioCb;
}

// ============================================================================
//                                  Module
// ============================================================================

// ----------------------------------------------------------------------------
//  Module::TimeUntilNextProcess
//
//  Returns the number of milliseconds until the module want a worker thread
//  to call Process().
// ----------------------------------------------------------------------------

int64_t AudioDeviceModuleImpl::TimeUntilNextProcess()
{
    int64_t now = TickTime::MillisecondTimestamp();
    int64_t deltaProcess = kAdmMaxIdleTimeProcess - (now - _lastProcessTime);
    return deltaProcess;
}

// ----------------------------------------------------------------------------
//  Module::Process
//
//  Check for posted error and warning reports. Generate callbacks if
//  new reports exists.
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::Process()
{

    _lastProcessTime = TickTime::MillisecondTimestamp();

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

int32_t AudioDeviceModuleImpl::ActiveAudioLayer(AudioLayer* audioLayer) const {
  AudioLayer activeAudio;
  if (_ptrAudioDevice->ActiveAudioLayer(activeAudio) == -1) {
    return -1;
  }
  *audioLayer = activeAudio;
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

int32_t AudioDeviceModuleImpl::Init()
{

    if (_initialized)
        return 0;

    if (!_ptrAudioDevice)
        return -1;

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

int32_t AudioDeviceModuleImpl::Terminate()
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
//  InitSpeaker
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::InitSpeaker()
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->InitSpeaker());
}

// ----------------------------------------------------------------------------
//  InitMicrophone
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::InitMicrophone()
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->InitMicrophone());
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeIsAvailable
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::SpeakerVolumeIsAvailable(bool* available)
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

int32_t AudioDeviceModuleImpl::SetSpeakerVolume(uint32_t volume)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetSpeakerVolume(volume));
}

// ----------------------------------------------------------------------------
//  SpeakerVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::SpeakerVolume(uint32_t* volume) const
{
    CHECK_INITIALIZED();

    uint32_t level(0);

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

int32_t AudioDeviceModuleImpl::SetWaveOutVolume(uint16_t volumeLeft, uint16_t volumeRight)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetWaveOutVolume(volumeLeft, volumeRight));
}

// ----------------------------------------------------------------------------
//  WaveOutVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::WaveOutVolume(uint16_t* volumeLeft, uint16_t* volumeRight) const
{
    CHECK_INITIALIZED();

    uint16_t volLeft(0);
    uint16_t volRight(0);

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

int32_t AudioDeviceModuleImpl::MaxSpeakerVolume(uint32_t* maxVolume) const
{
    CHECK_INITIALIZED();

    uint32_t maxVol(0);

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

int32_t AudioDeviceModuleImpl::MinSpeakerVolume(uint32_t* minVolume) const
{
    CHECK_INITIALIZED();

    uint32_t minVol(0);

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

int32_t AudioDeviceModuleImpl::SpeakerVolumeStepSize(uint16_t* stepSize) const
{
    CHECK_INITIALIZED();

    uint16_t delta(0);

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

int32_t AudioDeviceModuleImpl::SpeakerMuteIsAvailable(bool* available)
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

int32_t AudioDeviceModuleImpl::SetSpeakerMute(bool enable)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetSpeakerMute(enable));
}

// ----------------------------------------------------------------------------
//  SpeakerMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::SpeakerMute(bool* enabled) const
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

int32_t AudioDeviceModuleImpl::MicrophoneMuteIsAvailable(bool* available)
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

int32_t AudioDeviceModuleImpl::SetMicrophoneMute(bool enable)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetMicrophoneMute(enable));
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::MicrophoneMute(bool* enabled) const
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

int32_t AudioDeviceModuleImpl::MicrophoneBoostIsAvailable(bool* available)
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

int32_t AudioDeviceModuleImpl::SetMicrophoneBoost(bool enable)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetMicrophoneBoost(enable));
}

// ----------------------------------------------------------------------------
//  MicrophoneBoost
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::MicrophoneBoost(bool* enabled) const
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

int32_t AudioDeviceModuleImpl::MicrophoneVolumeIsAvailable(bool* available)
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

int32_t AudioDeviceModuleImpl::SetMicrophoneVolume(uint32_t volume)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetMicrophoneVolume(volume));
}

// ----------------------------------------------------------------------------
//  MicrophoneVolume
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::MicrophoneVolume(uint32_t* volume) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    CHECK_INITIALIZED();

    uint32_t level(0);

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

int32_t AudioDeviceModuleImpl::StereoRecordingIsAvailable(bool* available) const
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

int32_t AudioDeviceModuleImpl::SetStereoRecording(bool enable)
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

    int8_t nChannels(1);
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

int32_t AudioDeviceModuleImpl::StereoRecording(bool* enabled) const
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

int32_t AudioDeviceModuleImpl::SetRecordingChannel(const ChannelType channel)
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

int32_t AudioDeviceModuleImpl::RecordingChannel(ChannelType* channel) const
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

int32_t AudioDeviceModuleImpl::StereoPlayoutIsAvailable(bool* available) const
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

int32_t AudioDeviceModuleImpl::SetStereoPlayout(bool enable)
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

    int8_t nChannels(1);
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

int32_t AudioDeviceModuleImpl::StereoPlayout(bool* enabled) const
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

int32_t AudioDeviceModuleImpl::SetAGC(bool enable)
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

int32_t AudioDeviceModuleImpl::PlayoutIsAvailable(bool* available)
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

int32_t AudioDeviceModuleImpl::RecordingIsAvailable(bool* available)
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

int32_t AudioDeviceModuleImpl::MaxMicrophoneVolume(uint32_t* maxVolume) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    CHECK_INITIALIZED();

    uint32_t maxVol(0);

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

int32_t AudioDeviceModuleImpl::MinMicrophoneVolume(uint32_t* minVolume) const
{
    CHECK_INITIALIZED();

    uint32_t minVol(0);

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

int32_t AudioDeviceModuleImpl::MicrophoneVolumeStepSize(uint16_t* stepSize) const
{
    CHECK_INITIALIZED();

    uint16_t delta(0);

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

int16_t AudioDeviceModuleImpl::PlayoutDevices()
{
    CHECK_INITIALIZED();

    uint16_t nPlayoutDevices = _ptrAudioDevice->PlayoutDevices();

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id, "output: #playout devices=%d", nPlayoutDevices);
    return ((int16_t)(nPlayoutDevices));
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice I (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::SetPlayoutDevice(uint16_t index)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetPlayoutDevice(index));
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice II (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::SetPlayoutDevice(WindowsDeviceType device)
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

int32_t AudioDeviceModuleImpl::PlayoutDeviceName(
    uint16_t index,
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

int32_t AudioDeviceModuleImpl::RecordingDeviceName(
    uint16_t index,
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

int16_t AudioDeviceModuleImpl::RecordingDevices()
{
    CHECK_INITIALIZED();

    uint16_t nRecordingDevices = _ptrAudioDevice->RecordingDevices();

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                 "output: #recording devices=%d", nRecordingDevices);
    return ((int16_t)nRecordingDevices);
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice I (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::SetRecordingDevice(uint16_t index)
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->SetRecordingDevice(index));
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::SetRecordingDevice(WindowsDeviceType device)
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

int32_t AudioDeviceModuleImpl::InitPlayout()
{
    CHECK_INITIALIZED();
    _audioDeviceBuffer.InitPlayout();
    return (_ptrAudioDevice->InitPlayout());
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::InitRecording()
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

int32_t AudioDeviceModuleImpl::StartPlayout()
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->StartPlayout());
}

// ----------------------------------------------------------------------------
//  StopPlayout
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::StopPlayout()
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

int32_t AudioDeviceModuleImpl::StartRecording()
{
    CHECK_INITIALIZED();
    return (_ptrAudioDevice->StartRecording());
}
// ----------------------------------------------------------------------------
//  StopRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::StopRecording()
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

int32_t AudioDeviceModuleImpl::RegisterEventObserver(AudioDeviceObserver* eventCallback)
{

    CriticalSectionScoped lock(&_critSectEventCb);
    _ptrCbAudioDeviceObserver = eventCallback;

    return 0;
}

// ----------------------------------------------------------------------------
//  RegisterAudioCallback
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::RegisterAudioCallback(AudioTransport* audioCallback)
{

    CriticalSectionScoped lock(&_critSectAudioCb);
    _audioDeviceBuffer.RegisterAudioCallback(audioCallback);

    return 0;
}

// ----------------------------------------------------------------------------
//  StartRawInputFileRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::StartRawInputFileRecording(
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

int32_t AudioDeviceModuleImpl::StopRawInputFileRecording()
{
    CHECK_INITIALIZED();

    return (_audioDeviceBuffer.StopInputFileRecording());
}

// ----------------------------------------------------------------------------
//  StartRawOutputFileRecording
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::StartRawOutputFileRecording(
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

int32_t AudioDeviceModuleImpl::StopRawOutputFileRecording()
{
    CHECK_INITIALIZED();

    return (_audioDeviceBuffer.StopOutputFileRecording());
}

// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

int32_t AudioDeviceModuleImpl::SetPlayoutBuffer(const BufferType type, uint16_t sizeMS)
{
    CHECK_INITIALIZED();

    if (_ptrAudioDevice->PlayoutIsInitialized())
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "unable to modify the playout buffer while playing side is initialized");
        return -1;
    }

    int32_t ret(0);

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

int32_t AudioDeviceModuleImpl::PlayoutBuffer(BufferType* type, uint16_t* sizeMS) const
{
    CHECK_INITIALIZED();

    BufferType bufType;
    uint16_t size(0);

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

int32_t AudioDeviceModuleImpl::PlayoutDelay(uint16_t* delayMS) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    CHECK_INITIALIZED();

    uint16_t delay(0);

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

int32_t AudioDeviceModuleImpl::RecordingDelay(uint16_t* delayMS) const
{
    WEBRTC_TRACE(kTraceStream, kTraceAudioDevice, _id, "%s", __FUNCTION__);
    CHECK_INITIALIZED();

    uint16_t delay(0);

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

int32_t AudioDeviceModuleImpl::CPULoad(uint16_t* load) const
{
    CHECK_INITIALIZED();

    uint16_t cpuLoad(0);

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

int32_t AudioDeviceModuleImpl::SetRecordingSampleRate(const uint32_t samplesPerSec)
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

int32_t AudioDeviceModuleImpl::RecordingSampleRate(uint32_t* samplesPerSec) const
{
    CHECK_INITIALIZED();

    int32_t sampleRate = _audioDeviceBuffer.RecordingSampleRate();

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

int32_t AudioDeviceModuleImpl::SetPlayoutSampleRate(const uint32_t samplesPerSec)
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

int32_t AudioDeviceModuleImpl::PlayoutSampleRate(uint32_t* samplesPerSec) const
{
    CHECK_INITIALIZED();

    int32_t sampleRate = _audioDeviceBuffer.PlayoutSampleRate();

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

int32_t AudioDeviceModuleImpl::ResetAudioDevice()
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

int32_t AudioDeviceModuleImpl::SetLoudspeakerStatus(bool enable)
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

int32_t AudioDeviceModuleImpl::GetLoudspeakerStatus(bool* enabled) const {
  CHECK_INITIALIZED();
  if (_ptrAudioDevice->GetLoudspeakerStatus(*enabled) != 0) {
    return -1;
  }
  return 0;
}

bool AudioDeviceModuleImpl::BuiltInAECIsEnabled() const {
  CHECK_INITIALIZED_BOOL();
  return _ptrAudioDevice->BuiltInAECIsEnabled();
}

bool AudioDeviceModuleImpl::BuiltInAECIsAvailable() const {
  CHECK_INITIALIZED_BOOL();
  return _ptrAudioDevice->BuiltInAECIsAvailable();
}

int32_t AudioDeviceModuleImpl::EnableBuiltInAEC(bool enable) {
  CHECK_INITIALIZED();
  return _ptrAudioDevice->EnableBuiltInAEC(enable);
}

bool AudioDeviceModuleImpl::BuiltInAGCIsAvailable() const {
  CHECK_INITIALIZED_BOOL();
  return _ptrAudioDevice->BuiltInAGCIsAvailable();
}

int32_t AudioDeviceModuleImpl::EnableBuiltInAGC(bool enable) {
  CHECK_INITIALIZED();
  return _ptrAudioDevice->EnableBuiltInAGC(enable);
}

bool AudioDeviceModuleImpl::BuiltInNSIsAvailable() const {
  CHECK_INITIALIZED_BOOL();
  return _ptrAudioDevice->BuiltInNSIsAvailable();
}

int32_t AudioDeviceModuleImpl::EnableBuiltInNS(bool enable) {
  CHECK_INITIALIZED();
  return _ptrAudioDevice->EnableBuiltInNS(enable);
}

int AudioDeviceModuleImpl::GetPlayoutAudioParameters(
    AudioParameters* params) const {
  return _ptrAudioDevice->GetPlayoutAudioParameters(params);
}

int AudioDeviceModuleImpl::GetRecordAudioParameters(
    AudioParameters* params) const {
  return _ptrAudioDevice->GetRecordAudioParameters(params);
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
    return _platformAudioLayer;
}

}  // namespace webrtc
