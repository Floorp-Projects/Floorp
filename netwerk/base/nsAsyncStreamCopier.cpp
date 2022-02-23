/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAsyncStreamCopier.h"
#include "nsIOService.h"
#include "nsIEventTarget.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsIBufferedStreams.h"
#include "nsIRequestObserver.h"
#include "mozilla/Logging.h"

using namespace mozilla;
using namespace mozilla::net;

#undef LOG
//
// MOZ_LOG=nsStreamCopier:5
//
static LazyLogModule gStreamCopierLog("nsStreamCopier");
#define LOG(args) MOZ_LOG(gStreamCopierLog, mozilla::LogLevel::Debug, args)

/**
 * An event used to perform initialization off the main thread.
 */
class AsyncApplyBufferingPolicyEvent final : public Runnable {
 public:
  /**
   * @param aCopier
   *        The nsAsyncStreamCopier requesting the information.
   */
  explicit AsyncApplyBufferingPolicyEvent(nsAsyncStreamCopier* aCopier)
      : mozilla::Runnable("AsyncApplyBufferingPolicyEvent"),
        mCopier(aCopier),
        mTarget(GetCurrentEventTarget()) {}

  NS_IMETHOD Run() override {
    nsresult rv = mCopier->ApplyBufferingPolicy();
    if (NS_FAILED(rv)) {
      mCopier->Cancel(rv);
      return NS_OK;
    }

    rv = mTarget->Dispatch(
        NewRunnableMethod("nsAsyncStreamCopier::AsyncCopyInternal", mCopier,
                          &nsAsyncStreamCopier::AsyncCopyInternal),
        NS_DISPATCH_NORMAL);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (NS_FAILED(rv)) {
      mCopier->Cancel(rv);
    }
    return NS_OK;
  }

 private:
  RefPtr<nsAsyncStreamCopier> mCopier;
  nsCOMPtr<nsIEventTarget> mTarget;
};

//-----------------------------------------------------------------------------

nsAsyncStreamCopier::nsAsyncStreamCopier()
    : mChunkSize(nsIOService::gDefaultSegmentSize) {
  LOG(("Creating nsAsyncStreamCopier @%p\n", this));
}

nsAsyncStreamCopier::~nsAsyncStreamCopier() {
  LOG(("Destroying nsAsyncStreamCopier @%p\n", this));
}

bool nsAsyncStreamCopier::IsComplete(nsresult* status) {
  MutexAutoLock lock(mLock);
  if (status) *status = mStatus;
  return !mIsPending;
}

nsIRequest* nsAsyncStreamCopier::AsRequest() {
  return static_cast<nsIRequest*>(static_cast<nsIAsyncStreamCopier*>(this));
}

void nsAsyncStreamCopier::Complete(nsresult status) {
  LOG(("nsAsyncStreamCopier::Complete [this=%p status=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(status)));

  nsCOMPtr<nsIRequestObserver> observer;
  nsCOMPtr<nsISupports> ctx;
  {
    MutexAutoLock lock(mLock);
    mCopierCtx = nullptr;

    if (mIsPending) {
      mIsPending = false;
      mStatus = status;

      // setup OnStopRequest callback and release references...
      observer = mObserver;
      mObserver = nullptr;
    }
  }

  if (observer) {
    LOG(("  calling OnStopRequest [status=%" PRIx32 "]\n",
         static_cast<uint32_t>(status)));
    observer->OnStopRequest(AsRequest(), status);
  }
}

void nsAsyncStreamCopier::OnAsyncCopyComplete(void* closure, nsresult status) {
  // AddRef'd in AsyncCopy. Will be released at the end of the method.
  RefPtr<nsAsyncStreamCopier> self = dont_AddRef((nsAsyncStreamCopier*)closure);
  self->Complete(status);
}

//-----------------------------------------------------------------------------
// nsISupports

// We cannot use simply NS_IMPL_ISUPPORTSx as both
// nsIAsyncStreamCopier and nsIAsyncStreamCopier2 implement nsIRequest

NS_IMPL_ADDREF(nsAsyncStreamCopier)
NS_IMPL_RELEASE(nsAsyncStreamCopier)
NS_INTERFACE_TABLE_HEAD(nsAsyncStreamCopier)
  NS_INTERFACE_TABLE_BEGIN
    NS_INTERFACE_TABLE_ENTRY(nsAsyncStreamCopier, nsIAsyncStreamCopier)
    NS_INTERFACE_TABLE_ENTRY(nsAsyncStreamCopier, nsIAsyncStreamCopier2)
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsAsyncStreamCopier, nsIRequest,
                                       nsIAsyncStreamCopier)
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsAsyncStreamCopier, nsISupports,
                                       nsIAsyncStreamCopier)
  NS_INTERFACE_TABLE_END
NS_INTERFACE_TABLE_TAIL

//-----------------------------------------------------------------------------
// nsIRequest

NS_IMETHODIMP
nsAsyncStreamCopier::GetName(nsACString& name) {
  name.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::IsPending(bool* result) {
  *result = !IsComplete();
  return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::GetStatus(nsresult* status) {
  IsComplete(status);
  return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::Cancel(nsresult status) {
  nsCOMPtr<nsISupports> copierCtx;
  {
    MutexAutoLock lock(mLock);
    if (!mIsPending) return NS_OK;
    copierCtx.swap(mCopierCtx);
  }

  if (NS_SUCCEEDED(status)) {
    NS_WARNING("cancel with non-failure status code");
    status = NS_BASE_STREAM_CLOSED;
  }

  if (copierCtx) NS_CancelAsyncCopy(copierCtx, status);

  return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::Suspend() {
  MOZ_ASSERT_UNREACHABLE("nsAsyncStreamCopier::Suspend");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAsyncStreamCopier::Resume() {
  MOZ_ASSERT_UNREACHABLE("nsAsyncStreamCopier::Resume");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAsyncStreamCopier::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = LOAD_NORMAL;
  return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::SetLoadFlags(nsLoadFlags aLoadFlags) { return NS_OK; }

NS_IMETHODIMP
nsAsyncStreamCopier::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return nsIAsyncStreamCopier::GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
nsAsyncStreamCopier::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return nsIAsyncStreamCopier::SetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
nsAsyncStreamCopier::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  *aLoadGroup = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::SetLoadGroup(nsILoadGroup* aLoadGroup) { return NS_OK; }

nsresult nsAsyncStreamCopier::InitInternal(nsIInputStream* source,
                                           nsIOutputStream* sink,
                                           nsIEventTarget* target,
                                           uint32_t chunkSize, bool closeSource,
                                           bool closeSink) {
  NS_ASSERTION(!mSource && !mSink, "Init() called more than once");
  if (chunkSize == 0) {
    chunkSize = nsIOService::gDefaultSegmentSize;
  }
  mChunkSize = chunkSize;

  mSource = source;
  mSink = sink;
  mCloseSource = closeSource;
  mCloseSink = closeSink;

  if (target) {
    mTarget = target;
  } else {
    nsresult rv;
    mTarget = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIAsyncStreamCopier

NS_IMETHODIMP
nsAsyncStreamCopier::Init(nsIInputStream* source, nsIOutputStream* sink,
                          nsIEventTarget* target, bool sourceBuffered,
                          bool sinkBuffered, uint32_t chunkSize,
                          bool closeSource, bool closeSink) {
  NS_ASSERTION(sourceBuffered || sinkBuffered,
               "at least one stream must be buffered");
  mMode = sourceBuffered ? NS_ASYNCCOPY_VIA_READSEGMENTS
                         : NS_ASYNCCOPY_VIA_WRITESEGMENTS;

  return InitInternal(source, sink, target, chunkSize, closeSource, closeSink);
}

//-----------------------------------------------------------------------------
// nsIAsyncStreamCopier2

NS_IMETHODIMP
nsAsyncStreamCopier::Init(nsIInputStream* source, nsIOutputStream* sink,
                          nsIEventTarget* target, uint32_t chunkSize,
                          bool closeSource, bool closeSink) {
  mShouldSniffBuffering = true;

  return InitInternal(source, sink, target, chunkSize, closeSource, closeSink);
}

/**
 * Detect whether the input or the output stream is buffered,
 * bufferize one of them if neither is buffered.
 */
nsresult nsAsyncStreamCopier::ApplyBufferingPolicy() {
  // This function causes I/O, it must not be executed on the main
  // thread.
  MOZ_ASSERT(!NS_IsMainThread());

  if (NS_OutputStreamIsBuffered(mSink)) {
    // Sink is buffered, no need to perform additional buffering
    mMode = NS_ASYNCCOPY_VIA_WRITESEGMENTS;
    return NS_OK;
  }
  if (NS_InputStreamIsBuffered(mSource)) {
    // Source is buffered, no need to perform additional buffering
    mMode = NS_ASYNCCOPY_VIA_READSEGMENTS;
    return NS_OK;
  }

  // No buffering, let's buffer the sink
  nsresult rv;
  nsCOMPtr<nsIBufferedOutputStream> sink =
      do_CreateInstance(NS_BUFFEREDOUTPUTSTREAM_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = sink->Init(mSink, mChunkSize);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mMode = NS_ASYNCCOPY_VIA_WRITESEGMENTS;
  mSink = sink;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Both nsIAsyncStreamCopier and nsIAsyncStreamCopier2

NS_IMETHODIMP
nsAsyncStreamCopier::AsyncCopy(nsIRequestObserver* observer, nsISupports* ctx) {
  LOG(("nsAsyncStreamCopier::AsyncCopy [this=%p observer=%p]\n", this,
       observer));

  NS_ASSERTION(mSource && mSink, "not initialized");
  nsresult rv;

  if (observer) {
    // build proxy for observer events
    rv = NS_NewRequestObserverProxy(getter_AddRefs(mObserver), observer, ctx);
    if (NS_FAILED(rv)) return rv;
  }

  // from this point forward, AsyncCopy is going to return NS_OK.  any errors
  // will be reported via OnStopRequest.
  mIsPending = true;

  if (mObserver) {
    rv = mObserver->OnStartRequest(AsRequest());
    if (NS_FAILED(rv)) Cancel(rv);
  }

  if (!mShouldSniffBuffering) {
    // No buffer sniffing required, let's proceed
    AsyncCopyInternal();
    return NS_OK;
  }

  if (NS_IsMainThread()) {
    // Don't perform buffer sniffing on the main thread
    nsCOMPtr<nsIRunnable> event = new AsyncApplyBufferingPolicyEvent(this);
    rv = mTarget->Dispatch(event, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      Cancel(rv);
    }
    return NS_OK;
  }

  // We're not going to block the main thread, so let's sniff here
  rv = ApplyBufferingPolicy();
  if (NS_FAILED(rv)) {
    Cancel(rv);
  }
  AsyncCopyInternal();
  return NS_OK;
}

// Launch async copy.
// All errors are reported through the observer.
void nsAsyncStreamCopier::AsyncCopyInternal() {
  MOZ_ASSERT(mMode == NS_ASYNCCOPY_VIA_READSEGMENTS ||
             mMode == NS_ASYNCCOPY_VIA_WRITESEGMENTS);

  nsresult rv;
  // We want to receive progress notifications; release happens in
  // OnAsyncCopyComplete.
  RefPtr<nsAsyncStreamCopier> self = this;
  {
    MutexAutoLock lock(mLock);
    rv = NS_AsyncCopy(mSource, mSink, mTarget, mMode, mChunkSize,
                      OnAsyncCopyComplete, this, mCloseSource, mCloseSink,
                      getter_AddRefs(mCopierCtx));
  }
  if (NS_FAILED(rv)) {
    Cancel(rv);
    return;  // release self
  }

  Unused << self.forget();  // Will be released in OnAsyncCopyComplete
}
