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

#include "nsStreamTransportService.h"
#include "nsNetSegmentUtils.h"
#include "nsAutoLock.h"
#include "netCore.h"
#include "prlog.h"

#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsISeekableStream.h"
#include "nsIPipe.h"
#include "nsITransport.h"
#include "nsIRunnable.h"
#include "nsIProxyObjectManager.h"

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=nsStreamTransport:5
//
static PRLogModuleInfo *gSTSLog;
#endif
#define LOG(args) PR_LOG(gSTSLog, PR_LOG_DEBUG, args)

#define MIN_THREADS   1
#define MAX_THREADS   4
#define DEFAULT_SEGMENT_SIZE  4096
#define DEFAULT_SEGMENT_COUNT 16

static nsStreamTransportService *gSTS = nsnull;

//-----------------------------------------------------------------------------

inline static nsresult
NewEventSinkProxy(nsITransportEventSink *sink, nsIEventQueue *eventQ,
                  nsITransportEventSink **result)
{
    return NS_GetProxyForObject(eventQ,
                                NS_GET_IID(nsITransportEventSink),
                                sink,
                                PROXY_ASYNC | PROXY_ALWAYS,
                                (void **) result);
}

//-----------------------------------------------------------------------------
// nsInputStreamTransport
//-----------------------------------------------------------------------------

class nsInputStreamTransport : public nsIRunnable
                             , public nsITransport
                             , public nsIOutputStreamNotify
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSITRANSPORT
    NS_DECL_NSIOUTPUTSTREAMNOTIFY

    nsInputStreamTransport(nsIInputStream *source,
                           PRUint32 offset,
                           PRUint32 limit,
                           PRBool closeWhenDone)
        : mSource(source)
        , mSourceCondition(NS_OK)
        , mOffset(offset)
        , mLimit(limit)
        , mSegSize(0)
        , mInProgress(PR_FALSE)
        , mCloseWhenDone(closeWhenDone)
        , mFirstTime(PR_TRUE)
    {
        NS_ADDREF(gSTS);
    }

    virtual ~nsInputStreamTransport()
    {
        nsStreamTransportService *serv = gSTS;
        NS_RELEASE(serv);
    }

    static NS_METHOD FillPipeSegment(nsIOutputStream *, void *, char *,
                                     PRUint32, PRUint32, PRUint32 *);

private:
    // the pipe input end is never accessed from Run
    nsCOMPtr<nsIAsyncInputStream>   mPipeIn;

    nsCOMPtr<nsIAsyncOutputStream>  mPipeOut;
    nsCOMPtr<nsITransportEventSink> mEventSink;
    nsCOMPtr<nsIInputStream>        mSource;
    nsresult                        mSourceCondition;
    PRUint32                        mOffset;
    PRUint32                        mLimit;
    PRUint32                        mSegSize;
    PRPackedBool                    mInProgress;
    PRPackedBool                    mCloseWhenDone;
    PRPackedBool                    mFirstTime;
};

NS_IMPL_THREADSAFE_ISUPPORTS3(nsInputStreamTransport,
                              nsIRunnable,
                              nsITransport,
                              nsIOutputStreamNotify)

NS_METHOD
nsInputStreamTransport::FillPipeSegment(nsIOutputStream *stream,
                                        void *closure,
                                        char *segment,
                                        PRUint32 offset,
                                        PRUint32 count,
                                        PRUint32 *countRead)
{
    nsInputStreamTransport *trans = (nsInputStreamTransport *) closure;

    // apply read limit
    PRUint32 limit = trans->mLimit - trans->mOffset;
    if (count > limit) {
        count = limit;
        if (count == 0) {
            *countRead = 0;
            return trans->mSourceCondition = NS_BASE_STREAM_CLOSED;
        }
    }

    nsresult rv = trans->mSource->Read(segment, count, countRead);
    if (NS_FAILED(rv))
        trans->mSourceCondition = rv;
    else if (*countRead == 0)
        trans->mSourceCondition = NS_BASE_STREAM_CLOSED;
    else {
        trans->mOffset += *countRead;
        if (trans->mEventSink)
            trans->mEventSink->OnTransportStatus(trans,
                                                 nsITransport::STATUS_READING,
                                                 trans->mOffset, trans->mLimit);
    }

    return trans->mSourceCondition;
}

/** nsIRunnable **/

NS_IMETHODIMP
nsInputStreamTransport::Run()
{
    LOG(("nsInputStreamTransport::Run\n"));

    nsresult rv;
    PRUint32 n;

    if (mFirstTime) {
        mFirstTime = PR_FALSE;

        if (mOffset != ~0U) {
            nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mSource);
            if (seekable)
                seekable->Seek(nsISeekableStream::NS_SEEK_SET, mOffset);
        }
        
        // mOffset now holds the number of bytes read, which we will use to
        // enforce mLimit.
        mOffset = 0;
    }

    // do as much work as we can before yielding this thread.
    for (;;) {
        rv = mPipeOut->WriteSegments(FillPipeSegment, this, mSegSize, &n);

        LOG(("nsInputStreamTransport: WriteSegments returned [this=%x rv=%x n=%u]\n",
            this, rv, n));

        if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
            // wait for pipe to become writable
            mPipeOut->AsyncWait(this, 0, nsnull);
            break;
        }

        if (NS_SUCCEEDED(rv)) {
            if (n == 0)
                rv = NS_BASE_STREAM_CLOSED;
        }
        else if (NS_FAILED(mSourceCondition))
            rv = mSourceCondition;

        // check for source error/eof
        if (NS_FAILED(rv)) {
            LOG(("nsInputStreamTransport: got write error [this=%x error=%x]\n",
                this, rv));
            // close mPipeOut, propogating error condition...
            mPipeOut->CloseEx(rv);
            mPipeOut = 0;
            if (mCloseWhenDone)
                mSource->Close();
            mSource = 0;
            break;
        }
    }
    return NS_OK;
}

/** nsIOutputStreamNotify **/

NS_IMETHODIMP
nsInputStreamTransport::OnOutputStreamReady(nsIAsyncOutputStream *stream)
{
    LOG(("nsInputStreamTransport::OnOutputStreamReady\n"));

    // called on whatever thread removed data from the pipe or closed it.
    NS_ASSERTION(mPipeOut == stream, "wrong stream");
    nsresult rv = gSTS->Dispatch(this); 
    NS_ASSERTION(NS_SUCCEEDED(rv), "Dispatch failed");
    return NS_OK;
}

/** nsITransport **/

NS_IMETHODIMP
nsInputStreamTransport::OpenInputStream(PRUint32 flags,
                                        PRUint32 segsize,
                                        PRUint32 segcount,
                                        nsIInputStream **result)
{
    NS_ENSURE_TRUE(!mInProgress, NS_ERROR_IN_PROGRESS);

    // XXX if the caller requests an unbuffered stream, then perhaps
    //     we'd want to simply return mSource; however, then we would
    //     not be reading mSource on a background thread.  is this ok?
 
    PRBool nonblocking = !(flags & OPEN_BLOCKING);

    net_ResolveSegmentParams(segsize, segcount);
    nsIMemory *segalloc = net_GetSegmentAlloc(segsize);

    nsresult rv = NS_NewPipe2(getter_AddRefs(mPipeIn),
                              getter_AddRefs(mPipeOut),
                              nonblocking, PR_TRUE,
                              segsize, segcount, segalloc);
    if (NS_FAILED(rv)) return rv;

    mSegSize = segsize;
    mInProgress = PR_TRUE;

    rv = gSTS->Dispatch(this); // now writing to pipe
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = mPipeIn);
    return NS_OK;
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

    return mPipeIn->CloseEx(reason);
}

NS_IMETHODIMP
nsInputStreamTransport::SetEventSink(nsITransportEventSink *sink,
                                     nsIEventQueue *eventQ)
{
    NS_ENSURE_TRUE(!mInProgress, NS_ERROR_IN_PROGRESS);

    if (eventQ)
        return NewEventSinkProxy(sink, eventQ, getter_AddRefs(mEventSink));

    mEventSink = sink;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsOutputStreamTransport
//-----------------------------------------------------------------------------

class nsOutputStreamTransport : public nsIRunnable
                              , public nsITransport
                              , public nsIInputStreamNotify
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSITRANSPORT
    NS_DECL_NSIINPUTSTREAMNOTIFY

    nsOutputStreamTransport(nsIOutputStream *sink,
                            PRUint32 offset,
                            PRUint32 limit,
                            PRBool closeWhenDone)
        : mSink(sink)
        , mSinkCondition(NS_OK)
        , mOffset(offset)
        , mLimit(limit)
        , mSegSize(0)
        , mInProgress(PR_FALSE)
        , mCloseWhenDone(closeWhenDone)
        , mFirstTime(PR_TRUE)
    {
        NS_ADDREF(gSTS);
    }

    virtual ~nsOutputStreamTransport()
    {
        nsStreamTransportService *serv = gSTS;
        NS_RELEASE(serv);
    }

    static NS_METHOD ConsumePipeSegment(nsIInputStream *, void *, const char *,
                                        PRUint32, PRUint32, PRUint32 *);

private:
    // the pipe output end is never accessed from Run
    nsCOMPtr<nsIAsyncOutputStream>  mPipeOut;
 
    nsCOMPtr<nsIAsyncInputStream>   mPipeIn;
    nsCOMPtr<nsITransportEventSink> mEventSink;
    nsCOMPtr<nsIOutputStream>       mSink;
    nsresult                        mSinkCondition;
    PRUint32                        mOffset;
    PRUint32                        mLimit;
    PRUint32                        mSegSize;
    PRPackedBool                    mInProgress;
    PRPackedBool                    mCloseWhenDone;
    PRPackedBool                    mFirstTime;
};

NS_IMPL_THREADSAFE_ISUPPORTS3(nsOutputStreamTransport,
                              nsIRunnable,
                              nsITransport,
                              nsIInputStreamNotify)

NS_METHOD
nsOutputStreamTransport::ConsumePipeSegment(nsIInputStream *stream,
                                            void *closure,
                                            const char *segment,
                                            PRUint32 offset,
                                            PRUint32 count,
                                            PRUint32 *countWritten)
{
    nsOutputStreamTransport *trans = (nsOutputStreamTransport *) closure;

    // apply write limit
    PRUint32 limit = trans->mLimit - trans->mOffset;
    if (count > limit) {
        count = limit;
        if (count == 0) {
            *countWritten = 0;
            return trans->mSinkCondition = NS_BASE_STREAM_CLOSED;
        }
    }

    nsresult rv = trans->mSink->Write(segment, count, countWritten);
    if (NS_FAILED(rv))
        trans->mSinkCondition = rv;
    else if (*countWritten == 0)
        trans->mSinkCondition = NS_BASE_STREAM_CLOSED;
    else {
        trans->mOffset += *countWritten;
        if (trans->mEventSink)
            trans->mEventSink->OnTransportStatus(trans,
                                                 nsITransport::STATUS_WRITING,
                                                 trans->mOffset, trans->mLimit);
    }

    return trans->mSinkCondition;
}

/** nsIRunnable **/

NS_IMETHODIMP
nsOutputStreamTransport::Run()
{
    LOG(("nsOutputStreamTransport::Run\n"));

    nsresult rv;
    PRUint32 n;

    if (mFirstTime) {
        mFirstTime = PR_FALSE;

        if (mOffset != ~0U) {
            nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mSink);
            if (seekable)
                seekable->Seek(nsISeekableStream::NS_SEEK_SET, mOffset);
        }
        
        // mOffset now holds the number of bytes written, which we will use to
        // enforce mLimit.
        mOffset = 0;
    }

    // do as much work as we can before yielding this thread.
    for (;;) {
        rv = mPipeIn->ReadSegments(ConsumePipeSegment, this, mSegSize, &n);

        LOG(("nsOutputStreamTransport: ReadSegments returned [this=%x rv=%x n=%u]\n",
            this, rv, n));

        if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
            // wait for pipe to become readable
            mPipeIn->AsyncWait(this, 0, nsnull);
            break;
        }

        if (NS_SUCCEEDED(rv)) {
            if (n == 0)
                rv = NS_BASE_STREAM_CLOSED;
        }
        else if (NS_FAILED(mSinkCondition))
            rv = mSinkCondition;

        // check for sink error
        if (NS_FAILED(rv)) {
            LOG(("nsOutputStreamTransport: got read error [this=%x error=%x]\n",
                this, rv));
            // close mPipeIn, propogating error condition...
            mPipeIn->CloseEx(rv);
            mPipeIn = 0;
            if (mCloseWhenDone)
                mSink->Close();
            mSink = 0;
            break;
        }
    }
    return NS_OK;
}

/** nsIInputStreamNotify **/

NS_IMETHODIMP
nsOutputStreamTransport::OnInputStreamReady(nsIAsyncInputStream *stream)
{
    LOG(("nsOutputStreamTransport::OnInputStreamReady\n"));

    // called on whatever thread added data to the pipe or closed it.
    NS_ASSERTION(mPipeIn == stream, "wrong stream");
    nsresult rv = gSTS->Dispatch(this); 
    NS_ASSERTION(NS_SUCCEEDED(rv), "Dispatch failed");
    return NS_OK;
}

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

    // XXX if the caller requests an unbuffered stream, then perhaps
    //     we'd want to simply return mSink; however, then we would
    //     not be writing to mSink on a background thread.  is this ok?
 
    PRBool nonblocking = !(flags & OPEN_BLOCKING);

    net_ResolveSegmentParams(segsize, segcount);
    nsIMemory *segalloc = net_GetSegmentAlloc(segsize);

    nsresult rv = NS_NewPipe2(getter_AddRefs(mPipeIn),
                              getter_AddRefs(mPipeOut),
                              PR_TRUE, nonblocking,
                              segsize, segcount, segalloc);
    if (NS_FAILED(rv)) return rv;

    mSegSize = segsize;
    mInProgress = PR_TRUE;

    rv = gSTS->Dispatch(this); // now reading from pipe
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = mPipeOut);
    return NS_OK;
}

NS_IMETHODIMP
nsOutputStreamTransport::Close(nsresult reason)
{
    if (NS_SUCCEEDED(reason))
        reason = NS_BASE_STREAM_CLOSED;

    return mPipeOut->CloseEx(reason);
}

NS_IMETHODIMP
nsOutputStreamTransport::SetEventSink(nsITransportEventSink *sink,
                                      nsIEventQueue *eventQ)
{
    NS_ENSURE_TRUE(!mInProgress, NS_ERROR_IN_PROGRESS);

    if (eventQ)
        return NewEventSinkProxy(sink, eventQ, getter_AddRefs(mEventSink));

    mEventSink = sink;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsStreamTransportService
//-----------------------------------------------------------------------------

nsStreamTransportService::nsStreamTransportService()
    : mLock(PR_NewLock())
{
    gSTS = this;

#if defined(PR_LOGGING)
    gSTSLog = PR_NewLogModule("nsStreamTransport");
#endif
}

nsStreamTransportService::~nsStreamTransportService()
{
    gSTS = 0;

    if (mLock)
        PR_DestroyLock(mLock);
}

NS_IMETHODIMP
nsStreamTransportService::Init()
{
    nsAutoLock lock(mLock);

    LOG(("nsStreamTransportService::Init\n"));

    return NS_NewThreadPool(getter_AddRefs(mPool),
                            MIN_THREADS, MAX_THREADS, 0);
}

NS_IMETHODIMP
nsStreamTransportService::Shutdown()
{
    nsAutoLock lock(mLock);

    LOG(("nsStreamTransportService::Shutdown\n"));

    if (mPool) {
        mPool->Shutdown();
        mPool = 0;
    }
    return NS_OK;
}

nsresult
nsStreamTransportService::Dispatch(nsIRunnable *runnable)
{
    nsAutoLock lock(mLock);

    if (!mPool)
        return NS_ERROR_NOT_INITIALIZED;

    return mPool->DispatchRequest(runnable);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStreamTransportService, nsIStreamTransportService)

NS_IMETHODIMP
nsStreamTransportService::CreateInputTransport(nsIInputStream *stream,
                                               PRInt32 offset,
                                               PRInt32 limit,
                                               PRBool closeWhenDone,
                                               nsITransport **result)
{
    nsAutoLock lock(mLock);
    NS_ENSURE_TRUE(mPool, NS_ERROR_NOT_INITIALIZED);

    nsInputStreamTransport *trans =
        new nsInputStreamTransport(stream, offset, limit, closeWhenDone);
    if (!trans)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*result = trans);
    return NS_OK;
}

NS_IMETHODIMP
nsStreamTransportService::CreateOutputTransport(nsIOutputStream *stream,
                                                PRInt32 offset,
                                                PRInt32 limit,
                                                PRBool closeWhenDone,
                                                nsITransport **result)
{
    nsAutoLock lock(mLock);
    NS_ENSURE_TRUE(mPool, NS_ERROR_NOT_INITIALIZED);

    nsOutputStreamTransport *trans =
        new nsOutputStreamTransport(stream, offset, limit, closeWhenDone);
    if (!trans)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*result = trans);
    return NS_OK;
}
                                            
