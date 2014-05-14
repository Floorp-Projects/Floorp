/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSegmentedBuffer_h__
#define nsSegmentedBuffer_h__

#include "nsIMemory.h"

class nsSegmentedBuffer
{
public:
  nsSegmentedBuffer()
    : mSegmentSize(0)
    , mMaxSize(0)
    , mSegmentArray(nullptr)
    , mSegmentArrayCount(0)
    , mFirstSegmentIndex(0)
    , mLastSegmentIndex(0)
  {
  }

  ~nsSegmentedBuffer()
  {
    Empty();
  }


  nsresult Init(uint32_t aSegmentSize, uint32_t aMaxSize);

  char* AppendNewSegment();   // pushes at end

  // returns true if no more segments remain:
  bool DeleteFirstSegment();  // pops from beginning

  // returns true if no more segments remain:
  bool DeleteLastSegment();  // pops from beginning

  // Call Realloc() on last segment.  This is used to reduce memory
  // consumption when data is not an exact multiple of segment size.
  bool ReallocLastSegment(size_t aNewSize);

  void Empty();               // frees all segments

  inline uint32_t GetSegmentCount()
  {
    if (mFirstSegmentIndex <= mLastSegmentIndex) {
      return mLastSegmentIndex - mFirstSegmentIndex;
    } else {
      return mSegmentArrayCount + mLastSegmentIndex - mFirstSegmentIndex;
    }
  }

  inline uint32_t GetSegmentSize()
  {
    return mSegmentSize;
  }
  inline uint32_t GetMaxSize()
  {
    return mMaxSize;
  }
  inline uint32_t GetSize()
  {
    return GetSegmentCount() * mSegmentSize;
  }

  inline char* GetSegment(uint32_t aIndex)
  {
    NS_ASSERTION(aIndex < GetSegmentCount(), "index out of bounds");
    int32_t i = ModSegArraySize(mFirstSegmentIndex + (int32_t)aIndex);
    return mSegmentArray[i];
  }

protected:
  inline int32_t ModSegArraySize(int32_t aIndex)
  {
    uint32_t result = aIndex & (mSegmentArrayCount - 1);
    NS_ASSERTION(result == aIndex % mSegmentArrayCount,
                 "non-power-of-2 mSegmentArrayCount");
    return result;
  }

  inline bool IsFull()
  {
    return ModSegArraySize(mLastSegmentIndex + 1) == mFirstSegmentIndex;
  }

protected:
  uint32_t            mSegmentSize;
  uint32_t            mMaxSize;
  char**              mSegmentArray;
  uint32_t            mSegmentArrayCount;
  int32_t             mFirstSegmentIndex;
  int32_t             mLastSegmentIndex;
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
