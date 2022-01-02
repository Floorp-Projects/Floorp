/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jni_Types_h__
#define mozilla_jni_Types_h__

#include <jni.h>

#include "mozilla/jni/Refs.h"
#include "mozilla/jni/TypeAdapter.h"

namespace mozilla {
namespace jni {
namespace detail {

// TypeAdapter specializations are the interfaces between native/C++ types such
// as int32_t and JNI types such as jint. The template parameter T is the native
// type, and each TypeAdapter specialization can have the following members:
//
//  * Call: JNIEnv member pointer for making a method call that returns T.
//  * StaticCall: JNIEnv member pointer for making a static call that returns T.
//  * Get: JNIEnv member pointer for getting a field of type T.
//  * StaticGet: JNIEnv member pointer for getting a static field of type T.
//  * Set: JNIEnv member pointer for setting a field of type T.
//  * StaticGet: JNIEnv member pointer for setting a static field of type T.
//  * ToNative: static function that converts the JNI type to the native type.
//  * FromNative: static function that converts the native type to the JNI type.

// TypeAdapter<LocalRef<Cls>> applies when jobject is a return value.
template <class Cls>
struct TypeAdapter<LocalRef<Cls>> {
  using JNIType = typename Cls::Ref::JNIType;

  static constexpr auto Call = &JNIEnv::CallObjectMethodA;
  static constexpr auto StaticCall = &JNIEnv::CallStaticObjectMethodA;
  static constexpr auto Get = &JNIEnv::GetObjectField;
  static constexpr auto StaticGet = &JNIEnv::GetStaticObjectField;

  // Declare instance as jobject because JNI methods return
  // jobject even if the return value is really jstring, etc.
  static LocalRef<Cls> ToNative(JNIEnv* env, jobject instance) {
    return LocalRef<Cls>::Adopt(env, JNIType(instance));
  }

  static JNIType FromNative(JNIEnv*, LocalRef<Cls>&& instance) {
    return instance.Forget();
  }
};

// clang is picky about function types, including attributes that modify the
// calling convention, lining up.  GCC appears to be somewhat less so.
#ifdef __clang__
#  define MOZ_JNICALL_ABI JNICALL
#else
#  define MOZ_JNICALL_ABI
#endif

// NDK r18 made jvalue* method parameters const. We detect the change directly
// instead of using ndk-version.h in order to remain compatible with r15 for
// now, which doesn't include those headers.
class CallArgs {
  static const jvalue* test(void (JNIEnv::*)(jobject, jmethodID,
                                             const jvalue*));
  static jvalue* test(void (JNIEnv::*)(jobject, jmethodID, jvalue*));

 public:
  using JValueType = decltype(test(&JNIEnv::CallVoidMethodA));
};

template <class Cls>
constexpr jobject (JNIEnv::*TypeAdapter<LocalRef<Cls>>::Call)(
    jobject, jmethodID, CallArgs::JValueType) MOZ_JNICALL_ABI;
template <class Cls>
constexpr jobject (JNIEnv::*TypeAdapter<LocalRef<Cls>>::StaticCall)(
    jclass, jmethodID, CallArgs::JValueType) MOZ_JNICALL_ABI;
template <class Cls>
constexpr jobject (JNIEnv::*TypeAdapter<LocalRef<Cls>>::Get)(jobject, jfieldID);
template <class Cls>
constexpr jobject (JNIEnv::*TypeAdapter<LocalRef<Cls>>::StaticGet)(jclass,
                                                                   jfieldID);

// TypeAdapter<Ref<Cls>> applies when jobject is a parameter value.
template <class Cls, typename T>
struct TypeAdapter<Ref<Cls, T>> {
  using JNIType = typename Ref<Cls, T>::JNIType;

  static constexpr auto Set = &JNIEnv::SetObjectField;
  static constexpr auto StaticSet = &JNIEnv::SetStaticObjectField;

  static DependentRef<Cls> ToNative(JNIEnv* env, JNIType instance) {
    return DependentRef<Cls>(instance);
  }

  static JNIType FromNative(JNIEnv*, const Ref<Cls, T>& instance) {
    return instance.Get();
  }
};

template <class Cls, typename T>
constexpr void (JNIEnv::*TypeAdapter<Ref<Cls, T>>::Set)(jobject, jfieldID,
                                                        jobject);
template <class Cls, typename T>
constexpr void (JNIEnv::*TypeAdapter<Ref<Cls, T>>::StaticSet)(jclass, jfieldID,
                                                              jobject);

// jstring has its own Param type.
template <>
struct TypeAdapter<StringParam> : public TypeAdapter<String::Ref> {};

template <class Cls>
struct TypeAdapter<const Cls&> : public TypeAdapter<Cls> {};

}  // namespace detail

using namespace detail;

}  // namespace jni
}  // namespace mozilla

#endif  // mozilla_jni_Types_h__
