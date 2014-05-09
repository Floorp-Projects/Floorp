//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChunkSet.h"

namespace mozilla {
namespace safebrowsing {

nsresult
ChunkSet::Serialize(nsACString& aChunkStr)
{
  aChunkStr.Truncate();

  uint32_t i = 0;
  while (i < mChunks.Length()) {
    if (i != 0) {
      aChunkStr.Append(',');
    }
    aChunkStr.AppendInt((int32_t)mChunks[i]);

    uint32_t first = i;
    uint32_t last = first;
    i++;
    while (i < mChunks.Length() && (mChunks[i] == mChunks[i - 1] + 1 || mChunks[i] == mChunks[i - 1])) {
      last = i++;
    }

    if (last != first) {
      aChunkStr.Append('-');
      aChunkStr.AppendInt((int32_t)mChunks[last]);
    }
  }

  return NS_OK;
}

nsresult
ChunkSet::Set(uint32_t aChunk)
{
  size_t idx = mChunks.BinaryIndexOf(aChunk);
  if (idx == nsTArray<uint32_t>::NoIndex) {
    mChunks.InsertElementSorted(aChunk);
  }
  return NS_OK;
}

nsresult
ChunkSet::Unset(uint32_t aChunk)
{
  mChunks.RemoveElementSorted(aChunk);

  return NS_OK;
}

bool
ChunkSet::Has(uint32_t aChunk) const
{
  return mChunks.BinaryIndexOf(aChunk) != nsTArray<uint32_t>::NoIndex;
}

nsresult
ChunkSet::Merge(const ChunkSet& aOther)
{
  const uint32_t *dupIter = aOther.mChunks.Elements();
  const uint32_t *end = aOther.mChunks.Elements() + aOther.mChunks.Length();

  for (const uint32_t *iter = dupIter; iter != end; iter++) {
    nsresult rv = Set(*iter);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
ChunkSet::Remove(const ChunkSet& aOther)
{
  uint32_t *addIter = mChunks.Elements();
  uint32_t *end = mChunks.Elements() + mChunks.Length();

  for (uint32_t *iter = addIter; iter != end; iter++) {
    if (!aOther.Has(*iter)) {
      *addIter = *iter;
      addIter++;
    }
  }

  mChunks.SetLength(addIter - mChunks.Elements());

  return NS_OK;
}

void
ChunkSet::Clear()
{
  mChunks.Clear();
}

}
}
