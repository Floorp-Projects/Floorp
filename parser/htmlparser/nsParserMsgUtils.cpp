/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIStringBundle.h"
#include "nsString.h"
#include "nsParserMsgUtils.h"
#include "nsNetCID.h"
#include "mozilla/Components.h"

static nsresult GetBundle(const char* aPropFileName,
                          nsIStringBundle** aBundle) {
  NS_ENSURE_ARG_POINTER(aPropFileName);
  NS_ENSURE_ARG_POINTER(aBundle);

  // Create a bundle for the localization

  nsCOMPtr<nsIStringBundleService> stringService =
      mozilla::components::StringBundle::Service();
  if (!stringService) return NS_ERROR_FAILURE;

  return stringService->CreateBundle(aPropFileName, aBundle);
}

nsresult nsParserMsgUtils::GetLocalizedStringByName(const char* aPropFileName,
                                                    const char* aKey,
                                                    nsString& oVal) {
  oVal.Truncate();

  NS_ENSURE_ARG_POINTER(aKey);

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = GetBundle(aPropFileName, getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle) {
    nsAutoString valUni;
    rv = bundle->GetStringFromName(aKey, valUni);
    if (NS_SUCCEEDED(rv)) {
      oVal.Assign(valUni);
    }
  }

  return rv;
}

nsresult nsParserMsgUtils::GetLocalizedStringByID(const char* aPropFileName,
                                                  uint32_t aID,
                                                  nsString& oVal) {
  oVal.Truncate();

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = GetBundle(aPropFileName, getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle) {
    nsAutoString valUni;
    rv = bundle->GetStringFromID(aID, valUni);
    if (NS_SUCCEEDED(rv)) {
      oVal.Assign(valUni);
    }
  }

  return rv;
}
