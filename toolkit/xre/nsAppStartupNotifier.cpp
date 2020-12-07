/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsICategoryManager.h"
#include "nsXPCOM.h"
#include "nsAppStartupNotifier.h"
#include "mozilla/SimpleEnumerator.h"

/* static */
nsresult nsAppStartupNotifier::NotifyObservers(const char* aCategory) {
  NS_ENSURE_ARG(aCategory);
  nsresult rv;

  // now initialize all startup listeners
  nsCOMPtr<nsICategoryManager> categoryManager =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsDependentCString category(aCategory);

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = categoryManager->EnumerateCategory(category, getter_AddRefs(enumerator));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (auto& categoryEntry : SimpleEnumerator<nsICategoryEntry>(enumerator)) {
    nsAutoCString contractId;
    categoryEntry->GetValue(contractId);

    nsCOMPtr<nsISupports> startupInstance;

    // If we see the word "service," in the beginning
    // of the contractId then we create it as a service
    // if not we do a createInstance
    if (StringBeginsWith(contractId, "service,"_ns)) {
      startupInstance = do_GetService(contractId.get() + 8, &rv);
    } else {
      startupInstance = do_CreateInstance(contractId.get(), &rv);
    }

    if (NS_SUCCEEDED(rv)) {
      // Try to QI to nsIObserver
      nsCOMPtr<nsIObserver> startupObserver =
          do_QueryInterface(startupInstance, &rv);
      if (NS_SUCCEEDED(rv)) {
        rv = startupObserver->Observe(nullptr, aCategory, nullptr);

        // mainly for debugging if you want to know if your observer worked.
        NS_ASSERTION(NS_SUCCEEDED(rv), "Startup Observer failed!\n");
      }
    } else {
#ifdef DEBUG
      nsAutoCString warnStr("Cannot create startup observer : ");
      warnStr += contractId.get();
      NS_WARNING(warnStr.get());
#endif
    }
  }

  return NS_OK;
}
