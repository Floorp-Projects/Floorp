/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Weak pointer functionality, implemented as a mixin for use with any class. */

/**
 * SupportsWeakPtr lets you have a pointer to an object 'Foo' without affecting
 * its lifetime. It works by creating a single shared reference counted object
 * (WeakReference) that each WeakPtr will access 'Foo' through. This lets 'Foo'
 * clear the pointer in the WeakReference without having to know about all of
 * the WeakPtrs to it and allows the WeakReference to live beyond the lifetime
 * of 'Foo'.
 *
 * PLEASE NOTE: This weak pointer implementation is not thread-safe.
 *
 * Note that when deriving from SupportsWeakPtr you should add
 * MOZ_DECLARE_WEAKREFERENCE_TYPENAME(ClassName) to the public section of your
 * class, where ClassName is the name of your class.
 *
 * The overhead of WeakPtr is that accesses to 'Foo' becomes an additional
 * dereference, and an additional heap allocated pointer sized object shared
 * between all of the WeakPtrs.
 *
 * Example of usage:
 *
 *   // To have a class C support weak pointers, inherit from
 *   // SupportsWeakPtr<C>.
 *   class C : public SupportsWeakPtr<C>
 *   {
 *   public:
 *     MOZ_DECLARE_WEAKREFERENCE_TYPENAME(C)
 *     int mNum;
 *     void act();
 *   };
 *
 *   C* ptr = new C();
 *
 *   // Get weak pointers to ptr. The first time a weak pointer
 *   // is obtained, a reference counted WeakReference object is created that
 *   // can live beyond the lifetime of 'ptr'. The WeakReference
 *   // object will be notified of 'ptr's destruction.
 *   WeakPtr<C> weak = ptr;
 *   WeakPtr<C> other = ptr;
 *
 *   // Test a weak pointer for validity before using it.
 *   if (weak) {
 *     weak->mNum = 17;
 *     weak->act();
 *   }
 *
 *   // Destroying the underlying object clears weak pointers to it.
 *   delete ptr;
 *
 *   MOZ_ASSERT(!weak, "Deleting |ptr| clears weak pointers to it.");
 *   MOZ_ASSERT(!other, "Deleting |ptr| clears all weak pointers to it.");
 *
 * WeakPtr is typesafe and may be used with any class. It is not required that
 * the class be reference-counted or allocated in any particular way.
 *
 * The API was loosely inspired by Chromium's weak_ptr.h:
 * http://src.chromium.org/svn/trunk/src/base/memory/weak_ptr.h
 */

#ifndef mozilla_WeakPtr_h
#define mozilla_WeakPtr_h

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"

#include <string.h>
#include <type_traits>

#if defined(MOZILLA_INTERNAL_API)
// For thread safety checking.
#  include "nsISupportsImpl.h"
#endif

#if defined(MOZILLA_INTERNAL_API) && \
    defined(MOZ_THREAD_SAFETY_OWNERSHIP_CHECKS_SUPPORTED)

// Weak referencing is not implemeted as thread safe.  When a WeakPtr
// is created or dereferenced on thread A but the real object is just
// being Released() on thread B, there is a possibility of a race
// when the proxy object (detail::WeakReference) is notified about
// the real object destruction just between when thread A is storing
// the object pointer locally and is about to add a reference to it.
//
// Hence, a non-null weak proxy object is considered to have a single
// "owning thread".  It means that each query for a weak reference,
// its dereference, and destruction of the real object must all happen
// on a single thread.  The following macros implement assertions for
// checking these conditions.
//
// We re-use XPCOM's nsAutoOwningThread checks when they are available. This has
// the advantage that it works with cooperative thread pools.

#  define MOZ_WEAKPTR_DECLARE_THREAD_SAFETY_CHECK \
    /* Will be none if mPtr = nullptr. */         \
    Maybe<nsAutoOwningThread> _owningThread;
#  define MOZ_WEAKPTR_INIT_THREAD_SAFETY_CHECK() \
    do {                                         \
      if (p) {                                   \
        _owningThread.emplace();                 \
      }                                          \
    } while (false)
#  define MOZ_WEAKPTR_ASSERT_THREAD_SAFETY()                                  \
    do {                                                                      \
      if (_owningThread.isSome() && !_owningThread.ref().IsCurrentThread()) { \
        WeakPtrTraits<T>::AssertSafeToAccessFromNonOwningThread();            \
      }                                                                       \
    } while (false)
#  define MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED(that) \
    (that)->AssertThreadSafety();
#  define MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED_IF(that) \
    do {                                                      \
      if (that) {                                             \
        (that)->AssertThreadSafety();                         \
      }                                                       \
    } while (false)

#  define MOZ_WEAKPTR_THREAD_SAFETY_CHECKING 1

#else

#  define MOZ_WEAKPTR_DECLARE_THREAD_SAFETY_CHECK
#  define MOZ_WEAKPTR_INIT_THREAD_SAFETY_CHECK() \
    do {                                         \
    } while (false)
#  define MOZ_WEAKPTR_ASSERT_THREAD_SAFETY() \
    do {                                     \
    } while (false)
#  define MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED(that) \
    do {                                                   \
    } while (false)
#  define MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED_IF(that) \
    do {                                                      \
    } while (false)

#endif

namespace mozilla {

template <typename T>
class WeakPtr;
template <typename T>
class SupportsWeakPtr;

#ifdef MOZ_REFCOUNTED_LEAK_CHECKING
#  define MOZ_DECLARE_WEAKREFERENCE_TYPENAME(T)  \
    static const char* weakReferenceTypeName() { \
      return "WeakReference<" #T ">";            \
    }
#else
#  define MOZ_DECLARE_WEAKREFERENCE_TYPENAME(T)
#endif

template <class T>
struct WeakPtrTraits {
  static void AssertSafeToAccessFromNonOwningThread() {
    MOZ_DIAGNOSTIC_ASSERT(false, "WeakPtr accessed from multiple threads");
  }
};

namespace detail {

// This can live beyond the lifetime of the class derived from
// SupportsWeakPtr.
template <class T>
class WeakReference : public ::mozilla::RefCounted<WeakReference<T>> {
 public:
  explicit WeakReference(T* p) : mPtr(p) {
    MOZ_WEAKPTR_INIT_THREAD_SAFETY_CHECK();
  }

  T* get() const {
    MOZ_WEAKPTR_ASSERT_THREAD_SAFETY();
    return mPtr;
  }

#ifdef MOZ_REFCOUNTED_LEAK_CHECKING
  const char* typeName() const {
    // The first time this is called mPtr is null, so don't
    // invoke any methods on mPtr.
    return T::weakReferenceTypeName();
  }
  size_t typeSize() const { return sizeof(*this); }
#endif

#ifdef MOZ_WEAKPTR_THREAD_SAFETY_CHECKING
  void AssertThreadSafety() { MOZ_WEAKPTR_ASSERT_THREAD_SAFETY(); }
#endif

 private:
  friend class mozilla::SupportsWeakPtr<T>;

  void detach() {
    MOZ_WEAKPTR_ASSERT_THREAD_SAFETY();
    mPtr = nullptr;
  }

  T* MOZ_NON_OWNING_REF mPtr;
  MOZ_WEAKPTR_DECLARE_THREAD_SAFETY_CHECK
};

}  // namespace detail

template <typename T>
class SupportsWeakPtr {
 protected:
  ~SupportsWeakPtr() {
    static_assert(std::is_base_of<SupportsWeakPtr<T>, T>::value,
                  "T must derive from SupportsWeakPtr<T>");
    DetachWeakPtr();
  }

 protected:
  void DetachWeakPtr() {
    if (mSelfReferencingWeakPtr) {
      mSelfReferencingWeakPtr.mRef->detach();
    }
  }

 private:
  const WeakPtr<T>& SelfReferencingWeakPtr() {
    if (!mSelfReferencingWeakPtr) {
      mSelfReferencingWeakPtr.mRef =
          new detail::WeakReference<T>(static_cast<T*>(this));
    } else {
      MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED(mSelfReferencingWeakPtr.mRef);
    }
    return mSelfReferencingWeakPtr;
  }

  const WeakPtr<const T>& SelfReferencingWeakPtr() const {
    const WeakPtr<T>& p =
        const_cast<SupportsWeakPtr*>(this)->SelfReferencingWeakPtr();
    return reinterpret_cast<const WeakPtr<const T>&>(p);
  }

  friend class WeakPtr<T>;
  friend class WeakPtr<const T>;

  WeakPtr<T> mSelfReferencingWeakPtr;
};

template <typename T>
class WeakPtr {
  typedef detail::WeakReference<T> WeakReference;
  using NonConstT = std::remove_const_t<T>;

 public:
  WeakPtr& operator=(const WeakPtr& aOther) {
    // We must make sure the reference we have now is safe to be dereferenced
    // before we throw it away... (this can be called from a ctor)
    MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED_IF(mRef);
    // ...and make sure the new reference is used on a single thread as well.
    MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED(aOther.mRef);

    mRef = aOther.mRef;
    return *this;
  }

  WeakPtr(const WeakPtr& aOther) {
    // The thread safety check is performed inside of the operator= method.
    *this = aOther;
  }

  WeakPtr& operator=(decltype(nullptr)) {
    // We must make sure the reference we have now is safe to be dereferenced
    // before we throw it away.
    MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED_IF(mRef);
    if (!mRef || mRef->get()) {
      // Ensure that mRef is dereferenceable in the uninitialized state.
      mRef = new WeakReference(nullptr);
    }
    return *this;
  }

  WeakPtr& operator=(SupportsWeakPtr<NonConstT> const* aOther) {
    // We must make sure the reference we have now is safe to be dereferenced
    // before we throw it away.
    MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED_IF(mRef);
    if (aOther) {
      *this = aOther->SelfReferencingWeakPtr();
    } else if (!mRef || mRef->get()) {
      // Ensure that mRef is dereferenceable in the uninitialized state.
      mRef = new WeakReference(nullptr);
    }
    // The thread safety check happens inside SelfReferencingWeakPtr
    // or is initialized in the WeakReference constructor.
    return *this;
  }

  WeakPtr& operator=(SupportsWeakPtr<NonConstT>* aOther) {
    // We must make sure the reference we have now is safe to be dereferenced
    // before we throw it away.
    MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED_IF(mRef);
    if (aOther) {
      *this = aOther->SelfReferencingWeakPtr();
    } else if (!mRef || mRef->get()) {
      // Ensure that mRef is dereferenceable in the uninitialized state.
      mRef = new WeakReference(nullptr);
    }
    // The thread safety check happens inside SelfReferencingWeakPtr
    // or is initialized in the WeakReference constructor.
    return *this;
  }

  MOZ_IMPLICIT WeakPtr(T* aOther) { *this = aOther; }

  // Ensure that mRef is dereferenceable in the uninitialized state.
  WeakPtr() : mRef(new WeakReference(nullptr)) {}

  operator T*() const { return mRef->get(); }
  T& operator*() const { return *mRef->get(); }

  T* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN { return mRef->get(); }

  T* get() const { return mRef->get(); }

  ~WeakPtr() { MOZ_WEAKPTR_ASSERT_THREAD_SAFETY_DELEGATED(mRef); }

 private:
  friend class SupportsWeakPtr<T>;

  explicit WeakPtr(const RefPtr<WeakReference>& aOther) : mRef(aOther) {}

  RefPtr<WeakReference> mRef;
};

#define NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR tmp->DetachWeakPtr();

#define NS_IMPL_CYCLE_COLLECTION_WEAK_PTR(class_, ...) \
  NS_IMPL_CYCLE_COLLECTION_CLASS(class_)               \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(class_)        \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)       \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR           \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                  \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(class_)      \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)     \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

#define NS_IMPL_CYCLE_COLLECTION_WEAK_PTR_INHERITED(class_, super_, ...) \
  NS_IMPL_CYCLE_COLLECTION_CLASS(class_)                                 \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(class_, super_)        \
    NS_IMPL_CYCLE_COLLECTION_UNLINK(__VA_ARGS__)                         \
    NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR                             \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_END                                    \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(class_, super_)      \
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(__VA_ARGS__)                       \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

}  // namespace mozilla

#endif /* mozilla_WeakPtr_h */
