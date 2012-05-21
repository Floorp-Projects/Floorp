/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nspr.h"
#include "nsCacheEntry.h"
#include "nsCacheEntryDescriptor.h"
#include "nsCacheMetaData.h"
#include "nsCacheRequest.h"
#include "nsThreadUtils.h"
#include "nsError.h"
#include "nsICacheService.h"
#include "nsCache.h"
#include "nsCacheService.h"
#include "nsCacheDevice.h"
#include "nsHashKeys.h"

using namespace mozilla;

nsCacheEntry::nsCacheEntry(nsCString *          key,
                           bool                 streamBased,
                           nsCacheStoragePolicy storagePolicy)
    : mKey(key),
      mFetchCount(0),
      mLastFetched(0),
      mLastModified(0),
      mExpirationTime(nsICache::NO_EXPIRATION_TIME),
      mFlags(0),
      mPredictedDataSize(-1),
      mDataSize(0),
      mCacheDevice(nsnull),
      mData(nsnull)
{
    MOZ_COUNT_CTOR(nsCacheEntry);
    PR_INIT_CLIST(this);
    PR_INIT_CLIST(&mRequestQ);
    PR_INIT_CLIST(&mDescriptorQ);

    if (streamBased) MarkStreamBased();
    SetStoragePolicy(storagePolicy);
}


nsCacheEntry::~nsCacheEntry()
{
    MOZ_COUNT_DTOR(nsCacheEntry);
    delete mKey;
    
    if (mData)
        nsCacheService::ReleaseObject_Locked(mData, mThread);
}


nsresult
nsCacheEntry::Create( const char *          key,
                      bool                  streamBased,
                      nsCacheStoragePolicy  storagePolicy,
                      nsCacheDevice *       device,
                      nsCacheEntry **       result)
{
    nsCString* newKey = new nsCString(key);
    if (!newKey) return NS_ERROR_OUT_OF_MEMORY;
    
    nsCacheEntry* entry = new nsCacheEntry(newKey, streamBased, storagePolicy);
    if (!entry) { delete newKey; return NS_ERROR_OUT_OF_MEMORY; }
    
    entry->SetCacheDevice(device);
    
    *result = entry;
    return NS_OK;
}


void
nsCacheEntry::Fetched()
{
    mLastFetched = SecondsFromPRTime(PR_Now());
    ++mFetchCount;
    MarkEntryDirty();
}


const char *
nsCacheEntry::GetDeviceID()
{
    if (mCacheDevice)  return mCacheDevice->GetDeviceID();
    return nsnull;
}


void
nsCacheEntry::TouchData()
{
    mLastModified = SecondsFromPRTime(PR_Now());
    MarkDataDirty();
}


void
nsCacheEntry::SetData(nsISupports * data)
{
    if (mData) {
        nsCacheService::ReleaseObject_Locked(mData, mThread);
        mData = nsnull;
    }

    if (data) {
        NS_ADDREF(mData = data);
        mThread = do_GetCurrentThread();
    }
}


void
nsCacheEntry::TouchMetaData()
{
    mLastModified = SecondsFromPRTime(PR_Now());
    MarkMetaDataDirty();
}


/**
 *  cache entry states
 *      0 descriptors (new entry)
 *      0 descriptors (existing, bound entry)
 *      n descriptors (existing, bound entry) valid
 *      n descriptors (existing, bound entry) not valid (wait until valid or doomed)
 */

nsresult
nsCacheEntry::RequestAccess(nsCacheRequest * request, nsCacheAccessMode *accessGranted)
{
    nsresult  rv = NS_OK;

    if (IsDoomed()) return NS_ERROR_CACHE_ENTRY_DOOMED;

    if (!IsInitialized()) {
        // brand new, unbound entry
        request->mKey = nsnull;  // steal ownership of the key string
        if (request->IsStreamBased())  MarkStreamBased();
        MarkInitialized();

        *accessGranted = request->AccessRequested() & nsICache::ACCESS_WRITE;
        NS_ASSERTION(*accessGranted, "new cache entry for READ-ONLY request");
        PR_APPEND_LINK(request, &mRequestQ);
        return rv;
    }

    if (IsStreamData() != request->IsStreamBased()) {
        *accessGranted = nsICache::ACCESS_NONE;
        return request->IsStreamBased() ?
            NS_ERROR_CACHE_DATA_IS_NOT_STREAM : NS_ERROR_CACHE_DATA_IS_STREAM;
    }

    if (PR_CLIST_IS_EMPTY(&mDescriptorQ)) {
        // 1st descriptor for existing bound entry
        *accessGranted = request->AccessRequested();
        if (*accessGranted & nsICache::ACCESS_WRITE) {
            MarkInvalid();
        } else {
            MarkValid();
        }
    } else {
        // nth request for existing, bound entry
        *accessGranted = request->AccessRequested() & ~nsICache::ACCESS_WRITE;
        if (!IsValid())
            rv = NS_ERROR_CACHE_WAIT_FOR_VALIDATION;
    }
    PR_APPEND_LINK(request,&mRequestQ);

    return rv;
}


nsresult
nsCacheEntry::CreateDescriptor(nsCacheRequest *           request,
                               nsCacheAccessMode          accessGranted,
                               nsICacheEntryDescriptor ** result)
{
    NS_ENSURE_ARG_POINTER(request && result);

    nsCacheEntryDescriptor * descriptor =
        new nsCacheEntryDescriptor(this, accessGranted);

    // XXX check request is on q
    PR_REMOVE_AND_INIT_LINK(request); // remove request regardless of success

    if (descriptor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    PR_APPEND_LINK(descriptor, &mDescriptorQ);

    CACHE_LOG_DEBUG(("  descriptor %p created for request %p on entry %p\n",
                    descriptor, request, this));

    NS_ADDREF(*result = descriptor);
    return NS_OK;
}


bool
nsCacheEntry::RemoveRequest(nsCacheRequest * request)
{
    // XXX if debug: verify this request belongs to this entry
    PR_REMOVE_AND_INIT_LINK(request);

    // return true if this entry should stay active
    return !((PR_CLIST_IS_EMPTY(&mRequestQ)) &&
             (PR_CLIST_IS_EMPTY(&mDescriptorQ)));
}


bool
nsCacheEntry::RemoveDescriptor(nsCacheEntryDescriptor * descriptor)
{
    NS_ASSERTION(descriptor->CacheEntry() == this, "### Wrong cache entry!!");
    nsresult rv = descriptor->CloseOutput();
    if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        return true;

    descriptor->ClearCacheEntry();
    PR_REMOVE_AND_INIT_LINK(descriptor);

    // Doom entry if something bad happens while closing. See bug #673543
    if (NS_FAILED(rv))
        nsCacheService::DoomEntry(this);

    if (!PR_CLIST_IS_EMPTY(&mDescriptorQ))
        return true;  // stay active if we still have open descriptors

    if (PR_CLIST_IS_EMPTY(&mRequestQ))
        return false; // no descriptors or requests, we can deactivate

    return true;     // find next best request to give a descriptor to
}


void
nsCacheEntry::DetachDescriptors(void)
{
    nsCacheEntryDescriptor * descriptor =
        (nsCacheEntryDescriptor *)PR_LIST_HEAD(&mDescriptorQ);

    while (descriptor != &mDescriptorQ) {
        nsCacheEntryDescriptor * nextDescriptor =
            (nsCacheEntryDescriptor *)PR_NEXT_LINK(descriptor);

        // Doom entry if something bad happens while closing. See bug #673543
        // Errors are handled different from RemoveDescriptor because this
        // method is only called from ClearDoomList (in which case the entry is
        // doomed anyway) and ClearActiveEntries (in which case we are shutting
        // down and really want to get rid of the entry immediately)
        if (NS_FAILED(descriptor->CloseOutput()))
            nsCacheService::DoomEntry(this);

        descriptor->ClearCacheEntry();
        PR_REMOVE_AND_INIT_LINK(descriptor);
        descriptor = nextDescriptor;
    }
}


/******************************************************************************
 * nsCacheEntryInfo - for implementing about:cache
 *****************************************************************************/

NS_IMPL_ISUPPORTS1(nsCacheEntryInfo, nsICacheEntryInfo)


NS_IMETHODIMP
nsCacheEntryInfo::GetClientID(char ** clientID)
{
    NS_ENSURE_ARG_POINTER(clientID);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return ClientIDFromCacheKey(*mCacheEntry->Key(), clientID);
}


NS_IMETHODIMP
nsCacheEntryInfo::GetDeviceID(char ** deviceID)
{
    NS_ENSURE_ARG_POINTER(deviceID);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *deviceID = NS_strdup(mCacheEntry->GetDeviceID());
    return *deviceID ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP
nsCacheEntryInfo::GetKey(nsACString &key)
{
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    return ClientKeyFromCacheKey(*mCacheEntry->Key(), key);
}


NS_IMETHODIMP
nsCacheEntryInfo::GetFetchCount(PRInt32 * fetchCount)
{
    NS_ENSURE_ARG_POINTER(fetchCount);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *fetchCount = mCacheEntry->FetchCount();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryInfo::GetLastFetched(PRUint32 * lastFetched)
{
    NS_ENSURE_ARG_POINTER(lastFetched);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *lastFetched = mCacheEntry->LastFetched();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryInfo::GetLastModified(PRUint32 * lastModified)
{
    NS_ENSURE_ARG_POINTER(lastModified);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *lastModified = mCacheEntry->LastModified();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryInfo::GetExpirationTime(PRUint32 * expirationTime)
{
    NS_ENSURE_ARG_POINTER(expirationTime);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *expirationTime = mCacheEntry->ExpirationTime();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryInfo::GetDataSize(PRUint32 * dataSize)
{
    NS_ENSURE_ARG_POINTER(dataSize);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;

    *dataSize = mCacheEntry->DataSize();
    return NS_OK;
}


NS_IMETHODIMP
nsCacheEntryInfo::IsStreamBased(bool * result)
{
    NS_ENSURE_ARG_POINTER(result);
    if (!mCacheEntry)  return NS_ERROR_NOT_AVAILABLE;
    
    *result = mCacheEntry->IsStreamData();
    return NS_OK;
}


/******************************************************************************
 *  nsCacheEntryHashTable
 *****************************************************************************/

PLDHashTableOps
nsCacheEntryHashTable::ops =
{
    PL_DHashAllocTable,
    PL_DHashFreeTable,
    HashKey,
    MatchEntry,
    MoveEntry,
    ClearEntry,
    PL_DHashFinalizeStub
};


nsCacheEntryHashTable::nsCacheEntryHashTable()
    : initialized(false)
{
    MOZ_COUNT_CTOR(nsCacheEntryHashTable);
}


nsCacheEntryHashTable::~nsCacheEntryHashTable()
{
    MOZ_COUNT_DTOR(nsCacheEntryHashTable);
    if (initialized)
        Shutdown();
}


nsresult
nsCacheEntryHashTable::Init()
{
    nsresult rv = NS_OK;
    initialized = PL_DHashTableInit(&table, &ops, nsnull,
                                           sizeof(nsCacheEntryHashTableEntry), 512);

    if (!initialized) rv = NS_ERROR_OUT_OF_MEMORY;
    
    return rv;
}

void
nsCacheEntryHashTable::Shutdown()
{
    if (initialized) {
        PL_DHashTableFinish(&table);
        initialized = false;
    }
}


nsCacheEntry *
nsCacheEntryHashTable::GetEntry( const nsCString * key)
{
    PLDHashEntryHdr *hashEntry;
    nsCacheEntry    *result = nsnull;

    NS_ASSERTION(initialized, "nsCacheEntryHashTable not initialized");
    if (!initialized)  return nsnull;
    
    hashEntry = PL_DHashTableOperate(&table, key, PL_DHASH_LOOKUP);
    if (PL_DHASH_ENTRY_IS_BUSY(hashEntry)) {
        result = ((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry;
    }
    return result;
}


nsresult
nsCacheEntryHashTable::AddEntry( nsCacheEntry *cacheEntry)
{
    PLDHashEntryHdr    *hashEntry;

    NS_ASSERTION(initialized, "nsCacheEntryHashTable not initialized");
    if (!initialized)  return NS_ERROR_NOT_INITIALIZED;
    if (!cacheEntry)   return NS_ERROR_NULL_POINTER;

    hashEntry = PL_DHashTableOperate(&table, cacheEntry->mKey, PL_DHASH_ADD);
#ifndef DEBUG_dougt
    NS_ASSERTION(((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry == 0,
                 "### nsCacheEntryHashTable::AddEntry - entry already used");
#endif
    ((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry = cacheEntry;

    return NS_OK;
}


void
nsCacheEntryHashTable::RemoveEntry( nsCacheEntry *cacheEntry)
{
    NS_ASSERTION(initialized, "nsCacheEntryHashTable not initialized");
    NS_ASSERTION(cacheEntry, "### cacheEntry == nsnull");

    if (!initialized)  return; // NS_ERROR_NOT_INITIALIZED

#if DEBUG
    // XXX debug code to make sure we have the entry we're trying to remove
    nsCacheEntry *check = GetEntry(cacheEntry->mKey);
    NS_ASSERTION(check == cacheEntry, "### Attempting to remove unknown cache entry!!!");
#endif
    (void) PL_DHashTableOperate(&table, cacheEntry->mKey, PL_DHASH_REMOVE);
}


void
nsCacheEntryHashTable::VisitEntries( PLDHashEnumerator etor, void *arg)
{
    NS_ASSERTION(initialized, "nsCacheEntryHashTable not initialized");
    if (!initialized)  return; // NS_ERROR_NOT_INITIALIZED
    PL_DHashTableEnumerate(&table, etor, arg);
}


/**
 *  hash table operation callback functions
 */

PLDHashNumber
nsCacheEntryHashTable::HashKey( PLDHashTable *table, const void *key)
{
    return HashString(*static_cast<const nsCString *>(key));
}

bool
nsCacheEntryHashTable::MatchEntry(PLDHashTable *       /* table */,
                                  const PLDHashEntryHdr * hashEntry,
                                  const void *            key)
{
    NS_ASSERTION(key !=  nsnull, "### nsCacheEntryHashTable::MatchEntry : null key");
    nsCacheEntry *cacheEntry = ((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry;

    return cacheEntry->mKey->Equals(*(nsCString *)key);
}


void
nsCacheEntryHashTable::MoveEntry(PLDHashTable * /* table */,
                                 const PLDHashEntryHdr *from,
                                 PLDHashEntryHdr       *to)
{
    ((nsCacheEntryHashTableEntry *)to)->cacheEntry =
        ((nsCacheEntryHashTableEntry *)from)->cacheEntry;
}


void
nsCacheEntryHashTable::ClearEntry(PLDHashTable * /* table */,
                                  PLDHashEntryHdr * hashEntry)
{
    ((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry = 0;
}
