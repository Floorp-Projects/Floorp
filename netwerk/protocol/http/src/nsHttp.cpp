/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsHttp.h"
#include "nscore.h"
#include "plhash.h"
#include "nsCRT.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *gHttpLog = nsnull;
#endif

// define storage for all atoms
#define HTTP_ATOM(_name, _value) nsHttpAtom nsHttp::_name = { _value };
#include "nsHttpAtomList.h"
#undef HTTP_ATOM

// we keep a linked list of atoms allocated on the heap for easy clean up
// when the atom table is destroyed.
struct HttpHeapAtom {
    char                *value;
    struct HttpHeapAtom *next;

    HttpHeapAtom(const char *v) : value(PL_strdup(v)), next(0) {}
   ~HttpHeapAtom() { PL_strfree(value); }
};

static struct PLHashTable  *gHttpAtomTable = nsnull;
static struct HttpHeapAtom *gHeapAtomsHead = nsnull;
static struct HttpHeapAtom *gHeapAtomsTail = nsnull;

// Hash string ignore case, based on PL_HashString
static PLHashNumber
StringHash(const PRUint8 *key)
{
    PLHashNumber h;
    const PRUint8 *s;

    h = 0;
    for (s = key; *s; s++)
        h = (h >> 28) ^ (h << 4) ^ nsCRT::ToLower((char)*s);
    return h;
}

static PRIntn
StringCompare(const char *a, const char *b)
{
    return PL_strcasecmp(a, b) == 0;
}

#if 0
#define NBUCKETS(ht)    (1 << (PL_HASH_BITS - (ht)->shift))
static void
DumpAtomTable()
{
    if (gHttpAtomTable) {
        PLHashEntry *he, **hep;
        PRUint32 i, nbuckets = NBUCKETS(gHttpAtomTable);
        for (i=0; i<nbuckets; ++i) {
            printf("bucket %d: ", i);
            hep = &gHttpAtomTable->buckets[i];
            while ((he = *hep) != 0) {
                printf("(%s,%x,%x) ", (const char *) he->key, he->keyHash, he->value);
                hep = &he->next;
            }
            printf("\n");
        }
    }
}
#endif

// We put the atoms in a hash table for speedy lookup.. see ResolveAtom.
static nsresult
CreateAtomTable()
{
    LOG(("CreateAtomTable\n"));

    if (gHttpAtomTable)
        return NS_OK;

    gHttpAtomTable = PL_NewHashTable(128, (PLHashFunction) StringHash,
                                          (PLHashComparator) StringCompare,
                                          (PLHashComparator) 0, 0, 0);
    if (!gHttpAtomTable)
        return NS_ERROR_OUT_OF_MEMORY;

#define HTTP_ATOM(_name, _value) \
    PL_HashTableAdd(gHttpAtomTable, _value, (void *) nsHttp::_name.get());
#include "nsHttpAtomList.h"
#undef HTTP_ATOM

    //DumpAtomTable();
    return NS_OK;
}

void
nsHttp::DestroyAtomTable()
{
    if (gHttpAtomTable) {
        PL_HashTableDestroy(gHttpAtomTable);
        gHttpAtomTable = nsnull;
    }
    while (gHeapAtomsHead) {
        gHeapAtomsTail = gHeapAtomsHead->next;
        delete gHeapAtomsHead;
        gHeapAtomsHead = gHeapAtomsTail;
    }
    gHeapAtomsTail = nsnull;
}

nsHttpAtom
nsHttp::ResolveAtom(const char *str)
{
    if (!gHttpAtomTable)
        CreateAtomTable();

    nsHttpAtom atom = { nsnull };

    if (gHttpAtomTable && str) {
        atom._val = (const char *) PL_HashTableLookup(gHttpAtomTable, str);

        // if the atom could not be found in the atom table, then we'll go
        // and allocate a new atom on the heap.
        if (!atom) {
            HttpHeapAtom *heapAtom = new HttpHeapAtom(str);
            if (!heapAtom)
                return atom;
            if (!heapAtom->value) {
                delete heapAtom;
                return atom;
            }

            // append this heap atom to the list of all heap atoms
            if (!gHeapAtomsHead) {
                gHeapAtomsHead = heapAtom;
                gHeapAtomsTail = heapAtom;
            }
            else {
                gHeapAtomsTail->next = heapAtom;
                gHeapAtomsTail = heapAtom;
            }

            // now insert the heap atom into the atom table
            PL_HashTableAdd(gHttpAtomTable, heapAtom->value, heapAtom->value);

            // now assign the value to the atom
            atom._val = (const char *) heapAtom->value;
        }
    }

    return atom;
}
