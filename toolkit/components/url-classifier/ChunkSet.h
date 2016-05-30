//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChunkSet_h__
#define ChunkSet_h__

#include "Entries.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace safebrowsing {

/**
 * Store the chunk numbers as an array of ranges of uint32_t.
 * We need chunk numbers in order to ask for incremental updates from the
 * server.
 */
class ChunkSet {
public:
  nsresult Serialize(nsACString& aStr);
  nsresult Set(uint32_t aChunk);
  bool Has(uint32_t chunk) const;
  nsresult Merge(const ChunkSet& aOther);
  uint32_t Length() const;
  nsresult Remove(const ChunkSet& aOther);
  void Clear();

  nsresult Write(nsIOutputStream* aOut);
  nsresult Read(nsIInputStream* aIn, uint32_t aNumElements);

private:
  class Range {
  public:
    Range(uint32_t aBegin, uint32_t aEnd) : mBegin(aBegin), mEnd(aEnd) {}

    uint32_t Length() const;
    nsresult Remove(const Range& aRange, ChunkSet& aRemainderSet) const;
    bool FoldLeft(const Range& aRange);

    bool operator==(const Range& rhs) const {
      return mBegin == rhs.mBegin;
    }
    bool operator<(const Range& rhs) const {
      return mBegin < rhs.mBegin;
    }

    uint32_t Begin() const {
      return mBegin;
    }
    void Begin(const uint32_t aBegin) {
      mBegin = aBegin;
    }
    uint32_t End() const {
      return mEnd;
    }
    void End(const uint32_t aEnd) {
      mEnd = aEnd;
    }

    bool Contains(const Range& aRange) const {
      return mBegin <= aRange.mBegin && aRange.mEnd <= mEnd;
    }
    bool Precedes(const Range& aRange) const {
      return mEnd + 1 == aRange.mBegin;
    }

    struct IntersectionComparator {
      int operator()(const Range& aRange) const {
        if (aRange.mBegin > mTarget.mEnd) {
          return -1;
        }
        if (mTarget.mBegin > aRange.mEnd) {
          return 1;
        }
        return 0;
      }

      explicit IntersectionComparator(const Range& aTarget) : mTarget(aTarget){}
      const Range& mTarget;
    };

  private:
    uint32_t mBegin;
    uint32_t mEnd;
  };

  static const size_t IO_BUFFER_SIZE = 1024;
  FallibleTArray<Range> mRanges;

  bool HasSubrange(const Range& aSubrange) const;
};

} // namespace safebrowsing
} // namespace mozilla

#endif
