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
 * This allocator is thread safe.
 *
 * CAVEATS: As the number of buckets increases, this allocators performance
 *          will drop. As a general guideline, dont use this for more
 *          than NS_MAX_BUCKETS (10)
 */

#ifndef nsRecyclingAllocator_h__
#define nsRecyclingAllocator_h__

#include "nscore.h"
#include "pratom.h"

#define NS_DEFAULT_RECYCLE_TIMEOUT 10  // secs
#define NS_MAX_BUCKETS             10

class nsITimer;

class NS_COM nsRecyclingAllocator {
 protected:
    struct nsMemBucket {
        void *ptr;
        PRUint32 size;

        // Is this bucket available ?
        // 1 : free
        // 0 or negative : in use
        // This is an int because we use atomic inc/dec to operate on it
        PRInt32 available;
    };

    PRUint32 mNBucket;
    nsMemBucket *mMemBucket;

    nsITimer *mRecycleTimer;
    PRUint32 mRecycleAfter;

    // mTouched  : says if the allocator touched any bucket
    //             If allocator didn't touch any bucket over a time
    //             time interval, timer will call FreeUnusedBuckets()
    PRInt32 mTouched;

    // mNAllocations: says how many buckets still have memory.
    //             If all buckets were freed the last time
    //             around and allocator didn't touch any bucket, then
    //             there is nothing to be freed.
    PRInt32 mNAllocations;

    // mId: a string for identifying the user of nsRecyclingAllocator
    //      User mainly for debug prints
    const char *mId;

 public:

    // nbucket : number of buckets to hold. Capped at NS_MAX_BUCKET
    // recycleAfter : Try recycling allocated buckets after this many seconds
    // id : a string used to identify debug prints. Will not be released.
    nsRecyclingAllocator(PRUint32 nbucket, PRUint32 recycleAfter = NS_DEFAULT_RECYCLE_TIMEOUT,
                         const char *id = NULL);
    ~nsRecyclingAllocator();

    // Allocation and free routines
    void* Malloc(PRUint32 size, PRBool zeroit = PR_FALSE);
    void  Free(void *ptr);

    void* Calloc(PRUint32 items, PRUint32 size)
    {
        return Malloc(items * size, PR_TRUE);
    }


    // FreeUnusedBuckets - Frees any bucket memory that isn't in use
    void FreeUnusedBuckets();

 protected:
    // Timer callback to trigger unused memory

    static void nsRecycleTimerCallback(nsITimer *aTimer, void *aClosure);

    // Bucket handling
    PRBool Claim(PRUint32 i)
    {
        PRBool claimed = (PR_AtomicDecrement(&mMemBucket[i].available) == 0);
        // Undo the claim, if claim failed
        if (!claimed)
            Unclaim(i);

        return claimed;
    }

    void Unclaim(PRUint32 i) { PR_AtomicIncrement(&mMemBucket[i].available); }

    // Touch will mark that someone used this allocator
    // Timer based release will free unused memory only if allocator
    // was not touched for mRecycleAfter seconds.
    void Touch() {
        if (!mTouched)
            PR_AtomicSet(&mTouched, 1);
    }
    void Untouch() {
        PR_AtomicSet(&mTouched, 0);
    }

    friend void nsRecycleTimerCallback(nsITimer *aTimer, void *aClosure);
};

#endif // nsRecyclingAllocator_h__
