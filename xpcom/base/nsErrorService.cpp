/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsErrorService.h"
#include "nsCRTGlue.h"
#include "nsAutoPtr.h"

static void*
CloneCString(nsHashKey *aKey, void *aData, void* aClosure)
{
  return NS_strdup((const char*)aData);
}

static bool
DeleteCString(nsHashKey *aKey, void *aData, void* aClosure)
{
  NS_Free(aData);
  return true;
}

nsInt2StrHashtable::nsInt2StrHashtable()
  : mHashtable(CloneCString, nullptr, DeleteCString, nullptr, 16)
{
}

nsresult
nsInt2StrHashtable::Put(uint32_t aKey, const char* aData)
{
  char* value = NS_strdup(aData);
  if (value == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsPRUint32Key k(aKey);
  char* oldValue = (char*)mHashtable.Put(&k, value);
  if (oldValue)
    NS_Free(oldValue);
  return NS_OK;
}

char*
nsInt2StrHashtable::Get(uint32_t aKey)
{
  nsPRUint32Key k(aKey);
  const char* value = (const char*)mHashtable.Get(&k);
  if (value == nullptr)
    return nullptr;
  return NS_strdup(value);
}

nsresult
nsInt2StrHashtable::Remove(uint32_t aKey)
{
  nsPRUint32Key k(aKey);
  char* oldValue = (char*)mHashtable.Remove(&k);
  if (oldValue)
    NS_Free(oldValue);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(nsErrorService, nsIErrorService)

nsresult
nsErrorService::Create(nsISupports* aOuter, const nsIID& aIID, void** aInstancePtr)
{
  if (NS_WARN_IF(aOuter))
    return NS_ERROR_NO_AGGREGATION;
  nsRefPtr<nsErrorService> serv = new nsErrorService();
  return serv->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsErrorService::RegisterErrorStringBundle(int16_t aErrorModule, const char* aStringBundleURL)
{
  return mErrorStringBundleURLMap.Put(aErrorModule, aStringBundleURL);
}

NS_IMETHODIMP
nsErrorService::UnregisterErrorStringBundle(int16_t aErrorModule)
{
  return mErrorStringBundleURLMap.Remove(aErrorModule);
}

NS_IMETHODIMP
nsErrorService::GetErrorStringBundle(int16_t aErrorModule, char** aResult)
{
  char* value = mErrorStringBundleURLMap.Get(aErrorModule);
  if (value == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;
  *aResult = value;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
