/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <dlfcn.h>
#include <prthread.h>

#include "mozilla/DebugOnly.h"
#include "mozilla/Assertions.h"
#include "mozilla/SyncRunnable.h"
#include "nsThreadUtils.h"
#include "AndroidBridge.h"

extern "C" {
  jclass __jsjni_GetGlobalClassRef(const char *className);
}

class GetGlobalClassRefRunnable : public mozilla::Runnable {
  public:
    GetGlobalClassRefRunnable(const char *className, jclass *foundClass) :
        mClassName(className), mResult(foundClass) {}
    NS_IMETHOD Run() override {
        *mResult = __jsjni_GetGlobalClassRef(mClassName);
        return NS_OK;
    }
  private:
    const char *mClassName;
    jclass *mResult;
};

extern "C" {
  __attribute__ ((visibility("default")))
  jclass
  jsjni_FindClass(const char *className) {
    // FindClass outside the main thread will run into problems due
    // to missing the classpath
    MOZ_ASSERT(NS_IsMainThread());
    JNIEnv *env = mozilla::jni::GetGeckoThreadEnv();
    return env->FindClass(className);
  }

  jclass
  __jsjni_GetGlobalClassRef(const char *className) {
    // root class globally
    JNIEnv *env = mozilla::jni::GetGeckoThreadEnv();
    jclass globalRef = static_cast<jclass>(env->NewGlobalRef(env->FindClass(className)));
    if (!globalRef)
      return nullptr;

    // return the newly create global reference
    return globalRef;
  }

  __attribute__ ((visibility("default")))
  jclass
  jsjni_GetGlobalClassRef(const char *className) {
    if (NS_IsMainThread()) {
      return __jsjni_GetGlobalClassRef(className);
    }

    nsCOMPtr<nsIThread> mainThread;
    mozilla::DebugOnly<nsresult> rv = NS_GetMainThread(getter_AddRefs(mainThread));
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    jclass foundClass;
    nsCOMPtr<nsIRunnable> runnable_ref(new GetGlobalClassRefRunnable(className,
                                                                     &foundClass));
    RefPtr<mozilla::SyncRunnable> sr = new mozilla::SyncRunnable(runnable_ref);
    sr->DispatchToThread(mainThread);
    if (!foundClass)
      return nullptr;

    return foundClass;
  }

  __attribute__ ((visibility("default")))
  jmethodID
  jsjni_GetStaticMethodID(jclass methodClass,
                          const char *methodName,
                          const char *signature) {
    JNIEnv *env = mozilla::jni::GetGeckoThreadEnv();
    return env->GetStaticMethodID(methodClass, methodName, signature);
  }

  __attribute__ ((visibility("default")))
  bool
  jsjni_ExceptionCheck() {
    JNIEnv *env = mozilla::jni::GetGeckoThreadEnv();
    return env->ExceptionCheck();
  }

  __attribute__ ((visibility("default")))
  void
  jsjni_CallStaticVoidMethodA(jclass cls,
                              jmethodID method,
                              jvalue *values) {
    JNIEnv *env = mozilla::jni::GetGeckoThreadEnv();

    mozilla::AutoLocalJNIFrame jniFrame(env);
    env->CallStaticVoidMethodA(cls, method, values);
  }

  __attribute__ ((visibility("default")))
  int
  jsjni_CallStaticIntMethodA(jclass cls,
                             jmethodID method,
                             jvalue *values) {
    JNIEnv *env = mozilla::jni::GetGeckoThreadEnv();

    mozilla::AutoLocalJNIFrame jniFrame(env);
    return env->CallStaticIntMethodA(cls, method, values);
  }

  __attribute__ ((visibility("default")))
  jobject jsjni_GetGlobalContextRef() {
    return mozilla::AndroidBridge::Bridge()->GetGlobalContextRef();
  }

  __attribute__ ((visibility("default")))
  JavaVM* jsjni_GetVM() {
    JavaVM* jvm;
    JNIEnv* const env = mozilla::jni::GetGeckoThreadEnv();
    MOZ_ALWAYS_TRUE(!env->GetJavaVM(&jvm));
    return jvm;
  }

  __attribute__ ((visibility("default")))
  JNIEnv* jsjni_GetJNIForThread() {
    return mozilla::jni::GetEnvForThread();
  }

  // For compatibility with JNI.jsm; some addons bundle their own JNI.jsm,
  // so we cannot just change the function name used in JNI.jsm.
  __attribute__ ((visibility("default")))
  JNIEnv* GetJNIForThread() {
    return mozilla::jni::GetEnvForThread();
  }
}
