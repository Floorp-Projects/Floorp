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
 * The Original Code is nsCacheEntry.cpp, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */


#include "nspr.h"
#include "nsCacheEntry.h"
#include "nsCacheEntryDescriptor.h"
#include "nsCacheMetaData.h"
#include "nsCacheRequest.h"
#include "nsError.h"
#include "nsICacheService.h"

#define ONE_YEAR (PR_USEC_PER_SEC * 60 * 60 * 24 * 365)
nsCacheEntry::nsCacheEntry(nsCString *          key,
                           PRBool               streamBased,
                           nsCacheStoragePolicy storagePolicy)
    : mKey(key),
      mFetchCount(0),
      mLastValidated(LL_ZERO),
      mExpirationTime(LL_ZERO),
      mFlags(0),
      mDataSize(0),
      mMetaSize(0),
      mCacheDevice(nsnull),
      mData(nsnull),
      mMetaData(nsnull)
{
    PR_INIT_CLIST(&mListLink);
    PR_INIT_CLIST(&mRequestQ);
    PR_INIT_CLIST(&mDescriptorQ);

    mLastFetched = PR_Now();
    
    if (streamBased) MarkStreamBased();

    if ((storagePolicy == nsICache::STORE_IN_MEMORY) ||
        (storagePolicy == nsICache::STORE_ANYWHERE)) {
        MarkAllowedInMemory();
    }
    
    if ((storagePolicy == nsICache::STORE_ON_DISK) ||
        (storagePolicy == nsICache::STORE_ANYWHERE)) {
        MarkAllowedOnDisk();
    }
}


nsCacheEntry::~nsCacheEntry()
{
    if (mCacheDevice) {
        //** ask device to clean up
    }
}


nsresult
nsCacheEntry::GetData(nsISupports **result)
{
    if (!result)         return NS_ERROR_NULL_POINTER;

    NS_IF_ADDREF(*result = mData);
    return NS_OK;
}


nsresult
nsCacheEntry::SetData(nsISupports * data)
{
    mData = data;
    return NS_OK;
}


nsresult
nsCacheEntry::GetMetaDataElement( const nsAReadableCString&  key,
                                  nsAReadableCString **      value)
{
    *value = mMetaData ? mMetaData->GetElement(&key) : nsnull;
    return NS_OK;
}


nsresult
nsCacheEntry::SetMetaDataElement( const nsAReadableCString& key, 
                                  const nsAReadableCString& value)
{
    if (!mMetaData) {
        mMetaData = new nsCacheMetaData();
        if (!mMetaData)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult rv = mMetaData->SetElement(key, value);
    if (NS_SUCCEEDED(rv))
        MarkMetaDataDirty();

    return rv;
}


nsresult
nsCacheEntry::GetKeyValueArray(nsCacheMetaDataKeyValuePair ** array,
                               PRUint32 *                     count)
{
    if (!array || !count)  return NS_ERROR_NULL_POINTER;

    if (!mMetaData) {
        *array = nsnull;
        *count = 0;
        return NS_OK;
    }
    return mMetaData->GetKeyValueArray(array, count);
}


void
nsCacheEntry::MarkValid()
{
    //** bind if not bound
    //** convert pending requests to descriptors, etc.
    mFlags |= eValidMask;
}


/**
 *  cache entry states
 *      0 descriptors (new entry)
 *      0 descriptors (existing, bound entry)
 *      n descriptors (existing, bound entry) valid
 *      n descriptors (existing, bound entry) not valid (wait until valid or doomed)
 */

nsresult
nsCacheEntry::CommonOpen(nsCacheRequest * request, nsCacheAccessMode *accessGranted)
{
    nsresult  rv = NS_OK;
    
    if (!IsInitialized()) {
        // brand new, unbound entry
        *accessGranted = request->mAccessRequested & nsICache::ACCESS_WRITE;
        NS_ASSERTION(*accessGranted, "new cache entry for READ-ONLY request");
        if (request->mStreamBased)  MarkStreamBased();
        mFetchCount = 1;
        MarkInitialized();
        return rv;
    }

    if (IsStreamData() != request->mStreamBased) {
        *accessGranted = nsICache::ACCESS_NONE;
        return request->mStreamBased ?
            NS_ERROR_CACHE_DATA_IS_NOT_STREAM : NS_ERROR_CACHE_DATA_IS_STREAM;
    }

    if (PR_CLIST_IS_EMPTY(&mDescriptorQ)) {
        // 1st descriptor for existing, bound entry
        *accessGranted = request->mAccessRequested;
    } else {
        // nth request for existing, bound entry
        *accessGranted = request->mAccessRequested & ~nsICache::ACCESS_WRITE;
        if (!IsValid())
            rv = NS_ERROR_CACHE_WAIT_FOR_VALIDATION;
    }
    return rv;
}


nsresult
nsCacheEntry::Open(nsCacheRequest * request, nsICacheEntryDescriptor ** result)
{
    if (!request)  return NS_ERROR_NULL_POINTER;

    nsCacheAccessMode  accessGranted;
    nsresult  rv = CommonOpen(request, &accessGranted);
    if (NS_SUCCEEDED(rv)) {
        //        rv = nsCacheEntryDescriptor::Create(this, accessGranted, result);

        nsCacheEntryDescriptor * descriptor =
            new nsCacheEntryDescriptor(this, accessGranted);

        if (descriptor == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;

        NS_ADDREF(*result = descriptor);

        if (NS_SUCCEEDED(rv)) {
            // queue the descriptor
            PR_APPEND_LINK((descriptor)->GetListNode(), &mDescriptorQ);
        }

    } else if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
        // queue request
        PR_APPEND_LINK(request->GetListNode(), &mRequestQ);
        //** allocate PRCondVar for request, if none
        //** release service lock
        //** wait until valid or doomed
    }
    return rv;
}


nsresult
nsCacheEntry::AsyncOpen(nsCacheRequest * request)
{
    if (!request)  return NS_ERROR_NULL_POINTER;

    nsCacheAccessMode  accessGranted;
    nsresult  rv = CommonOpen(request, &accessGranted);
    if (NS_SUCCEEDED(rv)) {
        nsICacheEntryDescriptor * descriptor;
        rv = nsCacheEntryDescriptor::Create(this, accessGranted, &descriptor);
        if (NS_SUCCEEDED(rv)) {
            //** queue the descriptor
            //** post event to call listener with 
        }
    } else if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
        //** queue request and we're done (MarkValid will notify pending requests)
    }
    return rv;
}


nsresult
nsCacheEntry::Doom()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


PRBool
nsCacheEntry::RemoveRequest(nsCacheRequest * request)
{
    //** if debug: verify this request belongs to this entry
    PR_REMOVE_AND_INIT_LINK(request->GetListNode());

    // return true if this entry should stay active
    return !((PR_CLIST_IS_EMPTY(&mRequestQ)) &&
             (PR_CLIST_IS_EMPTY(&mDescriptorQ)));
}


PRBool
nsCacheEntry::RemoveDescriptor(nsCacheEntryDescriptor * descriptor)
{
    //** if debug: verify this descriptor belongs to this entry
    PR_REMOVE_AND_INIT_LINK(descriptor->GetListNode());

    if (!PR_CLIST_IS_EMPTY(&mDescriptorQ))
        return PR_TRUE;  // stay active if we still have open descriptors

    if (PR_CLIST_IS_EMPTY(&mRequestQ))
        return PR_FALSE; // no descriptors or requests, we can deactivate

    //** find next best request to give a descriptor to
    return PR_TRUE;
}


/*
 *  nsCacheEntryHashTable
 */

PLDHashTableOps
nsCacheEntryHashTable::ops =
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


nsCacheEntryHashTable::nsCacheEntryHashTable()
    : initialized(0)
{
}


nsCacheEntryHashTable::~nsCacheEntryHashTable()
{
    PL_DHashTableFinish(&table);
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


nsCacheEntry *
nsCacheEntryHashTable::GetEntry( const nsCString * key)
{
    PLDHashEntryHdr *hashEntry;
    nsCacheEntry    *result = nsnull;

    NS_ASSERTION(initialized, "nsCacheEntryHashTable not initialized");
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
    if (!cacheEntry) return NS_ERROR_NULL_POINTER;

    hashEntry = PL_DHashTableOperate(&table, cacheEntry->mKey, PL_DHASH_ADD);
    NS_ASSERTION(((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry == 0,
                 "nsCacheEntryHashTable::AddEntry - entry already used");

    ((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry = cacheEntry;

    return NS_OK;
}

nsresult
nsCacheEntryHashTable::RemoveEntry( nsCacheEntry *cacheEntry)
{
    PLDHashEntryHdr    *hashEntry;

    NS_ASSERTION(initialized, "nsCacheEntryHashTable not initialized");
    if (!cacheEntry) return NS_ERROR_NULL_POINTER;

    hashEntry = PL_DHashTableOperate(&table, cacheEntry->mKey, PL_DHASH_ADD);
    if (PL_DHASH_ENTRY_IS_FREE(hashEntry)) {
        // it's not in the table!?!
        return NS_ERROR_UNEXPECTED;
    }
    return NS_OK;
}

/*
 *  hash table operation callback functions
 */

const void *
nsCacheEntryHashTable::GetKey( PLDHashTable * /*table*/, PLDHashEntryHdr *hashEntry)
{
    nsCacheEntry *cacheEntry = ((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry;
    return cacheEntry->mKey;
}


PLDHashNumber
nsCacheEntryHashTable::HashKey( PLDHashTable *table, const void *key)
{
    return PL_DHashStringKey(table,((nsCString *)key)->get());
}

PRBool
nsCacheEntryHashTable::MatchEntry(PLDHashTable *       /* table */,
                                  const PLDHashEntryHdr * hashEntry,
                                  const void *            key)
{
    NS_ASSERTION(key !=  nsnull, "nsCacheEntryHashTable::MatchEntry : null key");
    nsCacheEntry *cacheEntry = ((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry;

    return nsStr::StrCompare(*cacheEntry->mKey, *(nsCString *)key, -1, PR_FALSE) == 0;
}


void
nsCacheEntryHashTable::MoveEntry(PLDHashTable * /* table */,
                                 const PLDHashEntryHdr *from,
                                 PLDHashEntryHdr       *to)
{
    to->keyHash = from->keyHash;
    ((nsCacheEntryHashTableEntry *)to)->cacheEntry =
        ((nsCacheEntryHashTableEntry *)from)->cacheEntry;
}


void
nsCacheEntryHashTable::ClearEntry(PLDHashTable * /* table */,
                                  PLDHashEntryHdr * hashEntry)
{
    ((nsCacheEntryHashTableEntry *)hashEntry)->keyHash    = 0;
    ((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry = 0;
}

void
nsCacheEntryHashTable::Finalize(PLDHashTable * table)
{
    (void) PL_DHashTableEnumerate(table, FreeCacheEntries, nsnull);
}


PLDHashOperator
nsCacheEntryHashTable::FreeCacheEntries(PLDHashTable *table,
                              PLDHashEntryHdr *hdr,
                              PRUint32 number,
                              void *arg)
{
    nsCacheEntryHashTableEntry *entry = (nsCacheEntryHashTableEntry *)hdr;
    delete entry->cacheEntry;
    return PL_DHASH_NEXT;
}
