/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsErrorService.h"
#include "nsCRTGlue.h"
#include "nsAutoPtr.h"

static void*
CloneCString(nsHashKey *aKey, void *aData, void* closure)
{
  return NS_strdup((const char*)aData);
}

static bool
DeleteCString(nsHashKey *aKey, void *aData, void* closure)
{
  NS_Free(aData);
  return true;
}

nsInt2StrHashtable::nsInt2StrHashtable()
    : mHashtable(CloneCString, nullptr, DeleteCString, nullptr, 16)
{
}

nsresult
nsInt2StrHashtable::Put(uint32_t key, const char* aData)
{
  char* value = NS_strdup(aData);
  if (value == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;
  nsPRUint32Key k(key);
  char* oldValue = (char*)mHashtable.Put(&k, value);
  if (oldValue)
    NS_Free(oldValue);
  return NS_OK;
}

char* 
nsInt2StrHashtable::Get(uint32_t key)
{
  nsPRUint32Key k(key);
  const char* value = (const char*)mHashtable.Get(&k);
  if (value == nullptr)
    return nullptr;
  return NS_strdup(value);
}

nsresult
nsInt2StrHashtable::Remove(uint32_t key)
{
  nsPRUint32Key k(key);
  char* oldValue = (char*)mHashtable.Remove(&k);
  if (oldValue)
    NS_Free(oldValue);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsErrorService, nsIErrorService)

nsresult
nsErrorService::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (NS_WARN_IF(outer))
        return NS_ERROR_NO_AGGREGATION;
    nsRefPtr<nsErrorService> serv = new nsErrorService();
    return serv->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsErrorService::RegisterErrorStringBundle(int16_t errorModule, const char *stringBundleURL)
{
    return mErrorStringBundleURLMap.Put(errorModule, stringBundleURL);
}

NS_IMETHODIMP
nsErrorService::UnregisterErrorStringBundle(int16_t errorModule)
{
    return mErrorStringBundleURLMap.Remove(errorModule);
}

NS_IMETHODIMP
nsErrorService::GetErrorStringBundle(int16_t errorModule, char **result)
{
    char* value = mErrorStringBundleURLMap.Get(errorModule);
    if (value == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = value;
    return NS_OK;
}

NS_IMETHODIMP
nsErrorService::RegisterErrorStringBundleKey(nsresult error, const char *stringBundleKey)
{
    return mErrorStringBundleKeyMap.Put(static_cast<uint32_t>(error),
                                        stringBundleKey);
}

NS_IMETHODIMP
nsErrorService::UnregisterErrorStringBundleKey(nsresult error)
{
    return mErrorStringBundleKeyMap.Remove(static_cast<uint32_t>(error));
}

NS_IMETHODIMP
nsErrorService::GetErrorStringBundleKey(nsresult error, char **result)
{
    char* value = mErrorStringBundleKeyMap.Get(static_cast<uint32_t>(error));
    if (value == nullptr)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = value;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
