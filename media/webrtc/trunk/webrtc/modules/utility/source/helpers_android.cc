/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/modules/utility/interface/helpers_android.h"

#include <android/log.h>
#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <unistd.h>

#define TAG "HelpersAndroid"
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

namespace webrtc {

JNIEnv* GetEnv(JavaVM* jvm) {
  void* env = NULL;
  jint status = jvm->GetEnv(&env, JNI_VERSION_1_6);
  CHECK(((env != NULL) && (status == JNI_OK)) ||
        ((env == NULL) && (status == JNI_EDETACHED)))
      << "Unexpected GetEnv return: " << status << ":" << env;
  return reinterpret_cast<JNIEnv*>(env);
}

jmethodID GetMethodID (
    JNIEnv* jni, jclass c, const std::string& name, const char* signature) {
  jmethodID m = jni->GetMethodID(c, name.c_str(), signature);
  CHECK_EXCEPTION(jni) << "Error during GetMethodID: " << name << ", "
                       << signature;
  CHECK(m) << name << ", " << signature;
  return m;
}

jclass FindClass(JNIEnv* jni, const std::string& name) {
  jclass c = jni->FindClass(name.c_str());
  CHECK_EXCEPTION(jni) << "Error during FindClass: " << name;
  CHECK(c) << name;
  return c;
}

jobject NewGlobalRef(JNIEnv* jni, jobject o) {
  jobject ret = jni->NewGlobalRef(o);
  CHECK_EXCEPTION(jni) << "Error during NewGlobalRef";
  CHECK(ret);
  return ret;
}

void DeleteGlobalRef(JNIEnv* jni, jobject o) {
  jni->DeleteGlobalRef(o);
  CHECK_EXCEPTION(jni) << "Error during DeleteGlobalRef";
}

std::string GetThreadId() {
  char buf[21];  // Big enough to hold a kuint64max plus terminating NULL.
  int thread_id = gettid();
  CHECK_LT(snprintf(buf, sizeof(buf), "%i", thread_id),
      static_cast<int>(sizeof(buf))) << "Thread id is bigger than uint64??";
  return std::string(buf);
}

std::string GetThreadInfo() {
  return "@[tid=" + GetThreadId() + "]";
}

AttachThreadScoped::AttachThreadScoped(JavaVM* jvm)
    : attached_(false), jvm_(jvm), env_(NULL) {
  env_ = GetEnv(jvm);
  if (!env_) {
    // Adding debug log here so we can track down potential leaks and figure
    // out why we sometimes see "Native thread exiting without having called
    // DetachCurrentThread" in logcat outputs.
    ALOGD("Attaching thread to JVM%s", GetThreadInfo().c_str());
    jint res = jvm->AttachCurrentThread(&env_, NULL);
    attached_ = (res == JNI_OK);
    CHECK(attached_) << "AttachCurrentThread failed: " << res;
  }
}

AttachThreadScoped::~AttachThreadScoped() {
  if (attached_) {
    ALOGD("Detaching thread from JVM%s", GetThreadInfo().c_str());
    jint res = jvm_->DetachCurrentThread();
    CHECK(res == JNI_OK) << "DetachCurrentThread failed: " << res;
    CHECK(!GetEnv(jvm_));
  }
}

JNIEnv* AttachThreadScoped::env() { return env_; }

}  // namespace webrtc
