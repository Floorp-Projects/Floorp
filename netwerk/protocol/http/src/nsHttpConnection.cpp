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

#include "nsHttpConnection.h"
#include "nsHttpTransaction.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsHttpHandler.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIServiceManager.h"
#include "nsISSLSocketControl.h"
#include "nsIStringStream.h"
#include "netCore.h"
#include "nsNetCID.h"
#include "nsAutoLock.h"
#include "prmem.h"
#include "plevent.h"

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *NS_SOCKET_THREAD;
#endif

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

//-----------------------------------------------------------------------------
// nsHttpConnection <public>
//-----------------------------------------------------------------------------

nsHttpConnection::nsHttpConnection()
    : mTransaction(0)
    , mConnectionInfo(0)
    , mLock(nsnull)
    , mReadStartTime(0)
    , mLastActiveTime(0)
    , mIdleTimeout(0)
    , mKeepAlive(1) // assume to keep-alive by default
    , mKeepAliveMask(1)
    , mWriteDone(0)
    , mReadDone(0)
{
    LOG(("Creating nsHttpConnection @%x\n", this));

    NS_INIT_ISUPPORTS();
}

nsHttpConnection::~nsHttpConnection()
{
    LOG(("Destroying nsHttpConnection @%x\n", this));
 
    NS_IF_RELEASE(mConnectionInfo);
    NS_IF_RELEASE(mTransaction);

    if (mLock) {
        PR_DestroyLock(mLock);
        mLock = nsnull;
    }
}

nsresult
nsHttpConnection::Init(nsHttpConnectionInfo *info, PRUint16 maxHangTime)
{
    LOG(("nsHttpConnection::Init [this=%x]\n", this));

    NS_ENSURE_ARG_POINTER(info);
    NS_ENSURE_TRUE(!mConnectionInfo, NS_ERROR_ALREADY_INITIALIZED);

    mLock = PR_NewLock();
    if (!mLock)
        return NS_ERROR_OUT_OF_MEMORY;

    mConnectionInfo = info;
    NS_ADDREF(mConnectionInfo);

    mMaxHangTime = maxHangTime;
    return NS_OK;
}

// called from any thread, with the connection lock held
nsresult
nsHttpConnection::SetTransaction(nsAHttpTransaction *transaction,
                                 PRUint8 caps)
{
    LOG(("nsHttpConnection::SetTransaction [this=%x trans=%x]\n",
         this, transaction));

    NS_ENSURE_TRUE(!mTransaction, NS_ERROR_IN_PROGRESS);
    NS_ENSURE_ARG_POINTER(transaction);

    // take ownership of the transaction
    mTransaction = transaction;
    NS_ADDREF(mTransaction);

    // default mKeepAlive according to what will be requested
    mKeepAliveMask = mKeepAlive = (caps & NS_HTTP_ALLOW_KEEPALIVE);

    return ActivateConnection();
}

// called from the socket thread
nsresult
nsHttpConnection::ProxyStepUp()
{
    LOG(("nsHttpConnection::ProxyStepUp [this=%x]\n", this));
#ifdef DEBUG
    NS_PRECONDITION(PR_GetCurrentThread() == NS_SOCKET_THREAD, "wrong thread");
#endif

    nsCOMPtr<nsISupports> securityInfo;
    nsresult rv = mSocketTransport->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISSLSocketControl> ssl = do_QueryInterface(securityInfo, &rv);
    if (NS_FAILED(rv)) return rv;

    return ssl->ProxyStepUp();
}

PRBool
nsHttpConnection::CanReuse()
{
    return IsKeepAlive() && (NowInSeconds() - mLastActiveTime < mIdleTimeout)
                         && IsAlive();
}

PRBool
nsHttpConnection::IsAlive()
{
    if (!mSocketTransport)
        return PR_FALSE;

    PRBool isAlive = PR_FALSE;
    nsresult rv = mSocketTransport->IsAlive(0, &isAlive);
    NS_ASSERTION(NS_SUCCEEDED(rv), "IsAlive test failed");
    return isAlive;
}

//----------------------------------------------------------------------------
// nsHttpConnection::nsAHttpConnection
//----------------------------------------------------------------------------

// called from the socket thread
nsresult
nsHttpConnection::OnHeadersAvailable(nsAHttpTransaction *trans,
                                     nsHttpResponseHead *responseHead,
                                     PRBool *reset)
{
    LOG(("nsHttpConnection::OnHeadersAvailable [this=%p trans=%p response-head=%p]\n",
        this, trans, responseHead));

    NS_ENSURE_ARG_POINTER(trans);

    // we won't change our keep-alive policy unless the server has explicitly
    // told us to do so.

    if (!trans || !responseHead) {
        LOG(("nothing to do\n"));
        return NS_OK;
    }

    // inspect the connection headers for keep-alive info provided the
    // transaction completed successfully.
    const char *val = responseHead->PeekHeader(nsHttp::Connection);
    if (!val)
        val = responseHead->PeekHeader(nsHttp::Proxy_Connection);

    if ((responseHead->Version() < NS_HTTP_VERSION_1_1) ||
        (nsHttpHandler::get()->DefaultVersion() < NS_HTTP_VERSION_1_1)) {
        // HTTP/1.0 connections are by default NOT persistent
        if (val && !PL_strcasecmp(val, "keep-alive"))
            mKeepAlive = PR_TRUE;
        else
            mKeepAlive = PR_FALSE;
    }
    else {
        // HTTP/1.1 connections are by default persistent
        if (val && !PL_strcasecmp(val, "close")) 
            mKeepAlive = PR_FALSE;
        else
            mKeepAlive = PR_TRUE;
    }
    mKeepAliveMask = mKeepAlive;

    // if this connection is persistent, then the server may send a "Keep-Alive"
    // header specifying the maximum number of times the connection can be
    // reused as well as the maximum amount of time the connection can be idle
    // before the server will close it.  we ignore the max reuse count, because
    // a "keep-alive" connection is by definition capable of being reused, and
    // we only care about being able to reuse it once.  if a timeout is not 
    // specified then we use our advertized timeout value.
    if (mKeepAlive) {
        val = responseHead->PeekHeader(nsHttp::Keep_Alive);

        const char *cp = PL_strcasestr(val, "timeout=");
        if (cp)
            mIdleTimeout = (PRUint32) atoi(cp + 8);
        else
            mIdleTimeout = nsHttpHandler::get()->IdleTimeout();
        
        LOG(("Connection can be reused [this=%x idle-timeout=%u\n", this, mIdleTimeout));
    }

    // if we're doing an SSL proxy connect, then we need to check whether or not
    // the connect was successful.  if so, then we have to reset the transaction
    // and step-up the socket connection to SSL. finally, we have to wake up the
    // socket write request.
    if (mSSLProxyConnectStream) {
        mSSLProxyConnectStream = 0;
        if (responseHead->Status() == 200) {
            LOG(("SSL proxy CONNECT succeeded!\n"));
            *reset = PR_TRUE;
            ProxyStepUp();
            mWriteRequest->Resume();
        }
        else {
            LOG(("SSL proxy CONNECT failed!\n"));
            // close out the write request
            mWriteRequest->Cancel(NS_OK);
        }
    }

    return NS_OK;
}

// called from any thread
nsresult
nsHttpConnection::OnTransactionComplete(nsAHttpTransaction *trans, nsresult status)
{
    LOG(("nsHttpConnection::OnTransactionComplete [this=%x trans=%x status=%x]\n",
        this, trans, status));

    // trans may not be mTransaction
    if (trans != mTransaction)
        return NS_OK;

    nsCOMPtr<nsIRequest> writeReq, readReq;

    // clear the read/write requests atomically.
    {
        nsAutoLock lock(mLock);
        writeReq = mWriteRequest;
        readReq = mReadRequest;
    }

    // cancel the requests... this will cause OnStopRequest to be fired
    if (writeReq)
        writeReq->Cancel(status);
    if (readReq)
        readReq->Cancel(status);

    return NS_OK;
}

// not called from the socket thread
nsresult
nsHttpConnection::OnSuspend()
{
    // we only bother to suspend the read request, since that's the only
    // one that will effect our consumers.
 
    nsCOMPtr<nsIRequest> readReq;
    {
        nsAutoLock lock(mLock);
        readReq = mReadRequest;
    }

    if (readReq)
        readReq->Suspend();

    return NS_OK;
}

// not called from the socket thread
nsresult
nsHttpConnection::OnResume()
{
    // we only need to worry about resuming the read request, since that's
    // the only one that can be suspended.

    nsCOMPtr<nsIRequest> readReq;
    {
        nsAutoLock lock(mLock);
        readReq = mReadRequest;
    }

    if (readReq)
        readReq->Resume();

    return NS_OK;
}

// called on the socket thread
void
nsHttpConnection::DropTransaction(nsAHttpTransaction *trans)
{
    LOG(("nsHttpConnection::DropTransaction [trans=%p]\n", trans));

    // the assertion here is that the transaction will not be destroyed
    // by this release.  we unfortunately don't have a threadsafe way of
    // asserting this.
    NS_IF_RELEASE(mTransaction);
    mTransaction = 0;
    
    // if the transaction was dropped, then we cannot reuse this connection.
    mKeepAliveMask = mKeepAlive = PR_FALSE;
}

//-----------------------------------------------------------------------------
// nsHttpConnection <private>
//-----------------------------------------------------------------------------

// called on any thread
nsresult
nsHttpConnection::ActivateConnection()
{
    nsresult rv;

    // if we don't have a socket transport then create a new one
    if (!mSocketTransport) {
        rv = CreateTransport();
        if (NS_FAILED(rv)) return rv;

        // need to handle SSL proxy CONNECT if this is the first time.
        if (mConnectionInfo->UsingSSL() &&
            mConnectionInfo->UsingHttpProxy()) {
            rv = SetupSSLProxyConnect();
            if (NS_FAILED(rv)) return rv;
        }
    }

    // allow the socket transport to call us directly on progress
    rv = mSocketTransport->
            SetNotificationCallbacks(this, nsITransport::DONT_PROXY_PROGRESS);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRequest> writeReq, readReq;
    PRBool mustCancelWrite = PR_FALSE;
    PRBool mustCancelRead = PR_FALSE;

    mWriteDone = PR_FALSE;
    mReadDone = PR_FALSE;

    // because AsyncRead can result in listener callbacks on the socket
    // transport thread before it even runs, we have to addref this in order
    // to ensure that we stay around long enough to complete this call.
    NS_ADDREF_THIS();

    // by the same token, we need to ensure that the transaction stays around.
    // and we must not access it via mTransaction, since mTransaction is null'd
    // in our OnStopRequest.
    nsAHttpTransaction *trans = mTransaction;
    NS_ADDREF(trans);

    // We need to tell the socket transport what origin server we're
    // communicating with. This may not be the same as the original host,
    // if we're talking to a proxy server using persistent connections.
    // We update the connectionInfo as well, so that our logging is correct.
    // See bug 94088

    mSocketTransport->SetHost(mConnectionInfo->Host());
    mSocketTransport->SetPort(mConnectionInfo->Port());
    
    // fire off the read first so that we'll often detect premature EOF before
    // writing to the socket, though this is not necessary.
    rv = mSocketTransport->AsyncRead(this, nsnull,
                                     0, PRUint32(-1),
                                     nsITransport::DONT_PROXY_OBSERVER |
                                     nsITransport::DONT_PROXY_LISTENER,
                                     getter_AddRefs(readReq));
    if (NS_FAILED(rv)) goto end;

    // we pass a reference to ourselves as the context so we can
    // differentiate the stream listener events.
    rv = mSocketTransport->AsyncWrite(this, (nsIStreamListener*) this,
                                      0, PRUint32(-1),
                                      nsITransport::DONT_PROXY_OBSERVER |
                                      nsITransport::DONT_PROXY_PROVIDER,
                                      getter_AddRefs(writeReq));
    if (NS_FAILED(rv)) goto end;

    // grab pointers to the read/write requests provided they have not
    // already finished.  check for early cancelation (indicated by the
    // transaction being in the DONE state).
    {
        nsAutoLock lock(mLock);

        if (!mWriteDone) {
            mWriteRequest = writeReq;
            if (trans->IsDone())
                mustCancelWrite = PR_TRUE;
        }
        if (!mReadDone) {
            mReadRequest = readReq;
            if (trans->IsDone())
                mustCancelRead = PR_TRUE;
        }
    }

    // handle the case of an overlapped cancelation (ie. the transaction
    // could have been canceled prior to mReadRequest/mWriteRequest being
    // assigned).
    if (mustCancelWrite)
        writeReq->Cancel(trans->Status());
    if (mustCancelRead)
        readReq->Cancel(trans->Status());

end: 
    NS_RELEASE(trans);
    NS_RELEASE_THIS();
    return rv;
}

nsresult
nsHttpConnection::CreateTransport()
{
    nsresult rv;

    NS_PRECONDITION(!mSocketTransport, "unexpected");

    nsCOMPtr<nsISocketTransportService> sts =
            do_GetService(kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // configure the socket type based on the connection type requested.
    const char* type = nsnull;

    if (mConnectionInfo->UsingSSL()) {
        if (mConnectionInfo->UsingHttpProxy())
            type = "tlsstepup";
        else
            type = "ssl";
    }

    nsCOMPtr<nsITransport> transport;
    rv = sts->CreateTransportOfType(type,
        mConnectionInfo->Host(),
        mConnectionInfo->Port(),
        mConnectionInfo->ProxyInfo(),
        NS_HTTP_SEGMENT_SIZE,
        NS_HTTP_BUFFER_SIZE,
        getter_AddRefs(transport));
    if (NS_FAILED(rv)) return rv;

    // the transport has better be a socket transport !!
    mSocketTransport = do_QueryInterface(transport, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = mSocketTransport->SetReuseConnection(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

nsresult
nsHttpConnection::SetupSSLProxyConnect()
{
    nsresult rv;
    const char *val;

    LOG(("nsHttpConnection::SetupSSLProxyConnect [this=%x]\n", this));

    NS_ENSURE_TRUE(!mSSLProxyConnectStream, NS_ERROR_ALREADY_INITIALIZED);

    nsCAutoString buf;
    buf.Assign(mConnectionInfo->Host());
    buf.Append(':');
    buf.AppendInt(mConnectionInfo->Port());

    // CONNECT host:port HTTP/1.1
    nsHttpRequestHead request;
    request.SetMethod(nsHttp::Connect);
    request.SetVersion(nsHttpHandler::get()->DefaultVersion());
    request.SetRequestURI(buf.get());
    request.SetHeader(nsHttp::User_Agent, nsHttpHandler::get()->UserAgent());

    // NOTE: this cast is valid since this connection cannot be processing a
    // transaction pipeline until after the first HTTP/1.1 response.
    nsHttpTransaction *trans = NS_STATIC_CAST(nsHttpTransaction *, mTransaction);
    
    val = trans->RequestHead()->PeekHeader(nsHttp::Host);
    if (val) {
        // all HTTP/1.1 requests must include a Host header (even though it
        // may seem redundant in this case; see bug 82388).
        request.SetHeader(nsHttp::Host, val);
    }

    val = trans->RequestHead()->PeekHeader(nsHttp::Proxy_Authorization);
    if (val) {
        // we don't know for sure if this authorization is intended for the
        // SSL proxy, so we add it just in case.
        request.SetHeader(nsHttp::Proxy_Authorization, val);
    }

    buf.Truncate(0);
    request.Flatten(buf);
    buf.Append("\r\n");

    nsCOMPtr<nsISupports> sup;
    rv = NS_NewCStringInputStream(getter_AddRefs(sup), buf);
    if (NS_FAILED(rv)) return rv;

    mSSLProxyConnectStream = do_QueryInterface(sup, &rv);
    return rv;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ADDREF(nsHttpConnection)
NS_IMPL_THREADSAFE_RELEASE(nsHttpConnection)

NS_INTERFACE_MAP_BEGIN(nsHttpConnection)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIStreamProvider)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIRequestObserver, nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
    NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
NS_INTERFACE_MAP_END_THREADSAFE

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIRequestObserver
//-----------------------------------------------------------------------------

// called on the socket transport thread
NS_IMETHODIMP
nsHttpConnection::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    LOG(("nsHttpConnection::OnStartRequest [this=%x]\n", this));

    // because this code can run before AsyncWrite (AsyncRead) returns, we need
    // to capture the write (read) request object.  see bug 90196 for an example
    // of why this is necessary.
    if (ctxt) {
        mWriteRequest = request;
        // this only needs to be done once, so do it for the write request.
        if (mTransaction) {
            nsCOMPtr<nsISupports> info;
            mSocketTransport->GetSecurityInfo(getter_AddRefs(info));
            mTransaction->SetSecurityInfo(info);
        }
    }
    else {
        mReadRequest = request;
        mReadStartTime = NowInSeconds();
    }

    return NS_OK;
}

// called on the socket transport thread
NS_IMETHODIMP
nsHttpConnection::OnStopRequest(nsIRequest *request, nsISupports *ctxt,
                                nsresult status)
{
    LOG(("nsHttpConnection::OnStopRequest [this=%x ctxt=%x status=%x]\n",
        this, ctxt, status));

    if (ctxt == (nsISupports *) (nsIStreamListener *) this) {
        // Done writing, so mark the write request as complete.
        nsAutoLock lock(mLock);
        mWriteDone = PR_TRUE;
        mWriteRequest = 0;
    }
    else {
        // Done reading, so mark the read request as complete.
        nsAutoLock lock(mLock);
        mReadDone = PR_TRUE;
        mReadRequest = 0;
    }

    if (mReadDone && mWriteDone) {
        // if the transaction failed for any reason, the socket will not be
        // reusable.
        if (NS_FAILED(status))
            DontReuse();

        nsCOMPtr<nsISupports> info;
        mSocketTransport->GetSecurityInfo(getter_AddRefs(info));
        nsCOMPtr<nsISSLSocketControl> secCtrl(do_QueryInterface(info));

        // break the cycle between the security info and this
        if (secCtrl)
          secCtrl->SetNotificationCallbacks(nsnull);

        // break the cycle between the socket transport and this
        mSocketTransport->SetNotificationCallbacks(nsnull, 0);

        // make sure mTransaction is clear before calling OnStopTransaction
        if (mTransaction) { 
            nsAHttpTransaction *trans = mTransaction;
            mTransaction = nsnull;

            trans->OnStopTransaction(status);
            NS_RELEASE(trans);
        }

        // reset the keep-alive mask
        mKeepAliveMask = mKeepAlive;

        nsHttpHandler::get()->ReclaimConnection(this);
    }
    // no point in returning anything else but NS_OK
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIStreamProvider
//-----------------------------------------------------------------------------

// called on the socket transport thread
NS_IMETHODIMP
nsHttpConnection::OnDataWritable(nsIRequest *request, nsISupports *context,
                                 nsIOutputStream *outputStream,
                                 PRUint32 offset, PRUint32 count)
{
    if (!mTransaction) {
        LOG(("nsHttpConnection: no transaction! closing stream\n"));
        return NS_BASE_STREAM_CLOSED;
    }

    LOG(("nsHttpConnection::OnDataWritable [this=%x]\n", this));

    // if we're doing an SSL proxy connect, then we need to bypass calling
    // into the transaction.
    if (mSSLProxyConnectStream) {
        PRUint32 n;

        nsresult rv = mSSLProxyConnectStream->Available(&n);
        if (NS_FAILED(rv)) return rv;

        // if there are bytes available in the stream, then write them out.
        // otherwise, suspend the write request... it'll get restarted once
        // we get a response from the proxy server.
        if (n) {
            LOG(("writing data from proxy connect stream [count=%u]\n", n));
            return outputStream->WriteFrom(mSSLProxyConnectStream, n, &n);
        }

        LOG(("done writing proxy connect stream, waiting for response.\n"));
        return NS_BASE_STREAM_WOULD_BLOCK;
    }

    LOG(("calling mTransaction->OnDataWritable\n"));

    // in the normal case, we just want to defer to the transaction to write
    // out the request.
    return mTransaction->OnDataWritable(outputStream);
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIStreamListener
//-----------------------------------------------------------------------------

// called on the socket transport thread
NS_IMETHODIMP
nsHttpConnection::OnDataAvailable(nsIRequest *request, nsISupports *context,
                                  nsIInputStream *inputStream,
                                  PRUint32 offset, PRUint32 count)
{
    LOG(("nsHttpConnection::OnDataAvailable [this=%x]\n", this));

    if (!mTransaction) {
        LOG(("no transaction! closing stream\n"));
        return NS_BASE_STREAM_CLOSED;
    }

    mLastActiveTime = NowInSeconds();

    if (mKeepAliveMask &&
            (mLastActiveTime - mReadStartTime >= PRUint32(mMaxHangTime))) {
        LOG(("max hang time exceeded!\n"));
        // give the handler a chance to create a new persistent connection to
        // this host if we've been busy for too long.
        mKeepAliveMask = PR_FALSE;
        nsHttpHandler::get()->ProcessTransactionQ();
    }

    nsresult rv = mTransaction->OnDataReadable(inputStream);

    LOG(("mTransaction->OnDataReadable() returned [rv=%x]\n", rv));
    return rv;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

// not called on the socket transport thread
NS_IMETHODIMP
nsHttpConnection::GetInterface(const nsIID &iid, void **result)
{
    if (iid.Equals(NS_GET_IID(nsIProgressEventSink)))
        return QueryInterface(iid, result);

    if (mTransaction) {
        nsCOMPtr<nsIInterfaceRequestor> callbacks;
        mTransaction->GetNotificationCallbacks(getter_AddRefs(callbacks));
        return callbacks->GetInterface(iid, result);
    }

    return NS_ERROR_NO_INTERFACE;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIProgressEventSink
//-----------------------------------------------------------------------------

// called on the socket transport thread
NS_IMETHODIMP
nsHttpConnection::OnStatus(nsIRequest *req, nsISupports *ctx, nsresult status,
                           const PRUnichar *statusText)
{
    if (mTransaction)
        mTransaction->OnStatus(status, statusText);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpConnection::OnProgress(nsIRequest *req, nsISupports *ctx,
                             PRUint32 progress, PRUint32 progressMax)
{
    // we ignore progress notifications from the socket transport.
    // we'll generate these ourselves from OnDataAvailable.
    return NS_OK;
}
