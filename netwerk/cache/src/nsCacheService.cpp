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
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsProxiedService.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

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
        // XXX check for pending requests...

        // XXX finalize active entries

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

    if (!listener)  return NS_OK;  // we're sync, we're done.

    // get the nsIEventQueue for the request's thread
    nsresult  rv;
    // XXX can we just keep a reference so we don't have to do this everytime?
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv))  goto error;
    
    rv = eventQService->ResolveEventQueue(NS_CURRENT_EVENTQ,
                                          getter_AddRefs((*request)->mEventQ));
    if (NS_FAILED(rv))  goto error;
    
    
    if (!(*request)->mEventQ) {
        rv = NS_ERROR_UNEXPECTED; // XXX what is the right error?
        goto error;
    }

    return NS_OK;

 error:
    delete *request;
    *request = nsnull;
    return rv;
}


nsresult
nsCacheService::NotifyListener(nsCacheRequest *          request,
                               nsICacheEntryDescriptor * descriptor,
                               nsCacheAccessMode         accessGranted,
                               nsresult                  error)
{
    nsresult rv;

    // XXX can we hold onto the proxy object manager?
    NS_WITH_SERVICE(nsIProxyObjectManager, proxyObjMgr, kProxyObjectManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsICacheListener> listenerProxy;
    NS_ASSERTION(request->mEventQ, "no event queue for async request!");
    rv = proxyObjMgr->GetProxyForObject(request->mEventQ,
                                        NS_GET_IID(nsICacheListener),
                                        request->mListener,
                                        PROXY_ASYNC|PROXY_ALWAYS,
                                        getter_AddRefs(listenerProxy));
    if (NS_FAILED(rv)) return rv;

    return listenerProxy->OnDescriptorAvailable(descriptor, accessGranted, error);
}


nsresult
nsCacheService::ProcessRequest(nsCacheRequest * request,
                               nsICacheEntryDescriptor ** result)
{
    // !!! must be called with mCacheServiceLock held !!!
    nsresult           rv;
    nsCacheEntry *     entry = nsnull;
    nsCacheAccessMode  accessGranted = nsICache::ACCESS_NONE;
    if (result) *result = nsnull;

    while(1) {  // Activate entry loop
        rv = ActivateEntry(request, &entry);  // get the entry for this request
        if (NS_FAILED(rv))  break;

        while(1) { // Request Access loop
            NS_ASSERTION(entry, "no entry in Request Access loop!");
            // entry->RequestAccess queues request on entry
            rv = entry->RequestAccess(request, &accessGranted);
            if (rv != NS_ERROR_CACHE_WAIT_FOR_VALIDATION) break;
            
            if (request->mListener) // async exits - validate, doom, or close will resume
                return rv; 
            
            // XXX allocate condvar for request if necessary
            PR_Unlock(mCacheServiceLock);
            rv = request->WaitForValidation();
            PR_Lock(mCacheServiceLock);

            PR_REMOVE_AND_INIT_LINK(request);
            if (NS_FAILED(rv)) break;
            // okay, we're ready to process this request, request access again
        }
        if (rv != NS_ERROR_CACHE_ENTRY_DOOMED)  break;

        if (entry->IsNotInUse()) {
            // this request was the last one keeping it around, so get rid of it
            DeactivateEntry(entry);
        }
        // loop back around to look for another entry
    }

    nsCOMPtr<nsICacheEntryDescriptor> descriptor;
    
    if (NS_SUCCEEDED(rv))
        rv = entry->CreateDescriptor(request, accessGranted, getter_AddRefs(descriptor));

    if (request->mListener) {  // Asynchronous
        // call listener to report error or descriptor
        rv = NotifyListener(request, descriptor, accessGranted, rv);
    } else {        // Synchronous
        NS_IF_ADDREF(*result = descriptor);
    }
    return rv;
}


nsresult
nsCacheService::OpenCacheEntry(nsCacheSession *           session,
                                const char *               key,
                                nsCacheAccessMode          accessRequested,
                                nsICacheListener *         listener,
                                nsICacheEntryDescriptor ** result)
{
    if (*result)
        *result = nsnull;

    nsCacheRequest * request = nsnull;

    nsresult rv = CreateRequest(session, key, accessRequested, listener, &request);
    if (NS_FAILED(rv))  return rv;

    nsAutoLock cacheService(mCacheServiceLock);
    rv = ProcessRequest(request, result);

    // delete requests that have completed
    if (!listener || (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION))
        delete request;

    return rv;
}

#if 0
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
        // XXX acquire lock
        rv = ActivateEntry(request, &entry);
        if (NS_FAILED(rv)) break;
        // XXX check for NS_ERROR_CACHE_KEY_NOT_FOUND & READ-ONLY request

        rv = entry->Open(request, result); // XXX release lock before waiting on request
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

    // XXX acquire service lock PR_Lock(mCacheServiceLock);
    rv = ActivateEntry(request, &entry);
    if (NS_SUCCEEDED(rv)) {
        entry->AsyncOpen(request); // XXX release lock after request queued, etc.
    }

   return rv;
}
#endif

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
        entry = SearchCacheDevices(request->mKey, request->StoragePolicy());
        if (entry)  entry->MarkInitialized();
    }

    if (!entry && !(request->AccessRequested() & nsICache::ACCESS_WRITE)) {
        // this is a READ-ONLY request
        rv = NS_ERROR_CACHE_KEY_NOT_FOUND;
        goto error;
    }

    if (entry &&
        ((request->AccessRequested() == nsICache::ACCESS_WRITE) ||
         (entry->mExpirationTime &&
          entry->mExpirationTime < ConvertPRTimeToSeconds(PR_Now()))))
        // XXX beginning to look a lot like lisp
    {
        // this is FORCE-WRITE request or the entry has expired
        rv = DoomEntry_Internal(entry);
        if (NS_FAILED(rv)) {
            // XXX what to do?  Increment FailedDooms counter?
        }

        if (entry->IsNotInUse()) {
            DeactivateEntry(entry); // tell device to get rid of it
        }
        entry = nsnull;
    }

    if (!entry) {
        entry = new nsCacheEntry(request->mKey,
                                 request->IsStreamBased(),
                                 request->StoragePolicy());
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;
        
        // XXX  we could perform an early bind in some cases based on storage policy
    }

    rv = mActiveEntries.AddEntry(entry);
    if (NS_FAILED(rv)) goto error;
    entry->MarkActive();  // mark entry active, because it's now in mActiveEntries
    
    *result = entry;
    return NS_OK;
    
 error:
    *result = nsnull;
    if (entry) {
        // XXX clean up
        delete entry;
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


nsCacheDevice *
nsCacheService::EnsureEntryHasDevice(nsCacheEntry * entry)
{
    nsCacheDevice * device = entry->CacheDevice();
    if (device)  return device;

    if (entry->IsStreamData() && entry->IsAllowedOnDisk()) {
        // this is the default
        device = mDiskDevice;
    } else {
        NS_ASSERTION(entry->IsAllowedInMemory(), "oops.. bad flags");
        device = mMemoryDevice;
    }

    nsresult  rv = device->BindEntry(entry);
    if (NS_FAILED(rv)) return nsnull;

    entry->SetCacheDevice(device);
    return device;
}


nsresult
nsCacheService::ValidateEntry(nsCacheEntry * entry)
{
    nsCacheDevice * device = EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED; // XXX need better error here

    entry->MarkValid();
    nsresult rv = ProcessPendingRequests(entry);
    NS_ASSERTION(rv == NS_OK, "ProcessPendingRequests failed.");
    // XXX what else can be done?

    return rv;
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
        // XXX check rv, but what can we really do...
    }

    if (entry->IsActive()) {
        // remove from active entries
        rv = mActiveEntries.RemoveEntry(entry);
        entry->MarkInactive();
        if (NS_FAILED(rv)) {
            // XXX what to do
        }
    }

    // XXX check if there are descriptors still queued
    // put on doom list to wait for descriptors to close
    NS_ASSERTION(PR_CLIST_IS_EMPTY(entry),
                 "doomed entry still on device list");

    PR_APPEND_LINK(entry, &mDoomedEntries);

    // XXX reprocess pending requests
    rv = ProcessPendingRequests(entry);
    return rv;
}


nsresult
nsCacheService::OnDataSizeChange(nsCacheEntry * entry, PRInt32 deltaSize)
{
    nsAutoLock lock(mCacheServiceLock);

    nsCacheDevice * device = EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED; // XXX need better error here

    return device->OnDataSizeChange(entry, deltaSize);
}


nsresult
nsCacheService::GetTransportForEntry(nsCacheEntry *     entry,
                                     nsCacheAccessMode  mode,
                                     nsITransport    ** result)
{
    nsAutoLock lock(mCacheServiceLock);

    nsCacheDevice * device = EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED; // XXX need better error here

    return device->GetTransportForEntry(entry, mode, result);
}


void
nsCacheService::CloseDescriptor(nsCacheEntryDescriptor * descriptor)
{
    nsAutoLock lock(mCacheServiceLock);

    // ask entry to remove descriptor
    nsCacheEntry * entry       = descriptor->CacheEntry();
    PRBool         stillActive = entry->RemoveDescriptor(descriptor);
    nsresult       rv          = NS_OK;

    if (!entry->IsValid()) {
        if (PR_CLIST_IS_EMPTY(&entry->mRequestQ)) {
            rv = ProcessPendingRequests(entry);
        }
    }

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
        PR_REMOVE_AND_INIT_LINK(entry);
    } else {
        if (entry->IsActive()) {
            // remove from active entries
            rv = mActiveEntries.RemoveEntry(entry);
            entry->MarkInactive();
            NS_ASSERTION(NS_SUCCEEDED(rv),"failed to remove an active entry !?!");
        } else {
            // XXX bad state
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


nsresult
nsCacheService::ProcessPendingRequests(nsCacheEntry * entry)
{
    nsresult  rv = NS_OK;

    if (!entry->IsDoomed() && entry->IsInvalid()) {
        // 1st descriptor closed w/o MarkValid()
        // XXX assert no open descriptors
        // XXX find best requests to promote to 1st Writer
        // XXX process ACCESS_WRITE (shouldn't have any of these)
        // XXX ACCESS_READ_WRITE, ACCESS_READ
        NS_ASSERTION(PR_TRUE,
                     "closing descriptors w/o calling MarkValid is not supported yet.");
    }

    nsCacheRequest *   request = (nsCacheRequest *)PR_LIST_HEAD(&entry->mRequestQ);
    nsCacheRequest *   nextRequest;
    nsCacheAccessMode  accessGranted = nsICache::ACCESS_NONE;

    while (request != &entry->mRequestQ) {
        nextRequest = (nsCacheRequest *)PR_NEXT_LINK(request);

        if (request->mListener) {

            // Async request
            PR_REMOVE_AND_INIT_LINK(request);

            if (entry->IsDoomed()) {
                rv = ProcessRequest(request, nsnull);
                if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION)
                    rv = NS_OK;

                if (NS_FAILED(rv)) {
                    // XXX what to do?
                }
            } else if (entry->IsValid()) {
                rv = entry->RequestAccess(request, &accessGranted);
                NS_ASSERTION(NS_SUCCEEDED(rv),
                             "if entry is valid, RequestAccess must succeed.");

                // entry->CreateDescriptor dequeues request, and queues descriptor
                nsCOMPtr<nsICacheEntryDescriptor> descriptor;
                rv = entry->CreateDescriptor(request, accessGranted, getter_AddRefs(descriptor));
                
                // post call to listener to report error or descriptor
                rv = NotifyListener(request, descriptor, accessGranted, rv);
                if (NS_FAILED(rv)) {
                    // XXX what to do?
                }
                
            } else {
                // XXX bad state
            }
        } else {

            // Synchronous request
            request->WakeUp();
        }
        request = nextRequest;
    }

    return NS_OK;
}


/**
 * Cache Service Utility Functions
 */

// time conversion utils from nsCachedNetData.cpp
// Convert PRTime to unix-style time_t, i.e. seconds since the epoch
PRUint32
ConvertPRTimeToSeconds(PRTime time64)
{
    double fpTime;
    LL_L2D(fpTime, time64);
    return (PRUint32)(fpTime * 1e-6 + 0.5);
}


// Convert unix-style time_t, i.e. seconds since the epoch, to PRTime
PRTime
ConvertSecondsToPRTime(PRUint32 seconds)
{
    PRInt64 t64;
    LL_I2L(t64, seconds);
    PRInt64 mil;
    LL_I2L(mil, 1000000);
    LL_MUL(t64, t64, mil);
    return t64;
}
