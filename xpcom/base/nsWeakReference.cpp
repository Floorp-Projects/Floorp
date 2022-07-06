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

class nsWeakReference final : public nsIWeakReference {
 public:
  // nsISupports...
  NS_DECL_ISUPPORTS

  // nsIWeakReference...
  NS_DECL_NSIWEAKREFERENCE

 private:
  friend class nsSupportsWeakReference;

  explicit nsWeakReference(nsSupportsWeakReference* aReferent)
      : nsIWeakReference(aReferent)
  // ...I can only be constructed by an |nsSupportsWeakReference|
  {}

  ~nsWeakReference()
  // ...I will only be destroyed by calling |delete| myself.
  {
    MOZ_WEAKREF_ASSERT_OWNINGTHREAD;
    if (mObject) {
      static_cast<nsSupportsWeakReference*>(mObject)->NoticeProxyDestruction();
    }
  }

  void NoticeReferentDestruction()
  // ...called (only) by an |nsSupportsWeakReference| from _its_ dtor.
  {
    MOZ_WEAKREF_ASSERT_OWNINGTHREAD;
    mObject = nullptr;
  }
};

nsresult nsQueryReferent::operator()(const nsIID& aIID, void** aAnswer) const {
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

nsIWeakReference* NS_GetWeakReference(nsISupportsWeakReference* aInstancePtr,
                                      nsresult* aErrorPtr) {
  nsresult status;

  nsIWeakReference* result = nullptr;

  if (aInstancePtr) {
    status = aInstancePtr->GetWeakReference(&result);
  } else {
    status = NS_ERROR_NULL_POINTER;
  }

  if (aErrorPtr) {
    *aErrorPtr = status;
  }

  return result;
}

nsIWeakReference*  // or else |already_AddRefed<nsIWeakReference>|
NS_GetWeakReference(nsISupports* aInstancePtr, nsresult* aErrorPtr) {
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

nsresult nsSupportsWeakReference::GetWeakReference(
    nsIWeakReference** aInstancePtr) {
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
nsWeakReference::QueryReferentFromScript(const nsIID& aIID,
                                         void** aInstancePtr) {
  return QueryReferent(aIID, aInstancePtr);
}

nsresult nsIWeakReference::QueryReferent(const nsIID& aIID,
                                         void** aInstancePtr) {
  MOZ_WEAKREF_ASSERT_OWNINGTHREAD;

  if (!mObject) {
    return NS_ERROR_NULL_POINTER;
  }

  return mObject->QueryInterface(aIID, aInstancePtr);
}

size_t nsWeakReference::SizeOfOnlyThis(mozilla::MallocSizeOf aMallocSizeOf) {
  return aMallocSizeOf(this);
}

void nsSupportsWeakReference::ClearWeakReferences() {
  if (mProxy) {
    mProxy->NoticeReferentDestruction();
    mProxy = nullptr;
  }
}
