/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// nsWeakReference.cpp

#include "mozilla/Attributes.h"

#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"

#ifdef MOZ_THREAD_SAFETY_OWNERSHIP_CHECKS_SUPPORTED

#define MOZ_WEAKREF_DECL_OWNINGTHREAD nsAutoOwningThread _mWeakRefOwningThread;
#define MOZ_WEAKREF_ASSERT_OWNINGTHREAD \
  _mWeakRefOwningThread.AssertOwnership("nsWeakReference not thread-safe")
#define MOZ_WEAKREF_ASSERT_OWNINGTHREAD_DELEGATED(that) \
  (that)->_mWeakRefOwningThread.AssertOwnership("nsWeakReference not thread-safe")

#else

#define MOZ_WEAKREF_DECL_OWNINGTHREAD
#define MOZ_WEAKREF_ASSERT_OWNINGTHREAD do { } while (false)
#define MOZ_WEAKREF_ASSERT_OWNINGTHREAD_DELEGATED(that) do { } while (false)

#endif

class nsWeakReference final : public nsIWeakReference
{
public:
  // nsISupports...
  NS_DECL_ISUPPORTS

  // nsIWeakReference...
  NS_DECL_NSIWEAKREFERENCE
  size_t SizeOfOnlyThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

private:
  MOZ_WEAKREF_DECL_OWNINGTHREAD

  friend class nsSupportsWeakReference;

  explicit nsWeakReference(nsSupportsWeakReference* aReferent)
    : mReferent(aReferent)
    // ...I can only be constructed by an |nsSupportsWeakReference|
  {
  }

  ~nsWeakReference()
  // ...I will only be destroyed by calling |delete| myself.
  {
    MOZ_WEAKREF_ASSERT_OWNINGTHREAD;
    if (mReferent) {
      mReferent->NoticeProxyDestruction();
    }
  }

  void
  NoticeReferentDestruction()
  // ...called (only) by an |nsSupportsWeakReference| from _its_ dtor.
  {
    MOZ_WEAKREF_ASSERT_OWNINGTHREAD;
    mReferent = nullptr;
  }

  nsSupportsWeakReference* MOZ_NON_OWNING_REF mReferent;
};

nsresult
nsQueryReferent::operator()(const nsIID& aIID, void** aAnswer) const
{
  nsresult status;
  if (mWeakPtr) {
    if (NS_FAILED(status = mWeakPtr->QueryReferent(aIID, aAnswer))) {
      *aAnswer = 0;
    }
  } else {
    status = NS_ERROR_NULL_POINTER;
  }

  if (mErrorPtr) {
    *mErrorPtr = status;
  }
  return status;
}

nsIWeakReference*  // or else |already_AddRefed<nsIWeakReference>|
NS_GetWeakReference(nsISupports* aInstancePtr, nsresult* aErrorPtr)
{
  nsresult status;

  nsIWeakReference* result = nullptr;

  if (aInstancePtr) {
    nsCOMPtr<nsISupportsWeakReference> factoryPtr =
      do_QueryInterface(aInstancePtr, &status);
    if (factoryPtr) {
      status = factoryPtr->GetWeakReference(&result);
    }
    // else, |status| has already been set by |do_QueryInterface|
  } else {
    status = NS_ERROR_NULL_POINTER;
  }

  if (aErrorPtr) {
    *aErrorPtr = status;
  }
  return result;
}

nsresult
nsSupportsWeakReference::GetWeakReference(nsIWeakReference** aInstancePtr)
{
  if (!aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (!mProxy) {
    mProxy = new nsWeakReference(this);
  } else {
    MOZ_WEAKREF_ASSERT_OWNINGTHREAD_DELEGATED(mProxy);
  }
  *aInstancePtr = mProxy;

  nsresult status;
  if (!*aInstancePtr) {
    status = NS_ERROR_OUT_OF_MEMORY;
  } else {
    NS_ADDREF(*aInstancePtr);
    status = NS_OK;
  }

  return status;
}

NS_IMPL_ISUPPORTS(nsWeakReference, nsIWeakReference)

NS_IMETHODIMP
nsWeakReference::QueryReferent(const nsIID& aIID, void** aInstancePtr)
{
  MOZ_WEAKREF_ASSERT_OWNINGTHREAD;

  return mReferent ? mReferent->QueryInterface(aIID, aInstancePtr) :
                     NS_ERROR_NULL_POINTER;
}

size_t
nsWeakReference::SizeOfOnlyThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

void
nsSupportsWeakReference::ClearWeakReferences()
{
  if (mProxy) {
    mProxy->NoticeReferentDestruction();
    mProxy = nullptr;
  }
}

