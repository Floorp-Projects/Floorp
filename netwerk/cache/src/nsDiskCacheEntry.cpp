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
 * The Original Code is nsMemoryCacheDevice.cpp, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Patrick C. Beard <beard@netscape.com>
 */

#include "nsDiskCacheEntry.h"

NS_IMPL_ISUPPORTS0(nsDiskCacheEntry);

/******************************************************************************
 *  nsCacheEntryHashTable
 *****************************************************************************/

PLDHashTableOps nsDiskCacheEntryHashTable::ops =
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

nsDiskCacheEntryHashTable::nsDiskCacheEntryHashTable()
    : initialized(PR_FALSE)
{
}


nsDiskCacheEntryHashTable::~nsDiskCacheEntryHashTable()
{
    if (initialized)
        PL_DHashTableFinish(&table);
}


nsresult
nsDiskCacheEntryHashTable::Init()
{
    nsresult rv = NS_OK;
    initialized = PL_DHashTableInit(&table, &ops, nsnull,
                                    sizeof(HashTableEntry), 512);

    if (!initialized) rv = NS_ERROR_OUT_OF_MEMORY;
    
    return rv;
}


nsDiskCacheEntry *
nsDiskCacheEntryHashTable::GetEntry(const char * key)
{
    return GetEntry(::PL_DHashStringKey(NULL, key));
}


nsDiskCacheEntry *
nsDiskCacheEntryHashTable::GetEntry(PLDHashNumber key)
{
    nsDiskCacheEntry * result = nsnull;
    NS_ASSERTION(initialized, "nsDiskCacheEntryHashTable not initialized");
    HashTableEntry * hashEntry;
    hashEntry = (HashTableEntry*) PL_DHashTableOperate(&table, (void*) key, PL_DHASH_LOOKUP);
    if (PL_DHASH_ENTRY_IS_BUSY(hashEntry)) {
        result = hashEntry->mDiskCacheEntry;
    }
    return result;
}


nsresult
nsDiskCacheEntryHashTable::AddEntry(nsDiskCacheEntry * entry)
{
    NS_ENSURE_ARG_POINTER(entry);
    NS_ASSERTION(initialized, "nsDiskCacheEntryHashTable not initialized");

    HashTableEntry * hashEntry;
    hashEntry = (HashTableEntry *) PL_DHashTableOperate(&table,
                                                        (void*) entry->getHashNumber(),
                                                        PL_DHASH_ADD);
    if (!hashEntry) return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(hashEntry->mDiskCacheEntry = entry);

    return NS_OK;
}


void
nsDiskCacheEntryHashTable::RemoveEntry(nsDiskCacheEntry * entry)
{
    NS_ASSERTION(initialized, "nsDiskCacheEntryHashTable not initialized");
    NS_ASSERTION(entry, "### cacheEntry == nsnull");

    (void) PL_DHashTableOperate(&table, (void*) entry->getHashNumber(), PL_DHASH_REMOVE);
}


void
nsDiskCacheEntryHashTable::VisitEntries(Visitor *visitor)
{
    PL_DHashTableEnumerate(&table, VisitEntry, visitor);
}


PLDHashOperator PR_CALLBACK
nsDiskCacheEntryHashTable::VisitEntry(PLDHashTable *        table,
                                      PLDHashEntryHdr *     header,
                                      PRUint32              number,
                                      void *                arg)
{
    HashTableEntry* hashEntry = (HashTableEntry *) header;
    Visitor *visitor = (Visitor*) arg;
    return (visitor->VisitEntry(hashEntry->mDiskCacheEntry) ? PL_DHASH_NEXT : PL_DHASH_STOP);
}

/**
 *  hash table operation callback functions
 */
const void * PR_CALLBACK
nsDiskCacheEntryHashTable::GetKey(PLDHashTable * /*table*/, PLDHashEntryHdr * header)
{
    HashTableEntry * hashEntry = (HashTableEntry *) header;
    return (void*) hashEntry->mDiskCacheEntry->getHashNumber();
}


PLDHashNumber PR_CALLBACK
nsDiskCacheEntryHashTable::HashKey( PLDHashTable *table, const void *key)
{
    return (PLDHashNumber) key;
}

PRBool PR_CALLBACK
nsDiskCacheEntryHashTable::MatchEntry(PLDHashTable *             /* table */,
                                      const PLDHashEntryHdr *       header,
                                      const void *                  key)
{
    HashTableEntry * hashEntry = (HashTableEntry *) header;
    return (hashEntry->mDiskCacheEntry->getHashNumber() == (PLDHashNumber) key);
}

void PR_CALLBACK
nsDiskCacheEntryHashTable::MoveEntry(PLDHashTable *              /* table */,
                                 const PLDHashEntryHdr *            fromHeader,
                                 PLDHashEntryHdr       *            toHeader)
{
    HashTableEntry * fromEntry = (HashTableEntry *) fromHeader;
    HashTableEntry * toEntry = (HashTableEntry *) toHeader;
    toEntry->keyHash = fromEntry->keyHash;
    toEntry->mDiskCacheEntry = fromEntry->mDiskCacheEntry;
    fromEntry->mDiskCacheEntry = nsnull;
}


void PR_CALLBACK
nsDiskCacheEntryHashTable::ClearEntry(PLDHashTable *             /* table */,
                                      PLDHashEntryHdr *             header)
{
    HashTableEntry* hashEntry = (HashTableEntry *) header;
    hashEntry->keyHash = 0;
    NS_IF_RELEASE(hashEntry->mDiskCacheEntry);
}


void PR_CALLBACK
nsDiskCacheEntryHashTable::Finalize(PLDHashTable * table)
{
    (void) PL_DHashTableEnumerate(table, FreeCacheEntries, nsnull);
}


PLDHashOperator PR_CALLBACK
nsDiskCacheEntryHashTable::FreeCacheEntries(PLDHashTable *             /* table */,
                                            PLDHashEntryHdr *             header,
                                            PRUint32                      number,
                                            void *                        arg)
{
    HashTableEntry *entry = (HashTableEntry *) header;
    NS_IF_RELEASE(entry->mDiskCacheEntry);
    return PL_DHASH_NEXT;
}
