/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAsyncStreamCopier.h"
#include "nsStreamUtils.h"
#include "nsNetSegmentUtils.h"
#include "nsNetUtil.h"
#include "nsAutoLock.h"
#include "prlog.h"

#if defined(PR_LOGGING)
//
// NSPR_LOG_MODULES=nsStreamCopier:5
//
static PRLogModuleInfo *gStreamCopierLog = nsnull;
#endif
#define LOG(args) PR_LOG(gStreamCopierLog, PR_LOG_DEBUG, args)

//-----------------------------------------------------------------------------

nsAsyncStreamCopier::nsAsyncStreamCopier()
    : mInput(this)
    , mOutput(this)
    , mLock(PR_NewLock())
    , mChunkSize(NET_DEFAULT_SEGMENT_SIZE)
    , mStatus(NS_OK)
    , mIsPending(PR_FALSE)
{
#if defined(PR_LOGGING)
    if (!gStreamCopierLog)
        gStreamCopierLog = PR_NewLogModule("nsStreamCopier");
#endif
}

nsAsyncStreamCopier::~nsAsyncStreamCopier()
{
    PR_DestroyLock(mLock);
}

PRBool
nsAsyncStreamCopier::IsComplete(nsresult *status)
{
    nsAutoLock lock(mLock);
    if (status)
        *status = mStatus;
    return !mIsPending;
}

void
nsAsyncStreamCopier::Complete(nsresult reason)
{
    LOG(("nsAsyncStreamCopier::Complete [this=%x reason=%x]\n", this, reason));

    nsCOMPtr<nsIRequestObserver> observer;
    nsCOMPtr<nsISupports> ctx;
    {
        nsAutoLock lock(mLock);
        if (mIsPending) {
            mIsPending = PR_FALSE;
            mStatus = reason;

            // setup OnStopRequest callback and release references...
            observer = mObserver;
            ctx = mObserverContext;
            mObserver = nsnull;
            mObserverContext = nsnull;
        }
    }

    if (observer) {
        if (reason == NS_BASE_STREAM_CLOSED)
            reason = NS_OK;
        LOG(("  calling OnStopRequest [status=%x]\n", reason));
        observer->OnStopRequest(this, ctx, reason);
    }
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
    name = NS_LITERAL_CSTRING("nsAsyncStreamCopier");
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::IsPending(PRBool *result)
{
    *result = !IsComplete();
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::GetStatus(nsresult *status)
{
    IsComplete(status);

    // mask successful "error" code.
    if (*status == NS_BASE_STREAM_CLOSED)
        *status = NS_OK;

    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::Cancel(nsresult status)
{
    if (IsComplete())
        return NS_OK;

    if (NS_SUCCEEDED(status)) {
        NS_WARNING("cancel with non-failure status code");
        status = NS_BASE_STREAM_CLOSED;
    }

    mInput.CloseEx(status);
    mOutput.CloseEx(status);
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::Suspend()
{
    // this could be fairly easily implemented by making Read/ReadSegments
    // return NS_BASE_STREAM_WOULD_BLOCK.
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsAsyncStreamCopier::Resume()
{
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
    return NS_ERROR_NOT_IMPLEMENTED;
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
    return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// nsIAsyncStreamCopier

NS_IMETHODIMP
nsAsyncStreamCopier::Init(nsIInputStream *source,
                          nsIOutputStream *sink,
                          PRBool sourceBuffered,
                          PRBool sinkBuffered,
                          PRUint32 chunkSize)
{
    if (chunkSize == 0)
        chunkSize = NET_DEFAULT_SEGMENT_SIZE;
    mChunkSize = chunkSize;

    mInput.mSource = source;
    mInput.mAsyncSource = do_QueryInterface(source);
    mInput.mBuffered = sourceBuffered;

    mOutput.mSink = sink;
    mOutput.mAsyncSink = do_QueryInterface(sink);
    mOutput.mBuffered = sinkBuffered;
    return NS_OK;
}

NS_IMETHODIMP
nsAsyncStreamCopier::AsyncCopy(nsIRequestObserver *observer, nsISupports *ctx)
{
    LOG(("nsAsyncStreamCopier::AsyncCopy [this=%x observer=%x]\n", this, observer));

    NS_ENSURE_ARG_POINTER(observer);
    NS_ENSURE_TRUE(mInput.mSource, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(mOutput.mSink, NS_ERROR_NOT_INITIALIZED);

    nsresult rv;

    // we could perhaps work around this requirement by automatically using
    // the stream transport service, but then we are left having to make a
    // rather arbitrary selection between opening an asynchronous input 
    // stream or an asynchronous output stream.  that choice is probably
    // best left up to the consumer of this async stream copier.
    if (!mInput.mAsyncSource && !mOutput.mAsyncSink) {
        NS_ERROR("at least one stream must be asynchronous");
        return NS_ERROR_UNEXPECTED;
    }

    // build proxy for observer events
    rv = NS_NewRequestObserverProxy(getter_AddRefs(mObserver), observer);
    if (NS_FAILED(rv)) return rv;

    // from this point forward, AsyncCopy is going to return NS_OK.  any errors
    // will be reported via OnStopRequest.
    mIsPending = PR_TRUE;

    mObserverContext = ctx;
    rv = mObserver->OnStartRequest(this, mObserverContext);
    if (NS_FAILED(rv))
        Cancel(rv);

    rv = NS_AsyncCopy(&mInput, &mOutput, mInput.mBuffered, mOutput.mBuffered, mChunkSize);
    if (NS_FAILED(rv))
        Cancel(rv);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsInputWrapper
//
// NOTE: the input stream methods may be accessed on any thread; however, we
//       assume that Read/ReadSegments will not be called simultaneously on 
//       more than one thread.

NS_IMETHODIMP_(nsrefcnt) nsAsyncStreamCopier::
nsInputWrapper::AddRef()
{
    return mCopier->AddRef();
}

NS_IMETHODIMP_(nsrefcnt) nsAsyncStreamCopier::
nsInputWrapper::Release()
{
    return mCopier->Release();
}

NS_IMPL_QUERY_INTERFACE3(nsAsyncStreamCopier::nsInputWrapper,
                         nsIAsyncInputStream,
                         nsIInputStream,
                         nsIInputStreamNotify)

NS_IMETHODIMP nsAsyncStreamCopier::
nsInputWrapper::Close()
{
    return CloseEx(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsInputWrapper::Available(PRUint32 *avail)
{
    nsresult status;
    if (mCopier->IsComplete(&status))
        return status;

    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);

    nsresult rv = mSource->Available(avail);
    if (NS_FAILED(rv))
        CloseEx(rv);
    return rv;
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsInputWrapper::Read(char *buf, PRUint32 count, PRUint32 *countRead)
{
    nsresult status;
    if (mCopier->IsComplete(&status)) {
        *countRead = 0;
        return status == NS_BASE_STREAM_CLOSED ? NS_OK : status;
    }

    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);

    nsresult rv = mSource->Read(buf, count, countRead);
    /*
    if (NS_FAILED(rv)) {
        if (rv != NS_BASE_STREAM_WOULD_BLOCK)
            CloseEx(rv);
        // else, not a real error
    }
    else if (*countRead == 0)
        CloseEx(NS_BASE_STREAM_CLOSED);
    */
    return rv;
}

NS_METHOD nsAsyncStreamCopier::
nsInputWrapper::ReadSegmentsThunk(nsIInputStream *stream,
                                  void *closure,
                                  const char *segment,
                                  PRUint32 offset,
                                  PRUint32 count,
                                  PRUint32 *countRead)
{
    nsInputWrapper *self = (nsInputWrapper *) closure;
    return self->mWriter(self, self->mClosure, segment, offset, count, countRead);
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsInputWrapper::ReadSegments(nsWriteSegmentFun writer, void *closure,
                             PRUint32 count, PRUint32 *countRead)
{
    nsresult status;
    if (mCopier->IsComplete(&status)) {
        *countRead = 0;
        return status == NS_BASE_STREAM_CLOSED ? NS_OK : status;
    }

    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);

    if (!mBuffered)
        return NS_ERROR_NOT_IMPLEMENTED;

    mWriter = writer;
    mClosure = closure;

    nsresult rv = mSource->ReadSegments(ReadSegmentsThunk, this, count, countRead);
    /*
    if (NS_FAILED(rv)) {
        if (rv != NS_BASE_STREAM_WOULD_BLOCK)
            CloseEx(rv);
        // else, not a real error
    }
    else if (*countRead == 0)
        CloseEx(NS_BASE_STREAM_CLOSED);
    */
    return rv;
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsInputWrapper::IsNonBlocking(PRBool *result)
{
    nsresult status;
    if (mCopier->IsComplete(&status))
        return status;

    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);
    return mSource->IsNonBlocking(result);
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsInputWrapper::CloseEx(nsresult reason)
{
    mCopier->Complete(reason);

    if (mAsyncSource)
        mAsyncSource->CloseEx(reason);
    else
        mSource->Close();
    return NS_OK;
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsInputWrapper::AsyncWait(nsIInputStreamNotify *notify, PRUint32 amount, nsIEventQueue *eventQ)
{
    // we'll cheat a little bit here since we know that NS_AsyncCopy does pass
    // an event queue.
    NS_ASSERTION(eventQ == nsnull, "unexpected");

    if (mAsyncSource) {
        mNotify = notify;
        return mAsyncSource->AsyncWait(this, amount, eventQ);
    }

    // else, stream is ready
    notify->OnInputStreamReady(this);
    return NS_OK;
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsInputWrapper::OnInputStreamReady(nsIAsyncInputStream *stream)
{
    // simple thunk
    return mNotify->OnInputStreamReady(this);
}

//-----------------------------------------------------------------------------
// nsOutputWrapper
//
// NOTE: the output stream methods may be accessed on any thread; however, we
//       assume that Write/WriteSegments will not be called simultaneously on 
//       more than one thread.

NS_IMETHODIMP_(nsrefcnt) nsAsyncStreamCopier::
nsOutputWrapper::AddRef()
{
    return mCopier->AddRef();
}

NS_IMETHODIMP_(nsrefcnt) nsAsyncStreamCopier::
nsOutputWrapper::Release()
{
    return mCopier->Release();
}

NS_IMPL_QUERY_INTERFACE3(nsAsyncStreamCopier::nsOutputWrapper,
                         nsIAsyncOutputStream,
                         nsIOutputStream,
                         nsIOutputStreamNotify)

NS_IMETHODIMP nsAsyncStreamCopier::
nsOutputWrapper::Close()
{
    return CloseEx(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsOutputWrapper::Flush()
{
    NS_NOTREACHED("nsOutputWrapper::Flush");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsOutputWrapper::Write(const char *buf, PRUint32 count, PRUint32 *countWritten)
{
    nsresult status;
    if (mCopier->IsComplete(&status)) {
        *countWritten = 0;
        return status;
    }

    NS_ENSURE_TRUE(mSink, NS_ERROR_NOT_INITIALIZED);

    nsresult rv = mSink->Write(buf, count, countWritten);
    /*
    if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK)
        CloseEx(rv);
    */
    return rv;
}

NS_METHOD nsAsyncStreamCopier::
nsOutputWrapper::WriteSegmentsThunk(nsIOutputStream *stream,
                                    void *closure,
                                    char *segment,
                                    PRUint32 offset,
                                    PRUint32 count,
                                    PRUint32 *countWritten)
{
    nsOutputWrapper *self = (nsOutputWrapper *) closure;
    return self->mReader(self, self->mClosure, segment, offset, count, countWritten);
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsOutputWrapper::WriteSegments(nsReadSegmentFun reader, void *closure,
                               PRUint32 count, PRUint32 *countWritten)
{
    nsresult status;
    if (mCopier->IsComplete(&status)) {
        *countWritten = 0;
        return status;
    }

    NS_ENSURE_TRUE(mSink, NS_ERROR_NOT_INITIALIZED);

    if (!mBuffered)
        return NS_ERROR_NOT_IMPLEMENTED;

    mReader = reader;
    mClosure = closure;

    nsresult rv = mSink->WriteSegments(WriteSegmentsThunk, this, count, countWritten);
    /*
    if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK)
        CloseEx(rv);
    */
    return rv;
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsOutputWrapper::WriteFrom(nsIInputStream *stream, PRUint32 count, PRUint32 *countWritten)
{
    NS_NOTREACHED("nsOutputWrapper::WriteFrom");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsOutputWrapper::IsNonBlocking(PRBool *result)
{
    nsresult status;
    if (mCopier->IsComplete(&status))
        return status;

    NS_ENSURE_TRUE(mSink, NS_ERROR_NOT_INITIALIZED);
    return mSink->IsNonBlocking(result);
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsOutputWrapper::CloseEx(nsresult reason)
{
    mCopier->Complete(reason);

    if (mAsyncSink)
        mAsyncSink->CloseEx(reason);
    else
        mSink->Close();
    return NS_OK;
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsOutputWrapper::AsyncWait(nsIOutputStreamNotify *notify, PRUint32 amount, nsIEventQueue *eventQ)
{
    // we'll cheat a little bit here since we know that NS_AsyncCopy does pass
    // an event queue.
    NS_ASSERTION(eventQ == nsnull, "unexpected");

    if (mAsyncSink) {
        mNotify = notify;
        return mAsyncSink->AsyncWait(this, amount, eventQ);
    }
    
    // else, stream is ready
    notify->OnOutputStreamReady(this);
    return NS_OK;
}

NS_IMETHODIMP nsAsyncStreamCopier::
nsOutputWrapper::OnOutputStreamReady(nsIAsyncOutputStream *stream)
{
    // simple thunk
    return mNotify->OnOutputStreamReady(this);
}
