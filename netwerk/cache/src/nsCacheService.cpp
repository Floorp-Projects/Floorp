/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is nsCacheService.cpp, released February 10, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 10-February-2001
 */


#include "nsCacheService.h"
#include "nsCacheEntry.h"
#include "nsCacheEntryDescriptor.h"
#include "nsCacheDevice.h"
#include "nsCacheRequest.h"
#include "nsMemoryCacheDevice.h"
#include "nsDiskCacheDevice.h"
#include "nsAutoLock.h"
#include "nsVoidArray.h"

nsCacheService *   nsCacheService::gService = nsnull;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheService, nsICacheService)

nsCacheService::nsCacheService()
    : mCacheServiceLock(nsnull),
      mMemoryDevice(nsnull),
      mDiskDevice(nsnull),
      mDeactivateFailures(0),
      mDeactivatedUnboundEntries(0)
{
  NS_INIT_REFCNT();

  NS_ASSERTION(gService==nsnull, "multiple nsCacheService instances!");
  gService = this;

  // create list of cache devices
  PR_INIT_CLIST(&mDoomedEntries);
}

nsCacheService::~nsCacheService()
{
    if (mCacheServiceLock) // Shutdown hasn't been called yet.
        (void) Shutdown();

    gService = nsnull;
}


NS_IMETHODIMP
nsCacheService::Init()
{
    nsresult  rv;

    NS_ASSERTION(mCacheServiceLock== nsnull, "nsCacheService already initialized.");
    if (mCacheServiceLock)
        return NS_ERROR_ALREADY_INITIALIZED;

    mCacheServiceLock = PR_NewLock();
    if (mCacheServiceLock == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    // initialize hashtable for active cache entries
    rv = mActiveEntries.Init();
    if (NS_FAILED(rv)) goto error;

    // create memory cache
    mMemoryDevice = new nsMemoryCacheDevice;
    if (!mMemoryDevice) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto error;
    }
    rv = mMemoryDevice->Init();
    if (NS_FAILED(rv)) goto error;
    
    // create disk cache
    mDiskDevice = new nsDiskCacheDevice;
    if (!mDiskDevice) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto error;
    }
    rv = mDiskDevice->Init();
    if (NS_FAILED(rv)) goto error;

    return NS_OK;

 error:
    (void)Shutdown();

    return rv;
}


NS_IMETHODIMP
nsCacheService::Shutdown()
{
    NS_ASSERTION(mCacheServiceLock != nsnull, 
                 "can't shutdown nsCacheService unless it has been initialized.");

    if (mCacheServiceLock) {
        //** check for pending requests...

        //** finalize active entries

        // deallocate memory and disk caches
        delete mMemoryDevice;
        mMemoryDevice = nsnull;

        delete mDiskDevice;
        mDiskDevice = nsnull;

        PR_DestroyLock(mCacheServiceLock);
        mCacheServiceLock = nsnull;
    }
    return NS_OK;
}


NS_METHOD
nsCacheService::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsresult  rv;

    if (aOuter != nsnull)
        return NS_ERROR_NO_AGGREGATION;

    nsCacheService* cacheService = new nsCacheService();
    if (cacheService == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(cacheService);
    rv = cacheService->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = cacheService->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(cacheService);
    return rv;
}


NS_IMETHODIMP
nsCacheService::CreateSession(const char *          clientID,
                              nsCacheStoragePolicy  storagePolicy, 
                              PRBool                streamBased,
                              nsICacheSession     **result)
{
    *result = nsnull;

    nsCacheSession * session = new nsCacheSession(clientID, storagePolicy, streamBased);
    if (!session)  return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result = session);

    return NS_OK;
}


/* void visitEntries (in nsICacheVisitor visitor); */
NS_IMETHODIMP nsCacheService::VisitEntries(nsICacheVisitor *visitor)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult
nsCacheService::CreateRequest(nsCacheSession *   session,
                              const char *       clientKey,
                              nsCacheAccessMode  accessRequested,
                              nsICacheListener * listener,
                              nsCacheRequest **  request)
{
    NS_ASSERTION(request, "CommonOpenCacheEntry: request or entry is null");
     
    nsCString * key = new nsCString(*session->ClientID() +
                                    nsLiteralCString(":") +
                                    nsLiteralCString(clientKey));
    if (!key)
        return NS_ERROR_OUT_OF_MEMORY;
    
    // create request
    *request = new  nsCacheRequest(key, listener, accessRequested, 
                                   session->StreamBased(),
                                   session->StoragePolicy());
    
    if (!*request) {
        delete key;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}                                     


nsresult
nsCacheService::OpenCacheEntry(nsCacheSession *           session,
                               const char *               key, 
                               nsCacheAccessMode          accessRequested,
                               nsICacheEntryDescriptor ** result)
{
    *result = nsnull;
    if (!mCacheServiceLock)  return NS_ERROR_NOT_INITIALIZED;

    nsCacheRequest * request = nsnull;
    nsCacheEntry *   entry   = nsnull;

    nsresult rv = CreateRequest(session, key, accessRequested, nsnull, &request);
    if (NS_FAILED(rv))  return rv;

    while (1) {
        //** acquire lock
        rv = ActivateEntry(request, &entry);
        if (NS_FAILED(rv)) break;
        //** check for NS_ERROR_CACHE_KEY_NOT_FOUND & READ-ONLY request

        rv = entry->Open(request, result); //** release lock before waiting on request
        if (rv != NS_ERROR_CACHE_ENTRY_DOOMED) break;

    }

    delete request;
    return rv;
}


nsresult
nsCacheService::AsyncOpenCacheEntry(nsCacheSession *   session,
                                    const char *       key, 
                                    nsCacheAccessMode  accessRequested,
                                    nsICacheListener * listener)
{
    if (!mCacheServiceLock)  return NS_ERROR_NOT_INITIALIZED;
    if (!listener)           return NS_ERROR_NULL_POINTER;

    nsCacheRequest * request = nsnull;
    nsCacheEntry *   entry   = nsnull;

    nsresult rv = CreateRequest(session, key, accessRequested, listener, &request);

    //** acquire service lock PR_Lock(mCacheServiceLock);
    rv = ActivateEntry(request, &entry);
    if (NS_SUCCEEDED(rv)) {
        entry->AsyncOpen(request); //** release lock after request queued, etc.
    }

   return rv;
}


nsresult
nsCacheService::ActivateEntry(nsCacheRequest * request, 
                              nsCacheEntry ** result)
{
    nsresult        rv = NS_OK;

    NS_ASSERTION(request != nsnull, "ActivateEntry called with no request");
    if (result) *result = nsnull;
    if ((!request) || (!result))  return NS_ERROR_NULL_POINTER;


    // search active entries (including those not bound to device)
    nsCacheEntry *entry = mActiveEntries.GetEntry(request->mKey);

    if (!entry) {
        // search cache devices for entry
        entry = SearchCacheDevices(request->mKey, request->mStoragePolicy);
        entry->MarkInitialized();
    }

    if (!entry && !(request->mAccessRequested & nsICache::ACCESS_WRITE)) {
        // this is a READ-ONLY request
        rv = NS_ERROR_CACHE_KEY_NOT_FOUND;
        goto error;
    }

    if (entry && (request->mAccessRequested == nsICache::ACCESS_WRITE)) {
        // this is FORCE-WRITE request
        rv = DoomEntry_Internal(entry);
        if (NS_FAILED(rv)) {
            //** what to do?
        }

        if (entry->IsNotInUse()) {
            DeactivateEntry(entry); // tell device to get rid of it
        }
        entry = nsnull;
    }

    if (!entry) {
        entry = new nsCacheEntry(request->mKey,
                                 request->mStreamBased,
                                 request->mStoragePolicy);
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;
        
        //** we could perform an early bind in some cases based on storage policy
    }

    rv = mActiveEntries.AddEntry(entry);
    if (NS_FAILED(rv)) goto error;
    entry->MarkActive();  // mark entry active, because it's now in mActiveEntries
    
    *result = entry;
    return NS_OK;
    
 error:
    *result = nsnull;
    if (entry) {
        //** clean up
    }
    return rv;
}


nsCacheEntry *
nsCacheService::SearchCacheDevices(nsCString * key, nsCacheStoragePolicy policy)
{
    nsCacheEntry * entry = nsnull;

    if ((policy == nsICache::STORE_ANYWHERE) || (policy == nsICache::STORE_IN_MEMORY)) {
        entry = mMemoryDevice->FindEntry(key);
    }

    if (!entry && 
        ((policy == nsICache::STORE_ANYWHERE) || (policy == nsICache::STORE_ON_DISK))) {
        entry = mDiskDevice->FindEntry(key);
    }

    return entry;
}


nsresult
nsCacheService::BindEntry(nsCacheEntry * entry)
{
    nsresult  rv = NS_OK;
    nsCacheDevice * device;

    if (entry->IsStreamData() && entry->IsAllowedOnDisk()) {
        // this is the default
        device = mDiskDevice;
    } else {
        NS_ASSERTION(entry->IsAllowedInMemory(), "oops.. bad flags");
        device = mMemoryDevice;
    }

    rv = device->BindEntry(entry);
    if (NS_SUCCEEDED(rv))
        entry->SetCacheDevice(device);

    return rv;
}


nsresult
nsCacheService::ValidateEntry(nsCacheEntry * entry)
{
    //** bind if not bound
    //** convert pending requests to descriptors, etc.
    entry->MarkValid();
    return NS_OK;
}


nsresult
nsCacheService::DoomEntry(nsCacheEntry * entry)
{
    nsAutoLock lock(mCacheServiceLock);
    return DoomEntry_Internal(entry);
}


nsresult
nsCacheService::DoomEntry_Internal(nsCacheEntry * entry)
{
    if (entry->IsDoomed())  return NS_OK;

    nsresult  rv = NS_OK;
    entry->MarkDoomed();

    nsCacheDevice * device = entry->CacheDevice();
    if (device) {
        rv = device->DoomEntry(entry);
        //** check rv, but what can we really do...
    }

    if (entry->IsActive()) {
        // remove from active entries
        rv = mActiveEntries.RemoveEntry(entry);
        entry->MarkInactive();
        if (NS_FAILED(rv)) {
            //** what to do
        }
    }
    // put on doom list to wait for descriptors to close
    NS_ASSERTION(PR_CLIST_IS_EMPTY(entry->GetListNode()),
                 "doomed entry still on device list");

    PR_APPEND_LINK(entry->GetListNode(), &mDoomedEntries);
                 
    return rv;
}


nsresult
nsCacheService::OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize)
{
    nsAutoLock lock(mCacheServiceLock);
    nsresult   rv = NS_OK;

    nsCacheDevice * device = entry->CacheDevice();
    if (!device) {
        rv = BindEntry(entry);
        if (NS_FAILED(rv)) return rv;

        device = entry->CacheDevice();
    }
    return device->OnDataSizeChange(entry, deltaSize);
}


nsresult
nsCacheService::GetTransportForEntry(nsCacheEntry *     entry,
                                     nsCacheAccessMode  mode,
                                     nsITransport    ** result)
{
    nsAutoLock lock(mCacheServiceLock);
    nsresult   rv = NS_OK;

    nsCacheDevice * device = entry->CacheDevice();
    if (!device) {
        rv = BindEntry(entry);
        if (NS_FAILED(rv)) return rv;

        device = entry->CacheDevice();
    }
    return device->GetTransportForEntry(entry, mode, result);
}


void
nsCacheService::CloseDescriptor(nsCacheEntryDescriptor * descriptor)
{
    nsAutoLock lock(mCacheServiceLock);

    // ask entry to remove descriptor
    nsCacheEntry * entry       = descriptor->CacheEntry();
    PRBool         stillActive = entry->RemoveDescriptor(descriptor);
    
    if (!stillActive) {
        DeactivateEntry(entry);
    }
}


void
nsCacheService::DeactivateEntry(nsCacheEntry * entry)
{
    nsresult  rv = NS_OK;
    NS_ASSERTION(entry->IsNotInUse(), "deactivating an entry while in use!");

    if (entry->IsDoomed()) {
        // remove from Doomed list
        PR_REMOVE_AND_INIT_LINK(entry->GetListNode());
    } else {
        if (entry->IsActive()) {
            // remove from active entries
            rv = mActiveEntries.RemoveEntry(entry);
            entry->MarkInactive();
            NS_ASSERTION(NS_SUCCEEDED(rv),"failed to remove an active entry !?!");
        }
    }

    nsCacheDevice * device = entry->CacheDevice();
    if (device) {
        rv = device->DeactivateEntry(entry);
        if (NS_FAILED(rv)) {
            // increment deactivate failure count
            ++mDeactivateFailures;
        }
    } else {
        // increment deactivating unbound entry statistic
        ++mDeactivatedUnboundEntries;
        delete entry; // because no one else will
    }
}
