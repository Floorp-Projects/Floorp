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
#include "nsICacheVisitor.h"
#include "nsIObserverService.h"

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

nsCacheService *   nsCacheService::gService = nsnull;

NS_IMPL_THREADSAFE_ISUPPORTS2(nsCacheService, nsICacheService, nsIObserver)

nsCacheService::nsCacheService()
    : mCacheServiceLock(nsnull),
      mMemoryDevice(nsnull),
      mDiskDevice(nsnull),
      mTotalEntries(0),
      mCacheHits(0),
      mCacheMisses(0),
      mMaxKeyLength(0),
      mMaxDataSize(0),
      mMaxMetaSize(0),
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
#if DEBUG
    printf("### nsCacheService is now destroyed.\n");
#endif
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

    // observer XPCOM shutdown.
    {
        nsCOMPtr<nsIObserverService> observerService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            observerService->AddObserver(this, NS_LITERAL_STRING(NS_XPCOM_SHUTDOWN_OBSERVER_ID).get());
        }
    }
    
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
        // XXX this is not sufficient
        PRLock * tempLock = mCacheServiceLock;
        mCacheServiceLock = nsnull;
#if DEBUG
        printf("### beging nsCacheService::Shutdown()\n");
#endif

        // Clear entries
        ClearDoomList();
        ClearActiveEntries();

        // deallocate memory and disk caches
        delete mMemoryDevice;
        mMemoryDevice = nsnull;

        delete mDiskDevice;
        mDiskDevice = nsnull;

        PR_DestroyLock(tempLock);
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
    nsAutoLock lock(mCacheServiceLock);
     
    // XXX record the fact that a visitation is in progress, i.e. keep
    // list of visitors in progress.
    
    nsresult rv = mMemoryDevice->Visit(visitor);
    if (NS_FAILED(rv)) return rv;

    rv = mDiskDevice->Visit(visitor);
    if (NS_FAILED(rv)) return rv;
    
    // XXX notify any shutdown process that visitation is complete for THIS visitor.

    return NS_OK;
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

    if (mMaxKeyLength < key->Length()) mMaxKeyLength = key->Length();

    // create request
    *request = new  nsCacheRequest(key, listener, accessRequested, session);    
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

    return listenerProxy->OnCacheEntryAvailable(descriptor, accessGranted, error);
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
        nsresult rv2 = NotifyListener(request, descriptor, accessGranted, rv);
        if (NS_FAILED(rv2) && NS_SUCCEEDED(rv)) {
            rv = rv2;  // trigger delete request
        }
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
    if (result)
        *result = nsnull;

    nsCacheRequest * request = nsnull;

    nsresult rv = CreateRequest(session, key, accessRequested, listener, &request);
    if (NS_FAILED(rv))  return rv;

    nsAutoLock lock(mCacheServiceLock);
    rv = ProcessRequest(request, result);

    // delete requests that have completed
    if (!(listener && (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION)))
        delete request;

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
        entry = SearchCacheDevices(request->mKey, request->StoragePolicy());
        if (entry)  entry->MarkInitialized();
    }

    if (entry)  ++mCacheHits;
    else        ++mCacheMisses;

    if (!entry && !(request->AccessRequested() & nsICache::ACCESS_WRITE)) {
        // this is a READ-ONLY request
        rv = NS_ERROR_CACHE_KEY_NOT_FOUND;
        goto error;
    }

    if (entry &&
        ((request->AccessRequested() == nsICache::ACCESS_WRITE) ||
         (entry->mExpirationTime &&
          entry->mExpirationTime < ConvertPRTimeToSeconds(PR_Now()) &&
          request->WillDoomEntriesIfExpired())))
    {
        // this is FORCE-WRITE request or the entry has expired
        rv = DoomEntry_Locked(entry);
        if (NS_FAILED(rv)) {
            // XXX what to do?  Increment FailedDooms counter?
        }
        entry = nsnull;
    }

    if (!entry) {
        entry = new nsCacheEntry(request->mKey,
                                 request->IsStreamBased(),
                                 request->StoragePolicy());
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;
        
        ++mTotalEntries;
        
        // XXX  we could perform an early bind in some cases based on storage policy
    }

    if (!entry->IsActive()) {
        rv = mActiveEntries.AddEntry(entry);
        if (NS_FAILED(rv)) goto error;
        entry->MarkActive();  // mark entry active, because it's now in mActiveEntries
    }
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
    nsAutoLock lock(mCacheServiceLock);
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
    return DoomEntry_Locked(entry);
}


nsresult
nsCacheService::DoomEntry_Locked(nsCacheEntry * entry)
{
    if (entry->IsDoomed())  return NS_OK;

    nsresult  rv = NS_OK;
    entry->MarkDoomed();

    nsCacheDevice * device = entry->CacheDevice();
    if (device)  device->DoomEntry(entry);

    if (entry->IsActive()) {
        // remove from active entries
        mActiveEntries.RemoveEntry(entry);
        entry->MarkInactive();
     }

    // put on doom list to wait for descriptors to close
    NS_ASSERTION(PR_CLIST_IS_EMPTY(entry), "doomed entry still on device list");
    PR_APPEND_LINK(entry, &mDoomedEntries);

    // tell pending requests to get on with their lives...
    rv = ProcessPendingRequests(entry);
    
    // All requests have been removed, but there may still be open descriptors
    if (entry->IsNotInUse()) {
        DeactivateEntry(entry); // tell device to get rid of it
    }
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


nsresult        
nsCacheService::GetFileForEntry(nsCacheEntry *         entry,
                                nsIFile **             result)
{
    nsAutoLock lock(mCacheServiceLock);
    nsCacheDevice * device = EnsureEntryHasDevice(entry);
    if (!device)  return  NS_ERROR_UNEXPECTED; // XXX need better error here
    return device->GetFileForEntry(entry, result);
}

void
nsCacheService::DeactivateEntry(nsCacheEntry * entry)
{
    nsresult  rv = NS_OK;
    NS_ASSERTION(entry->IsNotInUse(), "### deactivating an entry while in use!");

    if (mMaxDataSize < entry->DataSize() )     mMaxDataSize = entry->DataSize();
    if (mMaxMetaSize < entry->MetaDataSize() ) mMaxMetaSize = entry->MetaDataSize();

    if (entry->IsDoomed()) {
        // remove from Doomed list
        PR_REMOVE_AND_INIT_LINK(entry);
    } else {
        if (entry->IsActive()) {
            // remove from active entries
            mActiveEntries.RemoveEntry(entry);
            entry->MarkInactive();
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
                else
                    delete request;

                if (NS_FAILED(rv)) {
                    // XXX what to do?
                }
            } else if (entry->IsValid()) {
                rv = entry->RequestAccess(request, &accessGranted);
                NS_ASSERTION(NS_SUCCEEDED(rv),
                             "if entry is valid, RequestAccess must succeed.");

                // entry->CreateDescriptor dequeues request, and queues descriptor
                nsCOMPtr<nsICacheEntryDescriptor> descriptor;
                rv = entry->CreateDescriptor(request,
                                             accessGranted,
                                             getter_AddRefs(descriptor));
                
                // post call to listener to report error or descriptor
                rv = NotifyListener(request, descriptor, accessGranted, rv);
                delete request;
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


void
nsCacheService::ClearPendingRequests(nsCacheEntry * entry)
{
    nsCacheRequest * request = (nsCacheRequest *)PR_LIST_HEAD(&entry->mRequestQ);
    
    while (request != &entry->mRequestQ) {
        nsCacheRequest * next = (nsCacheRequest *)PR_NEXT_LINK(request);

        // XXX we're just dropping these on the floor for now...definitely wrong.
        PR_REMOVE_AND_INIT_LINK(request);
        delete request;
        request = next;
    }
}


void
nsCacheService::ClearDoomList()
{
    nsCacheEntry * entry = (nsCacheEntry *)PR_LIST_HEAD(&mDoomedEntries);

    while (entry != &mDoomedEntries) {
        nsCacheEntry * next = (nsCacheEntry *)PR_NEXT_LINK(entry);
        
         entry->DetachDescriptors();
         DeactivateEntry(entry);
         entry = next;
    }        
}


void
nsCacheService::ClearActiveEntries()
{
    // XXX really we want a different finalize callback for mActiveEntries
    PL_DHashTableEnumerate(&mActiveEntries.table, DeactiveateAndClearEntry, nsnull);
}


PLDHashOperator
nsCacheService::DeactiveateAndClearEntry(PLDHashTable *    table,
                                         PLDHashEntryHdr * hdr,
                                         PRUint32          number,
                                         void *            arg)
{
    nsCacheEntry * entry = ((nsCacheEntryHashTableEntry *)hdr)->cacheEntry;
    NS_ASSERTION(entry, "### active entry = nsnull!");
    gService->ClearPendingRequests(entry);
    entry->DetachDescriptors();
    gService->DeactivateEntry(entry);
    
    ((nsCacheEntryHashTableEntry *)hdr)->keyHash    = 1; // mark removed
    ((nsCacheEntryHashTableEntry *)hdr)->cacheEntry = nsnull;
    return PL_DHASH_NEXT;
}


NS_IMETHODIMP nsCacheService::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
    return Shutdown();
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
