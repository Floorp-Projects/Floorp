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
 */

#include "nsHttpHandler.h"
#include "nsHttpTransaction.h"
#include "nsHttpConnection.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsHttpChunkedDecoder.h"
#include "nsIStringStream.h"
#include "pratom.h"

//-----------------------------------------------------------------------------
// nsHttpTransaction
//-----------------------------------------------------------------------------

nsHttpTransaction::nsHttpTransaction(nsIStreamListener *listener,
                                     nsIInterfaceRequestor *callbacks)
    : mListener(listener)
    , mCallbacks(callbacks)
    , mConnection(nsnull)
    , mResponseHead(nsnull)
    , mContentLength(-1)
    , mContentRead(0)
    , mChunkedDecoder(nsnull)
    , mTransactionDone(0)
    , mStatus(NS_OK)
    , mHaveStatusLine(PR_FALSE)
    , mHaveAllHeaders(PR_FALSE)
    , mFiredOnStart(PR_FALSE)
    , mNoContent(PR_FALSE)
{
    LOG(("Creating nsHttpTransaction @%x\n", this));

    NS_INIT_ISUPPORTS();

    NS_PRECONDITION(listener, "null listener");
}

nsHttpTransaction::~nsHttpTransaction()
{
    LOG(("Destroying nsHttpTransaction @%x\n", this));

    if (mConnection) {
        nsHttpHandler::get()->ReclaimConnection(mConnection);
        NS_RELEASE(mConnection);
    }

    if (mChunkedDecoder)
        delete mChunkedDecoder;
}

nsresult
nsHttpTransaction::SetupRequest(nsHttpRequestHead *requestHead,
                                nsIInputStream *requestStream)
{
    nsresult rv;

    LOG(("nsHttpTransaction::SetupRequest [this=%x]\n", this));

    NS_ENSURE_ARG_POINTER(requestHead);

    if (requestHead->Method() == nsHttp::Head)
        mNoContent = PR_TRUE;

    mReqHeaderBuf.SetLength(0);
    requestHead->Flatten(mReqHeaderBuf);

    LOG2(("http request [\n%s]\n", mReqHeaderBuf.get()));

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
    NS_ENSURE_ARG_POINTER(conn);
    NS_ENSURE_TRUE(!mConnection, NS_ERROR_ALREADY_INITIALIZED);

    mConnection = conn;
    NS_ADDREF(mConnection);

    return NS_OK;
}

nsHttpResponseHead *
nsHttpTransaction::TakeResponseHead()
{
    nsHttpResponseHead *head = mResponseHead;
    mResponseHead = nsnull;
    return head;
}

nsresult
nsHttpTransaction::GetSecurityInfo(nsISupports **result)
{
    if (mConnection)
        mConnection->GetSecurityInfo(result);
    return NS_OK;
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
    return rv;
}

// called on the socket transport thread
nsresult
nsHttpTransaction::OnStopTransaction(nsresult status)
{
    LOG(("nsHttpTransaction::OnStopTransaction [this=%x status=%x]\n",
        this, status));

    // make sure that this flag is set.
    mTransactionDone = PR_TRUE;
    mStatus = status;

	if (mListener) {
		if (!mFiredOnStart) {
			mFiredOnStart = PR_TRUE;
			mListener->OnStartRequest(this, nsnull); 
		}
		mListener->OnStopRequest(this, nsnull, status);
        mListener = 0;
	}
    return NS_OK;
}

nsresult
nsHttpTransaction::ParseLine(char *line)
{
    NS_PRECONDITION(!mHaveAllHeaders, "already have all headers");

    LOG(("nsHttpTransaction::ParseLine [%s]\n", line));
    
    // allocate the response head object if necessary
    if (!mResponseHead) {
        mResponseHead = new nsHttpResponseHead();
        if (!mResponseHead)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!mHaveStatusLine) {
        mResponseHead->ParseStatusLine(line);
        mHaveStatusLine = PR_TRUE;
    }
    else if (*line == '\0')
        mHaveAllHeaders = PR_TRUE;
    else
        mResponseHead->ParseHeaderLine(line);
    return NS_OK;
}

nsresult
nsHttpTransaction::ParseHead(char *buf,
                             PRUint32 count,
                             PRUint32 *countRead)
{
    char *eol;

    LOG(("nsHttpTransaction::ParseHead [count=%u]\n", count));

    *countRead = 0;

    NS_PRECONDITION(!mHaveAllHeaders, "oops");

    while ((eol = PL_strnchr(buf, '\n', count - *countRead)) != nsnull) {
        // found line in range [buf:eol]
        *eol = 0;

        // actually, in range [buf:eol-1]
        if ((eol > buf) && (*(eol-1) == '\r'))
            *(eol-1) = 0;

        // we may have a partial line to complete...
        if (!mLineBuf.IsEmpty()) {
            mLineBuf.Append(buf);
            ParseLine((char *) mLineBuf.get());
            mLineBuf.SetLength(0);
        }
        else
            ParseLine(buf);

        *countRead += (eol + 1 - buf);
        NS_ASSERTION(*countRead <= count, "oops");

        // skip over line
        buf = eol + 1;

        if (mHaveAllHeaders)
            break;
    }

    if (!mHaveAllHeaders && (count > *countRead)) {
        // remember this partial line
        mLineBuf.Assign(buf, count - *countRead);
        *countRead = count;

        LOG(("partial line [%s]\n", mLineBuf.get()));
    }

    // read something
    return NS_OK;
}

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
        if (mResponseHead) {
#if defined(PR_LOGGING)
            nsCAutoString headers;
            mResponseHead->Flatten(headers, PR_FALSE);
            LOG2(("http response [\n%s]\n", headers.get()));                        
#endif
            PRBool reset = PR_FALSE;

            // we ignore a 100 response, which means that we must reset
            // our state so that we'll parse from the beginning again.
            if (mResponseHead->Status() == 100) {
                LOG(("ignoring 100 response\n"));
                reset = PR_TRUE;
            }
            // notify the connection, give it a chance to cause a reset.
            else if (mConnection)
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

            // grab the content-length from the response headers
            mContentLength = mResponseHead->ContentLength();

            // handle chunked encoding here, so we'll know immediately when
            // we're done with the socket.  please note that _all_ other
            // decoding is done when the channel receives the content data
            // so as not to block the socket transport thread too much.
            const char *val =
                    mResponseHead->PeekHeader(nsHttp::Transfer_Encoding);
            if (PL_strcasestr(val, "chunked")) {
                // we only support the "chunked" transfer encoding right now.
                mChunkedDecoder = new nsHttpChunkedDecoder();
                if (!mChunkedDecoder)
                    return NS_ERROR_OUT_OF_MEMORY;
                LOG(("chunked decoder created\n"));

                val = mResponseHead->PeekHeader(nsHttp::Trailer);
                NS_ASSERTION(!val, "FIXME: unhandled trailer header present!");
            }
            else if (mContentLength == -1) {
                // check if this is a no-content response
                switch (mResponseHead->Status()) {
                case 204:
                case 205:
                case 304:
                    mNoContent = PR_TRUE;
                    LOG(("looks like this response should not contain a body.\n"));
                    break;
                }
                if (mNoContent)
                    mContentLength = 0;
                // otherwise, we'll wait for the server to close the connection.
            }
        }

        LOG(("nsHttpTransaction [this=%x] sending OnStartRequest\n", this));
        mFiredOnStart = PR_TRUE;

        rv = mListener->OnStartRequest(this, nsnull);
        if (NS_FAILED(rv)) {
            LOG(("OnStartRequest failed with [rv=%x]\n", rv));
            return rv;
        }
    }

    if (mChunkedDecoder) {
        // give the buf over to the chunked decoder so it can reformat the
        // data and tell us how much is really there.
        rv = mChunkedDecoder->HandleChunkedContent(buf, count, countRead);
        if (NS_FAILED(rv)) return rv;
    }
    else if (mContentLength >= 0)
        // when a content length has been specified...
        *countRead = PR_MIN(count, mContentLength - mContentRead);
    else
        // when we are just waiting for the server to close the connection...
        *countRead = count;

    if (*countRead) {
        // update count of content bytes read..
        mContentRead += *countRead;
        // and report progress
        mConnection->ReportProgress(mContentRead, mContentLength);
    }

    LOG(("nsHttpTransaction [this=%x count=%u read=%u mContentRead=%u mContentLength=%d]\n",
        this, count, *countRead, mContentRead, mContentLength));

    // check for end-of-file
    if ((mContentRead == PRUint32(mContentLength)) ||
        (mChunkedDecoder && mChunkedDecoder->ReachedEOF())) {
        // atomically mark the transaction as complete to ensure that
        // OnTransactionComplete is fired only once!
        PRInt32 priorVal = PR_AtomicSet(&mTransactionDone, 1);
        if (priorVal == 0 && mConnection) {
            // let the connection know that we are done with it; this should
            // result in OnStopTransaction being fired.
            return mConnection->OnTransactionComplete(NS_OK);
        }
        return NS_OK;
    }

    // if we didn't "read" anything and this is not a no-content response,
    // then we must return NS_BASE_STREAM_WOULD_BLOCK so we'll be called again.
    return (!mNoContent && !*countRead) ? NS_BASE_STREAM_WOULD_BLOCK : NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS2(nsHttpTransaction,
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

    // if the transaction is already "done" then there is nothing more to do.
    // ie., our consumer _will_ eventually receive their OnStopRequest.
    if (mTransactionDone) {
        LOG(("ignoring cancel since transaction is already done [this=%x]\n", this));
        return NS_OK;
    }

    // the status must be set immediately as the following routines may
    // take action asynchronously.
    mStatus = status;

    if (!mConnection) {
        // the connection is not assigned to a connection yet, so we must
        // notify the HTTP handler, so it can process the cancelation. 
        return nsHttpHandler::get()->CancelPendingTransaction(this, status);
    }

    // atomically cancel the connection.  it's important to consider that
    // the socket thread could already be in the middle of processing
    // completion, in which case we should ignore this cancelation request.

    PRInt32 priorVal = PR_AtomicSet(&mTransactionDone, 1);
    if (priorVal == 0)
        mConnection->OnTransactionComplete(status);

    return NS_OK;
}

NS_IMETHODIMP
nsHttpTransaction::Suspend()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpTransaction::Resume()
{
    LOG(("nsHttpTransaction::Resume [this=%x]\n", this));
    if (mConnection)
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

    // read some data from our source and put it in the given buf
    rv = mSource->Read(buf, count, bytesWritten);
    if (NS_FAILED(rv) || (*bytesWritten == 0)) {
        LOG(("nsHttpTransaction: mSource->Read() returned [rv=%x]\n", rv));
        return rv;
    }

    // pretend that no bytes were written (since we're just borrowing the
    // given buf anyways).
    count = *bytesWritten;
    *bytesWritten = 0;

    // we may not have read all of the headers yet...
    if (!mHaveAllHeaders) {
        PRUint32 offset = 0, bytesConsumed;

        while (count) {
            bytesConsumed = 0;

            rv = ParseHead(buf + offset, count, &bytesConsumed);
            if (NS_FAILED(rv)) return rv;

            count -= bytesConsumed;
            offset += bytesConsumed;

            // see if we're done reading headers
            if (mHaveAllHeaders) {
                LOG(("have all response headers\n"));
                break;
            }
        }

        if (count) {
            // buf has some content in it; shift bytes to top of buf.
            memmove(buf, buf + offset, count);
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
