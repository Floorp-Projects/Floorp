/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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

#include "nsErrorService.h"
#include "nsCRT.h"

static void* PR_CALLBACK
CloneCString(nsHashKey *aKey, void *aData, void* closure)
{
  return nsCRT::strdup((const char*)aData);
}

static PRBool PR_CALLBACK
DeleteCString(nsHashKey *aKey, void *aData, void* closure)
{
  nsCRT::free((char*)aData);
  return PR_TRUE;
}

nsInt2StrHashtable::nsInt2StrHashtable()
    : mHashtable(CloneCString, nsnull, DeleteCString, nsnull, 16)
{
}

nsInt2StrHashtable::~nsInt2StrHashtable()
{
}

nsresult
nsInt2StrHashtable::Put(PRUint32 key, const char* aData)
{
  char* value = nsCRT::strdup(aData);
  if (value == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  nsVoidKey k((void*)key);
  char* oldValue = (char*)mHashtable.Put(&k, value);
  if (oldValue)
    nsCRT::free(oldValue);
  return NS_OK;
}

char* 
nsInt2StrHashtable::Get(PRUint32 key)
{
  nsVoidKey k((void*)key);
  const char* value = (const char*)mHashtable.Get(&k);
  if (value == nsnull)
    return nsnull;
  return nsCRT::strdup(value);
}

nsresult
nsInt2StrHashtable::Remove(PRUint32 key)
{
  nsVoidKey k((void*)key);
  char* oldValue = (char*)mHashtable.Remove(&k);
  if (oldValue)
    nsCRT::free(oldValue);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsErrorService, nsIErrorService)

NS_METHOD
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
