#ifndef mozilla_jni_Utils_h__
#define mozilla_jni_Utils_h__

#include <jni.h>

#include "mozilla/Types.h"

/* See the comment in AndroidBridge about this function before using it */
extern "C" MOZ_EXPORT JNIEnv * GetJNIForThread();

namespace mozilla {
namespace jni {

bool ThrowException(JNIEnv *aEnv, const char *aClass,
                    const char *aMessage);

inline bool ThrowException(JNIEnv *aEnv, const char *aMessage)
{
    return ThrowException(aEnv, "java/lang/Exception", aMessage);
}

void HandleUncaughtException(JNIEnv *aEnv);

uintptr_t GetNativeHandle(JNIEnv* env, jobject instance);

void SetNativeHandle(JNIEnv* env, jobject instance, uintptr_t handle);

} // jni
} // mozilla

#endif // mozilla_jni_Utils_h__
