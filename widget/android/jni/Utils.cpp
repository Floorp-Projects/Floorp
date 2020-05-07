/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Utils.h"
#include "Types.h"

#include <android/log.h>
#include <pthread.h>

#include "mozilla/Assertions.h"

#include "GeneratedJNIWrappers.h"
#include "AndroidBuild.h"
#include "nsAppShell.h"
#include "nsExceptionHandler.h"

namespace mozilla {
namespace jni {

namespace detail {

#define DEFINE_PRIMITIVE_TYPE_ADAPTER(NativeType, JNIType, JNIName, ABIName)   \
                                                                               \
  constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::Call)(                  \
      jobject, jmethodID, CallArgs::JValueType) MOZ_JNICALL_ABI;               \
  constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::StaticCall)(            \
      jclass, jmethodID, CallArgs::JValueType) MOZ_JNICALL_ABI;                \
  constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::Get)(jobject, jfieldID) \
      ABIName;                                                                 \
  constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::StaticGet)(             \
      jclass, jfieldID) ABIName;                                               \
  constexpr void (JNIEnv::*TypeAdapter<NativeType>::Set)(jobject, jfieldID,    \
                                                         JNIType) ABIName;     \
  constexpr void (JNIEnv::*TypeAdapter<NativeType>::StaticSet)(                \
      jclass, jfieldID, JNIType) ABIName;                                      \
  constexpr void (JNIEnv::*TypeAdapter<NativeType>::GetArray)(                 \
      JNIType##Array, jsize, jsize, JNIType*)

DEFINE_PRIMITIVE_TYPE_ADAPTER(bool, jboolean, Boolean, /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int8_t, jbyte, Byte, /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(char16_t, jchar, Char, /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int16_t, jshort, Short, /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int32_t, jint, Int, /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int64_t, jlong, Long, /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(float, jfloat, Float, MOZ_JNICALL_ABI);
DEFINE_PRIMITIVE_TYPE_ADAPTER(double, jdouble, Double, MOZ_JNICALL_ABI);

#undef DEFINE_PRIMITIVE_TYPE_ADAPTER

}  // namespace detail

template <>
const char ObjectBase<Object, jobject>::name[] = "java/lang/Object";
template <>
const char ObjectBase<TypedObject<jstring>, jstring>::name[] =
    "java/lang/String";
template <>
const char ObjectBase<TypedObject<jclass>, jclass>::name[] = "java/lang/Class";
template <>
const char ObjectBase<TypedObject<jthrowable>, jthrowable>::name[] =
    "java/lang/Throwable";
template <>
const char ObjectBase<BoxedObject<jboolean>, jobject>::name[] =
    "java/lang/Boolean";
template <>
const char ObjectBase<BoxedObject<jbyte>, jobject>::name[] = "java/lang/Byte";
template <>
const char ObjectBase<BoxedObject<jchar>, jobject>::name[] =
    "java/lang/Character";
template <>
const char ObjectBase<BoxedObject<jshort>, jobject>::name[] = "java/lang/Short";
template <>
const char ObjectBase<BoxedObject<jint>, jobject>::name[] = "java/lang/Integer";
template <>
const char ObjectBase<BoxedObject<jlong>, jobject>::name[] = "java/lang/Long";
template <>
const char ObjectBase<BoxedObject<jfloat>, jobject>::name[] = "java/lang/Float";
template <>
const char ObjectBase<BoxedObject<jdouble>, jobject>::name[] =
    "java/lang/Double";
template <>
const char ObjectBase<TypedObject<jbooleanArray>, jbooleanArray>::name[] = "[Z";
template <>
const char ObjectBase<TypedObject<jbyteArray>, jbyteArray>::name[] = "[B";
template <>
const char ObjectBase<TypedObject<jcharArray>, jcharArray>::name[] = "[C";
template <>
const char ObjectBase<TypedObject<jshortArray>, jshortArray>::name[] = "[S";
template <>
const char ObjectBase<TypedObject<jintArray>, jintArray>::name[] = "[I";
template <>
const char ObjectBase<TypedObject<jlongArray>, jlongArray>::name[] = "[J";
template <>
const char ObjectBase<TypedObject<jfloatArray>, jfloatArray>::name[] = "[F";
template <>
const char ObjectBase<TypedObject<jdoubleArray>, jdoubleArray>::name[] = "[D";
template <>
const char ObjectBase<TypedObject<jobjectArray>, jobjectArray>::name[] =
    "[Ljava/lang/Object;";
template <>
const char ObjectBase<ByteBuffer, jobject>::name[] = "java/nio/ByteBuffer";

JavaVM* sJavaVM;
JNIEnv* sGeckoThreadEnv;

namespace {

pthread_key_t sThreadEnvKey;
jclass sOOMErrorClass;
jobject sClassLoader;
jmethodID sClassLoaderLoadClass;

void UnregisterThreadEnv(void* env) {
  if (!env) {
    // We were never attached.
    return;
  }
  // The thread may have already been detached. In that case, it's still
  // okay to call DetachCurrentThread(); it'll simply return an error.
  // However, we must not access | env | because it may be invalid.
  MOZ_ASSERT(sJavaVM);
  sJavaVM->DetachCurrentThread();
}

}  // namespace

void SetGeckoThreadEnv(JNIEnv* aEnv) {
  MOZ_ASSERT(aEnv);
  MOZ_ASSERT(!sGeckoThreadEnv || sGeckoThreadEnv == aEnv);

  if (!sGeckoThreadEnv &&
      pthread_key_create(&sThreadEnvKey, UnregisterThreadEnv)) {
    MOZ_CRASH("Failed to initialize required TLS");
  }

  sGeckoThreadEnv = aEnv;
  MOZ_ALWAYS_TRUE(!pthread_setspecific(sThreadEnvKey, aEnv));

  MOZ_ALWAYS_TRUE(!aEnv->GetJavaVM(&sJavaVM));
  MOZ_ASSERT(sJavaVM);

  sOOMErrorClass =
      Class::GlobalRef(
          Class::LocalRef::Adopt(aEnv->FindClass("java/lang/OutOfMemoryError")))
          .Forget();
  aEnv->ExceptionClear();

  sClassLoader = Object::GlobalRef(java::GeckoThread::ClsLoader()).Forget();
  sClassLoaderLoadClass = aEnv->GetMethodID(
      Class::LocalRef::Adopt(aEnv->GetObjectClass(sClassLoader)).Get(),
      "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
  MOZ_ASSERT(sClassLoader && sClassLoaderLoadClass);
}

JNIEnv* GetEnvForThread() {
  MOZ_ASSERT(sGeckoThreadEnv);

  JNIEnv* env = static_cast<JNIEnv*>(pthread_getspecific(sThreadEnvKey));
  if (env) {
    return env;
  }

  // We don't have a saved JNIEnv, so try to get one.
  // AttachCurrentThread() does the same thing as GetEnv() when a thread is
  // already attached, so we don't have to call GetEnv() at all.
  if (!sJavaVM->AttachCurrentThread(&env, nullptr)) {
    MOZ_ASSERT(env);
    MOZ_ALWAYS_TRUE(!pthread_setspecific(sThreadEnvKey, env));
    return env;
  }

  MOZ_CRASH("Failed to get JNIEnv for thread");
  return nullptr;  // unreachable
}

bool ThrowException(JNIEnv* aEnv, const char* aClass, const char* aMessage) {
  MOZ_ASSERT(aEnv, "Invalid thread JNI env");

  Class::LocalRef cls = Class::LocalRef::Adopt(aEnv->FindClass(aClass));
  MOZ_ASSERT(cls, "Cannot find exception class");

  return !aEnv->ThrowNew(cls.Get(), aMessage);
}

bool HandleUncaughtException(JNIEnv* aEnv) {
  MOZ_ASSERT(aEnv, "Invalid thread JNI env");

  if (!aEnv->ExceptionCheck()) {
    return false;
  }

#ifdef MOZ_CHECK_JNI
  aEnv->ExceptionDescribe();
#endif

  Throwable::LocalRef e =
      Throwable::LocalRef::Adopt(aEnv, aEnv->ExceptionOccurred());
  MOZ_ASSERT(e);
  aEnv->ExceptionClear();

  String::LocalRef stack = java::GeckoAppShell::GetExceptionStackTrace(e);
  if (stack && ReportException(aEnv, e.Get(), stack.Get())) {
    return true;
  }

  aEnv->ExceptionClear();
  java::GeckoAppShell::HandleUncaughtException(e);

  if (NS_WARN_IF(aEnv->ExceptionCheck())) {
    aEnv->ExceptionDescribe();
    aEnv->ExceptionClear();
  }

  return true;
}

bool ReportException(JNIEnv* aEnv, jthrowable aExc, jstring aStack) {
  bool result = true;

  result &= NS_SUCCEEDED(CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::JavaStackTrace,
      String::Ref::From(aStack)->ToCString()));

  auto appNotes = java::GeckoAppShell::GetAppNotes();
  if (NS_WARN_IF(aEnv->ExceptionCheck())) {
    aEnv->ExceptionDescribe();
    aEnv->ExceptionClear();
  } else if (appNotes) {
    CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("\n") +
                                               appNotes->ToCString());
  }

  if (sOOMErrorClass && aEnv->IsInstanceOf(aExc, sOOMErrorClass)) {
    NS_ABORT_OOM(0);  // Unknown OOM size
  }
  return result;
}

namespace {

jclass sJNIObjectClass;
jfieldID sJNIObjectHandleField;

bool EnsureJNIObject(JNIEnv* env, jobject instance) {
  if (!sJNIObjectClass) {
    sJNIObjectClass =
        Class::GlobalRef(Class::LocalRef::Adopt(GetClassRef(
                             env, "org/mozilla/gecko/mozglue/JNIObject")))
            .Forget();

    sJNIObjectHandleField = env->GetFieldID(sJNIObjectClass, "mHandle", "J");
  }

  MOZ_ASSERT(env->IsInstanceOf(instance, sJNIObjectClass),
             "Java class is not derived from JNIObject");
  return true;
}

}  // namespace

uintptr_t GetNativeHandle(JNIEnv* env, jobject instance) {
  if (!EnsureJNIObject(env, instance)) {
    return 0;
  }

  return static_cast<uintptr_t>(
      env->GetLongField(instance, sJNIObjectHandleField));
}

void SetNativeHandle(JNIEnv* env, jobject instance, uintptr_t handle) {
  if (!EnsureJNIObject(env, instance)) {
    return;
  }

  env->SetLongField(instance, sJNIObjectHandleField,
                    static_cast<jlong>(handle));
}

jclass GetClassRef(JNIEnv* aEnv, const char* aClassName) {
  // First try the default class loader.
  auto classRef = Class::LocalRef::Adopt(aEnv, aEnv->FindClass(aClassName));

  if ((!classRef || aEnv->ExceptionCheck()) && sClassLoader) {
    // If the default class loader failed but we have an app class loader, try
    // that. Clear the pending exception from failed FindClass call above.
    aEnv->ExceptionClear();
    classRef = Class::LocalRef::Adopt(
        aEnv,
        jclass(aEnv->CallObjectMethod(sClassLoader, sClassLoaderLoadClass,
                                      StringParam(aClassName, aEnv).Get())));
  }

  if (classRef && !aEnv->ExceptionCheck()) {
    return classRef.Forget();
  }

  __android_log_print(
      ANDROID_LOG_ERROR, "Gecko",
      ">>> FATAL JNI ERROR! FindClass(\"%s\") failed. "
      "Does the class require a newer API version? "
      "Or did ProGuard optimize away something it shouldn't have?",
      aClassName);
  aEnv->ExceptionDescribe();
  MOZ_CRASH("Cannot find JNI class");
  return nullptr;
}

void DispatchToGeckoPriorityQueue(already_AddRefed<nsIRunnable> aCall) {
  class RunnableEvent : public nsAppShell::Event {
    nsCOMPtr<nsIRunnable> mCall;

   public:
    explicit RunnableEvent(already_AddRefed<nsIRunnable> aCall)
        : mCall(aCall) {}
    void Run() override { NS_ENSURE_SUCCESS_VOID(mCall->Run()); }
  };

  nsAppShell::PostEvent(MakeUnique<RunnableEvent>(std::move(aCall)));
}

int GetAPIVersion() {
  static int32_t apiVersion = 0;
  if (!apiVersion && IsAvailable()) {
    apiVersion = java::sdk::VERSION::SDK_INT();
  }
  return apiVersion;
}

pid_t GetUIThreadId() {
  static pid_t uiThreadId;
  if (!uiThreadId) {
    uiThreadId = pid_t(java::GeckoThread::UiThreadId());
  }
  return uiThreadId;
}

}  // namespace jni
}  // namespace mozilla
