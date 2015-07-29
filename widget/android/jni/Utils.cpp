#include "Utils.h"

#include "mozilla/Assertions.h"

#include "AndroidBridge.h"
#include "GeneratedJNIWrappers.h"

namespace mozilla {
namespace jni {

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
