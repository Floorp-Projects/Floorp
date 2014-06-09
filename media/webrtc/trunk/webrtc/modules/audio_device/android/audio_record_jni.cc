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
 *  Android audio device implementation (JNI/AudioRecord usage)
 */

// TODO(xians): Break out attach and detach current thread to JVM to
// separate functions.

#include "AndroidJNIWrapper.h"
#include "webrtc/modules/audio_device/android/audio_record_jni.h"

#include <android/log.h>
#include <stdlib.h>

#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/audio_device/audio_device_config.h"
#include "webrtc/modules/audio_device/audio_device_utility.h"

#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

JavaVM* AudioRecordJni::globalJvm = NULL;
JNIEnv* AudioRecordJni::globalJNIEnv = NULL;
jobject AudioRecordJni::globalContext = NULL;
jclass AudioRecordJni::globalScClass = NULL;

int32_t AudioRecordJni::SetAndroidAudioDeviceObjects(void* javaVM, void* env,
                                                     void* context) {
  assert(env);
  globalJvm = reinterpret_cast<JavaVM*>(javaVM);
  globalJNIEnv = reinterpret_cast<JNIEnv*>(env);
  // Get java class type (note path to class packet).
  if (!globalScClass) {
    globalScClass = jsjni_GetGlobalClassRef(
        "org/webrtc/voiceengine/WebRtcAudioRecord");
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

void AudioRecordJni::ClearAndroidAudioDeviceObjects() {
  WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, -1,
               "%s: env is NULL, assuming deinit", __FUNCTION__);

  globalJvm = NULL;
  if (!globalJNIEnv) {
    WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, -1,
                 "%s: saved env already NULL", __FUNCTION__);
    return;
  }

  globalContext = reinterpret_cast<jobject>(NULL);

  globalJNIEnv->DeleteGlobalRef(globalScClass);
  globalScClass = reinterpret_cast<jclass>(NULL);

  globalJNIEnv = reinterpret_cast<JNIEnv*>(NULL);
}

AudioRecordJni::AudioRecordJni(
    const int32_t id, PlayoutDelayProvider* delay_provider)
    : _javaVM(NULL),
      _jniEnvRec(NULL),
      _javaScClass(0),
      _javaScObj(0),
      _javaRecBuffer(0),
      _javaDirectRecBuffer(NULL),
      _javaMidRecAudio(0),
      _ptrAudioBuffer(NULL),
      _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
      _id(id),
      _delay_provider(delay_provider),
      _initialized(false),
      _timeEventRec(*EventWrapper::Create()),
      _recStartStopEvent(*EventWrapper::Create()),
      _ptrThreadRec(NULL),
      _recThreadID(0),
      _recThreadIsInitialized(false),
      _shutdownRecThread(false),
      _recordingDeviceIsSpecified(false),
      _recording(false),
      _recIsInitialized(false),
      _micIsInitialized(false),
      _startRec(false),
      _recWarning(0),
      _recError(0),
      _delayRecording(0),
      _AGC(false),
      _samplingFreqIn((N_REC_SAMPLES_PER_SEC)),
      _recAudioSource(1) { // 1 is AudioSource.MIC which is our default
  memset(_recBuffer, 0, sizeof(_recBuffer));
}

AudioRecordJni::~AudioRecordJni() {
  WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
               "%s destroyed", __FUNCTION__);

  Terminate();

  delete &_recStartStopEvent;
  delete &_timeEventRec;
  delete &_critSect;
}

int32_t AudioRecordJni::Init() {
  CriticalSectionScoped lock(&_critSect);

  if (_initialized)
  {
    return 0;
  }

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

  const char* threadName = "jni_audio_capture_thread";
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
  _initialized = true;

  return 0;
}

int32_t AudioRecordJni::Terminate() {
  CriticalSectionScoped lock(&_critSect);

  if (!_initialized)
  {
    return 0;
  }

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
  _javaMidRecAudio = 0;
  _javaDirectRecBuffer = NULL;

  // Delete the references to the java buffers, this allows the
  // garbage collector to delete them
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

int32_t AudioRecordJni::RecordingDeviceName(uint16_t index,
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

int32_t AudioRecordJni::SetRecordingDevice(uint16_t index) {
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

int32_t AudioRecordJni::SetRecordingDevice(
    AudioDeviceModule::WindowsDeviceType device) {
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}

int32_t AudioRecordJni::RecordingIsAvailable(bool& available) {  // NOLINT
  available = false;

  // Try to initialize the playout side
  int32_t res = InitRecording();

  // Cancel effect of initialization
  StopRecording();

  if (res != -1)
  {
    available = true;
  }

  return res;
}

int32_t AudioRecordJni::InitRecording() {
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

  int retVal = -1;

  // call java sc object method
  jint res = env->CallIntMethod(_javaScObj, initRecordingID, _recAudioSource,
                                _samplingFreqIn);
  if (res < 0)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "InitRecording failed (%d)", res);
  }
  else
  {
    // Set the audio device buffer sampling rate
    _ptrAudioBuffer->SetRecordingSampleRate(_samplingFreqIn);

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

int32_t AudioRecordJni::StartRecording() {
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

int32_t AudioRecordJni::StopRecording() {
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

int32_t AudioRecordJni::SetAGC(bool enable) {
  _AGC = enable;
  return 0;
}

int32_t AudioRecordJni::MicrophoneIsAvailable(bool& available) {  // NOLINT
  // We always assume it's available
  available = true;
  return 0;
}

int32_t AudioRecordJni::InitMicrophone() {
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

int32_t AudioRecordJni::MicrophoneVolumeIsAvailable(
    bool& available) {  // NOLINT
  available = false;  // Mic volume not supported on Android
  return 0;
}

int32_t AudioRecordJni::SetMicrophoneVolume( uint32_t /*volume*/) {

  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}

int32_t AudioRecordJni::MicrophoneVolume(uint32_t& volume) const {  // NOLINT
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}

int32_t AudioRecordJni::MaxMicrophoneVolume(
    uint32_t& maxVolume) const {  // NOLINT
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}

int32_t AudioRecordJni::MinMicrophoneVolume(
    uint32_t& minVolume) const {  // NOLINT
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}

int32_t AudioRecordJni::MicrophoneVolumeStepSize(
    uint16_t& stepSize) const {
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}

int32_t AudioRecordJni::MicrophoneMuteIsAvailable(bool& available) {  // NOLINT
  available = false; // Mic mute not supported on Android
  return 0;
}

int32_t AudioRecordJni::SetMicrophoneMute(bool enable) {
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}

int32_t AudioRecordJni::MicrophoneMute(bool& enabled) const {  // NOLINT
  WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id,
               "  API call not supported on this platform");
  return -1;
}

int32_t AudioRecordJni::MicrophoneBoostIsAvailable(bool& available) {  // NOLINT
  available = false; // Mic boost not supported on Android
  return 0;
}

int32_t AudioRecordJni::SetMicrophoneBoost(bool enable) {
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

int32_t AudioRecordJni::MicrophoneBoost(bool& enabled) const {  // NOLINT
  if (!_micIsInitialized)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "  Microphone not initialized");
    return -1;
  }

  enabled = false;

  return 0;
}

int32_t AudioRecordJni::StereoRecordingIsAvailable(bool& available) {  // NOLINT
  available = false; // Stereo recording not supported on Android
  return 0;
}

int32_t AudioRecordJni::SetStereoRecording(bool enable) {
  if (enable)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "  Enabling not available");
    return -1;
  }

  return 0;
}

int32_t AudioRecordJni::StereoRecording(bool& enabled) const {  // NOLINT
  enabled = false;
  return 0;
}

int32_t AudioRecordJni::RecordingDelay(uint16_t& delayMS) const {  // NOLINT
  delayMS = _delayRecording;
  return 0;
}

bool AudioRecordJni::RecordingWarning() const {
  return (_recWarning > 0);
}

bool AudioRecordJni::RecordingError() const {
  return (_recError > 0);
}

void AudioRecordJni::ClearRecordingWarning() {
  _recWarning = 0;
}

void AudioRecordJni::ClearRecordingError() {
  _recError = 0;
}

void AudioRecordJni::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  CriticalSectionScoped lock(&_critSect);
  _ptrAudioBuffer = audioBuffer;
  // inform the AudioBuffer about default settings for this implementation
  _ptrAudioBuffer->SetRecordingSampleRate(N_REC_SAMPLES_PER_SEC);
  _ptrAudioBuffer->SetRecordingChannels(N_REC_CHANNELS);
}

int32_t AudioRecordJni::SetRecordingSampleRate(const uint32_t samplesPerSec) {
  if (samplesPerSec > 48000 || samplesPerSec < 8000)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "  Invalid sample rate");
    return -1;
  }

  // set the recording sample rate to use
  _samplingFreqIn = samplesPerSec;

  // Update the AudioDeviceBuffer
  _ptrAudioBuffer->SetRecordingSampleRate(samplesPerSec);

  return 0;
}

int32_t AudioRecordJni::InitJavaResources() {
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

  // Get rec buffer field ID.
  jfieldID fidRecBuffer = env->GetFieldID(_javaScClass, "_recBuffer",
                                          "Ljava/nio/ByteBuffer;");
  if (!fidRecBuffer)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "%s: could not get rec buffer fid", __FUNCTION__);
    return -1;
  }

  // Get rec buffer object.
  jobject javaRecBufferLocal = env->GetObjectField(_javaScObj, fidRecBuffer);
  if (!javaRecBufferLocal)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "%s: could not get rec buffer", __FUNCTION__);
    return -1;
  }

  // Create a global reference to the object (to tell JNI that we are
  // referencing it after this function has returned)
  // NOTE: we are referencing it only through the direct buffer (see below).
  _javaRecBuffer = env->NewGlobalRef(javaRecBufferLocal);
  if (!_javaRecBuffer)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "%s: could not get rec buffer reference", __FUNCTION__);
    return -1;
  }

  // Delete local object ref, we only use the global ref.
  env->DeleteLocalRef(javaRecBufferLocal);

  // Get direct buffer.
  _javaDirectRecBuffer = env->GetDirectBufferAddress(_javaRecBuffer);
  if (!_javaDirectRecBuffer)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "%s: could not get direct rec buffer", __FUNCTION__);
    return -1;
  }

  // Get the rec audio method ID.
  _javaMidRecAudio = env->GetMethodID(_javaScClass, "RecordAudio", "(I)I");
  if (!_javaMidRecAudio)
  {
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                 "%s: could not get rec audio mid", __FUNCTION__);
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

int32_t AudioRecordJni::InitSampleRate() {
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
    samplingFreq = _samplingFreqIn;
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
  _samplingFreqIn = samplingFreq;

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

bool AudioRecordJni::RecThreadFunc(void* pThis)
{
  return (static_cast<AudioRecordJni*> (pThis)->RecThreadProcess());
}

bool AudioRecordJni::RecThreadProcess()
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
    uint32_t samplesToRec = _samplingFreqIn / 100;

    // Call java sc object method to record data to direct buffer
    // Will block until data has been recorded (see java sc class),
    // therefore we must release the lock
    UnLock();
    jint recDelayInSamples = _jniEnvRec->CallIntMethod(_javaScObj,
                                                        _javaMidRecAudio,
                                                        2 * samplesToRec);
    if (recDelayInSamples < 0)
    {
      WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id,
                   "RecordAudio failed");
      _recWarning = 1;
    }
    else
    {
      _delayRecording = (recDelayInSamples * 1000) / _samplingFreqIn;
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
      _ptrAudioBuffer->SetVQEData(_delay_provider->PlayoutDelayMs(),
                                  _delayRecording, 0);

      // deliver recorded samples at specified sample rate, mic level
      // etc. to the observer using callback
      UnLock();
      _ptrAudioBuffer->DeliverRecordedData();
      Lock();
    }

  }  // _recording

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

}  // namespace webrtc
