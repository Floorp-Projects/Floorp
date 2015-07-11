#ifndef mozilla_jni_Types_h__
#define mozilla_jni_Types_h__

#include <jni.h>

#include "mozilla/jni/Refs.h"
#include "AndroidBridge.h"

namespace mozilla {
namespace jni {
namespace {

// TypeAdapter specializations are the interfaces between naive (C++) types such
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

template<typename T> struct TypeAdapter;


// TypeAdapter<LocalRef<Cls>> applies when jobject is a return value.
template<class Cls> struct TypeAdapter<LocalRef<Cls>> {
    static constexpr auto Call = &JNIEnv::CallObjectMethodA;
    static constexpr auto StaticCall = &JNIEnv::CallStaticObjectMethodA;
    static constexpr auto Get = &JNIEnv::GetObjectField;
    static constexpr auto StaticGet = &JNIEnv::GetStaticObjectField;

    static LocalRef<Cls> ToNative(JNIEnv* env, jobject instance) {
        return LocalRef<Cls>::Adopt(env, instance);
    }
};

template<class Cls> constexpr jobject
    (JNIEnv::*TypeAdapter<LocalRef<Cls>>::Call)(jobject, jmethodID, jvalue*);
template<class Cls> constexpr jobject
    (JNIEnv::*TypeAdapter<LocalRef<Cls>>::StaticCall)(jclass, jmethodID, jvalue*);
template<class Cls> constexpr jobject
    (JNIEnv::*TypeAdapter<LocalRef<Cls>>::Get)(jobject, jfieldID);
template<class Cls> constexpr jobject
    (JNIEnv::*TypeAdapter<LocalRef<Cls>>::StaticGet)(jclass, jfieldID);


// TypeAdapter<Ref<Cls>> applies when jobject is a parameter value.
template<class Cls> struct TypeAdapter<Ref<Cls>> {
    static constexpr auto Set = &JNIEnv::SetObjectField;
    static constexpr auto StaticSet = &JNIEnv::SetStaticObjectField;

    static jobject FromNative(JNIEnv*, const Ref<Cls>& instance) {
        return instance.Get();
    }
};

template<class Cls> constexpr void
    (JNIEnv::*TypeAdapter<Ref<Cls>>::Set)(jobject, jfieldID, jobject);
template<class Cls> constexpr void
    (JNIEnv::*TypeAdapter<Ref<Cls>>::StaticSet)(jclass, jfieldID, jobject);


// jstring has its own Param type.
template<> struct TypeAdapter<Param<String>>
        : public TypeAdapter<String::Ref>
{};


#define DEFINE_PRIMITIVE_TYPE_ADAPTER(NativeType, JNIType, JNIName) \
    \
    template<> struct TypeAdapter<NativeType> { \
        static constexpr auto Call = &JNIEnv::Call ## JNIName ## MethodA; \
        static constexpr auto StaticCall = &JNIEnv::CallStatic ## JNIName ## MethodA; \
        static constexpr auto Get = &JNIEnv::Get ## JNIName ## Field; \
        static constexpr auto StaticGet = &JNIEnv::GetStatic ## JNIName ## Field; \
        static constexpr auto Set = &JNIEnv::Set ## JNIName ## Field; \
        static constexpr auto StaticSet = &JNIEnv::SetStatic ## JNIName ## Field; \
    \
        static JNIType FromNative(JNIEnv*, NativeType val) { \
            return static_cast<JNIType>(val); \
        } \
        static NativeType ToNative(JNIEnv*, JNIType val) { \
            return static_cast<NativeType>(val); \
        } \
    }; \
    \
    constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::Call) \
            (jobject, jmethodID, jvalue*); \
    constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::StaticCall) \
            (jclass, jmethodID, jvalue*); \
    constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::Get) \
            (jobject, jfieldID); \
    constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::StaticGet) \
            (jclass, jfieldID); \
    constexpr void (JNIEnv::*TypeAdapter<NativeType>::Set) \
            (jobject, jfieldID, JNIType); \
    constexpr void (JNIEnv::*TypeAdapter<NativeType>::StaticSet) \
            (jclass, jfieldID, JNIType)


DEFINE_PRIMITIVE_TYPE_ADAPTER(bool,     jboolean, Boolean);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int8_t,   jbyte,    Byte);
DEFINE_PRIMITIVE_TYPE_ADAPTER(char16_t, jchar,    Char);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int16_t,  jshort,   Short);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int32_t,  jint,     Int);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int64_t,  jlong,    Long);
DEFINE_PRIMITIVE_TYPE_ADAPTER(float,    jfloat,   Float);
DEFINE_PRIMITIVE_TYPE_ADAPTER(double,   jdouble,  Double);

#undef DEFINE_PRIMITIVE_TYPE_ADAPTER

} // namespace
} // namespace jni
} // namespace mozilla

#endif // mozilla_jni_Types_h__
