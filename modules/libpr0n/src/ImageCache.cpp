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

#include "ImageCache.h"

#ifdef MOZ_NEW_CACHE

#include "prlog.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo *gImgLog;
#else
#define gImgLog
#endif

#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

#include "nsICache.h"
#include "nsICacheService.h"
#include "nsICacheSession.h"
#include "nsICacheEntryDescriptor.h"

static nsCOMPtr<nsICacheSession> gSession = nsnull;

ImageCache::ImageCache()
{
  /* member initializers and constructor code */
}

ImageCache::~ImageCache()
{
  /* destructor code */
}

void GetCacheSession(nsICacheSession **_retval)
{
  if (!gSession) {
    nsCOMPtr<nsICacheService> cacheService(do_GetService("@mozilla.org/network/cache-service;1"));
    if (!cacheService) {
      NS_WARNING("Unable to get the cache service");
      return;
    }

    cacheService->CreateSession("images", nsICache::NOT_STREAM_BASED, PR_FALSE, getter_AddRefs(gSession));
    if (!gSession) {
      NS_WARNING("Unable to create a cache session");
      return;
    }
  }

  *_retval = gSession;
  NS_IF_ADDREF(*_retval);
}


void ImageCache::Shutdown()
{
  gSession = nsnull;
}

PRBool ImageCache::Put(nsIURI *aKey, imgRequest *request, nsICacheEntryDescriptor **aEntry)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("ImageCache::Put\n"));

  nsresult rv;

  nsCOMPtr<nsICacheSession> ses;
  GetCacheSession(getter_AddRefs(ses));

  if (!ses)
    return PR_FALSE;

  nsXPIDLCString spec;
  aKey->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsICacheEntryDescriptor> entry;

  rv = ses->OpenCacheEntry(spec, nsICache::ACCESS_WRITE, getter_AddRefs(entry));

  if (!entry || NS_FAILED(rv))
    return PR_FALSE;

  entry->SetCacheElement(NS_STATIC_CAST(nsISupports *, NS_STATIC_CAST(imgIRequest*, request)));

  entry->MarkValid();

  *aEntry = entry;
  NS_ADDREF(*aEntry);

  return PR_TRUE;
}

PRBool ImageCache::Get(nsIURI *aKey, imgRequest **aRequest, nsICacheEntryDescriptor **aEntry)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
       ("ImageCache::Get\n"));

  nsresult rv;

  nsCOMPtr<nsICacheSession> ses;
  GetCacheSession(getter_AddRefs(ses));

  if (!ses)
    return PR_FALSE;

  nsXPIDLCString spec;
  aKey->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsICacheEntryDescriptor> entry;

  rv = ses->OpenCacheEntry(spec, nsICache::ACCESS_READ, getter_AddRefs(entry));

  if (!entry || NS_FAILED(rv))
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


PRBool ImageCache::Remove(nsIURI *aKey)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("ImageCache::Remove\n"));

  nsresult rv;

  nsCOMPtr<nsICacheSession> ses;
  GetCacheSession(getter_AddRefs(ses));

  if (!ses)
    return PR_FALSE;

  nsXPIDLCString spec;
  aKey->GetSpec(getter_Copies(spec));

  nsCOMPtr<nsICacheEntryDescriptor> entry;

  rv = ses->OpenCacheEntry(spec, nsICache::ACCESS_READ, getter_AddRefs(entry));

  if (!entry || NS_FAILED(rv))
    return PR_FALSE;

  entry->Doom();

  return PR_TRUE;
}

#endif /* MOZ_NEW_CACHE */
