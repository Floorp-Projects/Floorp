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

#ifdef MOZ_NEW_CACHE

#include "prlog.h"
#if defined(PR_LOGGING)
extern PRLogModuleInfo *gImgLog;
#else
#define gImgLog
#endif

#include "imgRequest.h"

#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

#include "nsICache.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsICacheEntryDescriptor.h"

#endif /* MOZ_NEW_CACHE */

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

#ifdef MOZ_NEW_CACHE

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
                              nsICache::NOT_STREAM_BASED,
                              PR_FALSE, getter_AddRefs(newSession));

  if (!newSession) {
    NS_WARNING("Unable to create a cache session");
    return;
  }

  if (isChrome)
    gChromeSession = newSession;
  else
    gSession = newSession;

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
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("imgCache::Put\n"));

  nsresult rv;

  nsCOMPtr<nsICacheSession> ses;
  GetCacheSession(aKey, getter_AddRefs(ses));
  if (!ses) return PR_FALSE;

  nsXPIDLCString spec;
  aKey->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsICacheEntryDescriptor> entry;

  rv = ses->OpenCacheEntry(spec, nsICache::ACCESS_WRITE, getter_AddRefs(entry));

  if (NS_FAILED(rv) || !entry)
    return PR_FALSE;

  nsCOMPtr<nsISupports> sup(do_QueryInterface(NS_STATIC_CAST(imgIRequest*, request)));
  entry->SetCacheElement(sup);

  entry->MarkValid();

  *aEntry = entry;
  NS_ADDREF(*aEntry);

  return PR_TRUE;
}

PRBool imgCache::Get(nsIURI *aKey, imgRequest **aRequest, nsICacheEntryDescriptor **aEntry)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("imgCache::Get\n"));

  nsresult rv;

  nsCOMPtr<nsICacheSession> ses;
  GetCacheSession(aKey, getter_AddRefs(ses));
  if (!ses) return PR_FALSE;

  nsXPIDLCString spec;
  aKey->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsICacheEntryDescriptor> entry;

  rv = ses->OpenCacheEntry(spec, nsICache::ACCESS_READ, getter_AddRefs(entry));

  if (NS_FAILED(rv) || !entry)
    return PR_FALSE;

  nsCOMPtr<nsISupports> sup;
  entry->GetCacheElement(getter_AddRefs(sup));

  nsCOMPtr<imgIRequest> req(do_QueryInterface(sup));
  *aRequest = NS_REINTERPRET_CAST(imgRequest*, req.get());
  NS_IF_ADDREF(*aRequest);

  *aEntry = entry;
  NS_ADDREF(*aEntry);

  return PR_TRUE;
}


PRBool imgCache::Remove(nsIURI *aKey)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("imgCache::Remove\n"));

  nsresult rv;

  nsCOMPtr<nsICacheSession> ses;
  GetCacheSession(aKey, getter_AddRefs(ses));
  if (!ses) return PR_FALSE;

  nsXPIDLCString spec;
  aKey->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsICacheEntryDescriptor> entry;

  rv = ses->OpenCacheEntry(spec, nsICache::ACCESS_READ, getter_AddRefs(entry));

  if (NS_FAILED(rv) || !entry)
    return PR_FALSE;

  entry->Doom();

  return PR_TRUE;
}

#endif /* MOZ_NEW_CACHE */

