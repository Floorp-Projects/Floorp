#ifndef mozilla_jni_Utils_h__
#define mozilla_jni_Utils_h__

#include <jni.h>

#if defined(DEBUG) || !defined(RELEASE_BUILD)
#include "mozilla/Assertions.h"
#include "MainThreadUtils.h"
#endif

namespace mozilla {
namespace jni {

extern JNIEnv* sGeckoThreadEnv;

inline bool IsAvailable()
{
    return !!sGeckoThreadEnv;
}

inline JNIEnv* GetGeckoThreadEnv()
{
#if defined(DEBUG) || !defined(RELEASE_BUILD)
    if (!NS_IsMainThread()) {
        MOZ_CRASH("Not on main thread");
    }
    if (!sGeckoThreadEnv) {
        MOZ_CRASH("Don't have a JNIEnv");
    }
#endif
    return sGeckoThreadEnv;
}

void SetGeckoThreadEnv(JNIEnv* aEnv);

JNIEnv* GetEnvForThread();

bool ThrowException(JNIEnv *aEnv, const char *aClass,
                    const char *aMessage);

inline bool ThrowException(JNIEnv *aEnv, const char *aMessage)
{
    return ThrowException(aEnv, "java/lang/Exception", aMessage);
}

inline bool ThrowException(const char *aClass, const char *aMessage)
{
    return ThrowException(GetEnvForThread(), aClass, aMessage);
}

inline bool ThrowException(const char *aMessage)
{
    return ThrowException(GetEnvForThread(), aMessage);
}

bool HandleUncaughtException(JNIEnv* aEnv);

#define MOZ_CATCH_JNI_EXCEPTION(env) \
    do { \
        if (mozilla::jni::HandleUncaughtException((env))) { \
            MOZ_CRASH("JNI exception"); \
        } \
    } while (0)

uintptr_t GetNativeHandle(JNIEnv* env, jobject instance);

void SetNativeHandle(JNIEnv* env, jobject instance, uintptr_t handle);

} // jni
} // mozilla

#endif // mozilla_jni_Utils_h__
