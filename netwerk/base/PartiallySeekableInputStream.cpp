/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PartiallySeekableInputStream.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsISeekableStream.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(PartiallySeekableInputStream);
NS_IMPL_RELEASE(PartiallySeekableInputStream);

NS_INTERFACE_MAP_BEGIN(PartiallySeekableInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY(nsITellableStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICloneableInputStream,
                                     mWeakCloneableInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     mWeakIPCSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStream, mWeakAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamCallback,
                                     mWeakAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamLength,
                                     mWeakInputStreamLength)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStreamLength,
                                     mWeakAsyncInputStreamLength)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamLengthCallback,
                                     mWeakAsyncInputStreamLength)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

PartiallySeekableInputStream::PartiallySeekableInputStream(
    already_AddRefed<nsIInputStream> aInputStream, uint64_t aBufferSize)
    : mInputStream(std::move(aInputStream)),
      mWeakCloneableInputStream(nullptr),
      mWeakIPCSerializableInputStream(nullptr),
      mWeakAsyncInputStream(nullptr),
      mWeakInputStreamLength(nullptr),
      mWeakAsyncInputStreamLength(nullptr),
      mBufferSize(aBufferSize),
      mPos(0),
      mClosed(false),
      mMutex("PartiallySeekableInputStream::mMutex") {
  Init();
}

PartiallySeekableInputStream::PartiallySeekableInputStream(
    already_AddRefed<nsIInputStream> aClonedBaseStream,
    PartiallySeekableInputStream* aClonedFrom)
    : mInputStream(std::move(aClonedBaseStream)),
      mWeakCloneableInputStream(nullptr),
      mWeakIPCSerializableInputStream(nullptr),
      mWeakAsyncInputStream(nullptr),
      mWeakInputStreamLength(nullptr),
      mWeakAsyncInputStreamLength(nullptr),
      mCachedBuffer(aClonedFrom->mCachedBuffer),
      mBufferSize(aClonedFrom->mBufferSize),
      mPos(aClonedFrom->mPos),
      mClosed(aClonedFrom->mClosed),
      mMutex("PartiallySeekableInputStream::mMutex") {
  Init();
}

void PartiallySeekableInputStream::Init() {
  MOZ_ASSERT(mInputStream);

#ifdef DEBUG
  nsCOMPtr<nsISeekableStream> seekableStream = do_QueryInterface(mInputStream);
  MOZ_ASSERT(!seekableStream);
#endif

  nsCOMPtr<nsICloneableInputStream> cloneableStream =
      do_QueryInterface(mInputStream);
  if (cloneableStream && SameCOMIdentity(mInputStream, cloneableStream)) {
    mWeakCloneableInputStream = cloneableStream;
  }

  nsCOMPtr<nsIIPCSerializableInputStream> serializableStream =
      do_QueryInterface(mInputStream);
  if (serializableStream && SameCOMIdentity(mInputStream, serializableStream)) {
    mWeakIPCSerializableInputStream = serializableStream;
  }

  nsCOMPtr<nsIAsyncInputStream> asyncInputStream =
      do_QueryInterface(mInputStream);
  if (asyncInputStream && SameCOMIdentity(mInputStream, asyncInputStream)) {
    mWeakAsyncInputStream = asyncInputStream;
  }

  nsCOMPtr<nsIInputStreamLength> inputStreamLength =
      do_QueryInterface(mInputStream);
  if (inputStreamLength && SameCOMIdentity(mInputStream, inputStreamLength)) {
    mWeakInputStreamLength = inputStreamLength;
  }

  nsCOMPtr<nsIAsyncInputStreamLength> asyncInputStreamLength =
      do_QueryInterface(mInputStream);
  if (asyncInputStreamLength &&
      SameCOMIdentity(mInputStream, asyncInputStreamLength)) {
    mWeakAsyncInputStreamLength = asyncInputStreamLength;
  }
}

NS_IMETHODIMP
PartiallySeekableInputStream::Close() {
  mInputStream->Close();
  mCachedBuffer.Clear();
  mPos = 0;
  mClosed = true;
  return NS_OK;
}

// nsIInputStream interface

NS_IMETHODIMP
PartiallySeekableInputStream::Available(uint64_t* aLength) {
  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  nsresult rv = mInputStream->Available(aLength);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mPos < mCachedBuffer.Length()) {
    *aLength += mCachedBuffer.Length() - mPos;
  }

  return NS_OK;
}

NS_IMETHODIMP
PartiallySeekableInputStream::Read(char* aBuffer, uint32_t aCount,
                                   uint32_t* aReadCount) {
  *aReadCount = 0;

  if (mClosed) {
    return NS_OK;
  }

  uint32_t byteRead = 0;

  if (mPos < mCachedBuffer.Length()) {
    // We are reading from the cached buffer.
    byteRead = XPCOM_MIN(mCachedBuffer.Length() - mPos, (uint64_t)aCount);
    memcpy(aBuffer, mCachedBuffer.Elements() + mPos, byteRead);
    *aReadCount = byteRead;
    mPos += byteRead;
  }

  if (byteRead < aCount) {
    MOZ_ASSERT(mPos >= mCachedBuffer.Length());
    MOZ_ASSERT_IF(mPos > mCachedBuffer.Length(),
                  mCachedBuffer.Length() == mBufferSize);

    // We can read from the stream.
    uint32_t byteWritten;
    nsresult rv =
        mInputStream->Read(aBuffer + byteRead, aCount - byteRead, &byteWritten);
    if (NS_WARN_IF(NS_FAILED(rv)) || byteWritten == 0) {
      return rv;
    }

    *aReadCount += byteWritten;

    // Maybe we have to cache something.
    if (mPos < mBufferSize) {
      uint32_t size = XPCOM_MIN(mPos + byteWritten, mBufferSize);
      mCachedBuffer.SetLength(size);
      memcpy(mCachedBuffer.Elements() + mPos, aBuffer + byteRead, size - mPos);
    }

    mPos += byteWritten;
  }

  return NS_OK;
}

NS_IMETHODIMP
PartiallySeekableInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                           void* aClosure, uint32_t aCount,
                                           uint32_t* aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PartiallySeekableInputStream::IsNonBlocking(bool* aNonBlocking) {
  return mInputStream->IsNonBlocking(aNonBlocking);
}

// nsICloneableInputStream interface

NS_IMETHODIMP
PartiallySeekableInputStream::GetCloneable(bool* aCloneable) {
  NS_ENSURE_STATE(mWeakCloneableInputStream);

  return mWeakCloneableInputStream->GetCloneable(aCloneable);
}

NS_IMETHODIMP
PartiallySeekableInputStream::Clone(nsIInputStream** aResult) {
  NS_ENSURE_STATE(mWeakCloneableInputStream);

  nsCOMPtr<nsIInputStream> clonedStream;
  nsresult rv = mWeakCloneableInputStream->Clone(getter_AddRefs(clonedStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> stream =
      new PartiallySeekableInputStream(clonedStream.forget(), this);

  stream.forget(aResult);
  return NS_OK;
}

// nsIAsyncInputStream interface

NS_IMETHODIMP
PartiallySeekableInputStream::CloseWithStatus(nsresult aStatus) {
  NS_ENSURE_STATE(mWeakAsyncInputStream);

  return mWeakAsyncInputStream->CloseWithStatus(aStatus);
}

NS_IMETHODIMP
PartiallySeekableInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                                        uint32_t aFlags,
                                        uint32_t aRequestedCount,
                                        nsIEventTarget* aEventTarget) {
  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  NS_ENSURE_STATE(mWeakAsyncInputStream);

  nsCOMPtr<nsIInputStreamCallback> callback = aCallback ? this : nullptr;
  {
    MutexAutoLock lock(mMutex);
    if (mAsyncWaitCallback && aCallback) {
      return NS_ERROR_FAILURE;
    }

    mAsyncWaitCallback = aCallback;
  }

  return mWeakAsyncInputStream->AsyncWait(callback, aFlags, aRequestedCount,
                                          aEventTarget);
}

// nsIInputStreamCallback

NS_IMETHODIMP
PartiallySeekableInputStream::OnInputStreamReady(nsIAsyncInputStream* aStream) {
  MOZ_ASSERT(mWeakAsyncInputStream);
  MOZ_ASSERT(mWeakAsyncInputStream == aStream);

  nsCOMPtr<nsIInputStreamCallback> callback;

  {
    MutexAutoLock lock(mMutex);

    // We have been canceled in the meanwhile.
    if (!mAsyncWaitCallback) {
      return NS_OK;
    }

    callback.swap(mAsyncWaitCallback);
  }

  MOZ_ASSERT(callback);
  return callback->OnInputStreamReady(this);
}

// nsIIPCSerializableInputStream

void PartiallySeekableInputStream::Serialize(
    mozilla::ipc::InputStreamParams& aParams,
    FileDescriptorArray& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed,
    mozilla::ipc::ParentToChildStreamActorManager* aManager) {
  SerializeInternal(aParams, aFileDescriptors, aDelayedStart, aMaxSize,
                    aSizeUsed, aManager);
}

void PartiallySeekableInputStream::Serialize(
    mozilla::ipc::InputStreamParams& aParams,
    FileDescriptorArray& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed,
    mozilla::ipc::ChildToParentStreamActorManager* aManager) {
  SerializeInternal(aParams, aFileDescriptors, aDelayedStart, aMaxSize,
                    aSizeUsed, aManager);
}

template <typename M>
void PartiallySeekableInputStream::SerializeInternal(
    mozilla::ipc::InputStreamParams& aParams,
    FileDescriptorArray& aFileDescriptors, bool aDelayedStart,
    uint32_t aMaxSize, uint32_t* aSizeUsed, M* aManager) {
  MOZ_ASSERT(mWeakIPCSerializableInputStream);
  MOZ_DIAGNOSTIC_ASSERT(mCachedBuffer.IsEmpty());
  mozilla::ipc::InputStreamHelper::SerializeInputStream(
      mInputStream, aParams, aFileDescriptors, aDelayedStart, aMaxSize,
      aSizeUsed, aManager);
}

bool PartiallySeekableInputStream::Deserialize(
    const mozilla::ipc::InputStreamParams& aParams,
    const FileDescriptorArray& aFileDescriptors) {
  MOZ_CRASH("This method should never be called!");
  return false;
}

// nsISeekableStream

NS_IMETHODIMP
PartiallySeekableInputStream::Seek(int32_t aWhence, int64_t aOffset) {
  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  int64_t offset;

  switch (aWhence) {
    case NS_SEEK_SET:
      offset = aOffset;
      break;
    case NS_SEEK_CUR:
      offset = mPos + aOffset;
      break;
    case NS_SEEK_END: {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    default:
      return NS_ERROR_ILLEGAL_VALUE;
  }

  if (offset < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if ((uint64_t)offset >= mCachedBuffer.Length() || mPos > mBufferSize) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  mPos = offset;
  return NS_OK;
}

NS_IMETHODIMP
PartiallySeekableInputStream::Tell(int64_t* aResult) {
  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  *aResult = mPos;
  return NS_OK;
}

NS_IMETHODIMP
PartiallySeekableInputStream::SetEOF() { return Close(); }

// nsIInputStreamLength

NS_IMETHODIMP
PartiallySeekableInputStream::Length(int64_t* aLength) {
  NS_ENSURE_STATE(mWeakInputStreamLength);
  return mWeakInputStreamLength->Length(aLength);
}

// nsIAsyncInputStreamLength

NS_IMETHODIMP
PartiallySeekableInputStream::AsyncLengthWait(
    nsIInputStreamLengthCallback* aCallback, nsIEventTarget* aEventTarget) {
  NS_ENSURE_STATE(mWeakAsyncInputStreamLength);

  nsCOMPtr<nsIInputStreamLengthCallback> callback = aCallback ? this : nullptr;
  {
    MutexAutoLock lock(mMutex);
    mAsyncInputStreamLengthCallback = aCallback;
  }

  return mWeakAsyncInputStreamLength->AsyncLengthWait(callback, aEventTarget);
}

NS_IMETHODIMP
PartiallySeekableInputStream::OnInputStreamLengthReady(
    nsIAsyncInputStreamLength* aStream, int64_t aLength) {
  nsCOMPtr<nsIInputStreamLengthCallback> callback;
  {
    MutexAutoLock lock(mMutex);
    // We have been canceled in the meanwhile.
    if (!mAsyncInputStreamLengthCallback) {
      return NS_OK;
    }

    callback.swap(mAsyncInputStreamLengthCallback);
  }

  return callback->OnInputStreamLengthReady(this, aLength);
}

}  // namespace net
}  // namespace mozilla
