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

#include "nsSocketTransport2.h"
#include "nsIOService.h"
#include "nsStreamUtils.h"
#include "nsNetSegmentUtils.h"
#include "nsNetCID.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "netCore.h"
#include "prmem.h"
#include "pratom.h"
#include "plstr.h"
#include "prerror.h"
#include "prerr.h"

#include "nsIServiceManager.h"
#include "nsIProxyObjectManager.h"
#include "nsISocketProviderService.h"
#include "nsISocketProvider.h"
#include "nsISSLSocketControl.h"
#include "nsIDNSService.h"
#include "nsIProxyInfo.h"
#include "nsIPipe.h"

#if defined(XP_WIN)
#include "nsNativeConnectionHelper.h"
#endif

//-----------------------------------------------------------------------------

static NS_DEFINE_CID(kSocketProviderServiceCID, NS_SOCKETPROVIDERSERVICE_CID);
static NS_DEFINE_CID(kDNSServiceCID, NS_DNSSERVICE_CID);

//-----------------------------------------------------------------------------

//#define TEST_CONNECT_ERRORS
#ifdef TEST_CONNECT_ERRORS
#include <stdlib.h>
static PRErrorCode RandomizeConnectError(PRErrorCode code)
{
    //
    // To test out these errors, load http://www.yahoo.com/.  It should load
    // correctly despite the random occurance of there errors.
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
            // IP address in mNetAddrList.
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
    , mNotify(nsnull)
    , mByteCount(0)
{
}

nsSocketInputStream::~nsSocketInputStream()
{
    // mNotify normally cleaned up inside Close()
    NS_ASSERTION(mNotify == nsnull, "leaking mNotify");
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

    nsIInputStreamNotify *notify = nsnull;

    {
        nsAutoLock lock(mTransport->mLock);

        // update condition, but be careful not to erase an already
        // existing error condition.
        if (NS_SUCCEEDED(mCondition))
            mCondition = condition;

        // see if anyone wants to be notified...
        notify = mNotify;
        mNotify = nsnull;
    }

    if (notify) {
        notify->OnInputStreamReady(this);
        NS_RELEASE(notify);
    }
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
    return CloseEx(NS_BASE_STREAM_CLOSED);
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
nsSocketInputStream::CloseEx(nsresult reason)
{
    LOG(("nsSocketInputStream::CloseEx [this=%x reason=%x]\n", this, reason));

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
nsSocketInputStream::AsyncWait(nsIInputStreamNotify *notify, PRUint32 amount,
                               nsIEventQueue *eventQ)
{
    LOG(("nsSocketInputStream::AsyncWait [this=%x]\n", this));

    nsresult rv;
    {
        nsAutoLock lock(mTransport->mLock);

        // replace a pending notify
        NS_IF_RELEASE(mNotify);

        if (eventQ) {
            // build event proxy
            rv = NS_NewInputStreamReadyEvent(&mNotify, notify, eventQ);
            if (NS_FAILED(rv)) {
                mNotify = 0;
                if (NS_SUCCEEDED(mCondition))
                    mCondition = rv;
            }
        }
        else
            NS_ADDREF(mNotify = notify);

        rv = mCondition;
    }

    if (NS_FAILED(rv))
        mTransport->OnInputClosed(rv);
    else
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
    , mNotify(nsnull)
    , mByteCount(0)
{
}

nsSocketOutputStream::~nsSocketOutputStream()
{
    // mNotify normally cleaned up inside Close()
    NS_ASSERTION(mNotify == nsnull, "leaking mNotify");
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

    nsIOutputStreamNotify *notify = nsnull;

    {
        nsAutoLock lock(mTransport->mLock);

        // update condition, but be careful not to erase an already
        // existing error condition.
        if (NS_SUCCEEDED(mCondition))
            mCondition = condition;

        // see if anyone wants to be notified...
        notify = mNotify;
        mNotify = nsnull;
    }

    if (notify) {
        notify->OnOutputStreamReady(this);
        NS_RELEASE(notify);
    }
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
    return CloseEx(NS_BASE_STREAM_CLOSED);
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
nsSocketOutputStream::CloseEx(nsresult reason)
{
    LOG(("nsSocketOutputStream::CloseEx [this=%x reason=%x]\n", this, reason));

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
nsSocketOutputStream::AsyncWait(nsIOutputStreamNotify *notify, PRUint32 amount,
                                nsIEventQueue *eventQ)
{
    LOG(("nsSocketOutputStream::AsyncWait [this=%x]\n", this));

    nsresult rv;
    {
        nsAutoLock lock(mTransport->mLock);

        // replace a pending notify
        NS_IF_RELEASE(mNotify);

        if (eventQ) {
            // build event proxy
            rv = NS_NewOutputStreamReadyEvent(&mNotify, notify, eventQ);
            if (NS_FAILED(rv)) {
                mNotify = 0;
                if (NS_SUCCEEDED(mCondition))
                    mCondition = rv;
            }
        }
        else
            NS_ADDREF(mNotify = notify);

        rv = mCondition;
    }
    if (NS_FAILED(rv))
        mTransport->OnOutputClosed(rv);
    else
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
    , mLock(PR_NewLock())
    , mFD(nsnull)
    , mFDref(0)
    , mFDconnected(PR_FALSE)
    , mInput(this)
    , mOutput(this)
    , mNetAddr(nsnull)
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
        PR_Free(mTypes);
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
        if (proxyType && strcmp(proxyType, "http") == 0)
            proxyType = nsnull;
    }

    LOG(("nsSocketTransport::Init [this=%x host=%s:%hu proxy=%s:%hu]\n",
        this, mHost.get(), mPort, mProxyHost.get(), mProxyPort));

    // include proxy type as a socket type if proxy type is not "http"
    mTypeCount = typeCount + (proxyType != nsnull);
    if (mTypeCount) {
        mTypes = (char **) PR_Malloc(mTypeCount * sizeof(char *));
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
        sink->OnTransportStatus(this, status, progress, ~0U);
}

nsresult
nsSocketTransport::ResolveHost()
{
    LOG(("nsSocketTransport::ResolveHost [this=%x]\n", this));

    nsresult rv;

    PRIPv6Addr addr;
    rv = gSocketTransportService->LookupHost(SocketHost(), SocketPort(), &addr);
    if (NS_SUCCEEDED(rv)) {
        // found address!
        mNetAddrList.Init(1);
        mNetAddr = mNetAddrList.GetNext(nsnull);
        PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, SocketPort(), mNetAddr);
        memcpy(&mNetAddr->ipv6.ip, &addr, sizeof(addr));
#ifdef PR_LOGGING
        char buf[128];
        PR_NetAddrToString(mNetAddr, buf, sizeof(buf));
        LOG((" -> using cached ip address [%s]\n", buf));
#endif
        // suppress resolving status message since we are bypassing that step.
        mState = STATE_RESOLVING;
        rv = gSocketTransportService->PostEvent(this,
                                                MSG_DNS_LOOKUP_COMPLETE,
                                                NS_OK, nsnull);
    }
    else {
        const char *host = SocketHost().get();

        nsCOMPtr<nsIDNSService> dns = do_GetService(kDNSServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = dns->Lookup(host, this, nsnull, getter_AddRefs(mDNSRequest));
        if (NS_FAILED(rv)) return rv;

        LOG(("  advancing to STATE_RESOLVING\n"));
        mState = STATE_RESOLVING;
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
        fd = PR_OpenTCPSocket(PR_AF_INET6);
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
                rv = provider->NewSocket(host, port, proxyHost, proxyPort,
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
                rv = provider->AddToSocket(host, port, proxyHost, proxyPort,
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

    //
    // create new socket fd, push io layers, etc.
    //
    PRFileDesc *fd;
    PRBool proxyTransparent;
    PRBool usingSSL;

    nsresult rv = BuildSocket(fd, proxyTransparent, usingSSL);
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

    // 
    // Initiate the connect() to the host...  
    //
    status = PR_Connect(fd, mNetAddr, NS_SOCKET_CONNECT_TIMEOUT);
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

    // can only recover from these errors
    if (mCondition != NS_ERROR_CONNECTION_REFUSED &&
        mCondition != NS_ERROR_PROXY_CONNECTION_REFUSED &&
        mCondition != NS_ERROR_NET_TIMEOUT &&
        mCondition != NS_ERROR_UNKNOWN_HOST &&
        mCondition != NS_ERROR_UNKNOWN_PROXY_HOST)
        return PR_FALSE;

    PRBool tryAgain = PR_FALSE;

    // try next ip address only if past the resolver stage...
    if (mState == STATE_CONNECTING) {
        PRNetAddr *nextAddr = mNetAddrList.GetNext(mNetAddr);
        if (nextAddr) {
            mNetAddr = nextAddr;
#if defined(PR_LOGGING)
            char buf[64];
            PR_NetAddrToString(mNetAddr, buf, sizeof(buf));
            LOG(("  ...trying next address: %s\n", buf));
#endif
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

        rv = gSocketTransportService->PostEvent(this, msg, NS_OK, nsnull);
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

    mInputClosed = PR_TRUE;
    // check if event should effect entire transport
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

    mOutputClosed = PR_TRUE;
    // check if event should effect entire transport
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

    gSocketTransportService->RememberHost(SocketHost(), SocketPort(),
                                          &mNetAddr->ipv6.ip);
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

NS_IMETHODIMP
nsSocketTransport::OnSocketEvent(PRUint32 type, PRUint32 uparam, void *vparam)
{
    LOG(("nsSocketTransport::OnSocketEvent [this=%x type=%u u=%x v=%x]\n",
        this, type, uparam, vparam));

    if (NS_FAILED(mCondition)) {
        // ignore event since we're apparently already dead.
        LOG(("  ignoring event [condition=%x]\n", mCondition));
        return NS_OK;
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
        // uparam contains DNS lookup status
        if (NS_FAILED(uparam)) {
            // fixup error code if proxy was not found
            if ((uparam == NS_ERROR_UNKNOWN_HOST) && !mProxyHost.IsEmpty())
                mCondition = NS_ERROR_UNKNOWN_PROXY_HOST;
            else
                mCondition = uparam;
        }
        else if (mState == STATE_RESOLVING)
            mCondition = InitiateSocket();
        break;

    case MSG_INPUT_CLOSED:
        LOG(("  MSG_INPUT_CLOSED\n"));
        OnMsgInputClosed(uparam);
        break;

    case MSG_INPUT_PENDING:
        LOG(("  MSG_INPUT_PENDING\n"));
        OnMsgInputPending();
        break;

    case MSG_OUTPUT_CLOSED:
        LOG(("  MSG_OUTPUT_CLOSED\n"));
        OnMsgOutputClosed(uparam);
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

    return NS_OK;
}

//-----------------------------------------------------------------------------
// socket handler impl

void
nsSocketTransport::OnSocketReady(PRFileDesc *fd, PRInt16 pollFlags)
{
    LOG(("nsSocketTransport::OnSocketReady [this=%x pollflags=%hd]\n",
        this, pollFlags));

    if (mState == STATE_TRANSFERRING) {
        // check poll flags for errors
        if ((pollFlags & PR_POLL_ERR) ||
            (pollFlags & PR_POLL_EXCEPT)) {
            // the network connection was abruptly closed
            mCondition = NS_ERROR_NET_RESET;
        }
        else if (pollFlags & PR_POLL_HUP) {
            // the network connection was gracefully closed
            mCondition = NS_BASE_STREAM_CLOSED;
        }
        else {
            if (pollFlags & PR_POLL_WRITE) {
                SendStatus(STATUS_SENDING_TO);
                // assume that we won't need to poll any longer (the stream will
                // request that we poll again if it is still pending).
                mPollFlags &= ~PR_POLL_WRITE;
                mOutput.OnSocketReady(NS_OK);
            }
            if (pollFlags & PR_POLL_READ) {
                SendStatus(STATUS_RECEIVING_FROM);
                // assume that we won't need to poll any longer (the stream will
                // request that we poll again if it is still pending).
                mPollFlags &= ~PR_POLL_READ;
                mInput.OnSocketReady(NS_OK);
            }
        }
    }
    else if (mState == STATE_CONNECTING) {
        PRStatus status = PR_ConnectContinue(fd, pollFlags);
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
                mCondition = ErrorAccordingToNSPR(PR_GetError());
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
}

// called on the socket thread only
void
nsSocketTransport::OnSocketDetached(PRFileDesc *fd)
{
    LOG(("nsSocketTransport::OnSocketDetached [this=%x cond=%x]\n",
        this, mCondition));

    PRBool tryAgain = PR_FALSE;

    // if we didn't initiate this detach, then be sure to pass an error
    // condition up to our consumers.  (e.g., STS is shutting down.)
    if (NS_SUCCEEDED(mCondition))
        mCondition = NS_ERROR_ABORT;
    else 
        tryAgain = RecoverFromError();

    if (tryAgain)
        mCondition = NS_OK;
    else {
        mState = STATE_CLOSED;

        // make sure there isn't any pending DNS request
        if (mDNSRequest) {
            mDNSRequest->Cancel(mCondition);
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
        rv = NS_AsyncCopy(&mInput, pipeOut, PR_FALSE, PR_TRUE,
                          segsize, 1, segalloc);
        if (NS_FAILED(rv)) return rv;

        *result = pipeIn;
    }
    else
        *result = &mInput;

    // flag input stream as open
    mInputClosed = PR_FALSE;

    rv = gSocketTransportService->PostEvent(this, MSG_ENSURE_CONNECT,
                                            0, nsnull);
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
        rv = NS_AsyncCopy(pipeIn, &mOutput, PR_TRUE, PR_FALSE,
                          segsize, 1, segalloc);
        if (NS_FAILED(rv)) return rv;

        *result = pipeOut;
    }
    else
        *result = &mOutput;

    // flag output stream as open
    mOutputClosed = PR_FALSE;

    rv = gSocketTransportService->PostEvent(this, MSG_ENSURE_CONNECT,
                                            0, nsnull);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::Close(nsresult reason)
{
    if (NS_SUCCEEDED(reason))
        reason = NS_BASE_STREAM_CLOSED;

    mInput.CloseEx(reason);
    mOutput.CloseEx(reason);
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
                                nsIEventQueue *eventQ)
{
    nsCOMPtr<nsITransportEventSink> temp;
    if (eventQ) {
        nsresult rv;

        rv = NS_GetProxyForObject(eventQ,
                                  NS_GET_IID(nsITransportEventSink),
                                  sink,
                                  PROXY_ASYNC | PROXY_ALWAYS,
                                  getter_AddRefs(temp));
        if (NS_FAILED(rv)) return rv;

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
nsSocketTransport::GetAddress(PRNetAddr *addr)
{
    //
    // NOTE: mNetAddr is assigned on either the socket thread or the DNS thread.
    //       assuming pointer assignment is atomic, this code does not need any
    //       locks to maintain thread safety.
    //

    if (!mNetAddr)
        return NS_ERROR_NOT_AVAILABLE;
        
    memcpy(addr, mNetAddr, sizeof(PRNetAddr));
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::OnStartLookup(nsISupports *ctx, const char *host)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::OnFound(nsISupports *ctx,
                           const char *host,
                           nsHostEnt *hostEnt)
{
    // no locking should be required... yes, we are running on the DNS
    // thread, but our state variables we're going to touch will not be
    // touched by anyone else until we post a MSG_DNS_LOOKUP_COMPLETE
    // event, or until the socket transports destructor runs.

    char **addrList = hostEnt->hostEnt.h_addr_list;
    
    if (addrList && addrList[0]) {
        PRUint32 len = 0;
        PRUint16 port = SocketPort();

        LOG(("nsSocketTransport::OnFound [%s:%hu this=%x] lookup succeeded [FQDN=%s]\n",
            host, port, this, hostEnt->hostEnt.h_name));

        // determine the number of address in the list
        for (; *addrList; ++addrList)
            ++len;
        addrList -= len;

        // allocate space for the addresses
        mNetAddrList.Init(len);

        // populate the address list
        PRNetAddr *addr = nsnull;
        while ((addr = mNetAddrList.GetNext(addr)) != nsnull) {
            PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, port, addr);
            if (hostEnt->hostEnt.h_addrtype == PR_AF_INET6)
                memcpy(&addr->ipv6.ip, *addrList, sizeof(addr->ipv6.ip));
            else
                PR_ConvertIPv4AddrToIPv6(*(PRUint32 *)(*addrList), &addr->ipv6.ip);
            ++addrList;
#if defined(PR_LOGGING)
            char buf[50];
            PR_NetAddrToString(addr, buf, sizeof(buf));
            LOG(("  => %s\n", buf));
#endif
        }

        // start with first address in list
        mNetAddr = mNetAddrList.GetNext(nsnull);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::OnStopLookup(nsISupports *ctx, const char *host,
                                nsresult status)
{
    LOG(("nsSocketTransport::OnStopLookup [this=%x status=%x]\n",
        this, status));

    // keep the DNS service honest...
    if (NS_SUCCEEDED(status) && (mNetAddr == nsnull)) {
        NS_ERROR("success without a result");
        status = NS_ERROR_UNEXPECTED;
    }

    nsresult rv = gSocketTransportService->PostEvent(this,
                                                     MSG_DNS_LOOKUP_COMPLETE,
                                                     status, nsnull);

    // if posting a message fails, then we should assume that the socket
    // transport has been shutdown.  this should never happen!  if it does
    // it means that the socket transport service was shutdown before the
    // DNS service.
    if (NS_FAILED(rv))
        NS_WARNING("unable to post DNS lookup complete message");

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsSocketTransport::NetAddrList
//-----------------------------------------------------------------------------

nsresult
nsSocketTransport::NetAddrList::Init(PRUint32 len)
{
    NS_ASSERTION(!mList, "already initialized");
    mList = new PRNetAddr[len];
    if (!mList)
        return NS_ERROR_OUT_OF_MEMORY;
    mLen = len;
    return NS_OK;
}

PRNetAddr *
nsSocketTransport::NetAddrList::GetNext(PRNetAddr *addr)
{
    if (!addr)
        return mList;

    PRUint32 offset = addr - mList;
    NS_ASSERTION(offset < mLen, "invalid address");
    if (offset + 1 < mLen)
        return addr + 1;

    return nsnull;
}
