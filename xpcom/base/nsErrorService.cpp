/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsErrorService.h"
#include "nsCRTGlue.h"
#include "nsAutoPtr.h"

NS_IMPL_ISUPPORTS(nsErrorService, nsIErrorService)

nsresult
nsErrorService::Create(nsISupports* aOuter, const nsIID& aIID, void** aInstancePtr)
{
  if (NS_WARN_IF(aOuter)) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsRefPtr<nsErrorService> serv = new nsErrorService();
  return serv->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsErrorService::RegisterErrorStringBundle(int16_t aErrorModule, const char* aStringBundleURL)
{
  mErrorStringBundleURLMap.Put(aErrorModule, new nsCString(aStringBundleURL));
  return NS_OK;
}

NS_IMETHODIMP
nsErrorService::UnregisterErrorStringBundle(int16_t aErrorModule)
{
  mErrorStringBundleURLMap.Remove(aErrorModule);
  return NS_OK;
}

NS_IMETHODIMP
nsErrorService::GetErrorStringBundle(int16_t aErrorModule, char** aResult)
{
  nsCString* bundleURL = mErrorStringBundleURLMap.Get(aErrorModule);
  *aResult = ToNewCString(*bundleURL);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
