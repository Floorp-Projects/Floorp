/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jni_Natives_h__
#define mozilla_jni_Natives_h__

#include <jni.h>

#include <type_traits>
#include <utility>

#include "mozilla/RefPtr.h"
#include "mozilla/RWLock.h"
#include "mozilla/Tuple.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/jni/Accessors.h"
#include "mozilla/jni/Refs.h"
#include "mozilla/jni/Types.h"
#include "mozilla/jni/Utils.h"
#include "nsThreadUtils.h"

#if defined(_MSC_VER)  // MSVC
#  define FUNCTION_SIGNATURE __FUNCSIG__
#elif defined(__GNUC__)  // GCC, Clang
#  define FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#endif

struct NativeException {
  const char* str;
};

template <class T>
static NativeException NullHandle() {
  return {FUNCTION_SIGNATURE};
}

template <class T>
static NativeException NullWeakPtr() {
  return {FUNCTION_SIGNATURE};
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
 *   class MyClass : public SupportsWeakPtr
 *                 , public MyJavaClass::Natives<MyClass>
 *   {
 *       // ...
 *
 *   public:
 *       using MyJavaClass::Natives<MyClass>::DisposeNative;
 *
 *       void AttachTo(const MyJavaClass::LocalRef& instance)
 *       {
 *           MyJavaClass::Natives<MyClass>::AttachNative(
 *                   instance, static_cast<SupportsWeakPtr*>(this));
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

/**
 * Type trait that determines whether a given class has a member named
 * T::OnWeakNonIntrusiveDetach.
 *
 * Example usage:
 * class Foo {};
 * class Bar {
 *  public:
 *   void OnWeakNonIntrusiveDetach(already_AddRefed<nsIRunnable> aRunnable);
 * };
 *
 * constexpr bool foo = HasWeakNonIntrusiveDetach<Foo>::value; // Expect false
 * constexpr bool bar = HasWeakNonIntrusiveDetach<Bar>::value; // Expect true
 */
template <typename, typename = std::void_t<>>
struct HasWeakNonIntrusiveDetach : std::false_type {};

template <typename T>
struct HasWeakNonIntrusiveDetach<
    T, std::void_t<decltype(std::declval<T>().OnWeakNonIntrusiveDetach(
           std::declval<already_AddRefed<nsIRunnable>>()))>> : std::true_type {
};

/**
 * Type trait that determines whether a given class is refcounted, ie. it has
 * both T::AddRef and T::Release methods.
 *
 * Example usage:
 * class Foo {};
 * class Bar {
 *  public:
 *   void AddRef();
 *   void Release();
 * };
 *
 * constexpr bool foo = IsRefCounted<Foo>::value; // Expect false
 * constexpr bool bar = IsRefCounted<Bar>::value; // Expect true
 */
template <typename, typename = std::void_t<>>
struct IsRefCounted : std::false_type {};

template <typename T>
struct IsRefCounted<T, std::void_t<decltype(std::declval<T>().AddRef(),
                                            std::declval<T>().Release())>>
    : std::true_type {};

/**
 * This enum is used for classifying the type of pointer that is stored
 * within a NativeWeakPtr. This classification is different from the one used
 * for normal native pointers.
 */
enum class NativePtrInternalType : size_t {
  OWNING = 1,
  WEAK = 2,
  REFPTR = 3,
};

/**
 * NativePtrInternalPicker uses some C++ SFINAE template-fu to figure out
 * what type of pointer the class specified by Impl needs to be.
 *
 * It does this by supplying multiple overloads of a method named Test.
 * Various overloads are enabled or disabled depending on whether or not Impl
 * can possibly support them.
 *
 * Each overload "returns" a reference to an array whose size corresponds to the
 * value of each enum in NativePtrInternalType. That size is then converted back
 * to the enum value, yielding the right type.
 */
template <class Impl>
class NativePtrInternalPicker {
  // Enable if Impl derives from SupportsWeakPtr, yielding type WEAK
  template <class I>
  static std::enable_if_t<
      std::is_base_of<SupportsWeakPtr, I>::value,
      char (&)[static_cast<size_t>(NativePtrInternalType::WEAK)]>
  Test(char);

  // Enable if Impl implements AddRef and Release, yielding type REFPTR
  template <class I, typename = decltype(&I::AddRef, &I::Release)>
  static char (&Test(int))[static_cast<size_t>(NativePtrInternalType::REFPTR)];

  // This overload uses '...' as its param to make its arguments less specific;
  // the compiler prefers more-specific overloads to less-specific ones.
  // OWNING is the fallback type.
  template <class>
  static char (&Test(...))[static_cast<size_t>(NativePtrInternalType::OWNING)];

 public:
  // Given a hypothetical function call Test<Impl>, convert the size of its
  // resulting array back into a NativePtrInternalType enum value.
  static const NativePtrInternalType value = static_cast<NativePtrInternalType>(
      sizeof(Test<Impl>('\0')) / sizeof(char));
};

/**
 * This enum is used for classifying the type of pointer that is stored in a
 * JNIObject's handle.
 *
 * We have two different weak pointer types:
 *   * WEAK_INTRUSIVE is a pointer to a class that derives from
 *     mozilla::SupportsWeakPtr.
 *   * WEAK_NON_INTRUSIVE is a pointer to a class that does not have any
 *     internal support for weak pointers, but does supply a
 *     OnWeakNonIntrusiveDetach method.
 */
enum class NativePtrType : size_t {
  OWNING = 1,
  WEAK_INTRUSIVE = 2,
  WEAK_NON_INTRUSIVE = 3,
  REFPTR = 4,
};

/**
 * NativePtrPicker uses some C++ SFINAE template-fu to figure out what type of
 * pointer the class specified by Impl needs to be.
 *
 * It does this by supplying multiple overloads of a method named Test.
 * Various overloads are enabled or disabled depending on whether or not Impl
 * can possibly support them.
 *
 * Each overload "returns" a reference to an array whose size corresponds to the
 * value of each enum in NativePtrInternalType. That size is then converted back
 * to the enum value, yielding the right type.
 */
template <class Impl>
class NativePtrPicker {
  // Just shorthand for each overload's return type
  template <NativePtrType PtrType>
  using ResultTypeT = char (&)[static_cast<size_t>(PtrType)];

  // Enable if Impl derives from SupportsWeakPtr, yielding type WEAK_INTRUSIVE
  template <typename I>
  static auto Test(void*)
      -> std::enable_if_t<std::is_base_of<SupportsWeakPtr, I>::value,
                          ResultTypeT<NativePtrType::WEAK_INTRUSIVE>>;

  // Enable if Impl implements OnWeakNonIntrusiveDetach, yielding type
  // WEAK_NON_INTRUSIVE
  template <typename I>
  static auto Test(void*)
      -> std::enable_if_t<HasWeakNonIntrusiveDetach<I>::value,
                          ResultTypeT<NativePtrType::WEAK_NON_INTRUSIVE>>;

  // We want the WEAK_NON_INTRUSIVE overload to take precedence over this one,
  // so we only enable this overload if Impl is refcounted AND it does not
  // implement OnWeakNonIntrusiveDetach. Yields type REFPTR.
  template <typename I>
  static auto Test(void*) -> std::enable_if_t<
      std::conjunction_v<IsRefCounted<I>,
                         std::negation<HasWeakNonIntrusiveDetach<I>>>,
      ResultTypeT<NativePtrType::REFPTR>>;

  // This overload uses '...' as its param to make its arguments less specific;
  // the compiler prefers more-specific overloads to less-specific ones.
  // OWNING is the fallback type.
  template <typename>
  static char (&Test(...))[static_cast<size_t>(NativePtrType::OWNING)];

 public:
  // Given a hypothetical function call Test<Impl>, convert the size of its
  // resulting array back into a NativePtrType enum value.
  static const NativePtrType value =
      static_cast<NativePtrType>(sizeof(Test<Impl>(nullptr)));
};

template <class Impl>
inline uintptr_t CheckNativeHandle(JNIEnv* env, uintptr_t handle) {
  if (!handle) {
    if (!env->ExceptionCheck()) {
      ThrowException(env, "java/lang/NullPointerException",
                     NullHandle<Impl>().str);
    }
    return 0;
  }
  return handle;
}

/**
 * This struct is used to describe various traits of a native pointer of type
 * Impl that will be attached to a JNIObject.
 *
 * See the definition of the NativePtrType::OWNING specialization for comments
 * describing the required fields.
 */
template <class Impl, NativePtrType Type = NativePtrPicker<Impl>::value>
struct NativePtrTraits;

template <class Impl>
struct NativePtrTraits<Impl, /* Type = */ NativePtrType::OWNING> {
  using AccessorType =
      Impl*;  // Pointer-like type returned by Access() (an actual pointer in
              // this case, but this is not strictly necessary)
  using HandleType = Impl*;  // Type of the pointer stored in JNIObject.mHandle
  using RefType = Impl*;     // Type of the pointer returned by Get()

  /**
   * Returns a RefType to the native implementation belonging to
   * the given Java object.
   */
  static RefType Get(JNIEnv* env, jobject instance) {
    static_assert(
        std::is_same<HandleType, RefType>::value,
        "HandleType and RefType must be identical for owning pointers");
    return reinterpret_cast<HandleType>(
        CheckNativeHandle<Impl>(env, GetNativeHandle(env, instance)));
  }

  /**
   * Returns a RefType to the native implementation belonging to
   * the given Java object.
   */
  template <class LocalRef>
  static RefType Get(const LocalRef& instance) {
    return Get(instance.Env(), instance.Get());
  }

  /**
   * Given a RefType, returns the pointer-like AccessorType used for
   * manipulating the native object.
   */
  static AccessorType Access(RefType aImpl, JNIEnv* aEnv = nullptr) {
    static_assert(
        std::is_same<AccessorType, RefType>::value,
        "AccessorType and RefType must be identical for owning pointers");
    return aImpl;
  }

  /**
   * Set the JNIObject's handle to the provided pointer, clearing any previous
   * handle if necessary.
   */
  template <class LocalRef>
  static void Set(const LocalRef& instance, UniquePtr<Impl>&& ptr) {
    Clear(instance);
    SetNativeHandle(instance.Env(), instance.Get(),
                    reinterpret_cast<uintptr_t>(ptr.release()));
    MOZ_CATCH_JNI_EXCEPTION(instance.Env());
  }

  /**
   * Clear the JNIObject's handle.
   */
  template <class LocalRef>
  static void Clear(const LocalRef& instance) {
    UniquePtr<Impl> ptr(reinterpret_cast<RefType>(
        GetNativeHandle(instance.Env(), instance.Get())));
    MOZ_CATCH_JNI_EXCEPTION(instance.Env());

    if (ptr) {
      SetNativeHandle(instance.Env(), instance.Get(), 0);
      MOZ_CATCH_JNI_EXCEPTION(instance.Env());
    }
  }
};

template <class Impl>
struct NativePtrTraits<Impl, /* Type = */ NativePtrType::WEAK_INTRUSIVE> {
  using AccessorType = Impl*;
  using HandleType = WeakPtr<Impl>*;
  using RefType = WeakPtr<Impl>;

  static RefType Get(JNIEnv* env, jobject instance) {
    const auto ptr = reinterpret_cast<HandleType>(
        CheckNativeHandle<Impl>(env, GetNativeHandle(env, instance)));
    return *ptr;
  }

  template <class LocalRef>
  static RefType Get(const LocalRef& instance) {
    return Get(instance.Env(), instance.Get());
  }

  static AccessorType Access(RefType aPtr, JNIEnv* aEnv = nullptr) {
    AccessorType const impl = *aPtr;
    if (!impl) {
      JNIEnv* env = aEnv ? aEnv : mozilla::jni::GetEnvForThread();
      ThrowException(env, "java/lang/NullPointerException",
                     NullWeakPtr<Impl>().str);
    }

    return impl;
  }

  template <class LocalRef>
  static void Set(const LocalRef& instance, Impl* ptr) {
    // Create the new handle first before clearing any old handle, so the
    // new handle is guaranteed to have different value than any old handle.
    const uintptr_t handle =
        reinterpret_cast<uintptr_t>(new WeakPtr<Impl>(ptr));
    Clear(instance);
    SetNativeHandle(instance.Env(), instance.Get(), handle);
    MOZ_CATCH_JNI_EXCEPTION(instance.Env());
  }

  template <class LocalRef>
  static void Clear(const LocalRef& instance) {
    const auto ptr = reinterpret_cast<HandleType>(
        GetNativeHandle(instance.Env(), instance.Get()));
    MOZ_CATCH_JNI_EXCEPTION(instance.Env());

    if (ptr) {
      SetNativeHandle(instance.Env(), instance.Get(), 0);
      MOZ_CATCH_JNI_EXCEPTION(instance.Env());
      delete ptr;
    }
  }
};

template <class Impl>
struct NativePtrTraits<Impl, /* Type = */ NativePtrType::REFPTR> {
  using AccessorType = Impl*;
  using HandleType = RefPtr<Impl>*;
  using RefType = Impl*;

  static RefType Get(JNIEnv* env, jobject instance) {
    const auto ptr = reinterpret_cast<HandleType>(
        CheckNativeHandle<Impl>(env, GetNativeHandle(env, instance)));
    if (!ptr) {
      return nullptr;
    }

    MOZ_ASSERT(*ptr);
    return *ptr;
  }

  template <class LocalRef>
  static RefType Get(const LocalRef& instance) {
    return Get(instance.Env(), instance.Get());
  }

  static AccessorType Access(RefType aImpl, JNIEnv* aEnv = nullptr) {
    static_assert(std::is_same<AccessorType, RefType>::value,
                  "AccessorType and RefType must be identical for refpointers");
    return aImpl;
  }

  template <class LocalRef>
  static void Set(const LocalRef& instance, RefType ptr) {
    // Create the new handle first before clearing any old handle, so the
    // new handle is guaranteed to have different value than any old handle.
    const uintptr_t handle = reinterpret_cast<uintptr_t>(new RefPtr<Impl>(ptr));
    Clear(instance);
    SetNativeHandle(instance.Env(), instance.Get(), handle);
    MOZ_CATCH_JNI_EXCEPTION(instance.Env());
  }

  template <class LocalRef>
  static void Clear(const LocalRef& instance) {
    const auto ptr = reinterpret_cast<HandleType>(
        GetNativeHandle(instance.Env(), instance.Get()));
    MOZ_CATCH_JNI_EXCEPTION(instance.Env());

    if (ptr) {
      SetNativeHandle(instance.Env(), instance.Get(), 0);
      MOZ_CATCH_JNI_EXCEPTION(instance.Env());
      delete ptr;
    }
  }
};

}  // namespace detail

// Forward declarations
template <typename NativeImpl>
class NativeWeakPtr;
template <typename NativeImpl>
class NativeWeakPtrHolder;

namespace detail {

/**
 * Given the class of a native implementation, as well as its
 * NativePtrInternalType, resolve traits for that type that will be used by
 * the NativeWeakPtrControlBlock.
 *
 * Note that we only implement specializations for OWNING and REFPTR types,
 * as a WEAK_INTRUSIVE type should not be using NativeWeakPtr anyway. The build
 * will fail if such an attempt is made.
 *
 * Traits need to implement two things:
 * 1. A |Type| field that resolves to a pointer type to be stored in the
 *    JNIObject's handle. It is assumed that setting a |Type| object to nullptr
 *    is sufficient to delete the underlying object.
 * 2. A static |AsRaw| method that converts a pointer of |Type| into a raw
 *    pointer.
 */
template <
    typename NativeImpl,
    NativePtrInternalType PtrType =
        ::mozilla::jni::detail::NativePtrInternalPicker<NativeImpl>::value>
struct NativeWeakPtrControlBlockStorageTraits;

template <typename NativeImpl>
struct NativeWeakPtrControlBlockStorageTraits<
    NativeImpl, ::mozilla::jni::detail::NativePtrInternalType::OWNING> {
  using Type = UniquePtr<NativeImpl>;

  static NativeImpl* AsRaw(const Type& aStorage) { return aStorage.get(); }
};

template <typename NativeImpl>
struct NativeWeakPtrControlBlockStorageTraits<
    NativeImpl, ::mozilla::jni::detail::NativePtrInternalType::REFPTR> {
  using Type = RefPtr<NativeImpl>;

  static NativeImpl* AsRaw(const Type& aStorage) { return aStorage.get(); }
};

// Forward Declaration
template <typename NativeImpl>
class Accessor;

/**
 * This class contains the shared data that is referenced by all NativeWeakPtr
 * objects that reference the same object.
 *
 * It retains a WeakRef to the Java object that owns this native object.
 * It uses a RWLock to control access to the native pointer itself.
 * Read locks are used when accessing the pointer (even when calling non-const
 * methods on the native object).
 * A write lock is only used when it is time to destroy the native object and
 * we need to clear the value of mNativeImpl.
 */
template <typename NativeImpl>
class MOZ_HEAP_CLASS NativeWeakPtrControlBlock final {
 public:
  using StorageTraits = NativeWeakPtrControlBlockStorageTraits<NativeImpl>;
  using StorageType = typename StorageTraits::Type;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(NativeWeakPtrControlBlock)

  NativeWeakPtrControlBlock(const NativeWeakPtrControlBlock&) = delete;
  NativeWeakPtrControlBlock(NativeWeakPtrControlBlock&&) = delete;
  NativeWeakPtrControlBlock& operator=(const NativeWeakPtrControlBlock&) =
      delete;
  NativeWeakPtrControlBlock& operator=(NativeWeakPtrControlBlock&&) = delete;

  // This is safe to call on any thread because mJavaOwner is immutable.
  mozilla::jni::Object::WeakRef GetJavaOwner() const { return mJavaOwner; }

 private:
  NativeWeakPtrControlBlock(::mozilla::jni::Object::Param aJavaOwner,
                            StorageType&& aNativeImpl)
      : mJavaOwner(aJavaOwner),
        mLock("mozilla::jni::detail::NativeWeakPtrControlBlock"),
        mNativeImpl(std::move(aNativeImpl)) {}

  ~NativeWeakPtrControlBlock() {
    // Make sure that somebody, somewhere, has detached us before destroying.
    MOZ_ASSERT(!(*this));
  }

  /**
   * Clear the native pointer so that subsequent accesses to the native pointer
   * via this control block are no longer available.
   *
   * We return the native pointer to the caller so that it may proceed with
   * cleaning up its resources.
   */
  StorageType Clear() {
    StorageType nativeImpl(nullptr);

    {  // Scope for lock
      AutoWriteLock lock(mLock);
      std::swap(mNativeImpl, nativeImpl);
    }

    return nativeImpl;
  }

  void Lock() const { mLock.ReadLock(); }

  void Unlock() const { mLock.ReadUnlock(); }

#if defined(DEBUG)
  // This is kind of expensive, so we only support it in debug builds.
  explicit operator bool() const {
    AutoReadLock lock(mLock);
    return !!mNativeImpl;
  }
#endif  // defined(DEBUG)

 private:
  friend class Accessor<NativeImpl>;
  friend class NativeWeakPtr<NativeImpl>;
  friend class NativeWeakPtrHolder<NativeImpl>;

 private:
  const mozilla::jni::Object::WeakRef mJavaOwner;
  mutable RWLock mLock;  // Protects mNativeImpl
  StorageType mNativeImpl;
};

/**
 * When a NativeWeakPtr is detached from its owning Java object, the calling
 * thread invokes the implementation's OnWeakNonIntrusiveDetach to perform
 * cleanup. We complete the remainder of the cleanup sequence on the Gecko
 * main thread by expecting OnWeakNonIntrusiveDetach implementations to invoke
 * this Runnable before exiting. It will move itself to the main thread if it
 * is not already there.
 */
template <typename NativeImpl>
class NativeWeakPtrDetachRunnable final : public Runnable {
 public:
  NativeWeakPtrDetachRunnable(
      already_AddRefed<detail::NativeWeakPtrControlBlock<NativeImpl>> aCtlBlock,
      const Object::LocalRef& aOwner,
      typename NativeWeakPtrControlBlockStorageTraits<NativeImpl>::Type
          aNativeImpl)
      : Runnable("mozilla::jni::detail::NativeWeakPtrDetachRunnable"),
        mCtlBlock(aCtlBlock),
        mOwner(aOwner),
        mNativeImpl(std::move(aNativeImpl)),
        mHasRun(false) {
    MOZ_RELEASE_ASSERT(!!mCtlBlock);
    MOZ_RELEASE_ASSERT(!!mNativeImpl);
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(NativeWeakPtrDetachRunnable, Runnable)

  NS_IMETHOD Run() override {
    mHasRun = true;

    if (!NS_IsMainThread()) {
      NS_DispatchToMainThread(this);
      return NS_OK;
    }

    // Get the owner object's native implementation
    auto owner = ToLocalRef(mOwner);
    auto attachedNativeImpl = NativePtrTraits<NativeImpl>::Get(owner);
    MOZ_RELEASE_ASSERT(!!attachedNativeImpl);

    // NativePtrTraits::ClearFinish cleans out the JNIObject's handle, which
    // obviously we don't want to attempt unless that handle still points to
    // our native implementation.
    if (attachedNativeImpl->IsSame(mCtlBlock)) {
      NativePtrTraits<NativeImpl>::ClearFinish(owner);
    }

    // Now we destroy that native object.
    mNativeImpl = nullptr;
    return NS_OK;
  }

 private:
  ~NativeWeakPtrDetachRunnable() {
    // Guard against somebody forgetting to call this runnable.
    MOZ_RELEASE_ASSERT(mHasRun, "You must run/dispatch this runnable!");
  }

 private:
  RefPtr<detail::NativeWeakPtrControlBlock<NativeImpl>> mCtlBlock;
  Object::GlobalRef mOwner;
  typename NativeWeakPtrControlBlockStorageTraits<NativeImpl>::Type mNativeImpl;
  bool mHasRun;
};

/**
 * If you want to temporarily access the object held by a NativeWeakPtr, you
 * must obtain one of these Accessor objects from the pointer. Access must
 * be done _exclusively_ using once of these objects!
 */
template <typename NativeImpl>
class MOZ_STACK_CLASS Accessor final {
 public:
  ~Accessor() {
    if (mCtlBlock) {
      mCtlBlock->Unlock();
    }
  }

  // Check whether the object is still valid before doing anything else
  explicit operator bool() const { return mCtlBlock && mCtlBlock->mNativeImpl; }

  // Normal member access
  NativeImpl* operator->() const {
    return NativeWeakPtrControlBlockStorageTraits<NativeImpl>::AsRaw(
        mCtlBlock->mNativeImpl);
  }

  // This allows us to support calling a pointer to a member function
  template <typename Member>
  auto operator->*(Member aMember) const {
    NativeImpl* impl =
        NativeWeakPtrControlBlockStorageTraits<NativeImpl>::AsRaw(
            mCtlBlock->mNativeImpl);
    return [impl, member = aMember](auto&&... aArgs) {
      return (impl->*member)(std::forward<decltype(aArgs)>(aArgs)...);
    };
  }

  // Only available for NativeImpl types that actually use refcounting.
  // The idea here is that it should be possible to obtain a strong ref from
  // a NativeWeakPtr if and only if NativeImpl supports refcounting.
  template <typename I = NativeImpl>
  auto AsRefPtr() const -> std::enable_if_t<IsRefCounted<I>::value, RefPtr<I>> {
    MOZ_ASSERT(I::HasThreadSafeRefCnt::value || NS_IsMainThread());
    return mCtlBlock->mNativeImpl;
  }

  Accessor(const Accessor&) = delete;
  Accessor(Accessor&&) = delete;
  Accessor& operator=(const Accessor&) = delete;
  Accessor& operator=(Accessor&&) = delete;

 private:
  explicit Accessor(
      const RefPtr<detail::NativeWeakPtrControlBlock<NativeImpl>>& aCtlBlock)
      : mCtlBlock(aCtlBlock) {
    if (aCtlBlock) {
      aCtlBlock->Lock();
    }
  }

 private:
  friend class NativeWeakPtr<NativeImpl>;
  friend class NativeWeakPtrHolder<NativeImpl>;

 private:
  const RefPtr<NativeWeakPtrControlBlock<NativeImpl>> mCtlBlock;
};

}  // namespace detail

/**
 * This class implements support for thread-safe weak pointers to native objects
 * that are owned by Java objects deriving from JNIObject.
 *
 * Any code that wants to access such a native object must have a copy of
 * a NativeWeakPtr to that object.
 */
template <typename NativeImpl>
class NativeWeakPtr {
 public:
  using Accessor = detail::Accessor<NativeImpl>;

  /**
   * Call this method to access the underlying object referenced by this
   * NativeWeakPtr.
   *
   * Always check the returned Accessor object for availability before calling
   * methods on it.
   *
   * For example, given:
   *
   * NativeWeakPtr<Foo> foo;
   * auto accessor = foo.Access();
   * if (accessor) {
   *   // Okay, safe to work with
   *   accessor->DoStuff();
   * } else {
   *   // The object's strong reference was cleared and is no longer available!
   * }
   */
  Accessor Access() const { return Accessor(mCtlBlock); }

  /**
   * Detach the underlying object's strong reference from its owning Java object
   * and clean it up.
   */
  void Detach() {
    if (!IsAttached()) {
      // Never attached to begin with; no-op
      return;
    }

    auto native = mCtlBlock->Clear();
    if (!native) {
      // Detach already in progress
      return;
    }

    Object::LocalRef owner(mCtlBlock->GetJavaOwner());
    MOZ_RELEASE_ASSERT(!!owner);

    // Save the raw pointer before we move native into the runnable so that we
    // may call OnWeakNonIntrusiveDetach on it even after moving native into
    // the runnable.
    NativeImpl* rawImpl =
        detail::NativeWeakPtrControlBlock<NativeImpl>::StorageTraits::AsRaw(
            native);
    rawImpl->OnWeakNonIntrusiveDetach(
        do_AddRef(new NativeWeakPtrDetachRunnable<NativeImpl>(
            mCtlBlock.forget(), owner, std::move(native))));
  }

  /**
   * This method does not indicate whether or not the weak pointer is still
   * valid; it only indicates whether we're actually attached to one.
   */
  bool IsAttached() const { return !!mCtlBlock; }

  /**
   * Does this pointer reference the same object as the one referenced by the
   * provided Accessor?
   */
  bool IsSame(const Accessor& aAccessor) const {
    return mCtlBlock == aAccessor.mCtlBlock;
  }

  /**
   * Does this pointer reference the same object as the one referenced by the
   * provided Control Block?
   */
  bool IsSame(const RefPtr<detail::NativeWeakPtrControlBlock<NativeImpl>>&
                  aOther) const {
    return mCtlBlock == aOther;
  }

  NativeWeakPtr() = default;
  MOZ_IMPLICIT NativeWeakPtr(decltype(nullptr)) {}
  NativeWeakPtr(const NativeWeakPtr& aOther) = default;
  NativeWeakPtr(NativeWeakPtr&& aOther) = default;
  NativeWeakPtr& operator=(const NativeWeakPtr& aOther) = default;
  NativeWeakPtr& operator=(NativeWeakPtr&& aOther) = default;

  NativeWeakPtr& operator=(decltype(nullptr)) {
    mCtlBlock = nullptr;
    return *this;
  }

 protected:
  // Construction of initial NativeWeakPtr for aCtlBlock
  explicit NativeWeakPtr(
      already_AddRefed<detail::NativeWeakPtrControlBlock<NativeImpl>> aCtlBlock)
      : mCtlBlock(aCtlBlock) {}

 private:
  // Construction of subsequent NativeWeakPtrs for aCtlBlock
  explicit NativeWeakPtr(
      const RefPtr<detail::NativeWeakPtrControlBlock<NativeImpl>>& aCtlBlock)
      : mCtlBlock(aCtlBlock) {}

  friend class NativeWeakPtrHolder<NativeImpl>;

 protected:
  RefPtr<detail::NativeWeakPtrControlBlock<NativeImpl>> mCtlBlock;
};

/**
 * A pointer to an instance of this class should be stored in a Java object's
 * JNIObject handle. New instances of native objects wrapped by NativeWeakPtr
 * are created using the static methods of this class.
 *
 * Why do we have distinct methods here instead of using AttachNative like other
 * pointer types that may be stored in JNIObject?
 *
 * Essentially, we want the creation and use of NativeWeakPtr to be as
 * deliberate as possible. Forcing a different creation mechanism is part of
 * that emphasis.
 *
 * Example:
 *
 * class NativeFoo {
 *  public:
 *   NativeFoo();
 *   void Bar();
 *   // The following method is required to be used with NativeWeakPtr
 *   void OnWeakNonIntrusiveDetach(already_AddRefed<Runnable> aDisposer);
 * };
 *
 * java::Object::LocalRef javaObj(...);
 *
 * // Create a new Foo that is attached to javaObj
 * auto weakFoo = NativeWeakPtrHolder<NativeFoo>::Attach(javaObj);
 *
 * // Now I can save weakFoo, access it, do whatever I want
 * if (auto accWeakFoo = weakFoo.Access()) {
 *   accWeakFoo->Bar();
 * }
 *
 * // Detach from javaObj and clean up
 * weakFoo.Detach();
 */
template <typename NativeImpl>
class MOZ_HEAP_CLASS NativeWeakPtrHolder final
    : public NativeWeakPtr<NativeImpl> {
  using Base = NativeWeakPtr<NativeImpl>;

 public:
  using Accessor = typename Base::Accessor;
  using StorageTraits =
      typename detail::NativeWeakPtrControlBlock<NativeImpl>::StorageTraits;
  using StorageType = typename StorageTraits::Type;

  /**
   * Create a new NativeImpl object, wrap it in a NativeWeakPtr, and store it
   * in the Java object's JNIObject handle.
   *
   * @return A NativeWeakPtr object that references the newly-attached object.
   */
  template <typename Cls, typename JNIType, typename... Args>
  static NativeWeakPtr<NativeImpl> Attach(const Ref<Cls, JNIType>& aJavaObject,
                                          Args&&... aArgs) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());

    StorageType nativeImpl(new NativeImpl(std::forward<Args>(aArgs)...));
    return AttachInternal(aJavaObject, std::move(nativeImpl));
  }

  /**
   * Given a new NativeImpl object, wrap it in a NativeWeakPtr, and store it
   * in the Java object's JNIObject handle.
   *
   * @return A NativeWeakPtr object that references the newly-attached object.
   */
  template <typename Cls, typename JNIType>
  static NativeWeakPtr<NativeImpl> AttachExisting(
      const Ref<Cls, JNIType>& aJavaObject,
      already_AddRefed<NativeImpl> aNativeImpl) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());

    StorageType nativeImpl(aNativeImpl);
    return AttachInternal(aJavaObject, std::move(nativeImpl));
  }

  ~NativeWeakPtrHolder() = default;

  MOZ_IMPLICIT NativeWeakPtrHolder(decltype(nullptr)) = delete;
  NativeWeakPtrHolder(const NativeWeakPtrHolder&) = delete;
  NativeWeakPtrHolder(NativeWeakPtrHolder&&) = delete;
  NativeWeakPtrHolder& operator=(const NativeWeakPtrHolder&) = delete;
  NativeWeakPtrHolder& operator=(NativeWeakPtrHolder&&) = delete;
  NativeWeakPtrHolder& operator=(decltype(nullptr)) = delete;

 private:
  template <typename Cls>
  NativeWeakPtrHolder(const LocalRef<Cls>& aJavaObject,
                      StorageType&& aNativeImpl)
      : NativeWeakPtr<NativeImpl>(
            do_AddRef(new NativeWeakPtrControlBlock<NativeImpl>(
                aJavaObject, std::move(aNativeImpl)))) {}

  /**
   * Internal function that actually wraps the native pointer, binds it to the
   * JNIObject, and then returns the NativeWeakPtr result.
   */
  template <typename Cls, typename JNIType>
  static NativeWeakPtr<NativeImpl> AttachInternal(
      const Ref<Cls, JNIType>& aJavaObject, StorageType&& aPtr) {
    auto localJavaObject = ToLocalRef(aJavaObject);
    NativeWeakPtrHolder<NativeImpl>* holder =
        new NativeWeakPtrHolder<NativeImpl>(localJavaObject, std::move(aPtr));
    static_assert(
        NativePtrPicker<NativeImpl>::value == NativePtrType::WEAK_NON_INTRUSIVE,
        "This type is not compatible with mozilla::jni::NativeWeakPtr");
    NativePtrTraits<NativeImpl>::Set(localJavaObject, holder);
    return NativeWeakPtr<NativeImpl>(holder->mCtlBlock);
  }
};

namespace detail {

/**
 * NativePtrTraits for the WEAK_NON_INTRUSIVE pointer type.
 */
template <class Impl>
struct NativePtrTraits<Impl, /* Type = */ NativePtrType::WEAK_NON_INTRUSIVE> {
  using AccessorType = typename NativeWeakPtrHolder<Impl>::Accessor;
  using HandleType = NativeWeakPtrHolder<Impl>*;
  using RefType = NativeWeakPtrHolder<Impl>* const;

  static RefType Get(JNIEnv* env, jobject instance) {
    return GetHandle(env, instance);
  }

  template <typename Cls>
  static RefType Get(const LocalRef<Cls>& instance) {
    return GetHandle(instance.Env(), instance.Get());
  }

  static AccessorType Access(RefType aPtr) { return aPtr->Access(); }

  template <typename Cls>
  static void Set(const LocalRef<Cls>& instance, HandleType ptr) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    const uintptr_t handle = reinterpret_cast<uintptr_t>(ptr);
    Clear(instance);
    SetNativeHandle(instance.Env(), instance.Get(), handle);
    MOZ_CATCH_JNI_EXCEPTION(instance.Env());
  }

  template <typename Cls>
  static void Clear(const LocalRef<Cls>& instance) {
    auto ptr = reinterpret_cast<HandleType>(
        GetNativeHandle(instance.Env(), instance.Get()));
    MOZ_CATCH_JNI_EXCEPTION(instance.Env());

    if (!ptr) {
      return;
    }

    ptr->Detach();
  }

  // This call is not safe to do unless we know for sure that instance's
  // native handle has not changed. It is up to NativeWeakPtrDetachRunnable
  // to perform this check.
  template <typename Cls>
  static void ClearFinish(const LocalRef<Cls>& instance) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    JNIEnv* const env = instance.Env();
    auto ptr =
        reinterpret_cast<HandleType>(GetNativeHandle(env, instance.Get()));
    MOZ_CATCH_JNI_EXCEPTION(env);
    MOZ_RELEASE_ASSERT(!!ptr);

    SetNativeHandle(env, instance.Get(), 0);
    MOZ_CATCH_JNI_EXCEPTION(env);
    // Deletion of ptr is done by the caller
  }

  // The call is stale if the native object has been destroyed on the
  // Gecko side, but the Java object is still attached to it through
  // a weak pointer. Stale calls should be discarded. Note that it's
  // an error if holder is nullptr here; we return false but the
  // native call will throw an error.
  template <class LocalRef>
  static bool IsStale(const LocalRef& instance) {
    JNIEnv* const env = mozilla::jni::GetEnvForThread();

    // We cannot use Get here because that method throws an exception when the
    // object is null, which is a valid state for a stale call.
    const auto holder =
        reinterpret_cast<HandleType>(GetNativeHandle(env, instance.Get()));
    MOZ_CATCH_JNI_EXCEPTION(env);

    if (!holder || !holder->IsAttached()) {
      return true;
    }

    auto acc(holder->Access());
    return !acc;
  }

 private:
  static HandleType GetHandle(JNIEnv* env, jobject instance) {
    return reinterpret_cast<HandleType>(
        CheckNativeHandle<Impl>(env, GetNativeHandle(env, instance)));
  }

  template <typename Cls>
  static HandleType GetHandle(const LocalRef<Cls>& instance) {
    return GetHandle(instance.Env(), instance.Get());
  }

  friend class NativeWeakPtrHolder<Impl>;
};

}  // namespace detail

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
template <typename T>
struct ProxyArg {
  static_assert(mozilla::IsPod<T>::value, "T must be primitive type");

  // Primitive types can be saved by value.
  typedef T Type;
  typedef typename TypeAdapter<T>::JNIType JNIType;

  static void Clear(JNIEnv* env, Type&) {}

  static Type From(JNIEnv* env, JNIType val) {
    return TypeAdapter<T>::ToNative(env, val);
  }
};

template <class C, typename T>
struct ProxyArg<Ref<C, T>> {
  // Ref types need to be saved by global ref.
  typedef typename C::GlobalRef Type;
  typedef typename TypeAdapter<Ref<C, T>>::JNIType JNIType;

  static void Clear(JNIEnv* env, Type& ref) { ref.Clear(env); }

  static Type From(JNIEnv* env, JNIType val) {
    return Type(env, C::Ref::From(val));
  }
};

template <typename C>
struct ProxyArg<const C&> : ProxyArg<C> {};
template <>
struct ProxyArg<StringParam> : ProxyArg<String::Ref> {};
template <class C>
struct ProxyArg<LocalRef<C>> : ProxyArg<typename C::Ref> {};

// ProxyNativeCall implements the functor object that is passed to OnNativeCall
template <class Impl, class Owner, bool IsStatic,
          bool HasThisArg /* has instance/class local ref in the call */,
          typename... Args>
class ProxyNativeCall {
  // "this arg" refers to the Class::LocalRef (for static methods) or
  // Owner::LocalRef (for instance methods) that we optionally (as indicated
  // by HasThisArg) pass into the destination C++ function.
  using ThisArgClass = std::conditional_t<IsStatic, Class, Owner>;
  using ThisArgJNIType = std::conditional_t<IsStatic, jclass, jobject>;

  // Type signature of the destination C++ function, which matches the
  // Method template parameter in NativeStubImpl::Wrap.
  using NativeCallType = std::conditional_t<
      IsStatic,
      std::conditional_t<HasThisArg, void (*)(const Class::LocalRef&, Args...),
                         void (*)(Args...)>,
      std::conditional_t<
          HasThisArg, void (Impl::*)(const typename Owner::LocalRef&, Args...),
          void (Impl::*)(Args...)>>;

  // Destination C++ function.
  NativeCallType mNativeCall;
  // Saved this arg.
  typename ThisArgClass::GlobalRef mThisArg;
  // Saved arguments.
  mozilla::Tuple<typename ProxyArg<Args>::Type...> mArgs;

  // We cannot use IsStatic and HasThisArg directly (without going through
  // extra hoops) because GCC complains about invalid overloads, so we use
  // another pair of template parameters, Static and ThisArg.

  template <bool Static, bool ThisArg, size_t... Indices>
  std::enable_if_t<Static && ThisArg, void> Call(
      const Class::LocalRef& cls, std::index_sequence<Indices...>) const {
    (*mNativeCall)(cls, mozilla::Get<Indices>(mArgs)...);
  }

  template <bool Static, bool ThisArg, size_t... Indices>
  std::enable_if_t<Static && !ThisArg, void> Call(
      const Class::LocalRef& cls, std::index_sequence<Indices...>) const {
    (*mNativeCall)(mozilla::Get<Indices>(mArgs)...);
  }

  template <bool Static, bool ThisArg, size_t... Indices>
  std::enable_if_t<!Static && ThisArg, void> Call(
      const typename Owner::LocalRef& inst,
      std::index_sequence<Indices...>) const {
    auto impl = NativePtrTraits<Impl>::Access(NativePtrTraits<Impl>::Get(inst));
    MOZ_CATCH_JNI_EXCEPTION(inst.Env());
    (impl->*mNativeCall)(inst, mozilla::Get<Indices>(mArgs)...);
  }

  template <bool Static, bool ThisArg, size_t... Indices>
  std::enable_if_t<!Static && !ThisArg, void> Call(
      const typename Owner::LocalRef& inst,
      std::index_sequence<Indices...>) const {
    auto impl = NativePtrTraits<Impl>::Access(NativePtrTraits<Impl>::Get(inst));
    MOZ_CATCH_JNI_EXCEPTION(inst.Env());
    (impl->*mNativeCall)(mozilla::Get<Indices>(mArgs)...);
  }

  template <size_t... Indices>
  void Clear(JNIEnv* env, std::index_sequence<Indices...>) {
    int dummy[] = {(ProxyArg<Args>::Clear(env, Get<Indices>(mArgs)), 0)...};
    mozilla::Unused << dummy;
  }

  static decltype(auto) GetNativeObject(Class::Param thisArg) {
    return nullptr;
  }

  static decltype(auto) GetNativeObject(typename Owner::Param thisArg) {
    return NativePtrTraits<Impl>::Access(
        NativePtrTraits<Impl>::Get(GetEnvForThread(), thisArg.Get()));
  }

 public:
  // The class that implements the call target.
  typedef Impl TargetClass;
  typedef typename ThisArgClass::Param ThisArgType;

  static const bool isStatic = IsStatic;

  ProxyNativeCall(ThisArgJNIType thisArg, NativeCallType nativeCall,
                  JNIEnv* env, typename ProxyArg<Args>::JNIType... args)
      : mNativeCall(nativeCall),
        mThisArg(env, ThisArgClass::Ref::From(thisArg)),
        mArgs(ProxyArg<Args>::From(env, args)...) {}

  ProxyNativeCall(ProxyNativeCall&&) = default;
  ProxyNativeCall(const ProxyNativeCall&) = default;

  // Get class ref for static calls or object ref for instance calls.
  typename ThisArgClass::Param GetThisArg() const { return mThisArg; }

  // Get the native object targeted by this call.
  // Returns nullptr for static calls.
  decltype(auto) GetNativeObject() const { return GetNativeObject(mThisArg); }

  // Return if target is the given function pointer / pointer-to-member.
  // Because we can only compare pointers of the same type, we use a
  // templated overload that is chosen only if given a different type of
  // pointer than our target pointer type.
  bool IsTarget(NativeCallType call) const { return call == mNativeCall; }
  template <typename T>
  bool IsTarget(T&&) const {
    return false;
  }

  // Redirect the call to another function / class member with the same
  // signature as the original target. Crash if given a wrong signature.
  void SetTarget(NativeCallType call) { mNativeCall = call; }
  template <typename T>
  void SetTarget(T&&) const {
    MOZ_CRASH();
  }

  void operator()() {
    JNIEnv* const env = GetEnvForThread();
    typename ThisArgClass::LocalRef thisArg(env, mThisArg);
    Call<IsStatic, HasThisArg>(thisArg, std::index_sequence_for<Args...>{});

    // Clear all saved global refs. We do this after the call is invoked,
    // and not inside the destructor because we already have a JNIEnv here,
    // so it's more efficient to clear out the saved args here. The
    // downside is that the call can only be invoked once.
    Clear(env, std::index_sequence_for<Args...>{});
    mThisArg.Clear(env);
  }
};

template <class Impl, bool HasThisArg, typename... Args>
struct Dispatcher {
  template <class Traits, bool IsStatic = Traits::isStatic,
            typename... ProxyArgs>
  static std::enable_if_t<Traits::dispatchTarget == DispatchTarget::PROXY, void>
  Run(ProxyArgs&&... args) {
    Impl::OnNativeCall(
        ProxyNativeCall<Impl, typename Traits::Owner, IsStatic, HasThisArg,
                        Args...>(std::forward<ProxyArgs>(args)...));
  }

  template <class Traits, bool IsStatic = Traits::isStatic, typename ThisArg,
            typename... ProxyArgs>
  static std::enable_if_t<
      Traits::dispatchTarget == DispatchTarget::GECKO_PRIORITY, void>
  Run(ThisArg thisArg, ProxyArgs&&... args) {
    // For a static method, do not forward the "this arg" (i.e. the class
    // local ref) if the implementation does not request it. This saves us
    // a pair of calls to add/delete global ref.
    auto proxy =
        ProxyNativeCall<Impl, typename Traits::Owner, IsStatic, HasThisArg,
                        Args...>((HasThisArg || !IsStatic) ? thisArg : nullptr,
                                 std::forward<ProxyArgs>(args)...);
    DispatchToGeckoPriorityQueue(
        NS_NewRunnableFunction("PriorityNativeCall", std::move(proxy)));
  }

  template <class Traits, bool IsStatic = Traits::isStatic, typename ThisArg,
            typename... ProxyArgs>
  static std::enable_if_t<Traits::dispatchTarget == DispatchTarget::GECKO, void>
  Run(ThisArg thisArg, ProxyArgs&&... args) {
    // For a static method, do not forward the "this arg" (i.e. the class
    // local ref) if the implementation does not request it. This saves us
    // a pair of calls to add/delete global ref.
    auto proxy =
        ProxyNativeCall<Impl, typename Traits::Owner, IsStatic, HasThisArg,
                        Args...>((HasThisArg || !IsStatic) ? thisArg : nullptr,
                                 std::forward<ProxyArgs>(args)...);
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("GeckoNativeCall", std::move(proxy)));
  }

  template <class Traits, bool IsStatic = false, typename... ProxyArgs>
  static std::enable_if_t<Traits::dispatchTarget == DispatchTarget::CURRENT,
                          void>
  Run(ProxyArgs&&... args) {
    MOZ_CRASH("Unreachable code");
  }
};

}  // namespace detail

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
#  define MOZ_JNICALL JNICALL __attribute__((force_align_arg_pointer))
#else
#  define MOZ_JNICALL JNICALL
#endif

template <class Traits, class Impl, class Args = typename Traits::Args>
class NativeStub;

template <class Traits, class Impl, typename... Args>
class NativeStub<Traits, Impl, jni::Args<Args...>> {
  using Owner = typename Traits::Owner;
  using ReturnType = typename Traits::ReturnType;

  static constexpr bool isStatic = Traits::isStatic;
  static constexpr bool isVoid = std::is_void_v<ReturnType>;

  struct VoidType {
    using JNIType = void;
  };
  using ReturnJNIType =
      typename std::conditional_t<isVoid, VoidType,
                                  TypeAdapter<ReturnType>>::JNIType;

  using ReturnTypeForNonVoidInstance =
      std::conditional_t<!isStatic && !isVoid, ReturnType, VoidType>;
  using ReturnTypeForVoidInstance =
      std::conditional_t<!isStatic && isVoid, ReturnType, VoidType&>;
  using ReturnTypeForNonVoidStatic =
      std::conditional_t<isStatic && !isVoid, ReturnType, VoidType>;
  using ReturnTypeForVoidStatic =
      std::conditional_t<isStatic && isVoid, ReturnType, VoidType&>;

  static_assert(Traits::dispatchTarget == DispatchTarget::CURRENT || isVoid,
                "Dispatched calls must have void return type");

 public:
  // Non-void instance method
  template <ReturnTypeForNonVoidInstance (Impl::*Method)(Args...)>
  static MOZ_JNICALL ReturnJNIType
  Wrap(JNIEnv* env, jobject instance,
       typename TypeAdapter<Args>::JNIType... args) {
    MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

    auto impl = NativePtrTraits<Impl>::Access(
        NativePtrTraits<Impl>::Get(env, instance));
    if (!impl) {
      // There is a pending JNI exception at this point.
      return ReturnJNIType();
    }
    return TypeAdapter<ReturnType>::FromNative(
        env, (impl->*Method)(TypeAdapter<Args>::ToNative(env, args)...));
  }

  // Non-void instance method with instance reference
  template <ReturnTypeForNonVoidInstance (Impl::*Method)(
      const typename Owner::LocalRef&, Args...)>
  static MOZ_JNICALL ReturnJNIType
  Wrap(JNIEnv* env, jobject instance,
       typename TypeAdapter<Args>::JNIType... args) {
    MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

    auto impl = NativePtrTraits<Impl>::Access(
        NativePtrTraits<Impl>::Get(env, instance));
    if (!impl) {
      // There is a pending JNI exception at this point.
      return ReturnJNIType();
    }
    auto self = Owner::LocalRef::Adopt(env, instance);
    const auto res = TypeAdapter<ReturnType>::FromNative(
        env, (impl->*Method)(self, TypeAdapter<Args>::ToNative(env, args)...));
    self.Forget();
    return res;
  }

  // Void instance method
  template <ReturnTypeForVoidInstance (Impl::*Method)(Args...)>
  static MOZ_JNICALL void Wrap(JNIEnv* env, jobject instance,
                               typename TypeAdapter<Args>::JNIType... args) {
    MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

    if (Traits::dispatchTarget != DispatchTarget::CURRENT) {
      Dispatcher<Impl, /* HasThisArg */ false, Args...>::template Run<Traits>(
          instance, Method, env, args...);
      return;
    }

    auto impl = NativePtrTraits<Impl>::Access(
        NativePtrTraits<Impl>::Get(env, instance));
    if (!impl) {
      // There is a pending JNI exception at this point.
      return;
    }
    (impl->*Method)(TypeAdapter<Args>::ToNative(env, args)...);
  }

  // Void instance method with instance reference
  template <ReturnTypeForVoidInstance (Impl::*Method)(
      const typename Owner::LocalRef&, Args...)>
  static MOZ_JNICALL void Wrap(JNIEnv* env, jobject instance,
                               typename TypeAdapter<Args>::JNIType... args) {
    MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

    if (Traits::dispatchTarget != DispatchTarget::CURRENT) {
      Dispatcher<Impl, /* HasThisArg */ true, Args...>::template Run<Traits>(
          instance, Method, env, args...);
      return;
    }

    auto impl = NativePtrTraits<Impl>::Access(
        NativePtrTraits<Impl>::Get(env, instance));
    if (!impl) {
      // There is a pending JNI exception at this point.
      return;
    }
    auto self = Owner::LocalRef::Adopt(env, instance);
    (impl->*Method)(self, TypeAdapter<Args>::ToNative(env, args)...);
    self.Forget();
  }

  // Overload for DisposeNative
  template <ReturnTypeForVoidInstance (*DisposeNative)(
      const typename Owner::LocalRef&)>
  static MOZ_JNICALL void Wrap(JNIEnv* env, jobject instance) {
    MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

    if (Traits::dispatchTarget != DispatchTarget::CURRENT) {
      using LocalRef = typename Owner::LocalRef;
      Dispatcher<Impl, /* HasThisArg */ false, const LocalRef&>::template Run<
          Traits, /* IsStatic */ true>(
          /* ThisArg */ nullptr, DisposeNative, env, instance);
      return;
    }

    auto self = Owner::LocalRef::Adopt(env, instance);
    DisposeNative(self);
    self.Forget();
  }

  // Non-void static method
  template <ReturnTypeForNonVoidStatic (*Method)(Args...)>
  static MOZ_JNICALL ReturnJNIType
  Wrap(JNIEnv* env, jclass, typename TypeAdapter<Args>::JNIType... args) {
    MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

    return TypeAdapter<ReturnType>::FromNative(
        env, (*Method)(TypeAdapter<Args>::ToNative(env, args)...));
  }

  // Non-void static method with class reference
  template <ReturnTypeForNonVoidStatic (*Method)(const Class::LocalRef&,
                                                 Args...)>
  static MOZ_JNICALL ReturnJNIType
  Wrap(JNIEnv* env, jclass cls, typename TypeAdapter<Args>::JNIType... args) {
    MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

    auto clazz = Class::LocalRef::Adopt(env, cls);
    const auto res = TypeAdapter<ReturnType>::FromNative(
        env, (*Method)(clazz, TypeAdapter<Args>::ToNative(env, args)...));
    clazz.Forget();
    return res;
  }

  // Void static method
  template <ReturnTypeForVoidStatic (*Method)(Args...)>
  static MOZ_JNICALL void Wrap(JNIEnv* env, jclass cls,
                               typename TypeAdapter<Args>::JNIType... args) {
    MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

    if (Traits::dispatchTarget != DispatchTarget::CURRENT) {
      Dispatcher<Impl, /* HasThisArg */ false, Args...>::template Run<Traits>(
          cls, Method, env, args...);
      return;
    }

    (*Method)(TypeAdapter<Args>::ToNative(env, args)...);
  }

  // Void static method with class reference
  template <ReturnTypeForVoidStatic (*Method)(const Class::LocalRef&, Args...)>
  static MOZ_JNICALL void Wrap(JNIEnv* env, jclass cls,
                               typename TypeAdapter<Args>::JNIType... args) {
    MOZ_ASSERT_JNI_THREAD(Traits::callingThread);

    if (Traits::dispatchTarget != DispatchTarget::CURRENT) {
      Dispatcher<Impl, /* HasThisArg */ true, Args...>::template Run<Traits>(
          cls, Method, env, args...);
      return;
    }

    auto clazz = Class::LocalRef::Adopt(env, cls);
    (*Method)(clazz, TypeAdapter<Args>::ToNative(env, args)...);
    clazz.Forget();
  }
};

// Generate a JNINativeMethod from a native
// method's traits class and a wrapped stub.
template <class Traits, typename Ret, typename... Args>
constexpr JNINativeMethod MakeNativeMethod(MOZ_JNICALL Ret (*stub)(JNIEnv*,
                                                                   Args...)) {
  return {Traits::name, Traits::signature, reinterpret_cast<void*>(stub)};
}

// Class inherited by implementing class.
template <class Cls, class Impl>
class NativeImpl {
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
                           SupportsWeakPtr* ptr) {
    static_assert(NativePtrPicker<Impl>::value == NativePtrType::WEAK_INTRUSIVE,
                  "Use another AttachNative for non-WeakPtr usage");
    return NativePtrTraits<Impl>::Set(instance, static_cast<Impl*>(ptr));
  }

  static void AttachNative(const typename Cls::LocalRef& instance,
                           UniquePtr<Impl>&& ptr) {
    static_assert(NativePtrPicker<Impl>::value == NativePtrType::OWNING,
                  "Use another AttachNative for WeakPtr or RefPtr usage");
    return NativePtrTraits<Impl>::Set(instance, std::move(ptr));
  }

  static void AttachNative(const typename Cls::LocalRef& instance, Impl* ptr) {
    static_assert(NativePtrPicker<Impl>::value == NativePtrType::REFPTR,
                  "Use another AttachNative for non-RefPtr usage");
    return NativePtrTraits<Impl>::Set(instance, ptr);
  }

  // Get the C++ instance associated with a Java instance.
  // There is always a pending exception if the return value is nullptr.
  static decltype(auto) GetNative(const typename Cls::LocalRef& instance) {
    return NativePtrTraits<Impl>::Get(instance);
  }

  static void DisposeNative(const typename Cls::LocalRef& instance) {
    NativePtrTraits<Impl>::Clear(instance);
  }

  NativeImpl() {
    // Initialize on creation if not already initialized.
    Init();
  }
};

// Define static member.
template <class C, class I>
bool NativeImpl<C, I>::sInited;

}  // namespace jni
}  // namespace mozilla

#endif  // mozilla_jni_Natives_h__
