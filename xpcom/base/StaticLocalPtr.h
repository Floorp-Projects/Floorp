/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticLocalPtr_h
#define mozilla_StaticLocalPtr_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

/**
 * StaticLocalAutoPtr and StaticLocalRefPtr are like UniquePtr and RefPtr,
 * except they are suitable for use as "magic static" local variables -- that
 * is, they are able to take advantage of C++11's guarantee of thread safety
 * during initialization by atomically constructing both the smart pointer
 * itself as well as the object being pointed to.
 *
 * A static local instance of StaticLocal{Auto,Ref}Ptr does not cause the
 * compiler to emit any atexit calls. In order to accomplish this,
 * StaticLocal{Auto,Ref}Ptr must have a trivial destructor. As a consequence,
 * it does not delete/release its raw pointer upon destruction.
 *
 * The clang plugin, run as part of our "static analysis" builds, makes it a
 * compile-time error to use StaticLocal{Auto,Ref}Ptr as anything except a
 * static local variable.
 *
 * StaticLocal{Auto,Ref}Ptr have a limited interface as compared to
 * ns{Auto,Ref}Ptr; this is intentional, since their range of acceptable uses is
 * smaller.
 */

template <typename T>
class MOZ_STATIC_LOCAL_CLASS StaticLocalAutoPtr final {
 public:
  explicit StaticLocalAutoPtr(T* aRawPtr) : mRawPtr(aRawPtr) {}

  StaticLocalAutoPtr(StaticLocalAutoPtr<T>&& aOther) : mRawPtr(aOther.mRawPtr) {
    aOther.mRawPtr = nullptr;
  }

  StaticLocalAutoPtr<T>& operator=(T* aRhs) {
    Assign(aRhs);
    return *this;
  }

  T* get() const { return mRawPtr; }

  operator T*() const { return get(); }

  T* operator->() const {
    MOZ_ASSERT(mRawPtr);
    return get();
  }

  T& operator*() const { return *get(); }

  T* forget() {
    T* temp = mRawPtr;
    mRawPtr = nullptr;
    return temp;
  }

 private:
  StaticLocalAutoPtr(const StaticLocalAutoPtr<T>& aOther) = delete;

  // We do not allow assignment as the intention of this class is to only
  // assign to mRawPtr during construction.
  StaticLocalAutoPtr& operator=(const StaticLocalAutoPtr<T>& aOther) = delete;
  StaticLocalAutoPtr& operator=(StaticLocalAutoPtr<T>&&) = delete;

  void Assign(T* aNewPtr) {
    MOZ_ASSERT(!aNewPtr || mRawPtr != aNewPtr);
    T* oldPtr = mRawPtr;
    mRawPtr = aNewPtr;
    delete oldPtr;
  }

  T* mRawPtr;
};

template <typename T>
class MOZ_STATIC_LOCAL_CLASS StaticLocalRefPtr final {
 public:
  explicit StaticLocalRefPtr(T* aRawPtr) : mRawPtr(nullptr) {
    AssignWithAddref(aRawPtr);
  }

  explicit StaticLocalRefPtr(already_AddRefed<T>& aPtr) : mRawPtr(nullptr) {
    AssignAssumingAddRef(aPtr.take());
  }

  explicit StaticLocalRefPtr(already_AddRefed<T>&& aPtr) : mRawPtr(nullptr) {
    AssignAssumingAddRef(aPtr.take());
  }

  StaticLocalRefPtr(const StaticLocalRefPtr<T>& aPtr)
      : StaticLocalRefPtr(aPtr.mRawPtr) {}

  StaticLocalRefPtr(StaticLocalRefPtr<T>&& aPtr) : mRawPtr(aPtr.mRawPtr) {
    aPtr.mRawPtr = nullptr;
  }

  StaticLocalRefPtr<T>& operator=(T* aRhs) {
    AssignWithAddref(aRhs);
    return *this;
  }

  already_AddRefed<T> forget() {
    T* temp = mRawPtr;
    mRawPtr = nullptr;
    return already_AddRefed<T>(temp);
  }

  T* get() const { return mRawPtr; }

  operator T*() const { return get(); }

  T* operator->() const {
    MOZ_ASSERT(mRawPtr);
    return get();
  }

  T& operator*() const { return *get(); }

 private:
  // We do not allow assignment as the intention of this class is to only
  // assign to mRawPtr during construction.
  StaticLocalRefPtr<T>& operator=(const StaticLocalRefPtr<T>& aRhs) = delete;
  StaticLocalRefPtr<T>& operator=(StaticLocalRefPtr<T>&& aRhs) = delete;

  void AssignWithAddref(T* aNewPtr) {
    if (aNewPtr) {
      aNewPtr->AddRef();
    }
    AssignAssumingAddRef(aNewPtr);
  }

  void AssignAssumingAddRef(T* aNewPtr) {
    T* oldPtr = mRawPtr;
    mRawPtr = aNewPtr;
    if (oldPtr) {
      oldPtr->Release();
    }
  }

  T* MOZ_OWNING_REF mRawPtr;
};

namespace StaticLocalPtr_internal {
class Zero;
}  // namespace StaticLocalPtr_internal

#define REFLEXIVE_EQUALITY_OPERATORS(type1, type2, eq_fn, ...) \
  template <__VA_ARGS__>                                       \
  inline bool operator==(type1 lhs, type2 rhs) {               \
    return eq_fn;                                              \
  }                                                            \
                                                               \
  template <__VA_ARGS__>                                       \
  inline bool operator==(type2 lhs, type1 rhs) {               \
    return rhs == lhs;                                         \
  }                                                            \
                                                               \
  template <__VA_ARGS__>                                       \
  inline bool operator!=(type1 lhs, type2 rhs) {               \
    return !(lhs == rhs);                                      \
  }                                                            \
                                                               \
  template <__VA_ARGS__>                                       \
  inline bool operator!=(type2 lhs, type1 rhs) {               \
    return !(lhs == rhs);                                      \
  }

// StaticLocalAutoPtr (in)equality operators

template <class T, class U>
inline bool operator==(const StaticLocalAutoPtr<T>& aLhs,
                       const StaticLocalAutoPtr<U>& aRhs) {
  return aLhs.get() == aRhs.get();
}

template <class T, class U>
inline bool operator!=(const StaticLocalAutoPtr<T>& aLhs,
                       const StaticLocalAutoPtr<U>& aRhs) {
  return !(aLhs == aRhs);
}

REFLEXIVE_EQUALITY_OPERATORS(const StaticLocalAutoPtr<T>&, const U*,
                             lhs.get() == rhs, class T, class U)

REFLEXIVE_EQUALITY_OPERATORS(const StaticLocalAutoPtr<T>&, U*, lhs.get() == rhs,
                             class T, class U)

// Let us compare StaticLocalAutoPtr to 0.
REFLEXIVE_EQUALITY_OPERATORS(const StaticLocalAutoPtr<T>&,
                             StaticLocalPtr_internal::Zero*,
                             lhs.get() == nullptr, class T)

// StaticLocalRefPtr (in)equality operators

template <class T, class U>
inline bool operator==(const StaticLocalRefPtr<T>& aLhs,
                       const StaticLocalRefPtr<U>& aRhs) {
  return aLhs.get() == aRhs.get();
}

template <class T, class U>
inline bool operator!=(const StaticLocalRefPtr<T>& aLhs,
                       const StaticLocalRefPtr<U>& aRhs) {
  return !(aLhs == aRhs);
}

REFLEXIVE_EQUALITY_OPERATORS(const StaticLocalRefPtr<T>&, const U*,
                             lhs.get() == rhs, class T, class U)

REFLEXIVE_EQUALITY_OPERATORS(const StaticLocalRefPtr<T>&, U*, lhs.get() == rhs,
                             class T, class U)

// Let us compare StaticLocalRefPtr to 0.
REFLEXIVE_EQUALITY_OPERATORS(const StaticLocalRefPtr<T>&,
                             StaticLocalPtr_internal::Zero*,
                             lhs.get() == nullptr, class T)

#undef REFLEXIVE_EQUALITY_OPERATORS

}  // namespace mozilla

// Declared in mozilla/RefPtr.h
template <class T>
template <class U>
RefPtr<T>::RefPtr(const mozilla::StaticLocalRefPtr<U>& aOther)
    : RefPtr(aOther.get()) {}

template <class T>
template <class U>
RefPtr<T>& RefPtr<T>::operator=(const mozilla::StaticLocalRefPtr<U>& aOther) {
  return operator=(aOther.get());
}

template <class T>
inline already_AddRefed<T> do_AddRef(
    const mozilla::StaticLocalRefPtr<T>& aObj) {
  RefPtr<T> ref(aObj);
  return ref.forget();
}

#endif  // mozilla_StaticLocalPtr_h
