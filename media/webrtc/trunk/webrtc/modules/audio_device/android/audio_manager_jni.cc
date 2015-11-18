/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/audio_manager_jni.h"

#include <android/log.h>
#include <assert.h>

#include "webrtc/modules/utility/interface/helpers_android.h"
#include "webrtc/system_wrappers/interface/trace.h"

#define TAG "AudioManagerJni"
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

namespace webrtc {

static JavaVM* g_jvm_ = NULL;
static JNIEnv* g_jni_env_ = NULL;
static jobject g_context_ = NULL;
static jclass g_audio_manager_class_ = NULL;
static jobject g_audio_manager_ = NULL;

AudioManagerJni::AudioManagerJni()
    : low_latency_supported_(false),
      native_output_sample_rate_(0),
      native_buffer_size_(0) {
  if (!HasDeviceObjects()) {
    assert(false);
  }
  AttachThreadScoped ats(g_jvm_);
  JNIEnv* env = ats.env();
  assert(env && "Unsupported JNI version!");
  CreateInstance(env);
  // Pre-store device specific values.
  SetLowLatencySupported(env);
  SetNativeOutputSampleRate(env);
  SetNativeFrameSize(env);
}

void AudioManagerJni::SetAndroidAudioDeviceObjects(void* jvm, void* context) {
  ALOGD("SetAndroidAudioDeviceObjects%s", GetThreadInfo().c_str());

  assert(jvm);
  assert(context);

  // Store global Java VM variables to be accessed by API calls.
  g_jvm_ = reinterpret_cast<JavaVM*>(jvm);
  g_jni_env_ = GetEnv(g_jvm_);
  g_context_ = g_jni_env_->NewGlobalRef(reinterpret_cast<jobject>(context));

  // FindClass must be made in this function since this function's contract
  // requires it to be called by a Java thread.
  // See
  // http://developer.android.com/training/articles/perf-jni.html#faq_FindClass
  // as to why this is necessary.
  // Get the AudioManagerAndroid class object.
  jclass javaAmClassLocal = g_jni_env_->FindClass(
      "org/webrtc/voiceengine/AudioManagerAndroid");
  assert(javaAmClassLocal);

  // Create a global reference such that the class object is not recycled by
  // the garbage collector.
  g_audio_manager_class_ = reinterpret_cast<jclass>(
      g_jni_env_->NewGlobalRef(javaAmClassLocal));
  assert(g_audio_manager_class_);
}

void AudioManagerJni::ClearAndroidAudioDeviceObjects() {
  ALOGD("ClearAndroidAudioDeviceObjects%s", GetThreadInfo().c_str());
  g_jni_env_->DeleteGlobalRef(g_audio_manager_class_);
  g_audio_manager_class_ = NULL;
  g_jni_env_->DeleteGlobalRef(g_context_);
  g_context_ = NULL;
  g_jni_env_->DeleteGlobalRef(g_audio_manager_);
  g_audio_manager_ = NULL;
  g_jni_env_ = NULL;
  g_jvm_ = NULL;
}

void AudioManagerJni::SetLowLatencySupported(JNIEnv* env) {
  jmethodID id = LookUpMethodId(env, "isAudioLowLatencySupported", "()Z");
  low_latency_supported_ = env->CallBooleanMethod(g_audio_manager_, id);
}

void AudioManagerJni::SetNativeOutputSampleRate(JNIEnv* env) {
  jmethodID id = LookUpMethodId(env, "getNativeOutputSampleRate", "()I");
  native_output_sample_rate_ = env->CallIntMethod(g_audio_manager_, id);
}

void AudioManagerJni::SetNativeFrameSize(JNIEnv* env) {
  jmethodID id = LookUpMethodId(env,
                                "getAudioLowLatencyOutputFrameSize", "()I");
  native_buffer_size_ = env->CallIntMethod(g_audio_manager_, id);
}

bool AudioManagerJni::HasDeviceObjects() {
  return g_jvm_ && g_jni_env_ && g_context_ && g_audio_manager_class_;
}

jmethodID AudioManagerJni::LookUpMethodId(JNIEnv* env,
                                          const char* method_name,
                                          const char* method_signature) {
  jmethodID ret_val = env->GetMethodID(g_audio_manager_class_, method_name,
                                       method_signature);
  assert(ret_val);
  return ret_val;
}

void AudioManagerJni::CreateInstance(JNIEnv* env) {
  // Get the method ID for the constructor taking Context.
  jmethodID id = LookUpMethodId(env, "<init>", "(Landroid/content/Context;)V");
  g_audio_manager_ = env->NewObject(g_audio_manager_class_, id, g_context_);
  // Create a global reference so that the instance is accessible until no
  // longer needed.
  g_audio_manager_ = env->NewGlobalRef(g_audio_manager_);
  assert(g_audio_manager_);
}

}  // namespace webrtc
