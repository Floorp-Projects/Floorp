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
#include "nsCacheDevice.h"
#include "nsMemoryCacheDevice.h"
#include "nsAutoLock.h"
#include "nsVoidArray.h"

nsCacheService *   nsCacheService::gService = nsnull;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsCacheService, nsICacheService)

nsCacheService::nsCacheService()
    : mCacheServiceLock(nsnull)
{
  NS_INIT_REFCNT();

  NS_ASSERTION(gService==nsnull, "multiple nsCacheService instances!");
  gService = this;

  // create list of cache devices
  PR_INIT_CLIST(&mDeviceList);
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

    // initialize hashtable for client ids
    rv = mClientIDs.Init();
    NS_ASSERTION(NS_SUCCEEDED(rv), "mClientIDs.Init() failed");

    // initialize hashtable for active cache entries
    rv = mActiveEntries.Init();

    if (NS_SUCCEEDED(rv)) {
        // create memory cache
#if 0
        nsMemoryCacheDevice *device = new nsMemoryCacheDevice;
        // (void) mDeviceList.AppendElement(device);
      
        PRUint32  deviceID = device->GetDeviceID();
#endif

        if (NS_SUCCEEDED(rv)) {
            // create disk cache
        }
    }


    if (NS_FAILED(rv)) {
        // if mem cache - destroy it
        // if disk cache - destroy it

        PR_DestroyLock(mCacheServiceLock);
        mCacheServiceLock = nsnull;
        
        // finish mActiveEntries hashtable
    }

    return rv;
}


NS_IMETHODIMP
nsCacheService::Shutdown()
{
    NS_ASSERTION(mCacheServiceLock != nsnull, 
                 "can't shutdown nsCacheService unless it has been initialized.");

    if (mCacheServiceLock) {
        //** check for pending requests...

        //** finalize active entries and clientID

        //** deallocate memory and disk caches

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

/* nsICacheSession createSession (in string clientID, in long storagePolicy, in boolean streamBased); */
NS_IMETHODIMP nsCacheService::CreateSession(const char *clientID, PRInt32 storagePolicy, PRBool streamBased, nsICacheSession **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


/* void visitEntries (in nsICacheVisitor visitor); */
NS_IMETHODIMP nsCacheService::VisitEntries(nsICacheVisitor *visitor)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


nsresult
nsCacheService::CommonOpenCacheEntry(const char *clientID, const char *clientKey,
                                     PRUint32  accessRequested, PRBool  streamBased,
                                     nsCacheRequest **request, nsCacheEntry **entry)
{
    NS_ASSERTION(request && entry, "CommonOpenCacheEntry: request or entry is null");
    nsAutoLock lock(mCacheServiceLock);
    
    mClientIDs.AddClientID(clientID);  // add clientID to list of clientIDs
    
    
    nsCString * key = new nsCString(nsLiteralCString(clientID) + 
                                    nsLiteralCString(clientKey));
    if (!key) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    
    // create request
    *request = new  nsCacheRequest(key, (nsICacheListener *)nsnull, 
                                   accessRequested, streamBased);
    
    // procure active entry (and queue request?)
    return ActivateEntry(*request, entry);
}                                     


nsresult
nsCacheService::OpenCacheEntry(const char *clientID, const char *clientKey, 
                               PRUint32  accessRequested, PRBool  streamBased,
                               nsICacheEntryDescriptor **result)
{
    result = nsnull;
    if (!mCacheServiceLock)
        return NS_ERROR_NOT_INITIALIZED;

    nsCacheRequest * request = nsnull;
    nsCacheEntry *   entry   = nsnull;

    nsresult rv = CommonOpenCacheEntry(clientID, clientKey, accessRequested, streamBased,
                              &request, &entry);
    if (NS_FAILED(rv))  return rv;

    while (1) {
        rv = entry->Open(request, result);
        if (NS_SUCCEEDED(rv) | (rv != NS_ERROR_CACHE_ENTRY_DOOMED)) break;

        rv = ActivateEntry(request, &entry);
        if (NS_FAILED(rv)) break;
    }
    return rv;
}


nsresult
nsCacheService::AsyncOpenCacheEntry(const char *  clientID, const char *  key, 
                                    PRUint32  accessRequested, PRBool streamBased, 
                                    nsICacheListener *listener)
{
    if (!mCacheServiceLock)
        return NS_ERROR_NOT_INITIALIZED;


    nsCacheRequest * request = nsnull;
    nsCacheEntry *   entry   = nsnull;

    nsresult rv = CommonOpenCacheEntry(clientID, key, accessRequested, streamBased,
                              &request, &entry);

    if (NS_SUCCEEDED(rv)) {
        entry->AsyncOpen(request);
    }

   return rv;
}

nsresult
nsCacheService::ActivateEntry(nsCacheRequest * request, 
                              nsCacheEntry ** result)
{
    nsresult        rv = NS_OK;
    PRBool          entryIsNew = PR_FALSE;
    nsCacheDevice * device = nsnull;

    NS_ASSERTION(request != nsnull, "ActivateEntry called with no request");
    if (request == nsnull)  return NS_ERROR_NULL_POINTER;
    if (result) *result = nsnull;

    // PRBool asyncRequest = (result == nsnull);
    
    nsAutoLock lock(mCacheServiceLock);  // hold monitor to keep mActiveEntries coherent

 
    // search active entries (including those not bound to device)
    nsCacheEntry *entry = mActiveEntries.GetEntry(request->mKey);

    // doom existing entry if we are processing a FORCE-WRITE
    if (entry && (request->mAccessRequested == nsICache::ACCESS_WRITE)) {
        entry->Doom();
    }

    if (!entry) {
        entry = new nsCacheEntry(request->mKey);
        if (!entry)
            return NS_ERROR_OUT_OF_MEMORY;

        entryIsNew = PR_TRUE;
        rv = mActiveEntries.AddEntry(entry);
        if (NS_FAILED(rv)) return rv;

        //** queue request

        rv = SearchCacheDevices(entry, &device);

        if ((rv == NS_ERROR_CACHE_KEY_NOT_FOUND) &&
            !(request->mAccessRequested & nsICache::ACCESS_WRITE)) {
            // this was a READ-ONLY request, deallocate entry
            //** dealloc entry, call listener with error, etc.
            *result = nsnull;
            return NS_ERROR_CACHE_KEY_NOT_FOUND;
        }
    } else {
        //** queue request
    }

    return rv;
}


nsresult
SearchCacheDevices(nsCacheEntry * entry, nsCacheDevice ** result)
{
    nsresult        rv = NS_OK;
    nsCacheDevice * device;

    *result = nsnull;
    // if we don't alter the device list after initialization,
    // we don't have to protect it with the monitor

    //** first device
    while (device) {
        rv = device->ActivateEntryIfFound(entry);
        if (NS_SUCCEEDED(rv)) {
            *result = device;
            break;
        } else if (rv != NS_ERROR_CACHE_KEY_NOT_FOUND) {
            //** handle errors (no memory, disk full, mismatched streamBased, whatever)
        }
        //** next device
    }

    return rv;
}

nsCacheEntry *
nsCacheService::SearchActiveEntries(const nsCString * key)
{
    return nsnull;
}



/*
 *  nsCacheClientHashTable
 */

PLDHashTableOps
nsCacheClientHashTable::ops =
{
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    GetKey,
    HashKey,
    MatchEntry,
    MoveEntry,
    ClearEntry,
    Finalize
};


nsCacheClientHashTable::nsCacheClientHashTable()
    : initialized(0)
{
}

nsCacheClientHashTable::~nsCacheClientHashTable()
{
    //** maybe we should finalize the table...
}


nsresult
nsCacheClientHashTable::Init()
{
    nsresult rv = NS_OK;
    initialized = PL_DHashTableInit(&table, &ops, nsnull,
                                    sizeof(nsCacheClientHashTableEntry), 16);

    if (!initialized) rv = NS_ERROR_OUT_OF_MEMORY;
    return rv;
}


void
nsCacheClientHashTable::AddClientID(const char * clientID)
{
    PLDHashEntryHdr *hashEntry;
    char * copy;
    hashEntry = PL_DHashTableOperate(&table, clientID, PL_DHASH_ADD);
    if (((nsCacheClientHashTableEntry *)hashEntry)->clientID == 0) {
        copy = nsCRT::strdup(clientID);
        ((nsCacheClientHashTableEntry *)hashEntry)->clientID = copy;
    }
}

//** enumerate clientIDs


/*
 *  hash table operation callback functions
 */

const void *
nsCacheClientHashTable::GetKey( PLDHashTable * /*table*/, PLDHashEntryHdr *hashEntry)
{
    return ((nsCacheClientHashTableEntry *)hashEntry)->clientID;
}

PRBool
nsCacheClientHashTable::MatchEntry(PLDHashTable *       /* table */,
                                  const PLDHashEntryHdr * hashEntry,
                                  const void *            key)
{
    NS_ASSERTION(key !=  nsnull, "nsCacheClientHashTable::MatchEntry : null key");
    char * clientID = ((nsCacheClientHashTableEntry *)hashEntry)->clientID;

    return nsCRT::strcmp((char *)key, clientID) == 0;
}


void
nsCacheClientHashTable::MoveEntry(PLDHashTable * /* table */,
                                 const PLDHashEntryHdr *from,
                                 PLDHashEntryHdr       *to)
{
    to->keyHash = from->keyHash;
    ((nsCacheClientHashTableEntry *)to)->clientID =
        ((nsCacheClientHashTableEntry *)from)->clientID;
}


void
nsCacheClientHashTable::ClearEntry(PLDHashTable * /* table */,
                                  PLDHashEntryHdr * hashEntry)
{
    ((nsCacheClientHashTableEntry *)hashEntry)->keyHash  = 0;
    ((nsCacheClientHashTableEntry *)hashEntry)->clientID = 0;
}

void
nsCacheClientHashTable::Finalize(PLDHashTable * /* table */)
{
    //** gee, if there's anything left in the table, maybe we should get rid of it.
}

