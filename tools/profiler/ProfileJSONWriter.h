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

class ChunkedJSONWriteFunc : public mozilla::JSONWriteFunc
{
  friend class SpliceableJSONWriter;

  const static size_t kChunkSize = 4096 * 512;
  char* mChunkPtr;
  char* mChunkEnd;
  mozilla::Vector<mozilla::UniquePtr<char[]>> mChunkList;
  mozilla::Vector<size_t> mChunkLengths;

  void AllocChunk() {
    MOZ_ASSERT(mChunkLengths.length() == mChunkList.length());
    mozilla::UniquePtr<char[]> newChunk = mozilla::MakeUnique<char[]>(kChunkSize);
    mChunkPtr = newChunk.get();
    mChunkEnd = mChunkPtr + kChunkSize;
    MOZ_ALWAYS_TRUE(mChunkLengths.append(0));
    MOZ_ALWAYS_TRUE(mChunkList.append(mozilla::Move(newChunk)));
  }

public:
  ChunkedJSONWriteFunc() {
    AllocChunk();
  }

  void Write(const char* aStr) override {
    MOZ_ASSERT(strlen(aStr) < kChunkSize);

    size_t len = strlen(aStr);
    char* newPtr = mChunkPtr + len;
    if (newPtr >= mChunkEnd) {
      MOZ_ASSERT(*mChunkPtr == '\0');
      AllocChunk();
      newPtr = mChunkPtr + len;
    }

    memcpy(mChunkPtr, aStr, len);
    mChunkPtr = newPtr;
    mChunkLengths.back() += len;
    *mChunkPtr = '\0';
  }

  mozilla::UniquePtr<char[]> CopyData() {
    MOZ_ASSERT(mChunkLengths.length() == mChunkList.length());
    size_t totalLen = 1;
    for (size_t i = 0; i < mChunkLengths.length(); i++) {
      MOZ_ASSERT(strlen(mChunkList[i].get()) == mChunkLengths[i]);
      totalLen += mChunkLengths[i];
    }
    mozilla::UniquePtr<char[]> c = mozilla::MakeUnique<char[]>(totalLen);
    char* ptr = c.get();
    for (size_t i = 0; i < mChunkList.length(); i++) {
      size_t len = mChunkLengths[i];
      memcpy(ptr, mChunkList[i].get(), len);
      ptr += len;
    }
    *ptr = '\0';
    return c;
  }
};

struct OStreamJSONWriteFunc : public mozilla::JSONWriteFunc
{
  std::ostream& mStream;

  explicit OStreamJSONWriteFunc(std::ostream& aStream)
    : mStream(aStream)
  { }

  void Write(const char* aStr) override {
    mStream << aStr;
  }
};

class SpliceableJSONWriter : public mozilla::JSONWriter
{
public:
  explicit SpliceableJSONWriter(mozilla::UniquePtr<mozilla::JSONWriteFunc> aWriter)
    : JSONWriter(mozilla::Move(aWriter))
  { }

  void StartBareList(CollectionStyle aStyle = SingleLineStyle) {
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
};

#endif // PROFILEJSONWRITER_H
