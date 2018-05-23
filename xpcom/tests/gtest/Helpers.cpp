/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Helper routines for xpcom gtests. */

#include "Helpers.h"

#include <algorithm>
#include "gtest/gtest.h"
#include "nsIOutputStream.h"
#include "nsStreamUtils.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace testing {

// Populate an array with the given number of bytes.  Data is lorem ipsum
// random text, but deterministic across multiple calls.
void
CreateData(uint32_t aNumBytes, nsTArray<char>& aDataOut)
{
  static const char data[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec egestas "
    "purus eu condimentum iaculis. In accumsan leo eget odio porttitor, non "
    "rhoncus nulla vestibulum. Etiam lacinia consectetur nisl nec "
    "sollicitudin. Sed fringilla accumsan diam, pulvinar varius massa. Duis "
    "mollis dignissim felis, eget tempus nisi tristique ut. Fusce euismod, "
    "lectus non lacinia tempor, tellus diam suscipit quam, eget hendrerit "
    "lacus nunc fringilla ante. Sed ultrices massa vitae risus molestie, ut "
    "finibus quam laoreet nullam.";
  static const uint32_t dataLength = sizeof(data) - 1;

  aDataOut.SetCapacity(aNumBytes);

  while (aNumBytes > 0) {
    uint32_t amount = std::min(dataLength, aNumBytes);
    aDataOut.AppendElements(data, amount);
    aNumBytes -= amount;
  }
}

// Write the given number of bytes out to the stream.  Loop until expected
// bytes count is reached or an error occurs.
void
Write(nsIOutputStream* aStream, const nsTArray<char>& aData, uint32_t aOffset,
      uint32_t aNumBytes)
{
  uint32_t remaining =
    std::min(aNumBytes, static_cast<uint32_t>(aData.Length() - aOffset));

  while (remaining > 0) {
    uint32_t numWritten;
    nsresult rv = aStream->Write(aData.Elements() + aOffset, remaining,
                                 &numWritten);
    ASSERT_TRUE(NS_SUCCEEDED(rv));
    if (numWritten < 1) {
      break;
    }
    aOffset += numWritten;
    remaining -= numWritten;
  }
}

// Write the given number of bytes and then close the stream.
void
WriteAllAndClose(nsIOutputStream* aStream, const nsTArray<char>& aData)
{
  Write(aStream, aData, 0, aData.Length());
  aStream->Close();
}

// Synchronously consume the given input stream and validate the resulting data
// against the given array of expected values.
void
ConsumeAndValidateStream(nsIInputStream* aStream,
                         const nsTArray<char>& aExpectedData)
{
  nsDependentCSubstring data(aExpectedData.Elements(), aExpectedData.Length());
  ConsumeAndValidateStream(aStream, data);
}

// Synchronously consume the given input stream and validate the resulting data
// against the given string of expected values.
void
ConsumeAndValidateStream(nsIInputStream* aStream,
                         const nsACString& aExpectedData)
{
  nsAutoCString outputData;
  nsresult rv = NS_ConsumeStream(aStream, UINT32_MAX, outputData);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ(aExpectedData.Length(), outputData.Length());
  ASSERT_TRUE(aExpectedData.Equals(outputData));
}

NS_IMPL_ISUPPORTS(OutputStreamCallback, nsIOutputStreamCallback);

OutputStreamCallback::OutputStreamCallback()
  : mCalled(false)
{
}

OutputStreamCallback::~OutputStreamCallback()
{
}

NS_IMETHODIMP
OutputStreamCallback::OnOutputStreamReady(nsIAsyncOutputStream* aStream)
{
  mCalled = true;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(InputStreamCallback, nsIInputStreamCallback);

InputStreamCallback::InputStreamCallback()
  : mCalled(false)
{
}

InputStreamCallback::~InputStreamCallback()
{
}

NS_IMETHODIMP
InputStreamCallback::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  mCalled = true;
  return NS_OK;
}

AsyncStringStream::AsyncStringStream(const nsACString& aBuffer)
{
  NS_NewCStringInputStream(getter_AddRefs(mStream), aBuffer);
}

NS_IMETHODIMP
AsyncStringStream::Available(uint64_t* aLength)
{
  return mStream->Available(aLength);
}

NS_IMETHODIMP
AsyncStringStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount)
{
  return mStream->Read(aBuffer, aCount, aReadCount);
}

NS_IMETHODIMP
AsyncStringStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                uint32_t aCount, uint32_t *aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
AsyncStringStream::Close()
{
  nsresult rv = mStream->Close();
  if (NS_SUCCEEDED(rv)) {
    MaybeExecCallback(mCallback, mCallbackEventTarget);
  }
  return rv;
}

NS_IMETHODIMP
AsyncStringStream::IsNonBlocking(bool* aNonBlocking)
{
  return mStream->IsNonBlocking(aNonBlocking);
}

NS_IMETHODIMP
AsyncStringStream::CloseWithStatus(nsresult aStatus)
{
  return Close();
}

NS_IMETHODIMP
AsyncStringStream::AsyncWait(nsIInputStreamCallback* aCallback,
                             uint32_t aFlags, uint32_t aRequestedCount,
                             nsIEventTarget* aEventTarget)
{
  if (aFlags & nsIAsyncInputStream::WAIT_CLOSURE_ONLY) {
    mCallback = aCallback;
    mCallbackEventTarget = aEventTarget;
    return NS_OK;
  }

  MaybeExecCallback(aCallback, aEventTarget);
  return NS_OK;
}

void
AsyncStringStream::MaybeExecCallback(nsIInputStreamCallback* aCallback,
                                     nsIEventTarget* aEventTarget)
{
  if (!aCallback) {
    return;
  }

  nsCOMPtr<nsIInputStreamCallback> callback = aCallback;
  nsCOMPtr<nsIAsyncInputStream> self = this;

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "AsyncWait", [callback, self]() { callback->OnInputStreamReady(self); });

  if (aEventTarget) {
    aEventTarget->Dispatch(r.forget());
  } else {
    r->Run();
  }
}

NS_IMPL_ISUPPORTS(AsyncStringStream, nsIAsyncInputStream, nsIInputStream)

NS_IMPL_ADDREF(LengthInputStream);
NS_IMPL_RELEASE(LengthInputStream);

NS_INTERFACE_MAP_BEGIN(LengthInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamLength, mIsInputStreamLength)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStreamLength, mIsAsyncInputStreamLength)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

NS_IMPL_ISUPPORTS(LengthCallback, nsIInputStreamLengthCallback)

} // namespace testing
