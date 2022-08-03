/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStreamTransportService.h"
#include "ErrorList.h"
#include "nsXPCOMCIDInternal.h"
#include "nsNetSegmentUtils.h"
#include "nsTransportUtils.h"
#include "nsStreamUtils.h"
#include "nsError.h"
#include "nsNetCID.h"

#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIPipe.h"
#include "nsITransport.h"
#include "nsIObserverService.h"
#include "nsThreadPool.h"
#include "mozilla/Services.h"

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// nsInputStreamTransport
//
// Implements nsIInputStream as a wrapper around the real input stream.  This
// allows the transport to support seeking, range-limiting, progress reporting,
// and close-when-done semantics while utilizing NS_AsyncCopy.
//-----------------------------------------------------------------------------

class nsInputStreamTransport : public nsITransport,
                               public nsIAsyncInputStream,
                               public nsIInputStreamCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORT
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMCALLBACK

  nsInputStreamTransport(nsIInputStream* source, bool closeWhenDone)
      : mSource(source), mCloseWhenDone(closeWhenDone) {
    mAsyncSource = do_QueryInterface(mSource);
  }

 private:
  virtual ~nsInputStreamTransport() = default;

  Mutex mMutex MOZ_UNANNOTATED{"nsInputStreamTransport::mMutex"};

  // This value is protected by mutex.
  nsCOMPtr<nsIInputStreamCallback> mAsyncWaitCallback;

  nsCOMPtr<nsIAsyncInputStream> mPipeIn;

  // while the copy is active, these members may only be accessed from the
  // nsIInputStream implementation.
  nsCOMPtr<nsITransportEventSink> mEventSink;
  nsCOMPtr<nsIInputStream> mSource;

  // It can be null.
  nsCOMPtr<nsIAsyncInputStream> mAsyncSource;

  int64_t mOffset{0};
  const bool mCloseWhenDone;

  // this variable serves as a lock to prevent the state of the transport
  // from being modified once the copy is in progress.
  bool mInProgress{false};
};

NS_IMPL_ADDREF(nsInputStreamTransport);
NS_IMPL_RELEASE(nsInputStreamTransport);

NS_INTERFACE_MAP_BEGIN(nsInputStreamTransport)
  NS_INTERFACE_MAP_ENTRY(nsITransport)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAsyncInputStream, !!mAsyncSource)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIInputStreamCallback, !!mAsyncSource)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITransport)
NS_INTERFACE_MAP_END

/** nsITransport **/

NS_IMETHODIMP
nsInputStreamTransport::OpenInputStream(uint32_t flags, uint32_t segsize,
                                        uint32_t segcount,
                                        nsIInputStream** result) {
  NS_ENSURE_TRUE(!mInProgress, NS_ERROR_IN_PROGRESS);

  nsresult rv;
  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  // XXX if the caller requests an unbuffered stream, then perhaps
  //     we'd want to simply return mSource; however, then we would
  //     not be reading mSource on a background thread.  is this ok?

  bool nonblocking = !(flags & OPEN_BLOCKING);

  net_ResolveSegmentParams(segsize, segcount);

  nsCOMPtr<nsIAsyncOutputStream> pipeOut;
  rv = NS_NewPipe2(getter_AddRefs(mPipeIn), getter_AddRefs(pipeOut),
                   nonblocking, true, segsize, segcount);
  if (NS_FAILED(rv)) return rv;

  mInProgress = true;

  // startup async copy process...
  rv = NS_AsyncCopy(this, pipeOut, target, NS_ASYNCCOPY_VIA_WRITESEGMENTS,
                    segsize);
  if (NS_FAILED(rv)) {
    return rv;
  }
  *result = do_AddRef(mPipeIn).take();
  return NS_OK;
}

NS_IMETHODIMP
nsInputStreamTransport::OpenOutputStream(uint32_t flags, uint32_t segsize,
                                         uint32_t segcount,
                                         nsIOutputStream** result) {
  // this transport only supports reading!
  MOZ_ASSERT_UNREACHABLE("nsInputStreamTransport::OpenOutputStream");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsInputStreamTransport::Close(nsresult reason) {
  if (NS_SUCCEEDED(reason)) reason = NS_BASE_STREAM_CLOSED;

  return mPipeIn->CloseWithStatus(reason);
}

NS_IMETHODIMP
nsInputStreamTransport::SetEventSink(nsITransportEventSink* sink,
                                     nsIEventTarget* target) {
  NS_ENSURE_TRUE(!mInProgress, NS_ERROR_IN_PROGRESS);

  if (target) {
    return net_NewTransportEventSinkProxy(getter_AddRefs(mEventSink), sink,
                                          target);
  }

  mEventSink = sink;
  return NS_OK;
}

/** nsIInputStream **/

NS_IMETHODIMP
nsInputStreamTransport::Close() {
  if (mCloseWhenDone) mSource->Close();

  // make additional reads return early...
  mOffset = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsInputStreamTransport::Available(uint64_t* result) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInputStreamTransport::Read(char* buf, uint32_t count, uint32_t* result) {
  nsresult rv = mSource->Read(buf, count, result);

  if (NS_SUCCEEDED(rv)) {
    mOffset += *result;
    if (mEventSink) {
      mEventSink->OnTransportStatus(this, NS_NET_STATUS_READING, mOffset, -1);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsInputStreamTransport::ReadSegments(nsWriteSegmentFun writer, void* closure,
                                     uint32_t count, uint32_t* result) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInputStreamTransport::IsNonBlocking(bool* result) {
  *result = false;
  return NS_OK;
}

// nsIAsyncInputStream interface

NS_IMETHODIMP
nsInputStreamTransport::CloseWithStatus(nsresult aStatus) { return Close(); }

NS_IMETHODIMP
nsInputStreamTransport::AsyncWait(nsIInputStreamCallback* aCallback,
                                  uint32_t aFlags, uint32_t aRequestedCount,
                                  nsIEventTarget* aEventTarget) {
  NS_ENSURE_STATE(!!mAsyncSource);

  nsCOMPtr<nsIInputStreamCallback> callback = aCallback ? this : nullptr;

  {
    MutexAutoLock lock(mMutex);

    if (NS_WARN_IF(mAsyncWaitCallback && aCallback &&
                   mAsyncWaitCallback != aCallback)) {
      return NS_ERROR_FAILURE;
    }

    mAsyncWaitCallback = aCallback;
  }

  return mAsyncSource->AsyncWait(callback, aFlags, aRequestedCount,
                                 aEventTarget);
}

// nsIInputStreamCallback

NS_IMETHODIMP
nsInputStreamTransport::OnInputStreamReady(nsIAsyncInputStream* aStream) {
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

//-----------------------------------------------------------------------------
// nsStreamTransportService
//-----------------------------------------------------------------------------

nsStreamTransportService::nsStreamTransportService() = default;

nsStreamTransportService::~nsStreamTransportService() {
  NS_ASSERTION(!mPool, "thread pool wasn't shutdown");
}

nsresult nsStreamTransportService::Init() {
  // Can't be used multithreaded before this
  MOZ_PUSH_IGNORE_THREAD_SAFETY
  MOZ_ASSERT(!mPool);
  mPool = new nsThreadPool();

  // Configure the pool
  mPool->SetName("StreamTrans"_ns);
  mPool->SetThreadLimit(25);
  mPool->SetIdleThreadLimit(5);
  mPool->SetIdleThreadTimeoutRegressive(true);
  mPool->SetIdleThreadTimeout(PR_SecondsToInterval(30));
  MOZ_POP_THREAD_SAFETY

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  if (obsSvc) obsSvc->AddObserver(this, "xpcom-shutdown-threads", false);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsStreamTransportService, nsIStreamTransportService,
                  nsIEventTarget, nsIObserver)

NS_IMETHODIMP
nsStreamTransportService::DispatchFromScript(nsIRunnable* task,
                                             uint32_t flags) {
  nsCOMPtr<nsIRunnable> event(task);
  return Dispatch(event.forget(), flags);
}

NS_IMETHODIMP
nsStreamTransportService::Dispatch(already_AddRefed<nsIRunnable> task,
                                   uint32_t flags) {
  nsCOMPtr<nsIRunnable> event(task);  // so it gets released on failure paths
  nsCOMPtr<nsIThreadPool> pool;
  {
    mozilla::MutexAutoLock lock(mShutdownLock);
    if (mIsShutdown) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    pool = mPool;
  }
  NS_ENSURE_TRUE(pool, NS_ERROR_NOT_INITIALIZED);
  return pool->Dispatch(event.forget(), flags);
}

NS_IMETHODIMP
nsStreamTransportService::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                                          uint32_t aDelayMs) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStreamTransportService::RegisterShutdownTask(nsITargetShutdownTask*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStreamTransportService::UnregisterShutdownTask(nsITargetShutdownTask*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(bool)
nsStreamTransportService::IsOnCurrentThreadInfallible() {
  nsCOMPtr<nsIThreadPool> pool;
  {
    mozilla::MutexAutoLock lock(mShutdownLock);
    pool = mPool;
  }
  if (!pool) {
    return false;
  }
  return pool->IsOnCurrentThread();
}

NS_IMETHODIMP
nsStreamTransportService::IsOnCurrentThread(bool* result) {
  nsCOMPtr<nsIThreadPool> pool;
  {
    mozilla::MutexAutoLock lock(mShutdownLock);
    if (mIsShutdown) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    pool = mPool;
  }
  NS_ENSURE_TRUE(pool, NS_ERROR_NOT_INITIALIZED);
  return pool->IsOnCurrentThread(result);
}

NS_IMETHODIMP
nsStreamTransportService::CreateInputTransport(nsIInputStream* stream,
                                               bool closeWhenDone,
                                               nsITransport** result) {
  RefPtr<nsInputStreamTransport> trans =
      new nsInputStreamTransport(stream, closeWhenDone);
  trans.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsStreamTransportService::Observe(nsISupports* subject, const char* topic,
                                  const char16_t* data) {
  NS_ASSERTION(strcmp(topic, "xpcom-shutdown-threads") == 0, "oops");

  {
    nsCOMPtr<nsIThreadPool> pool;
    {
      mozilla::MutexAutoLock lock(mShutdownLock);
      mIsShutdown = true;
      pool = mPool.forget();
    }

    if (pool) {
      pool->Shutdown();
    }
  }
  return NS_OK;
}

class AvailableEvent final : public Runnable {
 public:
  AvailableEvent(nsIInputStream* stream, nsIInputAvailableCallback* callback)
      : Runnable("net::AvailableEvent"),
        mStream(stream),
        mCallback(callback),
        mDoingCallback(false),
        mSize(0),
        mResultForCallback(NS_OK) {
    mCallbackTarget = GetCurrentEventTarget();
  }

  NS_IMETHOD Run() override {
    if (mDoingCallback) {
      // pong
      mCallback->OnInputAvailableComplete(mSize, mResultForCallback);
      mCallback = nullptr;
    } else {
      // ping
      mResultForCallback = mStream->Available(&mSize);
      mStream = nullptr;
      mDoingCallback = true;

      nsCOMPtr<nsIRunnable> event(this);  // overly cute
      mCallbackTarget->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
      mCallbackTarget = nullptr;
    }
    return NS_OK;
  }

 private:
  virtual ~AvailableEvent() = default;

  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsIInputAvailableCallback> mCallback;
  nsCOMPtr<nsIEventTarget> mCallbackTarget;
  bool mDoingCallback;
  uint64_t mSize;
  nsresult mResultForCallback;
};

NS_IMETHODIMP
nsStreamTransportService::InputAvailable(nsIInputStream* stream,
                                         nsIInputAvailableCallback* callback) {
  nsCOMPtr<nsIThreadPool> pool;
  {
    mozilla::MutexAutoLock lock(mShutdownLock);
    if (mIsShutdown) {
      return NS_ERROR_NOT_INITIALIZED;
    }
    pool = mPool;
  }
  nsCOMPtr<nsIRunnable> event = new AvailableEvent(stream, callback);
  return pool->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
}

}  // namespace net
}  // namespace mozilla
