#ifndef mozilla_jni_Natives_h__
#define mozilla_jni_Natives_h__

#include <jni.h>

#include "mozilla/Move.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/jni/Accessors.h"
#include "mozilla/jni/Refs.h"
#include "mozilla/jni/Types.h"
#include "mozilla/jni/Utils.h"

namespace mozilla {
namespace jni {

/**
 * C++ classes implementing instance (non-static) native methods can choose
 * from one of two ownership models, when associating a C++ object with a Java
 * instance.
 *
 * * If the C++ class inherits from mozilla::SupportsWeakPtr, weak pointers
 *   will be used. The Java instance will store and own the pointer to a
 *   WeakPtr object. The C++ class itself is otherwise not owned or directly
 *   referenced. To attach a Java instance to a C++ instance, pass in a pointer
 *   to the C++ class (i.e. MyClass*).
 *
 *   class MyClass : public SupportsWeakPtr<MyClass>
 *                 , public MyJavaClass::Natives<MyClass>
 *   {
 *       // ...
 *
 *   public:
 *       MOZ_DECLARE_WEAKREFERENCE_TYPENAME(MyClass)
 *       using MyJavaClass::Natives<MyClass>::Dispose;
 *
 *       void AttachTo(const MyJavaClass::LocalRef& instance)
 *       {
 *           MyJavaClass::Natives<MyClass>::AttachInstance(instance, this);
 *
 *           // "instance" does NOT own "this", so the C++ object
 *           // lifetime is separate from the Java object lifetime.
 *       }
 *   };
 *
 * * If the C++ class doesn't inherit from mozilla::SupportsWeakPtr, the Java
 *   instance will store and own a pointer to the C++ object itself. This
 *   pointer must not be stored or deleted elsewhere. To attach a Java instance
 *   to a C++ instance, pass in a reference to a UniquePtr of the C++ class
 *   (i.e. UniquePtr<MyClass>).
 *
 *   class MyClass : public MyJavaClass::Natives<MyClass>
 *   {
 *       // ...
 *
 *   public:
 *       using MyJavaClass::Natives<MyClass>::Dispose;
 *
 *       static void AttachTo(const MyJavaClass::LocalRef& instance)
 *       {
 *           MyJavaClass::Natives<MyClass>::AttachInstance(
 *                   instance, mozilla::MakeUnique<MyClass>());
 *
 *           // "instance" owns the newly created C++ object, so the C++
 *           // object is destroyed as soon as instance.dispose() is called.
 *       }
 *   };
 */

namespace {

uintptr_t CheckNativeHandle(JNIEnv* env, uintptr_t handle)
{
    if (!handle) {
        if (!env->ExceptionCheck()) {
            ThrowException(env, "java/lang/NullPointerException",
                           "Null native pointer");
        }
        return 0;
    }
    return handle;
}

template<class Impl, bool UseWeakPtr = mozilla::IsBaseOf<
                         SupportsWeakPtr<Impl>, Impl>::value /* = false */>
struct NativePtr
{
    static Impl* Get(JNIEnv* env, jobject instance)
    {
        return reinterpret_cast<Impl*>(CheckNativeHandle(
                env, GetNativeHandle(env, instance)));
    }

    template<class LocalRef>
    static Impl* Get(const LocalRef& instance)
    {
        return Get(instance.Env(), instance.Get());
    }

    template<class LocalRef>
    static void Set(const LocalRef& instance, UniquePtr<Impl>&& ptr)
    {
        Clear(instance);
        SetNativeHandle(instance.Env(), instance.Get(),
                          reinterpret_cast<uintptr_t>(ptr.release()));
        HandleUncaughtException(instance.Env());
    }

    template<class LocalRef>
    static void Clear(const LocalRef& instance)
    {
        UniquePtr<Impl> ptr(reinterpret_cast<Impl*>(
                GetNativeHandle(instance.Env(), instance.Get())));
        HandleUncaughtException(instance.Env());

        if (ptr) {
            SetNativeHandle(instance.Env(), instance.Get(), 0);
            HandleUncaughtException(instance.Env());
        }
    }
};

template<class Impl>
struct NativePtr<Impl, /* UseWeakPtr = */ true>
{
    static Impl* Get(JNIEnv* env, jobject instance)
    {
        const auto ptr = reinterpret_cast<WeakPtr<Impl>*>(
                CheckNativeHandle(env, GetNativeHandle(env, instance)));
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

    template<class LocalRef>
    static Impl* Get(const LocalRef& instance)
    {
        return Get(instance.Env(), instance.Get());
    }

    template<class LocalRef>
    static void Set(const LocalRef& instance, SupportsWeakPtr<Impl>* ptr)
    {
        Clear(instance);
        SetNativeHandle(instance.Env(), instance.Get(),
                          reinterpret_cast<uintptr_t>(new WeakPtr<Impl>(ptr)));
        HandleUncaughtException(instance.Env());
    }

    template<class LocalRef>
    static void Clear(const LocalRef& instance)
    {
        const auto ptr = reinterpret_cast<WeakPtr<Impl>*>(
                GetNativeHandle(instance.Env(), instance.Get()));
        HandleUncaughtException(instance.Env());

        if (ptr) {
            SetNativeHandle(instance.Env(), instance.Get(), 0);
            HandleUncaughtException(instance.Env());
            delete ptr;
        }
    }
};

} // namespace

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
        Impl* const impl = NativePtr<Impl>::Get(env, instance);
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
        Impl* const impl = NativePtr<Impl>::Get(env, instance);
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
        Impl* const impl = NativePtr<Impl>::Get(env, instance);
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
        Impl* const impl = NativePtr<Impl>::Get(env, instance);
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
        JNIEnv* const env = GetEnvForThread();
        MOZ_ALWAYS_TRUE(!env->RegisterNatives(
                Accessor::EnsureClassRef<Cls>(env),
                 Natives::methods,
                 sizeof(Natives::methods) / sizeof(Natives::methods[0])));
        sInited = true;
    }

protected:

    // Associate a C++ instance with a Java instance.
    static void AttachNative(const typename Cls::LocalRef& instance,
                             SupportsWeakPtr<Impl>* ptr)
    {
        static_assert(mozilla::IsBaseOf<SupportsWeakPtr<Impl>, Impl>::value,
                      "Attach with UniquePtr&& when not using WeakPtr");
        return NativePtr<Impl>::Set(instance, ptr);
    }

    static void AttachNative(const typename Cls::LocalRef& instance,
                             UniquePtr<Impl>&& ptr)
    {
        static_assert(!mozilla::IsBaseOf<SupportsWeakPtr<Impl>, Impl>::value,
                      "Attach with SupportsWeakPtr* when using WeakPtr");
        return NativePtr<Impl>::Set(instance, mozilla::Move(ptr));
    }

    // Get the C++ instance associated with a Java instance.
    // There is always a pending exception if the return value is nullptr.
    static Impl* GetNative(const typename Cls::LocalRef& instance) {
        return NativePtr<Impl>::Get(instance);
    }

    NativeImpl() {
        // Initialize on creation if not already initialized.
        Init();
    }

    void DisposeNative(const typename Cls::LocalRef& instance) {
        NativePtr<Impl>::Clear(instance);
    }
};

// Define static member.
template<class C, class I>
bool NativeImpl<C, I>::sInited;

} // namespace jni
} // namespace mozilla

#endif // mozilla_jni_Natives_h__
