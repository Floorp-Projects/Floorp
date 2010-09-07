/* vim:set st=2 sts=2 ts=2 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is a cache for services in a category.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"

#include "nsXPCOMCID.h"

#include "nsCategoryCache.h"

nsCategoryObserver::nsCategoryObserver(const char* aCategory,
                                       nsCategoryListener* aListener)
  : mListener(nsnull), mCategory(aCategory), mObserversRemoved(false)
{
  if (!mHash.Init()) {
    // OOM
    return;
  }

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
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (serv) {
    serv->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
    serv->AddObserver(this, NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID, PR_FALSE);
    serv->AddObserver(this, NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID, PR_FALSE);
    serv->AddObserver(this, NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID, PR_FALSE);
  }

  for (PRInt32 i = entries.Length() - 1; i >= 0; --i)
    mListener->EntryAdded(entries[i]);
}

nsCategoryObserver::~nsCategoryObserver() {
}

NS_IMPL_ISUPPORTS1(nsCategoryObserver, nsIObserver)

void
nsCategoryObserver::ListenerDied() {
  mListener = nsnull;
  RemoveObservers();
}

NS_HIDDEN_(void)
nsCategoryObserver::RemoveObservers() {
  if (mObserversRemoved)
    return;

  mObserversRemoved = true;
  nsCOMPtr<nsIObserverService> obsSvc =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
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
    if (mHash.Get(str, nsnull))
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
