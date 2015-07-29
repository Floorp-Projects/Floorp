#ifndef mozilla_jni_Accessors_h__
#define mozilla_jni_Accessors_h__

#include <jni.h>

#include "mozilla/jni/Refs.h"
#include "mozilla/jni/Types.h"
#include "AndroidBridge.h"

namespace mozilla {
namespace jni{

namespace {

// Helper class to convert an arbitrary type to a jvalue, e.g. Value(123).val.
struct Value
{
    Value(jboolean z) { val.z = z; }
    Value(jbyte b)    { val.b = b; }
    Value(jchar c)    { val.c = c; }
    Value(jshort s)   { val.s = s; }
    Value(jint i)     { val.i = i; }
    Value(jlong j)    { val.j = j; }
    Value(jfloat f)   { val.f = f; }
    Value(jdouble d)  { val.d = d; }
    Value(jobject l)  { val.l = l; }

    jvalue val;
};

}

// Base class for Method<>, Field<>, and Constructor<>.
class Accessor {
public:
    template<class Cls>
    static jclass EnsureClassRef(JNIEnv* env)
    {
        if (!Cls::sClassRef) {
            MOZ_ALWAYS_TRUE(Cls::sClassRef =
                AndroidBridge::GetClassGlobalRef(env, Cls::name));
        }
        return Cls::sClassRef;
    }

private:
    static void GetNsresult(JNIEnv* env, nsresult* rv)
    {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            *rv = NS_ERROR_FAILURE;
        } else {
            *rv = NS_OK;
        }
    }

protected:
    // Called before making a JNIEnv call.
    template<class Traits>
    static JNIEnv* BeginAccess()
    {
        JNIEnv* const env = Traits::isMultithreaded
                ? GetJNIForThread() : AndroidBridge::GetJNIEnv();

        EnsureClassRef<class Traits::Owner>(env);
        return env;
    }

    // Called after making a JNIEnv call.
    template<class Traits>
    static void EndAccess(JNIEnv* env, nsresult* rv)
    {
        if (Traits::exceptionMode == ExceptionMode::ABORT) {
            return HandleUncaughtException(env);

        } else if (Traits::exceptionMode == ExceptionMode::NSRESULT) {
            return GetNsresult(env, rv);
        }
    }
};


// Member<> is used to call a JNI method given a traits class.
template<class Traits, typename ReturnType = typename Traits::ReturnType>
class Method : public Accessor
{
    typedef Accessor Base;
    typedef class Traits::Owner Owner;

protected:
    static jmethodID sID;

    static JNIEnv* BeginAccess()
    {
        JNIEnv* const env = Base::BeginAccess<Traits>();

        if (sID) {
            return env;
        }

        if (Traits::isStatic) {
            MOZ_ALWAYS_TRUE(sID = AndroidBridge::GetStaticMethodID(
                env, Traits::Owner::sClassRef, Traits::name, Traits::signature));
        } else {
            MOZ_ALWAYS_TRUE(sID = AndroidBridge::GetMethodID(
                env, Traits::Owner::sClassRef, Traits::name, Traits::signature));
        }
        return env;
    }

    static void EndAccess(JNIEnv* env, nsresult* rv)
    {
        return Base::EndAccess<Traits>(env, rv);
    }

public:
    template<typename... Args>
    static ReturnType Call(const Owner* cls, nsresult* rv, const Args&... args)
    {
        JNIEnv* const env = BeginAccess();

        jvalue jargs[] = {
            Value(TypeAdapter<Args>::FromNative(env, args)).val ...
        };

        auto result = TypeAdapter<ReturnType>::ToNative(env,
                Traits::isStatic ?
                (env->*TypeAdapter<ReturnType>::StaticCall)(
                        Owner::sClassRef, sID, jargs) :
                (env->*TypeAdapter<ReturnType>::Call)(
                        cls->mInstance, sID, jargs));

        EndAccess(env, rv);
        return result;
    }
};

// Define sID member.
template<class T, typename R> jmethodID Method<T, R>::sID;


// Specialize void because C++ forbids us from
// using a "void" temporary result variable.
template<class Traits>
class Method<Traits, void> : public Method<Traits, bool>
{
    typedef Method<Traits, bool> Base;
    typedef typename Traits::Owner Owner;

public:
    template<typename... Args>
    static void Call(const Owner* cls, nsresult* rv,
                     const Args&... args) override
    {
        JNIEnv* const env = Base::BeginAccess();

        jvalue jargs[] = {
            Value(TypeAdapter<Args>::FromNative(env, args)).val ...
        };

        if (Traits::isStatic) {
            env->CallStaticVoidMethodA(Owner::sClassRef, Base::sID, jargs);
        } else {
            env->CallVoidMethodA(cls->mInstance, Base::sID, jargs);
        }

        Base::EndAccess(env, rv);
    }
};


// Constructor<> is used to construct a JNI instance given a traits class.
template<class Traits>
class Constructor : protected Method<Traits, typename Traits::ReturnType> {
    typedef class Traits::Owner Owner;
    typedef typename Traits::ReturnType ReturnType;
    typedef Method<Traits, ReturnType> Base;

public:
    template<typename... Args>
    static ReturnType Call(const Owner* cls, nsresult* rv,
                           const Args&... args) override
    {
        JNIEnv* const env = Base::BeginAccess();

        jvalue jargs[] = {
            Value(TypeAdapter<Args>::FromNative(env, args)).val ...
        };

        auto result = TypeAdapter<ReturnType>::ToNative(
                env, env->NewObjectA(Owner::sClassRef, Base::sID, jargs));

        Base::EndAccess(env, rv);
        return result;
    }
};


// Field<> is used to access a JNI field given a traits class.
template<class Traits>
class Field : public Accessor
{
    typedef Accessor Base;
    typedef class Traits::Owner Owner;
    typedef typename Traits::ReturnType GetterType;
    typedef typename Traits::SetterType SetterType;

private:

    static jfieldID sID;

    static JNIEnv* BeginAccess()
    {
        JNIEnv* const env = Base::BeginAccess<Traits>();

        if (sID) {
            return env;
        }

        if (Traits::isStatic) {
            MOZ_ALWAYS_TRUE(sID = AndroidBridge::GetStaticFieldID(
                env, Traits::Owner::sClassRef, Traits::name, Traits::signature));
        } else {
            MOZ_ALWAYS_TRUE(sID = AndroidBridge::GetFieldID(
                env, Traits::Owner::sClassRef, Traits::name, Traits::signature));
        }
        return env;
    }

    static void EndAccess(JNIEnv* env, nsresult* rv)
    {
        return Base::EndAccess<Traits>(env, rv);
    }

public:
    static GetterType Get(const Owner* cls, nsresult* rv)
    {
        JNIEnv* const env = BeginAccess();

        auto result = TypeAdapter<GetterType>::ToNative(
                env, Traits::isStatic ?

                (env->*TypeAdapter<GetterType>::StaticGet)
                        (Owner::sClassRef, sID) :

                (env->*TypeAdapter<GetterType>::Get)
                        (cls->mInstance, sID));

        EndAccess(env, rv);
        return result;
    }

    static void Set(const Owner* cls, nsresult* rv, SetterType val)
    {
        JNIEnv* const env = BeginAccess();

        if (Traits::isStatic) {
            (env->*TypeAdapter<SetterType>::StaticSet)(
                    Owner::sClassRef, sID,
                    TypeAdapter<SetterType>::FromNative(env, val));
        } else {
            (env->*TypeAdapter<SetterType>::Set)(
                    cls->mInstance, sID,
                    TypeAdapter<SetterType>::FromNative(env, val));
        }

        EndAccess(env, rv);
    }
};

// Define sID member.
template<class T> jfieldID Field<T>::sID;


// Define the sClassRef member declared in Refs.h and
// used by Method and Field above.
template<class C> jclass Class<C>::sClassRef;

} // namespace jni
} // namespace mozilla

#endif // mozilla_jni_Accessors_h__
