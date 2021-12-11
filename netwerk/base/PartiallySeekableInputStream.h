/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PartiallySeekableInputStream_h
#define PartiallySeekableInputStream_h

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIInputStreamLength.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsISeekableStream.h"

namespace mozilla {
namespace net {

// A wrapper for making a stream seekable for the first |aBufferSize| bytes.
// Note that this object takes the ownership of the underlying stream.

class PartiallySeekableInputStream final : public nsISeekableStream,
                                           public nsIAsyncInputStream,
                                           public nsICloneableInputStream,
                                           public nsIIPCSerializableInputStream,
                                           public nsIInputStreamCallback,
                                           public nsIInputStreamLength,
                                           public nsIAsyncInputStreamLength,
                                           public nsIInputStreamLengthCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSITELLABLESTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIINPUTSTREAMLENGTH
  NS_DECL_NSIASYNCINPUTSTREAMLENGTH
  NS_DECL_NSIINPUTSTREAMLENGTHCALLBACK

  explicit PartiallySeekableInputStream(
      already_AddRefed<nsIInputStream> aInputStream,
      uint64_t aBufferSize = 4096);

 private:
  PartiallySeekableInputStream(
      already_AddRefed<nsIInputStream> aClonedBaseStream,
      PartiallySeekableInputStream* aClonedFrom);

  ~PartiallySeekableInputStream() = default;

  void Init();

  template <typename M>
  void SerializeInternal(mozilla::ipc::InputStreamParams& aParams,
                         FileDescriptorArray& aFileDescriptors,
                         bool aDelayedStart, uint32_t aMaxSize,
                         uint32_t* aSizeUsed, M* aManager);

  nsCOMPtr<nsIInputStream> mInputStream;

  // Raw pointers because these are just QI of mInputStream.
  nsICloneableInputStream* mWeakCloneableInputStream;
  nsIIPCSerializableInputStream* mWeakIPCSerializableInputStream;
  nsIAsyncInputStream* mWeakAsyncInputStream;
  nsIInputStreamLength* mWeakInputStreamLength;
  nsIAsyncInputStreamLength* mWeakAsyncInputStreamLength;

  // Protected by mutex.
  nsCOMPtr<nsIInputStreamCallback> mAsyncWaitCallback;

  // Protected by mutex.
  nsCOMPtr<nsIInputStreamLengthCallback> mAsyncInputStreamLengthCallback;

  nsTArray<char> mCachedBuffer;

  uint64_t mBufferSize;
  uint64_t mPos;
  bool mClosed;

  Mutex mMutex;
};

}  // namespace net
}  // namespace mozilla

#endif  // PartiallySeekableInputStream_h
