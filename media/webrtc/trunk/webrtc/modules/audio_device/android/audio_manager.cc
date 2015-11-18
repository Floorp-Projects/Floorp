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
#if !defined(MOZ_WIDGET_GONK)
#include "AndroidJNIWrapper.h"
#endif

#include <android/log.h>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_device/android/audio_common.h"
#if !defined(MOZ_WIDGET_GONK)
#include "webrtc/modules/utility/interface/helpers_android.h"
#endif

#define TAG "AudioManager"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

namespace webrtc {

#if !defined(MOZ_WIDGET_GONK)
static JavaVM* g_jvm = NULL;
static jobject g_context = NULL;
static jclass g_audio_manager_class = NULL;
#endif

void AudioManager::SetAndroidAudioDeviceObjects(void* jvm, void* context) {
#if !defined(MOZ_WIDGET_GONK)
  ALOGD("SetAndroidAudioDeviceObjects%s", GetThreadInfo().c_str());

  CHECK(jvm);
  CHECK(context);

  g_jvm = reinterpret_cast<JavaVM*>(jvm);
  JNIEnv* jni = GetEnv(g_jvm);
  CHECK(jni) << "AttachCurrentThread must be called on this tread";

  if (!g_context) {
    g_context = NewGlobalRef(jni, reinterpret_cast<jobject>(context));
  }

  if (!g_audio_manager_class) {
    g_audio_manager_class = jsjni_GetGlobalClassRef(
                                "org/webrtc/voiceengine/WebRtcAudioManager");
    DCHECK(g_audio_manager_class);
  }
  // Register native methods with the WebRtcAudioManager class. These methods
  // are declared private native in WebRtcAudioManager.java.
  JNINativeMethod native_methods[] = {
      {"nativeCacheAudioParameters", "(IIJ)V",
       reinterpret_cast<void*>(&webrtc::AudioManager::CacheAudioParameters)}};
  jni->RegisterNatives(g_audio_manager_class,
                       native_methods, arraysize(native_methods));
  CHECK_EXCEPTION(jni) << "Error during RegisterNatives";
#endif
}

void AudioManager::ClearAndroidAudioDeviceObjects() {
#if !defined(MOZ_WIDGET_GONK)
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
#endif
}

AudioManager::AudioManager()
    : initialized_(false) {
#if !defined(MOZ_WIDGET_GONK)
  j_audio_manager_ = NULL;
  ALOGD("ctor%s", GetThreadInfo().c_str());
#endif
  CHECK(HasDeviceObjects());
  CreateJavaInstance();
}

AudioManager::~AudioManager() {
#if !defined(MOZ_WIDGET_GONK)
  ALOGD("~dtor%s", GetThreadInfo().c_str());
#endif
  DCHECK(thread_checker_.CalledOnValidThread());
  Close();
#if !defined(MOZ_WIDGET_GONK)
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jni->DeleteGlobalRef(j_audio_manager_);
  j_audio_manager_ = NULL;
#endif
  DCHECK(!initialized_);
}

bool AudioManager::Init() {
#if !defined(MOZ_WIDGET_GONK)
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
#endif
  initialized_ = true;
  return true;
}

bool AudioManager::Close() {
#if !defined(MOZ_WIDGET_GONK)
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
#endif
  initialized_ = false;
  return true;
}

#if !defined(MOZ_WIDGET_GONK)
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
#endif

AudioParameters AudioManager::GetPlayoutAudioParameters() const {
  CHECK(playout_parameters_.is_valid());
  return playout_parameters_;
}

AudioParameters AudioManager::GetRecordAudioParameters() const {
  CHECK(record_parameters_.is_valid());
  return record_parameters_;
}

bool AudioManager::HasDeviceObjects() {
#if !defined(MOZ_WIDGET_GONK)
  return (g_jvm && g_context && g_audio_manager_class);
#else
  return true;
#endif
}

void AudioManager::CreateJavaInstance() {
#if !defined(MOZ_WIDGET_GONK)
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
#endif
}

}  // namespace webrtc
