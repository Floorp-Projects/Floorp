/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sts=4 sw=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include "nsIOService.h"
#include "nsInputStreamPump.h"
#include "nsIServiceManager.h"
#include "nsIStreamTransportService.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsISeekableStream.h"
#include "nsITransport.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsStreamUtils.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsCOMPtr.h"
#include "prlog.h"
#include "nsPrintfCString.h"
#include "GeckoProfiler.h"
#include <algorithm>

static NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);

#if defined(PR_LOGGING)
//
// NSPR_LOG_MODULES=nsStreamPump:5
//
static PRLogModuleInfo *gStreamPumpLog = nullptr;
#endif
#define LOG(args) PR_LOG(gStreamPumpLog, PR_LOG_DEBUG, args)

//-----------------------------------------------------------------------------
// nsInputStreamPump methods
//-----------------------------------------------------------------------------

nsInputStreamPump::nsInputStreamPump()
    : mState(STATE_IDLE)
    , mStreamOffset(0)
    , mStreamLength(UINT64_MAX)
    , mStatus(NS_OK)
    , mSuspendCount(0)
    , mLoadFlags(LOAD_NORMAL)
    , mWaiting(false)
    , mCloseWhenDone(false)
    , mRetargeting(false)
{
#if defined(PR_LOGGING)
    if (!gStreamPumpLog)
        gStreamPumpLog = PR_NewLogModule("nsStreamPump");
#endif
}

nsInputStreamPump::~nsInputStreamPump()
{
}

nsresult
nsInputStreamPump::Create(nsInputStreamPump  **result,
                          nsIInputStream      *stream,
                          int64_t              streamPos,
                          int64_t              streamLen,
                          uint32_t             segsize,
                          uint32_t             segcount,
                          bool                 closeWhenDone)
{
    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    nsRefPtr<nsInputStreamPump> pump = new nsInputStreamPump();
    if (pump) {
        rv = pump->Init(stream, streamPos, streamLen,
                        segsize, segcount, closeWhenDone);
        if (NS_SUCCEEDED(rv)) {
            *result = nullptr;
            pump.swap(*result);
        }
    }
    return rv;
}

struct PeekData {
  PeekData(nsInputStreamPump::PeekSegmentFun fun, void* closure)
    : mFunc(fun), mClosure(closure) {}

  nsInputStreamPump::PeekSegmentFun mFunc;
  void* mClosure;
};

static NS_METHOD
CallPeekFunc(nsIInputStream *aInStream, void *aClosure,
             const char *aFromSegment, uint32_t aToOffset, uint32_t aCount,
             uint32_t *aWriteCount)
{
  NS_ASSERTION(aToOffset == 0, "Called more than once?");
  NS_ASSERTION(aCount > 0, "Called without data?");

  PeekData* data = static_cast<PeekData*>(aClosure);
  data->mFunc(data->mClosure,
              reinterpret_cast<const uint8_t*>(aFromSegment), aCount);
  return NS_BINDING_ABORTED;
}

nsresult
nsInputStreamPump::PeekStream(PeekSegmentFun callback, void* closure)
{
  NS_ASSERTION(mAsyncStream, "PeekStream called without stream");

  // See if the pipe is closed by checking the return of Available.
  uint64_t dummy64;
  nsresult rv = mAsyncStream->Available(&dummy64);
  if (NS_FAILED(rv))
    return rv;
  uint32_t dummy = (uint32_t)std::min(dummy64, (uint64_t)UINT32_MAX);

  PeekData data(callback, closure);
  return mAsyncStream->ReadSegments(CallPeekFunc,
                                    &data,
                                    nsIOService::gDefaultSegmentSize,
                                    &dummy);
}

nsresult
nsInputStreamPump::EnsureWaiting()
{
    // no need to worry about multiple threads... an input stream pump lives
    // on only one thread at a time.
    MOZ_ASSERT(mAsyncStream);
    if (!mWaiting) {
        // Ensure OnStateStop is called on the main thread.
        if (mState == STATE_STOP) {
            nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
            if (mTargetThread != mainThread) {
                mTargetThread = do_QueryInterface(mainThread);
            }
        }
        MOZ_ASSERT(mTargetThread);
        nsresult rv = mAsyncStream->AsyncWait(this, 0, 0, mTargetThread);
        if (NS_FAILED(rv)) {
            NS_ERROR("AsyncWait failed");
            return rv;
        }
        // Any retargeting during STATE_START or START_TRANSFER is complete
        // after the call to AsyncWait; next callback wil be on mTargetThread.
        mRetargeting = false;
        mWaiting = true;
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsInputStreamPump::nsISupports
//-----------------------------------------------------------------------------

// although this class can only be accessed from one thread at a time, we do
// allow its ownership to move from thread to thread, assuming the consumer
// understands the limitations of this.
NS_IMPL_ISUPPORTS4(nsInputStreamPump,
                   nsIRequest,
                   nsIThreadRetargetableRequest,
                   nsIInputStreamCallback,
                   nsIInputStreamPump)

//-----------------------------------------------------------------------------
// nsInputStreamPump::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsInputStreamPump::GetName(nsACString &result)
{
    result.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamPump::IsPending(bool *result)
{
    *result = (mState != STATE_IDLE);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamPump::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamPump::Cancel(nsresult status)
{
    LOG(("nsInputStreamPump::Cancel [this=%p status=%x]\n",
        this, status));

    if (NS_FAILED(mStatus)) {
        LOG(("  already canceled\n"));
        return NS_OK;
    }

    NS_ASSERTION(NS_FAILED(status), "cancel with non-failure status code");
    mStatus = status;

    // close input stream
    if (mAsyncStream) {
        mAsyncStream->CloseWithStatus(status);
        if (mSuspendCount == 0)
            EnsureWaiting();
        // Otherwise, EnsureWaiting will be called by Resume().
        // Note that while suspended, OnInputStreamReady will
        // not do anything, and also note that calling asyncWait
        // on a closed stream works and will dispatch an event immediately.
    }
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamPump::Suspend()
{
    LOG(("nsInputStreamPump::Suspend [this=%p]\n", this));
    NS_ENSURE_TRUE(mState != STATE_IDLE, NS_ERROR_UNEXPECTED);
    ++mSuspendCount;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamPump::Resume()
{
    LOG(("nsInputStreamPump::Resume [this=%p]\n", this));
    NS_ENSURE_TRUE(mSuspendCount > 0, NS_ERROR_UNEXPECTED);
    NS_ENSURE_TRUE(mState != STATE_IDLE, NS_ERROR_UNEXPECTED);

    if (--mSuspendCount == 0)
        EnsureWaiting();
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamPump::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamPump::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    mLoadFlags = aLoadFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamPump::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    NS_IF_ADDREF(*aLoadGroup = mLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamPump::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsInputStreamPump::nsIInputStreamPump implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsInputStreamPump::Init(nsIInputStream *stream,
                        int64_t streamPos, int64_t streamLen,
                        uint32_t segsize, uint32_t segcount,
                        bool closeWhenDone)
{
    NS_ENSURE_TRUE(mState == STATE_IDLE, NS_ERROR_IN_PROGRESS);

    mStreamOffset = uint64_t(streamPos);
    if (int64_t(streamLen) >= int64_t(0))
        mStreamLength = uint64_t(streamLen);
    mStream = stream;
    mSegSize = segsize;
    mSegCount = segcount;
    mCloseWhenDone = closeWhenDone;

    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamPump::AsyncRead(nsIStreamListener *listener, nsISupports *ctxt)
{
    NS_ENSURE_TRUE(mState == STATE_IDLE, NS_ERROR_IN_PROGRESS);
    NS_ENSURE_ARG_POINTER(listener);
    MOZ_ASSERT(NS_IsMainThread(), "nsInputStreamPump should be read from the "
                                  "main thread only.");

    //
    // OK, we need to use the stream transport service if
    //
    // (1) the stream is blocking
    // (2) the stream does not support nsIAsyncInputStream
    //

    bool nonBlocking;
    nsresult rv = mStream->IsNonBlocking(&nonBlocking);
    if (NS_FAILED(rv)) return rv;

    if (nonBlocking) {
        mAsyncStream = do_QueryInterface(mStream);
        //
        // if the stream supports nsIAsyncInputStream, and if we need to seek
        // to a starting offset, then we must do so here.  in the non-async
        // stream case, the stream transport service will take care of seeking
        // for us.
        // 
        if (mAsyncStream && (mStreamOffset != UINT64_MAX)) {
            nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mStream);
            if (seekable)
                seekable->Seek(nsISeekableStream::NS_SEEK_SET, mStreamOffset);
        }
    }

    if (!mAsyncStream) {
        // ok, let's use the stream transport service to read this stream.
        nsCOMPtr<nsIStreamTransportService> sts =
            do_GetService(kStreamTransportServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsITransport> transport;
        rv = sts->CreateInputTransport(mStream, mStreamOffset, mStreamLength,
                                       mCloseWhenDone, getter_AddRefs(transport));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIInputStream> wrapper;
        rv = transport->OpenInputStream(0, mSegSize, mSegCount, getter_AddRefs(wrapper));
        if (NS_FAILED(rv)) return rv;

        mAsyncStream = do_QueryInterface(wrapper, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    // release our reference to the original stream.  from this point forward,
    // we only reference the "stream" via mAsyncStream.
    mStream = 0;

    // mStreamOffset now holds the number of bytes currently read.  we use this
    // to enforce the mStreamLength restriction.
    mStreamOffset = 0;

    // grab event queue (we must do this here by contract, since all notifications
    // must go to the thread which called AsyncRead)
    mTargetThread = do_GetCurrentThread();
    NS_ENSURE_STATE(mTargetThread);

    rv = EnsureWaiting();
    if (NS_FAILED(rv)) return rv;

    if (mLoadGroup)
        mLoadGroup->AddRequest(this, nullptr);

    mState = STATE_START;
    mListener = listener;
    mListenerContext = ctxt;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsInputStreamPump::nsIInputStreamCallback implementation
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsInputStreamPump::OnInputStreamReady(nsIAsyncInputStream *stream)
{
    LOG(("nsInputStreamPump::OnInputStreamReady [this=%p]\n", this));

    PROFILER_LABEL("Input", "nsInputStreamPump::OnInputStreamReady");
    // this function has been called from a PLEvent, so we can safely call
    // any listener or progress sink methods directly from here.

    for (;;) {
        if (mSuspendCount || mState == STATE_IDLE) {
            mWaiting = false;
            break;
        }

        uint32_t nextState;
        switch (mState) {
        case STATE_START:
            nextState = OnStateStart();
            break;
        case STATE_TRANSFER:
            nextState = OnStateTransfer();
            break;
        case STATE_STOP:
            mRetargeting = false;
            nextState = OnStateStop();
            break;
        default:
            nextState = 0;
            NS_NOTREACHED("Unknown enum value.");
            return NS_ERROR_UNEXPECTED;
        }

        bool stillTransferring = (mState == STATE_TRANSFER &&
                                  nextState == STATE_TRANSFER);
        if (stillTransferring) {
            NS_ASSERTION(NS_SUCCEEDED(mStatus),
                         "Should not have failed status for ongoing transfer");
        } else {
            NS_ASSERTION(mState != nextState,
                         "Only OnStateTransfer can be called more than once.");
        }
        if (mRetargeting) {
            NS_ASSERTION(mState != STATE_STOP,
                         "Retargeting should not happen during OnStateStop.");
        }

        // Set mRetargeting so EnsureWaiting will be called. It ensures that
        // OnStateStop is called on the main thread. 
        if (nextState == STATE_STOP && !NS_IsMainThread()) {
            mRetargeting = true;
        }

        // Wait asynchronously if there is still data to transfer, or we're
        // switching event delivery to another thread.
        if (!mSuspendCount && (stillTransferring || mRetargeting)) {
            mState = nextState;
            mWaiting = false;
            nsresult rv = EnsureWaiting();
            if (NS_SUCCEEDED(rv))
                break;
            
            // Failure to start asynchronous wait: stop transfer.
            // Do not set mStatus if it was previously set to report a failure.
            if (NS_SUCCEEDED(mStatus)) {
                mStatus = rv;
            }
            nextState = STATE_STOP;
        }

        mState = nextState;
    }
    return NS_OK;
}

uint32_t
nsInputStreamPump::OnStateStart()
{
    PROFILER_LABEL("nsInputStreamPump", "OnStateStart");
    LOG(("  OnStateStart [this=%p]\n", this));

    nsresult rv;

    // need to check the reason why the stream is ready.  this is required
    // so our listener can check our status from OnStartRequest.
    // XXX async streams should have a GetStatus method!
    if (NS_SUCCEEDED(mStatus)) {
        uint64_t avail;
        rv = mAsyncStream->Available(&avail);
        if (NS_FAILED(rv) && rv != NS_BASE_STREAM_CLOSED)
            mStatus = rv;
    }

    rv = mListener->OnStartRequest(this, mListenerContext);

    // an error returned from OnStartRequest should cause us to abort; however,
    // we must not stomp on mStatus if already canceled.
    if (NS_FAILED(rv) && NS_SUCCEEDED(mStatus))
        mStatus = rv;

    return NS_SUCCEEDED(mStatus) ? STATE_TRANSFER : STATE_STOP;
}

uint32_t
nsInputStreamPump::OnStateTransfer()
{
    PROFILER_LABEL("Input", "nsInputStreamPump::OnStateTransfer");
    LOG(("  OnStateTransfer [this=%p]\n", this));

    // if canceled, go directly to STATE_STOP...
    if (NS_FAILED(mStatus))
        return STATE_STOP;

    nsresult rv;

    uint64_t avail;
    rv = mAsyncStream->Available(&avail);
    LOG(("  Available returned [stream=%x rv=%x avail=%llu]\n", mAsyncStream.get(), rv, avail));

    if (rv == NS_BASE_STREAM_CLOSED) {
        rv = NS_OK;
        avail = 0;
    }
    else if (NS_SUCCEEDED(rv) && avail) {
        // figure out how much data to report (XXX detect overflow??)
        if (avail > mStreamLength - mStreamOffset)
            avail = mStreamLength - mStreamOffset;

        if (avail) {
            // we used to limit avail to 16K - we were afraid some ODA handlers
            // might assume they wouldn't get more than 16K at once
            // we're removing that limit since it speeds up local file access.
            // Now there's an implicit 64K limit of 4 16K segments
            // NOTE: ok, so the story is as follows.  OnDataAvailable impls
            //       are by contract supposed to consume exactly |avail| bytes.
            //       however, many do not... mailnews... stream converters...
            //       cough, cough.  the input stream pump is fairly tolerant
            //       in this regard; however, if an ODA does not consume any
            //       data from the stream, then we could potentially end up in
            //       an infinite loop.  we do our best here to try to catch
            //       such an error.  (see bug 189672)

            // in most cases this QI will succeed (mAsyncStream is almost always
            // a nsPipeInputStream, which implements nsISeekableStream::Tell).
            int64_t offsetBefore;
            nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mAsyncStream);
            if (seekable && NS_FAILED(seekable->Tell(&offsetBefore))) {
                NS_NOTREACHED("Tell failed on readable stream");
                offsetBefore = 0;
            }

            uint32_t odaAvail =
                avail > UINT32_MAX ?
                UINT32_MAX : uint32_t(avail);

            LOG(("  calling OnDataAvailable [offset=%llu count=%llu(%u)]\n",
                mStreamOffset, avail, odaAvail));

            rv = mListener->OnDataAvailable(this, mListenerContext, mAsyncStream,
                                            mStreamOffset, odaAvail);

            // don't enter this code if ODA failed or called Cancel
            if (NS_SUCCEEDED(rv) && NS_SUCCEEDED(mStatus)) {
                // test to see if this ODA failed to consume data
                if (seekable) {
                    // NOTE: if Tell fails, which can happen if the stream is
                    // now closed, then we assume that everything was read.
                    int64_t offsetAfter;
                    if (NS_FAILED(seekable->Tell(&offsetAfter)))
                        offsetAfter = offsetBefore + odaAvail;
                    if (offsetAfter > offsetBefore)
                        mStreamOffset += (offsetAfter - offsetBefore);
                    else if (mSuspendCount == 0) {
                        //
                        // possible infinite loop if we continue pumping data!
                        //
                        // NOTE: although not allowed by nsIStreamListener, we
                        // will allow the ODA impl to Suspend the pump.  IMAP
                        // does this :-(
                        //
                        NS_ERROR("OnDataAvailable implementation consumed no data");
                        mStatus = NS_ERROR_UNEXPECTED;
                    }
                }
                else
                    mStreamOffset += odaAvail; // assume ODA behaved well
            }
        }
    }

    // an error returned from Available or OnDataAvailable should cause us to
    // abort; however, we must not stomp on mStatus if already canceled.

    if (NS_SUCCEEDED(mStatus)) {
        if (NS_FAILED(rv))
            mStatus = rv;
        else if (avail) {
            // if stream is now closed, advance to STATE_STOP right away.
            // Available may return 0 bytes available at the moment; that
            // would not mean that we are done.
            // XXX async streams should have a GetStatus method!
            rv = mAsyncStream->Available(&avail);
            if (NS_SUCCEEDED(rv))
                return STATE_TRANSFER;
            if (rv != NS_BASE_STREAM_CLOSED)
                mStatus = rv;
        }
    }
    return STATE_STOP;
}

nsresult
nsInputStreamPump::OnStateStopForFailure()
{
    MOZ_ASSERT(NS_FAILED(mStatus), "OnStateStopForFailure should be called "
                                   "in a failed state");
    MOZ_ASSERT(NS_IsMainThread(), "OnStateStopForFailure should be on the "
                                  "main thread");

    mState = OnStateStop();
    return NS_OK;
}

uint32_t
nsInputStreamPump::OnStateStop()
{
    if (NS_FAILED(mStatus)) {
        // If EnsureWaiting has failed, it's possible that we could be off main
        // thread. We may have to dispatch OnStateStop to the main thread
        // directly. Note: this would result in OnStateStop being called
        // outside the context of OnInputStreamReady.
        if (!NS_IsMainThread()) {
            nsresult rv = NS_DispatchToMainThread(
                NS_NewRunnableMethod(this, &nsInputStreamPump::OnStateStopForFailure));
            NS_ENSURE_SUCCESS(rv, STATE_IDLE);
            return STATE_IDLE;
        }
    } else {
        MOZ_ASSERT(NS_IsMainThread(), "In a success state, OnStateStop should "
                                      "be on the main thread");
    }

    PROFILER_LABEL("Input", "nsInputStreamPump::OnStateTransfer");
    LOG(("  OnStateStop [this=%p status=%x]\n", this, mStatus));

    // if an error occurred, we must be sure to pass the error onto the async
    // stream.  in some cases, this is redundant, but since close is idempotent,
    // this is OK.  otherwise, be sure to honor the "close-when-done" option.

    if (NS_FAILED(mStatus))
        mAsyncStream->CloseWithStatus(mStatus);
    else if (mCloseWhenDone)
        mAsyncStream->Close();

    mAsyncStream = 0;
    mTargetThread = 0;
    mIsPending = false;
    mListener->OnStopRequest(this, mListenerContext, mStatus);
    mListener = 0;
    mListenerContext = 0;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nullptr, mStatus);

    return STATE_IDLE;
}

//-----------------------------------------------------------------------------
// nsIThreadRetargetableRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsInputStreamPump::RetargetDeliveryTo(nsIEventTarget* aNewTarget)
{
    NS_ENSURE_ARG(aNewTarget);
    NS_ENSURE_TRUE(mState == STATE_START || mState == STATE_TRANSFER,
                   NS_ERROR_UNEXPECTED);

    // If canceled, do not retarget. Return with canceled status.
    if (NS_FAILED(mStatus)) {
        return mStatus;
    }

    if (aNewTarget == mTargetThread) {
        NS_WARNING("Retargeting delivery to same thread");
        return NS_OK;
    }

    // Ensure that |mListener| and any subsequent listeners can be retargeted
    // to another thread.
    nsresult rv = NS_OK;
    nsCOMPtr<nsIThreadRetargetableStreamListener> retargetableListener =
        do_QueryInterface(mListener, &rv);
    if (NS_SUCCEEDED(rv) && retargetableListener) {
        rv = retargetableListener->CheckListenerChain();
        if (NS_SUCCEEDED(rv)) {
            mTargetThread = aNewTarget;
            mRetargeting = true;
        }
    }
    LOG(("nsInputStreamPump::RetargetDeliveryTo [this=%x aNewTarget=%p] "
         "%s listener [%p] rv[%x]",
         this, aNewTarget, (mTargetThread == aNewTarget ? "success" : "failure"),
         (nsIStreamListener*)mListener, rv));
    return rv;
}
