/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBufferedStreams_h__
#define nsBufferedStreams_h__

#include "nsIBufferedStreams.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"
#include "nsISeekableStream.h"
#include "nsIStreamBufferAccess.h"
#include "nsCOMPtr.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIAsyncInputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIInputStreamLength.h"
#include "mozilla/Mutex.h"

////////////////////////////////////////////////////////////////////////////////

class nsBufferedStream : public nsISeekableStream {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSITELLABLESTREAM

  nsBufferedStream();

  nsresult Close();

 protected:
  virtual ~nsBufferedStream();

  nsresult Init(nsISupports* stream, uint32_t bufferSize);
  nsresult GetData(nsISupports** aResult);
  NS_IMETHOD Fill() = 0;
  NS_IMETHOD Flush() = 0;

  uint32_t mBufferSize;
  char* mBuffer;

  // mBufferStartOffset is the offset relative to the start of mStream.
  int64_t mBufferStartOffset;

  // mCursor is the read cursor for input streams, or write cursor for
  // output streams, and is relative to mBufferStartOffset.
  uint32_t mCursor;

  // mFillPoint is the amount available in the buffer for input streams,
  // or the high watermark of bytes written into the buffer, and therefore
  // is relative to mBufferStartOffset.
  uint32_t mFillPoint;

  nsCOMPtr<nsISupports> mStream;  // cast to appropriate subclass

  bool mBufferDisabled;
  bool mEOF;  // True if mStream is at EOF
  uint8_t mGetBufferCount;
};

////////////////////////////////////////////////////////////////////////////////

class nsBufferedInputStream final : public nsBufferedStream,
                                    public nsIBufferedInputStream,
                                    public nsIStreamBufferAccess,
                                    public nsIIPCSerializableInputStream,
                                    public nsIAsyncInputStream,
                                    public nsIInputStreamCallback,
                                    public nsICloneableInputStream,
                                    public nsIInputStreamLength,
                                    public nsIAsyncInputStreamLength,
                                    public nsIInputStreamLengthCallback {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIBUFFEREDINPUTSTREAM
  NS_DECL_NSISTREAMBUFFERACCESS
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMLENGTH
  NS_DECL_NSIASYNCINPUTSTREAMLENGTH
  NS_DECL_NSIINPUTSTREAMLENGTHCALLBACK

  nsBufferedInputStream();

  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  nsIInputStream* Source() { return (nsIInputStream*)mStream.get(); }

 protected:
  virtual ~nsBufferedInputStream() = default;

  template <typename M>
  void SerializeInternal(mozilla::ipc::InputStreamParams& aParams,
                         FileDescriptorArray& aFileDescriptors,
                         bool aDelayedStart, uint32_t aMaxSize,
                         uint32_t* aSizeUsed, M* aManager);

  NS_IMETHOD Fill() override;
  NS_IMETHOD Flush() override { return NS_OK; }  // no-op for input streams

  mozilla::Mutex mMutex;

  // This value is protected by mutex.
  nsCOMPtr<nsIInputStreamCallback> mAsyncWaitCallback;

  // This value is protected by mutex.
  nsCOMPtr<nsIInputStreamLengthCallback> mAsyncInputStreamLengthCallback;

  bool mIsIPCSerializable;
  bool mIsAsyncInputStream;
  bool mIsCloneableInputStream;
  bool mIsInputStreamLength;
  bool mIsAsyncInputStreamLength;
};

////////////////////////////////////////////////////////////////////////////////

class nsBufferedOutputStream : public nsBufferedStream,
                               public nsISafeOutputStream,
                               public nsIBufferedOutputStream,
                               public nsIStreamBufferAccess {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSISAFEOUTPUTSTREAM
  NS_DECL_NSIBUFFEREDOUTPUTSTREAM
  NS_DECL_NSISTREAMBUFFERACCESS

  nsBufferedOutputStream() : nsBufferedStream() {}

  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  nsIOutputStream* Sink() { return (nsIOutputStream*)mStream.get(); }

 protected:
  virtual ~nsBufferedOutputStream() { nsBufferedOutputStream::Close(); }

  NS_IMETHOD Fill() override { return NS_OK; }  // no-op for output streams

  nsCOMPtr<nsISafeOutputStream> mSafeStream;  // QI'd from mStream
};

////////////////////////////////////////////////////////////////////////////////

#endif  // nsBufferedStreams_h__
