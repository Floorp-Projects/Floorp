/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BASEPROFILEJSONWRITER_H
#define BASEPROFILEJSONWRITER_H

#include "mozilla/HashFunctions.h"
#include "mozilla/HashTable.h"
#include "mozilla/JSONWriter.h"
#include "mozilla/UniquePtr.h"

#include <functional>
#include <ostream>
#include <string_view>

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

  void Write(const Span<const char>& aStr) override {
    MOZ_ASSERT(mChunkPtr >= mChunkList.back().get() && mChunkPtr <= mChunkEnd);
    MOZ_ASSERT(mChunkEnd >= mChunkList.back().get() + mChunkLengths.back());
    MOZ_ASSERT(*mChunkPtr == '\0');

    // Most strings to be written are small, but subprocess profiles (e.g.,
    // from the content process in e10s) may be huge. If the string is larger
    // than a chunk, allocate its own chunk.
    char* newPtr;
    if (aStr.size() >= kChunkSize) {
      AllocChunk(aStr.size() + 1);
      newPtr = mChunkPtr + aStr.size();
    } else {
      newPtr = mChunkPtr + aStr.size();
      if (newPtr >= mChunkEnd) {
        AllocChunk(kChunkSize);
        newPtr = mChunkPtr + aStr.size();
      }
    }

    memcpy(mChunkPtr, aStr.data(), aStr.size());
    *newPtr = '\0';
    mChunkPtr = newPtr;
    mChunkLengths.back() += aStr.size();
  }
  void CopyDataIntoLazilyAllocatedBuffer(
      const std::function<char*(size_t)>& aAllocator) const {
    // Request a buffer for the full content plus a null terminator.
    MOZ_ASSERT(mChunkLengths.length() == mChunkList.length());
    size_t totalLen = 1;
    for (size_t i = 0; i < mChunkLengths.length(); i++) {
      MOZ_ASSERT(strlen(mChunkList[i].get()) == mChunkLengths[i]);
      totalLen += mChunkLengths[i];
    }
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

  void Write(const Span<const char>& aStr) override {
    std::string_view sv(aStr.data(), aStr.size());
    mStream << sv;
  }

  std::ostream& mStream;
};

class SpliceableJSONWriter : public JSONWriter {
 public:
  explicit SpliceableJSONWriter(UniquePtr<JSONWriteFunc> aWriter)
      : JSONWriter(std::move(aWriter)) {}

  void StartBareList(CollectionStyle aStyle = MultiLineStyle) {
    StartCollection(scEmptyString, scEmptyString, aStyle);
  }

  void EndBareList() { EndCollection(scEmptyString); }

  void NullElements(uint32_t aCount) {
    for (uint32_t i = 0; i < aCount; i++) {
      NullElement();
    }
  }

  void Splice(const Span<const char>& aStr) {
    Separator();
    WriteFunc()->Write(aStr);
    mNeedComma[mDepth] = true;
  }

  void Splice(const char* aStr, size_t aLen) {
    Separator();
    WriteFunc()->Write(Span<const char>(aStr, aLen));
    mNeedComma[mDepth] = true;
  }

  // Splice the given JSON directly in, without quoting.
  void SplicedJSONProperty(const Span<const char>& aMaybePropertyName,
                           const Span<const char>& aJsonValue) {
    Scalar(aMaybePropertyName, aJsonValue);
  }

  void CopyAndSplice(const ChunkedJSONWriteFunc& aFunc) {
    Separator();
    for (size_t i = 0; i < aFunc.mChunkList.length(); i++) {
      WriteFunc()->Write(
          Span<const char>(aFunc.mChunkList[i].get(), aFunc.mChunkLengths[i]));
    }
    mNeedComma[mDepth] = true;
  }

  // Takes the chunks from aFunc and write them. If move is not possible
  // (e.g., using OStreamJSONWriteFunc), aFunc's chunks are copied and its
  // storage cleared.
  virtual void TakeAndSplice(ChunkedJSONWriteFunc&& aFunc) {
    Separator();
    for (size_t i = 0; i < aFunc.mChunkList.length(); i++) {
      WriteFunc()->Write(
          Span<const char>(aFunc.mChunkList[i].get(), aFunc.mChunkLengths[i]));
    }
    aFunc.mChunkPtr = nullptr;
    aFunc.mChunkEnd = nullptr;
    aFunc.mChunkList.clear();
    aFunc.mChunkLengths.clear();
    mNeedComma[mDepth] = true;
  }
};

class SpliceableChunkedJSONWriter final : public SpliceableJSONWriter {
 public:
  explicit SpliceableChunkedJSONWriter()
      : SpliceableJSONWriter(MakeUnique<ChunkedJSONWriteFunc>()) {}

  // Access the ChunkedJSONWriteFunc as reference-to-const, usually to copy data
  // out.
  const ChunkedJSONWriteFunc& ChunkedWriteFunc() const {
    MOZ_ASSERT(!mTaken);
    // The WriteFunc was non-fallibly allocated as a ChunkedJSONWriteFunc in the
    // only constructor above, so it's safe to cast to ChunkedJSONWriteFunc*.
    return *static_cast<const ChunkedJSONWriteFunc*>(WriteFunc());
  }

  // Access the ChunkedJSONWriteFunc as rvalue-reference, usually to take its
  // data out. This writer shouldn't be used anymore after this.
  ChunkedJSONWriteFunc&& TakeChunkedWriteFunc() {
#ifdef DEBUG
    MOZ_ASSERT(!mTaken);
    mTaken = true;
#endif  //
    // The WriteFunc was non-fallibly allocated as a ChunkedJSONWriteFunc in the
    // only constructor above, so it's safe to cast to ChunkedJSONWriteFunc*.
    return std::move(*static_cast<ChunkedJSONWriteFunc*>(WriteFunc()));
  }

  // Adopts the chunks from aFunc without copying.
  void TakeAndSplice(ChunkedJSONWriteFunc&& aFunc) override {
    MOZ_ASSERT(!mTaken);
    Separator();
    // The WriteFunc was non-fallibly allocated as a ChunkedJSONWriteFunc in the
    // only constructor above, so it's safe to cast to ChunkedJSONWriteFunc*.
    static_cast<ChunkedJSONWriteFunc*>(WriteFunc())->Take(std::move(aFunc));
    mNeedComma[mDepth] = true;
  }

#ifdef DEBUG
 private:
  bool mTaken = false;
#endif  //
};

class JSONSchemaWriter {
  JSONWriter& mWriter;
  uint32_t mIndex;

 public:
  explicit JSONSchemaWriter(JSONWriter& aWriter) : mWriter(aWriter), mIndex(0) {
    aWriter.StartObjectProperty("schema",
                                SpliceableJSONWriter::SingleLineStyle);
  }

  void WriteField(const Span<const char>& aName) {
    mWriter.IntProperty(aName, mIndex++);
  }

  template <size_t Np1>
  void WriteField(const char (&aName)[Np1]) {
    WriteField(Span<const char>(aName, Np1 - 1));
  }

  ~JSONSchemaWriter() { mWriter.EndObject(); }
};

// This class helps create an indexed list of unique strings, and inserts the
// index as a JSON value. The collected list of unique strings can later be
// inserted as a JSON array.
// This can be useful for elements/properties with many repeated strings.
//
// With only JSONWriter w,
// `w.WriteElement("a"); w.WriteElement("b"); w.WriteElement("a");`
// when done inside a JSON array, will generate:
// `["a", "b", "c"]`
//
// With UniqueStrings u,
// `u.WriteElement(w, "a"); u.WriteElement(w, "b"); u.WriteElement(w, "a");`
// when done inside a JSON array, will generate:
// `[0, 1, 0]`
// and later, `u.SpliceStringTableElements(w)` (inside a JSON array), will
// output the corresponding indexed list of unique strings:
// `["a", "b"]`
class UniqueJSONStrings {
 public:
  // Start an empty list of unique strings.
  MFBT_API UniqueJSONStrings();

  // Start with a copy of the strings from another list.
  MFBT_API explicit UniqueJSONStrings(const UniqueJSONStrings& aOther);

  MFBT_API ~UniqueJSONStrings();

  // Add `aStr` to the list (if not already there), and write its index as a
  // named object property.
  void WriteProperty(JSONWriter& aWriter, const Span<const char>& aName,
                     const Span<const char>& aStr) {
    aWriter.IntProperty(aName, GetOrAddIndex(aStr));
  }

  // Add `aStr` to the list (if not already there), and write its index as an
  // array element.
  void WriteElement(JSONWriter& aWriter, const Span<const char>& aStr) {
    aWriter.IntElement(GetOrAddIndex(aStr));
  }

  // Splice all collected unique strings into an array. This should only be done
  // once, and then this UniqueStrings shouldn't be used anymore.
  MFBT_API void SpliceStringTableElements(SpliceableJSONWriter& aWriter);

 private:
  // If `aStr` is already listed, return its index.
  // Otherwise add it to the list and return the new index.
  MFBT_API uint32_t GetOrAddIndex(const Span<const char>& aStr);

  SpliceableChunkedJSONWriter mStringTableWriter;
  HashMap<HashNumber, uint32_t> mStringHashToIndexMap;
};

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // BASEPROFILEJSONWRITER_H
