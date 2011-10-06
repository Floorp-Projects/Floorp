/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
    PRBool DeleteFirstSegment();  // pops from beginning

    // returns true if no more segments remain:
    PRBool DeleteLastSegment();  // pops from beginning

    // Call Realloc() on last segment.  This is used to reduce memory
    // consumption when data is not an exact multiple of segment size.
    PRBool ReallocLastSegment(size_t newSize);

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

   inline PRBool IsFull() {
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
