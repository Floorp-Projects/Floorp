/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* This file holds a temporary implementation of hash tables. It will
   be replaced with STL or NSPR
 */

#include "rdf-int.h"

typedef struct _HashEntryStruct {
    struct _HashEntryStruct*         next;          /* hash chain linkage */
    char*   key;
    void* value;
} HashEntryStruct;

typedef HashEntryStruct* HashEntry;

typedef struct _HashTableStruct {
    int                size;
    HashEntry*         buckets;      /* vector of hash buckets */
} HashTableStruct;



int 
hashKey (HashTable ht, char* key) {
    size_t len = strlen(key);
    int    sum = 0;
    size_t    n = 0;
	int ans;
    for (n = 0; n < len; n++) sum = sum + (int)key[n];
    ans = sum & ht->size;
	if (ans == ht->size) ans = ans-1;
	return ans; 
}

HashTable 
NewHashTable(int size) {
    HashTable ht = (HashTable)getMem(sizeof(HashTableStruct));
    ht->size = size;
    ht->buckets = (HashEntry*)getMem(sizeof(HashEntry) * size);
    return ht;
}

void*
HashLookup(HashTable ht, char* key) {
    int offset = hashKey(ht, key);
    HashEntry he = ht->buckets[offset];
    while (he) {
	if (strcmp(he->key, key) == 0) return he->value;
	he = he->next;
    }
    return NULL;
}

void 
HashAdd (HashTable ht, char* key, void* value) {
    int offset = hashKey(ht, key);
    HashEntry he = ht->buckets[offset];
    HashEntry prev = he;
    while (he) {
	if (strcmp(he->key, key) == 0) {
	    if (value == he->value) {
		return;
	    } else {
		he->value = value;
		return;
	    }
	}
	prev = he;
	he = he->next;
    }
    he = (HashEntry) fgetMem(sizeof(HashEntryStruct));
    he->value = value;
    he->key   = key;
    if (prev) {
      prev->next = he;
    } else {
      ht->buckets[offset] = he;
    }
}

	

