/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *
 * This allocator is useful when we cycle through a small set of allocations
 * repeatedly with minimal overlap. For eg. something we do for every gif
 * file read (or) buffers required for decompression of every file from jar.
 *
 * What this does is keeps around the first set of memory allocated and
 * reuses it subsequently. If all buckets are full, this falls back to
 * malloc/free
 *
 * Uses a timer to release all memory allocated if not used for more than
 * 10 secs automatically.
 *
 * Also there is a 4 byte maintenance overhead on every allocation.
 *
 * This allocator is thread safe.
 *
 * CAVEATS: As the number of buckets increases, this allocators performance
 *          will drop, as the list of freed items is a linked list sorted on size.
 */

#ifndef nsRecyclingAllocator_h__
#define nsRecyclingAllocator_h__

#include "mozilla/Mutex.h"
#include "nscore.h"
#include "nsIRecyclingAllocator.h"

#define NS_DEFAULT_RECYCLE_TIMEOUT 10  // secs
#define NS_ALLOCATOR_OVERHEAD_BYTES (sizeof(PRSize)) // bytes

class nsITimer;
class nsIMemory;

class nsRecyclingAllocator {
 protected:
    struct Block {
      PRSize bytes;
      Block *next;
    };

#define DATA(block) ((void *)(((char *)block) + NS_ALLOCATOR_OVERHEAD_BYTES))
#define DATA_TO_BLOCK(data) ((Block *)((char *)(data) - NS_ALLOCATOR_OVERHEAD_BYTES))

    // mMaxBlocks: Maximum number of blocks that are kept in the mFreeList for recycling
    PRUint32 mMaxBlocks;

    // mFreeListCount: Current number of blocks in the mFreeList
    PRUint32 mFreeListCount;

    // mFreeList: linked list of free blocks sorted by increasing order of size
    Block* mFreeList;

    // mLock: Thread safety for the member variables:
    //  mFreeList, mFreeListCount, mRecycleTimer, and mTouched
    mozilla::Mutex mLock;

    // Timer for freeing unused memory
    nsITimer *mRecycleTimer;

    // mRecycleAfter:
    //  Allocator should be untouched for this many seconds for freeing
    //  unused Blocks.
    PRUint32 mRecycleAfter;

    // mTouched:
    //  says if the allocator touched the freelist. If allocator didn't touch
    //  the freelist over a time time interval, timer will call ClearFreeList()
    bool mTouched;

#ifdef DEBUG
    // mId:
    //  a string for identifying the user of nsRecyclingAllocator
    //  User mainly for debug prints
    const char *mId;

    // mNAllocated: Number of blocks allocated
    PRInt32 mNAllocated;
#endif

 public:

    // nbucket : number of buckets to hold. Capped at NS_MAX_BUCKET
    // recycleAfter : Try recycling allocated buckets after this many seconds
    // id : a string used to identify debug prints. Will not be released.
    nsRecyclingAllocator(PRUint32 nbucket = 0, PRUint32 recycleAfter = NS_DEFAULT_RECYCLE_TIMEOUT,
                         const char *id = NULL);
    ~nsRecyclingAllocator();

    nsresult Init(PRUint32 nbucket, PRUint32 recycleAfter, const char *id);

    // Allocation and free routines
    void* Malloc(PRSize size, bool zeroit = false);
    void  Free(void *ptr);

    void* Calloc(PRUint32 items, PRSize size)
    {
        return Malloc(items * size, PR_TRUE);
    }

    // ClearFreeList - Frees all blocks kept by mFreelist, and stops the timer
    void ClearFreeList();

 protected:

    // Timer callback to trigger unused memory
    static void nsRecycleTimerCallback(nsITimer *aTimer, void *aClosure);
    friend void nsRecycleTimerCallback(nsITimer *aTimer, void *aClosure);
};

// ----------------------------------------------------------------------
// Wrapping the recyling allocator with nsIMemory
// ----------------------------------------------------------------------

// Wrapping the nsRecyclingAllocator with nsIMemory
class nsRecyclingAllocatorImpl : public nsRecyclingAllocator, public nsIRecyclingAllocator {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMEMORY
    NS_DECL_NSIRECYCLINGALLOCATOR

    nsRecyclingAllocatorImpl()
    {
    }

private:
    ~nsRecyclingAllocatorImpl() {}
};
#endif // nsRecyclingAllocator_h__
