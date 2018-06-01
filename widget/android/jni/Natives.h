#ifndef mozilla_jni_Natives_h__
#define mozilla_jni_Natives_h__

#include <jni.h>
#include <utility>

#include "nsThreadUtils.h"

#include "mozilla/Move.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Tuple.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/Unused.h"
#include "mozilla/jni/Accessors.h"
#include "mozilla/jni/Refs.h"
#include "mozilla/jni/Types.h"
#include "mozilla/jni/Utils.h"

struct NativeException {
    const char* str;
};

template<class T> static
NativeException NullHandle()
{
    return { __func__ };
}

template<class T> static
NativeException NullWeakPtr()
{
    return { __func__ };
}

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
 *   referenced. Note that mozilla::SupportsWeakPtr only supports being used on
 *   a single thread. To attach a Java instance to a C++ instance, pass in a
 *   mozilla::SupportsWeakPtr pointer to the C++ class (i.e. MyClass*).
 *
 *   class MyClass : public SupportsWeakPtr<MyClass>
 *                 , public MyJavaClass::Natives<MyClass>
 *   {
 *       // ...
 *
 *   public:
 *       MOZ_DECLARE_WEAKREFERENCE_TYPENAME(MyClass)
 *       using MyJavaClass::Natives<MyClass>::DisposeNative;
 *
 *       void AttachTo(const MyJavaClass::LocalRef& instance)
 *       {
 *           MyJavaClass::Natives<MyClass>::AttachNative(
 *                   instance, static_cast<SupportsWeakPtr<MyClass>*>(this));
 *
 *           // "instance" does NOT own "this", so the C++ object
 *           // lifetime is separate from the Java object lifetime.
 *       }
 *   };
 *
 * * If the C++ class contains public members AddRef() and Release(), the Java
 *   instance will store and own the pointer to a RefPtr object, which holds a
 *   strong reference on the C++ instance. Normal ref-counting considerations
 *   apply in this case; for example, disposing may cause the C++ instance to
 *   be deleted and the destructor to be run on the current thread, which may
 *   not be desirable. To attach a Java instance to a C++ instance, pass in a
 *   pointer to the C++ class (i.e. MyClass*).
 *
 *   class MyClass : public RefCounted<MyClass>
 *                 , public MyJavaClass::Natives<MyClass>
 *   {
 *       // ...
 *
 *   public:
 *       using MyJavaClass::Natives<MyClass>::DisposeNative;
 *
 *       void AttachTo(const MyJavaClass::LocalRef& instance)
 *       {
 *           MyJavaClass::Natives<MyClass>::AttachNative(instance, this);
 *
 *           // "instance" owns "this" through the RefPtr, so the C++ object
 *           // may be destroyed as soon as instance.disposeNative() is called.
 *       }
 *   };
 *
 * * In other cases, the Java instance will store and own a pointer to the C++
 *   object itself. This pointer must not be stored or deleted elsewhere. To
 *   attach a Java instance to a C++ instance, pass in a reference to a
 *   UniquePtr of the C++ class (i.e. UniquePtr<MyClass>).
 *
 *   class MyClass : public MyJavaClass::Natives<MyClass>
 *   {
 *       // ...
 *
 *   public:
 *       using MyJavaClass::Natives<MyClass>::DisposeNative;
 *
 *       static void AttachTo(const MyJavaClass::LocalRef& instance)
 *       {
 *           MyJavaClass::Natives<MyClass>::AttachNative(
 *                   instance, mozilla::MakeUnique<MyClass>());
 *
 *           // "instance" owns the newly created C++ object, so the C++
 *           // object is destroyed as soon as instance.disposeNative() is
 *           // called.
 *       }
 *   };
 */

namespace detail {

enum NativePtrType
{
    OWNING,
    WEAK,
    REFPTR
};

template<class Impl>
class NativePtrPicker
{
    template<class I> static typename EnableIf<
            IsBaseOf<SupportsWeakPtr<I>, I>::value,
            char(&)[NativePtrType::WEAK]>::Type Test(char);

    template<class I, typename = decltype(&I::AddRef, &I::Release)>
            static char (&Test(int))[NativePtrType::REFPTR];

    template<class> static char (&Test(...))[NativePtrType::OWNING];

public:
    static const int value = sizeof(Test<Impl>('\0')) / sizeof(char);
};

template<class Impl>
inline uintptr_t CheckNativeHandle(JNIEnv* env, uintptr_t handle)
{
    if (!handle) {
        if (!env->ExceptionCheck()) {
            ThrowException(env,
                           "java/lang/NullPointerException",
                           NullHandle<Impl>().str);
        }
        return 0;
    }
    return handle;
}

template<class Impl, int Type = NativePtrPicker<Impl>::value> struct NativePtr;

template<class Impl>
struct NativePtr<Impl, /* Type = */ NativePtrType::OWNING>
{
    static Impl* Get(JNIEnv* env, jobject instance)
    {
        return reinterpret_cast<Impl*>(CheckNativeHandle<Impl>(
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
struct NativePtr<Impl, /* Type = */ NativePtrType::WEAK>
{
    static Impl* Get(JNIEnv* env, jobject instance)
    {
        const auto ptr = reinterpret_cast<WeakPtr<Impl>*>(
                CheckNativeHandle<Impl>(env, GetNativeHandle(env, instance)));
        if (!ptr) {
            return nullptr;
        }

        Impl* const impl = *ptr;
        if (!impl) {
            ThrowException(env,
                           "java/lang/NullPointerException",
                           NullWeakPtr<Impl>().str);
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

template<class Impl>
struct NativePtr<Impl, /* Type = */ NativePtrType::REFPTR>
{
    static Impl* Get(JNIEnv* env, jobject instance)
    {
        const auto ptr = reinterpret_cast<RefPtr<Impl>*>(
                CheckNativeHandle<Impl>(env, GetNativeHandle(env, instance)));
        if (!ptr) {
            return nullptr;
        }

        MOZ_ASSERT(*ptr);
        return *ptr;
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
                        reinterpret_cast<uintptr_t>(new RefPtr<Impl>(ptr)));
        MOZ_CATCH_JNI_EXCEPTION(instance.Env());
    }

    template<class LocalRef>
    static void Clear(const LocalRef& instance)
    {
        const auto ptr = reinterpret_cast<RefPtr<Impl>*>(
                GetNativeHandle(instance.Env(), instance.Get()));
        MOZ_CATCH_JNI_EXCEPTION(instance.Env());

        if (ptr) {
            SetNativeHandle(instance.Env(), instance.Get(), 0);
            MOZ_CATCH_JNI_EXCEPTION(instance.Env());
            delete ptr;
        }
    }
};

} // namespace detail

using namespace detail;

/**
 * For JNI native methods that are dispatched to a proxy, i.e. using
 * @WrapForJNI(dispatchTo = "proxy"), the implementing C++ class must provide a
 * OnNativeCall member. Subsequently, every native call is automatically
 * wrapped in a functor object, and the object is passed to OnNativeCall. The
 * OnNativeCall implementation can choose to invoke the call, save it, dispatch
 * it to a different thread, etc. Each copy of functor may only be invoked
 * once.
 *
 * class MyClass : public MyJavaClass::Natives<MyClass>
 * {
 *     // ...
 *
 *     template<class Functor>
 *     class ProxyRunnable final : public Runnable
 *     {
 *         Functor mCall;
 *     public:
 *         ProxyRunnable(Functor&& call) : mCall(std::move(call)) {}
 *         virtual void run() override { mCall(); }
 *     };
 *
 * public:
 *     template<class Functor>
 *     static void OnNativeCall(Functor&& call)
 *     {
 *         RunOnAnotherThread(new ProxyRunnable(std::move(call)));
 *     }
 * };
 */

namespace detail {

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

// ProxyNativeCall implements the functor object that is passed to OnNativeCall
template<class Impl, class Owner, bool IsStatic,
         bool HasThisArg /* has instance/class local ref in the call */,
         typename... Args>
class ProxyNativeCall
{
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

    // We cannot use IsStatic and HasThisArg directly (without going through
    // extra hoops) because GCC complains about invalid overloads, so we use
    // another pair of template parameters, Static and ThisArg.

    template<bool Static, bool ThisArg, size_t... Indices>
    typename mozilla::EnableIf<Static && ThisArg, void>::Type
    Call(const Class::LocalRef& cls,
         std::index_sequence<Indices...>) const
    {
        (*mNativeCall)(cls, mozilla::Get<Indices>(mArgs)...);
    }

    template<bool Static, bool ThisArg, size_t... Indices>
    typename mozilla::EnableIf<Static && !ThisArg, void>::Type
    Call(const Class::LocalRef& cls,
         std::index_sequence<Indices...>) const
    {
        (*mNativeCall)(mozilla::Get<Indices>(mArgs)...);
    }

    template<bool Static, bool ThisArg, size_t... Indices>
    typename mozilla::EnableIf<!Static && ThisArg, void>::Type
    Call(const typename Owner::LocalRef& inst,
         std::index_sequence<Indices...>) const
    {
        Impl* const impl = NativePtr<Impl>::Get(inst);
        MOZ_CATCH_JNI_EXCEPTION(inst.Env());
        (impl->*mNativeCall)(inst, mozilla::Get<Indices>(mArgs)...);
    }

    template<bool Static, bool ThisArg, size_t... Indices>
    typename mozilla::EnableIf<!Static && !ThisArg, void>::Type
    Call(const typename Owner::LocalRef& inst,
         std::index_sequence<Indices...>) const
    {
        Impl* const impl = NativePtr<Impl>::Get(inst);
        MOZ_CATCH_JNI_EXCEPTION(inst.Env());
        (impl->*mNativeCall)(mozilla::Get<Indices>(mArgs)...);
    }

    template<size_t... Indices>
    void Clear(JNIEnv* env, std::index_sequence<Indices...>)
    {
        int dummy[] = {
            (ProxyArg<Args>::Clear(env, Get<Indices>(mArgs)), 0)...
        };
        mozilla::Unused << dummy;
    }

    static Impl* GetNativeObject(Class::Param thisArg) { return nullptr; }

    static Impl* GetNativeObject(typename Owner::Param thisArg)
    {
        return NativePtr<Impl>::Get(GetEnvForThread(), thisArg.Get());
    }

public:
    // The class that implements the call target.
    typedef Impl TargetClass;
    typedef typename ThisArgClass::Param ThisArgType;

    static const bool isStatic = IsStatic;

    ProxyNativeCall(ThisArgJNIType thisArg,
                    NativeCallType nativeCall,
                    JNIEnv* env,
                    typename ProxyArg<Args>::JNIType... args)
        : mNativeCall(nativeCall)
        , mThisArg(env, ThisArgClass::Ref::From(thisArg))
        , mArgs(ProxyArg<Args>::From(env, args)...)
    {}

    ProxyNativeCall(ProxyNativeCall&&) = default;
    ProxyNativeCall(const ProxyNativeCall&) = default;

    // Get class ref for static calls or object ref for instance calls.
    typename ThisArgClass::Param GetThisArg() const { return mThisArg; }

    // Get the native object targeted by this call.
    // Returns nullptr for static calls.
    Impl* GetNativeObject() const { return GetNativeObject(mThisArg); }

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
                thisArg, std::index_sequence_for<Args...>{});

        // Clear all saved global refs. We do this after the call is invoked,
        // and not inside the destructor because we already have a JNIEnv here,
        // so it's more efficient to clear out the saved args here. The
        // downside is that the call can only be invoked once.
        Clear(env, std::index_sequence_for<Args...>{});
        mThisArg.Clear(env);
    }
};

template<class Impl, bool HasThisArg, typename... Args>
struct Dispatcher
{
    template<class Traits, bool IsStatic = Traits::isStatic,
             typename... ProxyArgs>
    static typename EnableIf<
            Traits::dispatchTarget == DispatchTarget::PROXY, void>::Type
    Run(ProxyArgs&&... args)
    {
        Impl::OnNativeCall(ProxyNativeCall<
                Impl, typename Traits::Owner, IsStatic,
                HasThisArg, Args...>(std::forward<ProxyArgs>(args)...));
    }

    template<class Traits, bool IsStatic = Traits::isStatic,
             typename ThisArg, typename... ProxyArgs>
    static typename EnableIf<
            Traits::dispatchTarget == DispatchTarget::GECKO_PRIORITY, void>::Type
    Run(ThisArg thisArg, ProxyArgs&&... args)
    {
        // For a static method, do not forward the "this arg" (i.e. the class
        // local ref) if the implementation does not request it. This saves us
        // a pair of calls to add/delete global ref.
        auto proxy = ProxyNativeCall<Impl, typename Traits::Owner, IsStatic,
                                     HasThisArg, Args...>(
                (HasThisArg || !IsStatic) ? thisArg : nullptr,
                std::forward<ProxyArgs>(args)...);
        DispatchToGeckoPriorityQueue(
                NS_NewRunnableFunction("PriorityNativeCall", std::move(proxy)));
    }

    template<class Traits, bool IsStatic = Traits::isStatic,
             typename ThisArg, typename... ProxyArgs>
    static typename EnableIf<
            Traits::dispatchTarget == DispatchTarget::GECKO, void>::Type
    Run(ThisArg thisArg, ProxyArgs&&... args)
    {
        // For a static method, do not forward the "this arg" (i.e. the class
        // local ref) if the implementation does not request it. This saves us
        // a pair of calls to add/delete global ref.
        auto proxy = ProxyNativeCall<Impl, typename Traits::Owner, IsStatic,
                                     HasThisArg, Args...>(
                (HasThisArg || !IsStatic) ? thisArg : nullptr,
                std::forward<ProxyArgs>(args)...);
        NS_DispatchToMainThread(
                NS_NewRunnableFunction("GeckoNativeCall", std::move(proxy)));
    }

    template<class Traits, bool IsStatic = false, typename... ProxyArgs>
    static typename EnableIf<
            Traits::dispatchTarget == DispatchTarget::CURRENT, void>::Type
    Run(ProxyArgs&&... args)
    {
        MOZ_CRASH("Unreachable code");
    }
};

} // namespace detail

// Wrapper methods that convert arguments from the JNI types to the native
// types, e.g. from jobject to jni::Object::Ref. For instance methods, the
// wrapper methods also convert calls to calls on objects.
//
// We need specialization for static/non-static because the two have different
// signatures (jobject vs jclass and Impl::*Method vs *Method).
// We need specialization for return type, because void return type requires
// us to not deal with the return value.

// Bug 1207642 - Work around Dalvik bug by realigning stack on JNI entry
#ifdef __i386__
#define MOZ_JNICALL JNICALL __attribute__((force_align_arg_pointer))
#else
#define MOZ_JNICALL JNICALL
#endif

template<class Traits, class Impl, class Args = typename Traits::Args>
class NativeStub;

template<class Traits, class Impl, typename... Args>
class NativeStub<Traits, Impl, jni::Args<Args...>>
{
    using Owner = typename Traits::Owner;
    using ReturnType = typename Traits::ReturnType;

    static constexpr bool isStatic = Traits::isStatic;
    static constexpr bool isVoid = mozilla::IsVoid<ReturnType>::value;

    struct VoidType { using JNIType = void; };
    using ReturnJNIType = typename Conditional<
            isVoid, VoidType, TypeAdapter<ReturnType>>::Type::JNIType;

    using ReturnTypeForNonVoidInstance = typename Conditional<
            !isStatic && !isVoid, ReturnType, VoidType>::Type;
    using ReturnTypeForVoidInstance = typename Conditional<
            !isStatic && isVoid, ReturnType, VoidType&>::Type;
    using ReturnTypeForNonVoidStatic = typename Conditional<
            isStatic && !isVoid, ReturnType, VoidType>::Type;
    using ReturnTypeForVoidStatic = typename Conditional<
            isStatic && isVoid, ReturnType, VoidType&>::Type;

    static_assert(Traits::dispatchTarget == DispatchTarget::CURRENT || isVoid,
                  "Dispatched calls must have void return type");

public:
    // Non-void instance method
    template<ReturnTypeForNonVoidInstance (Impl::*Method) (Args...)>
    static MOZ_JNICALL ReturnJNIType
    Wrap(JNIEnv* env, jobject instance,
         typename TypeAdapter<Args>::JNIType... args)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

        Impl* const impl = NativePtr<Impl>::Get(env, instance);
        if (!impl) {
            // There is a pending JNI exception at this point.
            return ReturnJNIType();
        }
        return TypeAdapter<ReturnType>::FromNative(env,
                (impl->*Method)(TypeAdapter<Args>::ToNative(env, args)...));
    }

    // Non-void instance method with instance reference
    template<ReturnTypeForNonVoidInstance (Impl::*Method)
             (const typename Owner::LocalRef&, Args...)>
    static MOZ_JNICALL ReturnJNIType
    Wrap(JNIEnv* env, jobject instance,
         typename TypeAdapter<Args>::JNIType... args)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

        Impl* const impl = NativePtr<Impl>::Get(env, instance);
        if (!impl) {
            // There is a pending JNI exception at this point.
            return ReturnJNIType();
        }
        auto self = Owner::LocalRef::Adopt(env, instance);
        const auto res = TypeAdapter<ReturnType>::FromNative(env,
                (impl->*Method)(self, TypeAdapter<Args>::ToNative(env, args)...));
        self.Forget();
        return res;
    }

    // Void instance method
    template<ReturnTypeForVoidInstance (Impl::*Method) (Args...)>
    static MOZ_JNICALL void
    Wrap(JNIEnv* env, jobject instance,
         typename TypeAdapter<Args>::JNIType... args)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

        if (Traits::dispatchTarget != DispatchTarget::CURRENT) {
            Dispatcher<Impl, /* HasThisArg */ false, Args...>::
                    template Run<Traits>(instance, Method, env, args...);
            return;
        }

        Impl* const impl = NativePtr<Impl>::Get(env, instance);
        if (!impl) {
            // There is a pending JNI exception at this point.
            return;
        }
        (impl->*Method)(TypeAdapter<Args>::ToNative(env, args)...);
    }

    // Void instance method with instance reference
    template<ReturnTypeForVoidInstance (Impl::*Method)
             (const typename Owner::LocalRef&, Args...)>
    static MOZ_JNICALL void
    Wrap(JNIEnv* env, jobject instance,
         typename TypeAdapter<Args>::JNIType... args)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

        if (Traits::dispatchTarget != DispatchTarget::CURRENT) {
            Dispatcher<Impl, /* HasThisArg */ true, Args...>::
                    template Run<Traits>(instance, Method, env, args...);
            return;
        }

        Impl* const impl = NativePtr<Impl>::Get(env, instance);
        if (!impl) {
            // There is a pending JNI exception at this point.
            return;
        }
        auto self = Owner::LocalRef::Adopt(env, instance);
        (impl->*Method)(self, TypeAdapter<Args>::ToNative(env, args)...);
        self.Forget();
    }

    // Overload for DisposeNative
    template<ReturnTypeForVoidInstance (*DisposeNative)
             (const typename Owner::LocalRef&)>
    static MOZ_JNICALL void
    Wrap(JNIEnv* env, jobject instance)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

        if (Traits::dispatchTarget != DispatchTarget::CURRENT) {
            using LocalRef = typename Owner::LocalRef;
            Dispatcher<Impl, /* HasThisArg */ false, const LocalRef&>::
                    template Run<Traits, /* IsStatic */ true>(
                    /* ThisArg */ nullptr, DisposeNative, env, instance);
            return;
        }

        auto self = Owner::LocalRef::Adopt(env, instance);
        DisposeNative(self);
        self.Forget();
    }

    // Non-void static method
    template<ReturnTypeForNonVoidStatic (*Method) (Args...)>
    static MOZ_JNICALL ReturnJNIType
    Wrap(JNIEnv* env, jclass, typename TypeAdapter<Args>::JNIType... args)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

        return TypeAdapter<ReturnType>::FromNative(env,
                (*Method)(TypeAdapter<Args>::ToNative(env, args)...));
    }

    // Non-void static method with class reference
    template<ReturnTypeForNonVoidStatic (*Method)
             (const Class::LocalRef&, Args...)>
    static MOZ_JNICALL ReturnJNIType
    Wrap(JNIEnv* env, jclass cls, typename TypeAdapter<Args>::JNIType... args)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

        auto clazz = Class::LocalRef::Adopt(env, cls);
        const auto res = TypeAdapter<ReturnType>::FromNative(env,
                (*Method)(clazz, TypeAdapter<Args>::ToNative(env, args)...));
        clazz.Forget();
        return res;
    }

    // Void static method
    template<ReturnTypeForVoidStatic (*Method) (Args...)>
    static MOZ_JNICALL void
    Wrap(JNIEnv* env, jclass cls, typename TypeAdapter<Args>::JNIType... args)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

        if (Traits::dispatchTarget != DispatchTarget::CURRENT) {
            Dispatcher<Impl, /* HasThisArg */ false, Args...>::
                    template Run<Traits>(cls, Method, env, args...);
            return;
        }

        (*Method)(TypeAdapter<Args>::ToNative(env, args)...);
    }

    // Void static method with class reference
    template<ReturnTypeForVoidStatic (*Method)
             (const Class::LocalRef&, Args...)>
    static MOZ_JNICALL void
    Wrap(JNIEnv* env, jclass cls, typename TypeAdapter<Args>::JNIType... args)
    {
        MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

        if (Traits::dispatchTarget != DispatchTarget::CURRENT) {
            Dispatcher<Impl, /* HasThisArg */ true, Args...>::
                    template Run<Traits>(cls, Method, env, args...);
            return;
        }

        auto clazz = Class::LocalRef::Adopt(env, cls);
        (*Method)(clazz, TypeAdapter<Args>::ToNative(env, args)...);
        clazz.Forget();
    }
};

// Generate a JNINativeMethod from a native
// method's traits class and a wrapped stub.
template<class Traits, typename Ret, typename... Args>
constexpr JNINativeMethod MakeNativeMethod(MOZ_JNICALL Ret (*stub)(JNIEnv*, Args...))
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
        static_assert(NativePtrPicker<Impl>::value == NativePtrType::WEAK,
                      "Use another AttachNative for non-WeakPtr usage");
        return NativePtr<Impl>::Set(instance, static_cast<Impl*>(ptr));
    }

    static void AttachNative(const typename Cls::LocalRef& instance,
                             UniquePtr<Impl>&& ptr)
    {
        static_assert(NativePtrPicker<Impl>::value == NativePtrType::OWNING,
                      "Use another AttachNative for WeakPtr or RefPtr usage");
        return NativePtr<Impl>::Set(instance, std::move(ptr));
    }

    static void AttachNative(const typename Cls::LocalRef& instance, Impl* ptr)
    {
        static_assert(NativePtrPicker<Impl>::value == NativePtrType::REFPTR,
                      "Use another AttachNative for non-RefPtr usage");
        return NativePtr<Impl>::Set(instance, ptr);
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
