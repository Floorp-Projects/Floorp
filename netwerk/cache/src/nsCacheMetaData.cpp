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
#include "nsICacheEntryDescriptor.h"


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
    : initialized(PR_FALSE)
{
}

nsCacheMetaData::~nsCacheMetaData()
{
    if (initialized)
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


const nsACString *
nsCacheMetaData::GetElement(const nsACString * key)
{
    PLDHashEntryHdr * hashEntry;
    nsCString *       result = nsnull;

    // XXX need to copy string until we have scc's new flat string abstract class
    // XXX see nsCacheMetaData::HashKey below (bug 70075)
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
nsCacheMetaData::SetElement(const nsACString& key,
                            const nsACString& value)
{
    nsCacheMetaDataHashTableEntry * metaEntry;
    nsresult  rv = NS_ERROR_OUT_OF_MEMORY;  // presume the worst

    NS_ASSERTION(initialized, "nsCacheMetaDataHashTable not initialized");

    // XXX need to copy string until we have scc's new flat string abstract class
    // XXX see nsCacheMetaData::HashKey below (bug 70075)
    nsCString * tempKey = new nsCString(key);
    if (!tempKey) return rv;

    // XXX should empty value remove the key?

    metaEntry = (nsCacheMetaDataHashTableEntry *)
        PL_DHashTableOperate(&table, tempKey, PL_DHASH_ADD);
    if (!metaEntry) goto error_exit;


    if (metaEntry->key == nsnull) {
        metaEntry->key = new nsCString(key);
        if (metaEntry->key == nsnull) {
            goto error_exit;
        }
    }
    if (metaEntry->value != nsnull)
        delete metaEntry->value;  // clear the old value

    metaEntry->value = new nsCString(value);
    if (metaEntry->value == nsnull) {
        // XXX remove key?
        goto error_exit;
    }

    rv = NS_OK;
 error_exit:
    delete tempKey;
    return rv;
}


PRUint32
nsCacheMetaData::Size(void)
{
    PRUint32 size = 0;
    (void) PL_DHashTableEnumerate(&table, CalculateSize, &size);
    return size;
}


typedef struct {
    char *    bufPtr;
    PRUint32  bufSize;
    PRUint32  metaSize;
    nsresult  rv;
} AccumulateStruct;


nsresult
nsCacheMetaData::FlattenMetaData(char * buffer, PRUint32 bufSize)
{
    AccumulateStruct  state = {buffer, bufSize, 0, NS_OK};
    PL_DHashTableEnumerate(&table, AccumulateElement, &state);

    return NS_OK;
}

nsresult
nsCacheMetaData::UnflattenMetaData(char * data, PRUint32 size)
{
    if (size == 0)  return NS_OK;

    nsresult rv = NS_ERROR_UNEXPECTED;
    char* limit = data + size;
    while (data < limit) {
        const char* name = data;
        PRUint32 nameSize = strlen(name);
        data += 1 + nameSize;
        if (data < limit) {
            const char* value = data;
            PRUint32 valueSize = strlen(value);
            data += 1 + valueSize;
            rv = SetElement(nsDependentCString(name, nameSize),
                                   nsDependentCString(value, valueSize));
            if (NS_FAILED(rv)) break;
        }
    }
    return rv;
}


nsresult
nsCacheMetaData::VisitElements(nsICacheMetaDataVisitor * visitor)
{
    (void) PL_DHashTableEnumerate(&table, VisitElement, visitor);
    return NS_OK;
}


/*
 *  hash table operation callback functions
 */

const void * PR_CALLBACK
nsCacheMetaData::GetKey( PLDHashTable * /* table */, PLDHashEntryHdr *hashEntry)
{
    return ((nsCacheMetaDataHashTableEntry *)hashEntry)->key;
}


PLDHashNumber PR_CALLBACK
nsCacheMetaData::HashKey( PLDHashTable * table, const void *key)
{
    // XXX need scc's new flat string abstract class here (bug 70075)
    return PL_DHashStringKey(table, ((nsCString *)key)->get());
}


PRBool PR_CALLBACK
nsCacheMetaData::MatchEntry(PLDHashTable *       /* table */,
				     const PLDHashEntryHdr * hashEntry,
				     const void *            key)
{
    NS_ASSERTION(key !=  nsnull, "### nsCacheMetaDataHashTable::MatchEntry : null key");
    nsCString * entryKey = ((nsCacheMetaDataHashTableEntry *)hashEntry)->key;
    NS_ASSERTION(entryKey, "### hashEntry->key == nsnull");

    return entryKey->Equals(*NS_STATIC_CAST(const nsACString*,key));
}


void PR_CALLBACK
nsCacheMetaData::MoveEntry(PLDHashTable * /* table */,
				    const PLDHashEntryHdr *from,
				    PLDHashEntryHdr       *to)
{
    ((nsCacheMetaDataHashTableEntry *)to)->key =
        ((nsCacheMetaDataHashTableEntry *)from)->key;
    ((nsCacheMetaDataHashTableEntry *)to)->value =
        ((nsCacheMetaDataHashTableEntry *)from)->value;
}


void PR_CALLBACK
nsCacheMetaData::ClearEntry(PLDHashTable * /* table */,
                            PLDHashEntryHdr * hashEntry)
{
    ((nsCacheMetaDataHashTableEntry *)hashEntry)->key      = 0;
    ((nsCacheMetaDataHashTableEntry *)hashEntry)->value    = 0;
}


void PR_CALLBACK
nsCacheMetaData::Finalize(PLDHashTable * table)
{
    (void) PL_DHashTableEnumerate(table, FreeElement, nsnull);   
}


/**
 * hash table enumeration callback functions
 */

PLDHashOperator PR_CALLBACK
nsCacheMetaData::CalculateSize(PLDHashTable *table,
                               PLDHashEntryHdr *hdr,
                               PRUint32 number,
                               void *arg)
{
    nsCacheMetaDataHashTableEntry* hashEntry = (nsCacheMetaDataHashTableEntry *)hdr;
    *(PRUint32*)arg += (2 + hashEntry->key->Length() + hashEntry->value->Length());
    return PL_DHASH_NEXT;
}


PLDHashOperator PR_CALLBACK
nsCacheMetaData::AccumulateElement(PLDHashTable *table,
                                 PLDHashEntryHdr *hdr,
                                 PRUint32 number,
                                 void *arg)
{
    AccumulateStruct * state = (AccumulateStruct *)arg;
    nsCacheMetaDataHashTableEntry* hashEntry = (nsCacheMetaDataHashTableEntry *)hdr;
    PRUint32 keySize = 1 + hashEntry->key->Length();
    PRUint32 valSize = 1 + hashEntry->value->Length();
    if ((state->metaSize + keySize + valSize) > state->bufSize) {
        // not enough space to copy key/value pair
        state->rv = NS_ERROR_OUT_OF_MEMORY;
        NS_ERROR("buffer size too small for meta data.");
        return PL_DHASH_STOP;
    }
    
    memcpy(state->bufPtr, hashEntry->key->get(), keySize);
    state->bufPtr += keySize;
    memcpy(state->bufPtr, hashEntry->value->get(), valSize);
    state->bufPtr += valSize;
    state->metaSize += keySize + valSize;
    return PL_DHASH_NEXT;
}


PLDHashOperator PR_CALLBACK
nsCacheMetaData::FreeElement(PLDHashTable *table,
                              PLDHashEntryHdr *hdr,
                              PRUint32 number,
                              void *arg)
{
    nsCacheMetaDataHashTableEntry *entry = (nsCacheMetaDataHashTableEntry *)hdr;
    delete entry->key;
    delete entry->value;
    return PL_DHASH_NEXT;
}


PLDHashOperator PR_CALLBACK
nsCacheMetaData::VisitElement(PLDHashTable *table,
                              PLDHashEntryHdr *hdr,
                              PRUint32 number,
                              void *arg)
{
    nsCacheMetaDataHashTableEntry *entry   = (nsCacheMetaDataHashTableEntry *)hdr;
    nsICacheMetaDataVisitor       *visitor = (nsICacheMetaDataVisitor *)arg;
    const char * key   = entry->key   ? entry->key->get()   : nsnull;
    const char * value = entry->value ? entry->value->get() : nsnull;
    
    PRBool keepGoing;
    nsresult rv = visitor->VisitMetaDataElement(key, value, &keepGoing);

    return NS_SUCCEEDED(rv) && keepGoing ? PL_DHASH_NEXT : PL_DHASH_STOP;
}
