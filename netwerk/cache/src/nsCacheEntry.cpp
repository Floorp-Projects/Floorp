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


// XXX find better place to put this
          // Convert PRTime to unix-style time_t, i.e. seconds since the epoch
PRUint32  ConvertPRTimeToSeconds(PRTime time64);


nsCacheEntry::nsCacheEntry(nsCString *          key,
                           PRBool               streamBased,
                           nsCacheStoragePolicy storagePolicy)
    : mKey(key),
      mFetchCount(1),
      mLastValidated(0),
      mExpirationTime(0),
      mFlags(0),
      mDataSize(0),
      mMetaSize(0),
      mCacheDevice(nsnull),
      mData(nsnull),
      mMetaData(nsnull)
{
    PR_INIT_CLIST(this);
    PR_INIT_CLIST(&mRequestQ);
    PR_INIT_CLIST(&mDescriptorQ);

    mLastFetched = ConvertPRTimeToSeconds(PR_Now());
    
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
    delete mKey;
    delete mMetaData;
}


void
nsCacheEntry::SetDataSize( PRUint32   size)
{
    mDataSize = size;
    mLastModified = ConvertPRTimeToSeconds(PR_Now());
    MarkEntryDirty();
}

void
nsCacheEntry::SetMetaDataSize( PRUint32   size)
{
    mMetaSize = size;
    mLastModified = ConvertPRTimeToSeconds(PR_Now());
    MarkEntryDirty();
}

nsresult
nsCacheEntry::GetSecurityInfo( nsISupports ** result)
{
    NS_ENSURE_ARG_POINTER(result);
    NS_IF_ADDREF(*result = mSecurityInfo);
    return NS_OK;
}


nsresult
nsCacheEntry::SetSecurityInfo( nsISupports *  info)
{
    mSecurityInfo = info;
    return NS_OK;
}


nsresult
nsCacheEntry::GetData(nsISupports **result)
{
    if (!result)         
        return NS_ERROR_NULL_POINTER;

    NS_IF_ADDREF(*result = mData);
    return NS_OK;
}


nsresult
nsCacheEntry::SetData(nsISupports * data)
{
    mLastModified = ConvertPRTimeToSeconds(PR_Now());
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
        mMetaData = nsCacheMetaData::Create();
        if (!mMetaData)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult rv = mMetaData->SetElement(key, value);
    if (NS_SUCCEEDED(rv))
        MarkMetaDataDirty();
    
    // XXX calc meta data size

    mLastModified = ConvertPRTimeToSeconds(PR_Now());

    return rv;
}


#if 0
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
#endif

nsresult
nsCacheEntry::FlattenMetaData(char ** data, PRUint32 * size)
{
    NS_ENSURE_ARG_POINTER(size);

    if (mMetaData)
        return mMetaData->FlattenMetaData(data, size);

    if (data) *data = nsnull;
    *size = 0;

    return NS_OK;
}

nsresult
nsCacheEntry::UnflattenMetaData(char * data, PRUint32 size)
{
    delete mMetaData;
    mMetaData = nsCacheMetaData::Create();
    if (!mMetaData)
        return NS_ERROR_OUT_OF_MEMORY;
    return mMetaData->UnflattenMetaData(data, size);
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
    
    if (IsDoomed()) return NS_ERROR_CACHE_ENTRY_DOOMED;

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

    NS_ADDREF(*result = descriptor);
    return NS_OK;
}


PRBool
nsCacheEntry::RemoveRequest(nsCacheRequest * request)
{
    // XXX if debug: verify this request belongs to this entry
    PR_REMOVE_AND_INIT_LINK(request);

    // return true if this entry should stay active
    return !((PR_CLIST_IS_EMPTY(&mRequestQ)) &&
             (PR_CLIST_IS_EMPTY(&mDescriptorQ)));
}


PRBool
nsCacheEntry::RemoveDescriptor(nsCacheEntryDescriptor * descriptor)
{
    // XXX if debug: verify this descriptor belongs to this entry
    PR_REMOVE_AND_INIT_LINK(descriptor);

    if (!PR_CLIST_IS_EMPTY(&mDescriptorQ))
        return PR_TRUE;  // stay active if we still have open descriptors

    if (PR_CLIST_IS_EMPTY(&mRequestQ))
        return PR_FALSE; // no descriptors or requests, we can deactivate

    return PR_TRUE;     // find next best request to give a descriptor to
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
#ifndef DEBUG_dougt
    NS_ASSERTION(((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry == 0,
                 "nsCacheEntryHashTable::AddEntry - entry already used");
#endif
    ((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry = cacheEntry;

    return NS_OK;
}


void
nsCacheEntryHashTable::RemoveEntry( nsCacheEntry *cacheEntry)
{
    NS_ASSERTION(initialized, "nsCacheEntryHashTable not initialized");
    NS_ASSERTION(cacheEntry, "cacheEntry == nsnull");

    // XXX debug code to make sure we have the entry we're trying to remove

    (void) PL_DHashTableOperate(&table, cacheEntry->mKey, PL_DHASH_REMOVE);
}


void
nsCacheEntryHashTable::VisitEntries( nsCacheEntryHashTable::Visitor *visitor)
{
    PL_DHashTableEnumerate(&table, VisitEntry, visitor);
}


PLDHashOperator
nsCacheEntryHashTable::VisitEntry(PLDHashTable *table,
                                  PLDHashEntryHdr *hashEntry,
                                  PRUint32 number,
                                  void *arg)
{
    nsCacheEntry *cacheEntry = ((nsCacheEntryHashTableEntry *)hashEntry)->cacheEntry;
    nsCacheEntryHashTable::Visitor *visitor = (nsCacheEntryHashTable::Visitor*) arg;
    return (visitor->VisitEntry(cacheEntry) ? PL_DHASH_NEXT : PL_DHASH_STOP);
}

/**
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
