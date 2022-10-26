/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FixedBufferOutputStream_h
#define mozilla_FixedBufferOutputStream_h

#include <cstddef>
#include "mozilla/fallible.h"
#include "mozilla/Mutex.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"
#include "nsIOutputStream.h"
#include "nsStringFwd.h"

template <class T>
class RefPtr;

namespace mozilla {

class StreamBufferSink;

// An output stream so you can read your potentially-async input stream into
// a contiguous buffer using NS_AsyncCopy. Back when streams were more
// synchronous and people didn't know blocking I/O was bad, if you wanted to
// read a stream into a flat buffer, you could use NS_ReadInputStreamToString
// or NS_ReadInputStreamToBuffer.  But those don't work with async streams.
// This can be used to replace hand-rolled Read/AsyncWait() loops.  Because you
// specify the expected size up front, the buffer is pre-allocated so wasteful
// reallocations can be avoided.
class FixedBufferOutputStream final : public nsIOutputStream {
  template <typename T, typename... Args>
  friend RefPtr<T> MakeRefPtr(Args&&... aArgs);

 public:
  // Factory method to get a FixedBufferOutputStream by allocating a char buffer
  // with the given length.
  static RefPtr<FixedBufferOutputStream> Create(size_t aLength);

  // Factory method to get a FixedBufferOutputStream by allocating a char buffer
  // with the given length, fallible version.
  static RefPtr<FixedBufferOutputStream> Create(size_t aLength,
                                                const mozilla::fallible_t&);

  // Factory method to get a FixedBufferOutputStream from a preallocated char
  // buffer. The output stream doesn't take ownership of the char buffer, so the
  // char buffer must outlive the output stream (to avoid a use-after-free).
  static RefPtr<FixedBufferOutputStream> Create(mozilla::Span<char> aBuffer);

  // Factory method to get a FixedBufferOutputStream from an arbitrary
  // StreamBufferSink.
  static RefPtr<FixedBufferOutputStream> Create(
      UniquePtr<StreamBufferSink>&& aSink);

  nsDependentCSubstring WrittenData();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM

 private:
  explicit FixedBufferOutputStream(UniquePtr<StreamBufferSink>&& aSink);

  ~FixedBufferOutputStream() = default;

  const UniquePtr<StreamBufferSink> mSink;

  Mutex mMutex;

  size_t mOffset MOZ_GUARDED_BY(mMutex);
  bool mWriting MOZ_GUARDED_BY(mMutex);
  bool mClosed MOZ_GUARDED_BY(mMutex);
};

}  // namespace mozilla

#endif  // mozilla_FixedBufferOutputStream_h
