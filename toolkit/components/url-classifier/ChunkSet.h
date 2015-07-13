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
 * Store the chunk numbers as an array of uint32_t. We need chunk numbers in
 * order to ask for incremental updates from the server.
 * XXX: We should optimize this further to compress the many consecutive
 * numbers.
 */
class ChunkSet {
public:
  ChunkSet() {}
  ~ChunkSet() {}

  nsresult Serialize(nsACString& aStr);
  nsresult Set(uint32_t aChunk);
  bool Has(uint32_t chunk) const;
  nsresult Merge(const ChunkSet& aOther);
  uint32_t Length() const { return mChunks.Length(); }
  nsresult Remove(const ChunkSet& aOther);
  void Clear();

  nsresult Write(nsIOutputStream* aOut) {
    return WriteTArray(aOut, mChunks);
  }
  nsresult Read(nsIInputStream* aIn, uint32_t aNumElements) {
    return ReadTArray(aIn, &mChunks, aNumElements);
  }

private:
  FallibleTArray<uint32_t> mChunks;
};

} // namespace safebrowsing
} // namespace mozilla

#endif
