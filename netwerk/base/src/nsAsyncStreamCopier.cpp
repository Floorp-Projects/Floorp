/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIOService.h"
#include "nsAsyncStreamCopier.h"
#include "nsIEventTarget.h"
#include "nsStreamUtils.h"
#include "nsNetSegmentUtils.h"
#include "nsNetUtil.h"
#include "prlog.h"

using namespace mozilla;

#if defined(PR_LOGGING)
//
// NSPR_LOG_MODULES=nsStreamCopier:5
//
static PRLogModuleInfo *gStreamCopierLog = nsnull;
#endif
#define LOG(args) PR_LOG(gStreamCopierLog, PR_LOG_DEBUG, args)

//-----------------------------------------------------------------------------

nsAsyncStreamCopier::nsAsyncStreamCopier()
    : mLock("nsAsyncStreamCopier.mLock")
    , mMode(NS_ASYNCCOPY_VIA_READSEGMENTS)
    , mChunkSize(nsIOService::gDefaultSegmentSize)
    , mStatus(NS_OK)
    , mIsPending(false)
{
#if defined(PR_LOGGING)
    if (!gStreamCopierLog)
        gStreamCopierLog = PR_NewLogModule("nsStreamCopier");
#endif
    LOG(("Creating nsAsyncStreamCopier @%x\n", this));
}

nsAsyncStreamCopier::~nsAsyncStreamCopier()
{
    LOG(("Destroying nsAsyncStreamCopier @%x\n", this));
}

bool
nsAsyncStreamCopier::IsComplete(nsresult *status)
{
    MutexAutoLock lock(mLock);
    if (status)
        *status = mStatus;
    return !mIsPending;
}

void
nsAsyncStreamCopier::Complete(nsresult status)
{
    LOG(("nsAsyncStreamCopier::Complete [this=%x status=%x]\n", this, status));

    nsCOMPtr<nsIRequestObserver> observer;
    nsCOMPtr<nsISupports> ctx;
    {
        MutexAutoLock lock(mLock);
        mCopierCtx = nsnull;

        if (mIsPending) {
            mIsPending = false;
            mStatus = status;

            // setup OnStopRequest callback and release references...
            observer = mObserver;
            ctx = mObserverContext;
            mObserver = nsnull;
            mObserverContext = nsnull;
        }
    }

    if (observer) {
        LOG(("  calling OnStopRequest [status=%x]\n", status));
        observer->OnStopRequest(this, ctx, status);
    }
}

void
nsAsyncStreamCopier::OnAsyncCopyComplete(void *closure, nsresult status)
{
    nsAsyncStreamCopier *self = (nsAsyncStreamCopier *) closure;
    self->Complete(status);
    NS_RELEASE(self); // addref'd in AsyncCopy
}

//-----------------------------------------------------------------------------
// nsISupports

NS_IMPL_THREADSAFE_ISUPPORTS2(nsAsyncStreamCopier,
                              nsIRequest,
                              nsIAsyncStreamCopier)

//-----------------------------------------------------------------------------
// nsIRequest

NS_IMETHODIMP
nsAsyncStreamCopier::GetName(nsACString &name)
{
    name.Truncate();
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::IsPending(bool *result)
{
    *result = !IsComplete();
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::GetStatus(nsresult *status)
{
    IsComplete(status);
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::Cancel(nsresult status)
{
    nsCOMPtr<nsISupports> copierCtx;
    {
        MutexAutoLock lock(mLock);
        if (!mIsPending)
            return NS_OK;
        copierCtx.swap(mCopierCtx);
    }

    if (NS_SUCCEEDED(status)) {
        NS_WARNING("cancel with non-failure status code");
        status = NS_BASE_STREAM_CLOSED;
    }

    if (copierCtx)
        NS_CancelAsyncCopy(copierCtx, status);

    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::Suspend()
{
    NS_NOTREACHED("nsAsyncStreamCopier::Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAsyncStreamCopier::Resume()
{
    NS_NOTREACHED("nsAsyncStreamCopier::Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAsyncStreamCopier::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = LOAD_NORMAL;
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    *aLoadGroup = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIAsyncStreamCopier

NS_IMETHODIMP
nsAsyncStreamCopier::Init(nsIInputStream *source,
                          nsIOutputStream *sink,
                          nsIEventTarget *target,
                          bool sourceBuffered,
                          bool sinkBuffered,
                          PRUint32 chunkSize,
                          bool closeSource,
                          bool closeSink)
{
    NS_ASSERTION(sourceBuffered || sinkBuffered, "at least one stream must be buffered");

    if (chunkSize == 0)
        chunkSize = nsIOService::gDefaultSegmentSize;
    mChunkSize = chunkSize;

    mSource = source;
    mSink = sink;
    mCloseSource = closeSource;
    mCloseSink = closeSink;

    mMode = sourceBuffered ? NS_ASYNCCOPY_VIA_READSEGMENTS
                           : NS_ASYNCCOPY_VIA_WRITESEGMENTS;
    if (target)
        mTarget = target;
    else {
        nsresult rv;
        mTarget = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::AsyncCopy(nsIRequestObserver *observer, nsISupports *ctx)
{
    LOG(("nsAsyncStreamCopier::AsyncCopy [this=%x observer=%x]\n", this, observer));

    NS_ASSERTION(mSource && mSink, "not initialized");
    nsresult rv;

    if (observer) {
        // build proxy for observer events
        rv = NS_NewRequestObserverProxy(getter_AddRefs(mObserver), observer);
        if (NS_FAILED(rv)) return rv;
    }

    // from this point forward, AsyncCopy is going to return NS_OK.  any errors
    // will be reported via OnStopRequest.
    mIsPending = true;

    mObserverContext = ctx;
    if (mObserver) {
        rv = mObserver->OnStartRequest(this, mObserverContext);
        if (NS_FAILED(rv))
            Cancel(rv);
    }

    // we want to receive progress notifications; release happens in
    // OnAsyncCopyComplete.
    NS_ADDREF_THIS();
    {
      MutexAutoLock lock(mLock);
      rv = NS_AsyncCopy(mSource, mSink, mTarget, mMode, mChunkSize,
                        OnAsyncCopyComplete, this, mCloseSource, mCloseSink,
                        getter_AddRefs(mCopierCtx));
    }
    if (NS_FAILED(rv)) {
        NS_RELEASE_THIS();
        Cancel(rv);
    }

    return NS_OK;
}
