/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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

#ifndef nsSegmentedBuffer_h__
#define nsSegmentedBuffer_h__

#include "nsIAllocator.h"
#include "prclist.h"

class nsSegmentedBuffer
{
public:
    nsSegmentedBuffer();
    ~nsSegmentedBuffer();

    nsresult Init(PRUint32 segmentSize, PRUint32 maxSize,
                  nsIAllocator* allocator = nsnull);

    char* AppendNewSegment();   // pushes at end

    // returns true if no more segments remain:
    PRBool DeleteFirstSegment();  // pops from beginning

    void Empty();               // frees all segments

    PRUint32 GetSegmentCount() {
        if (mFirstSegmentIndex <= mLastSegmentIndex)
            return mLastSegmentIndex - mFirstSegmentIndex;
        else 
            return mSegmentArrayCount + mLastSegmentIndex - mFirstSegmentIndex;
    }

    PRUint32 GetSegmentSize() { return mSegmentSize; }

    PRUint32 GetSize() { return GetSegmentCount() * mSegmentSize; }

    char* GetSegment(PRUint32 index) {
        NS_ASSERTION(0 <= index && index < GetSegmentCount(),
                     "index out of bounds");
        PRInt32 i = ModSegArraySize(mFirstSegmentIndex + (PRInt32)index);
        return mSegmentArray[i];
    }

protected:
    PRInt32 ModSegArraySize(PRInt32 n) {
        PRUint32 result = n & (mSegmentArrayCount - 1);
        NS_ASSERTION(result == n % mSegmentArrayCount,
                     "non-power-of-2 mSegmentArrayCount");
        return result;
    }

    PRBool IsFull() {
        return ModSegArraySize(mLastSegmentIndex + 1) == mFirstSegmentIndex;
    }

protected:
    PRUint32            mSegmentSize;
    PRUint32            mMaxSize;
    nsIAllocator*       mSegAllocator;
    char**              mSegmentArray;
    PRUint32            mSegmentArrayCount;
    PRInt32             mFirstSegmentIndex;
    PRInt32             mLastSegmentIndex;
};

// NS_SEGMENTARRAY_INITIAL_SIZE: This number needs to start out as a
// power of 2 given how it gets used. We double the segment array
// when we overflow it, and use that fact that it's a power of 2 
// to compute a fast modulus operation in IsFull.
//
// 32 segment array entries can accommodate 128k of data if segments
// are 4k in size. That seems like a reasonable amount that will avoid
// needing to grow the segment array.
#define NS_SEGMENTARRAY_INITIAL_COUNT 32

#endif // nsSegmentedBuffer_h__
