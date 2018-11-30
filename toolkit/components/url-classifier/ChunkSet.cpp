//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChunkSet.h"

namespace mozilla {
namespace safebrowsing {

const size_t ChunkSet::IO_BUFFER_SIZE;

nsresult ChunkSet::Serialize(nsACString& aChunkStr) {
  aChunkStr.Truncate();
  for (const Range& range : mRanges) {
    if (&range != &mRanges[0]) {
      aChunkStr.Append(',');
    }

    aChunkStr.AppendInt((int32_t)range.Begin());
    if (range.Begin() != range.End()) {
      aChunkStr.Append('-');
      aChunkStr.AppendInt((int32_t)range.End());
    }
  }

  return NS_OK;
}

nsresult ChunkSet::Set(uint32_t aChunk) {
  if (!Has(aChunk)) {
    Range chunkRange(aChunk, aChunk);

    if (mRanges.Length() == 0) {
      if (!mRanges.AppendElement(chunkRange, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      return NS_OK;
    }

    if (mRanges.LastElement().Precedes(chunkRange)) {
      mRanges.LastElement().End(aChunk);
    } else if (chunkRange.Precedes(mRanges[0])) {
      mRanges[0].Begin(aChunk);
    } else {
      ChunkSet tmp;
      if (!tmp.mRanges.AppendElement(chunkRange, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      return Merge(tmp);
    }
  }

  return NS_OK;
}

bool ChunkSet::Has(uint32_t aChunk) const {
  size_t idx;
  return BinarySearchIf(mRanges, 0, mRanges.Length(),
                        Range::IntersectionComparator(Range(aChunk, aChunk)),
                        &idx);
  // IntersectionComparator works because we create a
  // single-chunk range.
}

nsresult ChunkSet::Merge(const ChunkSet& aOther) {
  size_t oldLen = mRanges.Length();

  for (const Range& mergeRange : aOther.mRanges) {
    if (!HasSubrange(mergeRange)) {
      if (!mRanges.InsertElementSorted(mergeRange, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  if (oldLen < mRanges.Length()) {
    for (size_t i = 1; i < mRanges.Length(); i++) {
      while (mRanges[i - 1].FoldLeft(mRanges[i])) {
        mRanges.RemoveElementAt(i);

        if (i == mRanges.Length()) {
          return NS_OK;
        }
      }
    }
  }

  return NS_OK;
}

uint32_t ChunkSet::Length() const {
  uint32_t len = 0;
  for (const Range& range : mRanges) {
    len += range.Length();
  }

  return len;
}

nsresult ChunkSet::Remove(const ChunkSet& aOther) {
  for (const Range& removalRange : aOther.mRanges) {
    if (mRanges.Length() == 0) {
      return NS_OK;
    }

    if (mRanges.LastElement().End() < removalRange.Begin() ||
        aOther.mRanges.LastElement().End() < mRanges[0].Begin()) {
      return NS_OK;
    }

    size_t intersectionIdx;
    while (BinarySearchIf(mRanges, 0, mRanges.Length(),
                          Range::IntersectionComparator(removalRange),
                          &intersectionIdx)) {
      ChunkSet remains;
      nsresult rv = mRanges[intersectionIdx].Remove(removalRange, remains);

      if (NS_FAILED(rv)) {
        return rv;
      }

      mRanges.RemoveElementAt(intersectionIdx);
      if (!mRanges.InsertElementsAt(intersectionIdx, remains.mRanges,
                                    fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

  return NS_OK;
}

void ChunkSet::Clear() { mRanges.Clear(); }

nsresult ChunkSet::Write(nsIOutputStream* aOut) const {
  nsTArray<uint32_t> chunks(IO_BUFFER_SIZE);

  for (const Range& range : mRanges) {
    for (uint32_t chunk = range.Begin(); chunk <= range.End(); chunk++) {
      chunks.AppendElement(chunk);

      if (chunks.Length() == chunks.Capacity()) {
        nsresult rv = WriteTArray(aOut, chunks);

        if (NS_FAILED(rv)) {
          return rv;
        }

        chunks.Clear();
      }
    }
  }

  nsresult rv = WriteTArray(aOut, chunks);

  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

nsresult ChunkSet::Read(nsIInputStream* aIn, uint32_t aNumElements) {
  nsTArray<uint32_t> chunks(IO_BUFFER_SIZE);

  while (aNumElements != 0) {
    chunks.Clear();

    uint32_t numToRead =
        aNumElements > IO_BUFFER_SIZE ? IO_BUFFER_SIZE : aNumElements;

    nsresult rv = ReadTArray(aIn, &chunks, numToRead);

    if (NS_FAILED(rv)) {
      return rv;
    }

    aNumElements -= numToRead;

    for (uint32_t c : chunks) {
      rv = Set(c);

      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  return NS_OK;
}

bool ChunkSet::HasSubrange(const Range& aSubrange) const {
  for (const Range& range : mRanges) {
    if (range.Contains(aSubrange)) {
      return true;
    } else if (!(aSubrange.Begin() > range.End() ||
                 range.Begin() > aSubrange.End())) {
      // In this case, aSubrange overlaps this range but is not a subrange.
      // because the ChunkSet implementation ensures that there are no
      // overlapping ranges, this means that aSubrange cannot be a subrange of
      // any of the following ranges
      return false;
    }
  }

  return false;
}

uint32_t ChunkSet::Range::Length() const { return mEnd - mBegin + 1; }

nsresult ChunkSet::Range::Remove(const Range& aRange,
                                 ChunkSet& aRemainderSet) const {
  if (mBegin < aRange.mBegin && aRange.mBegin <= mEnd) {
    // aRange overlaps & follows this range
    Range range(mBegin, aRange.mBegin - 1);
    if (!aRemainderSet.mRanges.AppendElement(range, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (mBegin <= aRange.mEnd && aRange.mEnd < mEnd) {
    // aRange overlaps & precedes this range
    Range range(aRange.mEnd + 1, mEnd);
    if (!aRemainderSet.mRanges.AppendElement(range, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

bool ChunkSet::Range::FoldLeft(const Range& aRange) {
  if (Contains(aRange)) {
    return true;
  } else if (Precedes(aRange) ||
             (mBegin <= aRange.mBegin && aRange.mBegin <= mEnd)) {
    mEnd = aRange.mEnd;
    return true;
  }

  return false;
}

}  // namespace safebrowsing
}  // namespace mozilla
