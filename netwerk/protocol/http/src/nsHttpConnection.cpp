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

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

//-----------------------------------------------------------------------------
// nsHttpConnection <public>
//-----------------------------------------------------------------------------

nsHttpConnection::nsHttpConnection()
    : mTransaction(0)
    , mConnectionInfo(0)
    , mLock(nsnull)
    , mReuseCount(0)
    , mMaxReuseCount(0)
    , mIdleTimeout(0)
    , mLastActiveTime(0)
    , mKeepAlive(0)
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
nsHttpConnection::Init(nsHttpConnectionInfo *info)
{
    LOG(("nsHttpConnection::Init [this=%x]\n", this));

    NS_ENSURE_ARG_POINTER(info);
    NS_ENSURE_TRUE(!mConnectionInfo, NS_ERROR_ALREADY_INITIALIZED);

    mLock = PR_NewLock();
    if (!mLock)
        return NS_ERROR_OUT_OF_MEMORY;

    mConnectionInfo = info;
    NS_ADDREF(mConnectionInfo);

    return NS_OK;
}

// called from any thread, with the connection lock held
nsresult
nsHttpConnection::SetTransaction(nsHttpTransaction *transaction)
{
    LOG(("nsHttpConnection::SetTransaction [this=%x trans=%x]\n",
         this, transaction));

    NS_ENSURE_TRUE(!mTransaction, NS_ERROR_IN_PROGRESS);
    NS_ENSURE_ARG_POINTER(transaction);

    // take ownership of the transaction
    mTransaction = transaction;
    NS_ADDREF(mTransaction);

    // build a proxy for the progress event sink
    mProgressSink = 0;
    if (mTransaction->Callbacks() && mTransaction->ConsumerEventQ()) {
        nsCOMPtr<nsIProgressEventSink> temp = do_GetInterface(mTransaction->Callbacks());
        if (temp) {
            nsCOMPtr<nsIProxyObjectManager> mgr;
            nsHttpHandler::get()->GetProxyObjectManager(getter_AddRefs(mgr));
            if (mgr)
                mgr->GetProxyForObject(mTransaction->ConsumerEventQ(),
                                       NS_GET_IID(nsIProgressEventSink),
                                       temp,
                                       PROXY_ASYNC | PROXY_ALWAYS,
                                       getter_AddRefs(mProgressSink));
        }
    }
 
    return ActivateConnection();
}

// called from the socket thread
nsresult
nsHttpConnection::OnHeadersAvailable(nsHttpTransaction *trans, PRBool *reset)
{
    LOG(("nsHttpConnection::OnHeadersAvailable [this=%x trans=%x]\n",
        this, trans));

    NS_ENSURE_ARG_POINTER(trans);

    // we won't change our keep-alive policy unless the server has explicitly
    // told us to do so.

    if (!trans || !trans->ResponseHead()) {
        LOG(("trans->ResponseHead() = %x\n", trans));
        return NS_OK;
    }

    // inspect the connection headers for keep-alive info provided the
    // transaction completed successfully.
    const char *val = trans->ResponseHead()->PeekHeader(nsHttp::Connection);
    if (!val)
        val = trans->ResponseHead()->PeekHeader(nsHttp::Proxy_Connection);

    if ((trans->ResponseHead()->Version() < NS_HTTP_VERSION_1_1) ||
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

    // if this connection is persistent, then the server may send a "Keep-Alive"
    // header specifying the maximum number of times the connection can be
    // reused as well as the maximum amount of time the connection can be idle
    // before the server will close it.  If this header is not present, then we
    // pick a suitably large number. Technically, any number > 0 will do, since
    // we reset this each time, and with HTTP/1.1 connections continue until we
    // get a {Proxy-,}Connection: close header. Don't just use 1 though, because
    // of pipelining
    if (mKeepAlive) {
        val = trans->ResponseHead()->PeekHeader(nsHttp::Keep_Alive);

        LOG(("val = [%s]\n", val));

        const char *cp = PL_strcasestr(val, "max=");
        if (cp)
            mMaxReuseCount = (PRUint32) atoi(cp + 4);
        else
            mMaxReuseCount = 100;

        cp = PL_strcasestr(val, "timeout=");
        if (cp)
            mIdleTimeout = (PRUint32) atoi(cp + 8);
        else
            mIdleTimeout = nsHttpHandler::get()->IdleTimeout();
        
        LOG(("Connection can be reused [this=%x max-reuse=%u "
             "keep-alive-timeout=%u\n", this, mMaxReuseCount, mIdleTimeout));
    }

    // if we're doing an SSL proxy connect, then we need to check whether or not
    // the connect was successful.  if so, then we have to reset the transaction
    // and step-up the socket connection to SSL. finally, we have to wake up the
    // socket write request.
    if (mSSLProxyConnectStream) {
        mSSLProxyConnectStream = 0;
        if (trans->ResponseHead()->Status() == 200) {
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
nsHttpConnection::OnTransactionComplete(nsHttpTransaction *trans, nsresult status)
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
nsHttpConnection::Suspend()
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
nsHttpConnection::Resume()
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

// called from the socket thread
nsresult
nsHttpConnection::ProxyStepUp()
{
    nsCOMPtr<nsISupports> securityInfo;
    nsresult rv;

    LOG(("nsHttpConnection::ProxyStepUp [this=%x]\n", this));

    rv = mSocketTransport->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISSLSocketControl> ssl = do_QueryInterface(securityInfo, &rv);
    if (NS_FAILED(rv)) return rv;

    return ssl->ProxyStepUp();
}

PRBool
nsHttpConnection::CanReuse()
{
    return mKeepAlive && (mReuseCount < mMaxReuseCount) && 
           (NowInSeconds() - mLastActiveTime < mIdleTimeout) && IsAlive();
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

// called on the socket thread
void
nsHttpConnection::DropTransaction()
{
    // the assertion here is that the transaction will not be destroyed
    // by this release.  we unfortunately don't have a threadsafe way of
    // asserting this.
    NS_IF_RELEASE(mTransaction);
    mTransaction = 0;
    mProgressSink = 0;
    
    // if the transaction was dropped, then we cannot reuse this connection.
    mKeepAlive = PR_FALSE;
}

// called on the socket thread
void
nsHttpConnection::ReportProgress(PRUint32 progress, PRInt32 progressMax)
{
    if (mProgressSink)
        mProgressSink->OnProgress(nsnull, nsnull, progress,
                                  progressMax < 0 ? 0 : PRUint32(progressMax));
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
        // unless its SOCKS.
        if (mConnectionInfo->UsingSSL() &&
            mConnectionInfo->ProxyHost() &&
            PL_strcmp(mConnectionInfo->ProxyType(), "socks") &&
            PL_strcmp(mConnectionInfo->ProxyType(), "socks4")) {
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
    nsHttpTransaction *trans = mTransaction;
    NS_ADDREF(trans);
    
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
    const char *types[3] = {0,0,0};
    PRUint32 count = 0;

    if (!PL_strcasecmp(mConnectionInfo->ProxyType(), "socks")) {
        types[count] = "socks";
        count++;
    }

    if (!PL_strcasecmp(mConnectionInfo->ProxyType(), "socks4")) {
        types[count] = "socks4";
        count++;
    }

    if (mConnectionInfo->UsingSSL()) {
        types[count] = "ssl";
        count++;
    }

    nsCOMPtr<nsITransport> transport;
    rv = sts->CreateTransportOfTypes(count, types,
                                     mConnectionInfo->Host(),
                                     mConnectionInfo->Port(),
                                     mConnectionInfo->ProxyHost(),
                                     mConnectionInfo->ProxyPort(),
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
    
    val = mTransaction->RequestHead()->PeekHeader(nsHttp::Host);
    if (val) {
        // all HTTP/1.1 requests must include a Host header (even though it
        // may seem redundant in this case; see bug 82388).
        request.SetHeader(nsHttp::Host, val);
    }

    val = mTransaction->RequestHead()->PeekHeader(nsHttp::Proxy_Authorization);
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

    if (mTransaction && ctxt) { // do this only once
        nsCOMPtr<nsISupports> info;
        mSocketTransport->GetSecurityInfo(getter_AddRefs(info));
        mTransaction->SetSecurityInfo(info);
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

        nsCOMPtr<nsISupports> info;
        mSocketTransport->GetSecurityInfo(getter_AddRefs(info));
        nsCOMPtr<nsISSLSocketControl> secCtrl(do_QueryInterface(info));

        // break the cycle between the security info and this
        if (secCtrl)
          secCtrl->SetNotificationCallbacks(nsnull);

        // break the cycle between the socket transport and this
        mSocketTransport->SetNotificationCallbacks(nsnull, 0);
        
        // don't need this anymore
        mProgressSink = 0;

        // make sure mTransaction is clear before calling OnStopTransaction
        if (mTransaction) { 
            nsHttpTransaction *trans = mTransaction;
            mTransaction = nsnull;

            trans->OnStopTransaction(status);
            NS_RELEASE(trans);
        }

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
    if (!mTransaction) {
        LOG(("nsHttpConnection: no transaction! closing stream\n"));
        return NS_BASE_STREAM_CLOSED;
    }

    mLastActiveTime = NowInSeconds();

    LOG(("nsHttpConnection::OnDataAvailable [this=%x]\n", this));

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

    if (mTransaction && mTransaction->Callbacks())
        return mTransaction->Callbacks()->GetInterface(iid, result);

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
    if (mProgressSink)
        mProgressSink->OnStatus(nsnull, nsnull, status, statusText);

    return NS_OK;
}

NS_IMETHODIMP
nsHttpConnection::OnProgress(nsIRequest *req, nsISupports *ctx,
                             PRUint32 progress, PRUint32 progressMax)
{
    // we ignore progress notifications from the socket transport.
    // we'll generate these ourselves from OnDataAvailable
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpConnectionInfo::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS0(nsHttpConnectionInfo)
