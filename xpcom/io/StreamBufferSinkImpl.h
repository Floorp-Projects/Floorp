/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StreamBufferSinkImpl_h
#define mozilla_StreamBufferSinkImpl_h

#include "mozilla/Buffer.h"
#include "mozilla/StreamBufferSink.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class BufferSink final : public StreamBufferSink {
 public:
  explicit BufferSink(Buffer<char>&& aBuffer) : mBuffer(std::move(aBuffer)) {}

  explicit BufferSink(size_t aLength) : mBuffer(aLength) {}

  static UniquePtr<BufferSink> Alloc(size_t aLength) {
    auto maybeBuffer = Buffer<char>::Alloc(aLength);
    if (!maybeBuffer) {
      return nullptr;
    }

    return MakeUnique<BufferSink>(maybeBuffer.extract());
  }

  mozilla::Span<char> Data() override { return mBuffer.AsWritableSpan(); }

 private:
  Buffer<char> mBuffer;
};

class nsBorrowedSink final : public StreamBufferSink {
 public:
  explicit nsBorrowedSink(mozilla::Span<char> aBuffer) : mBuffer(aBuffer) {}

  mozilla::Span<char> Data() override { return mBuffer; }

 private:
  mozilla::Span<char> mBuffer;
};

}  // namespace mozilla

#endif  // mozilla_StreamBufferSinkImpl_h
