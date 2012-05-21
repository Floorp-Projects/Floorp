/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsErrorService.h"
#include "nsCRT.h"

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
    : mHashtable(CloneCString, nsnull, DeleteCString, nsnull, 16)
{
}

nsresult
nsInt2StrHashtable::Put(PRUint32 key, const char* aData)
{
  char* value = NS_strdup(aData);
  if (value == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  nsPRUint32Key k(key);
  char* oldValue = (char*)mHashtable.Put(&k, value);
  if (oldValue)
    NS_Free(oldValue);
  return NS_OK;
}

char* 
nsInt2StrHashtable::Get(PRUint32 key)
{
  nsPRUint32Key k(key);
  const char* value = (const char*)mHashtable.Get(&k);
  if (value == nsnull)
    return nsnull;
  return NS_strdup(value);
}

nsresult
nsInt2StrHashtable::Remove(PRUint32 key)
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
    NS_ENSURE_NO_AGGREGATION(outer);
    nsErrorService* serv = new nsErrorService();
    if (serv == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(serv);
    nsresult rv = serv->QueryInterface(aIID, aInstancePtr);
    NS_RELEASE(serv);
    return rv;
}

NS_IMETHODIMP
nsErrorService::RegisterErrorStringBundle(PRInt16 errorModule, const char *stringBundleURL)
{
    return mErrorStringBundleURLMap.Put(errorModule, stringBundleURL);
}

NS_IMETHODIMP
nsErrorService::UnregisterErrorStringBundle(PRInt16 errorModule)
{
    return mErrorStringBundleURLMap.Remove(errorModule);
}

NS_IMETHODIMP
nsErrorService::GetErrorStringBundle(PRInt16 errorModule, char **result)
{
    char* value = mErrorStringBundleURLMap.Get(errorModule);
    if (value == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = value;
    return NS_OK;
}

NS_IMETHODIMP
nsErrorService::RegisterErrorStringBundleKey(nsresult error, const char *stringBundleKey)
{
    return mErrorStringBundleKeyMap.Put(error, stringBundleKey);
}

NS_IMETHODIMP
nsErrorService::UnregisterErrorStringBundleKey(nsresult error)
{
    return mErrorStringBundleKeyMap.Remove(error);
}

NS_IMETHODIMP
nsErrorService::GetErrorStringBundleKey(nsresult error, char **result)
{
    char* value = mErrorStringBundleKeyMap.Get(error);
    if (value == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = value;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
