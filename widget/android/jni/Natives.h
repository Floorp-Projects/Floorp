#ifndef mozilla_jni_Natives_h__
#define mozilla_jni_Natives_h__

#include <jni.h>

#include "mozilla/IndexSequence.h"
#include "mozilla/Move.h"
#include "mozilla/Tuple.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/unused.h"
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
        MOZ_CATCH_JNI_EXCEPTION(instance.Env());
    }

    template<class LocalRef>
    static void Clear(const LocalRef& instance)
    {
        UniquePtr<Impl> ptr(reinterpret_cast<Impl*>(
                GetNativeHandle(instance.Env(), instance.Get())));
        MOZ_CATCH_JNI_EXCEPTION(instance.Env());

        if (ptr) {
            SetNativeHandle(instance.Env(), instance.Get(), 0);
            MOZ_CATCH_JNI_EXCEPTION(instance.Env());
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
    static void Set(const LocalRef& instance, Impl* ptr)
    {
        Clear(instance);
        SetNativeHandle(instance.Env(), instance.Get(),
                          reinterpret_cast<uintptr_t>(new WeakPtr<Impl>(ptr)));
        MOZ_CATCH_JNI_EXCEPTION(instance.Env());
    }

    template<class LocalRef>
    static void Clear(const LocalRef& instance)
    {
        const auto ptr = reinterpret_cast<WeakPtr<Impl>*>(
                GetNativeHandle(instance.Env(), instance.Get()));
        MOZ_CATCH_JNI_EXCEPTION(instance.Env());

        if (ptr) {
            SetNativeHandle(instance.Env(), instance.Get(), 0);
            MOZ_CATCH_JNI_EXCEPTION(instance.Env());
            delete ptr;
        }
    }
};

} // namespace

/**
 * For C++ classes whose native methods all return void, they can choose to
 * have the native calls go through a proxy by inheriting from
 * mozilla::jni::UsesNativeCallProxy, and overriding the OnNativeCall member.
 * Subsequently, every native call is automatically wrapped in a functor
 * object, and the object is passed to OnNativeCall. The OnNativeCall
 * implementation can choose to invoke the call, save it, dispatch it to a
 * different thread, etc. Each copy of functor may only be invoked once.
 *
 * class MyClass : public MyJavaClass::Natives<MyClass>
 *               , public mozilla::jni::UsesNativeCallProxy
 * {
 *     // ...
 *
 *     template<class Functor>
 *     class ProxyRunnable final : public Runnable
 *     {
 *         Functor mCall;
 *     public:
 *         ProxyRunnable(Functor&& call) : mCall(mozilla::Move(call)) {}
 *         virtual void run() override { mCall(); }
 *     };
 *
 * public:
 *     template<class Functor>
 *     static void OnNativeCall(Functor&& call)
 *     {
 *         RunOnAnotherThread(new ProxyRunnable(mozilla::Move(call)));
 *     }
 * };
 */

struct UsesNativeCallProxy
{
    template<class Functor>
    static void OnNativeCall(Functor&& call)
    {
        // The default behavior is to invoke the call right away.
        call();
    }
};

namespace {

// ProxyArg is used to handle JNI ref arguments for proxies. Because a proxied
// call may happen outside of the original JNI native call, we must save all
// JNI ref arguments as global refs to avoid the arguments going out of scope.
template<typename T>
struct ProxyArg
{
    static_assert(mozilla::IsPod<T>::value, "T must be primitive type");

    // Primitive types can be saved by value.
    typedef T Type;
    typedef typename TypeAdapter<T>::JNIType JNIType;

    static void Clear(JNIEnv* env, Type&) {}

    static Type From(JNIEnv* env, JNIType val)
    {
        return TypeAdapter<T>::ToNative(env, val);
    }
};

template<class C, typename T>
struct ProxyArg<Ref<C, T>>
{
    // Ref types need to be saved by global ref.
    typedef typename C::GlobalRef Type;
    typedef typename TypeAdapter<Ref<C, T>>::JNIType JNIType;

    static void Clear(JNIEnv* env, Type& ref) { ref.Clear(env); }

    static Type From(JNIEnv* env, JNIType val)
    {
        return Type(env, C::Ref::From(val));
    }
};

template<typename C> struct ProxyArg<const C&> : ProxyArg<C> {};
template<> struct ProxyArg<StringParam> : ProxyArg<String::Ref> {};
template<class C> struct ProxyArg<LocalRef<C>> : ProxyArg<typename C::Ref> {};

// ProxyNativeCall implements the functor object that is passed to
// UsesNativeCallProxy::OnNativeCall
template<class Impl, class Owner, bool IsStatic,
         bool HasThisArg /* has instance/class local ref in the call */,
         typename... Args>
class ProxyNativeCall
{
    template<class T, class I, class A, bool S, bool V>
    friend class NativeStubImpl;

    // "this arg" refers to the Class::LocalRef (for static methods) or
    // Owner::LocalRef (for instance methods) that we optionally (as indicated
    // by HasThisArg) pass into the destination C++ function.
    typedef typename mozilla::Conditional<IsStatic,
            Class, Owner>::Type ThisArgClass;
    typedef typename mozilla::Conditional<IsStatic,
            jclass, jobject>::Type ThisArgJNIType;

    // Type signature of the destination C++ function, which matches the
    // Method template parameter in NativeStubImpl::Wrap.
    typedef typename mozilla::Conditional<IsStatic,
            typename mozilla::Conditional<HasThisArg,
                    void (*) (const Class::LocalRef&, Args...),
                    void (*) (Args...)>::Type,
            typename mozilla::Conditional<HasThisArg,
                    void (Impl::*) (const typename Owner::LocalRef&, Args...),
                    void (Impl::*) (Args...)>::Type>::Type NativeCallType;

    // Destination C++ function.
    NativeCallType mNativeCall;
    // Saved this arg.
    typename ThisArgClass::GlobalRef mThisArg;
    // Saved arguments.
    mozilla::Tuple<typename ProxyArg<Args>::Type...> mArgs;

    ProxyNativeCall(NativeCallType nativeCall,
                    JNIEnv* env,
                    ThisArgJNIType thisArg,
                    typename ProxyArg<Args>::JNIType... args)
        : mNativeCall(nativeCall)
        , mThisArg(env, ThisArgClass::Ref::From(thisArg))
        , mArgs(ProxyArg<Args>::From(env, args)...)
    {}

    // We cannot use IsStatic and HasThisArg directly (without going through
    // extra hoops) because GCC complains about invalid overloads, so we use
    // another pair of template parameters, Static and ThisArg.

    template<bool Static, bool ThisArg, size_t... Indices>
    typename mozilla::EnableIf<Static && ThisArg, void>::Type
    Call(const Class::LocalRef& cls,
         mozilla::IndexSequence<Indices...>) const
    {
        (*mNativeCall)(cls, mozilla::Get<Indices>(mArgs)...);
    }

    template<bool Static, bool ThisArg, size_t... Indices>
    typename mozilla::EnableIf<Static && !ThisArg, void>::Type
    Call(const Class::LocalRef& cls,
         mozilla::IndexSequence<Indices...>) const
    {
        (*mNativeCall)(mozilla::Get<Indices>(mArgs)...);
    }

    template<bool Static, bool ThisArg, size_t... Indices>
    typename mozilla::EnableIf<!Static && ThisArg, void>::Type
    Call(const typename Owner::LocalRef& inst,
         mozilla::IndexSequence<Indices...>) const
    {
        Impl* const impl = NativePtr<Impl>::Get(inst);
        MOZ_CATCH_JNI_EXCEPTION(inst.Env());
        (impl->*mNativeCall)(inst, mozilla::Get<Indices>(mArgs)...);
    }

    template<bool Static, bool ThisArg, size_t... Indices>
    typename mozilla::EnableIf<!Static && !ThisArg, void>::Type
    Call(const typename Owner::LocalRef& inst,
         mozilla::IndexSequence<Indices...>) const
    {
        Impl* const impl = NativePtr<Impl>::Get(inst);
        MOZ_CATCH_JNI_EXCEPTION(inst.Env());
        (impl->*mNativeCall)(mozilla::Get<Indices>(mArgs)...);
    }

    template<size_t... Indices>
    void Clear(JNIEnv* env, mozilla::IndexSequence<Indices...>)
    {
        int dummy[] = {
            (ProxyArg<Args>::Clear(env, Get<Indices>(mArgs)), 0)...
        };
        mozilla::Unused << dummy;
    }

public:
    // The class that implements the call target.
    typedef Impl TargetClass;
    typedef typename ThisArgClass::Param ThisArgType;

    static const bool isStatic = IsStatic;

    ProxyNativeCall(ProxyNativeCall&&) = default;
    ProxyNativeCall(const ProxyNativeCall&) = default;

    // Get class ref for static calls or object ref for instance calls.
    typename ThisArgClass::Param GetThisArg() const { return mThisArg; }

    // Return if target is the given function pointer / pointer-to-member.
    // Because we can only compare pointers of the same type, we use a
    // templated overload that is chosen only if given a different type of
    // pointer than our target pointer type.
    bool IsTarget(NativeCallType call) const { return call == mNativeCall; }
    template<typename T> bool IsTarget(T&&) const { return false; }

    // Redirect the call to another function / class member with the same
    // signature as the original target. Crash if given a wrong signature.
    void SetTarget(NativeCallType call) { mNativeCall = call; }
    template<typename T> void SetTarget(T&&) const { MOZ_CRASH(); }

    void operator()()
    {
        JNIEnv* const env = GetEnvForThread();
        typename ThisArgClass::LocalRef thisArg(env, mThisArg);
        Call<IsStatic, HasThisArg>(
                thisArg, typename IndexSequenceFor<Args...>::Type());

        // Clear all saved global refs. We do this after the call is invoked,
        // and not inside the destructor because we already have a JNIEnv here,
        // so it's more efficient to clear out the saved args here. The
        // downside is that the call can only be invoked once.
        Clear(env, typename IndexSequenceFor<Args...>::Type());
        mThisArg.Clear(env);
    }
};

// We can only use Impl::OnNativeCall when Impl is derived from
// UsesNativeCallProxy, otherwise it's a compile error. Therefore, the real
// Dispatch function is conditional on UsesNativeCallProxy being a base class
// of Impl. Otherwise, the dummy Dispatch function below that is chosen during
// overload resolution. Because Dispatch is called with an rvalue, the &&
// version is always chosen before the const& version, if possible.

template<class Impl, class O, bool S, bool V, typename... A>
typename mozilla::EnableIf<
        mozilla::IsBaseOf<UsesNativeCallProxy, Impl>::value, void>::Type
Dispatch(ProxyNativeCall<Impl, O, S, V, A...>&& call)
{
    Impl::OnNativeCall(mozilla::Move(call));
}

template<typename T>
void Dispatch(const T&) {}

} // namespace

template<class Cls, class Impl> class NativeImpl;

namespace detail {

// Wrapper methods that convert arguments from the JNI types to the native
// types, e.g. from jobject to jni::Object::Ref. For instance methods, the
// wrapper methods also convert calls to calls on objects.
//
// We need specialization for static/non-static because the two have different
// signatures (jobject vs jclass and Impl::*Method vs *Method).
// We need specialization for return type, because void return type requires
// us to not deal with the return value.

template<class Traits, class Impl, class Args, bool IsStatic, bool IsVoid>
class NativeStubImpl;

// Bug 1207642 - Work around Dalvik bug by realigning stack on JNI entry
#ifdef __i386__
#define MOZ_JNICALL JNICALL __attribute__((force_align_arg_pointer))
#else
#define MOZ_JNICALL JNICALL
#endif

// Specialization for instance methods with non-void return type
template<class Traits, class Impl, typename... Args>
class NativeStubImpl<Traits, Impl, jni::Args<Args...>,
                     /* IsStatic = */ false, /* IsVoid = */ false>
{
    typedef typename Traits::Owner Owner;
    typedef typename Traits::ReturnType ReturnType;
    typedef typename TypeAdapter<ReturnType>::JNIType ReturnJNIType;

public:
    // Instance method
    template<ReturnType (Impl::*Method) (Args...)>
    static MOZ_JNICALL ReturnJNIType Wrap(JNIEnv* env,
            jobject instance, typename TypeAdapter<Args>::JNIType... args)
    {
        static_assert(!mozilla::IsBaseOf<UsesNativeCallProxy, Impl>::value,
                      "Native call proxy only supports void return type");

        Impl* const impl = NativePtr<Impl>::Get(env, instance);
        if (!impl) {
            return ReturnJNIType();
        }
        return TypeAdapter<ReturnType>::FromNative(env,
                (impl->*Method)(TypeAdapter<Args>::ToNative(env, args)...));
    }

    // Instance method with instance reference
    template<ReturnType (Impl::*Method) (const typename Owner::LocalRef&, Args...)>
    static MOZ_JNICALL ReturnJNIType Wrap(JNIEnv* env,
            jobject instance, typename TypeAdapter<Args>::JNIType... args)
    {
        static_assert(!mozilla::IsBaseOf<UsesNativeCallProxy, Impl>::value,
                      "Native call proxy only supports void return type");

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
class NativeStubImpl<Traits, Impl, jni::Args<Args...>,
                     /* IsStatic = */ false, /* IsVoid = */ true>
{
    typedef typename Traits::Owner Owner;

public:
    // Instance method
    template<void (Impl::*Method) (Args...)>
    static MOZ_JNICALL void Wrap(JNIEnv* env,
            jobject instance, typename TypeAdapter<Args>::JNIType... args)
    {
        if (mozilla::IsBaseOf<UsesNativeCallProxy, Impl>::value) {
            Dispatch(ProxyNativeCall<
                     Impl, Owner, /* IsStatic */ false, /* HasThisArg */ false,
                     Args...>(Method, env, instance, args...));
            return;
        }
        Impl* const impl = NativePtr<Impl>::Get(env, instance);
        if (!impl) {
            return;
        }
        (impl->*Method)(TypeAdapter<Args>::ToNative(env, args)...);
    }

    // Instance method with instance reference
    template<void (Impl::*Method) (const typename Owner::LocalRef&, Args...)>
    static MOZ_JNICALL void Wrap(JNIEnv* env,
            jobject instance, typename TypeAdapter<Args>::JNIType... args)
    {
        if (mozilla::IsBaseOf<UsesNativeCallProxy, Impl>::value) {
            Dispatch(ProxyNativeCall<
                     Impl, Owner, /* IsStatic */ false, /* HasThisArg */ true,
                     Args...>(Method, env, instance, args...));
            return;
        }
        Impl* const impl = NativePtr<Impl>::Get(env, instance);
        if (!impl) {
            return;
        }
        auto self = Owner::LocalRef::Adopt(env, instance);
        (impl->*Method)(self, TypeAdapter<Args>::ToNative(env, args)...);
        self.Forget();
    }

    // Overload for DisposeNative
    template<void (*DisposeNative) (const typename Owner::LocalRef&)>
    static MOZ_JNICALL void Wrap(JNIEnv* env, jobject instance)
    {
        if (mozilla::IsBaseOf<UsesNativeCallProxy, Impl>::value) {
            auto cls = Class::LocalRef::Adopt(
                    env, env->GetObjectClass(instance));
            Dispatch(ProxyNativeCall<Impl, Owner, /* IsStatic */ true,
                    /* HasThisArg */ false, const typename Owner::LocalRef&>(
                    DisposeNative, env, cls.Get(), instance));
            return;
        }
        auto self = Owner::LocalRef::Adopt(env, instance);
        (Impl::DisposeNative)(self);
        self.Forget();
    }
};

// Specialization for static methods with non-void return type
template<class Traits, class Impl, typename... Args>
class NativeStubImpl<Traits, Impl, jni::Args<Args...>,
                     /* IsStatic = */ true, /* IsVoid = */ false>
{
    typedef typename Traits::ReturnType ReturnType;
    typedef typename TypeAdapter<ReturnType>::JNIType ReturnJNIType;

public:
    // Static method
    template<ReturnType (*Method) (Args...)>
    static MOZ_JNICALL ReturnJNIType Wrap(JNIEnv* env,
            jclass, typename TypeAdapter<Args>::JNIType... args)
    {
        static_assert(!mozilla::IsBaseOf<UsesNativeCallProxy, Impl>::value,
                      "Native call proxy only supports void return type");

        return TypeAdapter<ReturnType>::FromNative(env,
                (*Method)(TypeAdapter<Args>::ToNative(env, args)...));
    }

    // Static method with class reference
    template<ReturnType (*Method) (const Class::LocalRef&, Args...)>
    static MOZ_JNICALL ReturnJNIType Wrap(JNIEnv* env,
            jclass cls, typename TypeAdapter<Args>::JNIType... args)
    {
        static_assert(!mozilla::IsBaseOf<UsesNativeCallProxy, Impl>::value,
                      "Native call proxy only supports void return type");

        auto clazz = Class::LocalRef::Adopt(env, cls);
        const auto res = TypeAdapter<ReturnType>::FromNative(env,
                (*Method)(clazz, TypeAdapter<Args>::ToNative(env, args)...));
        clazz.Forget();
        return res;
    }
};

// Specialization for static methods with void return type
template<class Traits, class Impl, typename... Args>
class NativeStubImpl<Traits, Impl, jni::Args<Args...>,
                     /* IsStatic = */ true, /* IsVoid = */ true>
{
    typedef typename Traits::Owner Owner;

public:
    // Static method
    template<void (*Method) (Args...)>
    static MOZ_JNICALL void Wrap(JNIEnv* env,
            jclass cls, typename TypeAdapter<Args>::JNIType... args)
    {
        if (mozilla::IsBaseOf<UsesNativeCallProxy, Impl>::value) {
            Dispatch(ProxyNativeCall<
                    Impl, Owner, /* IsStatic */ true, /* HasThisArg */ false,
                    Args...>(Method, env, cls, args...));
            return;
        }
        (*Method)(TypeAdapter<Args>::ToNative(env, args)...);
    }

    // Static method with class reference
    template<void (*Method) (const Class::LocalRef&, Args...)>
    static MOZ_JNICALL void Wrap(JNIEnv* env,
            jclass cls, typename TypeAdapter<Args>::JNIType... args)
    {
        if (mozilla::IsBaseOf<UsesNativeCallProxy, Impl>::value) {
            Dispatch(ProxyNativeCall<
                    Impl, Owner, /* IsStatic */ true, /* HasThisArg */ true,
                    Args...>(Method, env, cls, args...));
            return;
        }
        auto clazz = Class::LocalRef::Adopt(env, cls);
        (*Method)(clazz, TypeAdapter<Args>::ToNative(env, args)...);
        clazz.Forget();
    }
};

} // namespace detail

// Form a stub wrapper from a native method's traits class and an implementing
// class. The stub wrapper has a Wrap function that will form a wrapped stub.
template<class Traits, class Impl>
struct NativeStub : detail::NativeStubImpl
    <Traits, Impl, typename Traits::Args, Traits::isStatic,
     mozilla::IsVoid<typename Traits::ReturnType>::value>
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
        const auto& ctx = typename Cls::Context();
        ctx.Env()->RegisterNatives(
                ctx.ClassRef(), Natives::methods,
                sizeof(Natives::methods) / sizeof(Natives::methods[0]));
        MOZ_CATCH_JNI_EXCEPTION(ctx.Env());
        sInited = true;
    }

protected:

    // Associate a C++ instance with a Java instance.
    static void AttachNative(const typename Cls::LocalRef& instance,
                             SupportsWeakPtr<Impl>* ptr)
    {
        static_assert(mozilla::IsBaseOf<SupportsWeakPtr<Impl>, Impl>::value,
                      "Attach with UniquePtr&& when not using WeakPtr");
        return NativePtr<Impl>::Set(instance, static_cast<Impl*>(ptr));
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

    static void DisposeNative(const typename Cls::LocalRef& instance) {
        NativePtr<Impl>::Clear(instance);
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
