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
 * The Original Code is nsCacheMetaData.cpp, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */

#include "nsCacheMetaData.h"
#include "nsString.h"



/*
 *  nsCacheClientHashTable
 */

PLDHashTableOps
nsCacheMetaData::ops =
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


nsCacheMetaData::nsCacheMetaData()
    : initialized(0)
{
}

nsCacheMetaData::~nsCacheMetaData()
{
    //** maybe we should finalize the table...
    PL_DHashTableFinish(&table);
}


nsresult
nsCacheMetaData::Init()
{
    nsresult rv = NS_OK;
    initialized = PL_DHashTableInit(&table, &ops, nsnull,
                                    sizeof(nsCacheMetaDataHashTableEntry), 16);

    if (!initialized) rv = NS_ERROR_OUT_OF_MEMORY;
    return rv;
}


nsCacheMetaData *
nsCacheMetaData::Create()
{
    nsCacheMetaData * metaData = new nsCacheMetaData();
    if (!metaData)
        return nsnull;

    nsresult rv = metaData->Init();
    if (NS_FAILED(rv)) {
        delete metaData;
        return nsnull;
    }

    return metaData;
}


nsAReadableCString *
nsCacheMetaData::GetElement(const nsAReadableCString * key)
{
    PLDHashEntryHdr * hashEntry;
    nsCString *       result = nsnull;

    //** need to copy string until we have scc's new flat string abstract class
    //** see nsCacheMetaData::HashKey below (bug 70075)
    nsCString * tempKey = new nsCString(*key);
    if (!tempKey) return result;

    NS_ASSERTION(initialized, "nsCacheMetaDataHashTable not initialized");
    hashEntry = PL_DHashTableOperate(&table, tempKey, PL_DHASH_LOOKUP);
    if (PL_DHASH_ENTRY_IS_BUSY(hashEntry)) {
        result = ((nsCacheMetaDataHashTableEntry *)hashEntry)->value;
    }

    delete tempKey;
    return result;
}


nsresult
nsCacheMetaData::SetElement(const nsAReadableCString& key,
                            const nsAReadableCString& value)
{
    nsCacheMetaDataHashTableEntry * metaEntry;

    NS_ASSERTION(initialized, "nsCacheMetaDataHashTable not initialized");

    //** need to copy string until we have scc's new flat string abstract class
    //** see nsCacheMetaData::HashKey below (bug 70075)
    nsCString * tempKey = new nsCString(key);
    if (!tempKey) return NS_ERROR_OUT_OF_MEMORY;

    //** should empty value remove the key?

    metaEntry = (nsCacheMetaDataHashTableEntry *)
        PL_DHashTableOperate(&table, tempKey, PL_DHASH_ADD);
    if (metaEntry->key == nsnull) {
        metaEntry->key = new nsCString(key);
        if (metaEntry->key == nsnull) {
            delete tempKey;
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    if (metaEntry->value != nsnull)
        delete metaEntry->value;

    metaEntry->value = new nsCString(value);
    if (metaEntry->value == nsnull) {
        //** remove key?
        delete tempKey;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    delete tempKey;
    return NS_OK;
}


//** enumerate MetaData elements
nsresult
nsCacheMetaData::GetKeyValueArray(nsCacheMetaDataKeyValuePair ** array,
                                  PRUint32 *                     count)
{
    // count elements
    PRUint32  total = 0;
    PRUint32  totalEntries = PL_DHashTableEnumerate(&table, CountElements, &total);

    if (total != totalEntries) {
        //** just checking
    }
    // allocate array

    // fill array

    return NS_ERROR_NOT_IMPLEMENTED;
}



/*
 *  hash table operation callback functions
 */

const void *
nsCacheMetaData::GetKey( PLDHashTable * /* table */, PLDHashEntryHdr *hashEntry)
{
    return ((nsCacheMetaDataHashTableEntry *)hashEntry)->key;
}


PLDHashNumber
nsCacheMetaData::HashKey( PLDHashTable * table, const void *key)
{
    //** need scc's new flat string abstract class here (bug 70075)
    return PL_DHashStringKey(table, ((nsCString *)key)->get());
}


PRBool
nsCacheMetaData::MatchEntry(PLDHashTable *       /* table */,
				     const PLDHashEntryHdr * hashEntry,
				     const void *            key)
{
    NS_ASSERTION(key !=  nsnull, "nsCacheMetaDataHashTable::MatchEntry : null key");
    nsCString * entryKey = ((nsCacheMetaDataHashTableEntry *)hashEntry)->key;
    NS_ASSERTION(entryKey, "hashEntry->key == nsnull");

    return entryKey->Equals(*NS_STATIC_CAST(const nsAReadableCString*,key));
}


void
nsCacheMetaData::MoveEntry(PLDHashTable * /* table */,
				    const PLDHashEntryHdr *from,
				    PLDHashEntryHdr       *to)
{
    to->keyHash = from->keyHash;
    ((nsCacheMetaDataHashTableEntry *)to)->key =
        ((nsCacheMetaDataHashTableEntry *)from)->key;
    ((nsCacheMetaDataHashTableEntry *)to)->value =
        ((nsCacheMetaDataHashTableEntry *)from)->value;
}


void
nsCacheMetaData::ClearEntry(PLDHashTable * /* table */,
                            PLDHashEntryHdr * hashEntry)
{
    ((nsCacheMetaDataHashTableEntry *)hashEntry)->keyHash  = 0;
    ((nsCacheMetaDataHashTableEntry *)hashEntry)->key      = 0;
    ((nsCacheMetaDataHashTableEntry *)hashEntry)->value    = 0;
}


void
nsCacheMetaData::Finalize(PLDHashTable * table)
{
    (void) PL_DHashTableEnumerate(table, FreeElements, nsnull);   
}


/**
 * hash table enumeration callback functions
 */

PLDHashOperator
nsCacheMetaData::CountElements(PLDHashTable *table,
                               PLDHashEntryHdr *hdr,
                               PRUint32 number,
                               void *arg)
{
    ++*(PRUint32 *)arg;
    return PL_DHASH_NEXT;
}


PLDHashOperator
nsCacheMetaData::AccumulateElements(PLDHashTable *table,
                                    PLDHashEntryHdr *hdr,
                                    PRUint32 number,
                                    void *arg)
{
    nsCacheMetaDataHashTableEntry *entry = (nsCacheMetaDataHashTableEntry *)hdr;
    nsCacheMetaDataKeyValuePair *pair    = (nsCacheMetaDataKeyValuePair *)arg;

    pair->key   = entry->key;
    pair->value = entry->value;
    return PL_DHASH_NEXT;
}


PLDHashOperator
nsCacheMetaData::FreeElements(PLDHashTable *table,
                              PLDHashEntryHdr *hdr,
                              PRUint32 number,
                              void *arg)
{
    nsCacheMetaDataHashTableEntry *entry = (nsCacheMetaDataHashTableEntry *)hdr;
    delete entry->key;
    delete entry->value;
    return PL_DHASH_NEXT;
}
