/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Android audio device implementation (JNI/AudioTrack/AudioRecord usage)
 */

// TODO(xians): Break out attach and detach current thread to JVM to
// separate functions.

#include <stdlib.h>
#include "audio_device_utility.h"
#include "audio_device_android_jni.h"
#include "audio_device_config.h"

#include "trace.h"
#include "thread_wrapper.h"
#include "event_wrapper.h"

// Android logging, uncomment to print trace to logcat instead of
// trace file/callback
//#include <android/log.h>
//#define WEBRTC_TRACE(a,b,c,...)  __android_log_print(ANDROID_LOG_DEBUG, \
//    "WebRTC AD jni", __VA_ARGS__)

namespace webrtc
{

JavaVM* globalJvm = NULL;
JNIEnv* globalJNIEnv = NULL;
jobject globalSndContext = NULL;
jclass globalScClass = NULL;

// ----------------------------------------------------------------------------
//  SetAndroidAudioDeviceObjects
//
//  Global function for setting Java pointers and creating Java
//  objects that are global to all instances of VoiceEngine used
//  by the same Java application.
// ----------------------------------------------------------------------------

WebRtc_Word32 SetAndroidAudioDeviceObjects(void* javaVM, void* env,
                                           void* context)
{
    WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, -1, "%s",
                 __FUNCTION__);

    globalJvm = (JavaVM*) javaVM;
    globalSndContext = (jobject) context;

    if (env)
    {
        globalJNIEnv = (JNIEnv *) env;

        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, -1,
                     "%s: will find class", __FUNCTION__);

        // get java class type (note path to class packet)
        jclass
                javaScClassLocal =
                        globalJNIEnv->FindClass(
                                "org/webrtc/voiceengine/AudioDeviceAndroid");
        if (!javaScClassLocal)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
                         "%s: could not find java class", __FUNCTION__);
            return -1; /* exception thrown */
        }

        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, -1,
                     "%s: will create global reference", __FUNCTION__);

        // create a global reference to the class (to tell JNI that we are
        // referencing it after this function has returned)
        globalScClass
                = reinterpret_cast<jclass> (globalJNIEnv->NewGlobalRef(
                        javaScClassLocal));
        if (!globalScClass)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
                         "%s: could not create reference", __FUNCTION__);
            return -1;
        }

        // Delete local class ref, we only use the global ref
        globalJNIEnv->DeleteLocalRef(javaScClassLocal);
    }
    else // User is resetting the env variable
    {
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, -1,
                     "%s: env is NULL, assuming deinit", __FUNCTION__);

        if (!globalJNIEnv)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, -1,
                         "%s: saved env already NULL", __FUNCTION__);
            return 0;
        }

        globalJNIEnv->DeleteGlobalRef(globalScClass);
        globalJNIEnv = (JNIEnv *) NULL;
    }

    return 0;
}

// ============================================================================
//                            Construction & Destruction
// ============================================================================

// ----------------------------------------------------------------------------
//  AudioDeviceAndroidJni - ctor
// ----------------------------------------------------------------------------

AudioDeviceAndroidJni::AudioDeviceAndroidJni(const WebRtc_Word32 id) :
            _ptrAudioBuffer(NULL),
            _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
            _id(id),
            _timeEventRec(*EventWrapper::Create()),
            _timeEventPlay(*EventWrapper::Create()),
            _recStartStopEvent(*EventWrapper::Create()),
            _playStartStopEvent(*EventWrapper::Create()),
            _ptrThreadPlay(NULL),
            _ptrThreadRec(NULL),
            _recThreadID(0),
            _playThreadID(0),
            _playThreadIsInitialized(false),
            _recThreadIsInitialized(false),
            _shutdownPlayThread(false),
            _shutdownRecThread(false),
            //    _recBuffer[2*REC_BUF_SIZE_IN_SAMPLES]
            _recordingDeviceIsSpecified(false),
            _playoutDeviceIsSpecified(false), _initialized(false),
            _recording(false), _playing(false), _recIsInitialized(false),
            _playIsInitialized(false), _micIsInitialized(false),
            _speakerIsInitialized(false), _startRec(false), _stopRec(false),
            _startPlay(false), _stopPlay(false), _playWarning(0),
            _playError(0), _recWarning(0), _recError(0), _delayPlayout(0),
            _delayRecording(0),
            _AGC(false),
            _samplingFreqIn(0),
            _samplingFreqOut(0),
            _maxSpeakerVolume(0),
            _loudSpeakerOn(false),
            _recAudioSource(1), // 1 is AudioSource.MIC which is our default
            _javaVM(NULL), _javaContext(NULL), _jniEnvPlay(NULL),
            _jniEnvRec(NULL), _javaScClass(0), _javaScObj(0),
            _javaPlayBuffer(0), _javaRecBuffer(0), _javaDirectPlayBuffer(NULL),
            _javaDirectRecBuffer(NULL), _javaMidPlayAudio(0),
            _javaMidRecAudio(0)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id,
                 "%s created", __FUNCTION__);

    memset(_recBuffer, 0, sizeof(_recBuffer));
}

// ----------------------------------------------------------------------------
//  AudioDeviceAndroidJni - dtor
// ----------------------------------------------------------------------------

AudioDeviceAndroidJni::~AudioDeviceAndroidJni()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s destroyed", __FUNCTION__);

    Terminate();

    delete &_recStartStopEvent;
    delete &_playStartStopEvent;
    delete &_timeEventRec;
    delete &_timeEventPlay;
    delete &_critSect;
}

// ============================================================================
//                                     API
// ============================================================================

// ----------------------------------------------------------------------------
//  AttachAudioBuffer
// ----------------------------------------------------------------------------

void AudioDeviceAndroidJni::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer)
{

    CriticalSectionScoped lock(&_critSect);

    _ptrAudioBuffer = audioBuffer;

    // inform the AudioBuffer about default settings for this implementation
    _ptrAudioBuffer->SetRecordingSampleRate(N_REC_SAMPLES_PER_SEC);
    _ptrAudioBuffer->SetPlayoutSampleRate(N_PLAY_SAMPLES_PER_SEC);
    _ptrAudioBuffer->SetRecordingChannels(N_REC_CHANNELS);
    _ptrAudioBuffer->SetPlayoutChannels(N_PLAY_CHANNELS);
}

// ----------------------------------------------------------------------------
//  ActiveAudioLayer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::ActiveAudioLayer(
        AudioDeviceModule::AudioLayer& audioLayer) const
{

    audioLayer = AudioDeviceModule::kPlatformDefaultAudio;

    return 0;
}

// ----------------------------------------------------------------------------
//  Init
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::Init()
{

    CriticalSectionScoped lock(&_critSect);

    if (_initialized)
    {
        return 0;
    }

    _playWarning = 0;
    _playError = 0;
    _recWarning = 0;
    _recError = 0;

    // Init Java member variables
    // and set up JNI interface to
    // AudioDeviceAndroid java class
    if (InitJavaResources() != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: Failed to init Java resources", __FUNCTION__);
        return -1;
    }

    // Check the sample rate to be used for playback and recording
    // and the max playout volume
    if (InitSampleRate() != 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: Failed to init samplerate", __FUNCTION__);
        return -1;
    }

    // RECORDING
    const char* threadName = "webrtc_jni_audio_capture_thread";
    _ptrThreadRec = ThreadWrapper::CreateThread(RecThreadFunc, this,
                                                kRealtimePriority, threadName);
    if (_ptrThreadRec == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to create the rec audio thread");
        return -1;
    }

    unsigned int threadID(0);
    if (!_ptrThreadRec->Start(threadID))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the rec audio thread");
        delete _ptrThreadRec;
        _ptrThreadRec = NULL;
        return -1;
    }
    _recThreadID = threadID;

    // PLAYOUT
    threadName = "webrtc_jni_audio_render_thread";
    _ptrThreadPlay = ThreadWrapper::CreateThread(PlayThreadFunc, this,
                                                 kRealtimePriority, threadName);
    if (_ptrThreadPlay == NULL)
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to create the play audio thread");
        return -1;
    }

    threadID = 0;
    if (!_ptrThreadPlay->Start(threadID))
    {
        WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                     "  failed to start the play audio thread");
        delete _ptrThreadPlay;
        _ptrThreadPlay = NULL;
        return -1;
    }
    _playThreadID = threadID;

    _initialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  Terminate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::Terminate()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized)
    {
        return 0;
    }

    // RECORDING
    StopRecording();
    _shutdownRecThread = true;
    _timeEventRec.Set(); // Release rec thread from waiting state
    if (_ptrThreadRec)
    {
        // First, the thread must detach itself from Java VM
        _critSect.Leave();
        if (kEventSignaled != _recStartStopEvent.Wait(5000))
        {
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceAudioDevice,
                         _id,
                         "%s: Recording thread shutdown timed out, cannot "
                         "terminate thread",
                         __FUNCTION__);
            // If we close thread anyway, the app will crash
            return -1;
        }
        _recStartStopEvent.Reset();
        _critSect.Enter();

        // Close down rec thread
        ThreadWrapper* tmpThread = _ptrThreadRec;
        _ptrThreadRec = NULL;
        _critSect.Leave();
        tmpThread->SetNotAlive();
        // Release again, we might have returned to waiting state
        _timeEventRec.Set();
        if (tmpThread->Stop())
        {
            delete tmpThread;
            _jniEnvRec = NULL;
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  failed to close down the rec audio thread");
        }
        _critSect.Enter();

        _recThreadIsInitialized = false;
    }
    _micIsInitialized = false;
    _recordingDeviceIsSpecified = false;

    // PLAYOUT
    StopPlayout();
    _shutdownPlayThread = true;
    _timeEventPlay.Set(); // Release rec thread from waiting state
    if (_ptrThreadPlay)
    {
        // First, the thread must detach itself from Java VM
        _critSect.Leave();
        if (kEventSignaled != _playStartStopEvent.Wait(5000))
        {
            WEBRTC_TRACE(
                         kTraceError,
                         kTraceAudioDevice,
                         _id,
                         "%s: Playout thread shutdown timed out, cannot "
                         "terminate thread",
                         __FUNCTION__);
            // If we close thread anyway, the app will crash
            return -1;
        }
        _playStartStopEvent.Reset();
        _critSect.Enter();

        // Close down play thread
        ThreadWrapper* tmpThread = _ptrThreadPlay;
        _ptrThreadPlay = NULL;
        _critSect.Leave();
        tmpThread->SetNotAlive();
        _timeEventPlay.Set();
        if (tmpThread->Stop())
        {
            delete tmpThread;
            _jniEnvPlay = NULL;
        }
        else
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  failed to close down the play audio thread");
        }
        _critSect.Enter();

        _playThreadIsInitialized = false;
    }
    _speakerIsInitialized = false;
    _playoutDeviceIsSpecified = false;

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    // get the JNI env for this thread
    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "%s: Could not attach thread to JVM (%d, %p)",
                         __FUNCTION__, res, env);
            return -1;
        }
        isAttached = true;
    }

    // Make method IDs and buffer pointers unusable
    _javaMidPlayAudio = 0;
    _javaMidRecAudio = 0;
    _javaDirectPlayBuffer = NULL;
    _javaDirectRecBuffer = NULL;

    // Delete the references to the java buffers, this allows the
    // garbage collector to delete them
    env->DeleteGlobalRef(_javaPlayBuffer);
    _javaPlayBuffer = 0;
    env->DeleteGlobalRef(_javaRecBuffer);
    _javaRecBuffer = 0;

    // Delete the references to the java object and class, this allows the
    // garbage collector to delete them
    env->DeleteGlobalRef(_javaScObj);
    _javaScObj = 0;
    _javaScClass = 0;

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "%s: Could not detach thread from JVM", __FUNCTION__);
        }
    }

    _initialized = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  Initialized
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::Initialized() const
{

    return (_initialized);
}

// ----------------------------------------------------------------------------
//  SpeakerIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SpeakerIsAvailable(bool& available)
{

    // We always assume it's available
    available = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  InitSpeaker
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::InitSpeaker()
{

    CriticalSectionScoped lock(&_critSect);

    if (_playing)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Playout already started");
        return -1;
    }

    if (!_playoutDeviceIsSpecified)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout device is not specified");
        return -1;
    }

    // Nothing needs to be done here, we use a flag to have consistent
    // behavior with other platforms
    _speakerIsInitialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MicrophoneIsAvailable(bool& available)
{

    // We always assume it's available
    available = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  InitMicrophone
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::InitMicrophone()
{

    CriticalSectionScoped lock(&_critSect);

    if (_recording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Recording already started");
        return -1;
    }

    if (!_recordingDeviceIsSpecified)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording device is not specified");
        return -1;
    }

    // Nothing needs to be done here, we use a flag to have consistent
    // behavior with other platforms
    _micIsInitialized = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::SpeakerIsInitialized() const
{

    return _speakerIsInitialized;
}

// ----------------------------------------------------------------------------
//  MicrophoneIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::MicrophoneIsInitialized() const
{

    return _micIsInitialized;
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SpeakerVolumeIsAvailable(bool& available)
{

    available = true; // We assume we are always be able to set/get volume

    return 0;
}

// ----------------------------------------------------------------------------
//  SetSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetSpeakerVolume(WebRtc_UWord32 volume)
{

    if (!_speakerIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Speaker not initialized");
        return -1;
    }
    if (!_javaContext)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Context is not set");
        return -1;
    }

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  Could not attach thread to JVM (%d, %p)", res, env);
            return -1;
        }
        isAttached = true;
    }

    // get the method ID
    jmethodID setPlayoutVolumeID = env->GetMethodID(_javaScClass,
                                                    "SetPlayoutVolume", "(I)I");

    // call java sc object method
    jint res = env->CallIntMethod(_javaScObj, setPlayoutVolumeID,
                                  static_cast<int> (volume));
    if (res < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "SetPlayoutVolume failed (%d)", res);
        return -1;
    }

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  Could not detach thread from JVM");
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SpeakerVolume(WebRtc_UWord32& volume) const
{

    if (!_speakerIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Speaker not initialized");
        return -1;
    }
    if (!_javaContext)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Context is not set");
        return -1;
    }

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  Could not attach thread to JVM (%d, %p)", res, env);
            return -1;
        }
        isAttached = true;
    }

    // get the method ID
    jmethodID getPlayoutVolumeID = env->GetMethodID(_javaScClass,
                                                    "GetPlayoutVolume", "()I");

    // call java sc object method
    jint level = env->CallIntMethod(_javaScObj, getPlayoutVolumeID);
    if (level < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "GetPlayoutVolume failed (%d)", level);
        return -1;
    }

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  Could not detach thread from JVM");
        }
    }

    volume = static_cast<WebRtc_UWord32> (level);

    return 0;
}

// ----------------------------------------------------------------------------
//  SetWaveOutVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetWaveOutVolume(
    WebRtc_UWord16 /*volumeLeft*/,
    WebRtc_UWord16 /*volumeRight*/)
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  WaveOutVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::WaveOutVolume(
    WebRtc_UWord16& /*volumeLeft*/,
    WebRtc_UWord16& /*volumeRight*/) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  MaxSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MaxSpeakerVolume(
        WebRtc_UWord32& maxVolume) const
{

    if (!_speakerIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Speaker not initialized");
        return -1;
    }

    maxVolume = _maxSpeakerVolume;

    return 0;
}

// ----------------------------------------------------------------------------
//  MinSpeakerVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MinSpeakerVolume(
        WebRtc_UWord32& minVolume) const
{

    if (!_speakerIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Speaker not initialized");
        return -1;
    }

    minVolume = 0;

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerVolumeStepSize
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SpeakerVolumeStepSize(
        WebRtc_UWord16& stepSize) const
{

    if (!_speakerIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Speaker not initialized");
        return -1;
    }

    stepSize = 1;

    return 0;
}

// ----------------------------------------------------------------------------
//  SpeakerMuteIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SpeakerMuteIsAvailable(bool& available)
{

    available = false; // Speaker mute not supported on Android

    return 0;
}

// ----------------------------------------------------------------------------
//  SetSpeakerMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetSpeakerMute(bool /*enable*/)
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  SpeakerMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SpeakerMute(bool& /*enabled*/) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMuteIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MicrophoneMuteIsAvailable(bool& available)
{

    available = false; // Mic mute not supported on Android

    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetMicrophoneMute(bool /*enable*/)
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneMute
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MicrophoneMute(bool& /*enabled*/) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoostIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MicrophoneBoostIsAvailable(bool& available)
{

    available = false; // Mic boost not supported on Android

    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneBoost
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetMicrophoneBoost(bool enable)
{

    if (!_micIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Microphone not initialized");
        return -1;
    }

    if (enable)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Enabling not available");
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  MicrophoneBoost
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MicrophoneBoost(bool& enabled) const
{

    if (!_micIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Microphone not initialized");
        return -1;
    }

    enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoRecordingIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::StereoRecordingIsAvailable(bool& available)
{

    available = false; // Stereo recording not supported on Android

    return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoRecording
//
//  Specifies the number of input channels.
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetStereoRecording(bool enable)
{

    if (enable)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Enabling not available");
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::StereoRecording(bool& enabled) const
{

    enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayoutIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::StereoPlayoutIsAvailable(bool& available)
{

    available = false; // Stereo playout not supported on Android

    return 0;
}

// ----------------------------------------------------------------------------
//  SetStereoPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetStereoPlayout(bool enable)
{

    if (enable)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Enabling not available");
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StereoPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::StereoPlayout(bool& enabled) const
{

    enabled = false;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetAGC
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetAGC(bool enable)
{

    _AGC = enable;

    return 0;
}

// ----------------------------------------------------------------------------
//  AGC
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::AGC() const
{

    return _AGC;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MicrophoneVolumeIsAvailable(
        bool& available)
{

    available = false; // Mic volume not supported on Android

    return 0;
}

// ----------------------------------------------------------------------------
//  SetMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetMicrophoneVolume(
        WebRtc_UWord32 /*volume*/)
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MicrophoneVolume(
        WebRtc_UWord32& /*volume*/) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  MaxMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MaxMicrophoneVolume(
        WebRtc_UWord32& /*maxVolume*/) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  MinMicrophoneVolume
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MinMicrophoneVolume(
        WebRtc_UWord32& /*minVolume*/) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  MicrophoneVolumeStepSize
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::MicrophoneVolumeStepSize(
        WebRtc_UWord16& /*stepSize*/) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  PlayoutDevices
// ----------------------------------------------------------------------------

WebRtc_Word16 AudioDeviceAndroidJni::PlayoutDevices()
{

    // There is one device only
    return 1;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice I (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetPlayoutDevice(WebRtc_UWord16 index)
{

    if (_playIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout already initialized");
        return -1;
    }

    if (0 != index)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Device index is out of range [0,0]");
        return -1;
    }

    // Do nothing but set a flag, this is to have consistent behavior
    // with other platforms
    _playoutDeviceIsSpecified = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetPlayoutDevice II (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetPlayoutDevice(
        AudioDeviceModule::WindowsDeviceType /*device*/)
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  PlayoutDeviceName
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::PlayoutDeviceName(
        WebRtc_UWord16 index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize])
{

    if (0 != index)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Device index is out of range [0,0]");
        return -1;
    }

    // Return empty string
    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDeviceName
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::RecordingDeviceName(
        WebRtc_UWord16 index,
        char name[kAdmMaxDeviceNameSize],
        char guid[kAdmMaxGuidSize])
{

    if (0 != index)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Device index is out of range [0,0]");
        return -1;
    }

    // Return empty string
    memset(name, 0, kAdmMaxDeviceNameSize);

    if (guid)
    {
        memset(guid, 0, kAdmMaxGuidSize);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDevices
// ----------------------------------------------------------------------------

WebRtc_Word16 AudioDeviceAndroidJni::RecordingDevices()
{

    // There is one device only
    return 1;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice I (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetRecordingDevice(WebRtc_UWord16 index)
{

    if (_recIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording already initialized");
        return -1;
    }

    // Recording device index will be used for specifying recording
    // audio source, allow any value
    _recAudioSource = index;
    _recordingDeviceIsSpecified = true;

    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingDevice II (II)
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetRecordingDevice(
        AudioDeviceModule::WindowsDeviceType /*device*/)
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  PlayoutIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::PlayoutIsAvailable(bool& available)
{

    available = false;

    // Try to initialize the playout side
    WebRtc_Word32 res = InitPlayout();

    // Cancel effect of initialization
    StopPlayout();

    if (res != -1)
    {
        available = true;
    }

    return res;
}

// ----------------------------------------------------------------------------
//  RecordingIsAvailable
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::RecordingIsAvailable(bool& available)
{

    available = false;

    // Try to initialize the playout side
    WebRtc_Word32 res = InitRecording();

    // Cancel effect of initialization
    StopRecording();

    if (res != -1)
    {
        available = true;
    }

    return res;
}

// ----------------------------------------------------------------------------
//  InitPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::InitPlayout()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Not initialized");
        return -1;
    }

    if (_playing)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Playout already started");
        return -1;
    }

    if (!_playoutDeviceIsSpecified)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout device is not specified");
        return -1;
    }

    if (_playIsInitialized)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playout already initialized");
        return 0;
    }

    // Initialize the speaker
    if (InitSpeaker() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  InitSpeaker() failed");
    }

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    // get the JNI env for this thread
    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                     "attaching");

        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  Could not attach thread to JVM (%d, %p)", res, env);
            return -1;
        }
        isAttached = true;
    }

    // get the method ID
    jmethodID initPlaybackID = env->GetMethodID(_javaScClass, "InitPlayback",
                                                "(I)I");

    int samplingFreq = 44100;
    if (_samplingFreqOut != 44)
    {
        samplingFreq = _samplingFreqOut * 1000;
    }

    int retVal = -1;

    // Call java sc object method
    jint res = env->CallIntMethod(_javaScObj, initPlaybackID, samplingFreq);
    if (res < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "InitPlayback failed (%d)", res);
    }
    else
    {
        // Set the audio device buffer sampling rate
        _ptrAudioBuffer->SetPlayoutSampleRate(_samplingFreqOut * 1000);
        _playIsInitialized = true;
        retVal = 0;
    }

    // Detach this thread if it was attached
    if (isAttached)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                     "detaching");
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  Could not detach thread from JVM");
        }
    }

    return retVal;
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::InitRecording()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_initialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Not initialized");
        return -1;
    }

    if (_recording)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Recording already started");
        return -1;
    }

    if (!_recordingDeviceIsSpecified)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording device is not specified");
        return -1;
    }

    if (_recIsInitialized)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Recording already initialized");
        return 0;
    }

    // Initialize the microphone
    if (InitMicrophone() == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  InitMicrophone() failed");
    }

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    // get the JNI env for this thread
    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  Could not attach thread to JVM (%d, %p)", res, env);
            return -1;
        }
        isAttached = true;
    }

    // get the method ID
    jmethodID initRecordingID = env->GetMethodID(_javaScClass, "InitRecording",
                                                 "(II)I");

    int samplingFreq = 44100;
    if (_samplingFreqIn != 44)
    {
        samplingFreq = _samplingFreqIn * 1000;
    }

    int retVal = -1;

    // call java sc object method
    jint res = env->CallIntMethod(_javaScObj, initRecordingID, _recAudioSource,
                                  samplingFreq);
    if (res < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "InitRecording failed (%d)", res);
    }
    else
    {
        // Set the audio device buffer sampling rate
        _ptrAudioBuffer->SetRecordingSampleRate(_samplingFreqIn * 1000);

        // the init rec function returns a fixed delay
        _delayRecording = res / _samplingFreqIn;

        _recIsInitialized = true;
        retVal = 0;
    }

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  Could not detach thread from JVM");
        }
    }

    return retVal;
}

// ----------------------------------------------------------------------------
//  StartRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::StartRecording()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_recIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Recording not initialized");
        return -1;
    }

    if (_recording)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Recording already started");
        return 0;
    }

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    // get the JNI env for this thread
    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  Could not attach thread to JVM (%d, %p)", res, env);
            return -1;
        }
        isAttached = true;
    }

    // get the method ID
    jmethodID startRecordingID = env->GetMethodID(_javaScClass,
                                                  "StartRecording", "()I");

    // Call java sc object method
    jint res = env->CallIntMethod(_javaScObj, startRecordingID);
    if (res < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "StartRecording failed (%d)", res);
        return -1;
    }

    _recWarning = 0;
    _recError = 0;

    // Signal to recording thread that we want to start
    _startRec = true;
    _timeEventRec.Set(); // Release thread from waiting state
    _critSect.Leave();
    // Wait for thread to init
    if (kEventSignaled != _recStartStopEvent.Wait(5000))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Timeout or error starting");
    }
    _recStartStopEvent.Reset();
    _critSect.Enter();

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  Could not detach thread from JVM");
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StopRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::StopRecording()

{

    CriticalSectionScoped lock(&_critSect);

    if (!_recIsInitialized)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Recording is not initialized");
        return 0;
    }

    // make sure we don't start recording (it's asynchronous),
    // assuming that we are under lock
    _startRec = false;

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    // get the JNI env for this thread
    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  Could not attach thread to JVM (%d, %p)", res, env);
            return -1;
        }
        isAttached = true;
    }

    // get the method ID
    jmethodID stopRecordingID = env->GetMethodID(_javaScClass, "StopRecording",
                                                 "()I");

    // Call java sc object method
    jint res = env->CallIntMethod(_javaScObj, stopRecordingID);
    if (res < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "StopRecording failed (%d)", res);
    }

    _recIsInitialized = false;
    _recording = false;
    _recWarning = 0;
    _recError = 0;

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  Could not detach thread from JVM");
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::RecordingIsInitialized() const
{

    return _recIsInitialized;
}

// ----------------------------------------------------------------------------
//  Recording
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::Recording() const
{

    return _recording;
}

// ----------------------------------------------------------------------------
//  PlayoutIsInitialized
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::PlayoutIsInitialized() const
{

    return _playIsInitialized;
}

// ----------------------------------------------------------------------------
//  StartPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::StartPlayout()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_playIsInitialized)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Playout not initialized");
        return -1;
    }

    if (_playing)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playout already started");
        return 0;
    }

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    // get the JNI env for this thread
    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  Could not attach thread to JVM (%d, %p)", res, env);
            return -1;
        }
        isAttached = true;
    }

    // get the method ID
    jmethodID startPlaybackID = env->GetMethodID(_javaScClass, "StartPlayback",
                                                 "()I");

    // Call java sc object method
    jint res = env->CallIntMethod(_javaScObj, startPlaybackID);
    if (res < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "StartPlayback failed (%d)", res);
        return -1;
    }

    _playWarning = 0;
    _playError = 0;

    // Signal to playout thread that we want to start
    _startPlay = true;
    _timeEventPlay.Set(); // Release thread from waiting state
    _critSect.Leave();
    // Wait for thread to init
    if (kEventSignaled != _playStartStopEvent.Wait(5000))
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Timeout or error starting");
    }
    _playStartStopEvent.Reset();
    _critSect.Enter();

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  Could not detach thread from JVM");
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  StopPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::StopPlayout()
{

    CriticalSectionScoped lock(&_critSect);

    if (!_playIsInitialized)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "  Playout is not initialized");
        return 0;
    }

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    // get the JNI env for this thread
    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  Could not attach thread to JVM (%d, %p)", res, env);
            return -1;
        }
        isAttached = true;
    }

    // get the method ID
    jmethodID stopPlaybackID = env->GetMethodID(_javaScClass, "StopPlayback",
                                                "()I");

    // Call java sc object method
    jint res = env->CallIntMethod(_javaScObj, stopPlaybackID);
    if (res < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "StopPlayback failed (%d)", res);
    }

    _playIsInitialized = false;
    _playing = false;
    _playWarning = 0;
    _playError = 0;

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "  Could not detach thread from JVM");
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  PlayoutDelay
//
//    Remaining amount of data still in the playout buffer.
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::PlayoutDelay(WebRtc_UWord16& delayMS) const
{
    delayMS = _delayPlayout;

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingDelay
//
//    Remaining amount of data still in the recording buffer.
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::RecordingDelay(
        WebRtc_UWord16& delayMS) const
{
    delayMS = _delayRecording;

    return 0;
}

// ----------------------------------------------------------------------------
//  Playing
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::Playing() const
{

    return _playing;
}

// ----------------------------------------------------------------------------
//  SetPlayoutBuffer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetPlayoutBuffer(
        const AudioDeviceModule::BufferType /*type*/,
        WebRtc_UWord16 /*sizeMS*/)
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  PlayoutBuffer
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::PlayoutBuffer(
        AudioDeviceModule::BufferType& type,
        WebRtc_UWord16& sizeMS) const
{

    type = AudioDeviceModule::kAdaptiveBufferSize;
    sizeMS = _delayPlayout; // Set to current playout delay

    return 0;
}

// ----------------------------------------------------------------------------
//  CPULoad
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::CPULoad(WebRtc_UWord16& /*load*/) const
{

    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
    return -1;
}

// ----------------------------------------------------------------------------
//  PlayoutWarning
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::PlayoutWarning() const
{
    return (_playWarning > 0);
}

// ----------------------------------------------------------------------------
//  PlayoutError
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::PlayoutError() const
{
    return (_playError > 0);
}

// ----------------------------------------------------------------------------
//  RecordingWarning
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::RecordingWarning() const
{
    return (_recWarning > 0);
}

// ----------------------------------------------------------------------------
//  RecordingError
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::RecordingError() const
{
    return (_recError > 0);
}

// ----------------------------------------------------------------------------
//  ClearPlayoutWarning
// ----------------------------------------------------------------------------

void AudioDeviceAndroidJni::ClearPlayoutWarning()
{
    _playWarning = 0;
}

// ----------------------------------------------------------------------------
//  ClearPlayoutError
// ----------------------------------------------------------------------------

void AudioDeviceAndroidJni::ClearPlayoutError()
{
    _playError = 0;
}

// ----------------------------------------------------------------------------
//  ClearRecordingWarning
// ----------------------------------------------------------------------------

void AudioDeviceAndroidJni::ClearRecordingWarning()
{
    _recWarning = 0;
}

// ----------------------------------------------------------------------------
//  ClearRecordingError
// ----------------------------------------------------------------------------

void AudioDeviceAndroidJni::ClearRecordingError()
{
    _recError = 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingSampleRate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetRecordingSampleRate(
        const WebRtc_UWord32 samplesPerSec)
{

    if (samplesPerSec > 48000 || samplesPerSec < 8000)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Invalid sample rate");
        return -1;
    }

    // set the recording sample rate to use
    if (samplesPerSec == 44100)
    {
        _samplingFreqIn = 44;
    }
    else
    {
        _samplingFreqIn = samplesPerSec / 1000;
    }

    // Update the AudioDeviceBuffer
    _ptrAudioBuffer->SetRecordingSampleRate(samplesPerSec);

    return 0;
}

// ----------------------------------------------------------------------------
//  SetPlayoutSampleRate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetPlayoutSampleRate(
        const WebRtc_UWord32 samplesPerSec)
{

    if (samplesPerSec > 48000 || samplesPerSec < 8000)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "  Invalid sample rate");
        return -1;
    }

    // set the playout sample rate to use
    if (samplesPerSec == 44100)
    {
        _samplingFreqOut = 44;
    }
    else
    {
        _samplingFreqOut = samplesPerSec / 1000;
    }

    // Update the AudioDeviceBuffer
    _ptrAudioBuffer->SetPlayoutSampleRate(samplesPerSec);

    return 0;
}

// ----------------------------------------------------------------------------
//  SetLoudspeakerStatus
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::SetLoudspeakerStatus(bool enable)
{

    if (!_javaContext)
    {
        WEBRTC_TRACE(kTraceError, kTraceUtility, -1,
                     "  Context is not set");
        return -1;
    }

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);

        // Get the JNI env for this thread
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceUtility, -1,
                         "  Could not attach thread to JVM (%d, %p)", res, env);
            return -1;
        }
        isAttached = true;
    }

    // get the method ID
    jmethodID setPlayoutSpeakerID = env->GetMethodID(_javaScClass,
                                                     "SetPlayoutSpeaker",
                                                     "(Z)I");

    // call java sc object method
    jint res = env->CallIntMethod(_javaScObj, setPlayoutSpeakerID, enable);
    if (res < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceUtility, -1,
                     "  SetPlayoutSpeaker failed (%d)", res);
        return -1;
    }

    _loudSpeakerOn = enable;

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceUtility, -1,
                         "  Could not detach thread from JVM");
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  GetLoudspeakerStatus
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::GetLoudspeakerStatus(bool& enabled) const
{

    enabled = _loudSpeakerOn;

    return 0;
}

// ============================================================================
//                                 Private Methods
// ============================================================================


// ----------------------------------------------------------------------------
//  InitJavaResources
//
//  Initializes needed Java resources like the JNI interface to
//  AudioDeviceAndroid.java
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::InitJavaResources()
{
    // todo: Check if we already have created the java object
    _javaVM = globalJvm;
    _javaContext = globalSndContext;
    _javaScClass = globalScClass;

    // use the jvm that has been set
    if (!_javaVM)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: Not a valid Java VM pointer", __FUNCTION__);
        return -1;
    }

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    // get the JNI env for this thread
    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "%s: Could not attach thread to JVM (%d, %p)",
                         __FUNCTION__, res, env);
            return -1;
        }
        isAttached = true;
    }

    WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                 "get method id");

    // get the method ID for the void(void) constructor
    jmethodID cid = env->GetMethodID(_javaScClass, "<init>", "()V");
    if (cid == NULL)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get constructor ID", __FUNCTION__);
        return -1; /* exception thrown */
    }

    WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                 "construct object", __FUNCTION__);

    // construct the object
    jobject javaScObjLocal = env->NewObject(_javaScClass, cid);
    if (!javaScObjLocal)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "%s: could not create Java sc object", __FUNCTION__);
        return -1;
    }

    // create a reference to the object (to tell JNI that we are referencing it
    // after this function has returned)
    _javaScObj = env->NewGlobalRef(javaScObjLocal);
    if (!_javaScObj)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not create Java sc object reference",
                     __FUNCTION__);
        return -1;
    }

    // Delete local object ref, we only use the global ref
    env->DeleteLocalRef(javaScObjLocal);

    //////////////////////
    // AUDIO MANAGEMENT

    // This is not mandatory functionality
    if (_javaContext)
    {
        // Get Context field ID
        jfieldID fidContext = env->GetFieldID(_javaScClass, "_context",
                                              "Landroid/content/Context;");
        if (!fidContext)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "%s: could not get Context fid", __FUNCTION__);
            return -1;
        }

        // Set the Java application Context so we can use AudioManager
        // Get Context object and check it
        jobject javaContext = (jobject) _javaContext;
        env->SetObjectField(_javaScObj, fidContext, javaContext);
        javaContext = env->GetObjectField(_javaScObj, fidContext);
        if (!javaContext)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "%s: could not set Context", __FUNCTION__);
            return -1;
        }

        // Delete local object ref
        env->DeleteLocalRef(javaContext);
    }
    else
    {
        WEBRTC_TRACE(
                     kTraceWarning,
                     kTraceAudioDevice,
                     _id,
                     "%s: did not set Context - some functionality is not "
                     "supported",
                     __FUNCTION__);
    }

    /////////////
    // PLAYOUT

    // Get play buffer field ID
    jfieldID fidPlayBuffer = env->GetFieldID(_javaScClass, "_playBuffer",
                                             "Ljava/nio/ByteBuffer;");
    if (!fidPlayBuffer)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get play buffer fid", __FUNCTION__);
        return -1;
    }

    // Get play buffer object
    jobject javaPlayBufferLocal =
            env->GetObjectField(_javaScObj, fidPlayBuffer);
    if (!javaPlayBufferLocal)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get play buffer", __FUNCTION__);
        return -1;
    }

    // Create a global reference to the object (to tell JNI that we are
    // referencing it after this function has returned)
    // NOTE: we are referencing it only through the direct buffer (see below)
    _javaPlayBuffer = env->NewGlobalRef(javaPlayBufferLocal);
    if (!_javaPlayBuffer)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get play buffer reference", __FUNCTION__);
        return -1;
    }

    // Delete local object ref, we only use the global ref
    env->DeleteLocalRef(javaPlayBufferLocal);

    // Get direct buffer
    _javaDirectPlayBuffer = env->GetDirectBufferAddress(_javaPlayBuffer);
    if (!_javaDirectPlayBuffer)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get direct play buffer", __FUNCTION__);
        return -1;
    }

    // Get the play audio method ID
    _javaMidPlayAudio = env->GetMethodID(_javaScClass, "PlayAudio", "(I)I");
    if (!_javaMidPlayAudio)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get play audio mid", __FUNCTION__);
        return -1;
    }

    //////////////
    // RECORDING

    // Get rec buffer field ID
    jfieldID fidRecBuffer = env->GetFieldID(_javaScClass, "_recBuffer",
                                            "Ljava/nio/ByteBuffer;");
    if (!fidRecBuffer)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get rec buffer fid", __FUNCTION__);
        return -1;
    }

    // Get rec buffer object
    jobject javaRecBufferLocal = env->GetObjectField(_javaScObj, fidRecBuffer);
    if (!javaRecBufferLocal)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get rec buffer", __FUNCTION__);
        return -1;
    }

    // Create a global reference to the object (to tell JNI that we are
    // referencing it after this function has returned)
    // NOTE: we are referencing it only through the direct buffer (see below)
    _javaRecBuffer = env->NewGlobalRef(javaRecBufferLocal);
    if (!_javaRecBuffer)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get rec buffer reference", __FUNCTION__);
        return -1;
    }

    // Delete local object ref, we only use the global ref
    env->DeleteLocalRef(javaRecBufferLocal);

    // Get direct buffer
    _javaDirectRecBuffer = env->GetDirectBufferAddress(_javaRecBuffer);
    if (!_javaDirectRecBuffer)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get direct rec buffer", __FUNCTION__);
        return -1;
    }

    // Get the rec audio method ID
    _javaMidRecAudio = env->GetMethodID(_javaScClass, "RecordAudio", "(I)I");
    if (!_javaMidRecAudio)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "%s: could not get rec audio mid", __FUNCTION__);
        return -1;
    }

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "%s: Could not detach thread from JVM", __FUNCTION__);
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  InitSampleRate
//
//  checks supported sample rates for playback 
//  and recording and initializes the rates to be used
//  Also stores the max playout volume returned from InitPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceAndroidJni::InitSampleRate()
{
    int samplingFreq = 44100;
    jint res = 0;

    // get the JNI env for this thread
    JNIEnv *env;
    bool isAttached = false;

    // get the JNI env for this thread
    if (_javaVM->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK)
    {
        // try to attach the thread and get the env
        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&env, NULL);
        if ((res < 0) || !env)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "%s: Could not attach thread to JVM (%d, %p)",
                         __FUNCTION__, res, env);
            return -1;
        }
        isAttached = true;
    }

    if (_samplingFreqIn > 0)
    {
        // read the configured sampling rate
        samplingFreq = 44100;
        if (_samplingFreqIn != 44)
        {
            samplingFreq = _samplingFreqIn * 1000;
        }
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                     "  Trying configured recording sampling rate %d",
                     samplingFreq);
    }

    // get the method ID
    jmethodID initRecordingID = env->GetMethodID(_javaScClass, "InitRecording",
                                                 "(II)I");

    bool keepTrying = true;
    while (keepTrying)
    {
        // call java sc object method
        res = env->CallIntMethod(_javaScObj, initRecordingID, _recAudioSource,
                                 samplingFreq);
        if (res < 0)
        {
            switch (samplingFreq)
            {
                case 44100:
                    samplingFreq = 16000;
                    break;
                case 16000:
                    samplingFreq = 8000;
                    break;
                default: // error
                    WEBRTC_TRACE(kTraceError,
                                 kTraceAudioDevice, _id,
                                 "%s: InitRecording failed (%d)", __FUNCTION__,
                                 res);
                    return -1;
            }
        }
        else
        {
            keepTrying = false;
        }
    }

    // set the recording sample rate to use
    if (samplingFreq == 44100)
    {
        _samplingFreqIn = 44;
    }
    else
    {
        _samplingFreqIn = samplingFreq / 1000;
    }

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                 "Recording sample rate set to (%d)", _samplingFreqIn);

    // get the method ID
    jmethodID stopRecordingID = env->GetMethodID(_javaScClass, "StopRecording",
                                                 "()I");

    // Call java sc object method
    res = env->CallIntMethod(_javaScObj, stopRecordingID);
    if (res < 0)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "StopRecording failed (%d)", res);
    }

    // get the method ID
    jmethodID initPlaybackID = env->GetMethodID(_javaScClass, "InitPlayback",
                                                "(I)I");

    if (_samplingFreqOut > 0)
    {
        // read the configured sampling rate
        samplingFreq = 44100;
        if (_samplingFreqOut != 44)
        {
            samplingFreq = _samplingFreqOut * 1000;
        }
        WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                     "  Trying configured playback sampling rate %d",
                     samplingFreq);
    }
    else
    {
        // set the preferred sampling frequency
        if (samplingFreq == 8000)
        {
            // try 16000
            samplingFreq = 16000;
        }
        // else use same as recording
    }

    keepTrying = true;
    while (keepTrying)
    {
        // call java sc object method
        res = env->CallIntMethod(_javaScObj, initPlaybackID, samplingFreq);
        if (res < 0)
        {
            switch (samplingFreq)
            {
                case 44100:
                    samplingFreq = 16000;
                    break;
                case 16000:
                    samplingFreq = 8000;
                    break;
                default: // error
                    WEBRTC_TRACE(kTraceError,
                                 kTraceAudioDevice, _id,
                                 "InitPlayback failed (%d)", res);
                    return -1;
            }
        }
        else
        {
            keepTrying = false;
        }
    }

    // Store max playout volume
    _maxSpeakerVolume = static_cast<WebRtc_UWord32> (res);
    if (_maxSpeakerVolume < 1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                     "  Did not get valid max speaker volume value (%d)",
                     _maxSpeakerVolume);
    }

    // set the playback sample rate to use
    if (samplingFreq == 44100)
    {
        _samplingFreqOut = 44;
    }
    else
    {
        _samplingFreqOut = samplingFreq / 1000;
    }

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                 "Playback sample rate set to (%d)", _samplingFreqOut);

    // get the method ID
    jmethodID stopPlaybackID = env->GetMethodID(_javaScClass, "StopPlayback",
                                                "()I");

    // Call java sc object method
    res = env->CallIntMethod(_javaScObj, stopPlaybackID);
    if (res < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                     "StopPlayback failed (%d)", res);
    }

    // Detach this thread if it was attached
    if (isAttached)
    {
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                         "%s: Could not detach thread from JVM", __FUNCTION__);
        }
    }

    return 0;
}

// ============================================================================
//                                  Thread Methods
// ============================================================================

// ----------------------------------------------------------------------------
//  PlayThreadFunc
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::PlayThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceAndroidJni*> (pThis)->PlayThreadProcess());
}

// ----------------------------------------------------------------------------
//  RecThreadFunc
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::RecThreadFunc(void* pThis)
{
    return (static_cast<AudioDeviceAndroidJni*> (pThis)->RecThreadProcess());
}

// ----------------------------------------------------------------------------
//  PlayThreadProcess
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::PlayThreadProcess()
{
    if (!_playThreadIsInitialized)
    {
        // Do once when thread is started

        // Attach this thread to JVM and get the JNI env for this thread
        jint res = _javaVM->AttachCurrentThread(&_jniEnvPlay, NULL);
        if ((res < 0) || !_jniEnvPlay)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice,
                         _id,
                         "Could not attach playout thread to JVM (%d, %p)",
                         res, _jniEnvPlay);
            return false; // Close down thread
        }

        _playThreadIsInitialized = true;
    }

    if (!_playing)
    {
        switch (_timeEventPlay.Wait(1000))
        {
            case kEventSignaled:
                WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice,
                             _id, "Playout thread event signal");
                _timeEventPlay.Reset();
                break;
            case kEventError:
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice,
                             _id, "Playout thread event error");
                return true;
            case kEventTimeout:
                WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice,
                             _id, "Playout thread event timeout");
                return true;
        }
    }

    Lock();

    if (_startPlay)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "_startPlay true, performing initial actions");
        _startPlay = false;
        _playing = true;
        _playWarning = 0;
        _playError = 0;
        _playStartStopEvent.Set();
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                     "Sent signal");
    }

    if (_playing)
    {
        WebRtc_Word8 playBuffer[2 * 480]; // Max 10 ms @ 48 kHz / 16 bit
        WebRtc_UWord32 samplesToPlay = _samplingFreqOut * 10;

        // ask for new PCM data to be played out using the AudioDeviceBuffer
        // ensure that this callback is executed without taking the
        // audio-thread lock
        UnLock();
        WebRtc_UWord32 nSamples =
                _ptrAudioBuffer->RequestPlayoutData(samplesToPlay);
        Lock();

        // Check again since play may have stopped during unlocked period
        if (!_playing)
        {
            UnLock();
            return true;
        }

        nSamples = _ptrAudioBuffer->GetPlayoutData(playBuffer);
        if (nSamples != samplesToPlay)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "  invalid number of output samples(%d)", nSamples);
            _playWarning = 1;
        }

        // Copy data to our direct buffer (held by java sc object)
        // todo: Give _javaDirectPlayBuffer directly to VoE?
        memcpy(_javaDirectPlayBuffer, playBuffer, nSamples * 2);

        UnLock();

        // Call java sc object method to process data in direct buffer
        // Will block until data has been put in OS playout buffer
        // (see java sc class)
        jint res = _jniEnvPlay->CallIntMethod(_javaScObj, _javaMidPlayAudio,
                                              2 * nSamples);
        if (res < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "PlayAudio failed (%d)", res);
            _playWarning = 1;
        }
        else if (res > 0)
        {
            // we are not recording and have got a delay value from playback
            _delayPlayout = res / _samplingFreqOut;
        }
        // If 0 is returned we are recording and then play delay is updated
        // in RecordProcess

        Lock();

    } // _playing

    if (_shutdownPlayThread)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                     "Detaching thread from Java VM");

        // Detach thread from Java VM
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice,
                         _id, "Could not detach playout thread from JVM");
            _shutdownPlayThread = false;
            // If we say OK (i.e. set event) and close thread anyway,
            // app will crash
        }
        else
        {
            _jniEnvPlay = NULL;
            _shutdownPlayThread = false;
            _playStartStopEvent.Set(); // Signal to Terminate() that we are done
            WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                         "Sent signal");
        }
    }

    UnLock();
    return true;
}

// ----------------------------------------------------------------------------
//  RecThreadProcess
// ----------------------------------------------------------------------------

bool AudioDeviceAndroidJni::RecThreadProcess()
{
    if (!_recThreadIsInitialized)
    {
        // Do once when thread is started

        // Attach this thread to JVM
        jint res = _javaVM->AttachCurrentThread(&_jniEnvRec, NULL);

        // Get the JNI env for this thread
        if ((res < 0) || !_jniEnvRec)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice,
                         _id, "Could not attach rec thread to JVM (%d, %p)",
                         res, _jniEnvRec);
            return false; // Close down thread
        }

        _recThreadIsInitialized = true;
    }

    // just sleep if rec has not started
    if (!_recording)
    {
        switch (_timeEventRec.Wait(1000))
        {
            case kEventSignaled:
                WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice,
                             _id, "Recording thread event signal");
                _timeEventRec.Reset();
                break;
            case kEventError:
                WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice,
                             _id, "Recording thread event error");
                return true;
            case kEventTimeout:
                WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice,
                             _id, "Recording thread event timeout");
                return true;
        }
    }

    Lock();

    if (_startRec)
    {
        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id,
                     "_startRec true, performing initial actions");
        _startRec = false;
        _recording = true;
        _recWarning = 0;
        _recError = 0;
        _recStartStopEvent.Set();
    }

    if (_recording)
    {
        WebRtc_UWord32 samplesToRec = _samplingFreqIn * 10;

        // Call java sc object method to record data to direct buffer
        // Will block until data has been recorded (see java sc class),
        // therefore we must release the lock
        UnLock();
        jint playDelayInSamples = _jniEnvRec->CallIntMethod(_javaScObj,
                                                            _javaMidRecAudio,
                                                            2 * samplesToRec);
        if (playDelayInSamples < 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                         "RecordAudio failed");
            _recWarning = 1;
        }
        else
        {
            _delayPlayout = playDelayInSamples / _samplingFreqOut;
        }
        Lock();

        // Check again since recording may have stopped during Java call
        if (_recording)
        {
//            WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
//                         "total delay is %d", msPlayDelay + _delayRecording);

            // Copy data to our direct buffer (held by java sc object)
            // todo: Give _javaDirectRecBuffer directly to VoE?
            // todo: Check count <= 480 ?
            memcpy(_recBuffer, _javaDirectRecBuffer, 2 * samplesToRec);

            // store the recorded buffer (no action will be taken if the
            // #recorded samples is not a full buffer)
            _ptrAudioBuffer->SetRecordedBuffer(_recBuffer, samplesToRec);

            // store vqe delay values
            _ptrAudioBuffer->SetVQEData(_delayPlayout, _delayRecording, 0);

            // deliver recorded samples at specified sample rate, mic level
            // etc. to the observer using callback
            UnLock();
            _ptrAudioBuffer->DeliverRecordedData();
            Lock();
        }

    } // _recording

    if (_shutdownRecThread)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                     "Detaching rec thread from Java VM");

        // Detach thread from Java VM
        if (_javaVM->DetachCurrentThread() < 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice,
                         _id, "Could not detach recording thread from JVM");
            _shutdownRecThread = false;
            // If we say OK (i.e. set event) and close thread anyway,
            // app will crash
        }
        else
        {
            _jniEnvRec = NULL;
            _shutdownRecThread = false;
            _recStartStopEvent.Set(); // Signal to Terminate() that we are done

            WEBRTC_TRACE(kTraceDebug, kTraceAudioDevice, _id,
                         "Sent signal rec");
        }
    }

    UnLock();
    return true;
}

} // namespace webrtc
