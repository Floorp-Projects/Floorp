/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsObserverList_h___
#define nsObserverList_h___

#include "nsISupports.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIObserver.h"
#include "nsIWeakReference.h"
#include "nsHashKeys.h"
#include "nsISimpleEnumerator.h"

struct ObserverRef
{
  ObserverRef(const ObserverRef& o) :
    isWeakRef(o.isWeakRef), ref(o.ref) { }
  
  ObserverRef(nsIObserver* aObserver) : isWeakRef(false), ref(aObserver) { }
  ObserverRef(nsIWeakReference* aWeak) : isWeakRef(true), ref(aWeak) { }

  bool isWeakRef;
  nsCOMPtr<nsISupports> ref;

  nsIObserver* asObserver() {
    NS_ASSERTION(!isWeakRef, "Isn't a strong ref.");
    return static_cast<nsIObserver*>((nsISupports*) ref);
  }

  nsIWeakReference* asWeak() {
    NS_ASSERTION(isWeakRef, "Isn't a weak ref.");
    return static_cast<nsIWeakReference*>((nsISupports*) ref);
  }

  bool operator==(nsISupports* b) const { return ref == b; }
};

class nsObserverList : public nsCharPtrHashKey
{
public:
  nsObserverList(const char *key) : nsCharPtrHashKey(key)
  { MOZ_COUNT_CTOR(nsObserverList); }

  ~nsObserverList() { MOZ_COUNT_DTOR(nsObserverList); }

  nsresult AddObserver(nsIObserver* anObserver, bool ownsWeak);
  nsresult RemoveObserver(nsIObserver* anObserver);

  void NotifyObservers(nsISupports *aSubject,
                       const char *aTopic,
                       const PRUnichar *someData);
  nsresult GetObserverList(nsISimpleEnumerator** anEnumerator);

  // Fill an array with the observers of this category.
  // The array is filled in last-added-first order.
  void FillObserverArray(nsCOMArray<nsIObserver> &aArray);

  // Unmark any strongly held observers implemented in JS so the cycle
  // collector will not traverse them.
  void UnmarkGrayStrongObservers();

private:
  nsTArray<ObserverRef> mObservers;
};

class nsObserverEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    nsObserverEnumerator(nsObserverList* aObserverList);

private:
    ~nsObserverEnumerator() { }

    PRInt32 mIndex; // Counts up from 0
    nsCOMArray<nsIObserver> mObservers;
};

#endif /* nsObserverList_h___ */
