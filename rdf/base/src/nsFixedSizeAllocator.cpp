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

nsresult
nsFixedSizeAllocator::Init(const char* aName,
                           size_t* aBucketSizes,
                           PRInt32 aNumBuckets,
                           PRInt32 aInitialSize,
                           PRInt32 aAlign)
{
    NS_PRECONDITION(aNumBuckets > 0, "no buckets");
    if (aNumBuckets <= 0)
        return NS_ERROR_INVALID_ARG;

    mNumBuckets = aNumBuckets;

    PRInt32 bucketspace = aNumBuckets * sizeof(Bucket);
    PL_InitArenaPool(&mPool, aName, bucketspace + aInitialSize, aAlign);

    void* p;
    PL_ARENA_ALLOCATE(p, &mPool, bucketspace);
    if (! p)
        return NS_ERROR_OUT_OF_MEMORY;

    mBuckets = NS_STATIC_CAST(Bucket*, p);

    for (PRInt32 i = 0; i < aNumBuckets; ++i) {
        mBuckets[i].mSize  = aBucketSizes[i];
        mBuckets[i].mFirst = nsnull;
    }

    return NS_OK;
}

void*
nsFixedSizeAllocator::Alloc(size_t aSize)
{
    Bucket* bucket = mBuckets;
    for (PRInt32 i = mNumBuckets; i >= 0; --i, ++bucket) {
        if (aSize != bucket->mSize) 
            continue;

        void* next;
        if (bucket->mFirst) {
            next = bucket->mFirst;
            bucket->mFirst = bucket->mFirst->mNext;
        }
        else {
            // Allocate headroom so we can have a backpointer. Use
            // 'sizeof double' so that the block stays well-aligned
            PL_ARENA_ALLOCATE(next, &mPool, aSize + sizeof(double));

            FreeEntry* entry = NS_STATIC_CAST(FreeEntry*, next);
            entry->mBucket = bucket;
        }

        next = NS_STATIC_CAST(void*, NS_STATIC_CAST(char*, next) + sizeof(double));

#ifdef DEBUG
        nsCRT::memset(next, 0xc8, bucket->mSize);
#endif

        return next;
    }

    // Oops, we don't carry that size.
    NS_ERROR("attempt to allocate bad size from fixed size allocator");
    return 0;
}



void
nsFixedSizeAllocator::Free(void* aPtr, size_t aSize)
{
    FreeEntry* entry = NS_REINTERPRET_CAST(FreeEntry*, NS_STATIC_CAST(char*, aPtr) - sizeof(double));

    Bucket* bucket = entry->mBucket;

#ifdef DEBUG
    NS_ASSERTION(bucket->mSize == aSize, "ack! corruption! bucket->mSize != aSize!");
    nsCRT::memset(aPtr, 0xd8, bucket->mSize);
#endif

    entry->mNext = bucket->mFirst;
    bucket->mFirst = entry;
}

