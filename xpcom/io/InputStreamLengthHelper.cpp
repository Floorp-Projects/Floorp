/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputStreamLengthHelper.h"
#include "mozilla/dom/WorkerCommon.h"
#include "nsIInputStream.h"
#include "nsIStreamTransportService.h"

static NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);

namespace mozilla {

namespace {

class AvailableEvent final : public Runnable
{
public:
  AvailableEvent(nsIInputStream* stream,
                 const std::function<void(int64_t aLength)>& aCallback)
    : Runnable("mozilla::AvailableEvent")
    , mStream(stream)
    , mCallback(aCallback)
    , mSize(-1)
  {
    mCallbackTarget = GetCurrentThreadSerialEventTarget();
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD
  Run() override
  {
    // ping
    if (!NS_IsMainThread()) {
      uint64_t size = 0;
      if (NS_WARN_IF(NS_FAILED(mStream->Available(&size)))) {
        mSize = -1;
      } else {
        mSize = (int64_t)size;
      }

      mStream = nullptr;

      nsCOMPtr<nsIRunnable> self(this); // overly cute
      mCallbackTarget->Dispatch(self.forget(), NS_DISPATCH_NORMAL);
      mCallbackTarget = nullptr;
      return NS_OK;
    }

    // pong
    mCallback(mSize);
    return NS_OK;
  }

private:
  nsCOMPtr<nsIInputStream> mStream;
  std::function<void(int64_t aLength)> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;

  int64_t mSize;
};

} // anonymous

/* static */ bool
InputStreamLengthHelper::GetSyncLength(nsIInputStream* aStream,
                                       int64_t* aLength)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aLength);

  *aLength = -1;

  // Sync length access.
  nsCOMPtr<nsIInputStreamLength> streamLength = do_QueryInterface(aStream);
  if (streamLength) {
    int64_t length = -1;
    nsresult rv = streamLength->Length(&length);

    // All good!
    if (NS_SUCCEEDED(rv)) {
      *aLength = length;
      return true;
    }

    // Already closed stream or an error occurred.
    if (rv == NS_BASE_STREAM_CLOSED ||
        NS_WARN_IF(rv == NS_ERROR_NOT_AVAILABLE) ||
        NS_WARN_IF(rv != NS_BASE_STREAM_WOULD_BLOCK)) {
      return true;
    }
  }

  nsCOMPtr<nsIAsyncInputStreamLength> asyncStreamLength =
    do_QueryInterface(aStream);
  if (asyncStreamLength) {
    // GetAsyncLength should be used.
    return false;
  }

  // For main-thread only, we want to avoid calling ::Available() for blocking
  // streams.
  if (NS_IsMainThread()) {
    bool nonBlocking = false;
    if (NS_WARN_IF(NS_FAILED(aStream->IsNonBlocking(&nonBlocking)))) {
      // Let's return -1. There is nothing else we can do here.
      return true;
    }

    if (!nonBlocking) {
      return false;
    }
  }

  // Fallback using available().
  uint64_t available = 0;
  nsresult rv = aStream->Available(&available);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Let's return -1. There is nothing else we can do here.
    return true;
  }

  *aLength = (int64_t)available;
  return true;
}

/* static */ void
InputStreamLengthHelper::GetAsyncLength(nsIInputStream* aStream,
                                        const std::function<void(int64_t aLength)>& aCallback)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aCallback);

  // We don't want to allow this class to be used on workers because we are not
  // using the correct Runnable types.
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread() || !dom::IsCurrentThreadRunningWorker());

  RefPtr<InputStreamLengthHelper> helper =
    new InputStreamLengthHelper(aStream, aCallback);

  // Let's be sure that we don't call ::Available() on main-thread.
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIInputStreamLength> streamLength = do_QueryInterface(aStream);
    nsCOMPtr<nsIAsyncInputStreamLength> asyncStreamLength =
      do_QueryInterface(aStream);
    if (!streamLength && !asyncStreamLength) {
      bool nonBlocking = false;
      if (NS_SUCCEEDED(aStream->IsNonBlocking(&nonBlocking)) && !nonBlocking) {
        nsCOMPtr<nsIEventTarget> target =
          do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
        MOZ_ASSERT(target);

        RefPtr<AvailableEvent> event = new AvailableEvent(aStream, aCallback);
        target->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
        return;
      }
    }
  }

  // Let's go async in order to have similar behaviors for sync and async
  // nsIInputStreamLength implementations.
  GetCurrentThreadSerialEventTarget()->Dispatch(helper, NS_DISPATCH_NORMAL);
}

InputStreamLengthHelper::InputStreamLengthHelper(nsIInputStream* aStream,
                                                 const std::function<void(int64_t aLength)>& aCallback)
  : Runnable("InputStreamLengthHelper")
  , mStream(aStream)
  , mCallback(aCallback)
{
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(aCallback);
}

InputStreamLengthHelper::~InputStreamLengthHelper() = default;

NS_IMETHODIMP
InputStreamLengthHelper::Run()
{
  // Sync length access.
  nsCOMPtr<nsIInputStreamLength> streamLength = do_QueryInterface(mStream);
  if (streamLength) {
    int64_t length = -1;
    nsresult rv = streamLength->Length(&length);

    // All good!
    if (NS_SUCCEEDED(rv)) {
      mCallback(length);
      return NS_OK;
    }

    // Already closed stream or an error occurred.
    if (rv == NS_BASE_STREAM_CLOSED ||
        NS_WARN_IF(rv == NS_ERROR_NOT_AVAILABLE) ||
        NS_WARN_IF(rv != NS_BASE_STREAM_WOULD_BLOCK)) {
      mCallback(-1);
      return NS_OK;
    }
  }

  // Async length access.
  nsCOMPtr<nsIAsyncInputStreamLength> asyncStreamLength =
    do_QueryInterface(mStream);
  if (asyncStreamLength) {
    nsresult rv =
     asyncStreamLength->AsyncLengthWait(this,
                                        GetCurrentThreadSerialEventTarget());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mCallback(-1);
    }

    return NS_OK;
  }

  // Fallback using available().
  uint64_t available = 0;
  nsresult rv = mStream->Available(&available);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mCallback(-1);
    return NS_OK;
  }

  mCallback((int64_t)available);
  return NS_OK;
}

NS_IMETHODIMP
InputStreamLengthHelper::OnInputStreamLengthReady(nsIAsyncInputStreamLength* aStream,
                                                  int64_t aLength)
{
  MOZ_ASSERT(mCallback);
  mCallback(aLength);
  mCallback = nullptr;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(InputStreamLengthHelper, Runnable,
                            nsIInputStreamLengthCallback)

} // mozilla namespace
