/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InputStreamLengthWrapper_h
#define InputStreamLengthWrapper_h

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsISeekableStream.h"
#include "nsIInputStreamLength.h"

namespace mozilla {

// A wrapper keeps an inputStream together with its length.
// This class can be used for nsIInputStreams that do not implement
// nsIInputStreamLength.

class InputStreamLengthWrapper final : public nsIAsyncInputStream
                                     , public nsICloneableInputStream
                                     , public nsIIPCSerializableInputStream
                                     , public nsISeekableStream
                                     , public nsIInputStreamCallback
                                     , public nsIInputStreamLength
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIINPUTSTREAMLENGTH

  // This method creates a InputStreamLengthWrapper around aInputStream if
  // this doesn't implement nsIInputStreamLength or
  // nsIInputStreamAsyncLength interface, but it implements
  // nsIAsyncInputStream. For this kind of streams,
  // InputStreamLengthHelper is not able to retrieve the length. This
  // method will make such streams ready to be used with
  // InputStreamLengthHelper.
  static already_AddRefed<nsIInputStream>
  MaybeWrap(already_AddRefed<nsIInputStream> aInputStream,
            int64_t aLength);

  // The length here will be used when nsIInputStreamLength::Length() is called.
  InputStreamLengthWrapper(already_AddRefed<nsIInputStream> aInputStream,
                           int64_t aLength);

  // This CTOR is meant to be used just for IPC.
  InputStreamLengthWrapper();

private:
  ~InputStreamLengthWrapper();

  void
  SetSourceStream(already_AddRefed<nsIInputStream> aInputStream);

  nsCOMPtr<nsIInputStream> mInputStream;

  // Raw pointers because these are just QI of mInputStream.
  nsICloneableInputStream* mWeakCloneableInputStream;
  nsIIPCSerializableInputStream* mWeakIPCSerializableInputStream;
  nsISeekableStream* mWeakSeekableInputStream;
  nsIAsyncInputStream* mWeakAsyncInputStream;

  int64_t mLength;
  bool mConsumed;

  mozilla::Mutex mMutex;

  // This is used for AsyncWait and it's protected by mutex.
  nsCOMPtr<nsIInputStreamCallback> mAsyncWaitCallback;
};

} // mozilla namespace

#endif // InputStreamLengthWrapper_h
