/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSegmentedBuffer_h__
#define nsSegmentedBuffer_h__

#include "nsMemory.h"
#include "prclist.h"

class nsSegmentedBuffer
{
public:
    nsSegmentedBuffer()
        : mSegmentSize(0), mMaxSize(0), 
          mSegAllocator(nsnull), mSegmentArray(nsnull),
          mSegmentArrayCount(0),
          mFirstSegmentIndex(0), mLastSegmentIndex(0) {}

    ~nsSegmentedBuffer() {
        Empty();
        NS_IF_RELEASE(mSegAllocator);
    }


    nsresult Init(PRUint32 segmentSize, PRUint32 maxSize,
                  nsIMemory* allocator = nsnull);

    char* AppendNewSegment();   // pushes at end

    // returns true if no more segments remain:
    bool DeleteFirstSegment();  // pops from beginning

    // returns true if no more segments remain:
    bool DeleteLastSegment();  // pops from beginning

    // Call Realloc() on last segment.  This is used to reduce memory
    // consumption when data is not an exact multiple of segment size.
    bool ReallocLastSegment(size_t newSize);

    void Empty();               // frees all segments

    inline PRUint32 GetSegmentCount() {
        if (mFirstSegmentIndex <= mLastSegmentIndex)
            return mLastSegmentIndex - mFirstSegmentIndex;
        else 
            return mSegmentArrayCount + mLastSegmentIndex - mFirstSegmentIndex;
    }

    inline PRUint32 GetSegmentSize() { return mSegmentSize; }
    inline PRUint32 GetMaxSize() { return mMaxSize; }
    inline PRUint32 GetSize() { return GetSegmentCount() * mSegmentSize; }

    inline char* GetSegment(PRUint32 indx) {
        NS_ASSERTION(indx < GetSegmentCount(), "index out of bounds");
        PRInt32 i = ModSegArraySize(mFirstSegmentIndex + (PRInt32)indx);
        return mSegmentArray[i];
    }

protected:
    inline PRInt32 ModSegArraySize(PRInt32 n) {
        PRUint32 result = n & (mSegmentArrayCount - 1);
        NS_ASSERTION(result == n % mSegmentArrayCount,
                     "non-power-of-2 mSegmentArrayCount");
        return result;
    }

   inline bool IsFull() {
        return ModSegArraySize(mLastSegmentIndex + 1) == mFirstSegmentIndex;
    }

protected:
    PRUint32            mSegmentSize;
    PRUint32            mMaxSize;
    nsIMemory*       mSegAllocator;
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
