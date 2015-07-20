/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCategoryCache_h_
#define nsCategoryCache_h_

#include "mozilla/Attributes.h"

#include "nsICategoryManager.h"
#include "nsIObserver.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsPrimitives.h"

#include "nsServiceManagerUtils.h"

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsInterfaceHashtable.h"

#include "nsXPCOM.h"

class nsCategoryObserver final : public nsIObserver
{
  ~nsCategoryObserver();

public:
  explicit nsCategoryObserver(const char* aCategory);

  void ListenerDied();
  nsInterfaceHashtable<nsCStringHashKey, nsISupports>& GetHash()
  {
    return mHash;
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
private:
  void RemoveObservers();

  nsInterfaceHashtable<nsCStringHashKey, nsISupports> mHash;
  nsCString mCategory;
  bool mObserversRemoved;
};

/**
 * This is a helper class that caches services that are registered in a certain
 * category. The intended usage is that a service stores a variable of type
 * nsCategoryCache<nsIFoo> in a member variable, where nsIFoo is the interface
 * that these services should implement. The constructor of this class should
 * then get the name of the category.
 */
template<class T>
class nsCategoryCache final
{
public:
  explicit nsCategoryCache(const char* aCategory)
    : mCategoryName(aCategory)
  {
  }
  ~nsCategoryCache()
  {
    if (mObserver) {
      mObserver->ListenerDied();
    }
  }

  void GetEntries(nsCOMArray<T>& aResult)
  {
    // Lazy initialization, so that services in this category can't
    // cause reentrant getService (bug 386376)
    if (!mObserver) {
      mObserver = new nsCategoryObserver(mCategoryName.get());
    }

    for (auto iter = mObserver->GetHash().Iter(); !iter.Done(); iter.Next()) {
      nsISupports* entry = iter.UserData();
      nsCOMPtr<T> service = do_QueryInterface(entry);
      if (service) {
        aResult.AppendObject(service);
      }
    }
  }

private:
  // Not to be implemented
  nsCategoryCache(const nsCategoryCache<T>&);

  nsCString mCategoryName;
  nsRefPtr<nsCategoryObserver> mObserver;
};

#endif
