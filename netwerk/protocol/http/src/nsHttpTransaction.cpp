/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 *   Andreas M. Schneider <clarence@clarence.de>
 */

#include "nsHttpHandler.h"
#include "nsHttpTransaction.h"
#include "nsHttpConnection.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsHttpChunkedDecoder.h"
#include "nsIStringStream.h"
#include "nsIFileStream.h"
#include "nsISocketTransportService.h"
#include "pratom.h"
#include "plevent.h"

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

static char *
LocateHttpStart(char *buf, PRUint32 len)
{
    // if we have received less than 4 bytes of data, then we'll have to
    // just accept a partial match, which may not be correct.
    if (len < 4)
        return (PL_strncasecmp(buf, "HTTP", len) == 0) ? buf : 0;

    // PL_strncasestr would be perfect for this, but unfortunately bug 96571
    // prevents its use here.
    while (len >= 4) {
        if (PL_strncasecmp(buf, "HTTP", 4) == 0)
            return buf;
        buf++;
        len--;
    }
    return 0;
}

#if defined(PR_LOGGING)
static void
LogHeaders(const char *lines)
{
    nsCAutoString buf;
    char *p;
    while ((p = PL_strstr(lines, "\r\n")) != nsnull) {
        buf.Assign(lines, p - lines);
        if (PL_strcasestr(buf.get(), "authorization: ") != nsnull) {
            char *p = PL_strchr(PL_strchr(buf.get(), ' ')+1, ' ');
            while (*++p) *p = '*';
        }
        LOG2(("  %s\n", buf.get()));
        lines = p + 2;
    }
}
#endif

//-----------------------------------------------------------------------------
// nsHttpTransaction <public>
//-----------------------------------------------------------------------------

nsHttpTransaction::nsHttpTransaction(nsIStreamListener *listener,
                                     nsIInterfaceRequestor *callbacks,
                                     PRUint8 caps)
    : mListener(listener)
    , mCallbacks(callbacks)
    , mConnection(nsnull)
    , mResponseHead(nsnull)
    , mContentLength(-1)
    , mContentRead(0)
    , mChunkedDecoder(nsnull)
    , mTransactionDone(0)
    , mStatus(NS_OK)
    , mRestartCount(0)
    , mCapabilities(caps)
    , mHaveStatusLine(PR_FALSE)
    , mHaveAllHeaders(PR_FALSE)
    , mFiredOnStart(PR_FALSE)
    , mNoContent(PR_FALSE)
    , mPrematureEOF(PR_FALSE)
{
    LOG(("Creating nsHttpTransaction @%x\n", this));

    NS_INIT_ISUPPORTS();

    NS_PRECONDITION(listener, "null listener");
}

nsHttpTransaction::~nsHttpTransaction()
{
    LOG(("Destroying nsHttpTransaction @%x\n", this));

    NS_IF_RELEASE(mConnection);

    delete mChunkedDecoder;
    delete mResponseHead;
}

nsresult
nsHttpTransaction::SetupRequest(nsHttpRequestHead *requestHead,
                                nsIInputStream *requestStream)
{
    nsresult rv;

    LOG(("nsHttpTransaction::SetupRequest [this=%x]\n", this));

    NS_ENSURE_ARG_POINTER(requestHead);

    // grab a reference to the calling thread's event queue.
    nsCOMPtr<nsIEventQueueService> eqs;
    nsHttpHandler::get()->GetEventQueueService(getter_AddRefs(eqs));
    if (eqs)
        eqs->ResolveEventQueue(NS_CURRENT_EVENTQ, getter_AddRefs(mConsumerEventQ));

    // build a proxy for the progress event sink
    if (mCallbacks && mConsumerEventQ) {
        nsCOMPtr<nsIProgressEventSink> temp = do_GetInterface(mCallbacks);
        if (temp) {
            nsCOMPtr<nsIProxyObjectManager> mgr;
            nsHttpHandler::get()->GetProxyObjectManager(getter_AddRefs(mgr));
            if (mgr)
                mgr->GetProxyForObject(mConsumerEventQ,
                                       NS_GET_IID(nsIProgressEventSink),
                                       temp,
                                       PROXY_ASYNC | PROXY_ALWAYS,
                                       getter_AddRefs(mProgressSink));
        }
    }

    if (requestHead->Method() == nsHttp::Head)
        mNoContent = PR_TRUE;

    // grab a weak reference to the request head
    mRequestHead = requestHead;

    mReqHeaderBuf.SetLength(0);
    requestHead->Flatten(mReqHeaderBuf);

#if defined(PR_LOGGING)
    if (LOG2_ENABLED()) {
        LOG2(("http request [\n"));
        LogHeaders(mReqHeaderBuf.get());
        LOG2(("]\n"));
    }
#endif

    mReqUploadStream = requestStream;
    if (!mReqUploadStream)
        // Write out end-of-headers sequence if NOT uploading data:
        mReqHeaderBuf.Append("\r\n");

    // Create a string stream for the request header buf
    nsCOMPtr<nsISupports> sup;
    rv = NS_NewCStringInputStream(getter_AddRefs(sup), mReqHeaderBuf);
    if (NS_FAILED(rv)) return rv;
    mReqHeaderStream = do_QueryInterface(sup, &rv);

    return rv;
}

nsresult
nsHttpTransaction::SetConnection(nsHttpConnection *conn)
{
    LOG(("nsHttpTransaction::SetConnection [this=%x mConnection=%x conn=%x]\n",
        this, mConnection, conn));

    mConnection = conn;
    NS_IF_ADDREF(mConnection);
    return NS_OK;
}

nsHttpResponseHead *
nsHttpTransaction::TakeResponseHead()
{
    if (!mHaveAllHeaders)
        return nsnull;

    nsHttpResponseHead *head = mResponseHead;
    mResponseHead = nsnull;
    return head;
}

// called on the socket transport thread
nsresult
nsHttpTransaction::OnDataWritable(nsIOutputStream *os)
{
    PRUint32 n = 0;

    LOG(("nsHttpTransaction::OnDataWritable [this=%x]\n", this));

    // check if we're done writing the headers
    nsresult rv = mReqHeaderStream->Available(&n);
    if (NS_FAILED(rv)) return rv;

    // let at most NS_HTTP_BUFFER_SIZE bytes be written at a time.
    
    if (n != 0)
        return os->WriteFrom(mReqHeaderStream, NS_HTTP_BUFFER_SIZE, &n);

    if (mReqUploadStream)
        return os->WriteFrom(mReqUploadStream, NS_HTTP_BUFFER_SIZE, &n);

    return NS_BASE_STREAM_CLOSED;
}

// called on the socket transport thread
nsresult
nsHttpTransaction::OnDataReadable(nsIInputStream *is)
{
    nsresult rv;

    LOG(("nsHttpTransaction::OnDataReadable [this=%x]\n", this));

    if (!mListener) {
        LOG(("nsHttpTransaction: no listener! closing stream\n"));
        return NS_BASE_STREAM_CLOSED;
    }

    mSource = is;

    // let our listener try to read up to NS_HTTP_BUFFER_SIZE from us.
    rv = mListener->OnDataAvailable(this, nsnull, this,
                                    mContentRead, NS_HTTP_BUFFER_SIZE);

    LOG(("nsHttpTransaction: listener returned [rv=%x]\n", rv));

    mSource = 0;

    // check if this transaction needs to be restarted
    if (mPrematureEOF) {
        mPrematureEOF = PR_FALSE;
        rv = Restart();
        // if successfully restarted, then return an error to abort the 
        // socket transport.
        if (NS_SUCCEEDED(rv))
            rv = NS_BINDING_ABORTED;
    }

    return rv;
}

// called on any thread
nsresult
nsHttpTransaction::OnStopTransaction(nsresult status)
{
    LOG(("nsHttpTransaction::OnStopTransaction [this=%x status=%x]\n",
        this, status));

    // if the connection was reset before we read any part of the response,
    // then we must try to restart the transaction.
    if ((status == NS_ERROR_NET_RESET) && (mContentRead == 0)) {
        // if restarting fails, then we must notify our listener.
        if (NS_SUCCEEDED(Restart()))
            return NS_OK;
    }

    mStatus = status;

	if (mListener) {
		if (!mFiredOnStart) {
			mFiredOnStart = PR_TRUE;
			mListener->OnStartRequest(this, nsnull); 
		}
		mListener->OnStopRequest(this, nsnull, status);
        mListener = 0;

        // from this point forward we can't access the request head.
        mRequestHead = nsnull;
	}
    return NS_OK;
}

void
nsHttpTransaction::OnStatus(nsresult status, const PRUnichar *statusText)
{
    LOG(("nsHttpTransaction::OnStatus [this=%x status=%x]\n", this, status));

    if (mProgressSink)
        mProgressSink->OnStatus(nsnull, nsnull, status, statusText);
}

//-----------------------------------------------------------------------------
// nsHttpTransaction <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpTransaction::Restart()
{
    nsresult rv;

    // limit the number of restart attempts - bug 92224
    if (++mRestartCount >= nsHttpHandler::get()->MaxRequestAttempts()) {
        LOG(("reached max request attempts, failing transaction @%x\n", this));
        return NS_BINDING_FAILED;
    }

    LOG(("restarting transaction @%x\n", this));

    // rewind streams in case we already wrote out the request
    nsCOMPtr<nsIRandomAccessStore> ras = do_QueryInterface(mReqHeaderStream);
    if (ras)
        ras->Seek(PR_SEEK_SET, 0);
    ras = do_QueryInterface(mReqUploadStream);
    if (ras)
        ras->Seek(PR_SEEK_SET, 0);

    // just in case the connection is holding the last reference to us...
    NS_ADDREF_THIS();

    // we don't want the connection to send anymore notifications to us.
    mConnection->DropTransaction();

    nsHttpConnectionInfo *ci = mConnection->ConnectionInfo();
    NS_ADDREF(ci);

    // we must release the connection before calling initiate transaction
    // since we will be getting a new connection.
    NS_RELEASE(mConnection);

    rv = nsHttpHandler::get()->InitiateTransaction(this, ci);
    NS_ASSERTION(NS_SUCCEEDED(rv), "InitiateTransaction failed");

    NS_RELEASE(ci);
    NS_RELEASE_THIS();
    return NS_OK;
}

void
nsHttpTransaction::ParseLine(char *line)
{
    LOG(("nsHttpTransaction::ParseLine [%s]\n", line));

    if (!mHaveStatusLine) {
        mResponseHead->ParseStatusLine(line);
        mHaveStatusLine = PR_TRUE;
        // XXX this should probably never happen
        if (mResponseHead->Version() == NS_HTTP_VERSION_0_9)
            mHaveAllHeaders = PR_TRUE;
    }
    else
        mResponseHead->ParseHeaderLine(line);
}

void
nsHttpTransaction::ParseLineSegment(char *segment, PRUint32 len)
{
    NS_PRECONDITION(!mHaveAllHeaders, "already have all headers");

    if (!mLineBuf.IsEmpty() && mLineBuf.Last() == '\n') {
        // if this segment is a continuation of the previous...
        if (mHaveStatusLine && (*segment == ' ' || *segment == '\t')) {
            // trim off the new line char
            mLineBuf.Truncate(mLineBuf.Length() - 1);
            mLineBuf.Append(segment, len);
        }
        else {
            // trim off the new line char and parse the line
            mLineBuf.Truncate(mLineBuf.Length() - 1);
            ParseLine(NS_CONST_CAST(char*,mLineBuf.get()));
            // stuff the segment into the line buf
            mLineBuf.Assign(segment, len);
        }
    }
    else
        mLineBuf.Append(segment, len);
    
    // a line buf with only a new line char signifies the end of headers.
    if (mLineBuf.First() == '\n') {
        mLineBuf.Truncate();
        // discard this response if it is a 100 continue.
        if (mResponseHead->Status() == 100) {
            LOG(("ignoring 100 response\n"));
            mHaveStatusLine = PR_FALSE;
            mResponseHead->Reset();
            return;
        }
        mHaveAllHeaders = PR_TRUE;
    }
}

nsresult
nsHttpTransaction::ParseHead(char *buf,
                             PRUint32 count,
                             PRUint32 *countRead)
{
    PRUint32 len;
    char *eol;

    LOG(("nsHttpTransaction::ParseHead [count=%u]\n", count));

    *countRead = 0;

    NS_PRECONDITION(!mHaveAllHeaders, "oops");
        
    // allocate the response head object if necessary
    if (!mResponseHead) {
        mResponseHead = new nsHttpResponseHead();
        if (!mResponseHead)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    // if we don't have a status line and the line buf is empty, then
    // this must be the first time we've been called.
    if (!mHaveStatusLine && mLineBuf.IsEmpty()) {
        // tolerate some junk before the status line
        char *p = LocateHttpStart(buf, PR_MIN(count, 32));
        if (!p) {
            mResponseHead->ParseStatusLine("");
            mHaveStatusLine = PR_TRUE;
            mHaveAllHeaders = PR_TRUE;
            return NS_OK;
        }
        if (p > buf) {
            // skip over the junk
            *countRead = p - buf;
            count -= *countRead;
            buf = p;
        }
    }
    // otherwise we can assume that we don't have a HTTP/0.9 response.

    while ((eol = NS_STATIC_CAST(char *, memchr(buf, '\n', count - *countRead))) != nsnull) {
        // found line in range [buf:eol]
        len = eol - buf + 1;

        *countRead += len;

        // actually, the line is in the range [buf:eol-1]
        if ((eol > buf) && (*(eol-1) == '\r'))
            len--;

        buf[len-1] = '\n';
        ParseLineSegment(buf, len);

        if (mHaveAllHeaders)
            return NS_OK;

        // skip over line
        buf = eol + 1;
    }

    // do something about a partial header line
    if (!mHaveAllHeaders && (len = count - *countRead)) {
        *countRead = count;
        // ignore a trailing carriage return, and don't bother calling
        // ParseLineSegment if buf only contains a carriage return.
        if ((buf[len-1] == '\r') && (--len == 0))
            return NS_OK;
        ParseLineSegment(buf, len);
    }
    return NS_OK;
}

// called on the socket thread
nsresult
nsHttpTransaction::HandleContentStart()
{
    nsresult rv;

    LOG(("nsHttpTransaction::HandleContentStart [this=%x response-head=%x]\n",
        this, mResponseHead));

    if (mResponseHead) {
#if defined(PR_LOGGING)
        if (LOG2_ENABLED()) {
            LOG2(("http response [\n"));
            nsCAutoString headers;
            mResponseHead->Flatten(headers, PR_FALSE);
            LogHeaders(headers.get());
            LOG2(("]\n"));
        }
#endif
        // notify the connection, give it a chance to cause a reset.
        PRBool reset = PR_FALSE;
        mConnection->OnHeadersAvailable(this, &reset);

        // looks like we should ignore this response, resetting...
        if (reset) {
            LOG(("resetting transaction's response head\n"));
            mHaveAllHeaders = PR_FALSE;
            mHaveStatusLine = PR_FALSE;
            mResponseHead->Reset();
            // wait to be called again...
            return NS_BASE_STREAM_WOULD_BLOCK;
        }

        // check if this is a no-content response
        switch (mResponseHead->Status()) {
        case 204:
        case 205:
        case 304:
            mNoContent = PR_TRUE;
            LOG(("this response should not contain a body.\n"));
            break;
        }

        if (mNoContent)
            mContentLength = 0;
        else {
            // grab the content-length from the response headers
            mContentLength = mResponseHead->ContentLength();

            // handle chunked encoding here, so we'll know immediately when
            // we're done with the socket.  please note that _all_ other
            // decoding is done when the channel receives the content data
            // so as not to block the socket transport thread too much.
            const char *val = mResponseHead->PeekHeader(nsHttp::Transfer_Encoding);
            if (PL_strcasestr(val, "chunked")) {
                // we only support the "chunked" transfer encoding right now.
                mChunkedDecoder = new nsHttpChunkedDecoder();
                if (!mChunkedDecoder)
                    return NS_ERROR_OUT_OF_MEMORY;
                LOG(("chunked decoder created\n"));
            }
#if defined(PR_LOGGING)
            else if (mContentLength == -1)
                LOG(("waiting for the server to close the connection.\n"));
#endif
        }
    }

    LOG(("nsHttpTransaction [this=%x] sending OnStartRequest\n", this));
    mFiredOnStart = PR_TRUE;

    rv = mListener->OnStartRequest(this, nsnull);
    LOG(("OnStartRequest returned rv=%x\n", rv));
    return rv;
}

// called on the socket thread
nsresult
nsHttpTransaction::HandleContent(char *buf,
                                 PRUint32 count,
                                 PRUint32 *countRead)
{
    nsresult rv;

    LOG(("nsHttpTransaction::HandleContent [this=%x count=%u]\n",
        this, count));

    *countRead = 0;

    if (mTransactionDone)
        return NS_OK;

    NS_PRECONDITION(mConnection, "no connection");

    if (!mFiredOnStart) {
        rv = HandleContentStart();
        if (NS_FAILED(rv)) return rv;
    }

    if (mChunkedDecoder) {
        // give the buf over to the chunked decoder so it can reformat the
        // data and tell us how much is really there.
        rv = mChunkedDecoder->HandleChunkedContent(buf, count, countRead);
        if (NS_FAILED(rv)) return rv;
    }
    else if (mContentLength >= 0) {
        // HTTP/1.0 servers have been known to send erroneous Content-Length
        // headers. So, unless the connection is keep-alive, we must make
        // allowances for a possibly invalid Content-Length header. Thus, if
        // NOT keep-alive, we simply accept everything in |buf|.
        if (mConnection->IsKeepAlive()) {
            *countRead = PRUint32(mContentLength) - mContentRead;
            *countRead = PR_MIN(count, *countRead);
        }
        else {
            *countRead = count;
            // mContentLength might need to be increased...
            if (*countRead + mContentRead > (PRUint32) mContentLength) {
                mContentLength = *countRead + mContentRead;
                //mResponseHead->SetContentLength(mContentLength);
            }
        }
    }
    else
        // when we are just waiting for the server to close the connection...
        *countRead = count;

    if (*countRead) {
        // update count of content bytes read and report progress...
        mContentRead += *countRead;
        if (mProgressSink)
            mProgressSink->OnProgress(nsnull, nsnull, mContentRead, PR_MAX(0, mContentLength));
    }

    LOG(("nsHttpTransaction [this=%x count=%u read=%u mContentRead=%u mContentLength=%d]\n",
        this, count, *countRead, mContentRead, mContentLength));

    // check for end-of-file
    if ((mContentRead == PRUint32(mContentLength)) ||
        (mChunkedDecoder && mChunkedDecoder->ReachedEOF())) {
        // atomically mark the transaction as complete to ensure that
        // OnTransactionComplete is fired only once!
        PRInt32 priorVal = PR_AtomicSet(&mTransactionDone, 1);
        if (priorVal == 0) {
            // let the connection know that we are done with it; this should
            // result in OnStopTransaction being fired.
            return mConnection->OnTransactionComplete(this, NS_OK);
        }
        return NS_OK;
    }

    // if we didn't "read" anything and this is not a no-content response,
    // then we must return NS_BASE_STREAM_WOULD_BLOCK so we'll be called again.
    return (!mNoContent && !*countRead) ? NS_BASE_STREAM_WOULD_BLOCK : NS_OK;
}

void
nsHttpTransaction::DeleteSelfOnConsumerThread()
{
    nsCOMPtr<nsIEventQueueService> eqs;
    nsCOMPtr<nsIEventQueue> currentEventQ;

    LOG(("nsHttpTransaction::DeleteSelfOnConsumerThread [this=%x]\n", this));

    nsHttpHandler::get()->GetEventQueueService(getter_AddRefs(eqs));
    if (eqs)
        eqs->ResolveEventQueue(NS_CURRENT_EVENTQ, getter_AddRefs(currentEventQ));

    if (currentEventQ == mConsumerEventQ)
        delete this;
    else {
        LOG(("proxying delete to consumer thread...\n"));

        PLEvent *event = new PLEvent;
        if (!event) {
            NS_WARNING("out of memory");
            // probably better to leak |this| than to delete it on this thread.
            return;
        }

        PL_InitEvent(event, this,
                nsHttpTransaction::DeleteThis_EventHandlerFunc,
                nsHttpTransaction::DeleteThis_EventCleanupFunc);

        PRStatus status = mConsumerEventQ->PostEvent(event);
        NS_ASSERTION(status == PR_SUCCESS, "PostEvent failed");
    }
}

void *PR_CALLBACK
nsHttpTransaction::DeleteThis_EventHandlerFunc(PLEvent *ev)
{
    nsHttpTransaction *trans =
            NS_STATIC_CAST(nsHttpTransaction *, PL_GetEventOwner(ev));

    LOG(("nsHttpTransaction::DeleteThis_EventHandlerFunc [trans=%x]\n", trans));

    delete trans;
    return nsnull;
}

void PR_CALLBACK
nsHttpTransaction::DeleteThis_EventCleanupFunc(PLEvent *ev)
{
    delete ev;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ADDREF(nsHttpTransaction)

NS_IMETHODIMP_(nsrefcnt)
nsHttpTransaction::Release()
{
    nsrefcnt count;
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    count = PR_AtomicDecrement((PRInt32 *) &mRefCnt);
    NS_LOG_RELEASE(this, count, "nsHttpTransaction");
    if (0 == count) {
        mRefCnt = 1; /* stablize */
        // it is essential that the transaction be destroyed on the consumer 
        // thread (we could be holding the last reference to our consumer).
        DeleteSelfOnConsumerThread();
        return 0;
    }
    return count;
}

NS_IMPL_THREADSAFE_QUERY_INTERFACE2(nsHttpTransaction,
                                    nsIRequest,
                                    nsIInputStream)

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpTransaction::GetName(PRUnichar **aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpTransaction::IsPending(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpTransaction::GetStatus(nsresult *aStatus)
{
    *aStatus = mStatus;
    return NS_OK;
}

// called from any thread
NS_IMETHODIMP
nsHttpTransaction::Cancel(nsresult status)
{
    LOG(("nsHttpTransaction::Cancel [this=%x status=%x]\n", this, status));

    // ignore cancelation if the transaction already has an error status.
    if (NS_FAILED(mStatus)) {
        LOG(("ignoring cancel since transaction has already failed "
             "[this=%x mStatus=%x]\n", this, mStatus));
        return NS_OK;
    }

    // the status must be set immediately as the cancelation may only take
    // action asynchronously.
    mStatus = status;

    // if the transaction is already "done" then there is nothing more to do.
    // ie., our consumer _will_ eventually receive their OnStopRequest.
    PRInt32 priorVal = PR_AtomicSet(&mTransactionDone, 1);
    if (priorVal == 1) {
        LOG(("ignoring cancel since transaction is already done [this=%x]\n", this));
        return NS_OK;
    }

    return nsHttpHandler::get()->CancelTransaction(this, status);
}

NS_IMETHODIMP
nsHttpTransaction::Suspend()
{
    LOG(("nsHttpTransaction::Suspend [this=%x]\n", this));
    if (mConnection && !mTransactionDone)
        mConnection->Suspend();
    return NS_OK;
}

// called from the consumer thread, while nothing is happening on the socket thread.
NS_IMETHODIMP
nsHttpTransaction::Resume()
{
    LOG(("nsHttpTransaction::Resume [this=%x]\n", this));
    if (mConnection && !mTransactionDone)
        mConnection->Resume();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpTransaction::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsHttpTransaction::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpTransaction::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsHttpTransaction::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsIInputStream
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpTransaction::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpTransaction::Available(PRUint32 *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpTransaction::Read(char *buf, PRUint32 count, PRUint32 *bytesWritten)
{
    nsresult rv;

    LOG(("nsHttpTransaction::Read [this=%x count=%u]\n", this, count));

    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);

    if (mTransactionDone)
        return NS_BASE_STREAM_CLOSED;

    *bytesWritten = 0;

    // read some data from our source and put it in the given buf
    rv = mSource->Read(buf, count, bytesWritten);
    LOG(("mSource->Read [rv=%x count=%u countRead=%u]\n", rv, count, *bytesWritten));
    if (NS_FAILED(rv)) {
        LOG(("nsHttpTransaction: mSource->Read() returned [rv=%x]\n", rv));
        return rv;
    }
    if (*bytesWritten == 0) {
        LOG(("nsHttpTransaction: reached EOF\n"));
        if (!mHaveStatusLine) {
            // we've read nothing from the socket...
            mPrematureEOF = PR_TRUE;
            // return would block to prevent being called again.
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
        if (!mHaveAllHeaders && !mLineBuf.IsEmpty()) {
            // the server has not sent the final \r\n terminating the header section,
            // and there is still a header line unparsed.  let's make sure we parse
            // the remaining header line, and then hopefully, the response will be
            // usable (see bug 88792).
            ParseLineSegment("\n", 1);
        }
        return rv;
    }

    // pretend that no bytes were written (since we're just borrowing the
    // given buf anyways).
    count = *bytesWritten;
    *bytesWritten = 0;

    // we may not have read all of the headers yet...
    if (!mHaveAllHeaders) {
        PRUint32 bytesConsumed = 0;

        rv = ParseHead(buf, count, &bytesConsumed);
        if (NS_FAILED(rv)) return rv;

        count -= bytesConsumed;

        if (count && bytesConsumed) {
            // buf has some content in it; shift bytes to top of buf.
            memmove(buf, buf + bytesConsumed, count);
        }
    }

    // even though count may be 0, we still want to call HandleContent
    // so it can complete the transaction if this is a "no-content" response.
    if (mHaveAllHeaders)
        return HandleContent(buf, count, bytesWritten);

    // wait for more data
    return NS_BASE_STREAM_WOULD_BLOCK;
}

NS_IMETHODIMP
nsHttpTransaction::ReadSegments(nsWriteSegmentFun writer, void *closure,
                                PRUint32 count, PRUint32 *countRead)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpTransaction::GetNonBlocking(PRBool *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpTransaction::GetObserver(nsIInputStreamObserver **obs)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
nsHttpTransaction::SetObserver(nsIInputStreamObserver *obs)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
