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

#include <stdlib.h>
#include "nsHttp.h"
#include "nsHttpPipeline.h"
#include "nsHttpHandler.h"
#include "nsIOService.h"
#include "nsIRequest.h"
#include "nsISocketTransport.h"
#include "nsIStringStream.h"
#include "nsIPipe.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsAutoLock.h"

#ifdef DEBUG
#include "prthread.h"
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

//-----------------------------------------------------------------------------
// nsHttpPushBackWriter
//-----------------------------------------------------------------------------

class nsHttpPushBackWriter : public nsAHttpSegmentWriter
{
public:
    nsHttpPushBackWriter(const char *buf, PRUint32 bufLen)
        : mBuf(buf)
        , mBufLen(bufLen)
        { }
    virtual ~nsHttpPushBackWriter() {}

    nsresult OnWriteSegment(char *buf, PRUint32 count, PRUint32 *countWritten)
    {
        if (mBufLen == 0)
            return NS_BASE_STREAM_CLOSED;

        if (count > mBufLen)
            count = mBufLen;

        memcpy(buf, mBuf, count);

        mBuf += count;
        mBufLen -= count;
        *countWritten = count;
        return NS_OK;
    }

private:
    const char *mBuf;
    PRUint32    mBufLen;
};

//-----------------------------------------------------------------------------
// nsHttpPipeline <public>
//-----------------------------------------------------------------------------

nsHttpPipeline::nsHttpPipeline()
    : mConnection(nsnull)
    , mStatus(NS_OK)
    , mRequestIsPartial(PR_FALSE)
    , mResponseIsPartial(PR_FALSE)
    , mPushBackBuf(nsnull)
    , mPushBackLen(0)
    , mPushBackMax(0)
{
}

nsHttpPipeline::~nsHttpPipeline()
{
    // make sure we aren't still holding onto any transactions!
    Close(NS_ERROR_ABORT);

    if (mPushBackBuf)
        free(mPushBackBuf);
}

nsresult
nsHttpPipeline::AddTransaction(nsAHttpTransaction *trans)
{
    LOG(("nsHttpPipeline::AddTransaction [this=%x trans=%x]\n", this, trans));

    NS_ADDREF(trans);
    mRequestQ.AppendElement(trans);

    if (mConnection) {
        trans->SetConnection(this);

        if (mRequestQ.Count() == 1)
            mConnection->ResumeSend();
    }

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

nsresult
nsHttpPipeline::OnHeadersAvailable(nsAHttpTransaction *trans,
                                   nsHttpRequestHead *requestHead,
                                   nsHttpResponseHead *responseHead,
                                   PRBool *reset)
{
    LOG(("nsHttpPipeline::OnHeadersAvailable [this=%x]\n", this));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ASSERTION(mConnection, "no connection");

    // trans has now received its response headers; forward to the real connection
    return mConnection->OnHeadersAvailable(trans, requestHead, responseHead, reset);
}

nsresult
nsHttpPipeline::ResumeSend()
{
    NS_NOTREACHED("nsHttpPipeline::ResumeSend");
    return NS_ERROR_UNEXPECTED;
}

nsresult
nsHttpPipeline::ResumeRecv()
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ASSERTION(mConnection, "no connection");
    return mConnection->ResumeRecv();
}

void
nsHttpPipeline::CloseTransaction(nsAHttpTransaction *trans, nsresult reason)
{
    LOG(("nsHttpPipeline::CloseTransaction [this=%x trans=%x reason=%x]\n",
        this, trans, reason));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ASSERTION(NS_FAILED(reason), "expecting failure code");

    // the specified transaction is to be closed with the given "reason"
    
    PRInt32 index;
    PRBool killPipeline = PR_FALSE;

    index = mRequestQ.IndexOf(trans);
    if (index >= 0) {
        if (index == 0 && mRequestIsPartial) {
            // the transaction is in the request queue.  check to see if any of
            // its data has been written out yet.
            killPipeline = PR_TRUE;
        }
        mRequestQ.RemoveElementAt(index);
    }
    else {
        index = mResponseQ.IndexOf(trans);
        if (index >= 0)
            mResponseQ.RemoveElementAt(index);
        // while we could avoid killing the pipeline if this transaction is the
        // last transaction in the pipeline, there doesn't seem to be that much
        // value in doing so.  most likely if this transaction is going away,
        // the others will be shortly as well.
        killPipeline = PR_TRUE;
    }

    trans->Close(reason);
    NS_RELEASE(trans);

    if (killPipeline)
        Close(reason);
}

void
nsHttpPipeline::GetConnectionInfo(nsHttpConnectionInfo **result)
{
    NS_ASSERTION(mConnection, "no connection");
    mConnection->GetConnectionInfo(result);
}

void
nsHttpPipeline::GetSecurityInfo(nsISupports **result)
{
    NS_ASSERTION(mConnection, "no connection");
    mConnection->GetSecurityInfo(result);
}

PRBool
nsHttpPipeline::IsPersistent()
{
    return PR_TRUE; // pipelining requires this to be true!
}

nsresult
nsHttpPipeline::PushBack(const char *data, PRUint32 length)
{
    LOG(("nsHttpPipeline::PushBack [this=%x len=%u]\n", this, length));
    
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ASSERTION(mPushBackLen == 0, "push back buffer already has data!");

    // PushBack is called recursively from WriteSegments

    // XXX we have a design decision to make here.  either we buffer the data
    // and process it when we return to WriteSegments, or we attempt to move
    // onto the next transaction from here.  doing so adds complexity with the
    // benefit of eliminating the extra buffer copy.  the buffer is at most
    // 4096 bytes, so it is really unclear if there is any value in the added
    // complexity.  besides simplicity, buffering this data has the advantage
    // that we'll call close on the transaction sooner, which will wake up
    // the HTTP channel sooner to continue with its work.

    if (!mPushBackBuf) {
        mPushBackMax = length;
        mPushBackBuf = (char *) malloc(mPushBackMax);
        if (!mPushBackBuf)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    else if (length > mPushBackMax) {
        // grow push back buffer as necessary.
        NS_ASSERTION(length <= NS_HTTP_SEGMENT_SIZE, "too big");
        mPushBackMax = length;
        mPushBackBuf = (char *) realloc(mPushBackBuf, mPushBackMax);
        if (!mPushBackBuf)
            return NS_ERROR_OUT_OF_MEMORY;
    }
 
    memcpy(mPushBackBuf, data, length);
    mPushBackLen = length;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpPipeline::nsAHttpConnection
//-----------------------------------------------------------------------------

void
nsHttpPipeline::SetConnection(nsAHttpConnection *conn)
{
    LOG(("nsHttpPipeline::SetConnection [this=%x conn=%x]\n", this, conn));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ASSERTION(!mConnection, "already have a connection");

    NS_IF_ADDREF(mConnection = conn);

    PRInt32 i, count = mRequestQ.Count();
    for (i=0; i<count; ++i)
        Request(i)->SetConnection(this);
}

void
nsHttpPipeline::GetSecurityCallbacks(nsIInterfaceRequestor **result)
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    // return security callbacks from first request
    nsAHttpTransaction *trans = Request(0);
    if (trans)
        trans->GetSecurityCallbacks(result);
    else
        *result = nsnull;
}

void
nsHttpPipeline::OnTransportStatus(nsresult status, PRUint32 progress)
{
    LOG(("nsHttpPipeline::OnStatus [this=%x status=%x progress=%u]\n",
        this, status, progress));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    nsAHttpTransaction *trans;
    switch (status) {
    case NS_NET_STATUS_RECEIVING_FROM:
        // forward this only to the transaction currently recieving data 
        trans = Response(0);
        if (trans)
            trans->OnTransportStatus(status, progress);
        break;
    default:
        // forward other notifications to all transactions
        PRInt32 i, count = mRequestQ.Count();
        for (i=0; i<count; ++i) {
            trans = Request(i);
            if (trans)
                trans->OnTransportStatus(status, progress);
        }
        break;
    }
}

PRBool
nsHttpPipeline::IsDone()
{
    return (mRequestQ.Count() == 0) && (mResponseQ.Count() == 0);
}

nsresult
nsHttpPipeline::Status()
{
    return mStatus;
}

PRUint32
nsHttpPipeline::Available()
{
    PRUint32 result = 0;

    PRInt32 i, count = mRequestQ.Count();
    for (i=0; i<count; ++i)
        result += Request(i)->Available();
    return result;
}

NS_METHOD
nsHttpPipeline::ReadFromPipe(nsIInputStream *stream,
                             void *closure,
                             const char *buf,
                             PRUint32 offset,
                             PRUint32 count,
                             PRUint32 *countRead)
{
    nsHttpPipeline *self = (nsHttpPipeline *) closure;
    return self->mReader->OnReadSegment(buf, count, countRead);
}

nsresult
nsHttpPipeline::ReadSegments(nsAHttpSegmentReader *reader,
                             PRUint32 count,
                             PRUint32 *countRead)
{
    LOG(("nsHttpPipeline::ReadSegments [this=%x count=%u]\n", this, count));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    nsresult rv;

    PRUint32 avail = 0;
    if (mSendBufIn) {
        rv = mSendBufIn->Available(&avail);
        if (NS_FAILED(rv)) return rv;
    }

    if (avail == 0) {
        rv = FillSendBuf();
        if (NS_FAILED(rv)) return rv;

        rv = mSendBufIn->Available(&avail);
        if (NS_FAILED(rv)) return rv;
    }

    // read no more than what was requested
    if (avail > count)
        avail = count;

    mReader = reader;

    rv = mSendBufIn->ReadSegments(ReadFromPipe, this, avail, countRead);

    mReader = nsnull;
    return rv;
}

nsresult
nsHttpPipeline::WriteSegments(nsAHttpSegmentWriter *writer,
                              PRUint32 count,
                              PRUint32 *countWritten)
{
    LOG(("nsHttpPipeline::WriteSegments [this=%x count=%u]\n", this, count));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    nsAHttpTransaction *trans; 
    nsresult rv;

    trans = Response(0);
    if (!trans) {
        if (mRequestQ.Count() > 0)
            rv = NS_BASE_STREAM_WOULD_BLOCK;
        else
            rv = NS_BASE_STREAM_CLOSED;
    }
    else {
        // 
        // ask the transaction to consume data from the connection.
        // PushBack may be called recursively.
        //
        rv = trans->WriteSegments(writer, count, countWritten);

        if (rv == NS_BASE_STREAM_CLOSED || trans->IsDone()) {
            trans->Close(NS_OK);
            NS_RELEASE(trans);
            mResponseQ.RemoveElementAt(0);
            mResponseIsPartial = PR_FALSE;

            // ask the connection manager to add additional transactions
            // to our pipeline.
            gHttpHandler->ConnMgr()->AddTransactionToPipeline(this);
        }
        else
            mResponseIsPartial = PR_TRUE;
    }

    if (mPushBackLen) {
        nsHttpPushBackWriter writer(mPushBackBuf, mPushBackLen);
        PRUint32 len = mPushBackLen, n;
        mPushBackLen = 0;
        // the push back buffer is never larger than NS_HTTP_SEGMENT_SIZE,
        // so we are guaranteed that the next response will eat the entire
        // push back buffer (even though it might again call PushBack).
        rv = WriteSegments(&writer, len, &n);
    }

    return rv;
}

void
nsHttpPipeline::Close(nsresult reason)
{
    LOG(("nsHttpPipeline::Close [this=%x reason=%x]\n", this, reason));

    // the connection is going away!
    mStatus = reason;

    // we must no longer reference the connection!
    NS_IF_RELEASE(mConnection);

    PRUint32 i, count;
    nsAHttpTransaction *trans;

    // any pending requests can ignore this error and be restarted
    count = mRequestQ.Count();
    for (i=0; i<count; ++i) {
        trans = Request(i);
        trans->Close(NS_ERROR_NET_RESET);
        NS_RELEASE(trans);
    }
    mRequestQ.Clear();

    trans = Response(0);
    if (trans) {
        // if the current response is partially complete, then it cannot be
        // restarted and will have to fail with the status of the connection.
        if (mResponseIsPartial)
            trans->Close(reason);
        else
            trans->Close(NS_ERROR_NET_RESET);
        NS_RELEASE(trans);
        
        // any remaining pending responses can be restarted
        count = mResponseQ.Count();
        for (i=1; i<count; ++i) {
            trans = Response(i);
            trans->Close(NS_ERROR_NET_RESET);
            NS_RELEASE(trans);
        }
        mResponseQ.Clear();
    }
}

nsresult
nsHttpPipeline::OnReadSegment(const char *segment,
                              PRUint32 count,
                              PRUint32 *countRead)
{
    return mSendBufOut->Write(segment, count, countRead);
}

nsresult
nsHttpPipeline::FillSendBuf()
{
    // reads from request queue, moving transactions to response queue
    // when they have been completely read.

    nsresult rv;
    
    if (!mSendBufIn) {
        // allocate a single-segment pipe
        rv = NS_NewPipe(getter_AddRefs(mSendBufIn),
                        getter_AddRefs(mSendBufOut),
                        NS_HTTP_SEGMENT_SIZE,
                        NS_HTTP_SEGMENT_SIZE,
                        PR_TRUE, PR_TRUE,
                        nsIOService::gBufferCache);
        if (NS_FAILED(rv)) return rv;
    }

    PRUint32 n, avail;
    nsAHttpTransaction *trans;
    while ((trans = Request(0)) != nsnull) {
        avail = trans->Available();
        if (avail) {
            rv = trans->ReadSegments(this, avail, &n);
            if (NS_FAILED(rv)) return rv;
            
            if (n == 0) {
                LOG(("send pipe is full"));
                break;
            }
        }
        avail = trans->Available();
        if (avail == 0) {
            // move transaction from request queue to response queue
            mRequestQ.RemoveElementAt(0);
            mResponseQ.AppendElement(trans);
            mRequestIsPartial = PR_FALSE;
        }
        else
            mRequestIsPartial = PR_TRUE;
    }
    return NS_OK;
}
