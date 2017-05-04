/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SlicedInputStream.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsISeekableStream.h"
#include "nsStreamUtils.h"

using namespace mozilla::ipc;

NS_IMPL_ADDREF(SlicedInputStream);
NS_IMPL_RELEASE(SlicedInputStream);

NS_INTERFACE_MAP_BEGIN(SlicedInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICloneableInputStream,
                                     mWeakCloneableInputStream || !mInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     mWeakIPCSerializableInputStream || !mInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISeekableStream,
                                     mWeakSeekableInputStream || !mInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStream,
                                     mWeakAsyncInputStream || !mInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamCallback,
                                     mWeakAsyncInputStream || !mInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

SlicedInputStream::SlicedInputStream(nsIInputStream* aInputStream,
                                     uint64_t aStart, uint64_t aLength)
  : mWeakCloneableInputStream(nullptr)
  , mWeakIPCSerializableInputStream(nullptr)
  , mWeakSeekableInputStream(nullptr)
  , mWeakAsyncInputStream(nullptr)
  , mStart(aStart)
  , mLength(aLength)
  , mCurPos(0)
  , mClosed(false)
  , mAsyncWaitFlags(0)
  , mAsyncWaitRequestedCount(0)
{
  MOZ_ASSERT(aInputStream);
  SetSourceStream(aInputStream);
}

SlicedInputStream::SlicedInputStream()
  : mWeakCloneableInputStream(nullptr)
  , mWeakIPCSerializableInputStream(nullptr)
  , mWeakSeekableInputStream(nullptr)
  , mWeakAsyncInputStream(nullptr)
  , mStart(0)
  , mLength(0)
  , mCurPos(0)
  , mClosed(false)
  , mAsyncWaitFlags(0)
  , mAsyncWaitRequestedCount(0)
{}

SlicedInputStream::~SlicedInputStream()
{}

void
SlicedInputStream::SetSourceStream(nsIInputStream* aInputStream)
{
  MOZ_ASSERT(!mInputStream);
  MOZ_ASSERT(aInputStream);

  mInputStream = aInputStream;

  nsCOMPtr<nsICloneableInputStream> cloneableStream =
    do_QueryInterface(aInputStream);
  if (cloneableStream && SameCOMIdentity(aInputStream, cloneableStream)) {
    mWeakCloneableInputStream = cloneableStream;
  }

  nsCOMPtr<nsIIPCSerializableInputStream> serializableStream =
    do_QueryInterface(aInputStream);
  if (serializableStream &&
      SameCOMIdentity(aInputStream, serializableStream)) {
    mWeakIPCSerializableInputStream = serializableStream;
  }

  nsCOMPtr<nsISeekableStream> seekableStream =
    do_QueryInterface(aInputStream);
  if (seekableStream && SameCOMIdentity(aInputStream, seekableStream)) {
    mWeakSeekableInputStream = seekableStream;
  }

  nsCOMPtr<nsIAsyncInputStream> asyncInputStream =
    do_QueryInterface(aInputStream);
  if (asyncInputStream && SameCOMIdentity(aInputStream, asyncInputStream)) {
    mWeakAsyncInputStream = asyncInputStream;
  }
}

NS_IMETHODIMP
SlicedInputStream::Close()
{
  NS_ENSURE_STATE(mInputStream);

  mClosed = true;
  return NS_OK;
}

// nsIInputStream interface

NS_IMETHODIMP
SlicedInputStream::Available(uint64_t* aLength)
{
  NS_ENSURE_STATE(mInputStream);

  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  nsresult rv = mInputStream->Available(aLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Let's remove extra length from the end.
  if (*aLength + mCurPos > mStart + mLength) {
    *aLength -= XPCOM_MIN(*aLength, (*aLength + mCurPos) - (mStart + mLength));
  }

  // Let's remove extra length from the begin.
  if (mCurPos < mStart) {
    *aLength -= XPCOM_MIN(*aLength, mStart - mCurPos);
  }

  return NS_OK;
}

NS_IMETHODIMP
SlicedInputStream::Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount)
{
  *aReadCount = 0;

  if (mClosed) {
    return NS_OK;
  }

  if (mCurPos < mStart) {
    nsCOMPtr<nsISeekableStream> seekableStream =
      do_QueryInterface(mInputStream);
    if (seekableStream) {
      nsresult rv = seekableStream->Seek(nsISeekableStream::NS_SEEK_SET,
                                         mStart);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      mCurPos = mStart;
    } else {
      char buf[4096];
      while (mCurPos < mStart) {
        uint32_t bytesRead;
        uint64_t bufCount = XPCOM_MIN(mStart - mCurPos, (uint64_t)sizeof(buf));
        nsresult rv = mInputStream->Read(buf, bufCount, &bytesRead);
        if (NS_WARN_IF(NS_FAILED(rv)) || bytesRead == 0) {
          return rv;
        }

         mCurPos += bytesRead;
      }
    }
  }

  // Let's reduce aCount in case it's too big.
  if (mCurPos + aCount > mStart + mLength) {
    aCount = mStart + mLength - mCurPos;
  }

  nsresult rv = mInputStream->Read(aBuffer, aCount, aReadCount);
  if (NS_WARN_IF(NS_FAILED(rv)) || *aReadCount == 0) {
    return rv;
  }

  mCurPos += *aReadCount;
  return NS_OK;
}

NS_IMETHODIMP
SlicedInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                uint32_t aCount, uint32_t *aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
SlicedInputStream::IsNonBlocking(bool* aNonBlocking)
{
  NS_ENSURE_STATE(mInputStream);
  return mInputStream->IsNonBlocking(aNonBlocking);
}

// nsICloneableInputStream interface

NS_IMETHODIMP
SlicedInputStream::GetCloneable(bool* aCloneable)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakCloneableInputStream);

  *aCloneable = true;
  return NS_OK;
}

NS_IMETHODIMP
SlicedInputStream::Clone(nsIInputStream** aResult)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakCloneableInputStream);

  nsCOMPtr<nsIInputStream> clonedStream;
  nsresult rv = mWeakCloneableInputStream->Clone(getter_AddRefs(clonedStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> sis =
    new SlicedInputStream(clonedStream, mStart, mLength);

  sis.forget(aResult);
  return NS_OK;
}

// nsIAsyncInputStream interface

NS_IMETHODIMP
SlicedInputStream::CloseWithStatus(nsresult aStatus)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakAsyncInputStream);

  return mWeakAsyncInputStream->CloseWithStatus(aStatus);
}

NS_IMETHODIMP
SlicedInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                             uint32_t aFlags,
                             uint32_t aRequestedCount,
                             nsIEventTarget* aEventTarget)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakAsyncInputStream);

  if (mAsyncWaitCallback && aCallback) {
    return NS_ERROR_FAILURE;
  }

  mAsyncWaitCallback = aCallback;

  if (!mAsyncWaitCallback) {
    return NS_OK;
  }

  // If we haven't started retrieving data, let's see if we can seek.
  // If we cannot seek, we will do consecutive reads.
  if (mCurPos < mStart && mWeakSeekableInputStream) {
    nsresult rv =
      mWeakSeekableInputStream->Seek(nsISeekableStream::NS_SEEK_SET, mStart);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mCurPos = mStart;
  }

  mAsyncWaitFlags = aFlags;
  mAsyncWaitRequestedCount = aRequestedCount;
  mAsyncWaitEventTarget = aEventTarget;

  // If we are not at the right position, let's do an asyncWait just internal.
  if (mCurPos < mStart) {
    return mWeakAsyncInputStream->AsyncWait(this, 0, mStart - mCurPos,
                                            aEventTarget);
  }

  return mWeakAsyncInputStream->AsyncWait(this, aFlags, aRequestedCount,
                                          aEventTarget);
}

// nsIInputStreamCallback

NS_IMETHODIMP
SlicedInputStream::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  MOZ_ASSERT(mInputStream);
  MOZ_ASSERT(mWeakAsyncInputStream);
  MOZ_ASSERT(mWeakAsyncInputStream == aStream);

  // We have been canceled in the meanwhile.
  if (!mAsyncWaitCallback) {
    return NS_OK;
  }

  if (mCurPos < mStart) {
    char buf[4096];
    while (mCurPos < mStart) {
      uint32_t bytesRead;
      uint64_t bufCount = XPCOM_MIN(mStart - mCurPos, (uint64_t)sizeof(buf));
      nsresult rv = mInputStream->Read(buf, bufCount, &bytesRead);
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        return mWeakAsyncInputStream->AsyncWait(this, 0, mStart - mCurPos,
                                                mAsyncWaitEventTarget);
      }

      if (NS_WARN_IF(NS_FAILED(rv)) || bytesRead == 0) {
        return RunAsyncWaitCallback();
      }

      mCurPos += bytesRead;
    }

    // Now we are ready to do the 'real' asyncWait.
    return mWeakAsyncInputStream->AsyncWait(this, mAsyncWaitFlags,
                                            mAsyncWaitRequestedCount,
                                            mAsyncWaitEventTarget);
  }

  return RunAsyncWaitCallback();
}

nsresult
SlicedInputStream::RunAsyncWaitCallback()
{
  nsCOMPtr<nsIInputStreamCallback> callback = mAsyncWaitCallback;

  mAsyncWaitCallback = nullptr;
  mAsyncWaitEventTarget = nullptr;

  return callback->OnInputStreamReady(this);
}

// nsIIPCSerializableInputStream

void
SlicedInputStream::Serialize(mozilla::ipc::InputStreamParams& aParams,
                             FileDescriptorArray& aFileDescriptors)
{
  MOZ_ASSERT(mInputStream);
  MOZ_ASSERT(mWeakIPCSerializableInputStream);

  SlicedInputStreamParams params;
  InputStreamHelper::SerializeInputStream(mInputStream, params.stream(),
                                          aFileDescriptors);
  params.start() = mStart;
  params.length() = mLength;
  params.curPos() = mCurPos;
  params.closed() = mClosed;

  aParams = params;
}

bool
SlicedInputStream::Deserialize(const mozilla::ipc::InputStreamParams& aParams,
                               const FileDescriptorArray& aFileDescriptors)
{
  MOZ_ASSERT(!mInputStream);
  MOZ_ASSERT(!mWeakIPCSerializableInputStream);

  if (aParams.type() !=
      InputStreamParams::TSlicedInputStreamParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return false;
  }

  const SlicedInputStreamParams& params =
    aParams.get_SlicedInputStreamParams();

  nsCOMPtr<nsIInputStream> stream =
    InputStreamHelper::DeserializeInputStream(params.stream(),
                                              aFileDescriptors);
  if (!stream) {
    NS_WARNING("Deserialize failed!");
    return false;
  }

  SetSourceStream(stream);

  mStart = params.start();
  mLength = params.length();
  mCurPos = params.curPos();
  mClosed = params.closed();

  return true;
}

mozilla::Maybe<uint64_t>
SlicedInputStream::ExpectedSerializedLength()
{
  if (!mInputStream || !mWeakIPCSerializableInputStream) {
    return mozilla::Nothing();
  }

  return mWeakIPCSerializableInputStream->ExpectedSerializedLength();
}

// nsISeekableStream

NS_IMETHODIMP
SlicedInputStream::Seek(int32_t aWhence, int64_t aOffset)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakSeekableInputStream);

  int64_t offset;
  nsresult rv;

  switch (aWhence) {
    case NS_SEEK_SET:
      offset = mStart + aOffset;
      break;
    case NS_SEEK_CUR:
      // mCurPos could be lower than mStart if the reading has not started yet.
      offset = XPCOM_MAX(mStart, mCurPos) + aOffset;
      break;
    case NS_SEEK_END: {
      uint64_t available;
      rv = mInputStream->Available(&available);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      offset = XPCOM_MIN(mStart + mLength, available) + aOffset;
      break;
    }
    default:
      return NS_ERROR_ILLEGAL_VALUE;
  }

  if (offset < (int64_t)mStart || offset > (int64_t)(mStart + mLength)) {
    return NS_ERROR_INVALID_ARG;
  }

  rv = mWeakSeekableInputStream->Seek(NS_SEEK_SET, offset);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mCurPos = offset;
  return NS_OK;
}

NS_IMETHODIMP
SlicedInputStream::Tell(int64_t *aResult)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakSeekableInputStream);

  int64_t tell = 0;

  nsresult rv = mWeakSeekableInputStream->Tell(&tell);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (tell < (int64_t)mStart) {
    *aResult = 0;
    return NS_OK;
  }

  *aResult = tell - mStart;
  if (*aResult > (int64_t)mLength) {
    *aResult = mLength;
  }

  return NS_OK;
}

NS_IMETHODIMP
SlicedInputStream::SetEOF()
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakSeekableInputStream);

  mClosed = true;
  return mWeakSeekableInputStream->SetEOF();
}
