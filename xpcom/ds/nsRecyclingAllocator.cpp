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
#include "nsIMemory.h"
#include "prprf.h"
#include "nsITimer.h"

using namespace mozilla;

#define NS_SEC_TO_MS(s) ((s) * 1000)

void
nsRecyclingAllocator::nsRecycleTimerCallback(nsITimer *aTimer, void *aClosure)
{
    nsRecyclingAllocator *obj = (nsRecyclingAllocator *) aClosure;

    MutexAutoLock lock(obj->mLock);

    if (!obj->mTouched)
    {
        obj->ClearFreeList();
    }
    else
    {
        // Clear touched so the next time the timer fires we can test whether
        // the allocator was used or not.
        obj->mTouched = PR_FALSE;
    }
}


nsRecyclingAllocator::nsRecyclingAllocator(PRUint32 nbucket, PRUint32 recycleAfter, const char *id) :
    mMaxBlocks(nbucket), mFreeListCount(0), mFreeList(nsnull),
    mLock("nsRecyclingAllocator.mLock"),
    mRecycleTimer(nsnull), mRecycleAfter(recycleAfter), mTouched(PR_FALSE)
#ifdef DEBUG
    , mId(id), mNAllocated(0)
#endif
{
}

nsresult
nsRecyclingAllocator::Init(PRUint32 nbucket, PRUint32 recycleAfter, const char *id)
{
    MutexAutoLock lock(mLock);

    ClearFreeList();

    // Reinitialize everything
    mMaxBlocks = nbucket;
    mRecycleAfter = recycleAfter;
#ifdef DEBUG
    mId = id;
#endif

    return NS_OK;
}

nsRecyclingAllocator::~nsRecyclingAllocator()
{
    ClearFreeList();
}

// Allocation and free routines
void*
nsRecyclingAllocator::Malloc(PRSize bytes, bool zeroit)
{
    // We don't enter lock for this check. This is intentional.
    // Here is my logic: we are checking if (mFreeList). Doing this check
    // without locking can lead to unpredictable results. YES. But the effect
    // of the unpredictedness are ok. here is why:
    //
    // a) if the check returned NULL when there is stuff in freelist
    //    We would just end up reallocating.
    //
    // b) if the check returned nonNULL when our freelist is empty
    //    This is the more likely and dangerous case. The code for
    //    FindFreeBlock() will enter lock, while (null) and return null.
    //
    // The reason why I chose to not enter lock for this check was that when
    // there are no free blocks, we don't want to impose any more overhead than
    // we already are for failing over to malloc/free.
    if (mFreeList) {
        MutexAutoLock lock(mLock);

        // Mark that we are using. This will prevent any
        // Timer based release of unused memory.
        mTouched = PR_TRUE;

        Block* freeNode = mFreeList;
        Block** prevp = &mFreeList;

        while (freeNode)
        {
            if (freeNode->bytes >= bytes)
            {
                // Found the best fit free block
                // Remove this block from free list
                *prevp = freeNode->next;
                mFreeListCount --;

                void *data = DATA(freeNode);
                if (zeroit)
                    memset(data, 0, bytes);
                return data;
            }

            prevp = &(freeNode->next);
            freeNode = freeNode->next;
        }
    }
    
    // We need to do an allocation
    // Add 4 bytes to what we allocate to hold the bucket index
    PRSize allocBytes = bytes + NS_ALLOCATOR_OVERHEAD_BYTES;

    // Make sure it is big enough to hold the whole block
    if (allocBytes < sizeof(Block)) allocBytes = sizeof(Block);

    // We don't have that memory already. Allocate.
    Block *ptr = (Block *) (zeroit ? calloc(1, allocBytes) : malloc(allocBytes));
  
    // Deal with no memory situation
    if (!ptr)
        return ptr;

#ifdef DEBUG
    mNAllocated++;
#endif
  
    // Store size and return data portion
    ptr->bytes = bytes;
    return DATA(ptr);
}

void
nsRecyclingAllocator::Free(void *ptr)
{
    Block* block = DATA_TO_BLOCK(ptr);

    MutexAutoLock lock(mLock);

    // Mark that we are using the allocator. This will prevent any
    // timer based release of unused memory.
    mTouched = PR_TRUE;

    // Check on maximum number of blocks to keep in the freelist
    if (mFreeListCount < mMaxBlocks) {
        // Find the right spot in the sorted list.
        Block* freeNode = mFreeList;
        Block** prevp = &mFreeList;
        while (freeNode)
        {
            if (freeNode->bytes >= block->bytes)
                break;
            prevp = &(freeNode->next);
            freeNode = freeNode->next;
        }

        // Needs to be inserted between *prevp and freeNode
        *prevp = block;
        block->next = freeNode;
        mFreeListCount ++;
    } else {
        // We are holding more than max. Failover to free
#ifdef DEBUG_dp
        char buf[1024];
        // Warn if we are failing over to malloc/free and not storing it
        // This says we have a misdesigned memory pool. The intent was
        // once the pool was full, we would never fail over to calloc.
        PR_snprintf(buf, sizeof(buf), "nsRecyclingAllocator(%s) FAILOVER 0x%p (%d) - %d allocations, %d max\n",
                    mId, (char *)ptr, block->bytes, mNAllocated, mMaxBlocks);
        NS_WARNING(buf);
        mNAllocated--;
#endif
        free(block);
    }

    // Set up timer for releasing memory for blocks from the freelist.
    // If this fails, then we won't have a timer to release unused
    // memory. We can live with that. Also, the next Free
    // will try again to set the timer.
    if (mRecycleAfter && !mRecycleTimer)
    {
        // known only to stuff in xpcom.
        extern nsresult NS_NewTimer(nsITimer* *aResult, nsTimerCallbackFunc aCallback, void *aClosure,
                                    PRUint32 aDelay, PRUint32 aType);

        (void) NS_NewTimer(&mRecycleTimer, nsRecycleTimerCallback, this,
                           NS_SEC_TO_MS(mRecycleAfter),
                           nsITimer::TYPE_REPEATING_SLACK);
        // This can fail during xpcom shutdown
        if (!mRecycleTimer)
            NS_WARNING("nsRecyclingAllocator: Creating timer failed");
    }
}

/* ClearFreeList
 *
 * Frees any bucket memory that isn't in use
 */

void
nsRecyclingAllocator::ClearFreeList()
{
#ifdef DEBUG_dp
    printf("DEBUG: nsRecyclingAllocator(%s) ClearFreeList (%d): ", mId, mFreeListCount);
#endif
    // Cancel the timer, because the freelist will be forcefully cleared after this.
    // We will revive the timer on the next allocation.
    // XXX Unfortunately there is no way to Cancel and restart the same timer.
    // XXX So we pretty much kill it and create a new one later.
    if (mRecycleTimer)
    {
        mRecycleTimer->Cancel();
        NS_RELEASE(mRecycleTimer);
    }

    // We will run through the freelist and free all blocks
    Block* node = mFreeList;
    while (node)
    {
#ifdef DEBUG_dp
        printf("%d ", node->bytes);
#endif
        Block *next = node->next;
        free(node);
        node = next;
    }
    mFreeList = nsnull;
    mFreeListCount = 0;

#ifdef DEBUG
    mNAllocated = 0;
#endif
#ifdef DEBUG_dp
    printf("\n");
#endif
}


// ----------------------------------------------------------------------
// Wrapping the recyling allocator with nsIMemory
// ----------------------------------------------------------------------

// nsIMemory methods
NS_IMPL_THREADSAFE_ISUPPORTS2(nsRecyclingAllocatorImpl, nsIMemory, nsIRecyclingAllocator)

NS_IMETHODIMP_(void *)
nsRecyclingAllocatorImpl::Alloc(PRSize size)
{
    return nsRecyclingAllocatorImpl::Malloc(size, PR_FALSE);
}

NS_IMETHODIMP_(void *)
nsRecyclingAllocatorImpl::Realloc(void *ptr, PRSize size)
{
    // XXX Not yet implemented
    return NULL;
}

NS_IMETHODIMP_(void)
nsRecyclingAllocatorImpl::Free(void *ptr)
{
    nsRecyclingAllocator::Free(ptr);
}

NS_IMETHODIMP
nsRecyclingAllocatorImpl::Init(size_t nbuckets, size_t recycleAfter, const char *id)
{
    return nsRecyclingAllocator::Init((PRUint32) nbuckets, (PRUint32) recycleAfter, id);
}

NS_IMETHODIMP
nsRecyclingAllocatorImpl::HeapMinimize(bool immediate)
{
    // XXX Not yet implemented
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRecyclingAllocatorImpl::IsLowMemory(bool *lowmemoryb_ptr)
{
    // XXX Not yet implemented
    return NS_ERROR_NOT_IMPLEMENTED;
}

