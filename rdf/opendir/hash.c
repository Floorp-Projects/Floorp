/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

int catCount = 0;
int itemCount = 0;

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
	if (startsWith("http://", key)) {
		itemCount++;
	} else {
		catCount++;
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

	

