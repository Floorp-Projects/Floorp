#include "Utils.h"

#include "mozilla/Assertions.h"

#include "GeneratedJNIWrappers.h"
#include "Refs.h"

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

} // jni
} // mozilla
