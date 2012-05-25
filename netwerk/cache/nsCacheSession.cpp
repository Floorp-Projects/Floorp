/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCacheSession.h"
#include "nsCacheService.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS1(nsCacheSession, nsICacheSession)

nsCacheSession::nsCacheSession(const char *         clientID,
                               nsCacheStoragePolicy storagePolicy,
                               bool                 streamBased)
    : mClientID(clientID),
      mInfo(0)
{
  SetStoragePolicy(storagePolicy);

  if (streamBased) MarkStreamBased();
  else SetStoragePolicy(nsICache::STORE_IN_MEMORY);

  MarkPublic();

  MarkDoomEntriesIfExpired();
}

nsCacheSession::~nsCacheSession()
{
  /* destructor code */
    // notify service we are going away?
}


NS_IMETHODIMP nsCacheSession::GetDoomEntriesIfExpired(bool *result)
{
    NS_ENSURE_ARG_POINTER(result);
    *result = WillDoomEntriesIfExpired();
    return NS_OK;
}


NS_IMETHODIMP nsCacheSession::SetDoomEntriesIfExpired(bool doomEntriesIfExpired)
{
    if (doomEntriesIfExpired)  MarkDoomEntriesIfExpired();
    else                       ClearDoomEntriesIfExpired();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheSession::OpenCacheEntry(const nsACString &         key, 
                               nsCacheAccessMode          accessRequested,
                               bool                       blockingMode,
                               nsICacheEntryDescriptor ** result)
{
    nsresult rv;
    rv =  nsCacheService::OpenCacheEntry(this,
                                         key,
                                         accessRequested,
                                         blockingMode,
                                         nsnull, // no listener
                                         result);
    return rv;
}


NS_IMETHODIMP nsCacheSession::AsyncOpenCacheEntry(const nsACString & key,
                                                  nsCacheAccessMode accessRequested,
                                                  nsICacheListener *listener,
                                                  bool              noWait)
{
    nsresult rv;
    rv = nsCacheService::OpenCacheEntry(this,
                                        key,
                                        accessRequested,
                                        !noWait,
                                        listener,
                                        nsnull); // no result

    if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) rv = NS_OK;
    return rv;
}

NS_IMETHODIMP nsCacheSession::EvictEntries()
{
    return nsCacheService::EvictEntriesForSession(this);
}


NS_IMETHODIMP nsCacheSession::IsStorageEnabled(bool *result)
{

    return nsCacheService::IsStorageEnabledForPolicy(StoragePolicy(), result);
}

NS_IMETHODIMP nsCacheSession::DoomEntry(const nsACString &key,
                                        nsICacheListener *listener)
{
    return nsCacheService::DoomEntry(this, key, listener);
}

NS_IMETHODIMP nsCacheSession::GetIsPrivate(bool* aPrivate)
{
    *aPrivate = IsPrivate();
    return NS_OK;
}

NS_IMETHODIMP nsCacheSession::SetIsPrivate(bool aPrivate)
{
    if (aPrivate)
        MarkPrivate();
    else
        MarkPublic();
    return NS_OK;
}
