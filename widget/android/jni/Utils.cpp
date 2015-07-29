#include "Utils.h"
#include "Types.h"

#include "mozilla/Assertions.h"

#include "AndroidBridge.h"
#include "GeneratedJNIWrappers.h"

namespace mozilla {
namespace jni {

namespace detail {

#define DEFINE_PRIMITIVE_TYPE_ADAPTER(NativeType, JNIType, JNIName) \
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

} // namespace detail


bool ThrowException(JNIEnv *aEnv, const char *aClass,
                    const char *aMessage)
{
    MOZ_ASSERT(aEnv, "Invalid thread JNI env");

    ClassObject::LocalRef cls =
            ClassObject::LocalRef::Adopt(aEnv->FindClass(aClass));
    MOZ_ASSERT(cls, "Cannot find exception class");

    return !aEnv->ThrowNew(cls.Get(), aMessage);
}

void HandleUncaughtException(JNIEnv *aEnv)
{
    MOZ_ASSERT(aEnv, "Invalid thread JNI env");

    if (!aEnv->ExceptionCheck()) {
        return;
    }

    Throwable::LocalRef e =
            Throwable::LocalRef::Adopt(aEnv->ExceptionOccurred());
    MOZ_ASSERT(e);

    aEnv->ExceptionClear();
    widget::GeckoAppShell::HandleUncaughtException(nullptr, e);

    // Should be dead by now...
    MOZ_CRASH("Failed to handle uncaught exception");
}

namespace {

jclass sJNIObjectClass;
jfieldID sJNIObjectHandleField;

bool EnsureJNIObject(JNIEnv* env, jobject instance) {
    if (!sJNIObjectClass) {
        sJNIObjectClass = AndroidBridge::GetClassGlobalRef(
                env, "org/mozilla/gecko/mozglue/JNIObject");

        sJNIObjectHandleField = AndroidBridge::GetFieldID(
                env, sJNIObjectClass, "mHandle", "J");
    }

    MOZ_ASSERT(env->IsInstanceOf(instance, sJNIObjectClass));
    return true;
}

} // namespace

uintptr_t GetNativeHandle(JNIEnv* env, jobject instance)
{
    if (!EnsureJNIObject(env, instance)) {
        return 0;
    }

    auto handle = static_cast<uintptr_t>(
            env->GetLongField(instance, sJNIObjectHandleField));

    if (!handle && !env->ExceptionCheck()) {
        ThrowException(env, "java/lang/NullPointerException",
                       "Null native pointer");
    }
    return handle;
}

void SetNativeHandle(JNIEnv* env, jobject instance, uintptr_t handle)
{
    if (!EnsureJNIObject(env, instance)) {
        return;
    }

    env->SetLongField(instance, sJNIObjectHandleField,
                      static_cast<jlong>(handle));
}

} // jni
} // mozilla
