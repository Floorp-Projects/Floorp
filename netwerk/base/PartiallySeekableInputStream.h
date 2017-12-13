/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PartiallySeekableInputStream_h
#define PartiallySeekableInputStream_h

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsISeekableStream.h"

namespace mozilla {
namespace net {

// A wrapper for making a stream seekable for the first |aBufferSize| bytes.
// Note that this object takes the ownership of the underlying stream.

class PartiallySeekableInputStream final : public nsISeekableStream
                                         , public nsIAsyncInputStream
                                         , public nsICloneableInputStream
                                         , public nsIIPCSerializableInputStream
                                         , public nsIInputStreamCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMCALLBACK

  explicit PartiallySeekableInputStream(already_AddRefed<nsIInputStream> aInputStream,
                                        uint64_t aBufferSize = 4096);

private:
  PartiallySeekableInputStream(already_AddRefed<nsIInputStream> aClonedBaseStream,
                               PartiallySeekableInputStream* aClonedFrom);

  ~PartiallySeekableInputStream() = default;

  void
  Init();

  nsCOMPtr<nsIInputStream> mInputStream;

  // Raw pointers because these are just QI of mInputStream.
  nsICloneableInputStream* mWeakCloneableInputStream;
  nsIIPCSerializableInputStream* mWeakIPCSerializableInputStream;
  nsIAsyncInputStream* mWeakAsyncInputStream;

  nsCOMPtr<nsIInputStreamCallback> mAsyncWaitCallback;

  nsTArray<char> mCachedBuffer;

  uint64_t mBufferSize;
  uint64_t mPos;
  bool mClosed;
};

} // net namespace
} // mozilla namespace

#endif // PartiallySeekableInputStream_h
