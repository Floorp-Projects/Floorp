/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStreamTransportService.h"
#include "nsXPCOMCIDInternal.h"
#include "nsNetSegmentUtils.h"
#include "nsTransportUtils.h"
#include "nsStreamUtils.h"
#include "nsNetError.h"
#include "nsNetCID.h"

#include "nsIServiceManager.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsISeekableStream.h"
#include "nsIPipe.h"
#include "nsITransport.h"
#include "nsIRunnable.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"

//-----------------------------------------------------------------------------
// nsInputStreamTransport
//
// Implements nsIInputStream as a wrapper around the real input stream.  This
// allows the transport to support seeking, range-limiting, progress reporting,
// and close-when-done semantics while utilizing NS_AsyncCopy.
//-----------------------------------------------------------------------------

class nsInputStreamTransport : public nsITransport
                             , public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITRANSPORT
    NS_DECL_NSIINPUTSTREAM

    nsInputStreamTransport(nsIInputStream *source,
                           PRUint64 offset,
                           PRUint64 limit,
                           bool closeWhenDone)
        : mSource(source)
        , mOffset(offset)
        , mLimit(limit)
        , mCloseWhenDone(closeWhenDone)
        , mFirstTime(true)
        , mInProgress(false)
    {
    }

    virtual ~nsInputStreamTransport()
    {
    }

private:
    nsCOMPtr<nsIAsyncInputStream>   mPipeIn;

    // while the copy is active, these members may only be accessed from the
    // nsIInputStream implementation.
    nsCOMPtr<nsITransportEventSink> mEventSink;
    nsCOMPtr<nsIInputStream>        mSource;
    PRUint64                        mOffset;
    PRUint64                        mLimit;
    bool                            mCloseWhenDone;
    bool                            mFirstTime;

    // this variable serves as a lock to prevent the state of the transport
    // from being modified once the copy is in progress.
    bool                            mInProgress;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsInputStreamTransport,
                              nsITransport,
                              nsIInputStream)

/** nsITransport **/

NS_IMETHODIMP
nsInputStreamTransport::OpenInputStream(PRUint32 flags,
                                        PRUint32 segsize,
                                        PRUint32 segcount,
                                        nsIInputStream **result)
{
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
    rv = NS_NewPipe2(getter_AddRefs(mPipeIn),
                     getter_AddRefs(pipeOut),
                     nonblocking, true,
                     segsize, segcount);
    if (NS_FAILED(rv)) return rv;

    mInProgress = true;

    // startup async copy process...
    rv = NS_AsyncCopy(this, pipeOut, target,
                      NS_ASYNCCOPY_VIA_WRITESEGMENTS, segsize);
    if (NS_SUCCEEDED(rv))
        NS_ADDREF(*result = mPipeIn);

    return rv;
}

NS_IMETHODIMP
nsInputStreamTransport::OpenOutputStream(PRUint32 flags,
                                         PRUint32 segsize,
                                         PRUint32 segcount,
                                         nsIOutputStream **result)
{
    // this transport only supports reading!
    NS_NOTREACHED("nsInputStreamTransport::OpenOutputStream");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsInputStreamTransport::Close(nsresult reason)
{
    if (NS_SUCCEEDED(reason))
        reason = NS_BASE_STREAM_CLOSED;

    return mPipeIn->CloseWithStatus(reason);
}

NS_IMETHODIMP
nsInputStreamTransport::SetEventSink(nsITransportEventSink *sink,
                                     nsIEventTarget *target)
{
    NS_ENSURE_TRUE(!mInProgress, NS_ERROR_IN_PROGRESS);

    if (target)
        return net_NewTransportEventSinkProxy(getter_AddRefs(mEventSink),
                                              sink, target);

    mEventSink = sink;
    return NS_OK;
}

/** nsIInputStream **/

NS_IMETHODIMP
nsInputStreamTransport::Close()
{
    if (mCloseWhenDone)
        mSource->Close();

    // make additional reads return early...
    mOffset = mLimit = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamTransport::Available(PRUint32 *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInputStreamTransport::Read(char *buf, PRUint32 count, PRUint32 *result)
{
    if (mFirstTime) {
        mFirstTime = false;
        if (mOffset != 0) {
            // read from current position if offset equal to max
            if (mOffset != LL_MAXUINT) {
                nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mSource);
                if (seekable)
                    seekable->Seek(nsISeekableStream::NS_SEEK_SET, mOffset);
            }
            // reset offset to zero so we can use it to enforce limit
            mOffset = 0;
        }
    }

    // limit amount read
    PRUint32 max = mLimit - mOffset;
    if (max == 0) {
        *result = 0;
        return NS_OK;
    }
        
    if (count > max)
        count = max;

    nsresult rv = mSource->Read(buf, count, result);

    if (NS_SUCCEEDED(rv)) {
        mOffset += *result;
        if (mEventSink)
            mEventSink->OnTransportStatus(this, STATUS_READING, mOffset, mLimit);
    }
    return rv;
}

NS_IMETHODIMP
nsInputStreamTransport::ReadSegments(nsWriteSegmentFun writer, void *closure,
                                     PRUint32 count, PRUint32 *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsInputStreamTransport::IsNonBlocking(bool *result)
{
    *result = false;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOutputStreamTransport
//
// Implements nsIOutputStream as a wrapper around the real input stream.  This
// allows the transport to support seeking, range-limiting, progress reporting,
// and close-when-done semantics while utilizing NS_AsyncCopy.
//-----------------------------------------------------------------------------

class nsOutputStreamTransport : public nsITransport
                              , public nsIOutputStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITRANSPORT
    NS_DECL_NSIOUTPUTSTREAM

    nsOutputStreamTransport(nsIOutputStream *sink,
                            PRUint64 offset,
                            PRUint64 limit,
                            bool closeWhenDone)
        : mSink(sink)
        , mOffset(offset)
        , mLimit(limit)
        , mCloseWhenDone(closeWhenDone)
        , mFirstTime(true)
        , mInProgress(false)
    {
    }

    virtual ~nsOutputStreamTransport()
    {
    }

private:
    nsCOMPtr<nsIAsyncOutputStream>  mPipeOut;
 
    // while the copy is active, these members may only be accessed from the
    // nsIOutputStream implementation.
    nsCOMPtr<nsITransportEventSink> mEventSink;
    nsCOMPtr<nsIOutputStream>       mSink;
    PRUint64                        mOffset;
    PRUint64                        mLimit;
    bool                            mCloseWhenDone;
    bool                            mFirstTime;

    // this variable serves as a lock to prevent the state of the transport
    // from being modified once the copy is in progress.
    bool                            mInProgress;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsOutputStreamTransport,
                              nsITransport,
                              nsIOutputStream)

/** nsITransport **/

NS_IMETHODIMP
nsOutputStreamTransport::OpenInputStream(PRUint32 flags,
                                         PRUint32 segsize,
                                         PRUint32 segcount,
                                         nsIInputStream **result)
{
    // this transport only supports writing!
    NS_NOTREACHED("nsOutputStreamTransport::OpenInputStream");
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsOutputStreamTransport::OpenOutputStream(PRUint32 flags,
                                          PRUint32 segsize,
                                          PRUint32 segcount,
                                          nsIOutputStream **result)
{
    NS_ENSURE_TRUE(!mInProgress, NS_ERROR_IN_PROGRESS);

    nsresult rv;
    nsCOMPtr<nsIEventTarget> target =
            do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    // XXX if the caller requests an unbuffered stream, then perhaps
    //     we'd want to simply return mSink; however, then we would
    //     not be writing to mSink on a background thread.  is this ok?
 
    bool nonblocking = !(flags & OPEN_BLOCKING);

    net_ResolveSegmentParams(segsize, segcount);

    nsCOMPtr<nsIAsyncInputStream> pipeIn;
    rv = NS_NewPipe2(getter_AddRefs(pipeIn),
                     getter_AddRefs(mPipeOut),
                     true, nonblocking,
                     segsize, segcount);
    if (NS_FAILED(rv)) return rv;

    mInProgress = true;

    // startup async copy process...
    rv = NS_AsyncCopy(pipeIn, this, target,
                      NS_ASYNCCOPY_VIA_READSEGMENTS, segsize);
    if (NS_SUCCEEDED(rv))
        NS_ADDREF(*result = mPipeOut);

    return rv;
}

NS_IMETHODIMP
nsOutputStreamTransport::Close(nsresult reason)
{
    if (NS_SUCCEEDED(reason))
        reason = NS_BASE_STREAM_CLOSED;

    return mPipeOut->CloseWithStatus(reason);
}

NS_IMETHODIMP
nsOutputStreamTransport::SetEventSink(nsITransportEventSink *sink,
                                      nsIEventTarget *target)
{
    NS_ENSURE_TRUE(!mInProgress, NS_ERROR_IN_PROGRESS);

    if (target)
        return net_NewTransportEventSinkProxy(getter_AddRefs(mEventSink),
                                              sink, target);

    mEventSink = sink;
    return NS_OK;
}

/** nsIOutputStream **/

NS_IMETHODIMP
nsOutputStreamTransport::Close()
{
    if (mCloseWhenDone)
        mSink->Close();

    // make additional writes return early...
    mOffset = mLimit = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsOutputStreamTransport::Flush()
{
    return NS_OK;
}

NS_IMETHODIMP
nsOutputStreamTransport::Write(const char *buf, PRUint32 count, PRUint32 *result)
{
    if (mFirstTime) {
        mFirstTime = false;
        if (mOffset != 0) {
            // write to current position if offset equal to max
            if (mOffset != LL_MAXUINT) {
                nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mSink);
                if (seekable)
                    seekable->Seek(nsISeekableStream::NS_SEEK_SET, mOffset);
            }
            // reset offset to zero so we can use it to enforce limit
            mOffset = 0;
        }
    }

    // limit amount written
    PRUint32 max = mLimit - mOffset;
    if (max == 0) {
        *result = 0;
        return NS_OK;
    }
        
    if (count > max)
        count = max;

    nsresult rv = mSink->Write(buf, count, result);

    if (NS_SUCCEEDED(rv)) {
        mOffset += *result;
        if (mEventSink)
            mEventSink->OnTransportStatus(this, STATUS_WRITING, mOffset, mLimit);
    }
    return rv;
}

NS_IMETHODIMP
nsOutputStreamTransport::WriteSegments(nsReadSegmentFun reader, void *closure,
                                       PRUint32 count, PRUint32 *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsOutputStreamTransport::WriteFrom(nsIInputStream *in, PRUint32 count, PRUint32 *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsOutputStreamTransport::IsNonBlocking(bool *result)
{
    *result = false;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsStreamTransportService
//-----------------------------------------------------------------------------

nsStreamTransportService::~nsStreamTransportService()
{
    NS_ASSERTION(!mPool, "thread pool wasn't shutdown");
}

nsresult
nsStreamTransportService::Init()
{
    mPool = do_CreateInstance(NS_THREADPOOL_CONTRACTID);
    NS_ENSURE_STATE(mPool);

    // Configure the pool
    mPool->SetThreadLimit(4);
    mPool->SetIdleThreadLimit(1);
    mPool->SetIdleThreadTimeout(PR_SecondsToInterval(60));

    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    if (obsSvc)
        obsSvc->AddObserver(this, "xpcom-shutdown-threads", false);
    return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsStreamTransportService,
                              nsIStreamTransportService,
                              nsIEventTarget,
                              nsIObserver)

NS_IMETHODIMP
nsStreamTransportService::Dispatch(nsIRunnable *task, PRUint32 flags)
{
    NS_ENSURE_TRUE(mPool, NS_ERROR_NOT_INITIALIZED);
    return mPool->Dispatch(task, flags);
}

NS_IMETHODIMP
nsStreamTransportService::IsOnCurrentThread(bool *result)
{
    NS_ENSURE_TRUE(mPool, NS_ERROR_NOT_INITIALIZED);
    return mPool->IsOnCurrentThread(result);
}

NS_IMETHODIMP
nsStreamTransportService::CreateInputTransport(nsIInputStream *stream,
                                               PRInt64 offset,
                                               PRInt64 limit,
                                               bool closeWhenDone,
                                               nsITransport **result)
{
    nsInputStreamTransport *trans =
        new nsInputStreamTransport(stream, offset, limit, closeWhenDone);
    if (!trans)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*result = trans);
    return NS_OK;
}

NS_IMETHODIMP
nsStreamTransportService::CreateOutputTransport(nsIOutputStream *stream,
                                                PRInt64 offset,
                                                PRInt64 limit,
                                                bool closeWhenDone,
                                                nsITransport **result)
{
    nsOutputStreamTransport *trans =
        new nsOutputStreamTransport(stream, offset, limit, closeWhenDone);
    if (!trans)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*result = trans);
    return NS_OK;
}

NS_IMETHODIMP
nsStreamTransportService::Observe(nsISupports *subject, const char *topic,
                                  const PRUnichar *data)
{
  NS_ASSERTION(strcmp(topic, "xpcom-shutdown-threads") == 0, "oops");

  if (mPool) {
    mPool->Shutdown();
    mPool = nsnull;
  }
  return NS_OK;
}
