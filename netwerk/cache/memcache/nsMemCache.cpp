/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *  
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

/**
 * nsMemCache is the implementation of an in-memory network-data
 * cache, used to cache the responses to network retrieval commands.
 * Each cache entry may contain both content, e.g. GIF image data, and
 * associated metadata, e.g. HTTP headers.  Each entry is indexed by
 * two different keys: a record id number and an opaque key, which is
 * created by the cache manager by combining the URI with a "secondary
 * key", e.g. HTTP post data.
 */

#include "nsMemCache.h"
#include "nsMemCacheRecord.h"
#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsHashtable.h"
#include "nsHashtableEnumerator.h"
#include "nsEnumeratorUtils.h"

PRInt32 nsMemCache::gRecordSerialNumber = 0;

nsMemCache::nsMemCache()
    : mNumEntries(0), mOccupancy(0), mEnabled(PR_TRUE),
      mHashTable(0)
{
    NS_INIT_REFCNT();
}

nsMemCache::~nsMemCache()
{
    nsresult rv;

    rv = RemoveAll();
    NS_WARN_IF_FALSE(NS_SUCCEEDED(rv) && (mNumEntries == 0),
                     "Failure to shut down memory cache cleanly. "
                     "Somewhere, someone is holding references to at least one cache record");
    delete mHashTable;
}

nsresult
nsMemCache::Init()
{
    mHashTable = new nsHashtable(256);
    if (!mHashTable)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsMemCache, nsINetDataCache)

NS_IMETHODIMP
nsMemCache::GetDescription(PRUnichar * *aDescription)
{
    nsAutoString description;
    description.AssignWithConversion("Memory Cache");

    *aDescription = ToNewUnicode(description);
    if (!*aDescription)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCache::Contains(const char *aKey, PRUint32 aKeyLength, PRBool *aFound)
{
    nsOpaqueKey opaqueKey(aKey, aKeyLength);
    *aFound = mHashTable->Exists(&opaqueKey);
    return NS_OK;
}

NS_IMETHODIMP
nsMemCache::GetCachedNetData(const char *aKey, PRUint32 aKeyLength,
                             nsINetDataCacheRecord* *aRecord)
{
    nsresult rv;
    nsMemCacheRecord* record = 0;
    nsOpaqueKey opaqueKey(aKey, aKeyLength);

    record = (nsMemCacheRecord*)mHashTable->Get(&opaqueKey);

    // No existing cache database entry was found.  Create a new one.
    // This requires two mappings in the hash table:
    //    Record ID  ==> record
    //    Opaque key ==> record
    if (!record) {
        record = new nsMemCacheRecord;
        if (!record)
            goto out_of_memory;
        rv = record->Init(aKey, aKeyLength, ++gRecordSerialNumber, this);
        if (NS_FAILED(rv)) goto out_of_memory;

        // Index the record by opaque key
        nsOpaqueKey opaqueKey2(record->mKey, record->mKeyLength);
        mHashTable->Put(&opaqueKey2, record);
        
        // Index the record by it's record ID
        char *recordIDbytes = NS_REINTERPRET_CAST(char *, &record->mRecordID);
        nsOpaqueKey opaqueKey3(recordIDbytes, sizeof record->mRecordID);
        mHashTable->Put(&opaqueKey3, record);
        
        // The hash table holds on to the record
        record->AddRef();
        mNumEntries++;
    }

    record->AddRef();
    *aRecord = record;
    return NS_OK;

 out_of_memory:
    delete record;
    return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsMemCache::GetCachedNetDataByID(PRInt32 RecordID,
                                 nsINetDataCacheRecord* *aRecord)
{
    nsOpaqueKey opaqueKey(NS_REINTERPRET_CAST(const char *, &RecordID),
                          sizeof RecordID);
    *aRecord = (nsINetDataCacheRecord*)mHashTable->Get(&opaqueKey);
    if (*aRecord) {
        NS_ADDREF(*aRecord);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_METHOD
nsMemCache::Delete(nsMemCacheRecord* aRecord)
{
    nsMemCacheRecord *removedRecord;

    char *recordIDbytes = NS_REINTERPRET_CAST(char *, &aRecord->mRecordID);
    nsOpaqueKey opaqueRecordIDKey(recordIDbytes,
                                  sizeof aRecord->mRecordID);
    removedRecord = (nsMemCacheRecord*)mHashTable->Remove(&opaqueRecordIDKey);
    NS_ASSERTION(removedRecord == aRecord, "memory cache database inconsistent");

    nsOpaqueKey opaqueKey(aRecord->mKey, aRecord->mKeyLength);
    removedRecord = (nsMemCacheRecord*)mHashTable->Remove(&opaqueKey);
    NS_ASSERTION(removedRecord == aRecord, "memory cache database inconsistent");

    PRUint32 bytesUsed = 0;
    aRecord->GetStoredContentLength(&bytesUsed);

    aRecord->Release();

    mOccupancy -= bytesUsed;
    mNumEntries--;
    
    return NS_OK;
}
    

NS_IMETHODIMP
nsMemCache::GetEnabled(PRBool *aEnabled)
{
    NS_ENSURE_ARG(aEnabled);
    *aEnabled = mEnabled;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCache::SetEnabled(PRBool aEnabled)
{
    mEnabled = aEnabled;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCache::GetFlags(PRUint32 *aFlags)
{
    NS_ENSURE_ARG(aFlags);
    *aFlags = MEMORY_CACHE;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCache::GetNumEntries(PRUint32 *aNumEntries)
{
    NS_ENSURE_ARG(aNumEntries);
    *aNumEntries = mNumEntries;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCache::GetMaxEntries(PRUint32 *aMaxEntries)
{
    NS_ENSURE_ARG(aMaxEntries);
    *aMaxEntries = MEM_CACHE_MAX_ENTRIES;
    return NS_OK;
}

static NS_METHOD
HashEntryConverter(nsHashKey *aKey, void *aValue,
                   void *unused, nsISupports **retval)
{
    nsMemCacheRecord *record;
    nsOpaqueKey *opaqueKey;

    record = (nsMemCacheRecord*)aValue;
    opaqueKey = (nsOpaqueKey*)aKey;

    // Hash table keys that index cache entries by their record ID
    // shouldn't be enumerated.
    if ((opaqueKey->GetBufferLength() == sizeof(PRInt32))) {

#ifdef DEBUG
        PRInt32 recordID;
        record->GetRecordID(&recordID);
        NS_ASSERTION(*((PRInt32*)opaqueKey->GetBuffer()) == recordID,
                     "Key has incorrect key length");
#endif
        return NS_ERROR_FAILURE;
    }

    NS_IF_ADDREF(record);
    *retval = NS_STATIC_CAST(nsISupports*, record);
    return NS_OK;
}

NS_IMETHODIMP
nsMemCache::NewCacheEntryIterator(nsISimpleEnumerator* *aIterator)
{
    nsCOMPtr<nsIEnumerator> iterator;

    NS_ENSURE_ARG(aIterator);
    NS_NewHashtableEnumerator(mHashTable, HashEntryConverter,
                              mHashTable, getter_AddRefs(iterator));
    return NS_NewAdapterEnumerator(aIterator, iterator);
}

NS_IMETHODIMP
nsMemCache::GetNextCache(nsINetDataCache* *aNextCache)
{
    NS_ENSURE_ARG(aNextCache);
    *aNextCache = mNextCache;
    NS_IF_ADDREF(*aNextCache);
    return NS_OK;
}

NS_IMETHODIMP
nsMemCache::SetNextCache(nsINetDataCache* aNextCache)
{
    mNextCache = aNextCache;
    return NS_OK;
}

NS_IMETHODIMP
nsMemCache::GetStorageInUse(PRUint32 *aStorageInUse)
{
    NS_ENSURE_ARG(aStorageInUse);

    // Convert from bytes to KB
    *aStorageInUse = (mOccupancy >> 10);
    return NS_OK;
}

NS_IMETHODIMP
nsMemCache::RemoveAll(void)
{
    PRBool failed;

    nsCOMPtr<nsISimpleEnumerator> iterator;
    nsCOMPtr<nsISupports> recordSupports;
    nsCOMPtr<nsINetDataCacheRecord> record;
    nsresult rv;

    failed = PR_FALSE;
    rv = NewCacheEntryIterator(getter_AddRefs(iterator));
    if (NS_FAILED(rv))
        return rv;

    PRBool notDone;
    while (1) {
        rv = iterator->HasMoreElements(&notDone);
        if (NS_FAILED(rv)) return rv;
        if (!notDone)
            break;
        
        iterator->GetNext(getter_AddRefs(recordSupports));
        record = do_QueryInterface(recordSupports);
        recordSupports = 0;

        PRUint32 bytesUsed;
        record->GetStoredContentLength(&bytesUsed);
        rv = record->Delete();
        if (NS_FAILED(rv)) {
            failed = PR_TRUE;
            continue;
        }
    }

    if (failed)
        return NS_ERROR_FAILURE;
    return NS_OK;
}
