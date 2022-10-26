/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StreamBufferSourceImpl_h
#define mozilla_StreamBufferSourceImpl_h

#include "mozilla/StreamBufferSource.h"

#include "nsTArray.h"

namespace mozilla {

class nsTArraySource final : public StreamBufferSource {
 public:
  explicit nsTArraySource(nsTArray<uint8_t>&& aArray)
      : mArray(std::move(aArray)) {}

  Span<const char> Data() override {
    return Span{reinterpret_cast<const char*>(mArray.Elements()),
                mArray.Length()};
  }

  bool Owning() override { return true; }

  size_t SizeOfExcludingThisEvenIfShared(MallocSizeOf aMallocSizeOf) override {
    return mArray.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  const nsTArray<uint8_t> mArray;
};

class nsCStringSource final : public StreamBufferSource {
 public:
  explicit nsCStringSource(nsACString&& aString)
      : mString(std::move(aString)) {}

  Span<const char> Data() override { return mString; }

  nsresult GetData(nsACString& aString) override {
    if (!aString.Assign(mString, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
  }

  bool Owning() override { return true; }

  size_t SizeOfExcludingThisIfUnshared(MallocSizeOf aMallocSizeOf) override {
    return mString.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }

  size_t SizeOfExcludingThisEvenIfShared(MallocSizeOf aMallocSizeOf) override {
    return mString.SizeOfExcludingThisEvenIfShared(aMallocSizeOf);
  }

 private:
  const nsCString mString;
};

class nsBorrowedSource final : public StreamBufferSource {
 public:
  explicit nsBorrowedSource(Span<const char> aBuffer) : mBuffer(aBuffer) {}

  Span<const char> Data() override { return mBuffer; }

  bool Owning() override { return false; }

  size_t SizeOfExcludingThisEvenIfShared(MallocSizeOf aMallocSizeOf) override {
    return 0;
  }

 private:
  const Span<const char> mBuffer;
};

}  // namespace mozilla

#endif  // mozilla_StreamBufferSourceImpl_h
