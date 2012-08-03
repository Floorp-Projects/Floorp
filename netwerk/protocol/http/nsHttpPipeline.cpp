/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    : mConnection(nullptr)
    , mStatus(NS_OK)
    , mRequestIsPartial(false)
    , mResponseIsPartial(false)
    , mClosed(false)
    , mUtilizedPipeline(false)
    , mPushBackBuf(nullptr)
    , mPushBackLen(0)
    , mPushBackMax(0)
    , mHttp1xTransactionCount(0)
    , mReceivingFromProgress(0)
    , mSendingToProgress(0)
    , mSuppressSendEvents(true)
{
}

nsHttpPipeline::~nsHttpPipeline()
{
    // make sure we aren't still holding onto any transactions!
    Close(NS_ERROR_ABORT);

    NS_IF_RELEASE(mConnection);

    if (mPushBackBuf)
        free(mPushBackBuf);
}

nsresult
nsHttpPipeline::AddTransaction(nsAHttpTransaction *trans)
{
    LOG(("nsHttpPipeline::AddTransaction [this=%x trans=%x]\n", this, trans));

    if (mRequestQ.Length() || mResponseQ.Length())
        mUtilizedPipeline = true;

    NS_ADDREF(trans);
    mRequestQ.AppendElement(trans);
    PRUint32 qlen = PipelineDepth();
    
    if (qlen != 1) {
        trans->SetPipelinePosition(qlen);
    }
    else {
        // do it for this case in case an idempotent cancellation
        // is being repeated and an old value needs to be cleared
        trans->SetPipelinePosition(0);
    }

    // trans->SetConnection() needs to be updated to point back at
    // the pipeline object.
    trans->SetConnection(this);

    if (mConnection && !mClosed && mRequestQ.Length() == 1)
        mConnection->ResumeSend();

    return NS_OK;
}

PRUint32
nsHttpPipeline::PipelineDepth()
{
    return mRequestQ.Length() + mResponseQ.Length();
}

nsresult
nsHttpPipeline::SetPipelinePosition(PRInt32 position)
{
    nsAHttpTransaction *trans = Response(0);
    if (trans)
        return trans->SetPipelinePosition(position);
    return NS_OK;
}

PRInt32
nsHttpPipeline::PipelinePosition()
{
    nsAHttpTransaction *trans = Response(0);
    if (trans)
        return trans->PipelinePosition();

    // The response queue is empty, so return oldest request
    if (mRequestQ.Length())
        return Request(mRequestQ.Length() - 1)->PipelinePosition();
    
    // No transactions in the pipeline
    return 0;
}

nsHttpPipeline *
nsHttpPipeline::QueryPipeline()
{
    return this;
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
                                   bool *reset)
{
    LOG(("nsHttpPipeline::OnHeadersAvailable [this=%x]\n", this));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ASSERTION(mConnection, "no connection");
    
    nsRefPtr<nsHttpConnectionInfo> ci;
    GetConnectionInfo(getter_AddRefs(ci));

    NS_ABORT_IF_FALSE(ci, "no connection info");
    
    bool pipeliningBefore = gHttpHandler->ConnMgr()->SupportsPipelining(ci);
    
    // trans has now received its response headers; forward to the real connection
    nsresult rv = mConnection->OnHeadersAvailable(trans,
                                                  requestHead,
                                                  responseHead,
                                                  reset);
    
    if (!pipeliningBefore && gHttpHandler->ConnMgr()->SupportsPipelining(ci))
        // The received headers have expanded the eligible
        // pipeline depth for this connection
        gHttpHandler->ConnMgr()->ProcessPendingQForEntry(ci);

    return rv;
}

void
nsHttpPipeline::CloseTransaction(nsAHttpTransaction *trans, nsresult reason)
{
    LOG(("nsHttpPipeline::CloseTransaction [this=%x trans=%x reason=%x]\n",
        this, trans, reason));

    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ASSERTION(NS_FAILED(reason), "expecting failure code");

    // the specified transaction is to be closed with the given "reason"
    
    PRInt32 index;
    bool killPipeline = false;

    index = mRequestQ.IndexOf(trans);
    if (index >= 0) {
        if (index == 0 && mRequestIsPartial) {
            // the transaction is in the request queue.  check to see if any of
            // its data has been written out yet.
            killPipeline = true;
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
        killPipeline = true;
    }

    // Marking this connection as non-reusable prevents other items from being
    // added to it and causes it to be torn down soon. Don't tear it down yet
    // as that would prevent Response(0) from being processed.
    DontReuse();

    trans->Close(reason);
    NS_RELEASE(trans);

    if (killPipeline) {
        // reschedule anything from this pipeline onto a different connection
        CancelPipeline(reason);
    }
}

nsresult
nsHttpPipeline::TakeTransport(nsISocketTransport  **aTransport,
                              nsIAsyncInputStream **aInputStream,
                              nsIAsyncOutputStream **aOutputStream)
{
    return mConnection->TakeTransport(aTransport, aInputStream, aOutputStream);
}

bool
nsHttpPipeline::IsPersistent()
{
    return true; // pipelining requires this
}

bool
nsHttpPipeline::IsReused()
{
    if (!mUtilizedPipeline && mConnection)
        return mConnection->IsReused();
    return true;
}

void
nsHttpPipeline::DontReuse()
{
    if (mConnection)
        mConnection->DontReuse();
}

nsresult
nsHttpPipeline::PushBack(const char *data, PRUint32 length)
{
    LOG(("nsHttpPipeline::PushBack [this=%x len=%u]\n", this, length));
    
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ASSERTION(mPushBackLen == 0, "push back buffer already has data!");

    // If we have no chance for a pipeline (e.g. due to an Upgrade)
    // then push this data down to original connection
    if (!mConnection->IsPersistent())
        return mConnection->PushBack(data, length);

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
        NS_ASSERTION(length <= nsIOService::gDefaultSegmentSize, "too big");
        mPushBackMax = length;
        mPushBackBuf = (char *) realloc(mPushBackBuf, mPushBackMax);
        if (!mPushBackBuf)
            return NS_ERROR_OUT_OF_MEMORY;
    }
 
    memcpy(mPushBackBuf, data, length);
    mPushBackLen = length;

    return NS_OK;
}

nsHttpConnection *
nsHttpPipeline::TakeHttpConnection()
{
    if (mConnection)
        return mConnection->TakeHttpConnection();
    return nullptr;
}

nsAHttpTransaction::Classifier
nsHttpPipeline::Classification()
{
    if (mConnection)
        return mConnection->Classification();

    LOG(("nsHttpPipeline::Classification this=%p "
         "has null mConnection using CLASS_SOLO default", this));
    return nsAHttpTransaction::CLASS_SOLO;
}

void
nsHttpPipeline::SetSSLConnectFailed()
{
    nsAHttpTransaction *trans = Request(0);

    if (trans)
        trans->SetSSLConnectFailed();
}

nsHttpRequestHead *
nsHttpPipeline::RequestHead()
{
    nsAHttpTransaction *trans = Request(0);

    if (trans)
        return trans->RequestHead();
    return nullptr;
}

PRUint32
nsHttpPipeline::Http1xTransactionCount()
{
  return mHttp1xTransactionCount;
}

nsresult
nsHttpPipeline::TakeSubTransactions(
    nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions)
{
    LOG(("nsHttpPipeline::TakeSubTransactions [this=%p]\n", this));

    if (mResponseQ.Length() || mRequestIsPartial)
        return NS_ERROR_ALREADY_OPENED;

    PRInt32 i, count = mRequestQ.Length();
    for (i = 0; i < count; ++i) {
        nsAHttpTransaction *trans = Request(i);
        // set the transaction conneciton object back to the underlying
        // nsHttpConnectionHandle
        trans->SetConnection(mConnection);
        outTransactions.AppendElement(trans);
        NS_RELEASE(trans);
    }
    mRequestQ.Clear();

    LOG(("   took %d\n", count));
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpPipeline::nsAHttpTransaction
//-----------------------------------------------------------------------------

void
nsHttpPipeline::SetConnection(nsAHttpConnection *conn)
{
    LOG(("nsHttpPipeline::SetConnection [this=%x conn=%x]\n", this, conn));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ASSERTION(!mConnection, "already have a connection");

    NS_IF_ADDREF(mConnection = conn);
}

nsAHttpConnection *
nsHttpPipeline::Connection()
{
    LOG(("nsHttpPipeline::Connection [this=%x conn=%x]\n", this, mConnection));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    return mConnection;
}

void
nsHttpPipeline::GetSecurityCallbacks(nsIInterfaceRequestor **result,
                                     nsIEventTarget        **target)
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    // depending on timing this could be either the request or the response
    // that is needed - but they both go to the same host. A request for these
    // callbacks directly in nsHttpTransaction would not make a distinction
    // over whether the the request had been transmitted yet.
    nsAHttpTransaction *trans = Request(0);
    if (!trans)
        trans = Response(0);
    if (trans)
        trans->GetSecurityCallbacks(result, target);
    else {
        *result = nullptr;
        if (target)
            *target = nullptr;
    }
}

void
nsHttpPipeline::OnTransportStatus(nsITransport* transport,
                                  nsresult status, PRUint64 progress)
{
    LOG(("nsHttpPipeline::OnStatus [this=%x status=%x progress=%llu]\n",
        this, status, progress));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    nsAHttpTransaction *trans;
    PRInt32 i, count;

    switch (status) {

    case NS_NET_STATUS_RESOLVING_HOST:
    case NS_NET_STATUS_RESOLVED_HOST:
    case NS_NET_STATUS_CONNECTING_TO:
    case NS_NET_STATUS_CONNECTED_TO:
        // These should only appear at most once per pipeline.
        // Deliver to the first transaction.

        trans = Request(0);
        if (!trans)
            trans = Response(0);
        if (trans)
            trans->OnTransportStatus(transport, status, progress);

        break;

    case NS_NET_STATUS_SENDING_TO:
        // This is generated by the socket transport when (part) of
        // a transaction is written out
        //
        // In pipelining this is generated out of FillSendBuf(), but it cannot do
        // so until the connection is confirmed by CONNECTED_TO.
        // See patch for bug 196827.
        //

        if (mSuppressSendEvents) {
            mSuppressSendEvents = false;
            
            // catch up by sending the event to all the transactions that have
            // moved from request to response and any that have been partially
            // sent. Also send WAITING_FOR to those that were completely sent
            count = mResponseQ.Length();
            for (i = 0; i < count; ++i) {
                Response(i)->OnTransportStatus(transport,
                                               NS_NET_STATUS_SENDING_TO,
                                               progress);
                Response(i)->OnTransportStatus(transport, 
                                               NS_NET_STATUS_WAITING_FOR,
                                               progress);
            }
            if (mRequestIsPartial && Request(0))
                Request(0)->OnTransportStatus(transport,
                                              NS_NET_STATUS_SENDING_TO,
                                              progress);
            mSendingToProgress = progress;
        }
        // otherwise ignore it
        break;
        
    case NS_NET_STATUS_WAITING_FOR: 
        // Created by nsHttpConnection when request pipeline has been totally
        // sent. Ignore it here because it is simulated in FillSendBuf() when
        // a request is moved from request to response.
        
        // ignore it
        break;

    case NS_NET_STATUS_RECEIVING_FROM:
        // Forward this only to the transaction currently recieving data. It is
        // normally generated by the socket transport, but can also
        // be repeated by the pushbackwriter if necessary.
        mReceivingFromProgress = progress;
        if (Response(0))
            Response(0)->OnTransportStatus(transport, status, progress);
        break;

    default:
        // forward other notifications to all request transactions
        count = mRequestQ.Length();
        for (i = 0; i < count; ++i)
            Request(i)->OnTransportStatus(transport, status, progress);
        break;
    }
}

bool
nsHttpPipeline::IsDone()
{
    bool done = true;
    
    PRUint32 i, count = mRequestQ.Length();
    for (i = 0; done && (i < count); i++)
        done = Request(i)->IsDone();

    count = mResponseQ.Length();
    for (i = 0; done && (i < count); i++)
        done = Response(i)->IsDone();
    
    return done;
}

nsresult
nsHttpPipeline::Status()
{
    return mStatus;
}

PRUint8
nsHttpPipeline::Caps()
{
    nsAHttpTransaction *trans = Request(0);
    if (!trans)
        trans = Response(0);

    return trans ? trans->Caps() : 0;
}

PRUint32
nsHttpPipeline::Available()
{
    PRUint32 result = 0;

    PRInt32 i, count = mRequestQ.Length();
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

    if (mClosed) {
        *countRead = 0;
        return mStatus;
    }

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

        // return EOF if send buffer is empty
        if (avail == 0) {
            *countRead = 0;
            return NS_OK;
        }
    }

    // read no more than what was requested
    if (avail > count)
        avail = count;

    mReader = reader;

    rv = mSendBufIn->ReadSegments(ReadFromPipe, this, avail, countRead);

    mReader = nullptr;
    return rv;
}

nsresult
nsHttpPipeline::WriteSegments(nsAHttpSegmentWriter *writer,
                              PRUint32 count,
                              PRUint32 *countWritten)
{
    LOG(("nsHttpPipeline::WriteSegments [this=%x count=%u]\n", this, count));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (mClosed)
        return NS_SUCCEEDED(mStatus) ? NS_BASE_STREAM_CLOSED : mStatus;

    nsAHttpTransaction *trans; 
    nsresult rv;

    trans = Response(0);
    // This code deals with the establishment of a CONNECT tunnel through
    // an HTTP proxy. It allows the connection to do the CONNECT/200
    // HTTP transaction to establish an SSL tunnel as a precursor to the
    // actual pipeline of regular HTTP transactions.
    if (!trans && mRequestQ.Length() &&
        mConnection->IsProxyConnectInProgress()) {
        LOG(("nsHttpPipeline::WriteSegments [this=%p] Forced Delegation\n",
             this));
        trans = Request(0);
    }

    if (!trans) {
        if (mRequestQ.Length() > 0)
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

            // Release the transaction if it is not IsProxyConnectInProgress()
            if (trans == Response(0)) {
                NS_RELEASE(trans);
                mResponseQ.RemoveElementAt(0);
                mResponseIsPartial = false;
                ++mHttp1xTransactionCount;
            }

            // ask the connection manager to add additional transactions
            // to our pipeline.
            nsRefPtr<nsHttpConnectionInfo> ci;
            GetConnectionInfo(getter_AddRefs(ci));
            if (ci)
                gHttpHandler->ConnMgr()->ProcessPendingQForEntry(ci);
        }
        else
            mResponseIsPartial = true;
    }

    if (mPushBackLen) {
        nsHttpPushBackWriter writer(mPushBackBuf, mPushBackLen);
        PRUint32 len = mPushBackLen, n;
        mPushBackLen = 0;

        // This progress notification has previously been sent from
        // the socket transport code, but it was delivered to the
        // previous transaction on the pipeline.
        nsITransport *transport = Transport();
        if (transport)
            OnTransportStatus(transport, NS_NET_STATUS_RECEIVING_FROM,
                              mReceivingFromProgress);

        // the push back buffer is never larger than NS_HTTP_SEGMENT_SIZE,
        // so we are guaranteed that the next response will eat the entire
        // push back buffer (even though it might again call PushBack).
        rv = WriteSegments(&writer, len, &n);
    }

    return rv;
}

PRUint32
nsHttpPipeline::CancelPipeline(nsresult originalReason)
{
    PRUint32 i, reqLen, respLen, total;
    nsAHttpTransaction *trans;

    reqLen = mRequestQ.Length();
    respLen = mResponseQ.Length();
    total = reqLen + respLen;

    // don't count the first response, if presnet
    if (respLen)
        total--;

    if (!total)
        return 0;

    // any pending requests can ignore this error and be restarted
    // unless it is during a CONNECT tunnel request
    for (i = 0; i < reqLen; ++i) {
        trans = Request(i);
        if (mConnection && mConnection->IsProxyConnectInProgress())
            trans->Close(originalReason);
        else
            trans->Close(NS_ERROR_NET_RESET);
        NS_RELEASE(trans);
    }
    mRequestQ.Clear();

    // any pending responses can be restarted except for the first one,
    // that we might want to finish on this pipeline or cancel individually.
    // Higher levels of callers ensure that we don't process non-idempotent
    // tranasction with the NS_HTTP_ALLOW_PIPELINING bit set
    for (i = 1; i < respLen; ++i) {
        trans = Response(i);
        trans->Close(NS_ERROR_NET_RESET);
        NS_RELEASE(trans);
    }

    if (respLen > 1)
        mResponseQ.TruncateLength(1);

    DontReuse();
    Classify(nsAHttpTransaction::CLASS_SOLO);

    return total;
}

void
nsHttpPipeline::Close(nsresult reason)
{
    LOG(("nsHttpPipeline::Close [this=%x reason=%x]\n", this, reason));

    if (mClosed) {
        LOG(("  already closed\n"));
        return;
    }

    // the connection is going away!
    mStatus = reason;
    mClosed = true;

    nsRefPtr<nsHttpConnectionInfo> ci;
    GetConnectionInfo(getter_AddRefs(ci));
    PRUint32 numRescheduled = CancelPipeline(reason);

    // numRescheduled can be 0 if there is just a single response in the
    // pipeline object. That isn't really a meaningful pipeline that
    // has been forced to be rescheduled so it does not need to generate
    // negative feedback.
    if (ci && numRescheduled)
        gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
            ci, nsHttpConnectionMgr::RedCanceledPipeline, nullptr, 0);

    nsAHttpTransaction *trans = Response(0);
    if (!trans)
        return;

    // The current transaction can be restarted via reset
    // if the response has not started to arrive and the reason
    // for failure is innocuous (e.g. not an SSL error)
    if (!mResponseIsPartial &&
        (reason == NS_ERROR_NET_RESET ||
         reason == NS_OK ||
         reason == NS_ERROR_NET_TIMEOUT ||
         reason == NS_BASE_STREAM_CLOSED)) {
        trans->Close(NS_ERROR_NET_RESET);
    }
    else {
        trans->Close(reason);
    }

    NS_RELEASE(trans);
    mResponseQ.Clear();
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
                        nsIOService::gDefaultSegmentSize,  /* segment size */
                        nsIOService::gDefaultSegmentSize,  /* max size */
                        true, true);
        if (NS_FAILED(rv)) return rv;
    }

    PRUint32 n, avail;
    nsAHttpTransaction *trans;
    nsITransport *transport = Transport();

    while ((trans = Request(0)) != nullptr) {
        avail = trans->Available();
        if (avail) {
            // if there is already a response in the responseq then this
            // new data comprises a pipeline. Update the transaction in the
            // response queue to reflect that if necessary. We are now sending
            // out a request while we haven't received all responses.
            nsAHttpTransaction *response = Response(0);
            if (response && !response->PipelinePosition())
                response->SetPipelinePosition(1);
            rv = trans->ReadSegments(this, avail, &n);
            if (NS_FAILED(rv)) return rv;
            
            if (n == 0) {
                LOG(("send pipe is full"));
                break;
            }

            mSendingToProgress += n;
            if (!mSuppressSendEvents && transport) {
                // Simulate a SENDING_TO event
                trans->OnTransportStatus(transport,
                                         NS_NET_STATUS_SENDING_TO,
                                         mSendingToProgress);
            }
        }

        avail = trans->Available();
        if (avail == 0) {
            // move transaction from request queue to response queue
            mRequestQ.RemoveElementAt(0);
            mResponseQ.AppendElement(trans);
            mRequestIsPartial = false;

            if (!mSuppressSendEvents && transport) {
                // Simulate a WAITING_FOR event
                trans->OnTransportStatus(transport,
                                         NS_NET_STATUS_WAITING_FOR,
                                         mSendingToProgress);
            }

            // It would be good to re-enable data read handlers via ResumeRecv()
            // except the read handler code can be synchronously dispatched on
            // the stack.
        }
        else
            mRequestIsPartial = true;
    }
    return NS_OK;
}
