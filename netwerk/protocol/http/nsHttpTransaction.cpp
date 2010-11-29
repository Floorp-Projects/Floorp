/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
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
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *   Andreas M. Schneider <clarence@clarence.de>
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

#ifdef MOZ_IPC
#include "base/basictypes.h"
#endif 

#include "nsIOService.h"
#include "nsHttpHandler.h"
#include "nsHttpTransaction.h"
#include "nsHttpConnection.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsHttpChunkedDecoder.h"
#include "nsTransportUtils.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsIOService.h"
#include "nsAutoLock.h"
#include "pratom.h"

#include "nsISeekableStream.h"
#include "nsISocketTransport.h"
#include "nsMultiplexInputStream.h"
#include "nsStringStream.h"

#include "nsComponentManagerUtils.h" // do_CreateInstance
#include "nsServiceManagerUtils.h"   // do_GetService
#include "nsIHttpActivityObserver.h"

#include "mozilla/FunctionTimer.h"

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
        LOG3(("  %s\n", buf.get()));
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
    , mPriority(0)
    , mRestartCount(0)
    , mCaps(0)
    , mClosed(PR_FALSE)
    , mConnected(PR_FALSE)
    , mHaveStatusLine(PR_FALSE)
    , mHaveAllHeaders(PR_FALSE)
    , mTransactionDone(PR_FALSE)
    , mResponseIsComplete(PR_FALSE)
    , mDidContentStart(PR_FALSE)
    , mNoContent(PR_FALSE)
    , mSentData(PR_FALSE)
    , mReceivedData(PR_FALSE)
    , mStatusEventPending(PR_FALSE)
    , mHasRequestBody(PR_FALSE)
    , mSSLConnectFailed(PR_FALSE)
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
}

nsresult
nsHttpTransaction::Init(PRUint8 caps,
                        nsHttpConnectionInfo *cinfo,
                        nsHttpRequestHead *requestHead,
                        nsIInputStream *requestBody,
                        PRBool requestBodyHasHeaders,
                        nsIEventTarget *target,
                        nsIInterfaceRequestor *callbacks,
                        nsITransportEventSink *eventsink,
                        nsIAsyncInputStream **responseBody)
{
    NS_TIME_FUNCTION;

    nsresult rv;

    LOG(("nsHttpTransaction::Init [this=%x caps=%x]\n", this, caps));

    NS_ASSERTION(cinfo, "ouch");
    NS_ASSERTION(requestHead, "ouch");
    NS_ASSERTION(target, "ouch");

    mActivityDistributor = do_GetService(NS_HTTPACTIVITYDISTRIBUTOR_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    PRBool activityDistributorActive;
    rv = mActivityDistributor->GetIsActive(&activityDistributorActive);
    if (NS_SUCCEEDED(rv) && activityDistributorActive) {
        // there are some observers registered at activity distributor, gather
        // nsISupports for the channel that called Init()
        mChannel = do_QueryInterface(eventsink);
        LOG(("nsHttpTransaction::Init() " \
             "mActivityDistributor is active " \
             "this=%x", this));
    } else {
        // there is no observer, so don't use it
        activityDistributorActive = PR_FALSE;
        mActivityDistributor = nsnull;
    }

    // create transport event sink proxy. it coalesces all events if and only 
    // if the activity observer is not active. when the observer is active
    // we need not to coalesce any events to get all expected notifications
    // of the transaction state, necessary for correct debugging and logging.
    rv = net_NewTransportEventSinkProxy(getter_AddRefs(mTransportSink),
                                        eventsink, target,
                                        !activityDistributorActive);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(mConnInfo = cinfo);
    mCallbacks = callbacks;
    mConsumerTarget = target;
    mCaps = caps;

    if (requestHead->Method() == nsHttp::Head)
        mNoContent = PR_TRUE;

    // Make sure that there is "Content-Length: 0" header in the requestHead
    // in case of POST and PUT methods when there is no requestBody and
    // requestHead doesn't contain "Transfer-Encoding" header.
    //
    // RFC1945 section 7.2.2:
    //   HTTP/1.0 requests containing an entity body must include a valid
    //   Content-Length header field.
    //
    // RFC2616 section 4.4:
    //   For compatibility with HTTP/1.0 applications, HTTP/1.1 requests
    //   containing a message-body MUST include a valid Content-Length header
    //   field unless the server is known to be HTTP/1.1 compliant.
    if ((requestHead->Method() == nsHttp::Post || requestHead->Method() == nsHttp::Put) &&
        !requestBody && !requestHead->PeekHeader(nsHttp::Transfer_Encoding)) {
        requestHead->SetHeader(nsHttp::Content_Length, NS_LITERAL_CSTRING("0"));
    }

    // grab a weak reference to the request head
    mRequestHead = requestHead;

    // make sure we eliminate any proxy specific headers from 
    // the request if we are talking HTTPS via a SSL tunnel.
    PRBool pruneProxyHeaders = cinfo->UsingSSL() &&
                               cinfo->UsingHttpProxy();
    mReqHeaderBuf.Truncate();
    requestHead->Flatten(mReqHeaderBuf, pruneProxyHeaders);

#if defined(PR_LOGGING)
    if (LOG3_ENABLED()) {
        LOG3(("http request [\n"));
        LogHeaders(mReqHeaderBuf.get());
        LOG3(("]\n"));
    }
#endif

    // If the request body does not include headers or if there is no request
    // body, then we must add the header/body separator manually.
    if (!requestBodyHasHeaders || !requestBody)
        mReqHeaderBuf.AppendLiteral("\r\n");

    // report the request header
    if (mActivityDistributor)
        mActivityDistributor->ObserveActivity(
            mChannel,
            NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
            NS_HTTP_ACTIVITY_SUBTYPE_REQUEST_HEADER,
            PR_Now(), LL_ZERO,
            mReqHeaderBuf);

    // Create a string stream for the request header buf (the stream holds
    // a non-owning reference to the request header data, so we MUST keep
    // mReqHeaderBuf around).
    nsCOMPtr<nsIInputStream> headers;
    rv = NS_NewByteInputStream(getter_AddRefs(headers),
                               mReqHeaderBuf.get(),
                               mReqHeaderBuf.Length());
    if (NS_FAILED(rv)) return rv;

    if (requestBody) {
        mHasRequestBody = PR_TRUE;

        // wrap the headers and request body in a multiplexed input stream.
        nsCOMPtr<nsIMultiplexInputStream> multi =
            do_CreateInstance(kMultiplexInputStream, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = multi->AppendStream(headers);
        if (NS_FAILED(rv)) return rv;

        rv = multi->AppendStream(requestBody);
        if (NS_FAILED(rv)) return rv;

        // wrap the multiplexed input stream with a buffered input stream, so
        // that we write data in the largest chunks possible.  this is actually
        // necessary to workaround some common server bugs (see bug 137155).
        rv = NS_NewBufferedInputStream(getter_AddRefs(mRequestStream), multi,
                                       nsIOService::gDefaultSegmentSize);
        if (NS_FAILED(rv)) return rv;
    }
    else
        mRequestStream = headers;

    rv = mRequestStream->Available(&mRequestSize);
    if (NS_FAILED(rv)) return rv;

    // create pipe for response stream
    rv = NS_NewPipe2(getter_AddRefs(mPipeIn),
                     getter_AddRefs(mPipeOut),
                     PR_TRUE, PR_TRUE,
                     nsIOService::gDefaultSegmentSize,
                     nsIOService::gDefaultSegmentCount,
                     nsIOService::gBufferCache);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*responseBody = mPipeIn);
    return NS_OK;
}

nsHttpResponseHead *
nsHttpTransaction::TakeResponseHead()
{
    if (!mHaveAllHeaders) {
        NS_WARNING("response headers not available or incomplete");
        return nsnull;
    }

    nsHttpResponseHead *head = mResponseHead;
    mResponseHead = nsnull;
    return head;
}

//----------------------------------------------------------------------------
// nsHttpTransaction::nsAHttpTransaction
//----------------------------------------------------------------------------

void
nsHttpTransaction::SetConnection(nsAHttpConnection *conn)
{
    NS_IF_RELEASE(mConnection);
    NS_IF_ADDREF(mConnection = conn);
}

void
nsHttpTransaction::GetSecurityCallbacks(nsIInterfaceRequestor **cb)
{
    NS_IF_ADDREF(*cb = mCallbacks);
}

void
nsHttpTransaction::OnTransportStatus(nsresult status, PRUint64 progress)
{
    LOG(("nsHttpTransaction::OnSocketStatus [this=%x status=%x progress=%llu]\n",
        this, status, progress));

    if (TimingEnabled()) {
        if (status == nsISocketTransport::STATUS_RESOLVING) {
            mTimings.domainLookupStart = PR_Now();
        } else if (status == nsISocketTransport::STATUS_RESOLVED) {
            mTimings.domainLookupEnd = PR_Now();
        } else if (status == nsISocketTransport::STATUS_CONNECTING_TO) {
            mTimings.connectStart = PR_Now();
        } else if (status == nsISocketTransport::STATUS_CONNECTED_TO) {
            mTimings.connectEnd = PR_Now();
        }
    }

    if (!mTransportSink)
        return;
    
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    // Need to do this before the STATUS_RECEIVING_FROM check below, to make
    // sure that the activity distributor gets told about all status events.
    if (mActivityDistributor) {
        // upon STATUS_WAITING_FOR; report request body sent
        if ((mHasRequestBody) &&
            (status == nsISocketTransport::STATUS_WAITING_FOR))
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_REQUEST_BODY_SENT,
                PR_Now(), LL_ZERO, EmptyCString());

        // report the status and progress
        mActivityDistributor->ObserveActivity(
            mChannel,
            NS_HTTP_ACTIVITY_TYPE_SOCKET_TRANSPORT,
            static_cast<PRUint32>(status),
            PR_Now(),
            progress,
            EmptyCString());
    }

    // nsHttpChannel synthesizes progress events in OnDataAvailable
    if (status == nsISocketTransport::STATUS_RECEIVING_FROM)
        return;

    PRUint64 progressMax;

    if (status == nsISocketTransport::STATUS_SENDING_TO) {
        // suppress progress when only writing request headers
        if (!mHasRequestBody)
            return;

        nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mRequestStream);
        NS_ASSERTION(seekable, "Request stream isn't seekable?!?");

        PRInt64 prog = 0;
        seekable->Tell(&prog);
        progress = prog;

        // when uploading, we include the request headers in the progress
        // notifications.
        progressMax = mRequestSize; // XXX mRequestSize is 32-bit!
    }
    else {
        progress = LL_ZERO;
        progressMax = 0;
    }

    mTransportSink->OnTransportStatus(nsnull, status, progress, progressMax);
}

PRBool
nsHttpTransaction::IsDone()
{
    return mTransactionDone;
}

nsresult
nsHttpTransaction::Status()
{
    return mStatus;
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
    nsresult rv = trans->mReader->OnReadSegment(buf, count, countRead);
    if (NS_FAILED(rv)) return rv;

    if (trans->TimingEnabled() && !trans->mTimings.requestStart) {
        // First data we're sending -> this is requestStart
        trans->mTimings.requestStart = PR_Now();
    }
    trans->mSentData = PR_TRUE;
    return NS_OK;
}

nsresult
nsHttpTransaction::ReadSegments(nsAHttpSegmentReader *reader,
                                PRUint32 count, PRUint32 *countRead)
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (mTransactionDone) {
        *countRead = 0;
        return mStatus;
    }

    if (!mConnected) {
        mConnected = PR_TRUE;
        mConnection->GetSecurityInfo(getter_AddRefs(mSecurityInfo));
    }

    mReader = reader;

    nsresult rv = mRequestStream->ReadSegments(ReadRequestSegment, this, count, countRead);

    mReader = nsnull;

    // if read would block then we need to AsyncWait on the request stream.
    // have callback occur on socket thread so we stay synchronized.
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        nsCOMPtr<nsIAsyncInputStream> asyncIn =
                do_QueryInterface(mRequestStream);
        if (asyncIn) {
            nsCOMPtr<nsIEventTarget> target;
            gHttpHandler->GetSocketThreadTarget(getter_AddRefs(target));
            if (target)
                asyncIn->AsyncWait(this, 0, 0, target);
            else {
                NS_ERROR("no socket thread event target");
                rv = NS_ERROR_UNEXPECTED;
            }
        }
    }

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
        return NS_BASE_STREAM_CLOSED; // stop iterating

    if (trans->TimingEnabled() && !trans->mTimings.responseStart) {
        trans->mTimings.responseStart = PR_Now();
    }

    nsresult rv;
    //
    // OK, now let the caller fill this segment with data.
    //
    rv = trans->mWriter->OnWriteSegment(buf, count, countWritten);
    if (NS_FAILED(rv)) return rv; // caller didn't want to write anything

    NS_ASSERTION(*countWritten > 0, "bad writer");
    trans->mReceivedData = PR_TRUE;

    // now let the transaction "play" with the buffer.  it is free to modify
    // the contents of the buffer and/or modify countWritten.
    rv = trans->ProcessData(buf, *countWritten, countWritten);
    if (NS_FAILED(rv))
        trans->Close(rv);

    return rv; // failure code only stops WriteSegments; it is not propagated.
}

nsresult
nsHttpTransaction::WriteSegments(nsAHttpSegmentWriter *writer,
                                 PRUint32 count, PRUint32 *countWritten)
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (mTransactionDone)
        return NS_SUCCEEDED(mStatus) ? NS_BASE_STREAM_CLOSED : mStatus;

    mWriter = writer;

    nsresult rv = mPipeOut->WriteSegments(WritePipeSegment, this, count, countWritten);

    mWriter = nsnull;

    // if pipe would block then we need to AsyncWait on it.  have callback
    // occur on socket thread so we stay synchronized.
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        nsCOMPtr<nsIEventTarget> target;
        gHttpHandler->GetSocketThreadTarget(getter_AddRefs(target));
        if (target)
            mPipeOut->AsyncWait(this, 0, 0, target);
        else {
            NS_ERROR("no socket thread event target");
            rv = NS_ERROR_UNEXPECTED;
        }
    }

    return rv;
}

void
nsHttpTransaction::Close(nsresult reason)
{
    LOG(("nsHttpTransaction::Close [this=%x reason=%x]\n", this, reason));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (mClosed) {
        LOG(("  already closed\n"));
        return;
    }

    mTimings.responseEnd = PR_Now();

    if (mActivityDistributor) {
        // report the reponse is complete if not already reported
        if (!mResponseIsComplete)
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_COMPLETE,
                PR_Now(),
                static_cast<PRUint64>(mContentRead.mValue),
                EmptyCString());

        // report that this transaction is closing
        mActivityDistributor->ObserveActivity(
            mChannel,
            NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
            NS_HTTP_ACTIVITY_SUBTYPE_TRANSACTION_CLOSE,
            PR_Now(), LL_ZERO, EmptyCString());
    }

    // we must no longer reference the connection!  find out if the 
    // connection was being reused before letting it go.
    PRBool connReused = PR_FALSE;
    if (mConnection)
        connReused = mConnection->IsReused();
    mConnected = PR_FALSE;

    //
    // if the connection was reset or closed before we wrote any part of the
    // request or if we wrote the request but didn't receive any part of the
    // response and the connection was being reused, then we can (and really
    // should) assume that we wrote to a stale connection and we must therefore
    // repeat the request over a new connection.
    //
    // NOTE: the conditions under which we will automatically retry the HTTP
    // request have to be carefully selected to avoid duplication of the
    // request from the point-of-view of the server.  such duplication could
    // have dire consequences including repeated purchases, etc.
    //
    // NOTE: because of the way SSL proxy CONNECT is implemented, it is
    // possible that the transaction may have received data without having
    // sent any data.  for this reason, mSendData == FALSE does not imply
    // mReceivedData == FALSE.  (see bug 203057 for more info.)
    //
    if (reason == NS_ERROR_NET_RESET || reason == NS_OK) {
        if (!mReceivedData && (!mSentData || connReused)) {
            // if restarting fails, then we must proceed to close the pipe,
            // which will notify the channel that the transaction failed.
            if (NS_SUCCEEDED(Restart()))
                return;
        }
    }

    PRBool relConn = PR_TRUE;
    if (NS_SUCCEEDED(reason)) {
        // the server has not sent the final \r\n terminating the header
        // section, and there may still be a header line unparsed.  let's make
        // sure we parse the remaining header line, and then hopefully, the
        // response will be usable (see bug 88792).  related to that, we may
        // also have an empty response containing no headers.  we should treat
        // that as an empty HTTP/0.9 response (see bug 300613).
        if (!mHaveAllHeaders) {
            char data = '\n';
            PRUint32 unused;
            ParseHead(&data, 1, &unused);
        }

        // honor the sticky connection flag...
        if (mCaps & NS_HTTP_STICKY_CONNECTION)
            relConn = PR_FALSE;
    }
    if (relConn && mConnection)
        NS_RELEASE(mConnection);

    mStatus = reason;
    mTransactionDone = PR_TRUE; // forcibly flag the transaction as complete
    mClosed = PR_TRUE;

    // release some resources that we no longer need
    mRequestStream = nsnull;
    mReqHeaderBuf.Truncate();
    mLineBuf.Truncate();
    if (mChunkedDecoder) {
        delete mChunkedDecoder;
        mChunkedDecoder = nsnull;
    }

    // closing this pipe triggers the channel's OnStopRequest method.
    mPipeOut->CloseWithStatus(reason);
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

    // clear old connection state...
    mSecurityInfo = 0;
    NS_IF_RELEASE(mConnection);

    // disable pipelining for the next attempt in case pipelining caused the
    // reset.  this is being overly cautious since we don't know if pipelining
    // was the problem here.
    mCaps &= ~NS_HTTP_ALLOW_PIPELINING;

    return gHttpHandler->InitiateTransaction(this, mPriority);
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
            ParseLine(mLineBuf.BeginWriting());
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
        // discard this response if it is a 100 continue or other 1xx status.
        if (mResponseHead->Status() / 100 == 1) {
            LOG(("ignoring 1xx response\n"));
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

        // report that we have a least some of the response
        if (mActivityDistributor)
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_START,
                PR_Now(), LL_ZERO, EmptyCString());
    }

    // if we don't have a status line and the line buf is empty, then
    // this must be the first time we've been called.
    if (!mHaveStatusLine && mLineBuf.IsEmpty()) {
        // tolerate some junk before the status line
        char *p = LocateHttpStart(buf, PR_MIN(count, 8));
        if (!p) {
            // Treat any 0.9 style response of a put as a failure.
            if (mRequestHead->Method() == nsHttp::Put)
                return NS_ERROR_ABORT;

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

    while ((eol = static_cast<char *>(memchr(buf, '\n', count - *countRead))) != nsnull) {
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
        if (LOG3_ENABLED()) {
            LOG3(("http response [\n"));
            nsCAutoString headers;
            mResponseHead->Flatten(headers, PR_FALSE);
            LogHeaders(headers.get());
            LOG3(("]\n"));
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
            mReceivedData = PR_FALSE;
            mSentData = PR_FALSE;
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
            // ignore chunked responses from HTTP/1.0 servers and proxies.
            if (mResponseHead->Version() >= NS_HTTP_VERSION_1_1 &&
                mResponseHead->HasHeaderValue(nsHttp::Transfer_Encoding, "chunked")) {
                // we only support the "chunked" transfer encoding right now.
                mChunkedDecoder = new nsHttpChunkedDecoder();
                if (!mChunkedDecoder)
                    return NS_ERROR_OUT_OF_MEMORY;
                LOG(("chunked decoder created\n"));
                // Ignore server specified Content-Length.
                mContentLength = -1;
            }
#if defined(PR_LOGGING)
            else if (mContentLength == nsInt64(-1))
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
        // Do not write content to the pipe if we haven't started streaming yet
        if (!mDidContentStart)
            return NS_OK;
    }

    if (mChunkedDecoder) {
        // give the buf over to the chunked decoder so it can reformat the
        // data and tell us how much is really there.
        rv = mChunkedDecoder->HandleChunkedContent(buf, count, contentRead, contentRemaining);
        if (NS_FAILED(rv)) return rv;
    }
    else if (mContentLength >= nsInt64(0)) {
        // HTTP/1.0 servers have been known to send erroneous Content-Length
        // headers. So, unless the connection is persistent, we must make
        // allowances for a possibly invalid Content-Length header. Thus, if
        // NOT persistent, we simply accept everything in |buf|.
        if (mConnection->IsPersistent()) {
            nsInt64 remaining = mContentLength - mContentRead;
            nsInt64 count64 = count;
            *contentRead = PR_MIN(count64, remaining);
            *contentRemaining = count - *contentRead;
        }
        else {
            *contentRead = count;
            // mContentLength might need to be increased...
            nsInt64 position = mContentRead + nsInt64(count);
            if (position > mContentLength) {
                mContentLength = position;
                //mResponseHead->SetContentLength(mContentLength);
            }
        }
    }
    else {
        // when we are just waiting for the server to close the connection...
        // (no explicit content-length given)
        *contentRead = count;
    }

    if (*contentRead) {
        // update count of content bytes read and report progress...
        mContentRead += *contentRead;
        /* when uncommenting, take care of 64-bit integers w/ PR_MAX...
        if (mProgressSink)
            mProgressSink->OnProgress(nsnull, nsnull, mContentRead, PR_MAX(0, mContentLength));
        */
    }

    LOG(("nsHttpTransaction::HandleContent [this=%x count=%u read=%u mContentRead=%lld mContentLength=%lld]\n",
        this, count, *contentRead, mContentRead.mValue, mContentLength.mValue));

    // check for end-of-file
    if ((mContentRead == mContentLength) ||
        (mChunkedDecoder && mChunkedDecoder->ReachedEOF())) {
        // the transaction is done with a complete response.
        mTransactionDone = PR_TRUE;
        mResponseIsComplete = PR_TRUE;

        // report the entire response has arrived
        if (mActivityDistributor)
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_COMPLETE,
                PR_Now(),
                static_cast<PRUint64>(mContentRead.mValue),
                EmptyCString());
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

        // report the completed response header
        if (mActivityDistributor && mResponseHead && mHaveAllHeaders) {
            nsCAutoString completeResponseHeaders;
            mResponseHead->Flatten(completeResponseHeaders, PR_FALSE);
            completeResponseHeaders.AppendLiteral("\r\n");
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_HEADER,
                PR_Now(), LL_ZERO,
                completeResponseHeaders);
        }
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
// nsHttpTransaction deletion event
//-----------------------------------------------------------------------------

class nsDeleteHttpTransaction : public nsRunnable {
public:
    nsDeleteHttpTransaction(nsHttpTransaction *trans)
        : mTrans(trans)
    {}

    NS_IMETHOD Run()
    {
        delete mTrans;
        return NS_OK;
    }
private:
    nsHttpTransaction *mTrans;
};

void
nsHttpTransaction::DeleteSelfOnConsumerThread()
{
    LOG(("nsHttpTransaction::DeleteSelfOnConsumerThread [this=%x]\n", this));
    
    PRBool val;
    if (NS_SUCCEEDED(mConsumerTarget->IsOnCurrentThread(&val)) && val)
        delete this;
    else {
        LOG(("proxying delete to consumer thread...\n"));
        nsCOMPtr<nsIRunnable> event = new nsDeleteHttpTransaction(this);
        if (NS_FAILED(mConsumerTarget->Dispatch(event, NS_DISPATCH_NORMAL)))
            NS_WARNING("failed to dispatch nsHttpDeleteTransaction event");
    }
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
                                    nsIInputStreamCallback,
                                    nsIOutputStreamCallback)

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsIInputStreamCallback
//-----------------------------------------------------------------------------

// called on the socket thread
NS_IMETHODIMP
nsHttpTransaction::OnInputStreamReady(nsIAsyncInputStream *out)
{
    if (mConnection) {
        nsresult rv = mConnection->ResumeSend();
        if (NS_FAILED(rv))
            NS_ERROR("ResumeSend failed");
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsIOutputStreamCallback
//-----------------------------------------------------------------------------

// called on the socket thread
NS_IMETHODIMP
nsHttpTransaction::OnOutputStreamReady(nsIAsyncOutputStream *out)
{
    if (mConnection) {
        nsresult rv = mConnection->ResumeRecv();
        if (NS_FAILED(rv))
            NS_ERROR("ResumeRecv failed");
    }
    return NS_OK;
}
