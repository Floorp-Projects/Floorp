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

#include "nsHttp.h"
#include "nsHttpPipeline.h"
#include "nsIRequest.h"
#include "nsISocketTransportService.h"
#include "nsIStringStream.h"
#include "nsIPipe.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsAutoLock.h"

#ifdef DEBUG
#include "prthread.h"
// defined by the socket transport service while active
extern PRThread *NS_SOCKET_THREAD;
#endif

//-----------------------------------------------------------------------------
// nsHttpPipeline::nsInputStreamWrapper
//-----------------------------------------------------------------------------

nsHttpPipeline::
nsInputStreamWrapper::nsInputStreamWrapper(const char *data, PRUint32 dataLen)
    : mData(data)
    , mDataLen(dataLen)
    , mDataPos(0)
{
}

nsHttpPipeline::
nsInputStreamWrapper::~nsInputStreamWrapper()
{
}

// this thing is going to be allocated on the stack
NS_IMETHODIMP_(nsrefcnt) nsHttpPipeline::
nsInputStreamWrapper::AddRef()
{
    return 1;
}

NS_IMETHODIMP_(nsrefcnt) nsHttpPipeline::
nsInputStreamWrapper::Release()
{
    return 1;
}

NS_IMPL_THREADSAFE_QUERY_INTERFACE1(nsHttpPipeline::nsInputStreamWrapper, nsIInputStream)

NS_IMETHODIMP nsHttpPipeline::
nsInputStreamWrapper::Close()
{
    return NS_OK;
}

NS_IMETHODIMP nsHttpPipeline::
nsInputStreamWrapper::Available(PRUint32 *result)
{
    *result = (mDataLen - mDataPos);
    return NS_OK;
}

static NS_METHOD
nsWriteToRawBuffer(nsIInputStream *inStr,
                   void *closure,
                   const char *fromRawSegment,
                   PRUint32 offset,
                   PRUint32 count,
                   PRUint32 *writeCount)
{
    char *toBuf = (char *) closure;
    memcpy(toBuf + offset, fromRawSegment, count);
    *writeCount = count;
    return NS_OK;
}

NS_IMETHODIMP nsHttpPipeline::
nsInputStreamWrapper::Read(char *buf, PRUint32 count, PRUint32 *countRead)
{
    return ReadSegments(nsWriteToRawBuffer, buf, count, countRead);
}

NS_IMETHODIMP nsHttpPipeline::
nsInputStreamWrapper::ReadSegments(nsWriteSegmentFun writer,
                                   void *closure,
                                   PRUint32 count,
                                   PRUint32 *countRead)
{
    nsresult rv;
    PRUint32 maxCount = mDataLen - mDataPos;
    if (count > maxCount)
        count = maxCount;

    // here's the code that distinguishes this implementation from other
    // string stream implementations.  normally, we'd return NS_OK to
    // signify EOF, but we need to return NS_BASE_STREAM_WOULD_BLOCK to
    // keep the nsStreamListenerProxy code happy.
    if (count == 0) {
        *countRead = 0;
        return NS_OK;
    }
    //if (count == 0)
    //    return NS_BASE_STREAM_WOULD_BLOCK;

    rv = writer(this, closure, mData + mDataPos, 0, count, countRead);
    if (NS_SUCCEEDED(rv))
        mDataPos += *countRead;
    return rv;
}

NS_IMETHODIMP nsHttpPipeline::
nsInputStreamWrapper::IsNonBlocking(PRBool *result)
{
    *result = PR_TRUE;
    return NS_OK;
}


//-----------------------------------------------------------------------------
// nsHttpPipeline <public>
//-----------------------------------------------------------------------------

nsHttpPipeline::nsHttpPipeline()
    : mConnection(nsnull)
    , mNumTrans(0)
    , mCurrentReader(-1)
    , mLock(nsnull)
    , mStatus(NS_OK)
{
    memset(mTransactionQ,     0, sizeof(PRUint32) * NS_HTTP_MAX_PIPELINED_REQUESTS);
    memset(mTransactionFlags, 0, sizeof(PRUint32) * NS_HTTP_MAX_PIPELINED_REQUESTS);
}

nsHttpPipeline::~nsHttpPipeline()
{
    NS_IF_RELEASE(mConnection);

    for (PRInt8 i=0; i<mNumTrans; i++) {
        if (mTransactionQ[i]) {
            nsAHttpTransaction *trans = mTransactionQ[i];
            NS_RELEASE(trans);
        }
    }

    if (mLock)
        PR_DestroyLock(mLock);
}

// called while inside nsHttpHandler::mConnectionLock
nsresult
nsHttpPipeline::Init(nsAHttpTransaction *firstTrans)
{
    LOG(("nsHttpPipeline::Init [this=%x trans=%x]\n", this, firstTrans));

    NS_ASSERTION(!mConnection, "already initialized");

    mLock = PR_NewLock();
    if (!mLock)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(firstTrans);
    mTransactionQ[0] = firstTrans;
    mNumTrans++;

    return NS_OK;
}

// called while inside nsHttpHandler::mConnectionLock
nsresult
nsHttpPipeline::AppendTransaction(nsAHttpTransaction *trans)
{
    LOG(("nsHttpPipeline::AppendTransaction [this=%x trans=%x]\n", this, trans));

    NS_ASSERTION(!mConnection, "already initialized");
    NS_ASSERTION(mNumTrans < NS_HTTP_MAX_PIPELINED_REQUESTS, "too many transactions");

    NS_ADDREF(trans);
    mTransactionQ[mNumTrans++] = trans;
    return NS_OK;
}


//-----------------------------------------------------------------------------
// nsHttpPipeline::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ADDREF(nsHttpPipeline)
NS_IMPL_THREADSAFE_RELEASE(nsHttpPipeline)

// multiple inheritance fun :-)
NS_INTERFACE_MAP_BEGIN(nsHttpPipeline)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsAHttpConnection)
NS_INTERFACE_MAP_END


//-----------------------------------------------------------------------------
// nsHttpPipeline::nsAHttpConnection
//-----------------------------------------------------------------------------

// called on the socket thread
nsresult
nsHttpPipeline::OnHeadersAvailable(nsAHttpTransaction *trans,
                                   nsHttpRequestHead *requestHead,
                                   nsHttpResponseHead *responseHead,
                                   PRBool *reset)
{
    LOG(("nsHttpPipeline::OnHeadersAvailable [this=%x]\n", this));

    NS_ASSERTION(mConnection, "no connection");
    // trans has now received its response headers; forward to the real connection
    return mConnection->OnHeadersAvailable(trans, requestHead, responseHead, reset);
}

// called on any thread
nsresult
nsHttpPipeline::OnTransactionComplete(nsAHttpTransaction *trans, nsresult status)
{
    LOG(("nsHttpPipeline::OnTransactionComplete [this=%x trans=%x status=%x]\n",
        this, trans, status));

    // called either from nsHttpTransaction::HandleContent (socket thread)
    //            or from nsHttpHandler::CancelTransaction (any thread)

    PRBool mustCancel = PR_FALSE, stopTrans = PR_FALSE;
    {
        nsAutoLock lock(mLock);

        PRInt8 transIndex = LocateTransaction_Locked(trans);
        NS_ASSERTION(transIndex != -1, "unknown transaction");

        mTransactionFlags[transIndex] = eTransactionComplete;

        if (NS_FAILED(status)) {
            mStatus = status;

            // don't bother waiting for a connection to be established if the
            // first transaction has been canceled.
            if (transIndex == 0)
                mustCancel = PR_TRUE;
            // go ahead and kill off the transaction if it hasn't started 
            // reading yet.
            if (transIndex > mCurrentReader) {
                stopTrans = PR_TRUE;
                DropTransaction_Locked(transIndex);
            }
        }
    }

    if (stopTrans)
        trans->OnStopTransaction(status);

    if (mustCancel) {
        NS_ASSERTION(mConnection, "no connection");
        mConnection->OnTransactionComplete(this, status);
    }

    return NS_OK;
}

// not called on the socket thread
nsresult
nsHttpPipeline::OnSuspend()
{
    LOG(("nsHttpPipeline::OnSuspend [this=%x]\n", this));

    NS_ASSERTION(mConnection, "no connection");
    return mConnection->OnSuspend();
}

// not called on the socket thread
nsresult
nsHttpPipeline::OnResume()
{
    LOG(("nsHttpPipeline::OnResume [this=%x]\n", this));

    NS_ASSERTION(mConnection, "no connection");
    return mConnection->OnResume();
}

// called on any thread
void
nsHttpPipeline::GetConnectionInfo(nsHttpConnectionInfo **result)
{
    LOG(("nsHttpPipeline::GetConnectionInfo [this=%x]\n", this));

    NS_ASSERTION(mConnection, "no connection");
    mConnection->GetConnectionInfo(result);
}

// called on the socket thread
void
nsHttpPipeline::DropTransaction(nsAHttpTransaction *trans)
{
    LOG(("nsHttpPipeline::DropTransaction [this=%x trans=%x]\n", this, trans));

    NS_ASSERTION(mConnection, "no connection");

    // clear the transaction from our queue
    {
        nsAutoLock lock(mLock);

        PRInt8 transIndex = LocateTransaction_Locked(trans);
        if (transIndex == -1)
            return;

        DropTransaction_Locked(transIndex);

        mStatus = NS_ERROR_NET_RESET;
    }

    // Assuming DropTransaction is called in response to a dead socket connection...
    mConnection->OnTransactionComplete(this, NS_ERROR_NET_RESET);
}

// called on any thread
PRBool
nsHttpPipeline::IsPersistent()
{
    return PR_TRUE;
}

// called on the socket thread
nsresult
nsHttpPipeline::PushBack(const char *data, PRUint32 length)
{
    LOG(("nsHttpPipeline::PushBack [this=%x len=%u]\n", this, length));

    nsInputStreamWrapper readable(data, length);

    return OnDataReadable(&readable);
}


//-----------------------------------------------------------------------------
// nsHttpPipeline::nsAHttpConnection
//-----------------------------------------------------------------------------

void
nsHttpPipeline::SetConnection(nsAHttpConnection *conn)
{
    LOG(("nsHttpPipeline::SetConnection [this=%x conn=%x]\n", this, conn));

    NS_ASSERTION(!mConnection, "already have a connection");
    NS_IF_ADDREF(mConnection = conn);

    // no need to be inside the lock
    for (PRInt8 i=0; i<mNumTrans; ++i) {
        NS_ASSERTION(mTransactionQ[i], "no transaction");
        if (mTransactionQ[i])
            mTransactionQ[i]->SetConnection(this);
    }
}

void
nsHttpPipeline::SetSecurityInfo(nsISupports *securityInfo)
{
    LOG(("nsHttpPipeline::SetSecurityInfo [this=%x]\n", this));

    // set security info on each transaction
    nsAutoLock lock(mLock);
    for (PRInt8 i=0; i<mNumTrans; ++i) {
        if (mTransactionQ[i])
            mTransactionQ[i]->SetSecurityInfo(securityInfo);
    }
}

void
nsHttpPipeline::GetNotificationCallbacks(nsIInterfaceRequestor **result)
{
    LOG(("nsHttpPipeline::GetNotificationCallbacks [this=%x]\n", this));

    // return notification callbacks from first transaction
    nsAutoLock lock(mLock);
    if (mTransactionQ[0])
        mTransactionQ[0]->GetNotificationCallbacks(result);
    else
        *result = nsnull;
}

PRUint32
nsHttpPipeline::GetRequestSize()
{
    LOG(("nsHttpPipeline::GetRequestSize [this=%x]\n", this));

    nsAutoLock lock(mLock);
    return GetRequestSize_Locked();
}

// called on the socket thread
nsresult
nsHttpPipeline::OnDataWritable(nsIOutputStream *stream)
{
    LOG(("nsHttpPipeline::OnDataWritable [this=%x]\n", this));

    NS_ASSERTION(PR_GetCurrentThread() == NS_SOCKET_THREAD, "wrong thread");

    nsresult rv;
    if (!mRequestData) {
        nsAutoLock lock(mLock);

        // check for early cancelation
        if (NS_FAILED(mStatus))
            return mStatus;

        // allocate a pipe for the request data
        PRUint32 size = GetRequestSize_Locked();
        nsCOMPtr<nsIOutputStream> outputStream;
        rv = NS_NewPipe(getter_AddRefs(mRequestData),
                        getter_AddRefs(outputStream),
                        size, size, PR_TRUE, PR_TRUE);
        if (NS_FAILED(rv)) return rv; 

        // fill the pipe
        for (PRInt32 i=0; i<mNumTrans; ++i) {
            // maybe this transaction has already been canceled...
            if (mTransactionQ[i]) {
                while (1) {
                    PRUint32 before = 0, after = 0;
                    mRequestData->Available(&before);
                    // append the transaction's request data to our buffer...
                    rv = mTransactionQ[i]->OnDataWritable(outputStream);
                    if (rv == NS_BASE_STREAM_CLOSED)
                        break; // advance to next transaction
                    if (NS_FAILED(rv))
                        return rv; // something bad happened!!
                    // else, there's more to write (the transaction may be
                    // writing in small chunks).  verify that the transaction
                    // actually wrote something to the pipe, and if it didn't,
                    // then advance to the next transaction to avoid an
                    // infinite loop (see bug 146884).
                    mRequestData->Available(&after);
                    if (before == after)
                        break;
                }
            }
        }
    }
    else {
        nsAutoLock lock(mLock);

        // check for early cancelation... (important for slow connections)
        // only abort if we haven't started reading
        if (NS_FAILED(mStatus) && (mCurrentReader == -1))
            return mStatus;
    }

    // find out how much data still needs to be written, and write it
    PRUint32 n = 0;
    rv = mRequestData->Available(&n);
    if (NS_FAILED(rv)) return rv;

    if (n > 0)
        return stream->WriteFrom(mRequestData, NS_HTTP_BUFFER_SIZE, &n);

    // if nothing to write, then signal EOF
    return NS_BASE_STREAM_CLOSED;
}

// called on the socket thread (may be called recursively)
nsresult
nsHttpPipeline::OnDataReadable(nsIInputStream *stream)
{
    LOG(("nsHttpPipeline::OnDataReadable [this=%x]\n", this));

    NS_ASSERTION(PR_GetCurrentThread() == NS_SOCKET_THREAD, "wrong thread");

    {
        nsresult rv = NS_OK;
        nsAutoLock lock(mLock);

        if (mCurrentReader == -1)
            mCurrentReader = 0;

        while (1) {
            nsAHttpTransaction *reader = mTransactionQ[mCurrentReader];
            // the reader may be NULL if it has already completed.
            if (!reader || (mTransactionFlags[mCurrentReader] & eTransactionComplete)) {
                // advance to next reader
                if (++mCurrentReader == mNumTrans) {
                    mCurrentReader = -1;
                    return NS_OK;
                }
                continue;
            }

            // remember the index of this reader
            PRUint32 readerIndex = mCurrentReader;
            PRUint32 bytesRemaining = 0;

            mTransactionFlags[readerIndex] |= eTransactionReading;

            // cannot hold lock while calling OnDataReadable... must also ensure
            // that |reader| doesn't dissappear on us.
            nsCOMPtr<nsISupports> readerDeathGrip(reader);
            PR_Unlock(mLock);

            rv = reader->OnDataReadable(stream);

            if (NS_SUCCEEDED(rv))
                rv = stream->Available(&bytesRemaining);

            PR_Lock(mLock);

            if (NS_FAILED(rv))
                return rv;

            // the reader may have completed...
            if (mTransactionFlags[readerIndex] & eTransactionComplete) {
                reader->OnStopTransaction(reader->Status());
                DropTransaction_Locked(readerIndex);
            }

            // the pipeline may have completed...
            if (NS_FAILED(mStatus) || IsDone_Locked())
                break; // exit lock

            // otherwise, if there is nothing left in the stream, then unwind...
            if (bytesRemaining == 0)
                return NS_OK;

            // PushBack may have been called during the call to OnDataReadable, so
            // we cannot depend on |reader| pointing to |mCurrentReader| anymore.
            // loop around, and re-acquire |reader|.
        }
    }

    NS_ASSERTION(mConnection, "no connection");
    mConnection->OnTransactionComplete(this, mStatus);
    return NS_OK;
}

// called on any thread
nsresult
nsHttpPipeline::OnStopTransaction(nsresult status)
{
    LOG(("nsHttpPipeline::OnStopTransaction [this=%x status=%x]\n", this, status));

    // called either from nsHttpHandler::CancelTransaction (mConnection == nsnull)
    //            or from nsHttpConnection::OnStopRequest (on the socket thread)

    if (mConnection) {
        NS_ASSERTION(PR_GetCurrentThread() == NS_SOCKET_THREAD, "wrong thread");
        nsAutoLock lock(mLock);
        // XXX this assertion is wrong!!  what about network errors??
        NS_ASSERTION(mStatus == status, "unexpected status");
        // reset any transactions that haven't already completed.
        //
        // normally, we'd expect the current reader to have completed already;
        // however, if the server happens to switch to HTTP/1.0 and not send a
        // Content-Length for one of the pipelined responses (yes, it does
        // happen!!), then we'll need to be sure to not reset the corresponding
        // transaction.
        for (PRInt8 i=0; i<mNumTrans; ++i) {
            if (mTransactionQ[i]) {
                nsAHttpTransaction *trans = mTransactionQ[i];
                NS_ADDREF(trans);

                PRBool mustReset = !(mTransactionFlags[i] & eTransactionReading);

                DropTransaction_Locked(i);

                PR_Unlock(mLock);
                if (mustReset)
                    // this will end up calling our DropTransaction, which will return
                    // early since we have already dropped this transaction.  this is
                    // important since it allows us to distinguish what we are doing
                    // here from premature EOF detection.
                    trans->OnStopTransaction(NS_ERROR_NET_RESET);
                else
                    trans->OnStopTransaction(status);
                PR_Lock(mLock);

                NS_RELEASE(trans);
            }
        }
        mCurrentReader = -1;
        mNumTrans = 0;
    }
    else {
        NS_ASSERTION(NS_FAILED(status), "unexpected cancelation status");
        NS_ASSERTION(mCurrentReader == -1, "unexpected reader");
        for (PRInt8 i=0; i<mNumTrans; ++i) {
            // maybe this transaction has already been canceled...
            if (mTransactionQ[i]) {
                mTransactionQ[i]->OnStopTransaction(status);
                DropTransaction_Locked(i);
            }
        }
    }

    return NS_OK;
}

// called on the socket thread
void
nsHttpPipeline::OnStatus(nsresult status, const PRUnichar *statusText)
{
    LOG(("nsHttpPipeline::OnStatus [this=%x status=%x]\n", this, status));

    nsAutoLock lock(mLock);
    switch (status) {
    case NS_NET_STATUS_RECEIVING_FROM:
        // forward this only to the transaction currently recieving data 
        if (mCurrentReader != -1 && mTransactionQ[mCurrentReader])
            mTransactionQ[mCurrentReader]->OnStatus(status, statusText);
        break;
    default:
        // forward other notifications to all transactions
        for (PRInt8 i=0; i<mNumTrans; ++i) {
            if (mTransactionQ[i])
                mTransactionQ[i]->OnStatus(status, statusText);
        }
        break;
    }
}

PRBool
nsHttpPipeline::IsDone()
{
    LOG(("nsHttpPipeline::IsDone [this=%x]\n", this));

    nsAutoLock lock(mLock);
    return IsDone_Locked();
}

nsresult
nsHttpPipeline::Status()
{
    LOG(("nsHttpPipeline::Status [this=%x status=%x]\n", this, mStatus));

    return mStatus;
}


//-----------------------------------------------------------------------------
// nsHttpPipeline <private>
//-----------------------------------------------------------------------------

PRBool
nsHttpPipeline::IsDone_Locked()
{
    // done if all of the transactions are null
    for (PRInt8 i=0; i<mNumTrans; ++i) {
        if (mTransactionQ[i])
            return PR_FALSE;
    }
    return PR_TRUE;
}

PRInt8
nsHttpPipeline::LocateTransaction_Locked(nsAHttpTransaction *trans)
{
    for (PRInt8 i=0; i<mNumTrans; ++i) {
        if (mTransactionQ[i] == trans)
            return i;
    }
    return -1;
}

void
nsHttpPipeline::DropTransaction_Locked(PRInt8 i)
{
    mTransactionFlags[i] = 0;
    NS_RELEASE(mTransactionQ[i]);
}

PRUint32
nsHttpPipeline::GetRequestSize_Locked()
{
    PRUint32 size = 0;
    for (PRInt8 i=0; i<mNumTrans; ++i) {
        // maybe this transaction has already been canceled...
        if (mTransactionQ[i])
            size += mTransactionQ[i]->GetRequestSize();
    }
    LOG(("  request-size=%u\n", size));
    return size; 
}
