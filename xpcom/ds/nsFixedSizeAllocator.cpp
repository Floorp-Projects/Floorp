/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  Implementation for nsFixedSizeAllocator

*/

#include "nsCRT.h"
#include "nsFixedSizeAllocator.h"

nsFixedSizeAllocator::Bucket *
nsFixedSizeAllocator::AddBucket(size_t aSize)
{
    void* p;
    PL_ARENA_ALLOCATE(p, &mPool, sizeof(Bucket));
    if (! p)
        return nsnull;

    Bucket* bucket = static_cast<Bucket*>(p);
    bucket->mSize  = aSize;
    bucket->mFirst = nsnull;
    bucket->mNext  = mBuckets;

    mBuckets = bucket;
    return bucket;
}

nsresult
nsFixedSizeAllocator::Init(const char* aName,
                           const size_t* aBucketSizes,
                           PRInt32 aNumBuckets,
                           PRInt32 aInitialSize,
                           PRInt32 aAlign)
{
    NS_PRECONDITION(aNumBuckets > 0, "no buckets");
    if (aNumBuckets <= 0)
        return NS_ERROR_INVALID_ARG;

    // Blow away the old pool if we're being re-initialized.
    if (mBuckets)
        PL_FinishArenaPool(&mPool);

    PRInt32 bucketspace = aNumBuckets * sizeof(Bucket);
    PL_InitArenaPool(&mPool, aName, bucketspace + aInitialSize, aAlign);

    mBuckets = nsnull;
    for (PRInt32 i = 0; i < aNumBuckets; ++i)
        AddBucket(aBucketSizes[i]);

    return NS_OK;
}

nsFixedSizeAllocator::Bucket *
nsFixedSizeAllocator::FindBucket(size_t aSize)
{
    Bucket** link = &mBuckets;
    Bucket* bucket;

    while ((bucket = *link) != nsnull) {
        if (aSize == bucket->mSize) {
            // Promote to the head of the list, under the assumption
            // that we'll allocate same-sized object contemporaneously.
            *link = bucket->mNext;
            bucket->mNext = mBuckets;
            mBuckets = bucket;
            return bucket;
        }

        link = &bucket->mNext;
    }
    return nsnull;
}

void*
nsFixedSizeAllocator::Alloc(size_t aSize)
{
    Bucket* bucket = FindBucket(aSize);
    if (! bucket) {
        // Oops, we don't carry that size. Let's fix that.
        bucket = AddBucket(aSize);
        if (! bucket)
            return nsnull;
    }

    void* next;
    if (bucket->mFirst) {
        next = bucket->mFirst;
        bucket->mFirst = bucket->mFirst->mNext;
    }
    else {
        PL_ARENA_ALLOCATE(next, &mPool, aSize);
        if (!next)
            return nsnull;
    }

#ifdef DEBUG
    memset(next, 0xc8, aSize);
#endif

    return next;
}

void
nsFixedSizeAllocator::Free(void* aPtr, size_t aSize)
{
    FreeEntry* entry = reinterpret_cast<FreeEntry*>(aPtr);
    Bucket* bucket = FindBucket(aSize);

#ifdef DEBUG
    NS_ASSERTION(bucket && bucket->mSize == aSize, "ack! corruption! bucket->mSize != aSize!");
    memset(aPtr, 0xd8, bucket->mSize);
#endif

    entry->mNext = bucket->mFirst;
    bucket->mFirst = entry;
}
