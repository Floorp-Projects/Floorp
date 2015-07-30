#ifndef mozilla_jni_Natives_h__
#define mozilla_jni_Natives_h__

#include <jni.h>

#include "mozilla/WeakPtr.h"
#include "mozilla/jni/Accessors.h"
#include "mozilla/jni/Refs.h"
#include "mozilla/jni/Types.h"
#include "mozilla/jni/Utils.h"

namespace mozilla {
namespace jni {

// Get the native pointer stored in a Java instance.
template<class Impl>
Impl* GetNativePtr(JNIEnv* env, jobject instance)
{
    const auto ptr = reinterpret_cast<const WeakPtr<Impl>*>(
            GetNativeHandle(env, instance));
    if (!ptr) {
        return nullptr;
    }

    Impl* const impl = *ptr;
    if (!impl) {
        ThrowException(env, "java/lang/NullPointerException",
                       "Native object already released");
    }
    return impl;
}

template<class Impl, class LocalRef>
Impl* GetNativePtr(const LocalRef& instance)
{
    return GetNativePtr<Impl>(instance.Env(), instance.Get());
}

template<class Impl, class LocalRef>
void ClearNativePtr(const LocalRef& instance)
{
    JNIEnv* const env = instance.Env();
    const auto ptr = reinterpret_cast<WeakPtr<Impl>*>(
            GetNativeHandle(env, instance.Get()));
    if (ptr) {
        SetNativeHandle(env, instance.Get(), 0);
        delete ptr;
    } else {
        // GetNativeHandle throws an exception when returning null.
        MOZ_ASSERT(env->ExceptionCheck());
        env->ExceptionClear();
    }
}

template<class Impl, class LocalRef>
void SetNativePtr(const LocalRef& instance, Impl* ptr)
{
    ClearNativePtr<Impl>(instance);
    SetNativeHandle(instance.Env(), instance.Get(),
                      reinterpret_cast<uintptr_t>(new WeakPtr<Impl>(ptr)));
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
        Impl* const impl = GetNativePtr<Impl>(env, instance);
        if (!impl) {
            return ReturnJNIType();
        }
        return TypeAdapter<ReturnType>::FromNative(env,
                (impl->*Method)(TypeAdapter<Args>::ToNative(env, args)...));
    }

    // Instance method with instance reference
    template<ReturnType (Impl::*Method) (const typename Owner::LocalRef&, Args...)>
    static ReturnJNIType Wrap(JNIEnv* env, jobject instance,
                              typename TypeAdapter<Args>::JNIType... args)
    {
        Impl* const impl = GetNativePtr<Impl>(env, instance);
        if (!impl) {
            return ReturnJNIType();
        }
        auto self = Owner::LocalRef::Adopt(env, instance);
        const auto res = TypeAdapter<ReturnType>::FromNative(env,
                (impl->*Method)(self, TypeAdapter<Args>::ToNative(env, args)...));
        self.Forget();
        return res;
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
        Impl* const impl = GetNativePtr<Impl>(env, instance);
        if (!impl) {
            return;
        }
        (impl->*Method)(TypeAdapter<Args>::ToNative(env, args)...);
    }

    // Instance method with instance reference
    template<void (Impl::*Method) (const typename Owner::LocalRef&, Args...)>
    static void Wrap(JNIEnv* env, jobject instance,
                     typename TypeAdapter<Args>::JNIType... args)
    {
        Impl* const impl = GetNativePtr<Impl>(env, instance);
        if (!impl) {
            return;
        }
        auto self = Owner::LocalRef::Adopt(env, instance);
        (impl->*Method)(self, TypeAdapter<Args>::ToNative(env, args)...);
        self.Forget();
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
    template<ReturnType (*Method) (const ClassObject::LocalRef&, Args...)>
    static ReturnJNIType Wrap(JNIEnv* env, jclass cls,
                              typename TypeAdapter<Args>::JNIType... args)
    {
        auto clazz = ClassObject::LocalRef::Adopt(env, cls);
        const auto res = TypeAdapter<ReturnType>::FromNative(env,
                (*Method)(clazz, TypeAdapter<Args>::ToNative(env, args)...));
        clazz.Forget();
        return res;
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
    template<void (*Method) (const ClassObject::LocalRef&, Args...)>
    static void Wrap(JNIEnv* env, jclass cls,
                     typename TypeAdapter<Args>::JNIType... args)
    {
        auto clazz = ClassObject::LocalRef::Adopt(env, cls);
        (*Method)(clazz, TypeAdapter<Args>::ToNative(env, args)...);
        clazz.Forget();
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
        MOZ_ALWAYS_TRUE(!env->RegisterNatives(
                Accessor::EnsureClassRef<Cls>(env),
                 Natives::methods,
                 sizeof(Natives::methods) / sizeof(Natives::methods[0])));
        sInited = true;
    }

protected:
    static Impl* GetNative(const typename Cls::LocalRef& instance) {
        return GetNativePtr<Impl>(instance);
    }

    NativeImpl() {
        // Initialize on creation if not already initialized.
        Init();
    }

    void AttachNative(const typename Cls::LocalRef& instance) {
        SetNativePtr<>(instance, static_cast<Impl*>(this));
    }

    void DisposeNative(const typename Cls::LocalRef& instance) {
        ClearNativePtr<Impl>(instance);
    }
};

// Define static member.
template<class C, class I>
bool NativeImpl<C, I>::sInited;

} // namespace jni
} // namespace mozilla

#endif // mozilla_jni_Natives_h__
