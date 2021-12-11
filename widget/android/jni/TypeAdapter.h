/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jni_TypeAdapter_h__
#define mozilla_jni_TypeAdapter_h__

#include <jni.h>

#include "mozilla/jni/Refs.h"

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

template <typename T>
struct TypeAdapter;

#define DEFINE_PRIMITIVE_TYPE_ADAPTER(NativeType, JNIType, JNIName)           \
                                                                              \
  template <>                                                                 \
  struct TypeAdapter<NativeType> {                                            \
    using JNI##Type = JNIType;                                                \
                                                                              \
    static constexpr auto Call = &JNIEnv::Call##JNIName##MethodA;             \
    static constexpr auto StaticCall = &JNIEnv::CallStatic##JNIName##MethodA; \
    static constexpr auto Get = &JNIEnv::Get##JNIName##Field;                 \
    static constexpr auto StaticGet = &JNIEnv::GetStatic##JNIName##Field;     \
    static constexpr auto Set = &JNIEnv::Set##JNIName##Field;                 \
    static constexpr auto StaticSet = &JNIEnv::SetStatic##JNIName##Field;     \
    static constexpr auto GetArray = &JNIEnv::Get##JNIName##ArrayRegion;      \
    static constexpr auto SetArray = &JNIEnv::Set##JNIName##ArrayRegion;      \
    static constexpr auto NewArray = &JNIEnv::New##JNIName##Array;            \
                                                                              \
    static JNIType FromNative(JNIEnv*, NativeType val) {                      \
      return static_cast<JNIType>(val);                                       \
    }                                                                         \
    static NativeType ToNative(JNIEnv*, JNIType val) {                        \
      return static_cast<NativeType>(val);                                    \
    }                                                                         \
  }

DEFINE_PRIMITIVE_TYPE_ADAPTER(bool, jboolean, Boolean);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int8_t, jbyte, Byte);
DEFINE_PRIMITIVE_TYPE_ADAPTER(char16_t, jchar, Char);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int16_t, jshort, Short);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int32_t, jint, Int);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int64_t, jlong, Long);
DEFINE_PRIMITIVE_TYPE_ADAPTER(float, jfloat, Float);
DEFINE_PRIMITIVE_TYPE_ADAPTER(double, jdouble, Double);

#undef DEFINE_PRIMITIVE_TYPE_ADAPTER

}  // namespace detail
}  // namespace jni
}  // namespace mozilla

#endif  // mozilla_jni_Types_h__
