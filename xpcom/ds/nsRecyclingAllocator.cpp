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
#include "nsAutoLock.h"
#include "prprf.h"
#include "nsITimer.h"

#define NS_SEC_TO_MS(s) ((s) * 1000)

void
nsRecyclingAllocator::nsRecycleTimerCallback(nsITimer *aTimer, void *aClosure)
{
    nsRecyclingAllocator *obj = (nsRecyclingAllocator *) aClosure;
    if (!obj->mTouched)
    {
        if (obj->mFreeList)
            obj->FreeUnusedBuckets();

        // If we are holding no more memory, there is no need for the timer.
        // We will revive the timer on the next allocation.
        // XXX Unfortunately there is no way to Cancel and restart the same timer.
        // XXX So we pretty much kill it and create a new one later.
        if (!obj->mFreeList && obj->mRecycleTimer)
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


nsRecyclingAllocator::nsRecyclingAllocator(PRUint32 nbucket, PRUint32 recycleAfter, const char *id) :
    mMaxBlocks(nbucket), mBlocks(nsnull), mFreeList(nsnull), mNotUsedList(nsnull),
    mRecycleTimer(nsnull), mRecycleAfter(recycleAfter), mTouched(0), mId(id)
#ifdef DEBUG
     ,mNAllocated(0)
#endif
{
    NS_ASSERTION(mMaxBlocks <= NS_MAX_BLOCKS, "Too many blocks. This will affect the allocator's performance.");

    mLock = PR_NewLock();
    NS_ASSERTION(mLock, "Recycling allocator cannot get lock");

    Init(nbucket, recycleAfter, id);
}

nsresult
nsRecyclingAllocator::Init(PRUint32 nbucket, PRUint32 recycleAfter, const char *id)
{
    nsAutoLock lock(mLock);

    // Free all memory held, if any
    while(mFreeList)
    {
        free(mFreeList->block);
        mFreeList = mFreeList->next;
    }
    mFreeList = nsnull;
    
    if (mBlocks)
        delete [] mBlocks;

    // Reinitialize everything
    mMaxBlocks = nbucket;
    if (nbucket)
    {
        // Create memory for our bookkeeping
        mBlocks = new BlockStoreNode[mMaxBlocks];
        if (!mBlocks)
            return NS_ERROR_OUT_OF_MEMORY;
        // Link them together
        mNotUsedList = mBlocks;
        for (PRUint32 i=0; i < mMaxBlocks-1; i++)
            mBlocks[i].next = &(mBlocks[i+1]);
    }

    mRecycleAfter = recycleAfter;
    mId = id;

    return NS_OK;
}

nsRecyclingAllocator::~nsRecyclingAllocator()
{
    // Cancel and destroy recycle timer
    if (mRecycleTimer)
    {
        mRecycleTimer->Cancel();
        NS_RELEASE(mRecycleTimer);
    }

    // Free all memory held, if any
    while(mFreeList)
    {
        free(mFreeList->block);
        mFreeList = mFreeList->next;
    }
    mFreeList = nsnull;
    
    if (mBlocks)
        delete [] mBlocks;

    if (mLock)
    {
        PR_DestroyLock(mLock);
        mLock = nsnull;
    }
}

// Allocation and free routines
void*
nsRecyclingAllocator::Malloc(PRSize bytes, PRBool zeroit)
{
    // Mark that we are using. This will prevent any
    // timer based release of unused memory.
    Touch();
  
    Block* freeBlock = FindFreeBlock(bytes);
    if (freeBlock)
    {
        void *data = DATA(freeBlock);

        if (zeroit)
            memset(data, 0, bytes);
        return data;
    }
     
    // We need to do an allocation
    // Add 4 bytes to what we allocate to hold the bucket index
    PRSize allocBytes = bytes + NS_ALLOCATOR_OVERHEAD_BYTES;
  
    // We dont have that memory already. Allocate.
    Block *ptr = (Block *) (zeroit ? calloc(1, allocBytes) : malloc(allocBytes));
    
    // Deal with no memory situation
    if (!ptr)
        return ptr;
  
    // This is the first allocation we are holding.
    // Setup timer for releasing memory
    // If this fails, then we wont have a timer to release unused
    // memory. We can live with that. Also, the next allocation
    // will try again to set the timer.
    if (mRecycleAfter && !mRecycleTimer)
    {
        // known only to stuff in xpcom.  
        extern nsresult NS_NewTimer(nsITimer* *aResult, nsTimerCallbackFunc aCallback, void *aClosure,
                                    PRUint32 aDelay, PRUint32 aType);

        (void) NS_NewTimer(&mRecycleTimer, nsRecycleTimerCallback, this, 
                           NS_SEC_TO_MS(mRecycleAfter),
                           nsITimer::TYPE_REPEATING_SLACK);
        NS_ASSERTION(mRecycleTimer, "nsRecyclingAllocator: Creating timer failed.\n");
    }
 
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
    // Mark that we are using the allocator. This will prevent any
    // timer based release of unused memory.
    Touch();

    Block* block = DATA_TO_BLOCK(ptr);

    if (!AddToFreeList(block))
    {
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
    nsAutoLock lock(mLock);

    // We will run through the freelist and free all blocks
    BlockStoreNode* node = mFreeList;
    while (node)
    {
        // Free the allocated block
        free(node->block);

#ifdef DEBUG_dp
        printf("%d ", node->bytes);
#endif
        // Clear Node
        node->block = nsnull;
        node->bytes = 0;
        node = node->next;
    }

    // remake the lists
    mNotUsedList = mBlocks;
    for (PRUint32 i=0; i < mMaxBlocks-1; i++)
        mBlocks[i].next = &(mBlocks[i+1]);
    mBlocks[mMaxBlocks-1].next = nsnull;
    mFreeList = nsnull;

#ifdef DEBUG        
    mNAllocated = 0;
#endif
#ifdef DEBUG_dp
    printf("\n");
#endif
}

nsRecyclingAllocator::Block*
nsRecyclingAllocator::FindFreeBlock(PRSize bytes)
{
    // We dont enter lock for this check. This is intentional.
    // Here is my logic: we are checking if (!mFreeList). Doing this check
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
    // the allocator is full, we dont want to impose any more overhead than
    // we already are for failing over to malloc/free.

    if (!mFreeList)
        return NULL;

    Block *block = nsnull;

    nsAutoLock lock(mLock);
    BlockStoreNode* freeNode = mFreeList;
    BlockStoreNode** prevp = &mFreeList;

    while (freeNode)
    {
        if (freeNode->bytes >= bytes)
        {
            // Found the best fit free block
            block = freeNode->block;

            // Clear the free node
            freeNode->block = nsnull;
            freeNode->bytes = 0;

            // Remove free node from free list
            *prevp = freeNode->next;

            // Add removed BlockStoreNode to not used list
            freeNode->next = mNotUsedList;
            mNotUsedList = freeNode;

            break;
        }

        prevp = &(freeNode->next);
        freeNode = freeNode->next;
    }
    return block;
}

PRInt32
nsRecyclingAllocator::AddToFreeList(Block* block)
{
    nsAutoLock lock(mLock);

    if (!mNotUsedList)
        return PR_FALSE;

    // Pick a node from the not used list
    BlockStoreNode *node = mNotUsedList;
    mNotUsedList = mNotUsedList->next;

    // Initialize the node
    node->bytes = block->bytes;
    node->block = block;

    // Find the right spot in the sorted list.
    BlockStoreNode* freeNode = mFreeList;
    BlockStoreNode** prevp = &mFreeList;
    while (freeNode)
    {
        if (freeNode->bytes >= block->bytes)
            break;
        prevp = &(freeNode->next);
        freeNode = freeNode->next;
    }

    // Needs to be inserted between *prevp and freeNode
    *prevp = node;
    node->next = freeNode;

    return PR_TRUE;
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
nsRecyclingAllocatorImpl::HeapMinimize(PRBool immediate)
{
    // XXX Not yet implemented
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRecyclingAllocatorImpl::IsLowMemory(PRBool *lowmemoryb_ptr)
{
    // XXX Not yet implemented
    return NS_ERROR_NOT_IMPLEMENTED;
}

