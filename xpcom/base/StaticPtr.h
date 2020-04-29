/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticPtr_h
#define mozilla_StaticPtr_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

/**
 * StaticAutoPtr and StaticRefPtr are like UniquePtr and RefPtr, except they
 * are suitable for use as global variables.
 *
 * In particular, a global instance of Static{Auto,Ref}Ptr doesn't cause the
 * compiler to emit  a static initializer (in release builds, anyway).
 *
 * In order to accomplish this, Static{Auto,Ref}Ptr must have a trivial
 * constructor and destructor.  As a consequence, it cannot initialize its raw
 * pointer to 0 on construction, and it cannot delete/release its raw pointer
 * upon destruction.
 *
 * Since the compiler guarantees that all global variables are initialized to
 * 0, these trivial constructors are safe.  Since we rely on this, the clang
 * plugin, run as part of our "static analysis" builds, makes it a compile-time
 * error to use Static{Auto,Ref}Ptr as anything except a global variable.
 *
 * Static{Auto,Ref}Ptr have a limited interface as compared to ns{Auto,Ref}Ptr;
 * this is intentional, since their range of acceptable uses is smaller.
 */

template <class T>
class MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS StaticAutoPtr {
 public:
  // In debug builds, check that mRawPtr is initialized for us as we expect
  // by the compiler.  In non-debug builds, don't declare a constructor
  // so that the compiler can see that the constructor is trivial.
#ifdef DEBUG
  StaticAutoPtr() {
#  ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wuninitialized"
    // False positive with gcc. See bug 1430729
#  endif
    MOZ_ASSERT(!mRawPtr);
#  ifdef __GNUC__
#    pragma GCC diagnostic pop
#  endif
  }
#endif

  StaticAutoPtr<T>& operator=(T* aRhs) {
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
  // Disallow copy constructor, but only in debug mode.  We only define
  // a default constructor in debug mode (see above); if we declared
  // this constructor always, the compiler wouldn't generate a trivial
  // default constructor for us in non-debug mode.
#ifdef DEBUG
  StaticAutoPtr(StaticAutoPtr<T>& aOther);
#endif

  void Assign(T* aNewPtr) {
    MOZ_ASSERT(!aNewPtr || mRawPtr != aNewPtr);
    T* oldPtr = mRawPtr;
    mRawPtr = aNewPtr;
    delete oldPtr;
  }

  T* mRawPtr;
};

template <class T>
class MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS StaticRefPtr {
 public:
  // In debug builds, check that mRawPtr is initialized for us as we expect
  // by the compiler.  In non-debug builds, don't declare a constructor
  // so that the compiler can see that the constructor is trivial.
#ifdef DEBUG
  StaticRefPtr() {
#  ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wuninitialized"
    // False positive with gcc. See bug 1430729
#  endif
    MOZ_ASSERT(!mRawPtr);
#  ifdef __GNUC__
#    pragma GCC diagnostic pop
#  endif
  }
#endif

  StaticRefPtr<T>& operator=(T* aRhs) {
    AssignWithAddref(aRhs);
    return *this;
  }

  StaticRefPtr<T>& operator=(const StaticRefPtr<T>& aRhs) {
    return (this = aRhs.mRawPtr);
  }

  StaticRefPtr<T>& operator=(already_AddRefed<T>& aRhs) {
    AssignAssumingAddRef(aRhs.take());
    return *this;
  }

  template <typename U>
  StaticRefPtr<T>& operator=(RefPtr<U>&& aRhs) {
    AssignAssumingAddRef(aRhs.forget().take());
    return *this;
  }

  StaticRefPtr<T>& operator=(already_AddRefed<T>&& aRhs) {
    AssignAssumingAddRef(aRhs.take());
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

namespace StaticPtr_internal {
class Zero;
}  // namespace StaticPtr_internal

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

// StaticAutoPtr (in)equality operators

template <class T, class U>
inline bool operator==(const StaticAutoPtr<T>& aLhs,
                       const StaticAutoPtr<U>& aRhs) {
  return aLhs.get() == aRhs.get();
}

template <class T, class U>
inline bool operator!=(const StaticAutoPtr<T>& aLhs,
                       const StaticAutoPtr<U>& aRhs) {
  return !(aLhs == aRhs);
}

REFLEXIVE_EQUALITY_OPERATORS(const StaticAutoPtr<T>&, const U*,
                             lhs.get() == rhs, class T, class U)

REFLEXIVE_EQUALITY_OPERATORS(const StaticAutoPtr<T>&, U*, lhs.get() == rhs,
                             class T, class U)

// Let us compare StaticAutoPtr to 0.
REFLEXIVE_EQUALITY_OPERATORS(const StaticAutoPtr<T>&, StaticPtr_internal::Zero*,
                             lhs.get() == nullptr, class T)

// StaticRefPtr (in)equality operators

template <class T, class U>
inline bool operator==(const StaticRefPtr<T>& aLhs,
                       const StaticRefPtr<U>& aRhs) {
  return aLhs.get() == aRhs.get();
}

template <class T, class U>
inline bool operator!=(const StaticRefPtr<T>& aLhs,
                       const StaticRefPtr<U>& aRhs) {
  return !(aLhs == aRhs);
}

REFLEXIVE_EQUALITY_OPERATORS(const StaticRefPtr<T>&, const U*, lhs.get() == rhs,
                             class T, class U)

REFLEXIVE_EQUALITY_OPERATORS(const StaticRefPtr<T>&, U*, lhs.get() == rhs,
                             class T, class U)

// Let us compare StaticRefPtr to 0.
REFLEXIVE_EQUALITY_OPERATORS(const StaticRefPtr<T>&, StaticPtr_internal::Zero*,
                             lhs.get() == nullptr, class T)

#undef REFLEXIVE_EQUALITY_OPERATORS

}  // namespace mozilla

// Declared in mozilla/RefPtr.h
template <class T>
template <class U>
RefPtr<T>::RefPtr(const mozilla::StaticRefPtr<U>& aOther)
    : RefPtr(aOther.get()) {}

template <class T>
template <class U>
RefPtr<T>& RefPtr<T>::operator=(const mozilla::StaticRefPtr<U>& aOther) {
  return operator=(aOther.get());
}

template <class T>
inline already_AddRefed<T> do_AddRef(const mozilla::StaticRefPtr<T>& aObj) {
  RefPtr<T> ref(aObj);
  return ref.forget();
}

#endif
