/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "imgCache.h"

#include "ImageLogging.h"

#include "imgRequest.h"

#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

#include "nsICache.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsICacheEntryDescriptor.h"

NS_IMPL_ISUPPORTS1(imgCache, imgICache)

imgCache::imgCache()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

imgCache::~imgCache()
{
  /* destructor code */
}

/* void clearCache (in boolean chrome); */
NS_IMETHODIMP imgCache::ClearCache(PRBool chrome)
{
  if (chrome)
    return imgCache::ClearChromeImageCache();
  else
    return imgCache::ClearImageCache();
}


/* void removeEntry(in nsIURI uri); */
NS_IMETHODIMP imgCache::RemoveEntry(nsIURI *uri)
{
  if (imgCache::Remove(uri))
    return NS_OK;

  return NS_ERROR_NOT_AVAILABLE;
}

static nsCOMPtr<nsICacheSession> gSession = nsnull;
static nsCOMPtr<nsICacheSession> gChromeSession = nsnull;

void GetCacheSession(nsIURI *aURI, nsICacheSession **_retval)
{
  NS_ASSERTION(aURI, "Null URI!");

  PRBool isChrome = PR_FALSE;
  aURI->SchemeIs("chrome", &isChrome);

  if (gSession && !isChrome) {
    *_retval = gSession;
    NS_ADDREF(*_retval);
    return;
  }

  if (gChromeSession && isChrome) {
    *_retval = gChromeSession;
    NS_ADDREF(*_retval);
    return;
  }

  nsCOMPtr<nsICacheService> cacheService(do_GetService("@mozilla.org/network/cache-service;1"));
  if (!cacheService) {
    NS_WARNING("Unable to get the cache service");
    return;
  }

  nsCOMPtr<nsICacheSession> newSession;
  cacheService->CreateSession(isChrome ? "image-chrome" : "image",
                              nsICache::STORE_IN_MEMORY,
                              nsICache::NOT_STREAM_BASED,
                              getter_AddRefs(newSession));

  if (!newSession) {
    NS_WARNING("Unable to create a cache session");
    return;
  }

  if (isChrome)
    gChromeSession = newSession;
  else {
    gSession = newSession;
    gSession->SetDoomEntriesIfExpired(PR_FALSE);
  }

  *_retval = newSession;
  NS_ADDREF(*_retval);
}


void imgCache::Shutdown()
{
  gSession = nsnull;
  gChromeSession = nsnull;
}


nsresult imgCache::ClearChromeImageCache()
{
  if (!gChromeSession)
    return NS_OK;

  return gChromeSession->EvictEntries();
}

nsresult imgCache::ClearImageCache()
{
  if (!gSession)
    return NS_OK;

  return gSession->EvictEntries();
}



PRBool imgCache::Put(nsIURI *aKey, imgRequest *request, nsICacheEntryDescriptor **aEntry)
{
  LOG_STATIC_FUNC(gImgLog, "imgCache::Put");

  nsresult rv;

  nsCOMPtr<nsICacheSession> ses;
  GetCacheSession(aKey, getter_AddRefs(ses));
  if (!ses) return PR_FALSE;

  nsXPIDLCString spec;
  aKey->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsICacheEntryDescriptor> entry;

  rv = ses->OpenCacheEntry(spec, nsICache::ACCESS_WRITE, nsICache::BLOCKING, getter_AddRefs(entry));

  if (NS_FAILED(rv) || !entry)
    return PR_FALSE;

  nsCOMPtr<nsISupports> sup = NS_REINTERPRET_CAST(nsISupports*, request);
  entry->SetCacheElement(sup);

  entry->MarkValid();

  *aEntry = entry;
  NS_ADDREF(*aEntry);

  return PR_TRUE;
}

static PRUint32
SecondsFromPRTime(PRTime prTime)
{
  PRInt64 microSecondsPerSecond, intermediateResult;
  PRUint32 seconds;
  
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_DIV(intermediateResult, prTime, microSecondsPerSecond);
  LL_L2UI(seconds, intermediateResult);
  return seconds;
}


PRBool imgCache::Get(nsIURI *aKey, PRBool aDoomIfExpired, imgRequest **aRequest, nsICacheEntryDescriptor **aEntry)
{
  LOG_STATIC_FUNC(gImgLog, "imgCache::Get");

  nsresult rv;

  nsCOMPtr<nsICacheSession> ses;
  GetCacheSession(aKey, getter_AddRefs(ses));
  if (!ses) return PR_FALSE;

  nsXPIDLCString spec;
  aKey->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsICacheEntryDescriptor> entry;

  rv = ses->OpenCacheEntry(spec, nsICache::ACCESS_READ, nsICache::BLOCKING, getter_AddRefs(entry));

  if (NS_FAILED(rv) || !entry)
    return PR_FALSE;

  if (aDoomIfExpired) {
    PRUint32 expirationTime;
    entry->GetExpirationTime(&expirationTime);
    if (expirationTime && (expirationTime <= SecondsFromPRTime(PR_Now()))) {
      entry->Doom();
      return PR_FALSE;
    }
  }

  nsCOMPtr<nsISupports> sup;
  entry->GetCacheElement(getter_AddRefs(sup));

  *aRequest = NS_REINTERPRET_CAST(imgRequest*, sup.get());
  NS_IF_ADDREF(*aRequest);

  *aEntry = entry;
  NS_ADDREF(*aEntry);

  return PR_TRUE;
}


PRBool imgCache::Remove(nsIURI *aKey)
{
  LOG_STATIC_FUNC(gImgLog, "imgCache::Remove");
  if (!aKey) return PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsICacheSession> ses;
  GetCacheSession(aKey, getter_AddRefs(ses));
  if (!ses) return PR_FALSE;

  nsXPIDLCString spec;
  aKey->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsICacheEntryDescriptor> entry;

  rv = ses->OpenCacheEntry(spec, nsICache::ACCESS_READ, nsICache::BLOCKING, getter_AddRefs(entry));

  if (NS_FAILED(rv) || !entry)
    return PR_FALSE;

  entry->Doom();

  return PR_TRUE;
}

