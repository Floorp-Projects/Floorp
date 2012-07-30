/* vim:set st=2 sts=2 ts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "nsISupportsPrimitives.h"

#include "nsXPCOMCID.h"

#include "nsCategoryCache.h"

nsCategoryObserver::nsCategoryObserver(const char* aCategory,
                                       nsCategoryListener* aListener)
  : mListener(nullptr), mCategory(aCategory), mObserversRemoved(false)
{
  mHash.Init();
  mListener = aListener;

  // First, enumerate the currently existing entries
  nsCOMPtr<nsICategoryManager> catMan =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (!catMan)
    return;

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  nsresult rv = catMan->EnumerateCategory(aCategory,
                                          getter_AddRefs(enumerator));
  if (NS_FAILED(rv))
    return;

  nsTArray<nsCString> entries;
  nsCOMPtr<nsISupports> entry;
  while (NS_SUCCEEDED(enumerator->GetNext(getter_AddRefs(entry)))) {
    nsCOMPtr<nsISupportsCString> entryName = do_QueryInterface(entry, &rv);

    if (NS_SUCCEEDED(rv)) {
      nsCAutoString categoryEntry;
      rv = entryName->GetData(categoryEntry);

      nsCString entryValue;
      catMan->GetCategoryEntry(aCategory,
                               categoryEntry.get(),
                               getter_Copies(entryValue));

      if (NS_SUCCEEDED(rv)) {
        mHash.Put(categoryEntry, entryValue);
        entries.AppendElement(entryValue);
      }
    }
  }

  // Now, listen for changes
  nsCOMPtr<nsIObserverService> serv =
    mozilla::services::GetObserverService();
  if (serv) {
    serv->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    serv->AddObserver(this, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID, false);
    serv->AddObserver(this, NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID, false);
    serv->AddObserver(this, NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID, false);
  }

  for (PRInt32 i = entries.Length() - 1; i >= 0; --i)
    mListener->EntryAdded(entries[i]);
}

nsCategoryObserver::~nsCategoryObserver() {
}

NS_IMPL_ISUPPORTS1(nsCategoryObserver, nsIObserver)

void
nsCategoryObserver::ListenerDied() {
  mListener = nullptr;
  RemoveObservers();
}

NS_HIDDEN_(void)
nsCategoryObserver::RemoveObservers() {
  if (mObserversRemoved)
    return;

  mObserversRemoved = true;
  nsCOMPtr<nsIObserverService> obsSvc =
    mozilla::services::GetObserverService();
  if (obsSvc) {
    obsSvc->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    obsSvc->RemoveObserver(this, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID);
    obsSvc->RemoveObserver(this, NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID);
    obsSvc->RemoveObserver(this, NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID);
  }
}

NS_IMETHODIMP
nsCategoryObserver::Observe(nsISupports* aSubject, const char* aTopic,
                            const PRUnichar* aData) {
  if (!mListener)
    return NS_OK;

  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    mHash.Clear();
    mListener->CategoryCleared();
    RemoveObservers();

    return NS_OK;
  }

  if (!aData ||
      !nsDependentString(aData).Equals(NS_ConvertASCIItoUTF16(mCategory)))
    return NS_OK;

  nsCAutoString str;
  nsCOMPtr<nsISupportsCString> strWrapper(do_QueryInterface(aSubject));
  if (strWrapper)
    strWrapper->GetData(str);

  if (strcmp(aTopic, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID) == 0) {
    // We may get an add notification even when we already have an entry. This
    // is due to the notification happening asynchronously, so if the entry gets
    // added and an nsCategoryObserver gets instantiated before events get
    // processed, we'd get the notification for an existing entry.
    // Do nothing in that case.
    if (mHash.Get(str, nullptr))
      return NS_OK;

    nsCOMPtr<nsICategoryManager> catMan =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
    if (!catMan)
      return NS_OK;

    nsCString entryValue;
    catMan->GetCategoryEntry(mCategory.get(),
                             str.get(),
                             getter_Copies(entryValue));

    mHash.Put(str, entryValue);
    mListener->EntryAdded(entryValue);
  } else if (strcmp(aTopic, NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID) == 0) {
    nsCAutoString val;
    if (mHash.Get(str, &val)) {
      mHash.Remove(str);
      mListener->EntryRemoved(val);
    }
  } else if (strcmp(aTopic, NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID) == 0) {
    mHash.Clear();
    mListener->CategoryCleared();
  }
  return NS_OK;
}
