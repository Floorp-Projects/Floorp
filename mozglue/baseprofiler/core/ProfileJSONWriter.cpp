/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfileJSONWriter.h"

#include "mozilla/HashFunctions.h"

#include "BaseProfiler.h"

namespace mozilla {
namespace baseprofiler {

void ChunkedJSONWriteFunc::Write(const char* aStr) {
  size_t len = strlen(aStr);
  Write(aStr, len);
}

void ChunkedJSONWriteFunc::Write(const char* aStr, size_t aLen) {
  MOZ_ASSERT(mChunkPtr >= mChunkList.back().get() && mChunkPtr <= mChunkEnd);
  MOZ_ASSERT(mChunkEnd >= mChunkList.back().get() + mChunkLengths.back());
  MOZ_ASSERT(*mChunkPtr == '\0');

  // Most strings to be written are small, but subprocess profiles (e.g.,
  // from the content process in e10s) may be huge. If the string is larger
  // than a chunk, allocate its own chunk.
  char* newPtr;
  if (aLen >= kChunkSize) {
    AllocChunk(aLen + 1);
    newPtr = mChunkPtr + aLen;
  } else {
    newPtr = mChunkPtr + aLen;
    if (newPtr >= mChunkEnd) {
      AllocChunk(kChunkSize);
      newPtr = mChunkPtr + aLen;
    }
  }

  memcpy(mChunkPtr, aStr, aLen);
  *newPtr = '\0';
  mChunkPtr = newPtr;
  mChunkLengths.back() += aLen;
}

size_t ChunkedJSONWriteFunc::GetTotalLength() const {
  MOZ_ASSERT(mChunkLengths.length() == mChunkList.length());
  size_t totalLen = 1;
  for (size_t i = 0; i < mChunkLengths.length(); i++) {
    MOZ_ASSERT(strlen(mChunkList[i].get()) == mChunkLengths[i]);
    totalLen += mChunkLengths[i];
  }
  return totalLen;
}

void ChunkedJSONWriteFunc::CopyDataIntoLazilyAllocatedBuffer(
    const std::function<char*(size_t)>& aAllocator) const {
  size_t totalLen = GetTotalLength();
  char* ptr = aAllocator(totalLen);

  if (!ptr) {
    // Failed to allocate memory.
    return;
  }

  for (size_t i = 0; i < mChunkList.length(); i++) {
    size_t len = mChunkLengths[i];
    memcpy(ptr, mChunkList[i].get(), len);
    ptr += len;
  }
  *ptr = '\0';
}

UniquePtr<char[]> ChunkedJSONWriteFunc::CopyData() const {
  UniquePtr<char[]> c;
  CopyDataIntoLazilyAllocatedBuffer([&](size_t allocationSize) {
    c = MakeUnique<char[]>(allocationSize);
    return c.get();
  });
  return c;
}

void ChunkedJSONWriteFunc::Take(ChunkedJSONWriteFunc&& aOther) {
  for (size_t i = 0; i < aOther.mChunkList.length(); i++) {
    MOZ_ALWAYS_TRUE(mChunkLengths.append(aOther.mChunkLengths[i]));
    MOZ_ALWAYS_TRUE(mChunkList.append(std::move(aOther.mChunkList[i])));
  }
  mChunkPtr = mChunkList.back().get() + mChunkLengths.back();
  mChunkEnd = mChunkPtr;
  aOther.mChunkPtr = nullptr;
  aOther.mChunkEnd = nullptr;
  aOther.mChunkList.clear();
  aOther.mChunkLengths.clear();
}

void ChunkedJSONWriteFunc::AllocChunk(size_t aChunkSize) {
  MOZ_ASSERT(mChunkLengths.length() == mChunkList.length());
  UniquePtr<char[]> newChunk = MakeUnique<char[]>(aChunkSize);
  mChunkPtr = newChunk.get();
  mChunkEnd = mChunkPtr + aChunkSize;
  *mChunkPtr = '\0';
  MOZ_ALWAYS_TRUE(mChunkLengths.append(0));
  MOZ_ALWAYS_TRUE(mChunkList.append(std::move(newChunk)));
}

void SpliceableJSONWriter::TakeAndSplice(ChunkedJSONWriteFunc* aFunc) {
  Separator();
  for (size_t i = 0; i < aFunc->mChunkList.length(); i++) {
    WriteFunc()->Write(aFunc->mChunkList[i].get());
  }
  aFunc->mChunkPtr = nullptr;
  aFunc->mChunkEnd = nullptr;
  aFunc->mChunkList.clear();
  aFunc->mChunkLengths.clear();
  mNeedComma[mDepth] = true;
}

void SpliceableJSONWriter::Splice(const char* aStr) {
  Separator();
  WriteFunc()->Write(aStr);
  mNeedComma[mDepth] = true;
}

void SpliceableChunkedJSONWriter::TakeAndSplice(ChunkedJSONWriteFunc* aFunc) {
  Separator();
  WriteFunc()->Take(std::move(*aFunc));
  mNeedComma[mDepth] = true;
}

}  // namespace baseprofiler
}  // namespace mozilla
