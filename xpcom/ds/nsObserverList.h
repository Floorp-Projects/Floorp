/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsObserverList_h___
#define nsObserverList_h___

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIObserver.h"
#include "nsHashKeys.h"
#include "nsMaybeWeakPtr.h"
#include "nsSimpleEnumerator.h"
#include "mozilla/Attributes.h"

class nsObserverList : public nsCharPtrHashKey {
  friend class nsObserverService;

 public:
  explicit nsObserverList(const char* aKey) : nsCharPtrHashKey(aKey) {
    MOZ_COUNT_CTOR(nsObserverList);
  }

  nsObserverList(nsObserverList&& aOther)
      : nsCharPtrHashKey(std::move(aOther)),
        mObservers(std::move(aOther.mObservers)) {}

  MOZ_COUNTED_DTOR(nsObserverList)

  [[nodiscard]] nsresult AddObserver(nsIObserver* aObserver, bool aOwnsWeak);
  [[nodiscard]] nsresult RemoveObserver(nsIObserver* aObserver);

  void NotifyObservers(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aSomeData);
  void GetObserverList(nsISimpleEnumerator** aEnumerator);

  // Fill an array with the observers of this category.
  // The array is filled in last-added-first order.
  void FillObserverArray(nsCOMArray<nsIObserver>& aArray);

  // Like FillObserverArray(), but only for strongly held observers.
  void AppendStrongObservers(nsCOMArray<nsIObserver>& aArray);

 private:
  nsMaybeWeakPtrArray<nsIObserver> mObservers;
};

class nsObserverEnumerator final : public nsSimpleEnumerator {
 public:
  NS_DECL_NSISIMPLEENUMERATOR

  explicit nsObserverEnumerator(nsObserverList* aObserverList);

  const nsID& DefaultInterface() override { return NS_GET_IID(nsIObserver); }

 private:
  ~nsObserverEnumerator() override = default;

  int32_t mIndex;  // Counts up from 0
  nsCOMArray<nsIObserver> mObservers;
};

#endif /* nsObserverList_h___ */
