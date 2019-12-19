/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A class for non-null strong pointers to reference-counted objects. */

#ifndef mozilla_OwningNonNull_h
#define mozilla_OwningNonNull_h

#include "nsAutoPtr.h"
#include "nsCycleCollectionNoteChild.h"

namespace mozilla {

template <class T>
class MOZ_IS_SMARTPTR_TO_REFCOUNTED OwningNonNull {
 public:
  OwningNonNull() {}

  MOZ_IMPLICIT OwningNonNull(T& aValue) { init(&aValue); }

  template <class U>
  MOZ_IMPLICIT OwningNonNull(already_AddRefed<U>&& aValue) {
    init(aValue);
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
  OwningNonNull<T>& operator=(const OwningNonNull<U>& aValue) {
    init(aValue);
    return *this;
  }

  // Don't allow assigning nullptr, it makes no sense
  void operator=(decltype(nullptr)) = delete;

  already_AddRefed<T> forget() {
#ifdef DEBUG
    mInited = false;
#endif
    return mPtr.forget();
  }

  template <class U>
  void forget(U** aOther) {
#ifdef DEBUG
    mInited = false;
#endif
    mPtr.forget(aOther);
  }

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

 protected:
  template <typename U>
  void init(U&& aValue) {
    mPtr = aValue;
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
  RefPtr<T> releaser(aField.forget());
  // Now just let releaser go out of scope.
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
