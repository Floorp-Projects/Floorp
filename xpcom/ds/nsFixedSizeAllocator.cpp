/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Chris Waterson <waterson@netscape.com
 *
 * Contributor(s): 
 */

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

    Bucket* bucket = NS_STATIC_CAST(Bucket*, p);
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
    nsCRT::memset(next, 0xc8, aSize);
#endif

    return next;
}

void
nsFixedSizeAllocator::Free(void* aPtr, size_t aSize)
{
    FreeEntry* entry = NS_REINTERPRET_CAST(FreeEntry*, aPtr);
    Bucket* bucket = FindBucket(aSize);

#ifdef DEBUG
    NS_ASSERTION(bucket && bucket->mSize == aSize, "ack! corruption! bucket->mSize != aSize!");
    nsCRT::memset(aPtr, 0xd8, bucket->mSize);
#endif

    entry->mNext = bucket->mFirst;
    bucket->mFirst = entry;
}
