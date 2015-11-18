/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/audio_manager.h"

#include <android/log.h>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/utility/interface/helpers_android.h"

#define TAG "AudioManager"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

namespace webrtc {

static JavaVM* g_jvm = NULL;
static jobject g_context = NULL;
static jclass g_audio_manager_class = NULL;

void AudioManager::SetAndroidAudioDeviceObjects(void* jvm, void* context) {
  ALOGD("SetAndroidAudioDeviceObjects%s", GetThreadInfo().c_str());

  CHECK(jvm);
  CHECK(context);

  g_jvm = reinterpret_cast<JavaVM*>(jvm);
  JNIEnv* jni = GetEnv(g_jvm);
  CHECK(jni) << "AttachCurrentThread must be called on this tread";

  g_context = NewGlobalRef(jni, reinterpret_cast<jobject>(context));
  jclass local_class = FindClass(
      jni, "org/webrtc/voiceengine/WebRtcAudioManager");
  g_audio_manager_class = reinterpret_cast<jclass>(
      NewGlobalRef(jni, local_class));
  CHECK_EXCEPTION(jni);

  // Register native methods with the WebRtcAudioManager class. These methods
  // are declared private native in WebRtcAudioManager.java.
  JNINativeMethod native_methods[] = {
      {"nativeCacheAudioParameters", "(IIJ)V",
       reinterpret_cast<void*>(&webrtc::AudioManager::CacheAudioParameters)}};
  jni->RegisterNatives(g_audio_manager_class,
                       native_methods, arraysize(native_methods));
  CHECK_EXCEPTION(jni) << "Error during RegisterNatives";
}

void AudioManager::ClearAndroidAudioDeviceObjects() {
  ALOGD("ClearAndroidAudioDeviceObjects%s", GetThreadInfo().c_str());
  JNIEnv* jni = GetEnv(g_jvm);
  CHECK(jni) << "AttachCurrentThread must be called on this tread";
  jni->UnregisterNatives(g_audio_manager_class);
  CHECK_EXCEPTION(jni) << "Error during UnregisterNatives";
  DeleteGlobalRef(jni, g_audio_manager_class);
  g_audio_manager_class = NULL;
  DeleteGlobalRef(jni, g_context);
  g_context = NULL;
  g_jvm = NULL;
}

AudioManager::AudioManager()
    : j_audio_manager_(NULL),
      initialized_(false) {
  ALOGD("ctor%s", GetThreadInfo().c_str());
  CHECK(HasDeviceObjects());
  CreateJavaInstance();
}

AudioManager::~AudioManager() {
  ALOGD("~dtor%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  Close();
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jni->DeleteGlobalRef(j_audio_manager_);
  j_audio_manager_ = NULL;
  DCHECK(!initialized_);
}

bool AudioManager::Init() {
  ALOGD("Init%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialized_);
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID initID = GetMethodID(jni, g_audio_manager_class, "init", "()Z");
  jboolean res = jni->CallBooleanMethod(j_audio_manager_, initID);
  CHECK_EXCEPTION(jni);
  if (!res) {
    ALOGE("init failed!");
    return false;
  }
  initialized_ = true;
  return true;
}

bool AudioManager::Close() {
  ALOGD("Close%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_)
    return true;
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID disposeID = GetMethodID(
      jni, g_audio_manager_class, "dispose", "()V");
  jni->CallVoidMethod(j_audio_manager_, disposeID);
  CHECK_EXCEPTION(jni);
  initialized_ = false;
  return true;
}

void JNICALL AudioManager::CacheAudioParameters(JNIEnv* env, jobject obj,
    jint sample_rate, jint channels, jlong nativeAudioManager) {
  webrtc::AudioManager* this_object =
      reinterpret_cast<webrtc::AudioManager*> (nativeAudioManager);
  this_object->OnCacheAudioParameters(env, sample_rate, channels);
}

void AudioManager::OnCacheAudioParameters(
    JNIEnv* env, jint sample_rate, jint channels) {
  ALOGD("OnCacheAudioParameters%s", GetThreadInfo().c_str());
  ALOGD("sample_rate: %d", sample_rate);
  ALOGD("channels: %d", channels);
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(henrika): add support stereo output.
  playout_parameters_.reset(sample_rate, channels);
  record_parameters_.reset(sample_rate, channels);
}

AudioParameters AudioManager::GetPlayoutAudioParameters() const {
  CHECK(playout_parameters_.is_valid());
  return playout_parameters_;
}

AudioParameters AudioManager::GetRecordAudioParameters() const {
  CHECK(record_parameters_.is_valid());
  return record_parameters_;
}

bool AudioManager::HasDeviceObjects() {
  return (g_jvm && g_context && g_audio_manager_class);
}

void AudioManager::CreateJavaInstance() {
  ALOGD("CreateJavaInstance");
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID constructorID = GetMethodID(
      jni, g_audio_manager_class, "<init>", "(Landroid/content/Context;J)V");
  j_audio_manager_ = jni->NewObject(g_audio_manager_class,
                                    constructorID,
                                    g_context,
                                    reinterpret_cast<intptr_t>(this));
  CHECK_EXCEPTION(jni) << "Error during NewObject";
  CHECK(j_audio_manager_);
  j_audio_manager_ = jni->NewGlobalRef(j_audio_manager_);
  CHECK_EXCEPTION(jni) << "Error during NewGlobalRef";
  CHECK(j_audio_manager_);
}

}  // namespace webrtc
