/* vim:set ts=4 sw=4 et cindent: */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif

#include "nsSocketTransport2.h"
#include "nsIOService.h"
#include "nsStreamUtils.h"
#include "nsNetSegmentUtils.h"
#include "nsTransportUtils.h"
#include "nsNetCID.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "netCore.h"
#include "prmem.h"
#include "pratom.h"
#include "plstr.h"
#include "prnetdb.h"
#include "prerror.h"
#include "prerr.h"

#include "nsIServiceManager.h"
#include "nsIProxyObjectManager.h"
#include "nsISocketProviderService.h"
#include "nsISocketProvider.h"
#include "nsISSLSocketControl.h"
#include "nsIProxyInfo.h"
#include "nsIPipe.h"

#if defined(XP_WIN)
#include "nsNativeConnectionHelper.h"
#endif

//-----------------------------------------------------------------------------

static NS_DEFINE_CID(kSocketProviderServiceCID, NS_SOCKETPROVIDERSERVICE_CID);
static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);

//-----------------------------------------------------------------------------

class nsSocketEvent : public PLEvent
{
public:
    nsSocketEvent(nsSocketTransport *transport, PRUint32 type,
                  nsresult status = NS_OK, nsISupports *param = nsnull)
        : mType(type)
        , mStatus(status)
        , mParam(param)
    {
        NS_ADDREF(transport);
        PL_InitEvent(this, transport, HandleEvent, DestroyEvent);
    }

    PR_STATIC_CALLBACK(void*)
    HandleEvent(PLEvent *event)
    {
        nsSocketTransport *trans = (nsSocketTransport *) event->owner; 
        nsSocketEvent *se = (nsSocketEvent *) event;
        trans->OnSocketEvent(se->mType, se->mStatus, se->mParam);
        return nsnull;
    }

    PR_STATIC_CALLBACK(void)
    DestroyEvent(PLEvent *event)
    {
        nsSocketTransport *trans = (nsSocketTransport *) event->owner; 
        NS_RELEASE(trans);
        delete (nsSocketEvent *) event;
    }

private:
    PRUint32              mType;
    nsresult              mStatus;
    nsCOMPtr<nsISupports> mParam;
};

//-----------------------------------------------------------------------------

//#define TEST_CONNECT_ERRORS
#ifdef TEST_CONNECT_ERRORS
#include <stdlib.h>
static PRErrorCode RandomizeConnectError(PRErrorCode code)
{
    //
    // To test out these errors, load http://www.yahoo.com/.  It should load
    // correctly despite the random occurance of these errors.
    //
    int n = rand();
    if (n > RAND_MAX/2) {
        struct {
            PRErrorCode err_code;
            const char *err_name;
        } 
        errors[] = {
            //
            // These errors should be recoverable provided there is another
            // IP address in mDNSRecord.
            //
            { PR_CONNECT_REFUSED_ERROR, "PR_CONNECT_REFUSED_ERROR" },
            { PR_CONNECT_TIMEOUT_ERROR, "PR_CONNECT_TIMEOUT_ERROR" },
            //
            // This error will cause this socket transport to error out;
            // however, if the consumer is HTTP, then the HTTP transaction
            // should be restarted when this error occurs.
            //
            { PR_CONNECT_RESET_ERROR, "PR_CONNECT_RESET_ERROR" },
        };
        n = n % (sizeof(errors)/sizeof(errors[0]));
        code = errors[n].err_code;
        LOG(("simulating NSPR error %d [%s]\n", code, errors[n].err_name));
    }
    return code;
}
#endif

//-----------------------------------------------------------------------------

static nsresult
ErrorAccordingToNSPR(PRErrorCode errorCode)
{
    nsresult rv;
    switch (errorCode) {
    case PR_WOULD_BLOCK_ERROR:
        rv = NS_BASE_STREAM_WOULD_BLOCK;
        break;
    case PR_CONNECT_ABORTED_ERROR:
    case PR_CONNECT_RESET_ERROR:
        rv = NS_ERROR_NET_RESET;
        break;
    case PR_END_OF_FILE_ERROR: // XXX document this correlation
        rv = NS_ERROR_NET_INTERRUPT;
        break;
    case PR_CONNECT_REFUSED_ERROR:
    case PR_NETWORK_UNREACHABLE_ERROR: // XXX need new nsresult for this!
    case PR_HOST_UNREACHABLE_ERROR:    // XXX and this!
    case PR_ADDRESS_NOT_AVAILABLE_ERROR:
        rv = NS_ERROR_CONNECTION_REFUSED;
        break;
    case PR_IO_TIMEOUT_ERROR:
    case PR_CONNECT_TIMEOUT_ERROR:
        rv = NS_ERROR_NET_TIMEOUT;
        break;
    default:
        rv = NS_ERROR_FAILURE;
    }
    LOG(("ErrorAccordingToNSPR [in=%d out=%x]\n", errorCode, rv));
    return rv;
}

//-----------------------------------------------------------------------------
// socket input stream impl 
//-----------------------------------------------------------------------------

nsSocketInputStream::nsSocketInputStream(nsSocketTransport *trans)
    : mTransport(trans)
    , mReaderRefCnt(0)
    , mCondition(NS_OK)
    , mCallbackFlags(0)
    , mByteCount(0)
{
}

nsSocketInputStream::~nsSocketInputStream()
{
}

// called on the socket transport thread...
//
//   condition : failure code if socket has been closed
//
void
nsSocketInputStream::OnSocketReady(nsresult condition)
{
    LOG(("nsSocketInputStream::OnSocketReady [this=%x cond=%x]\n",
        this, condition));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    nsCOMPtr<nsIInputStreamCallback> callback;
    {
        nsAutoLock lock(mTransport->mLock);

        // update condition, but be careful not to erase an already
        // existing error condition.
        if (NS_SUCCEEDED(mCondition))
            mCondition = condition;

        // ignore event if only waiting for closure and not closed.
        if (NS_FAILED(mCondition) || !(mCallbackFlags & WAIT_CLOSURE_ONLY)) {
            callback = mCallback;
            mCallback = nsnull;
            mCallbackFlags = 0;
        }
    }

    if (callback)
        callback->OnInputStreamReady(this);
}

NS_IMPL_QUERY_INTERFACE2(nsSocketInputStream,
                         nsIInputStream,
                         nsIAsyncInputStream)

NS_IMETHODIMP_(nsrefcnt)
nsSocketInputStream::AddRef()
{
    PR_AtomicIncrement((PRInt32*)&mReaderRefCnt);
    return mTransport->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
nsSocketInputStream::Release()
{
    if (PR_AtomicDecrement((PRInt32*)&mReaderRefCnt) == 0)
        Close();
    return mTransport->Release();
}

NS_IMETHODIMP
nsSocketInputStream::Close()
{
    return CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP
nsSocketInputStream::Available(PRUint32 *avail)
{
    LOG(("nsSocketInputStream::Available [this=%x]\n", this));

    *avail = 0;

    PRFileDesc *fd;
    {
        nsAutoLock lock(mTransport->mLock);

        if (NS_FAILED(mCondition))
            return mCondition;

        fd = mTransport->GetFD_Locked();
        if (!fd)
            return NS_BASE_STREAM_WOULD_BLOCK;
    }

    // cannot hold lock while calling NSPR.  (worried about the fact that PSM
    // synchronously proxies notifications over to the UI thread, which could
    // mistakenly try to re-enter this code.)
    PRInt32 n = PR_Available(fd);

    nsresult rv;
    {
        nsAutoLock lock(mTransport->mLock);

        mTransport->ReleaseFD_Locked(fd);

        if (n >= 0)
            *avail = n;
        else {
            PRErrorCode code = PR_GetError();
            if (code == PR_WOULD_BLOCK_ERROR)
                return NS_BASE_STREAM_WOULD_BLOCK;
            mCondition = ErrorAccordingToNSPR(code);
        }
        rv = mCondition;
    }
    if (NS_FAILED(rv))
        mTransport->OnInputClosed(rv);
    return rv;
}

NS_IMETHODIMP
nsSocketInputStream::Read(char *buf, PRUint32 count, PRUint32 *countRead)
{
    LOG(("nsSocketInputStream::Read [this=%x count=%u]\n", this, count));

    *countRead = 0;

    PRFileDesc *fd;
    {
        nsAutoLock lock(mTransport->mLock);

        if (NS_FAILED(mCondition))
            return (mCondition == NS_BASE_STREAM_CLOSED) ? NS_OK : mCondition;

        fd = mTransport->GetFD_Locked();
        if (!fd)
            return NS_BASE_STREAM_WOULD_BLOCK;
    }

    LOG(("  calling PR_Read [count=%u]\n", count));

    // cannot hold lock while calling NSPR.  (worried about the fact that PSM
    // synchronously proxies notifications over to the UI thread, which could
    // mistakenly try to re-enter this code.)
    PRInt32 n = PR_Read(fd, buf, count);

    LOG(("  PR_Read returned [n=%d]\n", n));

    nsresult rv;
    {
        nsAutoLock lock(mTransport->mLock);

#ifdef ENABLE_SOCKET_TRACING
        if (n > 0)
            mTransport->TraceInBuf(buf, n);
#endif

        mTransport->ReleaseFD_Locked(fd);

        if (n > 0)
            mByteCount += (*countRead = n);
        else if (n < 0) {
            PRErrorCode code = PR_GetError();
            if (code == PR_WOULD_BLOCK_ERROR)
                return NS_BASE_STREAM_WOULD_BLOCK;
            mCondition = ErrorAccordingToNSPR(code);
        }
        rv = mCondition;
    }
    if (NS_FAILED(rv))
        mTransport->OnInputClosed(rv);

    // only send this notification if we have indeed read some data.
    // see bug 196827 for an example of why this is important.
    if (n > 0)
        mTransport->SendStatus(nsISocketTransport::STATUS_RECEIVING_FROM);
    return rv;
}

NS_IMETHODIMP
nsSocketInputStream::ReadSegments(nsWriteSegmentFun writer, void *closure,
                                  PRUint32 count, PRUint32 *countRead)
{
    // socket stream is unbuffered
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketInputStream::IsNonBlocking(PRBool *nonblocking)
{
    *nonblocking = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketInputStream::CloseWithStatus(nsresult reason)
{
    LOG(("nsSocketInputStream::CloseWithStatus [this=%x reason=%x]\n", this, reason));

    // may be called from any thread
 
    nsresult rv;
    {
        nsAutoLock lock(mTransport->mLock);

        if (NS_SUCCEEDED(mCondition))
            rv = mCondition = reason;
        else
            rv = NS_OK;
    }
    if (NS_FAILED(rv))
        mTransport->OnInputClosed(rv);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketInputStream::AsyncWait(nsIInputStreamCallback *callback,
                               PRUint32 flags,
                               PRUint32 amount,
                               nsIEventTarget *target)
{
    LOG(("nsSocketInputStream::AsyncWait [this=%x]\n", this));

    {
        nsAutoLock lock(mTransport->mLock);

        if (target) {
            //
            // build event proxy
            //
            // failure to create an event proxy (most likely out of memory)
            // shouldn't alter the state of the transport.
            //
            nsCOMPtr<nsIInputStreamCallback> temp;
            nsresult rv = NS_NewInputStreamReadyEvent(getter_AddRefs(temp),
                                                      callback, target);
            if (NS_FAILED(rv)) return rv;
            mCallback = temp;
        }
        else
            mCallback = callback;

        mCallbackFlags = flags;
    }
    mTransport->OnInputPending();
    return NS_OK;
}

//-----------------------------------------------------------------------------
// socket output stream impl 
//-----------------------------------------------------------------------------

nsSocketOutputStream::nsSocketOutputStream(nsSocketTransport *trans)
    : mTransport(trans)
    , mWriterRefCnt(0)
    , mCondition(NS_OK)
    , mCallbackFlags(0)
    , mByteCount(0)
{
}

nsSocketOutputStream::~nsSocketOutputStream()
{
}

// called on the socket transport thread...
//
//   condition : failure code if socket has been closed
//
void
nsSocketOutputStream::OnSocketReady(nsresult condition)
{
    LOG(("nsSocketOutputStream::OnSocketReady [this=%x cond=%x]\n",
        this, condition));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    nsCOMPtr<nsIOutputStreamCallback> callback;
    {
        nsAutoLock lock(mTransport->mLock);

        // update condition, but be careful not to erase an already
        // existing error condition.
        if (NS_SUCCEEDED(mCondition))
            mCondition = condition;

        // ignore event if only waiting for closure and not closed.
        if (NS_FAILED(mCondition) || !(mCallbackFlags & WAIT_CLOSURE_ONLY)) {
            callback = mCallback;
            mCallback = nsnull;
            mCallbackFlags = 0;
        }
    }

    if (callback)
        callback->OnOutputStreamReady(this);
}

NS_IMPL_QUERY_INTERFACE2(nsSocketOutputStream,
                         nsIOutputStream,
                         nsIAsyncOutputStream)

NS_IMETHODIMP_(nsrefcnt)
nsSocketOutputStream::AddRef()
{
    PR_AtomicIncrement((PRInt32*)&mWriterRefCnt);
    return mTransport->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
nsSocketOutputStream::Release()
{
    if (PR_AtomicDecrement((PRInt32*)&mWriterRefCnt) == 0)
        Close();
    return mTransport->Release();
}

NS_IMETHODIMP
nsSocketOutputStream::Close()
{
    return CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP
nsSocketOutputStream::Flush()
{
    return NS_OK;
}

NS_IMETHODIMP
nsSocketOutputStream::Write(const char *buf, PRUint32 count, PRUint32 *countWritten)
{
    LOG(("nsSocketOutputStream::Write [this=%x count=%u]\n", this, count));

    *countWritten = 0;

    if (count == 0)
        return NS_OK;

    PRFileDesc *fd;
    {
        nsAutoLock lock(mTransport->mLock);

        if (NS_FAILED(mCondition))
            return mCondition;
        
        fd = mTransport->GetFD_Locked();
        if (!fd)
            return NS_BASE_STREAM_WOULD_BLOCK;
    }

    LOG(("  calling PR_Write [count=%u]\n", count));

    // cannot hold lock while calling NSPR.  (worried about the fact that PSM
    // synchronously proxies notifications over to the UI thread, which could
    // mistakenly try to re-enter this code.)
    PRInt32 n = PR_Write(fd, buf, count);

    LOG(("  PR_Write returned [n=%d]\n", n));
    NS_ASSERTION(n != 0, "unexpected return value");

    nsresult rv;
    {
        nsAutoLock lock(mTransport->mLock);

#ifdef ENABLE_SOCKET_TRACING
    if (n > 0)
        mTransport->TraceOutBuf(buf, n);
#endif

        mTransport->ReleaseFD_Locked(fd);

        if (n > 0)
            mByteCount += (*countWritten = n);
        else if (n < 0) {
            PRErrorCode code = PR_GetError();
            if (code == PR_WOULD_BLOCK_ERROR)
                return NS_BASE_STREAM_WOULD_BLOCK;
            mCondition = ErrorAccordingToNSPR(code);
        }
        rv = mCondition;
    }
    if (NS_FAILED(rv))
        mTransport->OnOutputClosed(rv);

    // only send this notification if we have indeed written some data.
    // see bug 196827 for an example of why this is important.
    if (n > 0)
        mTransport->SendStatus(nsISocketTransport::STATUS_SENDING_TO);
    return rv;
}

NS_IMETHODIMP
nsSocketOutputStream::WriteSegments(nsReadSegmentFun reader, void *closure,
                                    PRUint32 count, PRUint32 *countRead)
{
    // socket stream is unbuffered
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsSocketOutputStream::WriteFromSegments(nsIInputStream *input,
                                        void *closure,
                                        const char *fromSegment,
                                        PRUint32 offset,
                                        PRUint32 count,
                                        PRUint32 *countRead)
{
    nsSocketOutputStream *self = (nsSocketOutputStream *) closure;
    return self->Write(fromSegment, count, countRead);
}

NS_IMETHODIMP
nsSocketOutputStream::WriteFrom(nsIInputStream *stream, PRUint32 count, PRUint32 *countRead)
{
    return stream->ReadSegments(WriteFromSegments, this, count, countRead);
}

NS_IMETHODIMP
nsSocketOutputStream::IsNonBlocking(PRBool *nonblocking)
{
    *nonblocking = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketOutputStream::CloseWithStatus(nsresult reason)
{
    LOG(("nsSocketOutputStream::CloseWithStatus [this=%x reason=%x]\n", this, reason));

    // may be called from any thread
 
    nsresult rv;
    {
        nsAutoLock lock(mTransport->mLock);

        if (NS_SUCCEEDED(mCondition))
            rv = mCondition = reason;
        else
            rv = NS_OK;
    }
    if (NS_FAILED(rv))
        mTransport->OnOutputClosed(rv);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketOutputStream::AsyncWait(nsIOutputStreamCallback *callback,
                                PRUint32 flags,
                                PRUint32 amount,
                                nsIEventTarget *target)
{
    LOG(("nsSocketOutputStream::AsyncWait [this=%x]\n", this));

    {
        nsAutoLock lock(mTransport->mLock);

        if (target) {
            //
            // build event proxy
            //
            // failure to create an event proxy (most likely out of memory)
            // shouldn't alter the state of the transport.
            //
            nsCOMPtr<nsIOutputStreamCallback> temp;
            nsresult rv = NS_NewOutputStreamReadyEvent(getter_AddRefs(temp),
                                                       callback, target);
            if (NS_FAILED(rv)) return rv;
            mCallback = temp;
        }
        else
            mCallback = callback;

        mCallbackFlags = flags;
    }
    mTransport->OnOutputPending();
    return NS_OK;
}

//-----------------------------------------------------------------------------
// socket transport impl
//-----------------------------------------------------------------------------

nsSocketTransport::nsSocketTransport()
    : mTypes(nsnull)
    , mTypeCount(0)
    , mPort(0)
    , mProxyPort(0)
    , mProxyTransparent(PR_FALSE)
    , mState(STATE_CLOSED)
    , mAttached(PR_FALSE)
    , mInputClosed(PR_TRUE)
    , mOutputClosed(PR_TRUE)
    , mResolving(PR_FALSE)
    , mLock(PR_NewLock())
    , mFD(nsnull)
    , mFDref(0)
    , mFDconnected(PR_FALSE)
    , mInput(this)
    , mOutput(this)
{
    LOG(("creating nsSocketTransport @%x\n", this));

    NS_ADDREF(gSocketTransportService);
}

nsSocketTransport::~nsSocketTransport()
{
    LOG(("destroying nsSocketTransport @%x\n", this));

    // cleanup socket type info
    if (mTypes) {
        PRUint32 i;
        for (i=0; i<mTypeCount; ++i)
            PL_strfree(mTypes[i]);
        free(mTypes);
    }

    if (mLock)
        PR_DestroyLock(mLock);
 
    nsSocketTransportService *serv = gSocketTransportService;
    NS_RELEASE(serv); // nulls argument
}

nsresult
nsSocketTransport::Init(const char **types, PRUint32 typeCount,
                        const nsACString &host, PRUint16 port,
                        nsIProxyInfo *proxyInfo)
{
    // init socket type info

    mPort = port;
    mHost = host;

    const char *proxyType = nsnull;
    if (proxyInfo) {
        mProxyPort = proxyInfo->Port();
        mProxyHost = proxyInfo->Host();
        // grab proxy type (looking for "socks" for example)
        proxyType = proxyInfo->Type();
        if (proxyType && (strcmp(proxyType, "http") == 0 ||
                          strcmp(proxyType, "direct") == 0))
            proxyType = nsnull;
    }

    LOG(("nsSocketTransport::Init [this=%x host=%s:%hu proxy=%s:%hu]\n",
        this, mHost.get(), mPort, mProxyHost.get(), mProxyPort));

    // include proxy type as a socket type if proxy type is not "http"
    mTypeCount = typeCount + (proxyType != nsnull);
    if (mTypeCount) {
        mTypes = (char **) malloc(mTypeCount * sizeof(char *));
        if (!mTypes)
            return NS_ERROR_OUT_OF_MEMORY;

        // store socket types
        PRUint32 i, type = 0;
        if (proxyType)
            mTypes[type++] = PL_strdup(proxyType);
        for (i=0; i<typeCount; ++i)
            mTypes[type++] = PL_strdup(types[i]);

        // if we have socket types, then the socket provider service had
        // better exist!
        nsresult rv;
        nsCOMPtr<nsISocketProviderService> spserv =
            do_GetService(kSocketProviderServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        // now verify that each socket type has a registered socket provider.
        for (i=0; i<mTypeCount; ++i) {
            nsCOMPtr<nsISocketProvider> provider;
            rv = spserv->GetSocketProvider(mTypes[i], getter_AddRefs(provider));
            if (NS_FAILED(rv)) {
                NS_WARNING("no registered socket provider");
                return rv;
            }

            // note if socket type corresponds to a transparent proxy
            if ((strcmp(mTypes[i], "socks") == 0) ||
                (strcmp(mTypes[i], "socks4") == 0))
                mProxyTransparent = PR_TRUE;
        }
    }

    return NS_OK;
}

nsresult
nsSocketTransport::InitWithConnectedSocket(PRFileDesc *fd, const PRNetAddr *addr)
{
    NS_ASSERTION(!mFD, "already initialized");

    char buf[64];
    PR_NetAddrToString(addr, buf, sizeof(buf));
    mHost.Assign(buf);

    PRUint16 port;
    if (addr->raw.family == PR_AF_INET)
        port = addr->inet.port;
    else
        port = addr->ipv6.port;
    mPort = PR_ntohs(port);

    memcpy(&mNetAddr, addr, sizeof(PRNetAddr));

    mPollFlags = (PR_POLL_READ | PR_POLL_WRITE | PR_POLL_EXCEPT);
    mState = STATE_TRANSFERRING;

    mFD = fd;
    mFDref = 1;
    mFDconnected = 1;

    // make sure new socket is non-blocking
    PRSocketOptionData opt;
    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_TRUE;
    PR_SetSocketOption(mFD, &opt);

    LOG(("nsSocketTransport::InitWithConnectedSocket [this=%p addr=%s:%hu]\n",
        this, mHost.get(), mPort));

    // jump to InitiateSocket to get ourselves attached to the STS poll list.
    return PostEvent(MSG_RETRY_INIT_SOCKET);
}

nsresult
nsSocketTransport::PostEvent(PRUint32 type, nsresult status, nsISupports *param)
{
    LOG(("nsSocketTransport::PostEvent [this=%p type=%u status=%x param=%p]\n",
        this, type, status, param));

    PLEvent *event = new nsSocketEvent(this, type, status, param);
    if (!event)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = gSocketTransportService->PostEvent(event);
    if (NS_FAILED(rv))
        PL_DestroyEvent(event);

    return rv;
}

void
nsSocketTransport::SendStatus(nsresult status)
{
    LOG(("nsSocketTransport::SendStatus [this=%x status=%x]\n", this, status));

    nsCOMPtr<nsITransportEventSink> sink;
    PRUint32 progress;
    {
        nsAutoLock lock(mLock);
        sink = mEventSink;
        switch (status) {
        case STATUS_SENDING_TO:
            progress = mOutput.ByteCount();
            break;
        case STATUS_RECEIVING_FROM:
            progress = mInput.ByteCount();
            break;
        default:
            progress = 0;
            break;
        }
    }
    if (sink)
        sink->OnTransportStatus(this, status, progress, PR_UINT32_MAX);
}

nsresult
nsSocketTransport::ResolveHost()
{
    LOG(("nsSocketTransport::ResolveHost [this=%x]\n", this));

    nsresult rv;

    nsCOMPtr<nsIDNSService> dns = do_GetService(kDNSServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    mResolving = PR_TRUE;

    rv = dns->AsyncResolve(SocketHost(), 0, this, nsnull,
                           getter_AddRefs(mDNSRequest));
    if (NS_SUCCEEDED(rv)) {
        LOG(("  advancing to STATE_RESOLVING\n"));
        mState = STATE_RESOLVING;
        // only report that we are resolving if we are still resolving...
        if (mResolving)
            SendStatus(STATUS_RESOLVING);
    }
    return rv;
}

nsresult
nsSocketTransport::BuildSocket(PRFileDesc *&fd, PRBool &proxyTransparent, PRBool &usingSSL)
{
    LOG(("nsSocketTransport::BuildSocket [this=%x]\n", this));

    nsresult rv;

    proxyTransparent = PR_FALSE;
    usingSSL = PR_FALSE;

    if (mTypeCount == 0) {
        fd = PR_OpenTCPSocket(mNetAddr.raw.family);
        rv = fd ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        fd = nsnull;

        nsCOMPtr<nsISocketProviderService> spserv =
            do_GetService(kSocketProviderServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        const char *host      = mHost.get();
        PRInt32     port      = (PRInt32) mPort;
        const char *proxyHost = mProxyHost.IsEmpty() ? nsnull : mProxyHost.get();
        PRInt32     proxyPort = (PRInt32) mProxyPort;
        
        PRUint32 i;
        for (i=0; i<mTypeCount; ++i) {
            nsCOMPtr<nsISocketProvider> provider;

            LOG(("  pushing io layer [%u:%s]\n", i, mTypes[i]));

            rv = spserv->GetSocketProvider(mTypes[i], getter_AddRefs(provider));
            if (NS_FAILED(rv))
                break;

            nsCOMPtr<nsISupports> secinfo;
            if (i == 0) {
                // if this is the first type, we'll want the 
                // service to allocate a new socket
                rv = provider->NewSocket(mNetAddr.raw.family,
                                         host, port, proxyHost, proxyPort,
                                         &fd, getter_AddRefs(secinfo));

                if (NS_SUCCEEDED(rv) && !fd) {
                    NS_NOTREACHED("NewSocket succeeded but failed to create a PRFileDesc");
                    rv = NS_ERROR_UNEXPECTED;
                }
            }
            else {
                // the socket has already been allocated, 
                // so we just want the service to add itself
                // to the stack (such as pushing an io layer)
                rv = provider->AddToSocket(mNetAddr.raw.family,
                                           host, port, proxyHost, proxyPort,
                                           fd, getter_AddRefs(secinfo));
            }
            if (NS_FAILED(rv))
                break;

            // if the service was ssl or starttls, we want to hold onto the socket info
            PRBool isSSL = (strcmp(mTypes[i], "ssl") == 0);
            if (isSSL || (strcmp(mTypes[i], "starttls") == 0)) {
                // remember security info and give notification callbacks to PSM...
                nsCOMPtr<nsIInterfaceRequestor> callbacks;
                {
                    nsAutoLock lock(mLock);
                    mSecInfo = secinfo;
                    callbacks = mCallbacks;
                    LOG(("  [secinfo=%x callbacks=%x]\n", mSecInfo.get(), mCallbacks.get()));
                }
                // don't call into PSM while holding mLock!!
                nsCOMPtr<nsISSLSocketControl> secCtrl(do_QueryInterface(secinfo));
                if (secCtrl)
                    secCtrl->SetNotificationCallbacks(callbacks);
                // remember if socket type is SSL so we can ProxyStartSSL if need be.
                usingSSL = isSSL;
            }
            else if ((strcmp(mTypes[i], "socks") == 0) ||
                     (strcmp(mTypes[i], "socks4") == 0)) {
                // since socks is transparent, any layers above
                // it do not have to worry about proxy stuff
                proxyHost = nsnull;
                proxyPort = -1;
                proxyTransparent = PR_TRUE;
            }
        }

        if (NS_FAILED(rv)) {
            LOG(("  error pushing io layer [%u:%s rv=%x]\n", i, mTypes[i], rv));
            if (fd)
                PR_Close(fd);
        }
    }

    return rv;
}

nsresult
nsSocketTransport::InitiateSocket()
{
    LOG(("nsSocketTransport::InitiateSocket [this=%x]\n", this));

    nsresult rv;

    //
    // find out if it is going to be ok to attach another socket to the STS.
    // if not then we have to wait for the STS to tell us that it is ok.
    // the notification is asynchronous, which means that when we could be
    // in a race to call AttachSocket once notified.  for this reason, when
    // we get notified, we just re-enter this function.  as a result, we are
    // sure to ask again before calling AttachSocket.  in this way we deal
    // with the race condition.  though it isn't the most elegant solution,
    // it is far simpler than trying to build a system that would guarantee
    // FIFO ordering (which wouldn't even be that valuable IMO).  see bug
    // 194402 for more info.
    //
    if (!gSocketTransportService->CanAttachSocket()) {
        PLEvent *event = new nsSocketEvent(this, MSG_RETRY_INIT_SOCKET);
        if (!event)
            return NS_ERROR_OUT_OF_MEMORY;
        rv = gSocketTransportService->NotifyWhenCanAttachSocket(event);
        if (NS_FAILED(rv))
            PL_DestroyEvent(event);
        return rv;
    }

    //
    // if we already have a connected socket, then just attach and return.
    //
    if (mFD) {
        rv = gSocketTransportService->AttachSocket(mFD, this);
        if (NS_SUCCEEDED(rv))
            mAttached = PR_TRUE;
        return rv;
    }

    //
    // create new socket fd, push io layers, etc.
    //
    PRFileDesc *fd;
    PRBool proxyTransparent;
    PRBool usingSSL;

    rv = BuildSocket(fd, proxyTransparent, usingSSL);
    if (NS_FAILED(rv)) {
        LOG(("  BuildSocket failed [rv=%x]\n", rv));
        return rv;
    }

    PRStatus status;

    // Make the socket non-blocking...
    PRSocketOptionData opt;
    opt.option = PR_SockOpt_Nonblocking;
    opt.value.non_blocking = PR_TRUE;
    status = PR_SetSocketOption(fd, &opt);
    NS_ASSERTION(status == PR_SUCCESS, "unable to make socket non-blocking");
    
    // inform socket transport about this newly created socket...
    rv = gSocketTransportService->AttachSocket(fd, this);
    if (NS_FAILED(rv)) {
        PR_Close(fd);
        return rv;
    }
    mAttached = PR_TRUE;

    // assign mFD so that we can properly handle OnSocketDetached before we've
    // established a connection.
    {
        nsAutoLock lock(mLock);
        mFD = fd;
        mFDref = 1;
        mFDconnected = PR_FALSE;
    }

    LOG(("  advancing to STATE_CONNECTING\n"));
    mState = STATE_CONNECTING;
    SendStatus(STATUS_CONNECTING_TO);

#if defined(PR_LOGGING)
    if (LOG_ENABLED()) {
        char buf[64];
        PR_NetAddrToString(&mNetAddr, buf, sizeof(buf));
        LOG(("  trying address: %s\n", buf));
    }
#endif

    // 
    // Initiate the connect() to the host...  
    //
    status = PR_Connect(fd, &mNetAddr, NS_SOCKET_CONNECT_TIMEOUT);
    if (status == PR_SUCCESS) {
        // 
        // we are connected!
        //
        OnSocketConnected();
    }
    else {
        PRErrorCode code = PR_GetError();
#if defined(TEST_CONNECT_ERRORS)
        code = RandomizeConnectError(code);
#endif
        //
        // If the PR_Connect(...) would block, then poll for a connection.
        //
        if ((PR_WOULD_BLOCK_ERROR == code) || (PR_IN_PROGRESS_ERROR == code))
            mPollFlags = (PR_POLL_EXCEPT | PR_POLL_WRITE);
        //
        // If the socket is already connected, then return success...
        //
        else if (PR_IS_CONNECTED_ERROR == code) {
            //
            // we are connected!
            //
            OnSocketConnected();

            if (mSecInfo && !mProxyHost.IsEmpty() && proxyTransparent && usingSSL) {
                // if the connection phase is finished, and the ssl layer has
                // been pushed, and we were proxying (transparently; ie. nothing
                // has to happen in the protocol layer above us), it's time for
                // the ssl to start doing it's thing.
                nsCOMPtr<nsISSLSocketControl> secCtrl =
                    do_QueryInterface(mSecInfo);
                if (secCtrl) {
                    LOG(("  calling ProxyStartSSL()\n"));
                    secCtrl->ProxyStartSSL();
                }
                // XXX what if we were forced to poll on the socket for a successful
                // connection... wouldn't we need to call ProxyStartSSL after a call
                // to PR_ConnectContinue indicates that we are connected?
                //
                // XXX this appears to be what the old socket transport did.  why
                // isn't this broken?
            }
        }
        //
        // The connection was refused...
        //
        else {
            rv = ErrorAccordingToNSPR(code);
            if ((rv == NS_ERROR_CONNECTION_REFUSED) && !mProxyHost.IsEmpty())
                rv = NS_ERROR_PROXY_CONNECTION_REFUSED;
        }
    }
    return rv;
}

PRBool
nsSocketTransport::RecoverFromError()
{
    NS_ASSERTION(NS_FAILED(mCondition), "there should be something wrong");

    LOG(("nsSocketTransport::RecoverFromError [this=%x state=%x cond=%x]\n",
        this, mState, mCondition));

    // can only recover from errors in these states
    if (mState != STATE_RESOLVING && mState != STATE_CONNECTING)
        return PR_FALSE;

    // OK to check this outside mLock
    NS_ASSERTION(!mFDconnected, "socket should not be connected");

    // can only recover from these errors
    if (mCondition != NS_ERROR_CONNECTION_REFUSED &&
        mCondition != NS_ERROR_PROXY_CONNECTION_REFUSED &&
        mCondition != NS_ERROR_NET_TIMEOUT &&
        mCondition != NS_ERROR_UNKNOWN_HOST &&
        mCondition != NS_ERROR_UNKNOWN_PROXY_HOST)
        return PR_FALSE;

    PRBool tryAgain = PR_FALSE;

    // try next ip address only if past the resolver stage...
    if (mState == STATE_CONNECTING && mDNSRecord) {
        nsresult rv = mDNSRecord->GetNextAddr(SocketPort(), &mNetAddr);
        if (NS_SUCCEEDED(rv)) {
            LOG(("  trying again with next ip address\n"));
            tryAgain = PR_TRUE;
        }
    }

#if defined(XP_WIN)
    // If not trying next address, try to make a connection using dialup. 
    // Retry if that connection is made.
    if (!tryAgain) {
        PRBool autodialEnabled;
        gSocketTransportService->GetAutodialEnabled(&autodialEnabled);
        if (autodialEnabled)
            tryAgain = nsNativeConnectionHelper::OnConnectionFailed(SocketHost().get());
    }
#endif

    // prepare to try again.
    if (tryAgain) {
        nsresult rv;
        PRUint32 msg;

        if (mState == STATE_CONNECTING) {
            mState = STATE_RESOLVING;
            msg = MSG_DNS_LOOKUP_COMPLETE;
        }
        else {
            mState = STATE_CLOSED;
            msg = MSG_ENSURE_CONNECT;
        }

        rv = PostEvent(msg, NS_OK);
        if (NS_FAILED(rv))
            tryAgain = PR_FALSE;
    }

    return tryAgain;
}

// called on the socket thread only
void
nsSocketTransport::OnMsgInputClosed(nsresult reason)
{
    LOG(("nsSocketTransport::OnMsgInputClosed [this=%x reason=%x]\n",
        this, reason));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    mInputClosed = PR_TRUE;
    // check if event should affect entire transport
    if (NS_FAILED(reason) && (reason != NS_BASE_STREAM_CLOSED))
        mCondition = reason;                // XXX except if NS_FAILED(mCondition), right??
    else if (mOutputClosed)
        mCondition = NS_BASE_STREAM_CLOSED; // XXX except if NS_FAILED(mCondition), right??
    else {
        if (mState == STATE_TRANSFERRING)
            mPollFlags &= ~PR_POLL_READ;
        mInput.OnSocketReady(reason);
    }
}

// called on the socket thread only
void
nsSocketTransport::OnMsgOutputClosed(nsresult reason)
{
    LOG(("nsSocketTransport::OnMsgOutputClosed [this=%x reason=%x]\n",
        this, reason));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    mOutputClosed = PR_TRUE;
    // check if event should affect entire transport
    if (NS_FAILED(reason) && (reason != NS_BASE_STREAM_CLOSED))
        mCondition = reason;                // XXX except if NS_FAILED(mCondition), right??
    else if (mInputClosed)
        mCondition = NS_BASE_STREAM_CLOSED; // XXX except if NS_FAILED(mCondition), right??
    else {
        if (mState == STATE_TRANSFERRING)
            mPollFlags &= ~PR_POLL_WRITE;
        mOutput.OnSocketReady(reason);
    }
}

void
nsSocketTransport::OnSocketConnected()
{
    LOG(("  advancing to STATE_TRANSFERRING\n"));

    mPollFlags = (PR_POLL_READ | PR_POLL_WRITE | PR_POLL_EXCEPT);
    mState = STATE_TRANSFERRING;

    SendStatus(STATUS_CONNECTED_TO);

    // assign mFD (must do this within the transport lock), but take care not
    // to trample over mFDref if mFD is already set.
    {
        nsAutoLock lock(mLock);
        NS_ASSERTION(mFD, "no socket");
        NS_ASSERTION(mFDref == 1, "wrong socket ref count");
        mFDconnected = PR_TRUE;
    }
}

PRFileDesc *
nsSocketTransport::GetFD_Locked()
{
    // mFD is not available to the streams while disconnected.
    if (!mFDconnected)
        return nsnull;

    if (mFD)
        mFDref++;

    return mFD;
}

void
nsSocketTransport::ReleaseFD_Locked(PRFileDesc *fd)
{
    NS_ASSERTION(mFD == fd, "wrong fd");

    if (--mFDref == 0) {
        LOG(("nsSocketTransport: calling PR_Close [this=%x]\n", this));
        PR_Close(mFD);
        mFD = nsnull;
    }
}

//-----------------------------------------------------------------------------
// socket event handler impl

void
nsSocketTransport::OnSocketEvent(PRUint32 type, nsresult status, nsISupports *param)
{
    LOG(("nsSocketTransport::OnSocketEvent [this=%p type=%u status=%x param=%p]\n",
        this, type, status, param));

    if (NS_FAILED(mCondition)) {
        // block event since we're apparently already dead.
        LOG(("  blocking event [condition=%x]\n", mCondition));
        //
        // notify input/output streams in case either has a pending notify.
        //
        mInput.OnSocketReady(mCondition);
        mOutput.OnSocketReady(mCondition);
        return;
    }

    switch (type) {
    case MSG_ENSURE_CONNECT:
        LOG(("  MSG_ENSURE_CONNECT\n"));
        //
        // ensure that we have created a socket, attached it, and have a
        // connection.
        //
        if (mState == STATE_CLOSED)
            mCondition = ResolveHost();
        else
            LOG(("  ignoring redundant event\n"));
        break;

    case MSG_DNS_LOOKUP_COMPLETE:
        LOG(("  MSG_DNS_LOOKUP_COMPLETE\n"));
        mDNSRequest = 0;
        if (param) {
            mDNSRecord = NS_STATIC_CAST(nsIDNSRecord *, param);
            mDNSRecord->GetNextAddr(SocketPort(), &mNetAddr);
        }
        // status contains DNS lookup status
        if (NS_FAILED(status)) {
            // fixup error code if proxy was not found
            if ((status == NS_ERROR_UNKNOWN_HOST) && !mProxyHost.IsEmpty())
                mCondition = NS_ERROR_UNKNOWN_PROXY_HOST;
            else
                mCondition = status;
        }
        else if (mState == STATE_RESOLVING)
            mCondition = InitiateSocket();
        break;

    case MSG_RETRY_INIT_SOCKET:
        mCondition = InitiateSocket();
        break;

    case MSG_INPUT_CLOSED:
        LOG(("  MSG_INPUT_CLOSED\n"));
        OnMsgInputClosed(status);
        break;

    case MSG_INPUT_PENDING:
        LOG(("  MSG_INPUT_PENDING\n"));
        OnMsgInputPending();
        break;

    case MSG_OUTPUT_CLOSED:
        LOG(("  MSG_OUTPUT_CLOSED\n"));
        OnMsgOutputClosed(status);
        break;

    case MSG_OUTPUT_PENDING:
        LOG(("  MSG_OUTPUT_PENDING\n"));
        OnMsgOutputPending();
        break;

    default:
        LOG(("  unhandled event!\n"));
    }
    
    if (NS_FAILED(mCondition)) {
        LOG(("  after event [this=%x cond=%x]\n", this, mCondition));
        if (!mAttached) // need to process this error ourselves...
            OnSocketDetached(nsnull);
    }
    else if (mPollFlags == PR_POLL_EXCEPT)
        mPollFlags = 0; // make idle
}

//-----------------------------------------------------------------------------
// socket handler impl

void
nsSocketTransport::OnSocketReady(PRFileDesc *fd, PRInt16 outFlags)
{
    LOG(("nsSocketTransport::OnSocketReady [this=%x outFlags=%hd]\n",
        this, outFlags));

    if (mState == STATE_TRANSFERRING) {
        // if waiting to write and socket is writable or hit an exception.
        if ((mPollFlags & PR_POLL_WRITE) && (outFlags & ~PR_POLL_READ)) {
            // assume that we won't need to poll any longer (the stream will
            // request that we poll again if it is still pending).
            mPollFlags &= ~PR_POLL_WRITE;
            mOutput.OnSocketReady(NS_OK);
        }
        // if waiting to read and socket is readable or hit an exception.
        if ((mPollFlags & PR_POLL_READ) && (outFlags & ~PR_POLL_WRITE)) {
            // assume that we won't need to poll any longer (the stream will
            // request that we poll again if it is still pending).
            mPollFlags &= ~PR_POLL_READ;
            mInput.OnSocketReady(NS_OK);
        }
    }
    else if (mState == STATE_CONNECTING) {
        PRStatus status = PR_ConnectContinue(fd, outFlags);
        if (status == PR_SUCCESS) {
            //
            // we are connected!
            //
            OnSocketConnected();
        }
        else {
            PRErrorCode code = PR_GetError();
#if defined(TEST_CONNECT_ERRORS)
            code = RandomizeConnectError(code);
#endif
            //
            // If the connect is still not ready, then continue polling...
            //
            if ((PR_WOULD_BLOCK_ERROR == code) || (PR_IN_PROGRESS_ERROR == code)) {
                // Set up the select flags for connect...
                mPollFlags = (PR_POLL_EXCEPT | PR_POLL_WRITE);
            } 
            else {
                //
                // else, the connection failed...
                //
                mCondition = ErrorAccordingToNSPR(code);
                if ((mCondition == NS_ERROR_CONNECTION_REFUSED) && !mProxyHost.IsEmpty())
                    mCondition = NS_ERROR_PROXY_CONNECTION_REFUSED;
                LOG(("  connection failed! [reason=%x]\n", mCondition));
            }
        }
    }
    else {
        NS_ERROR("unexpected socket state");
        mCondition = NS_ERROR_UNEXPECTED;
    }

    if (mPollFlags == PR_POLL_EXCEPT)
        mPollFlags = 0; // make idle
}

// called on the socket thread only
void
nsSocketTransport::OnSocketDetached(PRFileDesc *fd)
{
    LOG(("nsSocketTransport::OnSocketDetached [this=%x cond=%x]\n",
        this, mCondition));

    NS_ASSERTION(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    // if we didn't initiate this detach, then be sure to pass an error
    // condition up to our consumers.  (e.g., STS is shutting down.)
    if (NS_SUCCEEDED(mCondition))
        mCondition = NS_ERROR_ABORT;

    if (RecoverFromError())
        mCondition = NS_OK;
    else {
        mState = STATE_CLOSED;

        // make sure there isn't any pending DNS request
        if (mDNSRequest) {
            mDNSRequest->Cancel();
            mDNSRequest = 0;
        }

        //
        // notify input/output streams
        //
        mInput.OnSocketReady(mCondition);
        mOutput.OnSocketReady(mCondition);
    }

    // finally, release our reference to the socket (must do this within
    // the transport lock) possibly closing the socket.
    {
        nsAutoLock lock(mLock);
        if (mFD) {
            ReleaseFD_Locked(mFD);
            // flag mFD as unusable; this prevents other consumers from 
            // acquiring a reference to mFD.
            mFDconnected = PR_FALSE;
        }
    }
}

//-----------------------------------------------------------------------------
// xpcom api

NS_IMPL_THREADSAFE_ISUPPORTS3(nsSocketTransport,
                              nsISocketTransport,
                              nsITransport,
                              nsIDNSListener)

NS_IMETHODIMP
nsSocketTransport::OpenInputStream(PRUint32 flags,
                                   PRUint32 segsize,
                                   PRUint32 segcount,
                                   nsIInputStream **result)
{
    LOG(("nsSocketTransport::OpenInputStream [this=%x flags=%x]\n",
        this, flags));

    NS_ENSURE_TRUE(!mInput.IsReferenced(), NS_ERROR_UNEXPECTED);

    nsresult rv;
    nsCOMPtr<nsIAsyncInputStream> pipeIn;

    if (!(flags & OPEN_UNBUFFERED) || (flags & OPEN_BLOCKING)) {
        // XXX if the caller wants blocking, then the caller also gets buffered!
        //PRBool openBuffered = !(flags & OPEN_UNBUFFERED);
        PRBool openBlocking =  (flags & OPEN_BLOCKING);

        net_ResolveSegmentParams(segsize, segcount);
        nsIMemory *segalloc = net_GetSegmentAlloc(segsize);

        // create a pipe
        nsCOMPtr<nsIAsyncOutputStream> pipeOut;
        rv = NS_NewPipe2(getter_AddRefs(pipeIn), getter_AddRefs(pipeOut),
                         !openBlocking, PR_TRUE, segsize, segcount, segalloc);
        if (NS_FAILED(rv)) return rv;

        // async copy from socket to pipe
        rv = NS_AsyncCopy(&mInput, pipeOut, gSocketTransportService,
                          NS_ASYNCCOPY_VIA_WRITESEGMENTS, segsize);
        if (NS_FAILED(rv)) return rv;

        *result = pipeIn;
    }
    else
        *result = &mInput;

    // flag input stream as open
    mInputClosed = PR_FALSE;

    rv = PostEvent(MSG_ENSURE_CONNECT);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::OpenOutputStream(PRUint32 flags,
                                    PRUint32 segsize,
                                    PRUint32 segcount,
                                    nsIOutputStream **result)
{
    LOG(("nsSocketTransport::OpenOutputStream [this=%x flags=%x]\n",
        this, flags));

    NS_ENSURE_TRUE(!mOutput.IsReferenced(), NS_ERROR_UNEXPECTED);

    nsresult rv;
    nsCOMPtr<nsIAsyncOutputStream> pipeOut;
    if (!(flags & OPEN_UNBUFFERED) || (flags & OPEN_BLOCKING)) {
        // XXX if the caller wants blocking, then the caller also gets buffered!
        //PRBool openBuffered = !(flags & OPEN_UNBUFFERED);
        PRBool openBlocking =  (flags & OPEN_BLOCKING);

        net_ResolveSegmentParams(segsize, segcount);
        nsIMemory *segalloc = net_GetSegmentAlloc(segsize);

        // create a pipe
        nsCOMPtr<nsIAsyncInputStream> pipeIn;
        rv = NS_NewPipe2(getter_AddRefs(pipeIn), getter_AddRefs(pipeOut),
                         PR_TRUE, !openBlocking, segsize, segcount, segalloc);
        if (NS_FAILED(rv)) return rv;

        // async copy from socket to pipe
        rv = NS_AsyncCopy(pipeIn, &mOutput, gSocketTransportService,
                          NS_ASYNCCOPY_VIA_READSEGMENTS, segsize);
        if (NS_FAILED(rv)) return rv;

        *result = pipeOut;
    }
    else
        *result = &mOutput;

    // flag output stream as open
    mOutputClosed = PR_FALSE;

    rv = PostEvent(MSG_ENSURE_CONNECT);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::Close(nsresult reason)
{
    if (NS_SUCCEEDED(reason))
        reason = NS_BASE_STREAM_CLOSED;

    mInput.CloseWithStatus(reason);
    mOutput.CloseWithStatus(reason);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetSecurityInfo(nsISupports **secinfo)
{
    nsAutoLock lock(mLock);
    NS_IF_ADDREF(*secinfo = mSecInfo);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetSecurityCallbacks(nsIInterfaceRequestor **callbacks)
{
    nsAutoLock lock(mLock);
    NS_IF_ADDREF(*callbacks = mCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetSecurityCallbacks(nsIInterfaceRequestor *callbacks)
{
    nsAutoLock lock(mLock);
    mCallbacks = callbacks;
    // XXX should we tell PSM about this?
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetEventSink(nsITransportEventSink *sink,
                                nsIEventTarget *target)
{
    nsCOMPtr<nsITransportEventSink> temp;
    if (target) {
        nsresult rv = net_NewTransportEventSinkProxy(getter_AddRefs(temp),
                                                     sink, target);
        if (NS_FAILED(rv))
            return rv;
        sink = temp.get();
    }

    nsAutoLock lock(mLock);
    mEventSink = sink;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::IsAlive(PRBool *result)
{
    *result = PR_FALSE;

    PRFileDesc *fd;
    {
        nsAutoLock lock(mLock);
        if (NS_FAILED(mCondition))
            return NS_OK;
        fd = GetFD_Locked();
        if (!fd)
            return NS_OK;
    }

    // XXX do some idle-time based checks??

    char c;
    PRInt32 rval = PR_Recv(fd, &c, 1, PR_MSG_PEEK, 0);

    if ((rval > 0) || (rval < 0 && PR_GetError() == PR_WOULD_BLOCK_ERROR))
        *result = PR_TRUE;

    {
        nsAutoLock lock(mLock);
        ReleaseFD_Locked(fd);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetHost(nsACString &host)
{
    host = SocketHost();
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetPort(PRInt32 *port)
{
    *port = (PRInt32) SocketPort();
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetPeerAddr(PRNetAddr *addr)
{
    // once we are in the connected state, mNetAddr will not change.
    // so if we can verify that we are in the connected state, then
    // we can freely access mNetAddr from any thread without being
    // inside a critical section.

    NS_ENSURE_TRUE(mState == STATE_TRANSFERRING, NS_ERROR_NOT_AVAILABLE);

    memcpy(addr, &mNetAddr, sizeof(mNetAddr));
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetSelfAddr(PRNetAddr *addr)
{
    // we must not call any PR methods on our file descriptor
    // while holding mLock since those methods might re-enter
    // socket transport code.

    PRFileDesc *fd;
    {
        nsAutoLock lock(mLock);
        fd = GetFD_Locked();
    }

    if (!fd)
        return NS_ERROR_NOT_CONNECTED;

    nsresult rv =
        (PR_GetSockName(fd, addr) == PR_SUCCESS) ? NS_OK : NS_ERROR_FAILURE;

    {
        nsAutoLock lock(mLock);
        ReleaseFD_Locked(fd);
    }

    return rv;
}

NS_IMETHODIMP
nsSocketTransport::OnLookupComplete(nsIDNSRequest *request,
                                    nsIDNSRecord  *rec,
                                    nsresult       status)
{
    // flag host lookup complete for the benefit of the ResolveHost method.
    mResolving = PR_FALSE;

    nsresult rv = PostEvent(MSG_DNS_LOOKUP_COMPLETE, status, rec);

    // if posting a message fails, then we should assume that the socket
    // transport has been shutdown.  this should never happen!  if it does
    // it means that the socket transport service was shutdown before the
    // DNS service.
    if (NS_FAILED(rv))
        NS_WARNING("unable to post DNS lookup complete message");

    return NS_OK;
}

#ifdef ENABLE_SOCKET_TRACING

#include <stdio.h>
#include <ctype.h>
#include "prenv.h"

static void
DumpBytesToFile(const char *path, const char *header, const char *buf, PRInt32 n)
{
    FILE *fp = fopen(path, "a");

    fprintf(fp, "\n%s [%d bytes]\n", header, n);

    const unsigned char *p;
    while (n) {
        p = (const unsigned char *) buf;

        PRInt32 i, row_max = PR_MIN(16, n);

        for (i = 0; i < row_max; ++i)
            fprintf(fp, "%02x  ", *p++);
        for (i = row_max; i < 16; ++i)
            fprintf(fp, "    ");

        p = (const unsigned char *) buf;
        for (i = 0; i < row_max; ++i, ++p) {
            if (isprint(*p))
                fprintf(fp, "%c", *p);
            else
                fprintf(fp, ".");
        }

        fprintf(fp, "\n");
        buf += row_max;
        n -= row_max;
    }

    fprintf(fp, "\n");
    fclose(fp);
}

void
nsSocketTransport::TraceInBuf(const char *buf, PRInt32 n)
{
    char *val = PR_GetEnv("NECKO_SOCKET_TRACE_LOG");
    if (!val || !*val)
        return;

    nsCAutoString header;
    header.Assign(NS_LITERAL_CSTRING("Reading from: ") + mHost);
    header.Append(':');
    header.AppendInt(mPort);

    DumpBytesToFile(val, header.get(), buf, n);
}

void
nsSocketTransport::TraceOutBuf(const char *buf, PRInt32 n)
{
    char *val = PR_GetEnv("NECKO_SOCKET_TRACE_LOG");
    if (!val || !*val)
        return;

    nsCAutoString header;
    header.Assign(NS_LITERAL_CSTRING("Writing to: ") + mHost);
    header.Append(':');
    header.AppendInt(mPort);

    DumpBytesToFile(val, header.get(), buf, n);
}

#endif
