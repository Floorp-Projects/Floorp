#ifndef mozilla_jni_Utils_h__
#define mozilla_jni_Utils_h__

#include <jni.h>

#include "mozilla/UniquePtr.h"

#if defined(DEBUG) || !defined(RELEASE_BUILD)
#define MOZ_CHECK_JNI
#endif

#ifdef MOZ_CHECK_JNI
#include <pthread.h>
#include "mozilla/Assertions.h"
#include "APKOpen.h"
#include "MainThreadUtils.h"
#endif

namespace mozilla {
namespace jni {

// How exception during a JNI call should be treated.
enum class ExceptionMode
{
    // Abort on unhandled excepion (default).
    ABORT,
    // Ignore the exception and return to caller.
    IGNORE,
    // Catch any exception and return a nsresult.
    NSRESULT,
};

// Thread that a particular JNI call is allowed on.
enum class CallingThread
{
    // Can be called from any thread (default).
    ANY,
    // Can be called from the Gecko thread.
    GECKO,
    // Can be called from the Java UI thread.
    UI,
};

// If and where a JNI call will be dispatched.
enum class DispatchTarget
{
    // Call happens synchronously on the calling thread (default).
    CURRENT,
    // Call happens synchronously on the calling thread, but the call is
    // wrapped in a function object and is passed thru UsesNativeCallProxy.
    // Method must return void.
    PROXY,
    // Call is dispatched asynchronously on the Gecko thread. Method must
    // return void.
    GECKO,
};


extern JNIEnv* sGeckoThreadEnv;

inline bool IsAvailable()
{
    return !!sGeckoThreadEnv;
}

inline JNIEnv* GetGeckoThreadEnv()
{
#ifdef MOZ_CHECK_JNI
    MOZ_RELEASE_ASSERT(NS_IsMainThread(), "Must be on Gecko thread");
    MOZ_RELEASE_ASSERT(sGeckoThreadEnv, "Must have a JNIEnv");
#endif
    return sGeckoThreadEnv;
}

void SetGeckoThreadEnv(JNIEnv* aEnv);

JNIEnv* GetEnvForThread();

#ifdef MOZ_CHECK_JNI
#define MOZ_ASSERT_JNI_THREAD(thread) \
    do { \
        if ((thread) == mozilla::jni::CallingThread::GECKO) { \
            MOZ_RELEASE_ASSERT(::NS_IsMainThread()); \
        } else if ((thread) == mozilla::jni::CallingThread::UI) { \
            const bool isOnUiThread = ::pthread_equal(::pthread_self(), \
                                                      ::getJavaUiThread()); \
            MOZ_RELEASE_ASSERT(isOnUiThread); \
        } \
    } while (0)
#else
#define MOZ_ASSERT_JNI_THREAD(thread) do {} while (0)
#endif

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

jclass GetClassGlobalRef(JNIEnv* aEnv, const char* aClassName);


struct AbstractCall
{
    virtual ~AbstractCall() {}
    virtual void operator()() = 0;
};

void DispatchToGeckoThread(UniquePtr<AbstractCall>&& aCall);

} // jni
} // mozilla

#endif // mozilla_jni_Utils_h__
