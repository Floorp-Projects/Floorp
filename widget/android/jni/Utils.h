/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jni_Utils_h__
#define mozilla_jni_Utils_h__

#include <jni.h>

#include "nsIRunnable.h"

#include "mozilla/UniquePtr.h"

#if defined(DEBUG) || !defined(RELEASE_OR_BETA)
#  define MOZ_CHECK_JNI
#endif

#ifdef MOZ_CHECK_JNI
#  include <unistd.h>
#  include "mozilla/Assertions.h"
#  include "APKOpen.h"
#  include "MainThreadUtils.h"
#endif

namespace mozilla {
namespace jni {

// How exception during a JNI call should be treated.
enum class ExceptionMode {
  // Abort on unhandled excepion (default).
  ABORT,
  // Ignore the exception and return to caller.
  IGNORE,
  // Catch any exception and return a nsresult.
  NSRESULT,
};

// Thread that a particular JNI call is allowed on.
enum class CallingThread {
  // Can be called from any thread (default).
  ANY,
  // Can be called from the Gecko thread.
  GECKO,
  // Can be called from the Java UI thread.
  UI,
};

// If and where a JNI call will be dispatched.
enum class DispatchTarget {
  // Call happens synchronously on the calling thread (default).
  CURRENT,
  // Call happens synchronously on the calling thread, but the call is
  // wrapped in a function object and is passed thru UsesNativeCallProxy.
  // Method must return void.
  PROXY,
  // Call is dispatched asynchronously on the Gecko thread to the XPCOM
  // (nsThread) event queue. Method must return void.
  GECKO,
  // Call is dispatched asynchronously on the Gecko thread to the widget
  // (nsAppShell) event queue. In most cases, events in the widget event
  // queue (aka native event queue) are favored over events in the XPCOM
  // event queue. Method must return void.
  GECKO_PRIORITY,
};

extern JavaVM* sJavaVM;
extern JNIEnv* sGeckoThreadEnv;

inline bool IsAvailable() { return !!sGeckoThreadEnv; }

inline JavaVM* GetVM() {
#ifdef MOZ_CHECK_JNI
  MOZ_ASSERT(sJavaVM);
#endif
  return sJavaVM;
}

inline JNIEnv* GetGeckoThreadEnv() {
#ifdef MOZ_CHECK_JNI
  MOZ_RELEASE_ASSERT(NS_IsMainThread(), "Must be on Gecko thread");
  MOZ_RELEASE_ASSERT(sGeckoThreadEnv, "Must have a JNIEnv");
#endif
  return sGeckoThreadEnv;
}

void SetGeckoThreadEnv(JNIEnv* aEnv);

JNIEnv* GetEnvForThread();

#ifdef MOZ_CHECK_JNI
#  define MOZ_ASSERT_JNI_THREAD(thread)                            \
    do {                                                           \
      if ((thread) == mozilla::jni::CallingThread::GECKO) {        \
        MOZ_RELEASE_ASSERT(::NS_IsMainThread());                   \
      } else if ((thread) == mozilla::jni::CallingThread::UI) {    \
        const bool isOnUiThread = (GetUIThreadId() == ::gettid()); \
        MOZ_RELEASE_ASSERT(isOnUiThread);                          \
      }                                                            \
    } while (0)
#else
#  define MOZ_ASSERT_JNI_THREAD(thread) \
    do {                                \
    } while (0)
#endif

bool ThrowException(JNIEnv* aEnv, const char* aClass, const char* aMessage);

inline bool ThrowException(JNIEnv* aEnv, const char* aMessage) {
  return ThrowException(aEnv, "java/lang/Exception", aMessage);
}

inline bool ThrowException(const char* aClass, const char* aMessage) {
  return ThrowException(GetEnvForThread(), aClass, aMessage);
}

inline bool ThrowException(const char* aMessage) {
  return ThrowException(GetEnvForThread(), aMessage);
}

bool HandleUncaughtException(JNIEnv* aEnv);

bool ReportException(JNIEnv* aEnv, jthrowable aExc, jstring aStack);

#define MOZ_CATCH_JNI_EXCEPTION(env)                    \
  do {                                                  \
    if (mozilla::jni::HandleUncaughtException((env))) { \
      MOZ_CRASH("JNI exception");                       \
    }                                                   \
  } while (0)

uintptr_t GetNativeHandle(JNIEnv* env, jobject instance);

void SetNativeHandle(JNIEnv* env, jobject instance, uintptr_t handle);

jclass GetClassRef(JNIEnv* aEnv, const char* aClassName);

void DispatchToGeckoPriorityQueue(already_AddRefed<nsIRunnable> aCall);

int GetAPIVersion();

pid_t GetUIThreadId();

bool IsOOMException(JNIEnv* aEnv);

}  // namespace jni
}  // namespace mozilla

#endif  // mozilla_jni_Utils_h__
