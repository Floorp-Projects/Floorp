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
nsCacheEntry::nsCacheEntry(nsCString * key)
    : mKey(key),
    mFetchCount(0),
    mFlags(0),
    mDataSize(0),
    mMetaSize(0),
      mCacheDevice(nsnull),
      mData(nsnull),
      mMetaData(nsnull)
{
    PR_INIT_CLIST(&mRequestQ);
    PR_INIT_CLIST(&mDescriptorQ);

    mLastFetched = mLastValidated = PR_Now();
    mExpirationTime = 0;
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
    if (IsStreamData())
        return NS_ERROR_CACHE_DATA_IS_STREAM;

    if (result) *result = mData;
    return NS_OK;
}


nsresult
nsCacheEntry::SetData(nsISupports * data)
{
    if (IsStreamData())
        return NS_ERROR_CACHE_DATA_IS_STREAM;

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
    return mMetaData->SetElement(&key, &value);
}


/**
 *  cache entry states
 *      0 descriptors (new entry)
 *      0 descriptors (existing, bound entry)
 *      n descriptors (existing, bound entry) valid
 *      n descriptors (existing, bound entry) not valid (wait until valid or doomed)
 */

nsresult
nsCacheEntry::CommonOpen(nsCacheRequest * request, PRUint32 *accessGranted)
{
    nsresult  rv = NS_OK;
    
    if (!IsInitialized()) {
        // brand new, unbound entry
        NS_ASSERTION(request->mAccessRequested & nsICacheService::WRITE,
                     "new cache entry for READ-ONLY request");
        if (request->mStreamBased)  MarkStreamBased();
        mFetchCount = 1;
        MarkInitialized();
        *accessGranted = request->mAccessRequested & ~nsICacheService::WRITE;
        return rv;
    }

    if (IsStreamData() != request->mStreamBased) {
        *accessGranted = nsICacheService::NO_ACCESS;
        return request->mStreamBased ?
            NS_ERROR_CACHE_DATA_IS_NOT_STREAM : NS_ERROR_CACHE_DATA_IS_STREAM;
    }

    if (PR_CLIST_IS_EMPTY(&mDescriptorQ)) {
        // 1st descriptor for existing, bound entry
        *accessGranted = request->mAccessRequested;
    } else {
        // nth request for existing, bound entry
        *accessGranted = request->mAccessRequested & ~nsICacheService::WRITE;
        if (!IsValid())
            rv = NS_ERROR_CACHE_WAIT_FOR_VALIDATION;
    }
    return rv;
}


nsresult
nsCacheEntry::Open(nsCacheRequest * request, nsICacheEntryDescriptor ** result)
{
    if (!request)  return NS_ERROR_NULL_POINTER;

    PRUint32  accessGranted;
    nsresult  rv = CommonOpen(request, &accessGranted);
    if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
        //** queue request
        //** allocate PRCondVar for request, if none
        //** release service lock
        //** wait until valid or doomed
    }

    if (NS_SUCCEEDED(rv)) {
        rv = nsCacheEntryDescriptor::Create(this, accessGranted, result);
        //** queue the descriptor
    }

    return rv;
}


nsresult
nsCacheEntry::AsyncOpen(nsCacheRequest * request)
{
    if (!request)  return NS_ERROR_NULL_POINTER;

    PRUint32  accessGranted;
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
    } else {
    }
    return NS_ERROR_NOT_IMPLEMENTED;
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
    //** maybe we should finalize the table...
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
    NS_ASSERTION(((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry != 0,
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
nsCacheEntryHashTable::Finalize(PLDHashTable * /* table */)
{
    //** gee, if there's anything left in the table, maybe we should get rid of it.
}

