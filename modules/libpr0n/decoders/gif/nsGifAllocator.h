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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * Allocator optimized for use with gif decoder
 *
 * For every image that gets loaded, we allocate
 *     4097 x 2 : gs->prefix
 *     4097 x 1 : gs->suffix
 *     4097 x 1 : gs->stack
 * for lzw to operate on the data. These are held for a very short interval
 * and freed. This allocator tries to keep one set of these around
 * and reuses them; automatically fails over to use calloc/free when all
 * buckets are full.
 */

#include "prlock.h"
#include "prlog.h"

class nsGifAllocator;
extern nsGifAllocator *gGifAllocator;

const PRInt32 kNumBuckets = 3;

class nsGifAllocator {
 protected:
    void *mMemBucket[kNumBuckets];
    PRUint32 mSize[kNumBuckets];
    PRLock *mLock;
    PRUint32 mFlag;

 public:
    nsGifAllocator() : mFlag(0), mLock(nsnull)
    {
        memset(mMemBucket, 0, sizeof mMemBucket);
        memset(mSize, 0, sizeof mSize);
        mLock = PR_NewLock();
        PR_ASSERT(mLock != NULL);
    }

    ~nsGifAllocator()
    {
        ClearBuckets();
        if (mLock)
            PR_DestroyLock(mLock);
    }

    // Gif allocators
    void* Calloc(PRUint32 items, PRUint32 size);
    void Free(void *ptr);
    // Clear all buckets of memory
    void ClearBuckets();

    // in-use flag getters/setters
    inline PRBool IsUsed(PRUint32 i)
    {
        PR_ASSERT(i <= 31);
        return mFlag & (1 << i);
    }
    inline void MarkUsed(PRUint32 i)
    {
        PR_ASSERT(i <= 31);
        mFlag |= (1 << i);
    }
    inline void ClearUsed(PRUint32 i)
    {
        PR_ASSERT(i <= 31);
        mFlag &= ~(1 << i);
    }
};
