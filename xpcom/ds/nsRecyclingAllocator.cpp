/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2001, 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *	Suresh Duddi <dp@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * nsRecyclingAllocator
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nsRecyclingAllocator.h"
#include "nsITimer.h"

#define NS_SEC_TO_MS(s) ((s) * 1000)

void
nsRecyclingAllocator::nsRecycleTimerCallback(nsITimer *aTimer, void *aClosure)
{
    nsRecyclingAllocator *obj = (nsRecyclingAllocator *) aClosure;
    if (!obj->mTouched)
    {
#ifdef DEBUG_dp
        printf("DEBUG: nsRecyclingAllocator not touched: %d allocations avaliable\n", obj->mNAllocations);
#endif
        if (obj->mNAllocations)
            obj->FreeUnusedBuckets();

        // If we are holding no more memory, there is not need for the timer.
        // We will revive the timer on the next allocation.
        // XXX Unfortunately there is no way to Cancel and restart the same timer.
        // XXX So we pretty much kill it and create a new one later.
        if (!obj->mNAllocations && obj->mRecycleTimer)
        {
            obj->mRecycleTimer->Cancel();
            NS_RELEASE(obj->mRecycleTimer);
        }
    }
    else
    {
        // Clear touched so the next time the timer fires we can test whether
        // the allocator was used or not.
        obj->Untouch();
    }
}


nsRecyclingAllocator::nsRecyclingAllocator(PRUint32 nbucket, PRUint32 recycleAfter, const char *id)
    : mNBucket(nbucket), mRecycleTimer(nsnull), mRecycleAfter(recycleAfter), mTouched(0),
      mNAllocations(0), mId(id)
{
    NS_ASSERTION(mNBucket <= NS_MAX_BUCKETS, "Too many buckets. This will affect the allocator's performance.");
    if (mNBucket > NS_MAX_BUCKETS)
    {
        mNBucket = NS_MAX_BUCKETS;
    }
    mMemBucket = (nsMemBucket *) calloc(mNBucket, sizeof(nsMemBucket));
    if (!mMemBucket)
        mNBucket = 0;
    else
    {
        for (PRUint32 i = 0; i < mNBucket; i++)
            mMemBucket[i].available = 1;

    }
}

nsRecyclingAllocator::~nsRecyclingAllocator()
{
    // Cancel and destroy recycle timer
    if (mRecycleTimer) {
        mRecycleTimer->Cancel();
        NS_RELEASE(mRecycleTimer);
    }

    // Free all memory held, if any
    if (mNAllocations)
        for (PRUint32 i = 0; i < mNBucket; i++)
        {
            PRBool claimed = Claim(i);
            
            // ASSERT that we claimed the bucked. If we cannot, then the bucket is in use.
            // We dont attempt to free this ptr.
            // This will most likely cause a leak of this memory.
            NS_ASSERTION(claimed, "~nsRecycledAllocator: Bucket memory still in use.");
      
            // Free bucket memory if not in use
            if (claimed && mMemBucket[i].ptr)
                free(mMemBucket[i].ptr);
        }

    // Free memory for buckets
    if (mMemBucket)
        free(mMemBucket);
}

// Allocation and free routines

void*
nsRecyclingAllocator::Malloc(PRUint32 bytes, PRBool zeroit)
{
    PRInt32 availableBucketIndex = -1;
    PRUint32 i;
    PRUint32 size;
    void *ptr;

    // Mark that we are using. This will prevent any
    // timer based release of unused memory.
    Touch();

    for (i = 0; i < mNBucket; i++)
    {
        size = mMemBucket[i].size;
        ptr = mMemBucket[i].ptr;

        // Dont look at buckets with no memory allocated or less memory than
        // what we need.
        // Can we do this check without claiming the bucket? I think we can
        // because the next thing we do is try claim this bucket.
        if (!ptr || size < bytes)
            continue;

        // Try Claim a bucket. If we cant, skip it.
        if (!Claim(i))
            continue;

        if (size == bytes)
        {
            // Let go of any freeAllocatedBucket that we claimed
            if (availableBucketIndex >= 0)
                Unclaim(availableBucketIndex);
            if (zeroit)
                memset(ptr, bytes, 0);
            return ptr;
        }
        // Meanwhile, remember a free allocated bucket.
        // At this point we know the bucket is not in use and it has more
        // than what we need
        if (availableBucketIndex < 0)
            availableBucketIndex = i;
        else
        {
            // See if this bucket is closer to what we need
            if (size < mMemBucket[availableBucketIndex].size)
            {
                Unclaim(availableBucketIndex);
                availableBucketIndex = i;
            }
            else
                // Undo our claim as availableBucketIndex does better than this one
                Unclaim(i);
        }
    }

    // See if we have an allocated bucket
    if (availableBucketIndex >= 0) {
        ptr = mMemBucket[availableBucketIndex].ptr;
        memset(ptr, bytes, 0);
        return ptr;
    }

    // We dont have that memory already
    // Allocate.
    ptr = zeroit ? calloc(1, bytes) : malloc(bytes);

    // Take care of no memory and no free slot situation
    if (!ptr || mNAllocations == (PRInt32)mNBucket) 
    {
#ifdef DEBUG_dp
        // Warn if we are failing over to malloc and not storing it
        // This says we have a misdesigned memory pool. The intent was
        // once the pool was full, we would never fail over to calloc.
        printf("nsRecyclingAllocator(%s) malloc %d - FAILOVER 0x%p Memory pool has sizes: ",
               mId, bytes, ptr);
        for (i = 0; i < mNBucket; i++)
        {
            printf("%d ", mMemBucket[i].size);
        }
        printf("\n");
#endif
        return ptr;
    }
  
    // Find a free unallocated bucket and store allocation
    for (i = 0; i < mNBucket; i++)
    {
        // If bucket cannot be claimed, continue search.
        if (!Claim(i))
            continue;

        if (!mMemBucket[i].ptr)
        {
            // Found free slot. Store it
            mMemBucket[i].ptr = ptr;
            mMemBucket[i].size = bytes;
            PR_AtomicIncrement(&mNAllocations);
            // This is the first allocation we are holding.
            // Setup timer for releasing memory
            // If this fails, then we wont have a timer to release unused
            // memory. We can live with that. Also, the next allocation
            // will try again to set the timer.
            if (mNAllocations && !mRecycleTimer)
            {
                (void) NS_NewTimer(&mRecycleTimer, nsRecycleTimerCallback, this,
                                   NS_SEC_TO_MS(mRecycleAfter), NS_PRIORITY_LOWEST,
                                   NS_TYPE_REPEATING_SLACK);
            }
            return ptr;
        }

        // This is an already allocated bucket. Cant use this one.
        Unclaim(i);
    }
#ifdef DEBUG_dp
    // Warn if we are failing over to malloc and not storing it
    // This says we have a misdesigned memory pool. The intent was
    // once the pool was full, we would never fail over to calloc.
    printf("nsRecyclingAllocator(%s) malloc %d - FAILOVER 0x%p Memory pool has sizes: ",
           mId, bytes, ptr);
    for (i = 0; i < mNBucket; i++)
    {
        printf("%d ", mMemBucket[i].size);
    }
    printf("\n");
#endif

    return ptr;
}

void
nsRecyclingAllocator::Free(void *ptr)
{
    // Mark that we are using the allocator. This will prevent any
    // timer based release of unused memory.
    Touch();

    for (PRUint32 i = 0; i < mNBucket; i++)
    {
        if (mMemBucket[i].ptr == ptr)
        {
            // Ah ha. One of the slots we allocated.
            // Nothing to do. Mark it unused.
            Unclaim(i);
            return;
        }
    }

#ifdef DEBUG_dp
    // Warn if we are failing over to free
    printf("nsRecyclingAllocator(%s) free - FAILOVER 0x%p Memory pool has sizes: ", mId, ptr);
    for (i = 0; i < mNBucket; i++)
    {
        printf("%d ", mMemBucket[i].size);
    }
    printf("\n");
#endif
    // Failover to free
    free(ptr);
}

/* FreeUnusedBuckets
 *
 * Frees any bucket memory that isn't in use
 */

void
nsRecyclingAllocator::FreeUnusedBuckets()
{
#ifdef DEBUG_dp
    printf("DEBUG: nsRecyclingAllocator(%s) FreeUnusedBuckets: ", mId);
#endif
    if (!mNAllocations)
        return;

    for (PRUint32 i = 0; i < mNBucket; i++)
    {
        if (Claim(i))
        {
            if (mMemBucket[i].ptr)
            {
#ifdef DEBUG_dp
                printf("%d ", mMemBucket[i].size);
#endif
                free(mMemBucket[i].ptr);
                mMemBucket[i].ptr = nsnull;
                mMemBucket[i].size = 0;
                PR_AtomicDecrement(&mNAllocations);
            }
            Unclaim(i);
        }
    }

#ifdef DEBUG_dp
    printf("\n");
#endif
}
