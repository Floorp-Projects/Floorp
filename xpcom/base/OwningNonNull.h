/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A class for non-null strong pointers to reference-counted objects. */

#ifndef mozilla_OwningNonNull_h
#define mozilla_OwningNonNull_h

#include "nsCOMPtr.h"
#include "nsCycleCollectionNoteChild.h"

namespace mozilla {

// OwningNonNull<T> is similar to a RefPtr<T>, which is not null after initial
// initialization. It has a restricted interface compared to RefPtr, with some
// additional operations defined. The main use is in DOM bindings. Use it
// outside DOM bindings only if you can ensure it never escapes without being
// properly initialized, and you don't need to move it. Otherwise, use a
// RefPtr<T> instead.
//
// Compared to a plain RefPtr<T>, in particular
// - it is copyable but not movable
// - it can be constructed and assigned from T& and is convertible to T&
// implicitly
// - it cannot be cleared by the user once initialized, though it can be
//   re-assigned a new (non-null) value
// - it is not convertible to bool, but there is an explicit isInitialized
//   member function
//
// Beware that there are two cases where an OwningNonNull<T> actually is nullptr
// - it was default-constructed and not yet initialized
// - it was cleared during CC unlinking.
// All attempts to use it in an invalid state will trigger an assertion in debug
// builds.
//
// The original intent of OwningNonNull<T> was to implement a class with the
// same auto-conversion and annotation semantics as mozilla::dom::NonNull<T>
// (i.e. never null once you have properly initialized it, auto-converts to T&),
// but that holds a strong reference to the object involved. This was designed
// for use in DOM bindings and in particular for storing what WebIDL represents
// as InterfaceName (as opposed to `InterfaceName?`) in various containers
// (dictionaries, sequences). DOM bindings never allow a default-constructed
// uninitialized OwningNonNull to escape. RefPtr could have been used for this
// use case, just like we could have used T* instead of NonNull<T>, but it
// seemed desirable to explicitly annotate the non-null nature of the things
// involved to eliminate pointless null-checks, which otherwise tend to
// proliferate.
template <class T>
class MOZ_IS_SMARTPTR_TO_REFCOUNTED OwningNonNull {
 public:
  using element_type = T;

  OwningNonNull() = default;

  MOZ_IMPLICIT OwningNonNull(T& aValue) { init(&aValue); }

  template <class U>
  MOZ_IMPLICIT OwningNonNull(already_AddRefed<U>&& aValue) {
    init(aValue);
  }

  template <class U>
  MOZ_IMPLICIT OwningNonNull(RefPtr<U>&& aValue) {
    init(std::move(aValue));
  }

  template <class U>
  MOZ_IMPLICIT OwningNonNull(const OwningNonNull<U>& aValue) {
    init(aValue);
  }

  // This is no worse than get() in terms of const handling.
  operator T&() const { return ref(); }

  operator T*() const { return get(); }

  // Conversion to bool is always true, so delete to catch errors
  explicit operator bool() const = delete;

  T* operator->() const { return get(); }

  T& operator*() const { return ref(); }

  OwningNonNull<T>& operator=(T* aValue) {
    init(aValue);
    return *this;
  }

  OwningNonNull<T>& operator=(T& aValue) {
    init(&aValue);
    return *this;
  }

  template <class U>
  OwningNonNull<T>& operator=(already_AddRefed<U>&& aValue) {
    init(aValue);
    return *this;
  }

  template <class U>
  OwningNonNull<T>& operator=(RefPtr<U>&& aValue) {
    init(std::move(aValue));
    return *this;
  }

  template <class U>
  OwningNonNull<T>& operator=(const OwningNonNull<U>& aValue) {
    init(aValue);
    return *this;
  }

  // Don't allow assigning nullptr, it makes no sense
  void operator=(decltype(nullptr)) = delete;

  T& ref() const {
    MOZ_ASSERT(mInited);
    MOZ_ASSERT(mPtr, "OwningNonNull<T> was set to null");
    return *mPtr;
  }

  // Make us work with smart pointer helpers that expect a get().
  T* get() const {
    MOZ_ASSERT(mInited);
    MOZ_ASSERT(mPtr, "OwningNonNull<T> was set to null");
    return mPtr;
  }

  template <typename U>
  void swap(U& aOther) {
    mPtr.swap(aOther);
#ifdef DEBUG
    mInited = mPtr;
#endif
  }

  // We have some consumers who want to check whether we're inited in non-debug
  // builds as well.  Luckily, we have the invariant that we're inited precisely
  // when mPtr is non-null.
  bool isInitialized() const {
    MOZ_ASSERT(!!mPtr == mInited, "mInited out of sync with mPtr?");
    return mPtr;
  }

 private:
  void unlinkForCC() {
#ifdef DEBUG
    mInited = false;
#endif
    mPtr = nullptr;
  }

  // Allow ImplCycleCollectionUnlink to call unlinkForCC().
  template <typename U>
  friend void ImplCycleCollectionUnlink(OwningNonNull<U>& aField);

 protected:
  template <typename U>
  void init(U&& aValue) {
    mPtr = std::move(aValue);
    MOZ_ASSERT(mPtr);
#ifdef DEBUG
    mInited = true;
#endif
  }

  RefPtr<T> mPtr;
#ifdef DEBUG
  bool mInited = false;
#endif
};

template <typename T>
inline void ImplCycleCollectionUnlink(OwningNonNull<T>& aField) {
  aField.unlinkForCC();
}

template <typename T>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, OwningNonNull<T>& aField,
    const char* aName, uint32_t aFlags = 0) {
  CycleCollectionNoteChild(aCallback, aField.get(), aName, aFlags);
}

}  // namespace mozilla

// Declared in nsCOMPtr.h
template <class T>
template <class U>
nsCOMPtr<T>::nsCOMPtr(const mozilla::OwningNonNull<U>& aOther)
    : nsCOMPtr(aOther.get()) {}

template <class T>
template <class U>
nsCOMPtr<T>& nsCOMPtr<T>::operator=(const mozilla::OwningNonNull<U>& aOther) {
  return operator=(aOther.get());
}

// Declared in mozilla/RefPtr.h
template <class T>
template <class U>
RefPtr<T>::RefPtr(const mozilla::OwningNonNull<U>& aOther)
    : RefPtr(aOther.get()) {}

template <class T>
template <class U>
RefPtr<T>& RefPtr<T>::operator=(const mozilla::OwningNonNull<U>& aOther) {
  return operator=(aOther.get());
}

#endif  // mozilla_OwningNonNull_h
