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

#include "nsHttpConnection.h"
#include "nsHttpTransaction.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsHttpHandler.h"
#include "nsIOService.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIServiceManager.h"
#include "nsISSLSocketControl.h"
#include "nsStringStream.h"
#include "netCore.h"
#include "nsNetCID.h"
#include "nsAutoLock.h"
#include "prmem.h"

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

//-----------------------------------------------------------------------------
// nsHttpConnection <public>
//-----------------------------------------------------------------------------

nsHttpConnection::nsHttpConnection()
    : mTransaction(nsnull)
    , mConnInfo(nsnull)
    , mLock(nsnull)
    , mLastReadTime(0)
    , mIdleTimeout(0)
    , mKeepAlive(PR_TRUE) // assume to keep-alive by default
    , mKeepAliveMask(PR_TRUE)
    , mSupportsPipelining(PR_FALSE) // assume low-grade server
    , mIsReused(PR_FALSE)
    , mCompletedSSLConnect(PR_FALSE)
{
    LOG(("Creating nsHttpConnection @%x\n", this));

    // grab a reference to the handler to ensure that it doesn't go away.
    nsHttpHandler *handler = gHttpHandler;
    NS_ADDREF(handler);
}

nsHttpConnection::~nsHttpConnection()
{
    LOG(("Destroying nsHttpConnection @%x\n", this));
 
    NS_IF_RELEASE(mConnInfo);
    NS_IF_RELEASE(mTransaction);

    if (mLock) {
        PR_DestroyLock(mLock);
        mLock = nsnull;
    }

    // release our reference to the handler
    nsHttpHandler *handler = gHttpHandler;
    NS_RELEASE(handler);
}

nsresult
nsHttpConnection::Init(nsHttpConnectionInfo *info, PRUint16 maxHangTime)
{
    LOG(("nsHttpConnection::Init [this=%x]\n", this));

    NS_ENSURE_ARG_POINTER(info);
    NS_ENSURE_TRUE(!mConnInfo, NS_ERROR_ALREADY_INITIALIZED);

    mLock = PR_NewLock();
    if (!mLock)
        return NS_ERROR_OUT_OF_MEMORY;

    mConnInfo = info;
    NS_ADDREF(mConnInfo);

    mMaxHangTime = maxHangTime;
    mLastReadTime = NowInSeconds();
    return NS_OK;
}

// called on the socket thread
nsresult
nsHttpConnection::Activate(nsAHttpTransaction *trans, PRUint8 caps)
{
    nsresult rv;

    LOG(("nsHttpConnection::Activate [this=%x trans=%x caps=%x]\n",
         this, trans, caps));

    NS_ENSURE_ARG_POINTER(trans);
    NS_ENSURE_TRUE(!mTransaction, NS_ERROR_IN_PROGRESS);

    // take ownership of the transaction
    mTransaction = trans;
    NS_ADDREF(mTransaction);

    // set mKeepAlive according to what will be requested
    mKeepAliveMask = mKeepAlive = (caps & NS_HTTP_ALLOW_KEEPALIVE);

    // if we don't have a socket transport then create a new one
    if (!mSocketTransport) {
        rv = CreateTransport(caps);
        if (NS_FAILED(rv))
            goto loser;
    }

    // need to handle SSL proxy CONNECT if this is the first time.
    if (mConnInfo->UsingSSL() && mConnInfo->UsingHttpProxy() && !mCompletedSSLConnect) {
        rv = SetupSSLProxyConnect();
        if (NS_FAILED(rv))
            goto loser;
    }

    // wait for the output stream to be readable
    rv = mSocketOut->AsyncWait(this, 0, 0, nsnull);
    if (NS_SUCCEEDED(rv))
        return rv;

loser:
    NS_RELEASE(mTransaction);
    return rv;
}

void
nsHttpConnection::Close(nsresult reason)
{
    LOG(("nsHttpConnection::Close [this=%x reason=%x]\n", this, reason));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (NS_FAILED(reason)) {
        if (mSocketTransport) {
            mSocketTransport->SetSecurityCallbacks(nsnull);
            mSocketTransport->SetEventSink(nsnull, nsnull);
            mSocketTransport->Close(reason);
        }
        mKeepAlive = PR_FALSE;
    }
}

// called on the socket thread
nsresult
nsHttpConnection::ProxyStartSSL()
{
    LOG(("nsHttpConnection::ProxyStartSSL [this=%x]\n", this));
#ifdef DEBUG
    NS_PRECONDITION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
#endif

    nsCOMPtr<nsISupports> securityInfo;
    nsresult rv = mSocketTransport->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISSLSocketControl> ssl = do_QueryInterface(securityInfo, &rv);
    if (NS_FAILED(rv)) return rv;

    return ssl->ProxyStartSSL();
}

PRBool
nsHttpConnection::CanReuse()
{
    return IsKeepAlive() && (NowInSeconds() - mLastReadTime < mIdleTimeout)
                         && IsAlive();
}

PRUint32 nsHttpConnection::TimeToLive()
{
    PRInt32 tmp = mIdleTimeout - (NowInSeconds() - mLastReadTime);
    if (0 > tmp)
        tmp = 0;

    return tmp;
}

PRBool
nsHttpConnection::IsAlive()
{
    if (!mSocketTransport)
        return PR_FALSE;

    PRBool alive;
    nsresult rv = mSocketTransport->IsAlive(&alive);
    if (NS_FAILED(rv))
        alive = PR_FALSE;

//#define TEST_RESTART_LOGIC
#ifdef TEST_RESTART_LOGIC
    if (!alive) {
        LOG(("pretending socket is still alive to test restart logic\n"));
        alive = PR_TRUE;
    }
#endif

    return alive;
}

PRBool
nsHttpConnection::SupportsPipelining(nsHttpResponseHead *responseHead)
{
    // XXX there should be a strict mode available that disables this
    // blacklisting.

    // assuming connection is HTTP/1.1 with keep-alive enabled
    if (mConnInfo->UsingHttpProxy() && !mConnInfo->UsingSSL()) {
        // XXX check for bad proxy servers...
        return PR_TRUE;
    }

    // XXX what about checking for a Via header? (transparent proxies)

    // check for bad origin servers
    const char *val = responseHead->PeekHeader(nsHttp::Server);
    if (!val)
        return PR_FALSE; // no header, no love

    // The blacklist is indexed by the first character. All of these servers are
    // known to return their identifier as the first thing in the server string,
    // so we can do a leading match. 

    static const char *bad_servers[26][5] = {
        { nsnull }, { nsnull }, { nsnull }, { nsnull },                 // a - d
        { "EFAServer/", nsnull },                                       // e
        { nsnull }, { nsnull }, { nsnull }, { nsnull },                 // f - i
        { nsnull }, { nsnull }, { nsnull },                             // j - l 
        { "Microsoft-IIS/4.", "Microsoft-IIS/5.", nsnull },             // m
        { "Netscape-Enterprise/3.", "Netscape-Enterprise/4.", 
          "Netscape-Enterprise/5.", "Netscape-Enterprise/6.", nsnull }, // n
        { nsnull }, { nsnull }, { nsnull }, { nsnull },                 // o - r
        { nsnull }, { nsnull }, { nsnull }, { nsnull },                 // s - v
        { "WebLogic 3.", "WebLogic 4.","WebLogic 5.", "WebLogic 6.", nsnull }, // w 
        { nsnull }, { nsnull }, { nsnull }                              // x - z
    };  

    int index = val[0] - 'A'; // the whole table begins with capital letters
    if ((index >= 0) && (index <= 25))
    {
        for (int i = 0; bad_servers[index][i] != nsnull; i++) {
            if (!PL_strncmp (val, bad_servers[index][i], strlen (bad_servers[index][i]))) {
                LOG(("looks like this server does not support pipelining"));
                return PR_FALSE;
            }
        }
    }

    // ok, let's allow pipelining to this server
    return PR_TRUE;
}

//----------------------------------------------------------------------------
// nsHttpConnection::nsAHttpConnection compatible methods
//----------------------------------------------------------------------------

nsresult
nsHttpConnection::OnHeadersAvailable(nsAHttpTransaction *trans,
                                     nsHttpRequestHead *requestHead,
                                     nsHttpResponseHead *responseHead,
                                     PRBool *reset)
{
    LOG(("nsHttpConnection::OnHeadersAvailable [this=%p trans=%p response-head=%p]\n",
        this, trans, responseHead));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");
    NS_ENSURE_ARG_POINTER(trans);
    NS_ASSERTION(responseHead, "No response head?");

    // If the server issued an explicit timeout, then we need to close down the
    // socket transport.  We pass an error code of NS_ERROR_NET_RESET to
    // trigger the transactions 'restart' mechanism.  We tell it to reset its
    // response headers so that it will be ready to receive the new response.
    if (responseHead->Status() == 408) {
        Close(NS_ERROR_NET_RESET);
        *reset = PR_TRUE;
        return NS_OK;
    }

    // we won't change our keep-alive policy unless the server has explicitly
    // told us to do so.

    // inspect the connection headers for keep-alive info provided the
    // transaction completed successfully.
    const char *val = responseHead->PeekHeader(nsHttp::Connection);
    if (!val)
        val = responseHead->PeekHeader(nsHttp::Proxy_Connection);

    // reset to default (the server may have changed since we last checked)
    mSupportsPipelining = PR_FALSE;

    if ((responseHead->Version() < NS_HTTP_VERSION_1_1) ||
        (requestHead->Version() < NS_HTTP_VERSION_1_1)) {
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
        else {
            mKeepAlive = PR_TRUE;

            // Do not support pipelining when we are establishing
            // an SSL tunnel though an HTTP proxy. Pipelining support
            // determination must be based on comunication with the
            // target server in this case. See bug 422016 for futher
            // details.
            if (!mSSLProxyConnectStream)
              mSupportsPipelining = SupportsPipelining(responseHead);
        }
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
            mIdleTimeout = gHttpHandler->IdleTimeout();
        
        LOG(("Connection can be reused [this=%x idle-timeout=%u]\n", this, mIdleTimeout));
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
            nsresult rv = ProxyStartSSL();
            if (NS_FAILED(rv)) // XXX need to handle this for real
                LOG(("ProxyStartSSL failed [rv=%x]\n", rv));
            mCompletedSSLConnect = PR_TRUE;
            rv = mSocketOut->AsyncWait(this, 0, 0, nsnull);
            // XXX what if this fails -- need to handle this error
            NS_ASSERTION(NS_SUCCEEDED(rv), "mSocketOut->AsyncWait failed");
        }
        else {
            LOG(("SSL proxy CONNECT failed!\n"));
            // NOTE: this cast is valid since this connection cannot be
            // processing a transaction pipeline until after the first HTTP/1.1
            // response.
            nsHttpTransaction *trans =
                    static_cast<nsHttpTransaction *>(mTransaction);
            trans->SetSSLConnectFailed();
        }
    }

    return NS_OK;
}

void
nsHttpConnection::GetSecurityInfo(nsISupports **secinfo)
{
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (mSocketTransport) {
        if (NS_FAILED(mSocketTransport->GetSecurityInfo(secinfo)))
            *secinfo = nsnull;
    }
}

nsresult
nsHttpConnection::ResumeSend()
{
    LOG(("nsHttpConnection::ResumeSend [this=%p]\n", this));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (mSocketOut)
        return mSocketOut->AsyncWait(this, 0, 0, nsnull);

    NS_NOTREACHED("no socket output stream");
    return NS_ERROR_UNEXPECTED;
}

nsresult
nsHttpConnection::ResumeRecv()
{
    LOG(("nsHttpConnection::ResumeRecv [this=%p]\n", this));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    if (mSocketIn)
        return mSocketIn->AsyncWait(this, 0, 0, nsnull);

    NS_NOTREACHED("no socket input stream");
    return NS_ERROR_UNEXPECTED;
}

//-----------------------------------------------------------------------------
// nsHttpConnection <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpConnection::CreateTransport(PRUint8 caps)
{
    nsresult rv;

    NS_PRECONDITION(!mSocketTransport, "unexpected");

    nsCOMPtr<nsISocketTransportService> sts =
            do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    // configure the socket type based on the connection type requested.
    const char* types[1];

    if (mConnInfo->UsingSSL())
        types[0] = "ssl";
    else
        types[0] = gHttpHandler->DefaultSocketType();

    nsCOMPtr<nsISocketTransport> strans;
    PRUint32 typeCount = (types[0] != nsnull);

    rv = sts->CreateTransport(types, typeCount,
                              nsDependentCString(mConnInfo->Host()),
                              mConnInfo->Port(),
                              mConnInfo->ProxyInfo(),
                              getter_AddRefs(strans));
    if (NS_FAILED(rv)) return rv;

    PRUint32 tmpFlags = 0;
    if (caps & NS_HTTP_REFRESH_DNS)
        tmpFlags = nsISocketTransport::BYPASS_CACHE;
    
    if (caps & NS_HTTP_LOAD_ANONYMOUS)
        tmpFlags |= nsISocketTransport::ANONYMOUS_CONNECT;
    
    strans->SetConnectionFlags(tmpFlags); 

    strans->SetQoSBits(gHttpHandler->GetQoSBits());

    // NOTE: these create cyclical references, which we break inside
    //       nsHttpConnection::Close
    rv = strans->SetEventSink(this, nsnull);
    if (NS_FAILED(rv)) return rv;
    rv = strans->SetSecurityCallbacks(this);
    if (NS_FAILED(rv)) return rv;

    // next open the socket streams
    nsCOMPtr<nsIOutputStream> sout;
    rv = strans->OpenOutputStream(nsITransport::OPEN_UNBUFFERED, 0, 0,
                                  getter_AddRefs(sout));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIInputStream> sin;
    rv = strans->OpenInputStream(nsITransport::OPEN_UNBUFFERED, 0, 0,
                                 getter_AddRefs(sin));
    if (NS_FAILED(rv)) return rv;

    mSocketTransport = strans;
    mSocketIn = do_QueryInterface(sin);
    mSocketOut = do_QueryInterface(sout);
    return NS_OK;
}

void
nsHttpConnection::CloseTransaction(nsAHttpTransaction *trans, nsresult reason)
{
    LOG(("nsHttpConnection::CloseTransaction[this=%x trans=%x reason=%x]\n",
        this, trans, reason));

    NS_ASSERTION(trans == mTransaction, "wrong transaction");
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    // mask this error code because its not a real error.
    if (reason == NS_BASE_STREAM_CLOSED)
        reason = NS_OK;

    mTransaction->Close(reason);

    NS_RELEASE(mTransaction);
    mTransaction = 0;

    if (NS_FAILED(reason))
        Close(reason);

    // flag the connection as reused here for convenience sake.  certainly
    // it might be going away instead ;-)
    mIsReused = PR_TRUE;
}

NS_METHOD
nsHttpConnection::ReadFromStream(nsIInputStream *input,
                                 void *closure,
                                 const char *buf,
                                 PRUint32 offset,
                                 PRUint32 count,
                                 PRUint32 *countRead)
{
    // thunk for nsIInputStream instance
    nsHttpConnection *conn = (nsHttpConnection *) closure;
    return conn->OnReadSegment(buf, count, countRead);
}

nsresult
nsHttpConnection::OnReadSegment(const char *buf,
                                PRUint32 count,
                                PRUint32 *countRead)
{
    if (count == 0) {
        // some ReadSegments implementations will erroneously call the writer
        // to consume 0 bytes worth of data.  we must protect against this case
        // or else we'd end up closing the socket prematurely.
        NS_ERROR("bad ReadSegments implementation");
        return NS_ERROR_FAILURE; // stop iterating
    }

    nsresult rv = mSocketOut->Write(buf, count, countRead);
    if (NS_FAILED(rv))
        mSocketOutCondition = rv;
    else if (*countRead == 0)
        mSocketOutCondition = NS_BASE_STREAM_CLOSED;
    else
        mSocketOutCondition = NS_OK; // reset condition

    return mSocketOutCondition;
}

nsresult
nsHttpConnection::OnSocketWritable()
{
    LOG(("nsHttpConnection::OnSocketWritable [this=%x]\n", this));

    nsresult rv;
    PRUint32 n;
    PRBool again = PR_TRUE;

    do {
        // if we're doing an SSL proxy connect, then we need to bypass calling
        // into the transaction.
        //
        // NOTE: this code path can't be shared since the transaction doesn't
        // implement nsIInputStream.  doing so is not worth the added cost of
        // extra indirections during normal reading.
        //
        if (mSSLProxyConnectStream) {
            LOG(("  writing CONNECT request stream\n"));
            rv = mSSLProxyConnectStream->ReadSegments(ReadFromStream, this,
                                                      nsIOService::gDefaultSegmentSize,
                                                      &n);
        }
        else {
            LOG(("  writing transaction request stream\n"));
            rv = mTransaction->ReadSegments(this, nsIOService::gDefaultSegmentSize, &n);
        }

        LOG(("  ReadSegments returned [rv=%x read=%u sock-cond=%x]\n",
            rv, n, mSocketOutCondition));

        // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
        if (rv == NS_BASE_STREAM_CLOSED) {
            rv = NS_OK;
            n = 0;
        }

        if (NS_FAILED(rv)) {
            // if the transaction didn't want to write any more data, then
            // wait for the transaction to call ResumeSend.
            if (rv == NS_BASE_STREAM_WOULD_BLOCK)
                rv = NS_OK;
            again = PR_FALSE;
        }
        else if (NS_FAILED(mSocketOutCondition)) {
            if (mSocketOutCondition == NS_BASE_STREAM_WOULD_BLOCK)
                rv = mSocketOut->AsyncWait(this, 0, 0, nsnull); // continue writing
            else
                rv = mSocketOutCondition;
            again = PR_FALSE;
        }
        else if (n == 0) {
            // 
            // at this point we've written out the entire transaction, and now we
            // must wait for the server's response.  we manufacture a status message
            // here to reflect the fact that we are waiting.  this message will be
            // trumped (overwritten) if the server responds quickly.
            //
            mTransaction->OnTransportStatus(nsISocketTransport::STATUS_WAITING_FOR,
                                            LL_ZERO);

            rv = mSocketIn->AsyncWait(this, 0, 0, nsnull); // start reading
            again = PR_FALSE;
        }
        // write more to the socket until error or end-of-request...
    } while (again);

    return rv;
}

nsresult
nsHttpConnection::OnWriteSegment(char *buf,
                                 PRUint32 count,
                                 PRUint32 *countWritten)
{
    if (count == 0) {
        // some WriteSegments implementations will erroneously call the reader
        // to provide 0 bytes worth of data.  we must protect against this case
        // or else we'd end up closing the socket prematurely.
        NS_ERROR("bad WriteSegments implementation");
        return NS_ERROR_FAILURE; // stop iterating
    }

    nsresult rv = mSocketIn->Read(buf, count, countWritten);
    if (NS_FAILED(rv))
        mSocketInCondition = rv;
    else if (*countWritten == 0)
        mSocketInCondition = NS_BASE_STREAM_CLOSED;
    else
        mSocketInCondition = NS_OK; // reset condition

    return mSocketInCondition;
}

nsresult
nsHttpConnection::OnSocketReadable()
{
    LOG(("nsHttpConnection::OnSocketReadable [this=%x]\n", this));

    PRUint32 now = NowInSeconds();

    if (mKeepAliveMask && (now - mLastReadTime >= PRUint32(mMaxHangTime))) {
        LOG(("max hang time exceeded!\n"));
        // give the handler a chance to create a new persistent connection to
        // this host if we've been busy for too long.
        mKeepAliveMask = PR_FALSE;
        gHttpHandler->ProcessPendingQ(mConnInfo);
    }
    mLastReadTime = now;

    nsresult rv;
    PRUint32 n;
    PRBool again = PR_TRUE;

    do {
        rv = mTransaction->WriteSegments(this, nsIOService::gDefaultSegmentSize, &n);
        if (NS_FAILED(rv)) {
            // if the transaction didn't want to take any more data, then
            // wait for the transaction to call ResumeRecv.
            if (rv == NS_BASE_STREAM_WOULD_BLOCK)
                rv = NS_OK;
            again = PR_FALSE;
        }
        else if (NS_FAILED(mSocketInCondition)) {
            // continue waiting for the socket if necessary...
            if (mSocketInCondition == NS_BASE_STREAM_WOULD_BLOCK)
                rv = mSocketIn->AsyncWait(this, 0, 0, nsnull);
            else
                rv = mSocketInCondition;
            again = PR_FALSE;
        }
        // read more from the socket until error...
    } while (again);

    return rv;
}

nsresult
nsHttpConnection::SetupSSLProxyConnect()
{
    const char *val;

    LOG(("nsHttpConnection::SetupSSLProxyConnect [this=%x]\n", this));

    NS_ENSURE_TRUE(!mSSLProxyConnectStream, NS_ERROR_ALREADY_INITIALIZED);

    nsCAutoString buf;
    nsresult rv = nsHttpHandler::GenerateHostPort(
            nsDependentCString(mConnInfo->Host()), mConnInfo->Port(), buf);
    if (NS_FAILED(rv))
        return rv;

    // CONNECT host:port HTTP/1.1
    nsHttpRequestHead request;
    request.SetMethod(nsHttp::Connect);
    request.SetVersion(gHttpHandler->HttpVersion());
    request.SetRequestURI(buf);
    request.SetHeader(nsHttp::User_Agent, gHttpHandler->UserAgent());

    // send this header for backwards compatibility.
    request.SetHeader(nsHttp::Proxy_Connection, NS_LITERAL_CSTRING("keep-alive"));

    // NOTE: this cast is valid since this connection cannot be processing a
    // transaction pipeline until after the first HTTP/1.1 response.
    nsHttpTransaction *trans = static_cast<nsHttpTransaction *>(mTransaction);
    
    val = trans->RequestHead()->PeekHeader(nsHttp::Host);
    if (val) {
        // all HTTP/1.1 requests must include a Host header (even though it
        // may seem redundant in this case; see bug 82388).
        request.SetHeader(nsHttp::Host, nsDependentCString(val));
    }

    val = trans->RequestHead()->PeekHeader(nsHttp::Proxy_Authorization);
    if (val) {
        // we don't know for sure if this authorization is intended for the
        // SSL proxy, so we add it just in case.
        request.SetHeader(nsHttp::Proxy_Authorization, nsDependentCString(val));
    }

    buf.Truncate();
    request.Flatten(buf, PR_FALSE);
    buf.AppendLiteral("\r\n");

    return NS_NewCStringInputStream(getter_AddRefs(mSSLProxyConnectStream), buf);
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS4(nsHttpConnection,
                              nsIInputStreamCallback,
                              nsIOutputStreamCallback,
                              nsITransportEventSink,
                              nsIInterfaceRequestor)

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIInputStreamCallback
//-----------------------------------------------------------------------------

// called on the socket transport thread
NS_IMETHODIMP
nsHttpConnection::OnInputStreamReady(nsIAsyncInputStream *in)
{
    NS_ASSERTION(in == mSocketIn, "unexpected stream");
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    // if the transaction was dropped...
    if (!mTransaction) {
        LOG(("  no transaction; ignoring event\n"));
        return NS_OK;
    }

    nsresult rv = OnSocketReadable();
    if (NS_FAILED(rv))
        CloseTransaction(mTransaction, rv);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIOutputStreamCallback
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpConnection::OnOutputStreamReady(nsIAsyncOutputStream *out)
{
    NS_ASSERTION(out == mSocketOut, "unexpected stream");
    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    // if the transaction was dropped...
    if (!mTransaction) {
        LOG(("  no transaction; ignoring event\n"));
        return NS_OK;
    }

    nsresult rv = OnSocketWritable();
    if (NS_FAILED(rv))
        CloseTransaction(mTransaction, rv);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsITransportEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpConnection::OnTransportStatus(nsITransport *trans,
                                    nsresult status,
                                    PRUint64 progress,
                                    PRUint64 progressMax)
{
    if (mTransaction)
        mTransaction->OnTransportStatus(status, progress);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpConnection::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

// not called on the socket transport thread
NS_IMETHODIMP
nsHttpConnection::GetInterface(const nsIID &iid, void **result)
{
    // NOTE: This function is only called on the UI thread via sync proxy from
    //       the socket transport thread.  If that weren't the case, then we'd
    //       have to worry about the possibility of mTransaction going away
    //       part-way through this function call.  See CloseTransaction.
    NS_ASSERTION(PR_GetCurrentThread() != gSocketThread, "wrong thread");
 
    if (mTransaction) {
        nsCOMPtr<nsIInterfaceRequestor> callbacks;
        mTransaction->GetSecurityCallbacks(getter_AddRefs(callbacks));
        if (callbacks)
            return callbacks->GetInterface(iid, result);
    }

    return NS_ERROR_NO_INTERFACE;
}
