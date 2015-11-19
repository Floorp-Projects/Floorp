/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "AndroidJNIWrapper.h"
#include "webrtc/modules/audio_device/android/audio_record_jni.h"

#include <android/log.h>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_device/android/audio_common.h"

#define TAG "AudioRecordJni"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

namespace webrtc {

// We are unable to obtain exact measurements of the hardware delay on Android.
// Instead, a lower bound (based on measurements) is used.
// TODO(henrika): is it possible to improve this?
static const int kHardwareDelayInMilliseconds = 100;

static JavaVM* g_jvm = NULL;
static jobject g_context = NULL;
static jclass g_audio_record_class = NULL;

void AudioRecordJni::SetAndroidAudioDeviceObjects(void* jvm, void* context) {
  ALOGD("SetAndroidAudioDeviceObjects%s", GetThreadInfo().c_str());

  CHECK(jvm);
  CHECK(context);

  g_jvm = reinterpret_cast<JavaVM*>(jvm);
  JNIEnv* jni = GetEnv(g_jvm);
  CHECK(jni) << "AttachCurrentThread must be called on this tread";

  if (!g_context) {
    // Protect context from being deleted during garbage collection.
    g_context = NewGlobalRef(jni, reinterpret_cast<jobject>(context));
  }

  if (!g_audio_record_class) {
    g_audio_record_class = jsjni_GetGlobalClassRef(
                               "org/webrtc/voiceengine/WebRtcAudioRecord");
    DCHECK(g_audio_record_class);
  }

  // Register native methods with the WebRtcAudioRecord class. These methods
  // are declared private native in WebRtcAudioRecord.java.
  JNINativeMethod native_methods[] = {
      {"nativeCacheDirectBufferAddress", "(Ljava/nio/ByteBuffer;J)V",
          reinterpret_cast<void*>(
       &webrtc::AudioRecordJni::CacheDirectBufferAddress)},
      {"nativeDataIsRecorded", "(IJ)V",
          reinterpret_cast<void*>(&webrtc::AudioRecordJni::DataIsRecorded)}};
  jni->RegisterNatives(g_audio_record_class,
                       native_methods, arraysize(native_methods));
  CHECK_EXCEPTION(jni) << "Error during RegisterNatives";
}

void AudioRecordJni::ClearAndroidAudioDeviceObjects() {
  ALOGD("ClearAndroidAudioDeviceObjects%s", GetThreadInfo().c_str());
  JNIEnv* jni = GetEnv(g_jvm);
  CHECK(jni) << "AttachCurrentThread must be called on this tread";
  jni->UnregisterNatives(g_audio_record_class);
  CHECK_EXCEPTION(jni) << "Error during UnregisterNatives";
  DeleteGlobalRef(jni, g_audio_record_class);
  g_audio_record_class = NULL;
  DeleteGlobalRef(jni, g_context);
  g_context = NULL;
  g_jvm = NULL;
}

AudioRecordJni::AudioRecordJni(
    PlayoutDelayProvider* delay_provider, AudioManager* audio_manager)
    : delay_provider_(delay_provider),
      audio_parameters_(audio_manager->GetRecordAudioParameters()),
      j_audio_record_(NULL),
      direct_buffer_address_(NULL),
      direct_buffer_capacity_in_bytes_(0),
      frames_per_buffer_(0),
      initialized_(false),
      recording_(false),
      audio_device_buffer_(NULL),
      playout_delay_in_milliseconds_(0) {
  ALOGD("ctor%s", GetThreadInfo().c_str());
  DCHECK(audio_parameters_.is_valid());
  CHECK(HasDeviceObjects());
  CreateJavaInstance();
  // Detach from this thread since we want to use the checker to verify calls
  // from the Java based audio thread.
  thread_checker_java_.DetachFromThread();
}

AudioRecordJni::~AudioRecordJni() {
  ALOGD("~dtor%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jni->DeleteGlobalRef(j_audio_record_);
  j_audio_record_ = NULL;
}

int32_t AudioRecordJni::Init() {
  ALOGD("Init%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  return 0;
}

int32_t AudioRecordJni::Terminate() {
  ALOGD("Terminate%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  StopRecording();
  return 0;
}

int32_t AudioRecordJni::InitRecording() {
  ALOGD("InitRecording%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialized_);
  DCHECK(!recording_);
  if (initialized_ || recording_) {
    return -1;
  }
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID initRecordingID = GetMethodID(
      jni, g_audio_record_class, "InitRecording", "(II)I");
  jint frames_per_buffer = jni->CallIntMethod(
      j_audio_record_, initRecordingID, audio_parameters_.sample_rate(),
      audio_parameters_.channels());
  CHECK_EXCEPTION(jni);
  if (frames_per_buffer < 0) {
    ALOGE("InitRecording failed!");
    return -1;
  }
  frames_per_buffer_ = frames_per_buffer;
  ALOGD("frames_per_buffer: %d", frames_per_buffer_);
  CHECK_EQ(direct_buffer_capacity_in_bytes_,
           frames_per_buffer_ * kBytesPerFrame);
  CHECK_EQ(frames_per_buffer_, audio_parameters_.frames_per_buffer());
  initialized_ = true;
  return 0;
}

int32_t AudioRecordJni::StartRecording() {
  ALOGD("StartRecording%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  DCHECK(!recording_);
  if (!initialized_ || recording_) {
    return -1;
  }
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID startRecordingID = GetMethodID(
      jni, g_audio_record_class, "StartRecording", "()Z");
  jboolean res = jni->CallBooleanMethod(j_audio_record_, startRecordingID);
  CHECK_EXCEPTION(jni);
  if (!res) {
    ALOGE("StartRecording failed!");
    return -1;
  }
  recording_ = true;
  return 0;
}

int32_t AudioRecordJni::StopRecording() {
  ALOGD("StopRecording%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_ || !recording_) {
    return 0;
  }
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID stopRecordingID = GetMethodID(
      jni, g_audio_record_class, "StopRecording", "()Z");
  jboolean res = jni->CallBooleanMethod(j_audio_record_, stopRecordingID);
  CHECK_EXCEPTION(jni);
  if (!res) {
    ALOGE("StopRecording failed!");
    return -1;
  }
  // If we don't detach here, we will hit a DCHECK in OnDataIsRecorded() next
  // time StartRecording() is called since it will create a new Java thread.
  thread_checker_java_.DetachFromThread();
  initialized_ = false;
  recording_ = false;
  return 0;
}

int32_t AudioRecordJni::RecordingDelay(uint16_t& delayMS) const {  // NOLINT
  // TODO(henrika): is it possible to improve this estimate?
  delayMS = kHardwareDelayInMilliseconds;
  return 0;
}

void AudioRecordJni::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  ALOGD("AttachAudioBuffer");
  DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
  const int sample_rate_hz = audio_parameters_.sample_rate();
  ALOGD("SetRecordingSampleRate(%d)", sample_rate_hz);
  audio_device_buffer_->SetRecordingSampleRate(sample_rate_hz);
  const int channels = audio_parameters_.channels();
  ALOGD("SetRecordingChannels(%d)", channels);
  audio_device_buffer_->SetRecordingChannels(channels);
}

bool AudioRecordJni::BuiltInAECIsAvailable() const {
  ALOGD("BuiltInAECIsAvailable%s", GetThreadInfo().c_str());
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID builtInAECIsAvailable = jni->GetStaticMethodID(
      g_audio_record_class, "BuiltInAECIsAvailable", "()Z");
  CHECK_EXCEPTION(jni);
  CHECK(builtInAECIsAvailable);
  jboolean hw_aec = jni->CallStaticBooleanMethod(g_audio_record_class,
                                                 builtInAECIsAvailable);
  CHECK_EXCEPTION(jni);
  return hw_aec;
}

int32_t AudioRecordJni::EnableBuiltInAEC(bool enable) {
  ALOGD("EnableBuiltInAEC%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID enableBuiltInAEC = GetMethodID(
      jni, g_audio_record_class, "EnableBuiltInAEC", "(Z)Z");
  jboolean res = jni->CallBooleanMethod(
      j_audio_record_, enableBuiltInAEC, enable);
  CHECK_EXCEPTION(jni);
  if (!res) {
    ALOGE("EnableBuiltInAEC failed!");
    return -1;
  }
  return 0;
}

void JNICALL AudioRecordJni::CacheDirectBufferAddress(
    JNIEnv* env, jobject obj, jobject byte_buffer, jlong nativeAudioRecord) {
  webrtc::AudioRecordJni* this_object =
      reinterpret_cast<webrtc::AudioRecordJni*> (nativeAudioRecord);
  this_object->OnCacheDirectBufferAddress(env, byte_buffer);
}

void AudioRecordJni::OnCacheDirectBufferAddress(
    JNIEnv* env, jobject byte_buffer) {
  ALOGD("OnCacheDirectBufferAddress");
  DCHECK(thread_checker_.CalledOnValidThread());
  direct_buffer_address_ =
      env->GetDirectBufferAddress(byte_buffer);
  jlong capacity = env->GetDirectBufferCapacity(byte_buffer);
  ALOGD("direct buffer capacity: %lld", capacity);
  direct_buffer_capacity_in_bytes_ = static_cast<int> (capacity);
}

void JNICALL AudioRecordJni::DataIsRecorded(
  JNIEnv* env, jobject obj, jint length, jlong nativeAudioRecord) {
  webrtc::AudioRecordJni* this_object =
      reinterpret_cast<webrtc::AudioRecordJni*> (nativeAudioRecord);
  this_object->OnDataIsRecorded(length);
}

// This method is called on a high-priority thread from Java. The name of
// the thread is 'AudioRecordThread'.
void AudioRecordJni::OnDataIsRecorded(int length) {
  DCHECK(thread_checker_java_.CalledOnValidThread());
  if (!audio_device_buffer_) {
    ALOGE("AttachAudioBuffer has not been called!");
    return;
  }
  if (playout_delay_in_milliseconds_ == 0) {
    playout_delay_in_milliseconds_ = delay_provider_->PlayoutDelayMs();
    ALOGD("cached playout delay: %d", playout_delay_in_milliseconds_);
  }
  audio_device_buffer_->SetRecordedBuffer(direct_buffer_address_,
                                          frames_per_buffer_);
  audio_device_buffer_->SetVQEData(playout_delay_in_milliseconds_,
                                   kHardwareDelayInMilliseconds,
                                   0 /* clockDrift */);
  if (audio_device_buffer_->DeliverRecordedData() == 1) {
    ALOGE("AudioDeviceBuffer::DeliverRecordedData failed!");
  }
}

bool AudioRecordJni::HasDeviceObjects() {
  return (g_jvm && g_context && g_audio_record_class);
}

void AudioRecordJni::CreateJavaInstance() {
  ALOGD("CreateJavaInstance");
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID constructorID = GetMethodID(
      jni, g_audio_record_class, "<init>", "(Landroid/content/Context;J)V");
  j_audio_record_ = jni->NewObject(g_audio_record_class,
                                   constructorID,
                                   g_context,
                                   reinterpret_cast<intptr_t>(this));
  CHECK_EXCEPTION(jni) << "Error during NewObject";
  CHECK(j_audio_record_);
  j_audio_record_ = jni->NewGlobalRef(j_audio_record_);
  CHECK_EXCEPTION(jni) << "Error during NewGlobalRef";
  CHECK(j_audio_record_);
}

int32_t AudioRecordJni::RecordingDeviceName(uint16_t index,
                                            char name[kAdmMaxDeviceNameSize],
                                            char guid[kAdmMaxGuidSize]) {
  // Return empty string
  memset(name, 0, kAdmMaxDeviceNameSize);

  if (guid)
  {
    memset(guid, 0, kAdmMaxGuidSize);
  }

  return 0;
}

}  // namespace webrtc
