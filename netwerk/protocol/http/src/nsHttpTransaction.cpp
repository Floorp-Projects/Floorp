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
#include "nsIOService.h"
#include "nsAutoLock.h"
#include "pratom.h"
#include "plevent.h"

#include "nsIStringStream.h"
#include "nsISeekableStream.h"
#include "nsISocketTransport.h"
#include "nsMultiplexInputStream.h"

//-----------------------------------------------------------------------------

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

//-----------------------------------------------------------------------------

static NS_DEFINE_CID(kMultiplexInputStream, NS_MULTIPLEXINPUTSTREAM_CID);

// mLineBuf is limited to this number of bytes.
#define MAX_LINEBUF_LENGTH (1024 * 10)

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

nsHttpTransaction::nsHttpTransaction()
    : mRequestSize(0)
    , mConnection(nsnull)
    , mConnInfo(nsnull)
    , mRequestHead(nsnull)
    , mResponseHead(nsnull)
    , mContentLength(-1)
    , mContentRead(0)
    , mChunkedDecoder(nsnull)
    , mStatus(NS_OK)
    , mLock(PR_NewLock())
    , mTransportStatus(0)
    , mTransportProgress(0)
    , mTransportProgressMax(0)
    , mTransportStatusInProgress(PR_FALSE)
    , mRestartCount(0)
    , mCaps(0)
    , mConnected(PR_FALSE)
    , mHaveStatusLine(PR_FALSE)
    , mHaveAllHeaders(PR_FALSE)
    , mTransactionDone(PR_FALSE)
    , mResponseIsComplete(PR_FALSE)
    , mDidContentStart(PR_FALSE)
    , mNoContent(PR_FALSE)
    , mPrematureEOF(PR_FALSE)
    , mDestroying(PR_FALSE)
{
    LOG(("Creating nsHttpTransaction @%x\n", this));
}

nsHttpTransaction::~nsHttpTransaction()
{
    LOG(("Destroying nsHttpTransaction @%x\n", this));

    NS_IF_RELEASE(mConnection);
    NS_IF_RELEASE(mConnInfo);

    delete mResponseHead;
    delete mChunkedDecoder;

    PR_DestroyLock(mLock);
}

nsresult
nsHttpTransaction::Init(PRUint8 caps,
                        nsHttpConnectionInfo *cinfo,
                        nsHttpRequestHead *requestHead,
                        nsIInputStream *requestBody,
                        PRBool requestBodyHasHeaders,
                        nsIEventQueue *queue,
                        nsIInterfaceRequestor *callbacks,
                        nsITransportEventSink *eventsink,
                        nsIAsyncInputStream **responseBody)
{
    nsresult rv;

    LOG(("nsHttpTransaction::Init [this=%x caps=%x]\n", this, caps));

    NS_ASSERTION(cinfo, "ouch");
    NS_ASSERTION(requestHead, "ouch");

    NS_ADDREF(mConnInfo = cinfo);
    mCallbacks = callbacks;
    mTransportSink = eventsink;
    mConsumerEventQ = queue;
    mCaps = caps;

    if (requestHead->Method() == nsHttp::Head)
        mNoContent = PR_TRUE;

    // grab a weak reference to the request head
    mRequestHead = requestHead;

    // make sure we eliminate any proxy specific headers from 
    // the request if we are talking HTTPS via a SSL tunnel.
    PRBool pruneProxyHeaders = cinfo->UsingSSL() &&
                               cinfo->UsingHttpProxy();
    mReqHeaderBuf.Truncate();
    requestHead->Flatten(mReqHeaderBuf, pruneProxyHeaders);

#if defined(PR_LOGGING)
    if (LOG2_ENABLED()) {
        LOG2(("http request [\n"));
        LogHeaders(mReqHeaderBuf.get());
        LOG2(("]\n"));
    }
#endif

    // If the request body does not include headers or if there is no request
    // body, then we must add the header/body separator manually.
    if (!requestBodyHasHeaders || !requestBody)
        mReqHeaderBuf.Append("\r\n");

    // Create a string stream for the request header buf (the stream holds
    // a non-owning reference to the request header data, so we MUST keep
    // mReqHeaderBuf around).
    nsCOMPtr<nsISupports> sup;
    rv = NS_NewByteInputStream(getter_AddRefs(sup),
                               mReqHeaderBuf.get(),
                               mReqHeaderBuf.Length());
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIInputStream> headers = do_QueryInterface(sup, &rv);

    if (requestBody) {
        // wrap the headers and request body in a multiplexed input stream.
        nsCOMPtr<nsIMultiplexInputStream> multi =
            do_CreateInstance(kMultiplexInputStream, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = multi->AppendStream(headers);
        if (NS_FAILED(rv)) return rv;

        rv = multi->AppendStream(requestBody);
        if (NS_FAILED(rv)) return rv;

        mRequestStream = multi;
    }
    else
        mRequestStream = headers;

    rv = mRequestStream->Available(&mRequestSize);
    if (NS_FAILED(rv)) return rv;

    // create pipe for response stream
    rv = NS_NewPipe2(getter_AddRefs(mPipeIn),
                     getter_AddRefs(mPipeOut),
                     PR_TRUE, PR_TRUE,
                     NS_HTTP_SEGMENT_SIZE,
                     NS_HTTP_SEGMENT_COUNT,
                     nsIOService::gBufferCache);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*responseBody = mPipeIn);
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

//----------------------------------------------------------------------------
// nsHttpTransaction::nsAHttpTransaction
//----------------------------------------------------------------------------

void
nsHttpTransaction::OnTransportStatus(nsresult status, PRUint32 progress)
{
    LOG(("nsHttpTransaction::OnSocketStatus [this=%x status=%x progress=%u]\n",
        this, status, progress));
    
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    PRBool postEvent;
    {
        nsAutoLock lock(mLock);

        // stamp latest socket status
        mTransportStatus = status;
        
        if (status == nsISocketTransport::STATUS_RECEIVING_FROM) {
            // ignore the progress argument and use our own.  as a result,
            // the progress reported will not include the size of the response
            // headers.  this is OK b/c we only want to report the progress
            // downloading the body of the response.
            mTransportProgress = mContentRead;
            mTransportProgressMax = mContentLength;
        }
        else {
            // when uploading, we include the request headers in the progress
            // notifications.
            mTransportProgress = progress;
            mTransportProgressMax = mRequestSize;
        }

        postEvent = !mTransportStatusInProgress;
    }

    // only post an event if there is not already an event in progress.  we
    // do this as an optimization to avoid an excessive number of status events.
    if (postEvent) {
        PLEvent *ev = new PLEvent;
        NS_ADDREF_THIS();
        PL_InitEvent(ev, this, TransportStatus_Handler, TransportStatus_Cleanup);
        if (mConsumerEventQ->PostEvent(ev) != PR_SUCCESS) {
            NS_RELEASE_THIS();
            delete ev;
        }
    }
}

PRUint32
nsHttpTransaction::Available()
{
    PRUint32 size;
    if (NS_FAILED(mRequestStream->Available(&size)))
        size = 0;
    return size;
}

NS_METHOD
nsHttpTransaction::ReadRequestSegment(nsIInputStream *stream,
                                      void *closure,
                                      const char *buf,
                                      PRUint32 offset,
                                      PRUint32 count,
                                      PRUint32 *countRead)
{
    nsHttpTransaction *trans = (nsHttpTransaction *) closure;
    return trans->mReader->OnReadSegment(buf, count, countRead);
}

nsresult
nsHttpTransaction::ReadSegments(nsAHttpSegmentReader *reader,
                                PRUint32 count, PRUint32 *countRead)
{
    NS_ASSERTION(PR_CurrentThread() == gSocketThread, "wrong thread");

    if (!mConnected) {
        mConnected = PR_TRUE;
        mConnection->GetSecurityInfo(getter_AddRefs(mSecurityInfo));
    }

    mReader = reader;

    nsresult rv = mRequestStream->ReadSegments(ReadRequestSegment, this, count, countRead);

    mReader = nsnull;
    return rv;
}

NS_METHOD
nsHttpTransaction::WritePipeSegment(nsIOutputStream *stream,
                                    void *closure,
                                    char *buf,
                                    PRUint32 offset,
                                    PRUint32 count,
                                    PRUint32 *countWritten)
{
    nsHttpTransaction *trans = (nsHttpTransaction *) closure;

    if (trans->mTransactionDone)
        return NS_BASE_STREAM_CLOSED;

    nsresult rv;
    //
    // OK, now let the caller fill this segment with data.
    //
    rv = trans->mWriter->OnWriteSegment(buf, count, countWritten);
    if (NS_FAILED(rv)) return rv; // caller didn't want to write anything

    NS_ASSERTION(*countWritten > 0, "bad writer");

    // now let the transaction "play" with the buffer.  it is free to modify
    // the contents of the buffer and/or modify countWritten.
    rv = trans->ProcessData(buf, *countWritten, countWritten);
    if (NS_FAILED(rv))
        trans->Close(rv);

    return rv; // failure code only stops WriteSegments; it is not propogated.
}

nsresult
nsHttpTransaction::WriteSegments(nsAHttpSegmentWriter *writer,
                                 PRUint32 count, PRUint32 *countWritten)
{
    NS_ASSERTION(PR_CurrentThread() == gSocketThread, "wrong thread");

    if (mTransactionDone)
        return NS_BASE_STREAM_CLOSED;

    mWriter = writer;

    nsresult rv = mPipeOut->WriteSegments(WritePipeSegment, this, count, countWritten);

    mWriter = nsnull;

    // if pipe would block then we need to AsyncWait on it.
    if (rv == NS_BASE_STREAM_WOULD_BLOCK)
        mPipeOut->AsyncWait(this, 0, nsnull);

    return rv;
}

void
nsHttpTransaction::Close(nsresult reason)
{
    LOG(("nsHttpTransaction::Close [this=%x reason=%x]\n", this, reason));

    NS_ASSERTION(PR_CurrentThread() == gSocketThread, "wrong thread");

    if (NS_FAILED(mStatus)) {
        LOG(("  already closed\n"));
        return;
    }

    // we must no longer reference the connection!
    NS_IF_RELEASE(mConnection);
    mConnected = PR_FALSE;

    // if the connection was reset before we read any part of the response,
    // then we must try to restart the transaction.
    if (reason == NS_ERROR_NET_RESET) {
        // if some data was read, then mask the reset error, so our listener
        // will treat this as a normal failure.  XXX we might want to map
        // this error to a special error code to indicate that the transfer
        // was abnormally interrupted.
        if (mContentRead > 0)
            reason = NS_ERROR_ABORT; // XXX NS_ERROR_NET_INTERRUPT??
        // if restarting fails, then we must notify our listener.
        else if (NS_SUCCEEDED(Restart()))
            return;
    }

    if (NS_SUCCEEDED(reason) && !mHaveAllHeaders && !mLineBuf.IsEmpty()) {
        // the server has not sent the final \r\n terminating the header section,
        // and there is still a header line unparsed.  let's make sure we parse
        // the remaining header line, and then hopefully, the response will be
        // usable (see bug 88792).
        ParseLineSegment("\n", 1);
    }

    mTransactionDone = PR_TRUE; // force this flag
    mStatus = reason;

    mPipeOut->CloseEx(reason);
}

//-----------------------------------------------------------------------------
// nsHttpTransaction <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpTransaction::Restart()
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    // limit the number of restart attempts - bug 92224
    if (++mRestartCount >= gHttpHandler->MaxRequestAttempts()) {
        LOG(("reached max request attempts, failing transaction @%x\n", this));
        return NS_ERROR_NET_RESET;
    }

    LOG(("restarting transaction @%x\n", this));

    // rewind streams in case we already wrote out the request
    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mRequestStream);
    if (seekable)
        seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);

    // clear the old socket info
    mSecurityInfo = 0;

    return gHttpHandler->InitiateTransaction(this);
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

nsresult
nsHttpTransaction::ParseLineSegment(char *segment, PRUint32 len)
{
    NS_PRECONDITION(!mHaveAllHeaders, "already have all headers");

    if (!mLineBuf.IsEmpty() && mLineBuf.Last() == '\n') {
        // trim off the new line char, and if this segment is
        // not a continuation of the previous or if we haven't
        // parsed the status line yet, then parse the contents
        // of mLineBuf.
        mLineBuf.Truncate(mLineBuf.Length() - 1);
        if (!mHaveStatusLine || (*segment != ' ' && *segment != '\t')) {
            ParseLine(NS_CONST_CAST(char*,mLineBuf.get()));
            mLineBuf.Truncate();
        }
    }

    // append segment to mLineBuf...
    if (mLineBuf.Length() + len > MAX_LINEBUF_LENGTH) {
        LOG(("excessively long header received, canceling transaction [trans=%x]", this));
        return NS_ERROR_ABORT;
    }
    mLineBuf.Append(segment, len);
    
    // a line buf with only a new line char signifies the end of headers.
    if (mLineBuf.First() == '\n') {
        mLineBuf.Truncate();
        // discard this response if it is a 100 continue.
        if (mResponseHead->Status() == 100) {
            LOG(("ignoring 100 response\n"));
            mHaveStatusLine = PR_FALSE;
            mResponseHead->Reset();
            return NS_OK;
        }
        mHaveAllHeaders = PR_TRUE;
    }
    return NS_OK;
}

nsresult
nsHttpTransaction::ParseHead(char *buf,
                             PRUint32 count,
                             PRUint32 *countRead)
{
    nsresult rv;
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
        char *p = LocateHttpStart(buf, PR_MIN(count, 8));
        if (!p) {
            mResponseHead->ParseStatusLine("");
            mHaveStatusLine = PR_TRUE;
            mHaveAllHeaders = PR_TRUE;
            return NS_OK;
        }
        if (p > buf) {
            // skip over the junk
            *countRead = p - buf;
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
        rv = ParseLineSegment(buf, len);
        if (NS_FAILED(rv))
            return rv;

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
        rv = ParseLineSegment(buf, len);
        if (NS_FAILED(rv))
            return rv;
    }
    return NS_OK;
}

// called on the socket thread
nsresult
nsHttpTransaction::HandleContentStart()
{
    LOG(("nsHttpTransaction::HandleContentStart [this=%x]\n", this));

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
        mConnection->OnHeadersAvailable(this, mRequestHead, mResponseHead, &reset);

        // looks like we should ignore this response, resetting...
        if (reset) {
            LOG(("resetting transaction's response head\n"));
            mHaveAllHeaders = PR_FALSE;
            mHaveStatusLine = PR_FALSE;
            mResponseHead->Reset();
            // wait to be called again...
            return NS_OK;
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
                // Ignore server specified Content-Length.
                mContentLength = -1;
            }
#if defined(PR_LOGGING)
            else if (mContentLength == -1)
                LOG(("waiting for the server to close the connection.\n"));
#endif
        }
    }

    mDidContentStart = PR_TRUE;
    return NS_OK;
}

// called on the socket thread
nsresult
nsHttpTransaction::HandleContent(char *buf,
                                 PRUint32 count,
                                 PRUint32 *contentRead,
                                 PRUint32 *contentRemaining)
{
    nsresult rv;

    LOG(("nsHttpTransaction::HandleContent [this=%x count=%u]\n", this, count));

    *contentRead = 0;
    *contentRemaining = 0;

    NS_ASSERTION(mConnection, "no connection");

    if (!mDidContentStart) {
        rv = HandleContentStart();
        if (NS_FAILED(rv)) return rv;
    }

    if (mChunkedDecoder) {
        // give the buf over to the chunked decoder so it can reformat the
        // data and tell us how much is really there.
        rv = mChunkedDecoder->HandleChunkedContent(buf, count, contentRead, contentRemaining);
        if (NS_FAILED(rv)) return rv;
    }
    else if (mContentLength >= 0) {
        // HTTP/1.0 servers have been known to send erroneous Content-Length
        // headers. So, unless the connection is persistent, we must make
        // allowances for a possibly invalid Content-Length header. Thus, if
        // NOT persistent, we simply accept everything in |buf|.
        if (mConnection->IsPersistent()) {
            *contentRead = PRUint32(mContentLength) - mContentRead;
            *contentRead = PR_MIN(count, *contentRead);
        }
        else {
            *contentRead = count;
            // mContentLength might need to be increased...
            if (*contentRead + mContentRead > (PRUint32) mContentLength) {
                mContentLength = *contentRead + mContentRead;
                //mResponseHead->SetContentLength(mContentLength);
            }
        }
        *contentRemaining = (count - *contentRead);
    }
    else {
        // when we are just waiting for the server to close the connection...
        *contentRead = count;
    }

    if (*contentRead) {
        // update count of content bytes read and report progress...
        mContentRead += *contentRead;
        /*
        if (mProgressSink)
            mProgressSink->OnProgress(nsnull, nsnull, mContentRead, PR_MAX(0, mContentLength));
        */
    }

    LOG(("nsHttpTransaction [this=%x count=%u read=%u mContentRead=%u mContentLength=%d]\n",
        this, count, *contentRead, mContentRead, mContentLength));

    // check for end-of-file
    if ((mContentRead == PRUint32(mContentLength)) ||
        (mChunkedDecoder && mChunkedDecoder->ReachedEOF())) {
        // the transaction is done with a complete response.
        mTransactionDone = PR_TRUE;
        mResponseIsComplete = PR_TRUE;
    }

    return NS_OK;
}

nsresult
nsHttpTransaction::ProcessData(char *buf, PRUint32 count, PRUint32 *countRead)
{
    nsresult rv;

    LOG(("nsHttpTransaction::ProcessData [this=%x count=%u]\n", this, count));

    *countRead = 0;

    // we may not have read all of the headers yet...
    if (!mHaveAllHeaders) {
        PRUint32 bytesConsumed = 0;

        rv = ParseHead(buf, count, &bytesConsumed);
        if (NS_FAILED(rv)) return rv;

        count -= bytesConsumed;

        // if buf has some content in it, shift bytes to top of buf.
        if (count && bytesConsumed)
            memmove(buf, buf + bytesConsumed, count);
    }

    // even though count may be 0, we still want to call HandleContent
    // so it can complete the transaction if this is a "no-content" response.
    if (mHaveAllHeaders) {
        PRUint32 countRemaining = 0;
        //
        // buf layout:
        // 
        // +--------------------------------------+----------------+-----+
        // |              countRead               | countRemaining |     |
        // +--------------------------------------+----------------+-----+
        //
        // count          : bytes read from the socket
        // countRead      : bytes corresponding to this transaction
        // countRemaining : bytes corresponding to next pipelined transaction
        //
        // NOTE:
        // count > countRead + countRemaining <==> chunked transfer encoding
        //
        rv = HandleContent(buf, count, countRead, &countRemaining);
        if (NS_FAILED(rv)) return rv;
        // we may have read more than our share, in which case we must give
        // the excess bytes back to the connection
        if (mResponseIsComplete && countRemaining) {
            NS_ASSERTION(mConnection, "no connection");
            mConnection->PushBack(buf + *countRead, countRemaining);
        }
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction events
//-----------------------------------------------------------------------------

void *PR_CALLBACK
nsHttpTransaction::TransportStatus_Handler(PLEvent *ev)
{
    nsHttpTransaction *trans =
            NS_STATIC_CAST(nsHttpTransaction *, PL_GetEventOwner(ev));

    LOG(("nsHttpTransaction::SocketStatus_Handler [trans=%x]\n", trans));

    nsresult status;
    PRUint32 progress, progressMax;
    {
        nsAutoLock lock(trans->mLock);

        status = trans->mTransportStatus;
        progress = trans->mTransportProgress;
        progressMax = trans->mTransportProgressMax;

        trans->mTransportStatusInProgress = PR_FALSE;
    }

    trans->mTransportSink->OnTransportStatus(nsnull, status, progress, progressMax);

    NS_RELEASE(trans);
    return nsnull;
}

void PR_CALLBACK
nsHttpTransaction::TransportStatus_Cleanup(PLEvent *ev)
{
    delete ev;
}

//-----------------------------------------------------------------------------

void
nsHttpTransaction::DeleteSelfOnConsumerThread()
{
    nsCOMPtr<nsIEventQueueService> eqs;
    nsCOMPtr<nsIEventQueue> currentEventQ;

    LOG(("nsHttpTransaction::DeleteSelfOnConsumerThread [this=%x]\n", this));
    
    NS_ASSERTION(!mDestroying, "deleting self again");
    mDestroying = PR_TRUE;

    gHttpHandler->GetEventQueueService(getter_AddRefs(eqs));
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

        PL_InitEvent(event, this, DeleteThis_Handler, DeleteThis_Cleanup);

        PRStatus status = mConsumerEventQ->PostEvent(event);
        NS_ASSERTION(status == PR_SUCCESS, "PostEvent failed");
    }
}

void *PR_CALLBACK
nsHttpTransaction::DeleteThis_Handler(PLEvent *ev)
{
    nsHttpTransaction *trans =
            NS_STATIC_CAST(nsHttpTransaction *, PL_GetEventOwner(ev));

    LOG(("nsHttpTransaction::DeleteThis_EventHandlerFunc [trans=%x]\n", trans));

    delete trans;
    return nsnull;
}

void PR_CALLBACK
nsHttpTransaction::DeleteThis_Cleanup(PLEvent *ev)
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
                                    nsIOutputStreamNotify,
                                    nsISocketEventHandler)

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsIOutputStreamNotify
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpTransaction::OnOutputStreamReady(nsIAsyncOutputStream *out)
{
    // proxy this event to the socket thread

    nsCOMPtr<nsISocketTransportService> sts;
    gHttpHandler->ConnMgr()->GetSTS(getter_AddRefs(sts));
    if (sts)
        sts->PostEvent(this, 0, 0, nsnull); // only one type of event so far

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsISocketEventHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpTransaction::OnSocketEvent(PRUint32 type, PRUint32 param1, void *param2)
{
    if (mConnection) {
        nsresult rv = mConnection->ResumeRecv();
        NS_ASSERTION(NS_SUCCEEDED(rv), "ResumeSend failed");
    }
    return NS_OK;
}
