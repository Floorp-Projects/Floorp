/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BASEPROFILEJSONWRITER_H
#define BASEPROFILEJSONWRITER_H

#include "mozilla/JSONWriter.h"
#include "mozilla/UniquePtr.h"

#include <functional>
#include <ostream>
#include <string>

namespace mozilla {
namespace baseprofiler {

class SpliceableJSONWriter;

// On average, profile JSONs are large enough such that we want to avoid
// reallocating its buffer when expanding. Additionally, the contents of the
// profile are not accessed until the profile is entirely written. For these
// reasons we use a chunked writer that keeps an array of chunks, which is
// concatenated together after writing is finished.
class ChunkedJSONWriteFunc final : public JSONWriteFunc {
 public:
  friend class SpliceableJSONWriter;

  ChunkedJSONWriteFunc() : mChunkPtr{nullptr}, mChunkEnd{nullptr} {
    AllocChunk(kChunkSize);
  }

  bool IsEmpty() const {
    MOZ_ASSERT_IF(!mChunkPtr, !mChunkEnd && mChunkList.length() == 0 &&
                                  mChunkLengths.length() == 0);
    return !mChunkPtr;
  }

  void Write(const char* aStr) override {
    size_t len = strlen(aStr);
    Write(aStr, len);
  }
  void Write(const char* aStr, size_t aLen) override {
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
  void CopyDataIntoLazilyAllocatedBuffer(
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
  UniquePtr<char[]> CopyData() const {
    UniquePtr<char[]> c;
    CopyDataIntoLazilyAllocatedBuffer([&](size_t allocationSize) {
      c = MakeUnique<char[]>(allocationSize);
      return c.get();
    });
    return c;
  }
  void Take(ChunkedJSONWriteFunc&& aOther) {
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
  // Returns the byte length of the complete combined string, including the
  // null terminator byte.
  size_t GetTotalLength() const {
    MOZ_ASSERT(mChunkLengths.length() == mChunkList.length());
    size_t totalLen = 1;
    for (size_t i = 0; i < mChunkLengths.length(); i++) {
      MOZ_ASSERT(strlen(mChunkList[i].get()) == mChunkLengths[i]);
      totalLen += mChunkLengths[i];
    }
    return totalLen;
  }

 private:
  void AllocChunk(size_t aChunkSize) {
    MOZ_ASSERT(mChunkLengths.length() == mChunkList.length());
    UniquePtr<char[]> newChunk = MakeUnique<char[]>(aChunkSize);
    mChunkPtr = newChunk.get();
    mChunkEnd = mChunkPtr + aChunkSize;
    *mChunkPtr = '\0';
    MOZ_ALWAYS_TRUE(mChunkLengths.append(0));
    MOZ_ALWAYS_TRUE(mChunkList.append(std::move(newChunk)));
  }

  static const size_t kChunkSize = 4096 * 512;

  // Pointer for writing inside the current chunk.
  //
  // The current chunk is always at the back of mChunkList, i.e.,
  // mChunkList.back() <= mChunkPtr <= mChunkEnd.
  char* mChunkPtr;

  // Pointer to the end of the current chunk.
  //
  // The current chunk is always at the back of mChunkList, i.e.,
  // mChunkEnd >= mChunkList.back() + mChunkLengths.back().
  char* mChunkEnd;

  // List of chunks and their lengths.
  //
  // For all i, the length of the string in mChunkList[i] is
  // mChunkLengths[i].
  Vector<UniquePtr<char[]>> mChunkList;
  Vector<size_t> mChunkLengths;
};

struct OStreamJSONWriteFunc final : public JSONWriteFunc {
  explicit OStreamJSONWriteFunc(std::ostream& aStream) : mStream(aStream) {}

  void Write(const char* aStr) override { mStream << aStr; }
  void Write(const char* aStr, size_t aLen) override { mStream << aStr; }

  std::ostream& mStream;
};

class SpliceableJSONWriter : public JSONWriter {
 public:
  explicit SpliceableJSONWriter(UniquePtr<JSONWriteFunc> aWriter)
      : JSONWriter(std::move(aWriter)) {}

  void StartBareList(CollectionStyle aStyle = MultiLineStyle) {
    StartCollection(nullptr, "", aStyle);
  }

  void EndBareList() { EndCollection(""); }

  void NullElements(uint32_t aCount) {
    for (uint32_t i = 0; i < aCount; i++) {
      NullElement();
    }
  }

  void Splice(const char* aStr) {
    Separator();
    WriteFunc()->Write(aStr);
    mNeedComma[mDepth] = true;
  }

  // Splice the given JSON directly in, without quoting.
  void SplicedJSONProperty(const char* aMaybePropertyName,
                           const char* aJsonValue) {
    Scalar(aMaybePropertyName, aJsonValue);
  }

  // Takes the chunks from aFunc and write them. If move is not possible
  // (e.g., using OStreamJSONWriteFunc), aFunc's chunks are copied and its
  // storage cleared.
  virtual void TakeAndSplice(ChunkedJSONWriteFunc* aFunc) {
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
};

class SpliceableChunkedJSONWriter final : public SpliceableJSONWriter {
 public:
  explicit SpliceableChunkedJSONWriter()
      : SpliceableJSONWriter(MakeUnique<ChunkedJSONWriteFunc>()) {}

  ChunkedJSONWriteFunc* ChunkedWriteFunc() const {
    // The WriteFunc was initialized with a ChunkedJSONWriteFunc in the only
    // constructor above, so it's safe to cast to ChunkedJSONWriteFunc*.
    return static_cast<ChunkedJSONWriteFunc*>(WriteFunc());
  }

  // Adopts the chunks from aFunc without copying.
  void TakeAndSplice(ChunkedJSONWriteFunc* aFunc) override {
    Separator();
    ChunkedWriteFunc()->Take(std::move(*aFunc));
    mNeedComma[mDepth] = true;
  }
};

class JSONSchemaWriter {
  JSONWriter& mWriter;
  uint32_t mIndex;

 public:
  explicit JSONSchemaWriter(JSONWriter& aWriter) : mWriter(aWriter), mIndex(0) {
    aWriter.StartObjectProperty("schema",
                                SpliceableJSONWriter::SingleLineStyle);
  }

  void WriteField(const char* aName) { mWriter.IntProperty(aName, mIndex++); }

  ~JSONSchemaWriter() { mWriter.EndObject(); }
};

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // BASEPROFILEJSONWRITER_H
