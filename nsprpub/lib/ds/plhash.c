/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

/*
 * PL hash table package.
 */
#include "plhash.h"
#include "prbit.h"
#include "prlog.h"
#include "prmem.h"
#include "prtypes.h"
#include <stdlib.h>
#include <string.h>

/* Compute the number of buckets in ht */
#define NBUCKETS(ht)    (1 << (PL_HASH_BITS - (ht)->shift))

/* The smallest table has 16 buckets */
#define MINBUCKETSLOG2  4
#define MINBUCKETS      (1 << MINBUCKETSLOG2)

/* Compute the maximum entries given n buckets that we will tolerate, ~90% */
#define OVERLOADED(n)   ((n) - ((n) >> 3))

/* Compute the number of entries below which we shrink the table by half */
#define UNDERLOADED(n)  (((n) > MINBUCKETS) ? ((n) >> 2) : 0)

/*
** Stubs for default hash allocator ops.
*/
static void * PR_CALLBACK
DefaultAllocTable(void *pool, PRSize size)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    return PR_MALLOC(size);
}

static void PR_CALLBACK
DefaultFreeTable(void *pool, void *item)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    PR_Free(item);
}

static PLHashEntry * PR_CALLBACK
DefaultAllocEntry(void *pool, const void *key)
{
#if defined(XP_MAC)
#pragma unused (pool,key)
#endif

    return PR_NEW(PLHashEntry);
}

static void PR_CALLBACK
DefaultFreeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
#if defined(XP_MAC)
#pragma unused (pool)
#endif

    if (flag == HT_FREE_ENTRY)
        PR_Free(he);
}

static PLHashAllocOps defaultHashAllocOps = {
    DefaultAllocTable, DefaultFreeTable,
    DefaultAllocEntry, DefaultFreeEntry
};

PR_IMPLEMENT(PLHashTable *)
PL_NewHashTable(PRUint32 n, PLHashFunction keyHash,
                PLHashComparator keyCompare, PLHashComparator valueCompare,
                const PLHashAllocOps *allocOps, void *allocPriv)
{
    PLHashTable *ht;
    PRSize nb;

    if (n <= MINBUCKETS) {
        n = MINBUCKETSLOG2;
    } else {
        n = PR_CeilingLog2(n);
        if ((PRInt32)n < 0)
            return 0;
    }

    if (!allocOps) allocOps = &defaultHashAllocOps;

    ht = (PLHashTable*)((*allocOps->allocTable)(allocPriv, sizeof *ht));
    if (!ht)
	return 0;
    memset(ht, 0, sizeof *ht);
    ht->shift = PL_HASH_BITS - n;
    n = 1 << n;
#if defined(WIN16)
    if (n > 16000) {
        (*allocOps->freeTable)(allocPriv, ht);
        return 0;
    }
#endif  /* WIN16 */
    nb = n * sizeof(PLHashEntry *);
    ht->buckets = (PLHashEntry**)((*allocOps->allocTable)(allocPriv, nb));
    if (!ht->buckets) {
        (*allocOps->freeTable)(allocPriv, ht);
        return 0;
    }
    memset(ht->buckets, 0, nb);

    ht->keyHash = keyHash;
    ht->keyCompare = keyCompare;
    ht->valueCompare = valueCompare;
    ht->allocOps = allocOps;
    ht->allocPriv = allocPriv;
    return ht;
}

PR_IMPLEMENT(void)
PL_HashTableDestroy(PLHashTable *ht)
{
    PRUint32 i, n;
    PLHashEntry *he, *next;
    const PLHashAllocOps *allocOps = ht->allocOps;
    void *allocPriv = ht->allocPriv;

    n = NBUCKETS(ht);
    for (i = 0; i < n; i++) {
        for (he = ht->buckets[i]; he; he = next) {
            next = he->next;
            (*allocOps->freeEntry)(allocPriv, he, HT_FREE_ENTRY);
        }
    }
#ifdef DEBUG
    memset(ht->buckets, 0xDB, n * sizeof ht->buckets[0]);
#endif
    (*allocOps->freeTable)(allocPriv, ht->buckets);
#ifdef DEBUG
    memset(ht, 0xDB, sizeof *ht);
#endif
    (*allocOps->freeTable)(allocPriv, ht);
}

/*
** Multiplicative hash, from Knuth 6.4.
*/
#define GOLDEN_RATIO    0x9E3779B9U

PR_IMPLEMENT(PLHashEntry **)
PL_HashTableRawLookup(PLHashTable *ht, PLHashNumber keyHash, const void *key)
{
    PLHashEntry *he, **hep, **hep0;
    PLHashNumber h;

#ifdef HASHMETER
    ht->nlookups++;
#endif
    h = keyHash * GOLDEN_RATIO;
    h >>= ht->shift;
    hep = hep0 = &ht->buckets[h];
    while ((he = *hep) != 0) {
        if (he->keyHash == keyHash && (*ht->keyCompare)(key, he->key)) {
            /* Move to front of chain if not already there */
            if (hep != hep0) {
                *hep = he->next;
                he->next = *hep0;
                *hep0 = he;
            }
            return hep0;
        }
        hep = &he->next;
#ifdef HASHMETER
        ht->nsteps++;
#endif
    }
    return hep;
}

PR_IMPLEMENT(PLHashEntry *)
PL_HashTableRawAdd(PLHashTable *ht, PLHashEntry **hep,
                   PLHashNumber keyHash, const void *key, void *value)
{
    PRUint32 i, n;
    PLHashEntry *he, *next, **oldbuckets;
    PRSize nb;

    /* Grow the table if it is overloaded */
    n = NBUCKETS(ht);
    if (ht->nentries >= OVERLOADED(n)) {
#ifdef HASHMETER
        ht->ngrows++;
#endif
        ht->shift--;
        oldbuckets = ht->buckets;
#if defined(WIN16)
        if (2 * n > 16000)
            return 0;
#endif  /* WIN16 */
        nb = 2 * n * sizeof(PLHashEntry *);
        ht->buckets = (PLHashEntry**)
            ((*ht->allocOps->allocTable)(ht->allocPriv, nb));
        if (!ht->buckets) {
            ht->buckets = oldbuckets;
            return 0;
        }
        memset(ht->buckets, 0, nb);

        for (i = 0; i < n; i++) {
            for (he = oldbuckets[i]; he; he = next) {
                next = he->next;
                hep = PL_HashTableRawLookup(ht, he->keyHash, he->key);
                PR_ASSERT(*hep == 0);
                he->next = 0;
                *hep = he;
            }
        }
#ifdef DEBUG
        memset(oldbuckets, 0xDB, n * sizeof oldbuckets[0]);
#endif
        (*ht->allocOps->freeTable)(ht->allocPriv, oldbuckets);
        hep = PL_HashTableRawLookup(ht, keyHash, key);
    }

    /* Make a new key value entry */
    he = (*ht->allocOps->allocEntry)(ht->allocPriv, key);
    if (!he)
	return 0;
    he->keyHash = keyHash;
    he->key = key;
    he->value = value;
    he->next = *hep;
    *hep = he;
    ht->nentries++;
    return he;
}

PR_IMPLEMENT(PLHashEntry *)
PL_HashTableAdd(PLHashTable *ht, const void *key, void *value)
{
    PLHashNumber keyHash;
    PLHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key);
    hep = PL_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) != 0) {
        /* Hit; see if values match */
        if ((*ht->valueCompare)(he->value, value)) {
            /* key,value pair is already present in table */
            return he;
        }
        if (he->value)
            (*ht->allocOps->freeEntry)(ht->allocPriv, he, HT_FREE_VALUE);
        he->value = value;
        return he;
    }
    return PL_HashTableRawAdd(ht, hep, keyHash, key, value);
}

PR_IMPLEMENT(void)
PL_HashTableRawRemove(PLHashTable *ht, PLHashEntry **hep, PLHashEntry *he)
{
    PRUint32 i, n;
    PLHashEntry *next, **oldbuckets;
    PRSize nb;

    *hep = he->next;
    (*ht->allocOps->freeEntry)(ht->allocPriv, he, HT_FREE_ENTRY);

    /* Shrink table if it's underloaded */
    n = NBUCKETS(ht);
    if (--ht->nentries < UNDERLOADED(n)) {
#ifdef HASHMETER
        ht->nshrinks++;
#endif
        ht->shift++;
        oldbuckets = ht->buckets;
        nb = n * sizeof(PLHashEntry*) / 2;
        ht->buckets = (PLHashEntry**)(
            (*ht->allocOps->allocTable)(ht->allocPriv, nb));
        if (!ht->buckets) {
            ht->buckets = oldbuckets;
            return;
        }
        memset(ht->buckets, 0, nb);

        for (i = 0; i < n; i++) {
            for (he = oldbuckets[i]; he; he = next) {
                next = he->next;
                hep = PL_HashTableRawLookup(ht, he->keyHash, he->key);
                PR_ASSERT(*hep == 0);
                he->next = 0;
                *hep = he;
            }
        }
#ifdef DEBUG
        memset(oldbuckets, 0xDB, n * sizeof oldbuckets[0]);
#endif
        (*ht->allocOps->freeTable)(ht->allocPriv, oldbuckets);
    }
}

PR_IMPLEMENT(PRBool)
PL_HashTableRemove(PLHashTable *ht, const void *key)
{
    PLHashNumber keyHash;
    PLHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key);
    hep = PL_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) == 0)
        return PR_FALSE;

    /* Hit; remove element */
    PL_HashTableRawRemove(ht, hep, he);
    return PR_TRUE;
}

PR_IMPLEMENT(void *)
PL_HashTableLookup(PLHashTable *ht, const void *key)
{
    PLHashNumber keyHash;
    PLHashEntry *he, **hep;

    keyHash = (*ht->keyHash)(key);
    hep = PL_HashTableRawLookup(ht, keyHash, key);
    if ((he = *hep) != 0) {
        return he->value;
    }
    return 0;
}

/*
** Iterate over the entries in the hash table calling func for each
** entry found. Stop if "f" says to (return value & PR_ENUMERATE_STOP).
** Return a count of the number of elements scanned.
*/
PR_IMPLEMENT(int)
PL_HashTableEnumerateEntries(PLHashTable *ht, PLHashEnumerator f, void *arg)
{
    PLHashEntry *he, **hep;
    PRUint32 i, nbuckets;
    int rv, n = 0;
    PLHashEntry *todo = 0;

    nbuckets = NBUCKETS(ht);
    for (i = 0; i < nbuckets; i++) {
        hep = &ht->buckets[i];
        while ((he = *hep) != 0) {
            rv = (*f)(he, n, arg);
            n++;
            if (rv & (HT_ENUMERATE_REMOVE | HT_ENUMERATE_UNHASH)) {
                *hep = he->next;
                if (rv & HT_ENUMERATE_REMOVE) {
                    he->next = todo;
                    todo = he;
                }
            } else {
                hep = &he->next;
            }
            if (rv & HT_ENUMERATE_STOP) {
                goto out;
            }
        }
    }

out:
    hep = &todo;
    while ((he = *hep) != 0) {
        PL_HashTableRawRemove(ht, hep, he);
    }
    return n;
}

#ifdef HASHMETER
#include <math.h>
#include <stdio.h>

PR_IMPLEMENT(void)
PL_HashTableDumpMeter(PLHashTable *ht, PLHashEnumerator dump, FILE *fp)
{
    double mean, variance;
    PRUint32 nchains, nbuckets;
    PRUint32 i, n, maxChain, maxChainLen;
    PLHashEntry *he;

    variance = 0;
    nchains = 0;
    maxChainLen = 0;
    nbuckets = NBUCKETS(ht);
    for (i = 0; i < nbuckets; i++) {
        he = ht->buckets[i];
        if (!he)
            continue;
        nchains++;
        for (n = 0; he; he = he->next)
            n++;
        variance += n * n;
        if (n > maxChainLen) {
            maxChainLen = n;
            maxChain = i;
        }
    }
    mean = (double)ht->nentries / nchains;
    variance = fabs(variance / nchains - mean * mean);

    fprintf(fp, "\nHash table statistics:\n");
    fprintf(fp, "     number of lookups: %u\n", ht->nlookups);
    fprintf(fp, "     number of entries: %u\n", ht->nentries);
    fprintf(fp, "       number of grows: %u\n", ht->ngrows);
    fprintf(fp, "     number of shrinks: %u\n", ht->nshrinks);
    fprintf(fp, "   mean steps per hash: %g\n", (double)ht->nsteps
                                                / ht->nlookups);
    fprintf(fp, "mean hash chain length: %g\n", mean);
    fprintf(fp, "    standard deviation: %g\n", sqrt(variance));
    fprintf(fp, " max hash chain length: %u\n", maxChainLen);
    fprintf(fp, "        max hash chain: [%u]\n", maxChain);

    for (he = ht->buckets[maxChain], i = 0; he; he = he->next, i++)
        if ((*dump)(he, i, fp) != HT_ENUMERATE_NEXT)
            break;
}
#endif /* HASHMETER */

PR_IMPLEMENT(int)
PL_HashTableDump(PLHashTable *ht, PLHashEnumerator dump, FILE *fp)
{
    int count;

    count = PL_HashTableEnumerateEntries(ht, dump, fp);
#ifdef HASHMETER
    PL_HashTableDumpMeter(ht, dump, fp);
#endif
    return count;
}

PR_IMPLEMENT(PLHashNumber)
PL_HashString(const void *key)
{
    PLHashNumber h;
    const PRUint8 *s;

    h = 0;
    for (s = (const PRUint8*)key; *s; s++)
        h = (h >> 28) ^ (h << 4) ^ *s;
    return h;
}

PR_IMPLEMENT(int)
PL_CompareStrings(const void *v1, const void *v2)
{
    return strcmp((const char*)v1, (const char*)v2) == 0;
}

PR_IMPLEMENT(int)
PL_CompareValues(const void *v1, const void *v2)
{
    return v1 == v2;
}
