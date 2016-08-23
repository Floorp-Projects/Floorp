/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BufferList_h
#define mozilla_BufferList_h

#include <algorithm>
#include "mozilla/AllocPolicy.h"
#include "mozilla/Move.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Types.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Vector.h"
#include <string.h>

// BufferList represents a sequence of buffers of data. A BufferList can choose
// to own its buffers or not. The class handles writing to the buffers,
// iterating over them, and reading data out. Unlike SegmentedVector, the
// buffers may be of unequal size. Like SegmentedVector, BufferList is a nice
// way to avoid large contiguous allocations (which can trigger OOMs).

namespace mozilla {

template<typename AllocPolicy>
class BufferList : private AllocPolicy
{
  // Each buffer in a BufferList has a size and a capacity. The first mSize
  // bytes are initialized and the remaining |mCapacity - mSize| bytes are free.
  struct Segment
  {
    char* mData;
    size_t mSize;
    size_t mCapacity;

    Segment(char* aData, size_t aSize, size_t aCapacity)
     : mData(aData),
       mSize(aSize),
       mCapacity(aCapacity)
    {
    }

    Segment(const Segment&) = delete;
    Segment& operator=(const Segment&) = delete;

    Segment(Segment&&) = default;
    Segment& operator=(Segment&&) = default;

    char* Start() const { return mData; }
    char* End() const { return mData + mSize; }
  };

  template<typename OtherAllocPolicy>
  friend class BufferList;

 public:
  // For the convenience of callers, all segments are required to be a multiple
  // of 8 bytes in capacity. Also, every buffer except the last one is required
  // to be full (i.e., size == capacity). Therefore, a byte at offset N within
  // the BufferList and stored in memory at an address A will satisfy
  // (N % Align == A % Align) if Align == 2, 4, or 8.
  //
  // NB: FlattenBytes can create non-full segments in the middle of the
  // list. However, it ensures that these buffers are 8-byte aligned, so the
  // offset invariant is not violated.
  static const size_t kSegmentAlignment = 8;

  // Allocate a BufferList. The BufferList will free all its buffers when it is
  // destroyed. An initial buffer of size aInitialSize and capacity
  // aInitialCapacity is allocated automatically. This data will be contiguous
  // an can be accessed via |Start()|. Subsequent buffers will be allocated with
  // capacity aStandardCapacity.
  BufferList(size_t aInitialSize,
             size_t aInitialCapacity,
             size_t aStandardCapacity,
             AllocPolicy aAP = AllocPolicy())
   : AllocPolicy(aAP),
     mOwning(true),
     mSegments(aAP),
     mSize(0),
     mStandardCapacity(aStandardCapacity)
  {
    MOZ_ASSERT(aInitialCapacity % kSegmentAlignment == 0);
    MOZ_ASSERT(aStandardCapacity % kSegmentAlignment == 0);

    if (aInitialCapacity) {
      AllocateSegment(aInitialSize, aInitialCapacity);
    }
  }

  BufferList(const BufferList& aOther) = delete;

  BufferList(BufferList&& aOther)
   : mOwning(aOther.mOwning),
     mSegments(Move(aOther.mSegments)),
     mSize(aOther.mSize),
     mStandardCapacity(aOther.mStandardCapacity)
  {
    aOther.mSegments.clear();
    aOther.mSize = 0;
  }

  BufferList& operator=(const BufferList& aOther) = delete;

  BufferList& operator=(BufferList&& aOther)
  {
    Clear();

    mOwning = aOther.mOwning;
    mSegments = Move(aOther.mSegments);
    mSize = aOther.mSize;
    aOther.mSegments.clear();
    aOther.mSize = 0;
    return *this;
  }

  ~BufferList() { Clear(); }

  // Returns the sum of the sizes of all the buffers.
  size_t Size() const { return mSize; }

  void Clear()
  {
    if (mOwning) {
      for (Segment& segment : mSegments) {
        this->free_(segment.mData);
      }
    }
    mSegments.clear();

    mSize = 0;
  }

  // Iterates over bytes in the segments. You can advance it by as many bytes as
  // you choose.
  class IterImpl
  {
    // Invariants:
    //   (0) mSegment <= bufferList.mSegments.size()
    //   (1) mData <= mDataEnd
    //   (2) If mSegment is not the last segment, mData < mDataEnd
    uintptr_t mSegment;
    char* mData;
    char* mDataEnd;

    friend class BufferList;

  public:
    explicit IterImpl(const BufferList& aBuffers)
     : mSegment(0),
       mData(nullptr),
       mDataEnd(nullptr)
    {
      if (!aBuffers.mSegments.empty()) {
        mData = aBuffers.mSegments[0].Start();
        mDataEnd = aBuffers.mSegments[0].End();
      }
    }

    // Returns a pointer to the raw data. It is valid to access up to
    // RemainingInSegment bytes of this buffer.
    char* Data() const
    {
      MOZ_RELEASE_ASSERT(!Done());
      return mData;
    }

    // Returns true if the memory in the range [Data(), Data() + aBytes) is all
    // part of one contiguous buffer.
    bool HasRoomFor(size_t aBytes) const
    {
      MOZ_RELEASE_ASSERT(mData <= mDataEnd);
      return size_t(mDataEnd - mData) >= aBytes;
    }

    // Returns the maximum value aBytes for which HasRoomFor(aBytes) will be
    // true.
    size_t RemainingInSegment() const
    {
      MOZ_RELEASE_ASSERT(mData <= mDataEnd);
      return mDataEnd - mData;
    }

    // Advances the iterator by aBytes bytes. aBytes must be less than
    // RemainingInSegment(). If advancing by aBytes takes the iterator to the
    // end of a buffer, it will be moved to the beginning of the next buffer
    // unless it is the last buffer.
    void Advance(const BufferList& aBuffers, size_t aBytes)
    {
      const Segment& segment = aBuffers.mSegments[mSegment];
      MOZ_RELEASE_ASSERT(segment.Start() <= mData);
      MOZ_RELEASE_ASSERT(mData <= mDataEnd);
      MOZ_RELEASE_ASSERT(mDataEnd == segment.End());

      MOZ_RELEASE_ASSERT(HasRoomFor(aBytes));
      mData += aBytes;

      if (mData == mDataEnd && mSegment + 1 < aBuffers.mSegments.length()) {
        mSegment++;
        const Segment& nextSegment = aBuffers.mSegments[mSegment];
        mData = nextSegment.Start();
        mDataEnd = nextSegment.End();
        MOZ_RELEASE_ASSERT(mData < mDataEnd);
      }
    }

    // Advance the iterator by aBytes, possibly crossing segments. This function
    // returns false if it runs out of buffers to advance through. Otherwise it
    // returns true.
    bool AdvanceAcrossSegments(const BufferList& aBuffers, size_t aBytes)
    {
      size_t bytes = aBytes;
      while (bytes) {
        size_t toAdvance = std::min(bytes, RemainingInSegment());
        if (!toAdvance) {
          return false;
        }
        Advance(aBuffers, toAdvance);
        bytes -= toAdvance;
      }
      return true;
    }

    // Returns true when the iterator reaches the end of the BufferList.
    bool Done() const
    {
      return mData == mDataEnd;
    }
  };

  // Special convenience method that returns Iter().Data().
  char* Start() { return mSegments[0].mData; }

  IterImpl Iter() const { return IterImpl(*this); }

  // Copies aSize bytes from aData into the BufferList. The storage for these
  // bytes may be split across multiple buffers. Size() is increased by aSize.
  inline bool WriteBytes(const char* aData, size_t aSize);

  // Copies possibly non-contiguous byte range starting at aIter into
  // aData. aIter is advanced by aSize bytes. Returns false if it runs out of
  // data before aSize.
  inline bool ReadBytes(IterImpl& aIter, char* aData, size_t aSize) const;

  // FlattenBytes reconfigures the BufferList so that data in the range
  // [aIter, aIter + aSize) is stored contiguously. A pointer to this data is
  // returned in aOutData. Returns false if not enough data is available. All
  // other iterators are invalidated by this method.
  //
  // This method requires aIter and aSize to be 8-byte aligned.
  inline bool FlattenBytes(IterImpl& aIter, const char** aOutData, size_t aSize);

  // Return a new BufferList that shares storage with this BufferList. The new
  // BufferList is read-only. It allows iteration over aSize bytes starting at
  // aIter. Borrow can fail, in which case *aSuccess will be false upon
  // return. The borrowed BufferList can use a different AllocPolicy than the
  // original one. However, it is not responsible for freeing buffers, so the
  // AllocPolicy is only used for the buffer vector.
  template<typename BorrowingAllocPolicy>
  BufferList<BorrowingAllocPolicy> Borrow(IterImpl& aIter, size_t aSize, bool* aSuccess,
                                          BorrowingAllocPolicy aAP = BorrowingAllocPolicy()) const;

  // Return a new BufferList and move storage from this BufferList to it. The
  // new BufferList owns the buffers. Move can fail, in which case *aSuccess
  // will be false upon return. The new BufferList can use a different
  // AllocPolicy than the original one. The new OtherAllocPolicy is responsible
  // for freeing buffers, so the OtherAllocPolicy must use freeing method
  // compatible to the original one.
  template<typename OtherAllocPolicy>
  BufferList<OtherAllocPolicy> MoveFallible(bool* aSuccess, OtherAllocPolicy aAP = OtherAllocPolicy());

  // Return a new BufferList that adopts the byte range starting at Iter so that
  // range [aIter, aIter + aSize) is transplanted to the returned BufferList.
  // Contents of the buffer before aIter + aSize is left undefined.
  // Extract can fail, in which case *aSuccess will be false upon return. The
  // moved buffers are erased from the original BufferList. In case of extract
  // fails, the original BufferList is intact.  All other iterators except aIter
  // are invalidated.
  // This method requires aIter and aSize to be 8-byte aligned.
  BufferList Extract(IterImpl& aIter, size_t aSize, bool* aSuccess);

private:
  explicit BufferList(AllocPolicy aAP)
   : AllocPolicy(aAP),
     mOwning(false),
     mSize(0),
     mStandardCapacity(0)
  {
  }

  void* AllocateSegment(size_t aSize, size_t aCapacity)
  {
    MOZ_RELEASE_ASSERT(mOwning);

    char* data = this->template pod_malloc<char>(aCapacity);
    if (!data) {
      return nullptr;
    }
    if (!mSegments.append(Segment(data, aSize, aCapacity))) {
      this->free_(data);
      return nullptr;
    }
    mSize += aSize;
    return data;
  }

  bool mOwning;
  Vector<Segment, 1, AllocPolicy> mSegments;
  size_t mSize;
  size_t mStandardCapacity;
};

template<typename AllocPolicy>
bool
BufferList<AllocPolicy>::WriteBytes(const char* aData, size_t aSize)
{
  MOZ_RELEASE_ASSERT(mOwning);
  MOZ_RELEASE_ASSERT(mStandardCapacity);

  size_t copied = 0;
  size_t remaining = aSize;

  if (!mSegments.empty()) {
    Segment& lastSegment = mSegments.back();

    size_t toCopy = std::min(aSize, lastSegment.mCapacity - lastSegment.mSize);
    memcpy(lastSegment.mData + lastSegment.mSize, aData, toCopy);
    lastSegment.mSize += toCopy;
    mSize += toCopy;

    copied += toCopy;
    remaining -= toCopy;
  }

  while (remaining) {
    size_t toCopy = std::min(remaining, mStandardCapacity);

    void* data = AllocateSegment(toCopy, mStandardCapacity);
    if (!data) {
      return false;
    }
    memcpy(data, aData + copied, toCopy);

    copied += toCopy;
    remaining -= toCopy;
  }

  return true;
}

template<typename AllocPolicy>
bool
BufferList<AllocPolicy>::ReadBytes(IterImpl& aIter, char* aData, size_t aSize) const
{
  size_t copied = 0;
  size_t remaining = aSize;
  while (remaining) {
    size_t toCopy = std::min(aIter.RemainingInSegment(), remaining);
    if (!toCopy) {
      // We've run out of data in the last segment.
      return false;
    }
    memcpy(aData + copied, aIter.Data(), toCopy);
    copied += toCopy;
    remaining -= toCopy;

    aIter.Advance(*this, toCopy);
  }

  return true;
}

template<typename AllocPolicy>
bool
BufferList<AllocPolicy>::FlattenBytes(IterImpl& aIter, const char** aOutData, size_t aSize)
{
  MOZ_RELEASE_ASSERT(aSize);
  MOZ_RELEASE_ASSERT(mOwning);

  if (aIter.HasRoomFor(aSize)) {
    // If the data is already contiguous, just return a pointer.
    *aOutData = aIter.Data();
    aIter.Advance(*this, aSize);
    return true;
  }

  // This buffer will become the new contiguous segment.
  char* buffer = this->template pod_malloc<char>(Size());
  if (!buffer) {
    return false;
  }

  size_t copied = 0;
  size_t offset;
  bool found = false;
  for (size_t i = 0; i < mSegments.length(); i++) {
    Segment& segment = mSegments[i];
    memcpy(buffer + copied, segment.Start(), segment.mSize);

    if (i == aIter.mSegment) {
      offset = copied + (aIter.mData - segment.Start());

      // Do we have aSize bytes after aIter?
      if (Size() - offset >= aSize) {
        found = true;
        *aOutData = buffer + offset;

        aIter.mSegment = 0;
        aIter.mData = buffer + offset + aSize;
        aIter.mDataEnd = buffer + Size();
      }
    }

    this->free_(segment.mData);

    copied += segment.mSize;
  }

  mSegments.clear();
  mSegments.infallibleAppend(Segment(buffer, Size(), Size()));

  if (!found) {
    aIter.mSegment = 0;
    aIter.mData = Start();
    aIter.mDataEnd = Start() + Size();
  }

  return found;
}

template<typename AllocPolicy> template<typename BorrowingAllocPolicy>
BufferList<BorrowingAllocPolicy>
BufferList<AllocPolicy>::Borrow(IterImpl& aIter, size_t aSize, bool* aSuccess,
                                BorrowingAllocPolicy aAP) const
{
  BufferList<BorrowingAllocPolicy> result(aAP);

  size_t size = aSize;
  while (size) {
    size_t toAdvance = std::min(size, aIter.RemainingInSegment());

    if (!toAdvance || !result.mSegments.append(typename BufferList<BorrowingAllocPolicy>::Segment(aIter.mData, toAdvance, toAdvance))) {
      *aSuccess = false;
      return result;
    }
    aIter.Advance(*this, toAdvance);
    size -= toAdvance;
  }

  result.mSize = aSize;
  *aSuccess = true;
  return result;
}

template<typename AllocPolicy> template<typename OtherAllocPolicy>
BufferList<OtherAllocPolicy>
BufferList<AllocPolicy>::MoveFallible(bool* aSuccess, OtherAllocPolicy aAP)
{
  BufferList<OtherAllocPolicy> result(0, 0, mStandardCapacity, aAP);

  IterImpl iter = Iter();
  while (!iter.Done()) {
    size_t toAdvance = iter.RemainingInSegment();

    if (!toAdvance || !result.mSegments.append(typename BufferList<OtherAllocPolicy>::Segment(iter.mData, toAdvance, toAdvance))) {
      *aSuccess = false;
      return result;
    }
    iter.Advance(*this, toAdvance);
  }

  result.mSize = mSize;
  mSegments.clear();
  mSize = 0;
  *aSuccess = true;
  return result;
}

template<typename AllocPolicy>
BufferList<AllocPolicy>
BufferList<AllocPolicy>::Extract(IterImpl& aIter, size_t aSize, bool* aSuccess)
{
  MOZ_RELEASE_ASSERT(aSize);
  MOZ_RELEASE_ASSERT(mOwning);
  MOZ_ASSERT(aSize % kSegmentAlignment == 0);
  MOZ_ASSERT(intptr_t(aIter.mData) % kSegmentAlignment == 0);

  IterImpl iter = aIter;
  size_t size = aSize;
  size_t toCopy = std::min(size, aIter.RemainingInSegment());
  MOZ_ASSERT(toCopy % kSegmentAlignment == 0);

  BufferList result(0, toCopy, mStandardCapacity);
  BufferList error(0, 0, mStandardCapacity);

  // Copy the head
  if (!result.WriteBytes(aIter.mData, toCopy)) {
    *aSuccess = false;
    return error;
  }
  iter.Advance(*this, toCopy);
  size -= toCopy;

  // Move segments to result
  auto resultGuard = MakeScopeExit([&] {
    *aSuccess = false;
    result.mSegments.erase(result.mSegments.begin()+1, result.mSegments.end());
  });

  size_t movedSize = 0;
  uintptr_t toRemoveStart = iter.mSegment;
  uintptr_t toRemoveEnd = iter.mSegment;
  while (!iter.Done() &&
         !iter.HasRoomFor(size)) {
    if (!result.mSegments.append(Segment(mSegments[iter.mSegment].mData,
                                         mSegments[iter.mSegment].mSize,
                                         mSegments[iter.mSegment].mCapacity))) {
      return error;
    }
    movedSize += iter.RemainingInSegment();
    size -= iter.RemainingInSegment();
    toRemoveEnd++;
    iter.Advance(*this, iter.RemainingInSegment());
  }

  if (size)  {
    if (!iter.HasRoomFor(size) ||
        !result.WriteBytes(iter.Data(), size)) {
      return error;
    }
    iter.Advance(*this, size);
  }

  mSegments.erase(mSegments.begin() + toRemoveStart, mSegments.begin() + toRemoveEnd);
  mSize -= movedSize;
  aIter.mSegment = iter.mSegment - (toRemoveEnd - toRemoveStart);
  aIter.mData = iter.mData;
  aIter.mDataEnd = iter.mDataEnd;
  MOZ_ASSERT(aIter.mDataEnd == mSegments[aIter.mSegment].End());
  result.mSize = aSize;

  resultGuard.release();
  *aSuccess = true;
  return result;
}

} // namespace mozilla

#endif /* mozilla_BufferList_h */
