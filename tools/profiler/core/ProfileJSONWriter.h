/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PROFILEJSONWRITER_H
#define PROFILEJSONWRITER_H

#include <ostream>
#include <string>
#include <string.h>

#include "mozilla/JSONWriter.h"
#include "mozilla/UniquePtr.h"

class SpliceableChunkedJSONWriter;

// On average, profile JSONs are large enough such that we want to avoid
// reallocating its buffer when expanding. Additionally, the contents of the
// profile are not accessed until the profile is entirely written. For these
// reasons we use a chunked writer that keeps an array of chunks, which is
// concatenated together after writing is finished.
class ChunkedJSONWriteFunc : public mozilla::JSONWriteFunc
{
public:
  friend class SpliceableJSONWriter;

  ChunkedJSONWriteFunc() : mChunkPtr{nullptr}, mChunkEnd{nullptr} {
    AllocChunk(kChunkSize);
  }

  bool IsEmpty() const {
    MOZ_ASSERT_IF(!mChunkPtr, !mChunkEnd &&
                              mChunkList.length() == 0 &&
                              mChunkLengths.length() == 0);
    return !mChunkPtr;
  }

  void Write(const char* aStr) override;
  mozilla::UniquePtr<char[]> CopyData() const;
  void Take(ChunkedJSONWriteFunc&& aOther);

private:
  void AllocChunk(size_t aChunkSize);

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
  mozilla::Vector<mozilla::UniquePtr<char[]>> mChunkList;
  mozilla::Vector<size_t> mChunkLengths;
};

struct OStreamJSONWriteFunc : public mozilla::JSONWriteFunc
{
  explicit OStreamJSONWriteFunc(std::ostream& aStream)
    : mStream(aStream)
  { }

  void Write(const char* aStr) override {
    mStream << aStr;
  }

  std::ostream& mStream;
};

class SpliceableJSONWriter : public mozilla::JSONWriter
{
public:
  explicit SpliceableJSONWriter(mozilla::UniquePtr<mozilla::JSONWriteFunc> aWriter)
    : JSONWriter(std::move(aWriter))
  { }

  void StartBareList(CollectionStyle aStyle = MultiLineStyle) {
    StartCollection(nullptr, "", aStyle);
  }

  void EndBareList() {
    EndCollection("");
  }

  void NullElements(uint32_t aCount) {
    for (uint32_t i = 0; i < aCount; i++) {
      NullElement();
    }
  }

  void Splice(const ChunkedJSONWriteFunc* aFunc);
  void Splice(const char* aStr);

  // Splice the given JSON directly in, without quoting.
  void SplicedJSONProperty(const char* aMaybePropertyName,
                           const char* aJsonValue)
  {
    Scalar(aMaybePropertyName, aJsonValue);
  }

  // Takes the chunks from aFunc and write them. If move is not possible
  // (e.g., using OStreamJSONWriteFunc), aFunc's chunks are copied and its
  // storage cleared.
  virtual void TakeAndSplice(ChunkedJSONWriteFunc* aFunc);
};

class SpliceableChunkedJSONWriter : public SpliceableJSONWriter
{
public:
  explicit SpliceableChunkedJSONWriter()
    : SpliceableJSONWriter(mozilla::MakeUnique<ChunkedJSONWriteFunc>())
  { }

  ChunkedJSONWriteFunc* WriteFunc() const {
    return static_cast<ChunkedJSONWriteFunc*>(JSONWriter::WriteFunc());
  }

  // Adopts the chunks from aFunc without copying.
  virtual void TakeAndSplice(ChunkedJSONWriteFunc* aFunc) override;
};

#endif // PROFILEJSONWRITER_H
