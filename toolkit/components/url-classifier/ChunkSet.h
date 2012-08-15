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
 * Store the chunks as an array of uint32.
 * XXX: We should optimize this further to compress the
 * many consecutive numbers.
 */
class ChunkSet {
public:
  ChunkSet() {}
  ~ChunkSet() {}

  nsresult Serialize(nsACString& aStr);
  nsresult Set(PRUint32 aChunk);
  nsresult Unset(PRUint32 aChunk);
  void Clear();
  nsresult Merge(const ChunkSet& aOther);
  nsresult Remove(const ChunkSet& aOther);

  bool Has(PRUint32 chunk) const;

  uint32 Length() const { return mChunks.Length(); }

  nsresult Write(nsIOutputStream* aOut) {
    return WriteTArray(aOut, mChunks);
  }

  nsresult Read(nsIInputStream* aIn, PRUint32 aNumElements) {
    return ReadTArray(aIn, &mChunks, aNumElements);
  }

  uint32 *Begin() { return mChunks.Elements(); }
  uint32 *End() { return mChunks.Elements() + mChunks.Length(); }

private:
  nsTArray<uint32> mChunks;
};

}
}

#endif
