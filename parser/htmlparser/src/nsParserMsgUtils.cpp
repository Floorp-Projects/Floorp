/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsITextContent.h"
#include "nsISupportsArray.h"
#include "nsXPIDLString.h"
#include "nsParserMsgUtils.h"
#include "nsNetCID.h"

static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);

// This code is derived from nsFormControlHelper::GetLocalizedString()

static nsresult GetBundle(const char * aPropFileName, nsIStringBundle **aBundle)
{
  NS_ENSURE_ARG_POINTER(aPropFileName);
  NS_ENSURE_ARG_POINTER(aBundle);

  // Create a bundle for the localization
  nsresult rv;
  
  nsCOMPtr<nsIStringBundleService> stringService = 
    do_GetService(kStringBundleServiceCID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = stringService->CreateBundle(aPropFileName, aBundle);
  
  return rv;
}

nsresult
nsParserMsgUtils::GetLocalizedStringByName(const char * aPropFileName, const char* aKey, nsString& oVal)
{
  oVal.Truncate();

  NS_ENSURE_ARG_POINTER(aKey);

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = GetBundle(aPropFileName,getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle) {
    nsXPIDLString valUni;
    nsAutoString key; key.AssignWithConversion(aKey);
    rv = bundle->GetStringFromName(key.get(), getter_Copies(valUni));
    if (NS_SUCCEEDED(rv) && valUni) {
      oVal.Assign(valUni);
    }  
  }

  return rv;
}

nsresult
nsParserMsgUtils::GetLocalizedStringByID(const char * aPropFileName, PRUint32 aID, nsString& oVal)
{
  oVal.Truncate();

  nsCOMPtr<nsIStringBundle> bundle;
  nsresult rv = GetBundle(aPropFileName,getter_AddRefs(bundle));
  if (NS_SUCCEEDED(rv) && bundle) {
    nsXPIDLString valUni;
    rv = bundle->GetStringFromID(aID, getter_Copies(valUni));
    if (NS_SUCCEEDED(rv) && valUni) {
      oVal.Assign(valUni);
    }  
  }

  return rv;
}
