/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SlicedInputStream_h
#define SlicedInputStream_h

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsISeekableStream.h"
#include "nsIInputStreamLength.h"

namespace mozilla {

// A wrapper for a slice of an underlying input stream.

class SlicedInputStream final : public nsIAsyncInputStream,
                                public nsICloneableInputStream,
                                public nsIIPCSerializableInputStream,
                                public nsISeekableStream,
                                public nsIInputStreamCallback,
                                public nsIInputStreamLength,
                                public nsIAsyncInputStreamLength,
                                public nsIInputStreamLengthCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSITELLABLESTREAM
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIINPUTSTREAMLENGTH
  NS_DECL_NSIASYNCINPUTSTREAMLENGTH
  NS_DECL_NSIINPUTSTREAMLENGTHCALLBACK

  // Create an input stream whose data comes from a slice of aInputStream.  The
  // slice begins at aStart bytes beyond aInputStream's current position, and
  // extends for a maximum of aLength bytes.  If aInputStream contains fewer
  // than aStart bytes, reading from SlicedInputStream returns no data.  If
  // aInputStream contains more than aStart bytes, but fewer than aStart +
  // aLength bytes, reading from SlicedInputStream returns as many bytes as can
  // be consumed from aInputStream after reading aLength bytes.
  //
  // aInputStream should not be read from after constructing a
  // SlicedInputStream wrapper around it.

  SlicedInputStream(already_AddRefed<nsIInputStream> aInputStream,
                    uint64_t aStart, uint64_t aLength);

  // This CTOR is meant to be used just for IPC.
  SlicedInputStream();

 private:
  ~SlicedInputStream();

  template <typename M>
  void SerializeInternal(mozilla::ipc::InputStreamParams& aParams,
                         FileDescriptorArray& aFileDescriptors,
                         bool aDelayedStart, uint32_t aMaxSize,
                         uint32_t* aSizeUsed, M* aManager);

  void SetSourceStream(already_AddRefed<nsIInputStream> aInputStream);

  uint64_t AdjustRange(uint64_t aRange);

  nsCOMPtr<nsIInputStream> mInputStream;

  // Raw pointers because these are just QI of mInputStream.
  nsICloneableInputStream* mWeakCloneableInputStream;
  nsIIPCSerializableInputStream* mWeakIPCSerializableInputStream;
  nsISeekableStream* mWeakSeekableInputStream;
  nsITellableStream* mWeakTellableInputStream;
  nsIAsyncInputStream* mWeakAsyncInputStream;
  nsIInputStreamLength* mWeakInputStreamLength;
  nsIAsyncInputStreamLength* mWeakAsyncInputStreamLength;

  uint64_t mStart;
  uint64_t mLength;
  uint64_t mCurPos;

  bool mClosed;

  // These four are used for AsyncWait. They are protected by mutex because
  // touched on multiple threads.
  nsCOMPtr<nsIInputStreamCallback> mAsyncWaitCallback;
  nsCOMPtr<nsIEventTarget> mAsyncWaitEventTarget;
  uint32_t mAsyncWaitFlags;
  uint32_t mAsyncWaitRequestedCount;

  // This is use for nsIAsyncInputStreamLength::AsyncWait.
  // This is protected by mutex.
  nsCOMPtr<nsIInputStreamLengthCallback> mAsyncWaitLengthCallback;

  Mutex mMutex;
};

}  // namespace mozilla

#endif  // SlicedInputStream_h
