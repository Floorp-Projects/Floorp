/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsObserverList.h"

#include "mozilla/ResultExtensions.h"
#include "mozilla/Try.h"
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

nsCOMArray<nsIObserver> nsObserverList::ReverseCloneObserverArray() {
  nsCOMArray<nsIObserver> array;
  array.SetCapacity(mObservers.Length());

  // XXX This could also use RemoveElementsBy if we lifted the promise to fill
  // aArray in reverse order. Although there shouldn't be anyone explicitly
  // relying on the order, changing this might cause subtle problems, so we
  // better leave it as it is.

  for (int32_t i = mObservers.Length() - 1; i >= 0; --i) {
    nsCOMPtr<nsIObserver> observer = mObservers[i].GetValue();
    if (observer) {
      array.AppendElement(observer.forget());
    } else {
      // the object has gone away, remove the weakref
      mObservers.RemoveElementAt(i);
    }
  }

  return array;
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
  const nsCOMArray<nsIObserver> observers = ReverseCloneObserverArray();

  for (int32_t i = 0; i < observers.Count(); ++i) {
    observers[i]->Observe(aSubject, aTopic, someData);
  }
}

nsObserverEnumerator::nsObserverEnumerator(nsObserverList* aObserverList)
    : mIndex(0), mObservers(aObserverList->ReverseCloneObserverArray()) {}

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
