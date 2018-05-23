/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __Helpers_h
#define __Helpers_h

#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInputStreamLength.h"
#include "nsString.h"
#include "nsStringStream.h"
#include "nsTArrayForwardDeclare.h"
#include "nsThreadUtils.h"
#include <stdint.h>

class nsIInputStream;
class nsIOutputStream;

namespace testing {

void
CreateData(uint32_t aNumBytes, nsTArray<char>& aDataOut);

void
Write(nsIOutputStream* aStream, const nsTArray<char>& aData, uint32_t aOffset,
      uint32_t aNumBytes);

void
WriteAllAndClose(nsIOutputStream* aStream, const nsTArray<char>& aData);

void
ConsumeAndValidateStream(nsIInputStream* aStream,
                         const nsTArray<char>& aExpectedData);

void
ConsumeAndValidateStream(nsIInputStream* aStream,
                         const nsACString& aExpectedData);

class OutputStreamCallback final : public nsIOutputStreamCallback
{
public:
  OutputStreamCallback();

  bool Called() const { return mCalled; }

private:
  ~OutputStreamCallback();

  bool mCalled;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
};

class InputStreamCallback final : public nsIInputStreamCallback
{
public:
  InputStreamCallback();

  bool Called() const { return mCalled; }

private:
  ~InputStreamCallback();

  bool mCalled;
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK
};

class AsyncStringStream final : public nsIAsyncInputStream
{
  nsCOMPtr<nsIInputStream> mStream;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  explicit AsyncStringStream(const nsACString& aBuffer);

private:
  ~AsyncStringStream() = default;

  void
  MaybeExecCallback(nsIInputStreamCallback* aCallback,
                    nsIEventTarget* aEventTarget);

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackEventTarget;
};

// This class implements a simple nsIInputStreamLength stream.
class LengthInputStream : public nsIInputStream
                        , public nsIInputStreamLength
                        , public nsIAsyncInputStreamLength
{
  nsCOMPtr<nsIInputStream> mStream;
  bool mIsInputStreamLength;
  bool mIsAsyncInputStreamLength;
  nsresult mLengthRv;
  bool mNegativeValue;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  LengthInputStream(const nsACString& aBuffer,
                    bool aIsInputStreamLength,
                    bool aIsAsyncInputStreamLength,
                    nsresult aLengthRv = NS_OK,
                    bool aNegativeValue = false)
    : mIsInputStreamLength(aIsInputStreamLength)
    , mIsAsyncInputStreamLength(aIsAsyncInputStreamLength)
    , mLengthRv(aLengthRv)
    , mNegativeValue(aNegativeValue)
  {
    NS_NewCStringInputStream(getter_AddRefs(mStream), aBuffer);
  }

  NS_IMETHOD
  Available(uint64_t* aLength) override
  {
    return mStream->Available(aLength);
  }

  NS_IMETHOD
  Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount) override
  {
    return mStream->Read(aBuffer, aCount, aReadCount);
  }

  NS_IMETHOD
  ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
               uint32_t aCount, uint32_t *aResult) override
  {
    return mStream->ReadSegments(aWriter, aClosure, aCount, aResult);
  }

  NS_IMETHOD
  Close() override
  {
    return mStream->Close();
  }

  NS_IMETHOD
  IsNonBlocking(bool* aNonBlocking) override
  {
    return mStream->IsNonBlocking(aNonBlocking);
  }

  NS_IMETHOD
  Length(int64_t* aLength) override
  {
    if (mNegativeValue) {
      *aLength = -1;
    } else {
      mStream->Available((uint64_t*)aLength);
    }
    return mLengthRv;
  }

  NS_IMETHOD
  AsyncLengthWait(nsIInputStreamLengthCallback* aCallback,
                  nsIEventTarget* aEventTarget) override
  {
    RefPtr<LengthInputStream> self = this;
    nsCOMPtr<nsIInputStreamLengthCallback> callback = aCallback;

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "AsyncLengthWait", [self, callback]() {
        int64_t length;
        self->Length(&length);
        callback->OnInputStreamLengthReady(self, length);
    });

    return aEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  }

protected:
  virtual ~LengthInputStream() = default;
};

class LengthCallback final : public nsIInputStreamLengthCallback
{
  bool mCalled;
  int64_t mSize;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  LengthCallback()
    : mCalled(false)
    , mSize(0)
  {}

  NS_IMETHOD
  OnInputStreamLengthReady(nsIAsyncInputStreamLength* aStream,
                           int64_t aLength) override
  {
    mCalled = true;
    mSize = aLength;
    return NS_OK;
  }

  bool
  Called() const
  {
    return mCalled;
  }

  int64_t
  Size() const
  {
    return mSize;
  }

private:
  ~LengthCallback() = default;
};

} // namespace testing

#endif // __Helpers_h
