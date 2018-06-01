/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputStreamLengthWrapper.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsISeekableStream.h"
#include "nsStreamUtils.h"

namespace mozilla {

using namespace ipc;

NS_IMPL_ADDREF(InputStreamLengthWrapper);
NS_IMPL_RELEASE(InputStreamLengthWrapper);

NS_INTERFACE_MAP_BEGIN(InputStreamLengthWrapper)
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
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamLength)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

InputStreamLengthWrapper::InputStreamLengthWrapper(already_AddRefed<nsIInputStream> aInputStream,
                                                   int64_t aLength)
  : mWeakCloneableInputStream(nullptr)
  , mWeakIPCSerializableInputStream(nullptr)
  , mWeakSeekableInputStream(nullptr)
  , mWeakAsyncInputStream(nullptr)
  , mLength(aLength)
  , mConsumed(false)
  , mMutex("InputStreamLengthWrapper::mMutex")
{
  MOZ_ASSERT(mLength >= 0);

  nsCOMPtr<nsIInputStream> inputStream = std::move(aInputStream);
  SetSourceStream(inputStream.forget());
}

InputStreamLengthWrapper::InputStreamLengthWrapper()
  : mWeakCloneableInputStream(nullptr)
  , mWeakIPCSerializableInputStream(nullptr)
  , mWeakSeekableInputStream(nullptr)
  , mWeakAsyncInputStream(nullptr)
  , mLength(-1)
  , mConsumed(false)
  , mMutex("InputStreamLengthWrapper::mMutex")
{}

InputStreamLengthWrapper::~InputStreamLengthWrapper() = default;

void
InputStreamLengthWrapper::SetSourceStream(already_AddRefed<nsIInputStream> aInputStream)
{
  MOZ_ASSERT(!mInputStream);

  mInputStream = std::move(aInputStream);

  nsCOMPtr<nsICloneableInputStream> cloneableStream =
    do_QueryInterface(mInputStream);
  if (cloneableStream && SameCOMIdentity(mInputStream, cloneableStream)) {
    mWeakCloneableInputStream = cloneableStream;
  }

  nsCOMPtr<nsIIPCSerializableInputStream> serializableStream =
    do_QueryInterface(mInputStream);
  if (serializableStream &&
      SameCOMIdentity(mInputStream, serializableStream)) {
    mWeakIPCSerializableInputStream = serializableStream;
  }

  nsCOMPtr<nsISeekableStream> seekableStream =
    do_QueryInterface(mInputStream);
  if (seekableStream && SameCOMIdentity(mInputStream, seekableStream)) {
    mWeakSeekableInputStream = seekableStream;
  }

  nsCOMPtr<nsIAsyncInputStream> asyncInputStream =
    do_QueryInterface(mInputStream);
  if (asyncInputStream && SameCOMIdentity(mInputStream, asyncInputStream)) {
    mWeakAsyncInputStream = asyncInputStream;
  }
}

// nsIInputStream interface

NS_IMETHODIMP
InputStreamLengthWrapper::Close()
{
  NS_ENSURE_STATE(mInputStream);
  return mInputStream->Close();
}

NS_IMETHODIMP
InputStreamLengthWrapper::Available(uint64_t* aLength)
{
  NS_ENSURE_STATE(mInputStream);
  return mInputStream->Available(aLength);
}

NS_IMETHODIMP
InputStreamLengthWrapper::Read(char* aBuffer, uint32_t aCount, uint32_t* aReadCount)
{
  NS_ENSURE_STATE(mInputStream);
  mConsumed = true;
  return mInputStream->Read(aBuffer, aCount, aReadCount);
}

NS_IMETHODIMP
InputStreamLengthWrapper::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                       uint32_t aCount, uint32_t *aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
InputStreamLengthWrapper::IsNonBlocking(bool* aNonBlocking)
{
  NS_ENSURE_STATE(mInputStream);
  return mInputStream->IsNonBlocking(aNonBlocking);
}

// nsICloneableInputStream interface

NS_IMETHODIMP
InputStreamLengthWrapper::GetCloneable(bool* aCloneable)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakCloneableInputStream);
  mWeakCloneableInputStream->GetCloneable(aCloneable);
  return NS_OK;
}

NS_IMETHODIMP
InputStreamLengthWrapper::Clone(nsIInputStream** aResult)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakCloneableInputStream);

  nsCOMPtr<nsIInputStream> clonedStream;
  nsresult rv = mWeakCloneableInputStream->Clone(getter_AddRefs(clonedStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> stream =
    new InputStreamLengthWrapper(clonedStream.forget(), mLength);

  stream.forget(aResult);
  return NS_OK;
}

// nsIAsyncInputStream interface

NS_IMETHODIMP
InputStreamLengthWrapper::CloseWithStatus(nsresult aStatus)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakAsyncInputStream);

  mConsumed = true;
  return mWeakAsyncInputStream->CloseWithStatus(aStatus);
}

NS_IMETHODIMP
InputStreamLengthWrapper::AsyncWait(nsIInputStreamCallback* aCallback,
                                    uint32_t aFlags,
                                    uint32_t aRequestedCount,
                                    nsIEventTarget* aEventTarget)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakAsyncInputStream);

  nsCOMPtr<nsIInputStreamCallback> callback = this;
  {
    MutexAutoLock lock(mMutex);

    if (mAsyncWaitCallback && aCallback) {
      return NS_ERROR_FAILURE;
    }

    bool hadCallback = !!mAsyncWaitCallback;
    mAsyncWaitCallback = aCallback;

    if (!mAsyncWaitCallback) {
      if (!hadCallback) {
        // No pending operation.
        return NS_OK;
      }

      // Abort current operation.
      callback = nullptr;
    }
  }

  return mWeakAsyncInputStream->AsyncWait(callback, aFlags, aRequestedCount,
                                          aEventTarget);
}

// nsIInputStreamCallback

NS_IMETHODIMP
InputStreamLengthWrapper::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  MOZ_ASSERT(mInputStream);
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

void
InputStreamLengthWrapper::Serialize(mozilla::ipc::InputStreamParams& aParams,
                                    FileDescriptorArray& aFileDescriptors)
{
  MOZ_ASSERT(mInputStream);
  MOZ_ASSERT(mWeakIPCSerializableInputStream);

  InputStreamLengthWrapperParams params;
  InputStreamHelper::SerializeInputStream(mInputStream, params.stream(),
                                          aFileDescriptors);
  params.length() = mLength;
  params.consumed() = mConsumed;

  aParams = params;
}

bool
InputStreamLengthWrapper::Deserialize(const mozilla::ipc::InputStreamParams& aParams,
                                      const FileDescriptorArray& aFileDescriptors)
{
  MOZ_ASSERT(!mInputStream);
  MOZ_ASSERT(!mWeakIPCSerializableInputStream);

  if (aParams.type() !=
      InputStreamParams::TInputStreamLengthWrapperParams) {
    NS_ERROR("Received unknown parameters from the other process!");
    return false;
  }

  const InputStreamLengthWrapperParams& params =
    aParams.get_InputStreamLengthWrapperParams();

  nsCOMPtr<nsIInputStream> stream =
    InputStreamHelper::DeserializeInputStream(params.stream(),
                                              aFileDescriptors);
  if (!stream) {
    NS_WARNING("Deserialize failed!");
    return false;
  }

  SetSourceStream(stream.forget());

  mLength = params.length();
  mConsumed = params.consumed();

  return true;
}

mozilla::Maybe<uint64_t>
InputStreamLengthWrapper::ExpectedSerializedLength()
{
  if (!mInputStream || !mWeakIPCSerializableInputStream) {
    return mozilla::Nothing();
  }

  return mWeakIPCSerializableInputStream->ExpectedSerializedLength();
}

// nsISeekableStream

NS_IMETHODIMP
InputStreamLengthWrapper::Seek(int32_t aWhence, int64_t aOffset)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakSeekableInputStream);

  mConsumed = true;
  return mWeakSeekableInputStream->Seek(aWhence, aOffset);
}

NS_IMETHODIMP
InputStreamLengthWrapper::Tell(int64_t *aResult)
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakSeekableInputStream);

  return mWeakSeekableInputStream->Tell(aResult);
}

NS_IMETHODIMP
InputStreamLengthWrapper::SetEOF()
{
  NS_ENSURE_STATE(mInputStream);
  NS_ENSURE_STATE(mWeakSeekableInputStream);

  mConsumed = true;
  return mWeakSeekableInputStream->SetEOF();
}

// nsIInputStreamLength

NS_IMETHODIMP
InputStreamLengthWrapper::Length(int64_t* aLength)
{
  NS_ENSURE_STATE(mInputStream);
  *aLength = mLength;
  return NS_OK;
}

} // namespace mozilla
