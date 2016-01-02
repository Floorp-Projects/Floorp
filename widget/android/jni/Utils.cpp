#include "Utils.h"
#include "Types.h"

#include <pthread.h>

#include "mozilla/Assertions.h"

#include "AndroidBridge.h"
#include "GeneratedJNIWrappers.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

namespace mozilla {
namespace jni {

namespace detail {

#define DEFINE_PRIMITIVE_TYPE_ADAPTER(NativeType, JNIType, JNIName, ABIName)	\
    \
    constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::Call) \
            (jobject, jmethodID, jvalue*) MOZ_JNICALL_ABI; \
    constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::StaticCall) \
            (jclass, jmethodID, jvalue*) MOZ_JNICALL_ABI; \
    constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::Get) \
            (jobject, jfieldID) ABIName; \
    constexpr JNIType (JNIEnv::*TypeAdapter<NativeType>::StaticGet) \
            (jclass, jfieldID) ABIName; \
    constexpr void (JNIEnv::*TypeAdapter<NativeType>::Set) \
            (jobject, jfieldID, JNIType) ABIName; \
    constexpr void (JNIEnv::*TypeAdapter<NativeType>::StaticSet) \
            (jclass, jfieldID, JNIType) ABIName

DEFINE_PRIMITIVE_TYPE_ADAPTER(bool,     jboolean, Boolean, /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int8_t,   jbyte,    Byte,    /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(char16_t, jchar,    Char,    /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int16_t,  jshort,   Short,   /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int32_t,  jint,     Int,     /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(int64_t,  jlong,    Long,    /*nothing*/);
DEFINE_PRIMITIVE_TYPE_ADAPTER(float,    jfloat,   Float,   MOZ_JNICALL_ABI);
DEFINE_PRIMITIVE_TYPE_ADAPTER(double,   jdouble,  Double,  MOZ_JNICALL_ABI);

#undef DEFINE_PRIMITIVE_TYPE_ADAPTER

} // namespace detail

constexpr char Object::name[];
template<> const char TypedObject<jstring>::name[] = "java/lang/String";
template<> const char TypedObject<jclass>::name[] = "java/lang/Class";
template<> const char TypedObject<jthrowable>::name[] = "java/lang/Throwable";
template<> const char TypedObject<jbooleanArray>::name[] = "[Z";
template<> const char TypedObject<jbyteArray>::name[] = "[B";
template<> const char TypedObject<jcharArray>::name[] = "[C";
template<> const char TypedObject<jshortArray>::name[] = "[S";
template<> const char TypedObject<jintArray>::name[] = "[I";
template<> const char TypedObject<jlongArray>::name[] = "[J";
template<> const char TypedObject<jfloatArray>::name[] = "[F";
template<> const char TypedObject<jdoubleArray>::name[] = "[D";
template<> const char TypedObject<jobjectArray>::name[] = "[Ljava/lang/Object;";


JNIEnv* sGeckoThreadEnv;

namespace {

JavaVM* sJavaVM;
pthread_key_t sThreadEnvKey;

void UnregisterThreadEnv(void* env)
{
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

} // namespace

void SetGeckoThreadEnv(JNIEnv* aEnv)
{
    MOZ_ASSERT(aEnv);
    MOZ_ASSERT(!sGeckoThreadEnv || sGeckoThreadEnv == aEnv);

    if (!sGeckoThreadEnv
            && pthread_key_create(&sThreadEnvKey, UnregisterThreadEnv)) {
        MOZ_CRASH("Failed to initialize required TLS");
    }

    sGeckoThreadEnv = aEnv;
    MOZ_ALWAYS_TRUE(!pthread_setspecific(sThreadEnvKey, aEnv));

    MOZ_ALWAYS_TRUE(!aEnv->GetJavaVM(&sJavaVM));
    MOZ_ASSERT(sJavaVM);
}

JNIEnv* GetEnvForThread()
{
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
    return nullptr; // unreachable
}

bool ThrowException(JNIEnv *aEnv, const char *aClass,
                    const char *aMessage)
{
    MOZ_ASSERT(aEnv, "Invalid thread JNI env");

    ClassObject::LocalRef cls =
            ClassObject::LocalRef::Adopt(aEnv->FindClass(aClass));
    MOZ_ASSERT(cls, "Cannot find exception class");

    return !aEnv->ThrowNew(cls.Get(), aMessage);
}

bool HandleUncaughtException(JNIEnv* aEnv)
{
    MOZ_ASSERT(aEnv, "Invalid thread JNI env");

    if (!aEnv->ExceptionCheck()) {
        return false;
    }

#ifdef DEBUG
    aEnv->ExceptionDescribe();
#endif

    Throwable::LocalRef e =
            Throwable::LocalRef::Adopt(aEnv->ExceptionOccurred());
    MOZ_ASSERT(e);

    aEnv->ExceptionClear();
    String::LocalRef stack = widget::GeckoAppShell::HandleUncaughtException(e);

#ifdef MOZ_CRASHREPORTER
    if (stack) {
        // GeckoAppShell wants us to annotate and trigger the crash reporter.
        CrashReporter::AnnotateCrashReport(
                NS_LITERAL_CSTRING("AuxiliaryJavaStack"), nsCString(stack));
    }
#endif // MOZ_CRASHREPORTER

    return true;
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

    return static_cast<uintptr_t>(
            env->GetLongField(instance, sJNIObjectHandleField));
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
