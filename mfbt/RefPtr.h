/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Helpers for defining and using refcounted objects. */

#ifndef mozilla_RefPtr_h
#define mozilla_RefPtr_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Move.h"
#include "mozilla/RefCounted.h"
#include "mozilla/RefCountType.h"
#include "mozilla/nsRefPtr.h"
#include "mozilla/TypeTraits.h"
#if defined(MOZILLA_INTERNAL_API)
#include "nsXPCOM.h"
#endif

#if defined(MOZILLA_INTERNAL_API) && \
    !defined(MOZILLA_XPCOMRT_API) && \
    (defined(DEBUG) || defined(FORCE_BUILD_REFCNT_LOGGING))
#define MOZ_REFCOUNTED_LEAK_CHECKING
#endif

namespace mozilla {

template<typename T> class OutParamRef;
template<typename T> OutParamRef<T> byRef(RefPtr<T>&);

/**
 * RefPtr points to a refcounted thing that has AddRef and Release
 * methods to increase/decrease the refcount, respectively.  After a
 * RefPtr<T> is assigned a T*, the T* can be used through the RefPtr
 * as if it were a T*.
 *
 * A RefPtr can forget its underlying T*, which results in the T*
 * being wrapped in a temporary object until the T* is either
 * re-adopted from or released by the temporary.
 */
template<typename T>
class RefPtr
{
  // To allow them to use unref()
  friend class OutParamRef<T>;

  struct DontRef {};

public:
  RefPtr() : mPtr(0) {}
  RefPtr(const RefPtr& aOther) : mPtr(ref(aOther.mPtr)) {}
  MOZ_IMPLICIT RefPtr(already_AddRefed<T>& aOther) : mPtr(aOther.take()) {}
  MOZ_IMPLICIT RefPtr(already_AddRefed<T>&& aOther) : mPtr(aOther.take()) {}
  MOZ_IMPLICIT RefPtr(T* aVal) : mPtr(ref(aVal)) {}

  template<typename U>
  MOZ_IMPLICIT RefPtr(const RefPtr<U>& aOther) : mPtr(ref(aOther.get())) {}

  ~RefPtr() { unref(mPtr); }

  RefPtr& operator=(const RefPtr& aOther)
  {
    assign(ref(aOther.mPtr));
    return *this;
  }
  RefPtr& operator=(already_AddRefed<T>& aOther)
  {
    assign(aOther.take());
    return *this;
  }
  RefPtr& operator=(already_AddRefed<T>&& aOther)
  {
    assign(aOther.take());
    return *this;
  }
  RefPtr& operator=(T* aVal)
  {
    assign(ref(aVal));
    return *this;
  }

  template<typename U>
  RefPtr& operator=(const RefPtr<U>& aOther)
  {
    assign(ref(aOther.get()));
    return *this;
  }

  already_AddRefed<T> forget()
  {
    T* tmp = mPtr;
    mPtr = nullptr;
    return already_AddRefed<T>(tmp);
  }

  T* get() const { return mPtr; }
  operator T*() const
#ifdef MOZ_HAVE_REF_QUALIFIERS
  &
#endif
  { return mPtr; }
  T* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN { return mPtr; }
  T& operator*() const { return *mPtr; }

#ifdef MOZ_HAVE_REF_QUALIFIERS
  // Don't allow implicit conversion of temporary RefPtr to raw pointer, because
  // the refcount might be one and the pointer will immediately become invalid.
  operator T*() const && = delete;

  // Needed to avoid the deleted operator above
  explicit operator bool() const { return !!mPtr; }
#endif

private:
  void assign(T* aVal)
  {
    T* tmp = mPtr;
    mPtr = aVal;
    unref(tmp);
  }

  T* MOZ_OWNING_REF mPtr;

  static MOZ_ALWAYS_INLINE T* ref(T* aVal)
  {
    if (aVal) {
      aVal->AddRef();
    }
    return aVal;
  }

  static MOZ_ALWAYS_INLINE void unref(T* aVal)
  {
    if (aVal) {
      aVal->Release();
    }
  }
};

/**
 * OutParamRef is a wrapper that tracks a refcounted pointer passed as
 * an outparam argument to a function.  OutParamRef implements COM T**
 * outparam semantics: this requires the callee to AddRef() the T*
 * returned through the T** outparam on behalf of the caller.  This
 * means the caller (through OutParamRef) must Release() the old
 * object contained in the tracked RefPtr.  It's OK if the callee
 * returns the same T* passed to it through the T** outparam, as long
 * as the callee obeys the COM discipline.
 *
 * Prefer returning already_AddRefed<T> from functions over creating T**
 * outparams and passing OutParamRef<T> to T**.  Prefer RefPtr<T>*
 * outparams over T** outparams.
 */
template<typename T>
class OutParamRef
{
  friend OutParamRef byRef<T>(RefPtr<T>&);

public:
  ~OutParamRef()
  {
    RefPtr<T>::unref(mRefPtr.mPtr);
    mRefPtr.mPtr = mTmp;
  }

  operator T**() { return &mTmp; }

private:
  explicit OutParamRef(RefPtr<T>& p) : mRefPtr(p), mTmp(p.get()) {}

  RefPtr<T>& mRefPtr;
  T* mTmp;

  OutParamRef() = delete;
  OutParamRef& operator=(const OutParamRef&) = delete;
};

/**
 * byRef cooperates with OutParamRef to implement COM outparam semantics.
 */
template<typename T>
OutParamRef<T>
byRef(RefPtr<T>& aPtr)
{
  return OutParamRef<T>(aPtr);
}

} // namespace mozilla

// Declared in nsRefPtr.h
template<class T> template<class U>
nsRefPtr<T>::nsRefPtr(mozilla::RefPtr<U>&& aOther)
  : nsRefPtr(aOther.forget())
{
}

template<class T> template<class U>
nsRefPtr<T>&
nsRefPtr<T>::operator=(mozilla::RefPtr<U>&& aOther)
{
  assign_assuming_AddRef(aOther.forget().take());
  return *this;
}

#endif /* mozilla_RefPtr_h */
