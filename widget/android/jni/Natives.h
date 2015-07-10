#ifndef mozilla_jni_Natives_h__
#define mozilla_jni_Natives_h__

#include <jni.h>

#include "mozilla/jni/Accessors.h"
#include "mozilla/jni/Refs.h"
#include "mozilla/jni/Types.h"
#include "mozilla/jni/Utils.h"

namespace mozilla {
namespace jni{

// Get the native pointer stored in a Java instance.
template<class Impl>
Impl* GetInstancePtr(JNIEnv* env, jobject instance)
{
    // TODO: implement instance native pointers.
    return nullptr;
}

namespace detail {

// Wrapper methods that convert arguments from the JNI types to the native
// types, e.g. from jobject to jni::Object::Ref. For instance methods, the
// wrapper methods also convert calls to calls on objects.
//
// We need specialization for static/non-static because the two have different
// signatures (jobject vs jclass and Impl::*Method vs *Method).
// We need specialization for return type, because void return type requires
// us to not deal with the return value.

template<bool IsStatic, typename ReturnType,
         class Traits, class Impl, class Args>
class NativeStubImpl;

// Specialization for instance methods with non-void return type
template<typename ReturnType, class Traits, class Impl, typename... Args>
class NativeStubImpl<false, ReturnType, Traits, Impl, jni::Args<Args...>>
{
    typedef typename Traits::Owner Owner;
    typedef typename TypeAdapter<ReturnType>::JNIType ReturnJNIType;

public:
    // Instance method
    template<ReturnType (Impl::*Method) (Args...)>
    static ReturnJNIType Wrap(JNIEnv* env, jobject instance,
                              typename TypeAdapter<Args>::JNIType... args)
    {
        Impl* const impl = GetInstancePtr<Impl>(env, instance);
        if (!impl) {
            return ReturnJNIType();
        }
        return TypeAdapter<ReturnType>::FromNative(env,
                (impl->*Method)(TypeAdapter<Args>::ToNative(env, args)...));
    }

    // Instance method with instance reference
    template<ReturnType (Impl::*Method) (typename Owner::Param, Args...)>
    static ReturnJNIType Wrap(JNIEnv* env, jobject instance,
                              typename TypeAdapter<Args>::JNIType... args)
    {
        Impl* const impl = GetInstancePtr<Impl>(env, instance);
        if (!impl) {
            return ReturnJNIType();
        }
        return TypeAdapter<ReturnType>::FromNative(env,
                (impl->*Method)(Owner::Ref::From(instance),
                                TypeAdapter<Args>::ToNative(env, args)...));
    }
};

// Specialization for instance methods with void return type
template<class Traits, class Impl, typename... Args>
class NativeStubImpl<false, void, Traits, Impl, jni::Args<Args...>>
{
    typedef typename Traits::Owner Owner;

public:
    // Instance method
    template<void (Impl::*Method) (Args...)>
    static void Wrap(JNIEnv* env, jobject instance,
                     typename TypeAdapter<Args>::JNIType... args)
    {
        Impl* const impl = GetInstancePtr<Impl>(env, instance);
        if (!impl) {
            return;
        }
        (impl->*Method)(TypeAdapter<Args>::ToNative(env, args)...);
    }

    // Instance method with instance reference
    template<void (Impl::*Method) (typename Owner::Param, Args...)>
    static void Wrap(JNIEnv* env, jobject instance,
                     typename TypeAdapter<Args>::JNIType... args)
    {
        Impl* const impl = GetInstancePtr<Impl>(env, instance);
        if (!impl) {
            return;
        }
        (impl->*Method)(Owner::Ref::From(instance),
                        TypeAdapter<Args>::ToNative(env, args)...);
    }
};

// Specialization for static methods with non-void return type
template<typename ReturnType, class Traits, class Impl, typename... Args>
class NativeStubImpl<true, ReturnType, Traits, Impl, jni::Args<Args...>>
{
    typedef typename TypeAdapter<ReturnType>::JNIType ReturnJNIType;

public:
    // Static method
    template<ReturnType (*Method) (Args...)>
    static ReturnJNIType Wrap(JNIEnv* env, jclass,
                              typename TypeAdapter<Args>::JNIType... args)
    {
        return TypeAdapter<ReturnType>::FromNative(env,
                (*Method)(TypeAdapter<Args>::ToNative(env, args)...));
    }

    // Static method with class reference
    template<ReturnType (*Method) (ClassObject::Param, Args...)>
    static ReturnJNIType Wrap(JNIEnv* env, jclass cls,
                              typename TypeAdapter<Args>::JNIType... args)
    {
        return TypeAdapter<ReturnType>::FromNative(env,
                (*Method)(ClassObject::Ref::From(cls),
                          TypeAdapter<Args>::ToNative(env, args)...));
    }
};

// Specialization for static methods with void return type
template<class Traits, class Impl, typename... Args>
class NativeStubImpl<true, void, Traits, Impl, jni::Args<Args...>>
{
public:
    // Static method
    template<void (*Method) (Args...)>
    static void Wrap(JNIEnv* env, jclass,
                     typename TypeAdapter<Args>::JNIType... args)
    {
        (*Method)(TypeAdapter<Args>::ToNative(env, args)...);
    }

    // Static method with class reference
    template<void (*Method) (ClassObject::Param, Args...)>
    static void Wrap(JNIEnv* env, jclass cls,
                     typename TypeAdapter<Args>::JNIType... args)
    {
        (*Method)(ClassObject::Ref::From(cls),
                  TypeAdapter<Args>::ToNative(env, args)...);
    }
};

} // namespace detail

// Form a stub wrapper from a native method's traits class and an implementing
// class. The stub wrapper has a Wrap function that will form a wrapped stub.
template<class Traits, class Impl>
struct NativeStub : detail::NativeStubImpl<Traits::isStatic,
                                           typename Traits::ReturnType,
                                           Traits, Impl, typename Traits::Args>
{
};

// Generate a JNINativeMethod from a native
// method's traits class and a wrapped stub.
template<class Traits, typename Ret, typename... Args>
constexpr JNINativeMethod MakeNativeMethod(Ret (*stub)(JNIEnv*, Args...))
{
    return {
        Traits::name,
        Traits::signature,
        reinterpret_cast<void*>(stub)
    };
}

// Class inherited by implementing class.
template<class Cls, class Impl>
class NativeImpl
{
    typedef typename Cls::template Natives<Impl> Natives;

    static bool sInited;

public:
    static void Init() {
        if (sInited) {
            return;
        }
        JNIEnv* const env = GetJNIForThread();
        env->RegisterNatives(Accessor::EnsureClassRef<Cls>(env),
                             Natives::methods,
                             sizeof(Natives::methods) / sizeof(JNINativeMethod));
        sInited = true;
    }

    NativeImpl() {
        // Initialize on creation if not already initialized.
        Init();
    }
};

// Define static member.
template<class C, class I>
bool NativeImpl<C, I>::sInited;

} // namespace jni
} // namespace mozilla

#endif // mozilla_jni_Natives_h__
