/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsObserverList.h"

#include "mozilla/ResultExtensions.h"
#include "nsCOMArray.h"
#include "xpcpublic.h"

nsresult nsObserverList::AddObserver(nsIObserver* anObserver, bool ownsWeak) {
  NS_ASSERTION(anObserver, "Null input");

  MOZ_TRY(mObservers.AppendWeakElement(anObserver, ownsWeak));
  return NS_OK;
}

nsresult nsObserverList::RemoveObserver(nsIObserver* anObserver) {
  NS_ASSERTION(anObserver, "Null input");

  MOZ_TRY(mObservers.RemoveWeakElement(anObserver));
  return NS_OK;
}

void nsObserverList::GetObserverList(nsISimpleEnumerator** anEnumerator) {
  RefPtr<nsObserverEnumerator> e(new nsObserverEnumerator(this));
  e.forget(anEnumerator);
}

void nsObserverList::FillObserverArray(nsCOMArray<nsIObserver>& aArray) {
  aArray.SetCapacity(mObservers.Length());

  nsMaybeWeakPtrArray<nsIObserver> observers(mObservers);

  for (int32_t i = observers.Length() - 1; i >= 0; --i) {
    nsCOMPtr<nsIObserver> observer = observers[i].GetValue();
    if (observer) {
      aArray.AppendObject(observer);
    } else {
      // the object has gone away, remove the weakref
      mObservers.RemoveElementAt(i);
    }
  }
}

void nsObserverList::AppendStrongObservers(nsCOMArray<nsIObserver>& aArray) {
  aArray.SetCapacity(aArray.Length() + mObservers.Length());

  for (int32_t i = mObservers.Length() - 1; i >= 0; --i) {
    if (!mObservers[i].IsWeak()) {
      nsCOMPtr<nsIObserver> observer = mObservers[i].GetValue();
      aArray.AppendObject(observer);
    }
  }
}

void nsObserverList::NotifyObservers(nsISupports* aSubject, const char* aTopic,
                                     const char16_t* someData) {
  nsCOMArray<nsIObserver> observers;
  FillObserverArray(observers);

  for (int32_t i = 0; i < observers.Count(); ++i) {
    observers[i]->Observe(aSubject, aTopic, someData);
  }
}

nsObserverEnumerator::nsObserverEnumerator(nsObserverList* aObserverList)
    : mIndex(0) {
  aObserverList->FillObserverArray(mObservers);
}

NS_IMETHODIMP
nsObserverEnumerator::HasMoreElements(bool* aResult) {
  *aResult = (mIndex < mObservers.Count());
  return NS_OK;
}

NS_IMETHODIMP
nsObserverEnumerator::GetNext(nsISupports** aResult) {
  if (mIndex == mObservers.Count()) {
    return NS_ERROR_FAILURE;
  }

  NS_ADDREF(*aResult = mObservers[mIndex]);
  ++mIndex;
  return NS_OK;
}
