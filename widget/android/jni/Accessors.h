#ifndef mozilla_jni_Accessors_h__
#define mozilla_jni_Accessors_h__

#include <jni.h>

#include "mozilla/jni/Refs.h"
#include "mozilla/jni/Types.h"
#include "mozilla/jni/Utils.h"
#include "AndroidBridge.h"

namespace mozilla {
namespace jni {

namespace detail {

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

} // namespace detail

using namespace detail;

// Base class for Method<>, Field<>, and Constructor<>.
class Accessor
{
    static void GetNsresult(JNIEnv* env, nsresult* rv)
    {
        if (env->ExceptionCheck()) {
#ifdef MOZ_CHECK_JNI
            env->ExceptionDescribe();
#endif
            env->ExceptionClear();
            *rv = NS_ERROR_FAILURE;
        } else {
            *rv = NS_OK;
        }
    }

protected:
    // Called after making a JNIEnv call.
    template<class Traits>
    static void EndAccess(const typename Traits::Owner::Context& ctx,
                          nsresult* rv)
    {
        if (Traits::exceptionMode == ExceptionMode::ABORT) {
            MOZ_CATCH_JNI_EXCEPTION(ctx.Env());

        } else if (Traits::exceptionMode == ExceptionMode::NSRESULT) {
            GetNsresult(ctx.Env(), rv);
        }
    }
};


// Member<> is used to call a JNI method given a traits class.
template<class Traits, typename ReturnType = typename Traits::ReturnType>
class Method : public Accessor
{
    typedef Accessor Base;
    typedef typename Traits::Owner::Context Context;

protected:
    static jmethodID sID;

    static void BeginAccess(const Context& ctx)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);
        static_assert(Traits::dispatchTarget == DispatchTarget::CURRENT,
                      "Dispatching not supported for method call");

        if (sID) {
            return;
        }

        if (Traits::isStatic) {
            MOZ_ALWAYS_TRUE(sID = AndroidBridge::GetStaticMethodID(
                ctx.Env(), ctx.ClassRef(), Traits::name, Traits::signature));
        } else {
            MOZ_ALWAYS_TRUE(sID = AndroidBridge::GetMethodID(
                ctx.Env(), ctx.ClassRef(), Traits::name, Traits::signature));
        }
    }

    static void EndAccess(const Context& ctx, nsresult* rv)
    {
        return Base::EndAccess<Traits>(ctx, rv);
    }

public:
    template<typename... Args>
    static ReturnType Call(const Context& ctx, nsresult* rv, const Args&... args)
    {
        JNIEnv* const env = ctx.Env();
        BeginAccess(ctx);

        jvalue jargs[] = {
            Value(TypeAdapter<Args>::FromNative(env, args)).val ...
        };

        auto result = TypeAdapter<ReturnType>::ToNative(env,
                Traits::isStatic ?
                (env->*TypeAdapter<ReturnType>::StaticCall)(
                        ctx.ClassRef(), sID, jargs) :
                (env->*TypeAdapter<ReturnType>::Call)(
                        ctx.Get(), sID, jargs));

        EndAccess(ctx, rv);
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
    typedef typename Traits::Owner::Context Context;

public:
    template<typename... Args>
    static void Call(const Context& ctx, nsresult* rv,
                     const Args&... args)
    {
        JNIEnv* const env = ctx.Env();
        Base::BeginAccess(ctx);

        jvalue jargs[] = {
            Value(TypeAdapter<Args>::FromNative(env, args)).val ...
        };

        if (Traits::isStatic) {
            env->CallStaticVoidMethodA(ctx.ClassRef(), Base::sID, jargs);
        } else {
            env->CallVoidMethodA(ctx.Get(), Base::sID, jargs);
        }

        Base::EndAccess(ctx, rv);
    }
};


// Constructor<> is used to construct a JNI instance given a traits class.
template<class Traits>
class Constructor : protected Method<Traits, typename Traits::ReturnType> {
    typedef typename Traits::Owner::Context Context;
    typedef typename Traits::ReturnType ReturnType;
    typedef Method<Traits, ReturnType> Base;

public:
    template<typename... Args>
    static ReturnType Call(const Context& ctx, nsresult* rv,
                           const Args&... args)
    {
        JNIEnv* const env = ctx.Env();
        Base::BeginAccess(ctx);

        jvalue jargs[] = {
            Value(TypeAdapter<Args>::FromNative(env, args)).val ...
        };

        auto result = TypeAdapter<ReturnType>::ToNative(
                env, env->NewObjectA(ctx.ClassRef(), Base::sID, jargs));

        Base::EndAccess(ctx, rv);
        return result;
    }
};


// Field<> is used to access a JNI field given a traits class.
template<class Traits>
class Field : public Accessor
{
    typedef Accessor Base;
    typedef typename Traits::Owner::Context Context;
    typedef typename Traits::ReturnType GetterType;
    typedef typename Traits::SetterType SetterType;

private:

    static jfieldID sID;

    static void BeginAccess(const Context& ctx)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);
        static_assert(Traits::dispatchTarget == DispatchTarget::CURRENT,
                      "Dispatching not supported for field access");

        if (sID) {
            return;
        }

        if (Traits::isStatic) {
            MOZ_ALWAYS_TRUE(sID = AndroidBridge::GetStaticFieldID(
                ctx.Env(), ctx.ClassRef(), Traits::name, Traits::signature));
        } else {
            MOZ_ALWAYS_TRUE(sID = AndroidBridge::GetFieldID(
                ctx.Env(), ctx.ClassRef(), Traits::name, Traits::signature));
        }
    }

    static void EndAccess(const Context& ctx, nsresult* rv)
    {
        return Base::EndAccess<Traits>(ctx, rv);
    }

public:
    static GetterType Get(const Context& ctx, nsresult* rv)
    {
        JNIEnv* const env = ctx.Env();
        BeginAccess(ctx);

        auto result = TypeAdapter<GetterType>::ToNative(
                env, Traits::isStatic ?

                (env->*TypeAdapter<GetterType>::StaticGet)
                        (ctx.ClassRef(), sID) :

                (env->*TypeAdapter<GetterType>::Get)
                        (ctx.Get(), sID));

        EndAccess(ctx, rv);
        return result;
    }

    static void Set(const Context& ctx, nsresult* rv, SetterType val)
    {
        JNIEnv* const env = ctx.Env();
        BeginAccess(ctx);

        if (Traits::isStatic) {
            (env->*TypeAdapter<SetterType>::StaticSet)(
                    ctx.ClassRef(), sID,
                    TypeAdapter<SetterType>::FromNative(env, val));
        } else {
            (env->*TypeAdapter<SetterType>::Set)(
                    ctx.Get(), sID,
                    TypeAdapter<SetterType>::FromNative(env, val));
        }

        EndAccess(ctx, rv);
    }
};

// Define sID member.
template<class T> jfieldID Field<T>::sID;


// Define the sClassRef member declared in Refs.h and
// used by Method and Field above.
template<class C, typename T> jclass Context<C, T>::sClassRef;

} // namespace jni
} // namespace mozilla

#endif // mozilla_jni_Accessors_h__
