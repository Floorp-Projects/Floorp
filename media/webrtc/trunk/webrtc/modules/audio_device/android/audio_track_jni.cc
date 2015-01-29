/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Android audio device implementation (JNI/AudioTrack usage)
 */

// TODO(xians): Break out attach and detach current thread to JVM to
// separate functions.

#include "webrtc/modules/audio_device/android/audio_track_jni.h"

#include <android/log.h>
#include <stdlib.h>

#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/modules/audio_device/audio_device_utility.h"

#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#include "AndroidJNIWrapper.h"

namespace webrtc {

JavaVM* AudioTrackJni::globalJvm = NULL;
JNIEnv* AudioTrackJni::globalJNIEnv = NULL;
jobject AudioTrackJni::globalContext = NULL;
jclass AudioTrackJni::globalScClass = NULL;

int32_t AudioTrackJni::SetAndroidAudioDeviceObjects(void* javaVM, void* env,
                                                    void* context) {
  assert(env);
  globalJvm = reinterpret_cast<JavaVM*>(javaVM);
  globalJNIEnv = reinterpret_cast<JNIEnv*>(env);

  // Check if we already got a reference
  if (!globalScClass) {
    // Get java class type (note path to class packet).
    globalScClass = jsjni_GetGlobalClassRef("org/webrtc/voiceengine/WebRtcAudioTrack");
    if (!globalScClass) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
                   "%s: could not find java class", __FUNCTION__);
      return -1; // exception thrown
    }
  }
  if (!globalContext) {
    globalContext = jsjni_GetGlobalContextRef();
    if (!globalContext) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
                   "%s: could not create context reference", __FUNCTION__);
      return -1;
    }
  }

  return 0;
}

void AudioTrackJni::ClearAndroidAudioDeviceObjects() {
  WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, -1,
               "%s: env is NULL, assuming deinit", __FUNCTION__);

  globalJvm = NULL;
  if (!globalJNIEnv) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, -1,
                 "%s: saved env already NULL", __FUNCTION__);
    return;
  }

  // No need to delete the shared global context ref.
  // globalJNIEnv->DeleteGlobalRef(globalContext);
  globalContext = reinterpret_cast<jobject>(NULL);

  globalJNIEnv->DeleteGlobalRef(globalScClass);
  globalScClass = reinterpret_cast<jclass>(NULL);

  globalJNIEnv = reinterpret_cast<JNIEnv*>(NULL);
}

AudioTrackJni::AudioTrackJni(const int32_t id)
    : _javaVM(NULL),
      _jniEnvPlay(NULL),
      _javaScClass(0),
      _javaScObj(0),
      _javaPlayBuffer(0),
      _javaDirectPlayBuffer(NULL),
      _javaMidPlayAudio(0),
      _ptrAudioBuffer(NULL),
      _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
      _id(id),
      _initialized(false),
      _timeEventPlay(*EventWrapper::Create()),
      _playStartStopEvent(*EventWrapper::Create()),
      _ptrThreadPlay(NULL),
      _playThreadID(0),
      _playThreadIsInitialized(false),
      _shutdownPlayThread(false),
      _playoutDeviceIsSpecified(false),
      _playing(false),
      _playIsInitialized(false),
      _speakerIsInitialized(false),
      _startPlay(false),
      _playWarning(0),
      _playError(0),
      _delayPlayout(0),
      _samplingFreqOut((N_PLAY_SAMPLES_PER_SEC)),
      _maxSpeakerVolume(0) {
}

AudioTrackJni::~AudioTrackJni() {
  WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
               "%s destroyed", __FUNCTION__);

  Terminate();

  delete &_playStartStopEvent;
  delete &_timeEventPlay;
  delete &_critSect;
}

int32_t AudioTrackJni::Init() {
  CriticalSectionScoped lock(&_critSect);
  if (_initialized)
  {
    return 0;
  }

  _playWarning = 0;
  _playError = 0;

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

  const char* threadName = "jni_audio_render_thread";
  _ptrThreadPlay = ThreadWrapper::CreateThread(PlayThreadFunc, this,
                                               kRealtimePriority, threadName);
  if (_ptrThreadPlay == NULL)
  {
    WEBRTC_TRACE(kTraceCritical, kTraceAudioDevice, _id,
                 "  failed to create the play audio thread");
    return -1;
  }

  unsigned int threadID = 0;
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

int32_t AudioTrackJni::Terminate() {
  CriticalSectionScoped lock(&_critSect);
  if (!_initialized)
  {
    return 0;
  }

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
  _javaDirectPlayBuffer = NULL;

  // Delete the references to the java buffers, this allows the
  // garbage collector to delete them
  env->DeleteGlobalRef(_javaPlayBuffer);
  _javaPlayBuffer = 0;

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

int32_t AudioTrackJni::PlayoutDeviceName(uint16_t index,
                                         char name[kAdmMaxDeviceNameSize],
                                         char guid[kAdmMaxGuidSize]) {
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

int32_t AudioTrackJni::SetPlayoutDevice(uint16_t index) {
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

int32_t AudioTrackJni::SetPlayoutDevice(
    AudioDeviceModule::WindowsDeviceType device) {
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}


int32_t AudioTrackJni::PlayoutIsAvailable(bool& available) {  // NOLINT
  available = false;

  // Try to initialize the playout side
  int32_t res = InitPlayout();

  // Cancel effect of initialization
  StopPlayout();

    if (res != -1)
    {
      available = true;
    }

    return res;
}

int32_t AudioTrackJni::InitPlayout() {
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

    int retVal = -1;

    // Call java sc object method
    jint res = env->CallIntMethod(_javaScObj, initPlaybackID, _samplingFreqOut);
    if (res < 0)
    {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                   "InitPlayback failed (%d)", res);
    }
    else
    {
      // Set the audio device buffer sampling rate
      _ptrAudioBuffer->SetPlayoutSampleRate(_samplingFreqOut);
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

int32_t AudioTrackJni::StartPlayout() {
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

int32_t AudioTrackJni::StopPlayout() {
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

int32_t AudioTrackJni::InitSpeaker() {
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

int32_t AudioTrackJni::SpeakerVolumeIsAvailable(bool& available) {  // NOLINT
  available = true; // We assume we are always be able to set/get volume
  return 0;
}

int32_t AudioTrackJni::SetSpeakerVolume(uint32_t volume) {
  if (!_speakerIsInitialized)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "  Speaker not initialized");
    return -1;
  }
  if (!globalContext)
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

int32_t AudioTrackJni::SpeakerVolume(uint32_t& volume) const {  // NOLINT
  if (!_speakerIsInitialized)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "  Speaker not initialized");
    return -1;
  }
  if (!globalContext)
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

  volume = static_cast<uint32_t> (level);

  return 0;
}


int32_t AudioTrackJni::MaxSpeakerVolume(uint32_t& maxVolume) const {  // NOLINT
  if (!_speakerIsInitialized)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "  Speaker not initialized");
    return -1;
  }

  maxVolume = _maxSpeakerVolume;

  return 0;
}

int32_t AudioTrackJni::MinSpeakerVolume(uint32_t& minVolume) const {  // NOLINT
  if (!_speakerIsInitialized)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "  Speaker not initialized");
    return -1;
  }
  minVolume = 0;
  return 0;
}

int32_t AudioTrackJni::SpeakerVolumeStepSize(
    uint16_t& stepSize) const {  // NOLINT
  if (!_speakerIsInitialized)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "  Speaker not initialized");
    return -1;
  }

  stepSize = 1;

  return 0;
}

int32_t AudioTrackJni::SpeakerMuteIsAvailable(bool& available) {  // NOLINT
  available = false; // Speaker mute not supported on Android
  return 0;
}

int32_t AudioTrackJni::SetSpeakerMute(bool enable) {
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}

int32_t AudioTrackJni::SpeakerMute(bool& /*enabled*/) const {

  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}

int32_t AudioTrackJni::StereoPlayoutIsAvailable(bool& available) {  // NOLINT
  available = false; // Stereo playout not supported on Android
  return 0;
}

int32_t AudioTrackJni::SetStereoPlayout(bool enable) {
  if (enable)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "  Enabling not available");
    return -1;
  }

  return 0;
}

int32_t AudioTrackJni::StereoPlayout(bool& enabled) const {  // NOLINT
  enabled = false;
  return 0;
}

int32_t AudioTrackJni::SetPlayoutBuffer(
    const AudioDeviceModule::BufferType type,
    uint16_t sizeMS) {
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  API call not supported on this platform");
  return -1;
}


int32_t AudioTrackJni::PlayoutBuffer(
    AudioDeviceModule::BufferType& type,  // NOLINT
    uint16_t& sizeMS) const {  // NOLINT
  type = AudioDeviceModule::kAdaptiveBufferSize;
  sizeMS = _delayPlayout; // Set to current playout delay

    return 0;
}

int32_t AudioTrackJni::PlayoutDelay(uint16_t& delayMS) const {  // NOLINT
  delayMS = _delayPlayout;
  return 0;
}

void AudioTrackJni::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  CriticalSectionScoped lock(&_critSect);
  _ptrAudioBuffer = audioBuffer;
  // inform the AudioBuffer about default settings for this implementation
  _ptrAudioBuffer->SetPlayoutSampleRate(N_PLAY_SAMPLES_PER_SEC);
  _ptrAudioBuffer->SetPlayoutChannels(N_PLAY_CHANNELS);
}

int32_t AudioTrackJni::SetPlayoutSampleRate(const uint32_t samplesPerSec) {
  if (samplesPerSec > 48000 || samplesPerSec < 8000)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "  Invalid sample rate");
    return -1;
    }

  // set the playout sample rate to use
  _samplingFreqOut = samplesPerSec;

  // Update the AudioDeviceBuffer
  _ptrAudioBuffer->SetPlayoutSampleRate(samplesPerSec);

  return 0;
}

bool AudioTrackJni::PlayoutWarning() const {
  return (_playWarning > 0);
}

bool AudioTrackJni::PlayoutError() const {
  return (_playError > 0);
}

void AudioTrackJni::ClearPlayoutWarning() {
  _playWarning = 0;
}

void AudioTrackJni::ClearPlayoutError() {
  _playError = 0;
}

int32_t AudioTrackJni::SetLoudspeakerStatus(bool enable) {
  if (!globalContext)
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

int32_t AudioTrackJni::GetLoudspeakerStatus(bool& enabled) const {  // NOLINT
  enabled = _loudSpeakerOn;
  return 0;
}

int32_t AudioTrackJni::InitJavaResources() {
  // todo: Check if we already have created the java object
  _javaVM = globalJvm;
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

  // Create a reference to the object (to tell JNI that we are referencing it
  // after this function has returned).
  _javaScObj = env->NewGlobalRef(javaScObjLocal);
  if (!_javaScObj)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "%s: could not create Java sc object reference",
                 __FUNCTION__);
    return -1;
  }

  // Delete local object ref, we only use the global ref.
  env->DeleteLocalRef(javaScObjLocal);

  //////////////////////
  // AUDIO MANAGEMENT

  // This is not mandatory functionality
  if (globalContext) {
    jfieldID context_id = env->GetFieldID(globalScClass,
                                          "_context",
                                          "Landroid/content/Context;");
    if (!context_id) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                   "%s: could not get _context id", __FUNCTION__);
      return -1;
    }

    env->SetObjectField(_javaScObj, context_id, globalContext);
    jobject javaContext = env->GetObjectField(_javaScObj, context_id);
    if (!javaContext) {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                   "%s: could not set or get _context", __FUNCTION__);
      return -1;
    }
  }
  else {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "%s: did not set Context - some functionality is not "
                 "supported",
                 __FUNCTION__);
  }

  /////////////
  // PLAYOUT

  // Get play buffer field ID.
  jfieldID fidPlayBuffer = env->GetFieldID(_javaScClass, "_playBuffer",
                                           "Ljava/nio/ByteBuffer;");
  if (!fidPlayBuffer)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "%s: could not get play buffer fid", __FUNCTION__);
    return -1;
  }

  // Get play buffer object.
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
  // NOTE: we are referencing it only through the direct buffer (see below).
  _javaPlayBuffer = env->NewGlobalRef(javaPlayBufferLocal);
  if (!_javaPlayBuffer)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "%s: could not get play buffer reference", __FUNCTION__);
    return -1;
  }

  // Delete local object ref, we only use the global ref.
  env->DeleteLocalRef(javaPlayBufferLocal);

  // Get direct buffer.
  _javaDirectPlayBuffer = env->GetDirectBufferAddress(_javaPlayBuffer);
  if (!_javaDirectPlayBuffer)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "%s: could not get direct play buffer", __FUNCTION__);
    return -1;
  }

  // Get the play audio method ID.
  _javaMidPlayAudio = env->GetMethodID(_javaScClass, "PlayAudio", "(I)I");
  if (!_javaMidPlayAudio)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "%s: could not get play audio mid", __FUNCTION__);
    return -1;
  }

  // Detach this thread if it was attached.
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

int32_t AudioTrackJni::InitSampleRate() {
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

  // get the method ID
  jmethodID initPlaybackID = env->GetMethodID(_javaScClass, "InitPlayback",
                                              "(I)I");

  if (_samplingFreqOut > 0)
  {
    // read the configured sampling rate
    samplingFreq = _samplingFreqOut;
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

  bool keepTrying = true;
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
  _maxSpeakerVolume = static_cast<uint32_t> (res);
  if (_maxSpeakerVolume < 1)
  {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
                 "  Did not get valid max speaker volume value (%d)",
                 _maxSpeakerVolume);
  }

  // set the playback sample rate to use
  _samplingFreqOut = samplingFreq;

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

bool AudioTrackJni::PlayThreadFunc(void* pThis)
{
  return (static_cast<AudioTrackJni*> (pThis)->PlayThreadProcess());
}

bool AudioTrackJni::PlayThreadProcess()
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
    int8_t playBuffer[2 * 480]; // Max 10 ms @ 48 kHz / 16 bit
    uint32_t samplesToPlay = _samplingFreqOut * 10;

    // ask for new PCM data to be played out using the AudioDeviceBuffer
    // ensure that this callback is executed without taking the
    // audio-thread lock
    UnLock();
    uint32_t nSamples =
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
          _delayPlayout = (res * 1000) / _samplingFreqOut;
        }
        Lock();

  }  // _playing

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

}  // namespace webrtc
