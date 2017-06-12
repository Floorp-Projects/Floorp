/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThrottleQueue.h"
#include "nsISeekableStream.h"
#include "nsIAsyncInputStream.h"
#include "nsStreamUtils.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------

class ThrottleInputStream final
  : public nsIAsyncInputStream
  , public nsISeekableStream
{
public:

  ThrottleInputStream(nsIInputStream* aStream, ThrottleQueue* aQueue);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSIASYNCINPUTSTREAM

  void AllowInput();

private:

  ~ThrottleInputStream();

  nsCOMPtr<nsIInputStream> mStream;
  RefPtr<ThrottleQueue> mQueue;
  nsresult mClosedStatus;

  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mEventTarget;
};

NS_IMPL_ISUPPORTS(ThrottleInputStream, nsIAsyncInputStream, nsIInputStream, nsISeekableStream)

ThrottleInputStream::ThrottleInputStream(nsIInputStream *aStream, ThrottleQueue* aQueue)
  : mStream(aStream)
  , mQueue(aQueue)
  , mClosedStatus(NS_OK)
{
  MOZ_ASSERT(aQueue != nullptr);
}

ThrottleInputStream::~ThrottleInputStream()
{
  Close();
}

NS_IMETHODIMP
ThrottleInputStream::Close()
{
  if (NS_FAILED(mClosedStatus)) {
    return mClosedStatus;
  }

  if (mQueue) {
    mQueue->DequeueStream(this);
    mQueue = nullptr;
    mClosedStatus = NS_BASE_STREAM_CLOSED;
  }
  return mStream->Close();
}

NS_IMETHODIMP
ThrottleInputStream::Available(uint64_t* aResult)
{
  if (NS_FAILED(mClosedStatus)) {
    return mClosedStatus;
  }

  return mStream->Available(aResult);
}

NS_IMETHODIMP
ThrottleInputStream::Read(char* aBuf, uint32_t aCount, uint32_t* aResult)
{
  if (NS_FAILED(mClosedStatus)) {
    return mClosedStatus;
  }

  uint32_t realCount;
  nsresult rv = mQueue->Available(aCount, &realCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (realCount == 0) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  rv = mStream->Read(aBuf, realCount, aResult);
  if (NS_SUCCEEDED(rv) && *aResult > 0) {
    mQueue->RecordRead(*aResult);
  }
  return rv;
}

NS_IMETHODIMP
ThrottleInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                  uint32_t aCount, uint32_t* aResult)
{
  if (NS_FAILED(mClosedStatus)) {
    return mClosedStatus;
  }

  uint32_t realCount;
  nsresult rv = mQueue->Available(aCount, &realCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (realCount == 0) {
    return NS_BASE_STREAM_WOULD_BLOCK;
  }

  rv = mStream->ReadSegments(aWriter, aClosure, realCount, aResult);
  if (NS_SUCCEEDED(rv) && *aResult > 0) {
    mQueue->RecordRead(*aResult);
  }
  return rv;
}

NS_IMETHODIMP
ThrottleInputStream::IsNonBlocking(bool* aNonBlocking)
{
  *aNonBlocking = true;
  return NS_OK;
}

NS_IMETHODIMP
ThrottleInputStream::Seek(int32_t aWhence, int64_t aOffset)
{
  if (NS_FAILED(mClosedStatus)) {
    return mClosedStatus;
  }

  nsCOMPtr<nsISeekableStream> sstream = do_QueryInterface(mStream);
  if (!sstream) {
    return NS_ERROR_FAILURE;
  }

  return sstream->Seek(aWhence, aOffset);
}

NS_IMETHODIMP
ThrottleInputStream::Tell(int64_t* aResult)
{
  if (NS_FAILED(mClosedStatus)) {
    return mClosedStatus;
  }

  nsCOMPtr<nsISeekableStream> sstream = do_QueryInterface(mStream);
  if (!sstream) {
    return NS_ERROR_FAILURE;
  }

  return sstream->Tell(aResult);
}

NS_IMETHODIMP
ThrottleInputStream::SetEOF()
{
  if (NS_FAILED(mClosedStatus)) {
    return mClosedStatus;
  }

  nsCOMPtr<nsISeekableStream> sstream = do_QueryInterface(mStream);
  if (!sstream) {
    return NS_ERROR_FAILURE;
  }

  return sstream->SetEOF();
}

NS_IMETHODIMP
ThrottleInputStream::CloseWithStatus(nsresult aStatus)
{
  if (NS_FAILED(mClosedStatus)) {
    // Already closed, ignore.
    return NS_OK;
  }
  if (NS_SUCCEEDED(aStatus)) {
    aStatus = NS_BASE_STREAM_CLOSED;
  }

  mClosedStatus = Close();
  if (NS_SUCCEEDED(mClosedStatus)) {
    mClosedStatus = aStatus;
  }
  return NS_OK;
}

NS_IMETHODIMP
ThrottleInputStream::AsyncWait(nsIInputStreamCallback *aCallback,
                               uint32_t aFlags,
                               uint32_t aRequestedCount,
                               nsIEventTarget *aEventTarget)
{
  if (aFlags != 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  mCallback = aCallback;
  mEventTarget = aEventTarget;
  if (mCallback) {
    mQueue->QueueStream(this);
  } else {
    mQueue->DequeueStream(this);
  }
  return NS_OK;
}

void
ThrottleInputStream::AllowInput()
{
  MOZ_ASSERT(mCallback);
  nsCOMPtr<nsIInputStreamCallback> callbackEvent =
    NS_NewInputStreamReadyEvent("ThrottleInputStream::AllowInput",
                                mCallback, mEventTarget);
  mCallback = nullptr;
  mEventTarget = nullptr;
  callbackEvent->OnInputStreamReady(this);
}

//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(ThrottleQueue, nsIInputChannelThrottleQueue, nsITimerCallback)

ThrottleQueue::ThrottleQueue()
  : mMeanBytesPerSecond(0)
  , mMaxBytesPerSecond(0)
  , mBytesProcessed(0)
  , mTimerArmed(false)
{
  nsresult rv;
  nsCOMPtr<nsIEventTarget> sts;
  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  if (NS_SUCCEEDED(rv))
    sts = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mTimer)
    mTimer->SetTarget(sts);
}

ThrottleQueue::~ThrottleQueue()
{
  if (mTimer && mTimerArmed) {
    mTimer->Cancel();
  }
  mTimer = nullptr;
}

NS_IMETHODIMP
ThrottleQueue::RecordRead(uint32_t aBytesRead)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  ThrottleEntry entry;
  entry.mTime = TimeStamp::Now();
  entry.mBytesRead = aBytesRead;
  mReadEvents.AppendElement(entry);
  mBytesProcessed += aBytesRead;
  return NS_OK;
}

NS_IMETHODIMP
ThrottleQueue::Available(uint32_t aRemaining, uint32_t* aAvailable)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  TimeStamp now = TimeStamp::Now();
  TimeStamp oneSecondAgo = now - TimeDuration::FromSeconds(1);
  size_t i;

  // Remove all stale events.
  for (i = 0; i < mReadEvents.Length(); ++i) {
    if (mReadEvents[i].mTime >= oneSecondAgo) {
      break;
    }
  }
  mReadEvents.RemoveElementsAt(0, i);

  uint32_t totalBytes = 0;
  for (i = 0; i < mReadEvents.Length(); ++i) {
    totalBytes += mReadEvents[i].mBytesRead;
  }

  uint32_t spread = mMaxBytesPerSecond - mMeanBytesPerSecond;
  double prob = static_cast<double>(rand()) / RAND_MAX;
  uint32_t thisSliceBytes = mMeanBytesPerSecond - spread +
    static_cast<uint32_t>(2 * spread * prob);

  if (totalBytes >= thisSliceBytes) {
    *aAvailable = 0;
  } else {
    *aAvailable = thisSliceBytes;
  }
  return NS_OK;
}

NS_IMETHODIMP
ThrottleQueue::Init(uint32_t aMeanBytesPerSecond, uint32_t aMaxBytesPerSecond)
{
  // Can be called on any thread.
  if (aMeanBytesPerSecond == 0 || aMaxBytesPerSecond == 0 || aMaxBytesPerSecond < aMeanBytesPerSecond) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  mMeanBytesPerSecond = aMeanBytesPerSecond;
  mMaxBytesPerSecond = aMaxBytesPerSecond;
  return NS_OK;
}

NS_IMETHODIMP
ThrottleQueue::BytesProcessed(uint64_t* aResult)
{
  *aResult = mBytesProcessed;
  return NS_OK;
}

NS_IMETHODIMP
ThrottleQueue::WrapStream(nsIInputStream* aInputStream, nsIAsyncInputStream** aResult)
{
  nsCOMPtr<nsIAsyncInputStream> result = new ThrottleInputStream(aInputStream, this);
  result.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ThrottleQueue::Notify(nsITimer* aTimer)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  // A notified reader may need to push itself back on the queue.
  // Swap out the list of readers so that this works properly.
  nsTArray<RefPtr<ThrottleInputStream>> events;
  events.SwapElements(mAsyncEvents);

  // Optimistically notify all the waiting readers, and then let them
  // requeue if there isn't enough bandwidth.
  for (size_t i = 0; i < events.Length(); ++i) {
    events[i]->AllowInput();
  }

  mTimerArmed = false;
  return NS_OK;
}

void
ThrottleQueue::QueueStream(ThrottleInputStream* aStream)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  if (mAsyncEvents.IndexOf(aStream) == mAsyncEvents.NoIndex) {
    mAsyncEvents.AppendElement(aStream);

    if (!mTimerArmed) {
      uint32_t ms = 1000;
      if (mReadEvents.Length() > 0) {
        TimeStamp t = mReadEvents[0].mTime + TimeDuration::FromSeconds(1);
        TimeStamp now = TimeStamp::Now();

        if (t > now) {
          ms = static_cast<uint32_t>((t - now).ToMilliseconds());
        } else {
          ms = 1;
        }
      }

      if (NS_SUCCEEDED(mTimer->InitWithCallback(this, ms, nsITimer::TYPE_ONE_SHOT))) {
        mTimerArmed = true;
      }
    }
  }
}

void
ThrottleQueue::DequeueStream(ThrottleInputStream* aStream)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  mAsyncEvents.RemoveElement(aStream);
}

}
}
