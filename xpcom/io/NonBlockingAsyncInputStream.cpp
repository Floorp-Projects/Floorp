/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonBlockingAsyncInputStream.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsISeekableStream.h"
#include "nsStreamUtils.h"

namespace mozilla {

using namespace ipc;

class NonBlockingAsyncInputStream::AsyncWaitRunnable final : public CancelableRunnable
{
  RefPtr<NonBlockingAsyncInputStream> mStream;
  nsCOMPtr<nsIInputStreamCallback> mCallback;

public:
  AsyncWaitRunnable(NonBlockingAsyncInputStream* aStream,
                    nsIInputStreamCallback* aCallback)
    : CancelableRunnable("AsyncWaitRunnable")
    , mStream(aStream)
    , mCallback(aCallback)
  {}

  NS_IMETHOD
  Run() override
  {
    mStream->RunAsyncWaitCallback(this, mCallback.forget());
    return NS_OK;
  }
};

NS_IMPL_ADDREF(NonBlockingAsyncInputStream);
NS_IMPL_RELEASE(NonBlockingAsyncInputStream);

NonBlockingAsyncInputStream::WaitClosureOnly::WaitClosureOnly(AsyncWaitRunnable* aRunnable,
                                                              nsIEventTarget* aEventTarget)
  : mRunnable(aRunnable)
  , mEventTarget(aEventTarget)
{}

NS_INTERFACE_MAP_BEGIN(NonBlockingAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICloneableInputStream,
                                     mWeakCloneableInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIIPCSerializableInputStream,
                                     mWeakIPCSerializableInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsISeekableStream,
                                     mWeakSeekableInputStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

/* static */ nsresult
NonBlockingAsyncInputStream::Create(already_AddRefed<nsIInputStream> aInputStream,
                                    nsIAsyncInputStream** aResult)
{
  MOZ_DIAGNOSTIC_ASSERT(aResult);

  nsCOMPtr<nsIInputStream> inputStream = std::move(aInputStream);

  bool nonBlocking = false;
  nsresult rv = inputStream->IsNonBlocking(&nonBlocking);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_DIAGNOSTIC_ASSERT(nonBlocking);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  nsCOMPtr<nsIAsyncInputStream> asyncInputStream =
    do_QueryInterface(inputStream);
  MOZ_DIAGNOSTIC_ASSERT(!asyncInputStream);
#endif // MOZ_DIAGNOSTIC_ASSERT_ENABLED

  RefPtr<NonBlockingAsyncInputStream> stream =
    new NonBlockingAsyncInputStream(inputStream.forget());

  stream.forget(aResult);
  return NS_OK;
}

NonBlockingAsyncInputStream::NonBlockingAsyncInputStream(already_AddRefed<nsIInputStream> aInputStream)
  : mInputStream(std::move(aInputStream))
  , mWeakCloneableInputStream(nullptr)
  , mWeakIPCSerializableInputStream(nullptr)
  , mWeakSeekableInputStream(nullptr)
  , mLock("NonBlockingAsyncInputStream::mLock")
  , mClosed(false)
{
  MOZ_ASSERT(mInputStream);

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
}

NonBlockingAsyncInputStream::~NonBlockingAsyncInputStream()
{}

NS_IMETHODIMP
NonBlockingAsyncInputStream::Close()
{
  RefPtr<AsyncWaitRunnable> waitClosureOnlyRunnable;
  nsCOMPtr<nsIEventTarget> waitClosureOnlyEventTarget;

  {
    MutexAutoLock lock(mLock);

    if (mClosed) {
      // Here we could return NS_BASE_STREAM_CLOSED as well, but just to avoid
      // warning messages, let's make everybody happy with a NS_OK.
      return NS_OK;
    }

    mClosed = true;

    NS_ENSURE_STATE(mInputStream);
    nsresult rv = mInputStream->Close();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mWaitClosureOnly.reset();
      return rv;
    }

    // If we have a WaitClosureOnly runnable, it's time to use it.
    if (mWaitClosureOnly.isSome()) {
      waitClosureOnlyRunnable = std::move(mWaitClosureOnly->mRunnable);
      waitClosureOnlyEventTarget = std::move(mWaitClosureOnly->mEventTarget);

      mWaitClosureOnly.reset();

      // Now we want to dispatch the asyncWaitCallback.
      mAsyncWaitCallback = waitClosureOnlyRunnable;
    }
  }

  if (waitClosureOnlyRunnable) {
    if (waitClosureOnlyEventTarget) {
      waitClosureOnlyEventTarget->Dispatch(waitClosureOnlyRunnable,
                                           NS_DISPATCH_NORMAL);
    } else {
      waitClosureOnlyRunnable->Run();
    }
  }

  return NS_OK;
}

// nsIInputStream interface

NS_IMETHODIMP
NonBlockingAsyncInputStream::Available(uint64_t* aLength)
{
  return mInputStream->Available(aLength);
}

NS_IMETHODIMP
NonBlockingAsyncInputStream::Read(char* aBuffer, uint32_t aCount,
                                  uint32_t* aReadCount)
{
  return mInputStream->Read(aBuffer, aCount, aReadCount);
}

namespace {

class MOZ_RAII ReadSegmentsData
{
public:
  ReadSegmentsData(NonBlockingAsyncInputStream* aStream,
                   nsWriteSegmentFun aFunc,
                   void* aClosure)
    : mStream(aStream)
    , mFunc(aFunc)
    , mClosure(aClosure)
  {}

  NonBlockingAsyncInputStream* mStream;
  nsWriteSegmentFun mFunc;
  void* mClosure;
};

nsresult
ReadSegmentsWriter(nsIInputStream* aInStream,
                   void* aClosure,
                   const char* aFromSegment,
                   uint32_t aToOffset,
                   uint32_t aCount,
                   uint32_t* aWriteCount)
{
  ReadSegmentsData* data = static_cast<ReadSegmentsData*>(aClosure);
  return data->mFunc(data->mStream, data->mClosure, aFromSegment, aToOffset,
                     aCount, aWriteCount);
}

} // anonymous

NS_IMETHODIMP
NonBlockingAsyncInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                          void* aClosure, uint32_t aCount,
                                          uint32_t* aResult)
{
  ReadSegmentsData data(this, aWriter, aClosure);
  return mInputStream->ReadSegments(ReadSegmentsWriter, &data, aCount, aResult);
}

NS_IMETHODIMP
NonBlockingAsyncInputStream::IsNonBlocking(bool* aNonBlocking)
{
  *aNonBlocking = true;
  return NS_OK;
}

// nsICloneableInputStream interface

NS_IMETHODIMP
NonBlockingAsyncInputStream::GetCloneable(bool* aCloneable)
{
  NS_ENSURE_STATE(mWeakCloneableInputStream);
  return mWeakCloneableInputStream->GetCloneable(aCloneable);
}

NS_IMETHODIMP
NonBlockingAsyncInputStream::Clone(nsIInputStream** aResult)
{
  NS_ENSURE_STATE(mWeakCloneableInputStream);

  nsCOMPtr<nsIInputStream> clonedStream;
  nsresult rv = mWeakCloneableInputStream->Clone(getter_AddRefs(clonedStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIAsyncInputStream> asyncStream;
  rv = Create(clonedStream.forget(), getter_AddRefs(asyncStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  asyncStream.forget(aResult);
  return NS_OK;
}

// nsIAsyncInputStream interface

NS_IMETHODIMP
NonBlockingAsyncInputStream::CloseWithStatus(nsresult aStatus)
{
  return Close();
}

NS_IMETHODIMP
NonBlockingAsyncInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                                       uint32_t aFlags,
                                       uint32_t aRequestedCount,
                                       nsIEventTarget* aEventTarget)
{
  RefPtr<AsyncWaitRunnable> runnable;
  {
    MutexAutoLock lock(mLock);

    if (aCallback && (mWaitClosureOnly.isSome() || mAsyncWaitCallback)) {
      return NS_ERROR_FAILURE;
    }

    if (!aCallback) {
      // Canceling previous callbacks.
      mWaitClosureOnly.reset();
      mAsyncWaitCallback = nullptr;
      return NS_OK;
    }

    // Maybe the stream is already closed.
    if (!mClosed) {
      uint64_t length;
      nsresult rv = mInputStream->Available(&length);
      if (NS_SUCCEEDED(rv) && length == 0) {
        mInputStream->Close();
        mClosed = true;
      }
    }

    runnable = new AsyncWaitRunnable(this, aCallback);
    if ((aFlags & nsIAsyncInputStream::WAIT_CLOSURE_ONLY) && !mClosed) {
      mWaitClosureOnly.emplace(runnable, aEventTarget);
      return NS_OK;
    }

    mAsyncWaitCallback = runnable;
  }

  MOZ_ASSERT(runnable);

  if (aEventTarget) {
    return aEventTarget->Dispatch(runnable.forget());
  }

  return runnable->Run();
}

// nsIIPCSerializableInputStream

void
NonBlockingAsyncInputStream::Serialize(mozilla::ipc::InputStreamParams& aParams,
                                       FileDescriptorArray& aFileDescriptors)
{
  MOZ_ASSERT(mWeakIPCSerializableInputStream);
  InputStreamHelper::SerializeInputStream(mInputStream, aParams,
                                          aFileDescriptors);
}

bool
NonBlockingAsyncInputStream::Deserialize(const mozilla::ipc::InputStreamParams& aParams,
                                         const FileDescriptorArray& aFileDescriptors)
{
  MOZ_CRASH("NonBlockingAsyncInputStream cannot be deserialized!");
  return true;
}

Maybe<uint64_t>
NonBlockingAsyncInputStream::ExpectedSerializedLength()
{
  NS_ENSURE_TRUE(mWeakIPCSerializableInputStream, Nothing());
  return mWeakIPCSerializableInputStream->ExpectedSerializedLength();
}

// nsISeekableStream

NS_IMETHODIMP
NonBlockingAsyncInputStream::Seek(int32_t aWhence, int64_t aOffset)
{
  NS_ENSURE_STATE(mWeakSeekableInputStream);
  return mWeakSeekableInputStream->Seek(aWhence, aOffset);
}

NS_IMETHODIMP
NonBlockingAsyncInputStream::Tell(int64_t* aResult)
{
  NS_ENSURE_STATE(mWeakSeekableInputStream);
  return mWeakSeekableInputStream->Tell(aResult);
}

NS_IMETHODIMP
NonBlockingAsyncInputStream::SetEOF()
{
  NS_ENSURE_STATE(mWeakSeekableInputStream);
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
NonBlockingAsyncInputStream::RunAsyncWaitCallback(NonBlockingAsyncInputStream::AsyncWaitRunnable* aRunnable,
                                                  already_AddRefed<nsIInputStreamCallback> aCallback)
{
  nsCOMPtr<nsIInputStreamCallback> callback = std::move(aCallback);

  {
    MutexAutoLock lock(mLock);
    if (mAsyncWaitCallback != aRunnable) {
      // The callback has been canceled in the meantime.
      return;
    }

    mAsyncWaitCallback = nullptr;
  }

  callback->OnInputStreamReady(this);
}

} // mozilla namespace
