/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsProxyRelease.h"
#include "nsPreloadedStream.h"
#include "ASpdySession.h"
#include "mozilla/Telemetry.h"
#include "nsISupportsPriority.h"
#include "nsHttpPipeline.h"

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

using namespace mozilla;
using namespace mozilla::net;

//-----------------------------------------------------------------------------
// nsHttpConnection <public>
//-----------------------------------------------------------------------------

nsHttpConnection::nsHttpConnection()
    : mTransaction(nullptr)
    , mCallbacksLock("nsHttpConnection::mCallbacksLock")
    , mIdleTimeout(0)
    , mConsiderReusedAfterInterval(0)
    , mConsiderReusedAfterEpoch(0)
    , mCurrentBytesRead(0)
    , mMaxBytesRead(0)
    , mTotalBytesRead(0)
    , mTotalBytesWritten(0)
    , mKeepAlive(true) // assume to keep-alive by default
    , mKeepAliveMask(true)
    , mDontReuse(false)
    , mSupportsPipelining(false) // assume low-grade server
    , mIsReused(false)
    , mCompletedProxyConnect(false)
    , mLastTransactionExpectedNoContent(false)
    , mIdleMonitoring(false)
    , mProxyConnectInProgress(false)
    , mHttp1xTransactionCount(0)
    , mRemainingConnectionUses(0xffffffff)
    , mClassification(nsAHttpTransaction::CLASS_GENERAL)
    , mNPNComplete(false)
    , mSetupNPNCalled(false)
    , mUsingSpdyVersion(0)
    , mPriority(nsISupportsPriority::PRIORITY_NORMAL)
    , mReportedSpdy(false)
    , mEverUsedSpdy(false)
{
    LOG(("Creating nsHttpConnection @%x\n", this));

    // grab a reference to the handler to ensure that it doesn't go away.
    nsHttpHandler *handler = gHttpHandler;
    NS_ADDREF(handler);
}

nsHttpConnection::~nsHttpConnection()
{
    LOG(("Destroying nsHttpConnection @%x\n", this));

    // release our reference to the handler
    nsHttpHandler *handler = gHttpHandler;
    NS_RELEASE(handler);

    if (!mEverUsedSpdy) {
        LOG(("nsHttpConnection %p performed %d HTTP/1.x transactions\n",
             this, mHttp1xTransactionCount));
        Telemetry::Accumulate(Telemetry::HTTP_REQUEST_PER_CONN,
                              mHttp1xTransactionCount);
    }

    if (mTotalBytesRead) {
        uint32_t totalKBRead = static_cast<uint32_t>(mTotalBytesRead >> 10);
        LOG(("nsHttpConnection %p read %dkb on connection spdy=%d\n",
             this, totalKBRead, mEverUsedSpdy));
        Telemetry::Accumulate(mEverUsedSpdy ?
                              Telemetry::SPDY_KBREAD_PER_CONN :
                              Telemetry::HTTP_KBREAD_PER_CONN,
                              totalKBRead);
    }
}

nsresult
nsHttpConnection::Init(nsHttpConnectionInfo *info,
                       uint16_t maxHangTime,
                       nsISocketTransport *transport,
                       nsIAsyncInputStream *instream,
                       nsIAsyncOutputStream *outstream,
                       nsIInterfaceRequestor *callbacks,
                       PRIntervalTime rtt)
{
    MOZ_ASSERT(transport && instream && outstream,
               "invalid socket information");
    LOG(("nsHttpConnection::Init [this=%p "
         "transport=%p instream=%p outstream=%p rtt=%d]\n",
         this, transport, instream, outstream,
         PR_IntervalToMilliseconds(rtt)));

    NS_ENSURE_ARG_POINTER(info);
    NS_ENSURE_TRUE(!mConnInfo, NS_ERROR_ALREADY_INITIALIZED);

    mConnInfo = info;
    mLastReadTime = PR_IntervalNow();
    mSupportsPipelining =
        gHttpHandler->ConnMgr()->SupportsPipelining(mConnInfo);
    mRtt = rtt;
    mMaxHangTime = PR_SecondsToInterval(maxHangTime);

    mSocketTransport = transport;
    mSocketIn = instream;
    mSocketOut = outstream;
    nsresult rv = mSocketTransport->SetEventSink(this, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    // See explanation for non-strictness of this operation in SetSecurityCallbacks.
    mCallbacks = new nsMainThreadPtrHolder<nsIInterfaceRequestor>(callbacks, false);
    rv = mSocketTransport->SetSecurityCallbacks(this);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

void
nsHttpConnection::StartSpdy(uint8_t spdyVersion)
{
    LOG(("nsHttpConnection::StartSpdy [this=%p]\n", this));

    MOZ_ASSERT(!mSpdySession);

    mUsingSpdyVersion = spdyVersion;
    mEverUsedSpdy = true;

    // Setting the connection as reused allows some transactions that fail
    // with NS_ERROR_NET_RESET to be restarted and SPDY uses that code
    // to handle clean rejections (such as those that arrived after
    // a server goaway was generated).
    mIsReused = true;

    // If mTransaction is a pipeline object it might represent
    // several requests. If so, we need to unpack that and
    // pack them all into a new spdy session.

    nsTArray<nsRefPtr<nsAHttpTransaction> > list;
    nsresult rv = mTransaction->TakeSubTransactions(list);

    if (rv == NS_ERROR_ALREADY_OPENED) {
        // Has the interface for TakeSubTransactions() changed?
        LOG(("TakeSubTranscations somehow called after "
             "nsAHttpTransaction began processing\n"));
        MOZ_ASSERT(false,
                   "TakeSubTranscations somehow called after "
                   "nsAHttpTransaction began processing");
        mTransaction->Close(NS_ERROR_ABORT);
        return;
    }

    if (NS_FAILED(rv) && rv != NS_ERROR_NOT_IMPLEMENTED) {
        // Has the interface for TakeSubTransactions() changed?
        LOG(("unexpected rv from nnsAHttpTransaction::TakeSubTransactions()"));
        MOZ_ASSERT(false,
                   "unexpected result from "
                   "nsAHttpTransaction::TakeSubTransactions()");
        mTransaction->Close(NS_ERROR_ABORT);
        return;
    }

    if (NS_FAILED(rv)) { // includes NS_ERROR_NOT_IMPLEMENTED
        MOZ_ASSERT(list.IsEmpty(), "sub transaction list not empty");

        // This is ok - treat mTransaction as a single real request.
        // Wrap the old http transaction into the new spdy session
        // as the first stream.
        mSpdySession = ASpdySession::NewSpdySession(spdyVersion,
                                                    mTransaction, mSocketTransport,
                                                    mPriority);
        LOG(("nsHttpConnection::StartSpdy moves single transaction %p "
             "into SpdySession %p\n", mTransaction.get(), mSpdySession.get()));
    }
    else {
        int32_t count = list.Length();

        LOG(("nsHttpConnection::StartSpdy moving transaction list len=%d "
             "into SpdySession %p\n", count, mSpdySession.get()));

        if (!count) {
            mTransaction->Close(NS_ERROR_ABORT);
            return;
        }

        for (int32_t index = 0; index < count; ++index) {
            if (!mSpdySession) {
                mSpdySession = ASpdySession::NewSpdySession(spdyVersion,
                                                            list[index], mSocketTransport,
                                                            mPriority);
            }
            else {
                // AddStream() cannot fail
                if (!mSpdySession->AddStream(list[index], mPriority)) {
                    MOZ_ASSERT(false, "SpdySession::AddStream failed");
                    LOG(("SpdySession::AddStream failed\n"));
                    mTransaction->Close(NS_ERROR_ABORT);
                    return;
                }
            }
        }
    }

    mSupportsPipelining = false; // dont use http/1 pipelines with spdy
    mTransaction = mSpdySession;
    mIdleTimeout = gHttpHandler->SpdyTimeout();
}

bool
nsHttpConnection::EnsureNPNComplete()
{
    // NPN is only used by SPDY right now.
    //
    // If for some reason the components to check on NPN aren't available,
    // this function will just return true to continue on and disable SPDY

    MOZ_ASSERT(mSocketTransport);
    if (!mSocketTransport) {
        // this cannot happen
        mNPNComplete = true;
        return true;
    }

    if (mNPNComplete)
        return true;

    nsresult rv;

    nsCOMPtr<nsISupports> securityInfo;
    nsCOMPtr<nsISSLSocketControl> ssl;
    nsAutoCString negotiatedNPN;

    rv = mSocketTransport->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (NS_FAILED(rv))
        goto npnComplete;

    ssl = do_QueryInterface(securityInfo, &rv);
    if (NS_FAILED(rv))
        goto npnComplete;

    rv = ssl->GetNegotiatedNPN(negotiatedNPN);
    if (rv == NS_ERROR_NOT_CONNECTED) {

        // By writing 0 bytes to the socket the SSL handshake machine is
        // pushed forward.
        uint32_t count = 0;
        rv = mSocketOut->Write("", 0, &count);

        if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK)
            goto npnComplete;
        return false;
    }

    if (NS_FAILED(rv))
        goto npnComplete;

    LOG(("nsHttpConnection::EnsureNPNComplete %p negotiated to '%s'",
         this, negotiatedNPN.get()));

    uint8_t spdyVersion;
    rv = gHttpHandler->SpdyInfo()->GetNPNVersionIndex(negotiatedNPN,
                                                      &spdyVersion);
    if (NS_SUCCEEDED(rv))
        StartSpdy(spdyVersion);

    Telemetry::Accumulate(Telemetry::SPDY_NPN_CONNECT, UsingSpdy());

npnComplete:
    LOG(("nsHttpConnection::EnsureNPNComplete setting complete to true"));
    mNPNComplete = true;
    return true;
}

// called on the socket thread
nsresult
nsHttpConnection::Activate(nsAHttpTransaction *trans, uint32_t caps, int32_t pri)
{
    nsresult rv;

    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    LOG(("nsHttpConnection::Activate [this=%p trans=%x caps=%x]\n",
         this, trans, caps));

    mPriority = pri;
    if (mTransaction && mUsingSpdyVersion)
        return AddTransaction(trans, pri);

    NS_ENSURE_ARG_POINTER(trans);
    NS_ENSURE_TRUE(!mTransaction, NS_ERROR_IN_PROGRESS);

    // reset the read timers to wash away any idle time
    mLastReadTime = PR_IntervalNow();

    // Update security callbacks
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    trans->GetSecurityCallbacks(getter_AddRefs(callbacks));
    SetSecurityCallbacks(callbacks);

    SetupNPN(caps); // only for spdy

    // take ownership of the transaction
    mTransaction = trans;

    MOZ_ASSERT(!mIdleMonitoring, "Activating a connection with an Idle Monitor");
    mIdleMonitoring = false;

    // set mKeepAlive according to what will be requested
    mKeepAliveMask = mKeepAlive = (caps & NS_HTTP_ALLOW_KEEPALIVE);

    // need to handle HTTP CONNECT tunnels if this is the first time if
    // we are tunneling through a proxy
    if (mConnInfo->UsingConnect() && !mCompletedProxyConnect) {
        rv = SetupProxyConnect();
        if (NS_FAILED(rv))
            goto failed_activation;
        mProxyConnectInProgress = true;
    }

    // Clear the per activation counter
    mCurrentBytesRead = 0;

    // The overflow state is not needed between activations
    mInputOverflow = nullptr;

    rv = OnOutputStreamReady(mSocketOut);

failed_activation:
    if (NS_FAILED(rv)) {
        mTransaction = nullptr;
    }

    return rv;
}

void
nsHttpConnection::SetupNPN(uint32_t caps)
{
    if (mSetupNPNCalled)                                /* do only once */
        return;
    mSetupNPNCalled = true;

    // Setup NPN Negotiation if necessary (only for SPDY)
    if (!mNPNComplete) {

        mNPNComplete = true;

        if (mConnInfo->UsingSSL()) {
            LOG(("nsHttpConnection::SetupNPN Setting up "
                 "Next Protocol Negotiation"));
            nsCOMPtr<nsISupports> securityInfo;
            nsresult rv =
                mSocketTransport->GetSecurityInfo(getter_AddRefs(securityInfo));
            if (NS_FAILED(rv))
                return;

            nsCOMPtr<nsISSLSocketControl> ssl =
                do_QueryInterface(securityInfo, &rv);
            if (NS_FAILED(rv))
                return;

            nsTArray<nsCString> protocolArray;

            // The first protocol is used as the fallback if none of the
            // protocols supported overlap with the server's list.
            // In the case of overlap, matching priority is driven by
            // the order of the server's advertisement.
            protocolArray.AppendElement(NS_LITERAL_CSTRING("http/1.1"));

            if (gHttpHandler->IsSpdyEnabled() &&
                !(caps & NS_HTTP_DISALLOW_SPDY)) {
                LOG(("nsHttpConnection::SetupNPN Allow SPDY NPN selection"));
                if (gHttpHandler->SpdyInfo()->ProtocolEnabled(0))
                    protocolArray.AppendElement(
                        gHttpHandler->SpdyInfo()->VersionString[0]);
                if (gHttpHandler->SpdyInfo()->ProtocolEnabled(1))
                    protocolArray.AppendElement(
                        gHttpHandler->SpdyInfo()->VersionString[1]);
            }

            if (NS_SUCCEEDED(ssl->SetNPNList(protocolArray))) {
                LOG(("nsHttpConnection::Init Setting up SPDY Negotiation OK"));
                mNPNComplete = false;
            }
        }
    }
}

void
nsHttpConnection::HandleAlternateProtocol(nsHttpResponseHead *responseHead)
{
    // Look for the Alternate-Protocol header. Alternate-Protocol is
    // essentially a way to rediect future transactions from http to
    // spdy.
    //

    if (!gHttpHandler->IsSpdyEnabled() || mUsingSpdyVersion)
        return;

    const char *val = responseHead->PeekHeader(nsHttp::Alternate_Protocol);
    if (!val)
        return;

    // The spec allows redirections to any port, but due to concerns over
    // silently redirecting to stealth ports we only allow port 443
    //
    // Alternate-Protocol: 5678:somethingelse, 443:npn-spdy/2

    uint8_t alternateProtocolVersion;
    if (NS_SUCCEEDED(gHttpHandler->SpdyInfo()->
                     GetAlternateProtocolVersionIndex(val,
                                                      &alternateProtocolVersion))) {
         LOG(("Connection %p Transaction %p found Alternate-Protocol "
             "header %s", this, mTransaction.get(), val));
        gHttpHandler->ConnMgr()->ReportSpdyAlternateProtocol(this);
    }
}

nsresult
nsHttpConnection::AddTransaction(nsAHttpTransaction *httpTransaction,
                                 int32_t priority)
{
    LOG(("nsHttpConnection::AddTransaction for SPDY"));

    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(mSpdySession && mUsingSpdyVersion,
               "AddTransaction to live http connection without spdy");
    MOZ_ASSERT(mTransaction,
               "AddTransaction to idle http connection");

    if (!mSpdySession->AddStream(httpTransaction, priority)) {
        MOZ_ASSERT(false, "AddStream should never fail due to"
                   "RoomForMore() admission check");
        return NS_ERROR_FAILURE;
    }

    ResumeSend();

    return NS_OK;
}

void
nsHttpConnection::Close(nsresult reason)
{
    LOG(("nsHttpConnection::Close [this=%p reason=%x]\n", this, reason));

    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (NS_FAILED(reason)) {
        if (mIdleMonitoring)
            EndIdleMonitoring();

        if (mSocketTransport) {
            mSocketTransport->SetEventSink(nullptr, nullptr);

            // If there are bytes sitting in the input queue then read them
            // into a junk buffer to avoid generating a tcp rst by closing a
            // socket with data pending. TLS is a classic case of this where
            // a Alert record might be superfulous to a clean HTTP/SPDY shutdown.
            // Never block to do this and limit it to a small amount of data.
            if (mSocketIn) {
                char buffer[4000];
                uint32_t count, total = 0;
                nsresult rv;
                do {
                    rv = mSocketIn->Read(buffer, 4000, &count);
                    if (NS_SUCCEEDED(rv))
                        total += count;
                }
                while (NS_SUCCEEDED(rv) && count > 0 && total < 64000);
                LOG(("nsHttpConnection::Close drained %d bytes\n", total));
            }

            mSocketTransport->SetSecurityCallbacks(nullptr);
            mSocketTransport->Close(reason);
            if (mSocketOut)
                mSocketOut->AsyncWait(nullptr, 0, 0, nullptr);
        }
        mKeepAlive = false;
    }
}

// called on the socket thread
nsresult
nsHttpConnection::ProxyStartSSL()
{
    LOG(("nsHttpConnection::ProxyStartSSL [this=%p]\n", this));
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    nsCOMPtr<nsISupports> securityInfo;
    nsresult rv = mSocketTransport->GetSecurityInfo(getter_AddRefs(securityInfo));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISSLSocketControl> ssl = do_QueryInterface(securityInfo, &rv);
    if (NS_FAILED(rv)) return rv;

    return ssl->ProxyStartSSL();
}

void
nsHttpConnection::DontReuse()
{
    mKeepAliveMask = false;
    mKeepAlive = false;
    mDontReuse = true;
    mIdleTimeout = 0;
    if (mSpdySession)
        mSpdySession->DontReuse();
}

// Checked by the Connection Manager before scheduling a pipelined transaction
bool
nsHttpConnection::SupportsPipelining()
{
    if (mTransaction &&
        mTransaction->PipelineDepth() >= mRemainingConnectionUses) {
        LOG(("nsHttpConnection::SupportsPipelining this=%p deny pipeline "
             "because current depth %d exceeds max remaining uses %d\n",
             this, mTransaction->PipelineDepth(), mRemainingConnectionUses));
        return false;
    }
    return mSupportsPipelining && IsKeepAlive() && !mDontReuse;
}

bool
nsHttpConnection::CanReuse()
{
    if (mDontReuse)
        return false;

    if ((mTransaction ? mTransaction->PipelineDepth() : 0) >=
        mRemainingConnectionUses) {
        return false;
    }

    bool canReuse;

    if (mSpdySession)
        canReuse = mSpdySession->CanReuse();
    else
        canReuse = IsKeepAlive();

    canReuse = canReuse && (IdleTime() < mIdleTimeout) && IsAlive();

    // An idle persistent connection should not have data waiting to be read
    // before a request is sent. Data here is likely a 408 timeout response
    // which we would deal with later on through the restart logic, but that
    // path is more expensive than just closing the socket now.

    uint64_t dataSize;
    if (canReuse && mSocketIn && !mUsingSpdyVersion && mHttp1xTransactionCount &&
        NS_SUCCEEDED(mSocketIn->Available(&dataSize)) && dataSize) {
        LOG(("nsHttpConnection::CanReuse %p %s"
             "Socket not reusable because read data pending (%llu) on it.\n",
             this, mConnInfo->Host(), dataSize));
        canReuse = false;
    }
    return canReuse;
}

bool
nsHttpConnection::CanDirectlyActivate()
{
    // return true if a new transaction can be addded to ths connection at any
    // time through Activate(). In practice this means this is a healthy SPDY
    // connection with room for more concurrent streams.

    return UsingSpdy() && CanReuse() &&
        mSpdySession && mSpdySession->RoomForMoreStreams();
}

PRIntervalTime
nsHttpConnection::IdleTime()
{
    return mSpdySession ?
        mSpdySession->IdleTime() : (PR_IntervalNow() - mLastReadTime);
}

// returns the number of seconds left before the allowable idle period
// expires, or 0 if the period has already expied.
uint32_t
nsHttpConnection::TimeToLive()
{
    if (IdleTime() >= mIdleTimeout)
        return 0;
    uint32_t timeToLive = PR_IntervalToSeconds(mIdleTimeout - IdleTime());

    // a positive amount of time can be rounded to 0. Because 0 is used
    // as the expiration signal, round all values from 0 to 1 up to 1.
    if (!timeToLive)
        timeToLive = 1;
    return timeToLive;
}

bool
nsHttpConnection::IsAlive()
{
    if (!mSocketTransport)
        return false;

    // SocketTransport::IsAlive can run the SSL state machine, so make sure
    // the NPN options are set before that happens.
    SetupNPN(0);

    bool alive;
    nsresult rv = mSocketTransport->IsAlive(&alive);
    if (NS_FAILED(rv))
        alive = false;

//#define TEST_RESTART_LOGIC
#ifdef TEST_RESTART_LOGIC
    if (!alive) {
        LOG(("pretending socket is still alive to test restart logic\n"));
        alive = true;
    }
#endif

    return alive;
}

bool
nsHttpConnection::SupportsPipelining(nsHttpResponseHead *responseHead)
{
    // SPDY supports infinite parallelism, so no need to pipeline.
    if (mUsingSpdyVersion)
        return false;

    // assuming connection is HTTP/1.1 with keep-alive enabled
    if (mConnInfo->UsingHttpProxy() && !mConnInfo->UsingConnect()) {
        // XXX check for bad proxy servers...
        return true;
    }

    // check for bad origin servers
    const char *val = responseHead->PeekHeader(nsHttp::Server);

    // If there is no server header we will assume it should not be banned
    // as facebook and some other prominent sites do this
    if (!val)
        return true;

    // The blacklist is indexed by the first character. All of these servers are
    // known to return their identifier as the first thing in the server string,
    // so we can do a leading match.

    static const char *bad_servers[26][6] = {
        { nullptr }, { nullptr }, { nullptr }, { nullptr },                 // a - d
        { "EFAServer/", nullptr },                                       // e
        { nullptr }, { nullptr }, { nullptr }, { nullptr },                 // f - i
        { nullptr }, { nullptr }, { nullptr },                             // j - l
        { "Microsoft-IIS/4.", "Microsoft-IIS/5.", nullptr },             // m
        { "Netscape-Enterprise/3.", "Netscape-Enterprise/4.",
          "Netscape-Enterprise/5.", "Netscape-Enterprise/6.", nullptr }, // n
        { nullptr }, { nullptr }, { nullptr }, { nullptr },                 // o - r
        { nullptr }, { nullptr }, { nullptr }, { nullptr },                 // s - v
        { "WebLogic 3.", "WebLogic 4.","WebLogic 5.", "WebLogic 6.",
          "Winstone Servlet Engine v0.", nullptr },                      // w
        { nullptr }, { nullptr }, { nullptr }                              // x - z
    };

    int index = val[0] - 'A'; // the whole table begins with capital letters
    if ((index >= 0) && (index <= 25))
    {
        for (int i = 0; bad_servers[index][i] != nullptr; i++) {
            if (!PL_strncmp (val, bad_servers[index][i], strlen (bad_servers[index][i]))) {
                LOG(("looks like this server does not support pipelining"));
                gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
                    mConnInfo, nsHttpConnectionMgr::RedBannedServer, this , 0);
                return false;
            }
        }
    }

    // ok, let's allow pipelining to this server
    return true;
}

//----------------------------------------------------------------------------
// nsHttpConnection::nsAHttpConnection compatible methods
//----------------------------------------------------------------------------

nsresult
nsHttpConnection::OnHeadersAvailable(nsAHttpTransaction *trans,
                                     nsHttpRequestHead *requestHead,
                                     nsHttpResponseHead *responseHead,
                                     bool *reset)
{
    LOG(("nsHttpConnection::OnHeadersAvailable [this=%p trans=%p response-head=%p]\n",
        this, trans, responseHead));

    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    NS_ENSURE_ARG_POINTER(trans);
    MOZ_ASSERT(responseHead, "No response head?");

    // If the server issued an explicit timeout, then we need to close down the
    // socket transport.  We pass an error code of NS_ERROR_NET_RESET to
    // trigger the transactions 'restart' mechanism.  We tell it to reset its
    // response headers so that it will be ready to receive the new response.
    uint16_t responseStatus = responseHead->Status();
    if (responseStatus == 408) {
        Close(NS_ERROR_NET_RESET);
        *reset = true;
        return NS_OK;
    }

    // we won't change our keep-alive policy unless the server has explicitly
    // told us to do so.

    // inspect the connection headers for keep-alive info provided the
    // transaction completed successfully. In the case of a non-sensical close
    // and keep-alive favor the close out of conservatism.

    bool explicitKeepAlive = false;
    bool explicitClose = responseHead->HasHeaderValue(nsHttp::Connection, "close") ||
        responseHead->HasHeaderValue(nsHttp::Proxy_Connection, "close");
    if (!explicitClose)
        explicitKeepAlive = responseHead->HasHeaderValue(nsHttp::Connection, "keep-alive") ||
            responseHead->HasHeaderValue(nsHttp::Proxy_Connection, "keep-alive");

    // reset to default (the server may have changed since we last checked)
    mSupportsPipelining = false;

    if ((responseHead->Version() < NS_HTTP_VERSION_1_1) ||
        (requestHead->Version() < NS_HTTP_VERSION_1_1)) {
        // HTTP/1.0 connections are by default NOT persistent
        if (explicitKeepAlive)
            mKeepAlive = true;
        else
            mKeepAlive = false;

        // We need at least version 1.1 to use pipelines
        gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
            mConnInfo, nsHttpConnectionMgr::RedVersionTooLow, this, 0);
    }
    else {
        // HTTP/1.1 connections are by default persistent
        if (explicitClose) {
            mKeepAlive = false;

            // persistent connections are required for pipelining to work - if
            // this close was not pre-announced then generate the negative
            // BadExplicitClose feedback
            if (mRemainingConnectionUses > 1)
                gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
                    mConnInfo, nsHttpConnectionMgr::BadExplicitClose, this, 0);
        }
        else {
            mKeepAlive = true;

            // Do not support pipelining when we are establishing
            // an SSL tunnel though an HTTP proxy. Pipelining support
            // determination must be based on comunication with the
            // target server in this case. See bug 422016 for futher
            // details.
            if (!mProxyConnectStream)
              mSupportsPipelining = SupportsPipelining(responseHead);
        }
    }
    mKeepAliveMask = mKeepAlive;

    // Update the pipelining status in the connection info object
    // and also read it back. It is possible the ci status is
    // locked to false if pipelining has been banned on this ci due to
    // some kind of observed flaky behavior
    if (mSupportsPipelining) {
        // report the pipelining-compatible header to the connection manager
        // as positive feedback. This will undo 1 penalty point the host
        // may have accumulated in the past.

        gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
            mConnInfo, nsHttpConnectionMgr::NeutralExpectedOK, this, 0);

        mSupportsPipelining =
            gHttpHandler->ConnMgr()->SupportsPipelining(mConnInfo);
    }

    // If this connection is reserved for revalidations and we are
    // receiving a document that failed revalidation then switch the
    // classification to general to avoid pipelining more revalidations behind
    // it.
    if (mClassification == nsAHttpTransaction::CLASS_REVALIDATION &&
        responseStatus != 304) {
        mClassification = nsAHttpTransaction::CLASS_GENERAL;
    }

    // if this connection is persistent, then the server may send a "Keep-Alive"
    // header specifying the maximum number of times the connection can be
    // reused as well as the maximum amount of time the connection can be idle
    // before the server will close it.  we ignore the max reuse count, because
    // a "keep-alive" connection is by definition capable of being reused, and
    // we only care about being able to reuse it once.  if a timeout is not
    // specified then we use our advertized timeout value.
    bool foundKeepAliveMax = false;
    if (mKeepAlive) {
        const char *val = responseHead->PeekHeader(nsHttp::Keep_Alive);

        if (!mUsingSpdyVersion) {
            const char *cp = PL_strcasestr(val, "timeout=");
            if (cp)
                mIdleTimeout = PR_SecondsToInterval((uint32_t) atoi(cp + 8));
            else
                mIdleTimeout = gHttpHandler->IdleTimeout();

            cp = PL_strcasestr(val, "max=");
            if (cp) {
                int val = atoi(cp + 4);
                if (val > 0) {
                    foundKeepAliveMax = true;
                    mRemainingConnectionUses = static_cast<uint32_t>(val);
                }
            }
        }
        else {
            mIdleTimeout = gHttpHandler->SpdyTimeout();
        }

        LOG(("Connection can be reused [this=%p idle-timeout=%usec]\n",
             this, PR_IntervalToSeconds(mIdleTimeout)));
    }

    if (!foundKeepAliveMax && mRemainingConnectionUses && !mUsingSpdyVersion)
        --mRemainingConnectionUses;

    if (!mProxyConnectStream)
        HandleAlternateProtocol(responseHead);

    // If we're doing a proxy connect, we need to check whether or not
    // it was successful.  If so, we have to reset the transaction and step-up
    // the socket connection if using SSL. Finally, we have to wake up the
    // socket write request.
    if (mProxyConnectStream) {
        MOZ_ASSERT(!mUsingSpdyVersion,
                   "SPDY NPN Complete while using proxy connect stream");
        mProxyConnectStream = 0;
        if (responseStatus == 200) {
            LOG(("proxy CONNECT succeeded! ssl=%s\n",
                 mConnInfo->UsingSSL() ? "true" :"false"));
            *reset = true;
            nsresult rv;
            if (mConnInfo->UsingSSL()) {
                rv = ProxyStartSSL();
                if (NS_FAILED(rv)) // XXX need to handle this for real
                    LOG(("ProxyStartSSL failed [rv=%x]\n", rv));
            }
            mCompletedProxyConnect = true;
            mProxyConnectInProgress = false;
            rv = mSocketOut->AsyncWait(this, 0, 0, nullptr);
            // XXX what if this fails -- need to handle this error
            MOZ_ASSERT(NS_SUCCEEDED(rv), "mSocketOut->AsyncWait failed");
        }
        else {
            LOG(("proxy CONNECT failed! ssl=%s\n",
                 mConnInfo->UsingSSL() ? "true" :"false"));
            mTransaction->SetProxyConnectFailed();
        }
    }

    const char *upgradeReq = requestHead->PeekHeader(nsHttp::Upgrade);
    // Don't use persistent connection for Upgrade unless there's an auth failure:
    // some proxies expect to see auth response on persistent connection.
    if (upgradeReq && responseStatus != 401 && responseStatus != 407) {
        LOG(("HTTP Upgrade in play - disable keepalive\n"));
        DontReuse();
    }

    if (responseStatus == 101) {
        const char *upgradeResp = responseHead->PeekHeader(nsHttp::Upgrade);
        if (!upgradeReq || !upgradeResp ||
            !nsHttp::FindToken(upgradeResp, upgradeReq,
                               HTTP_HEADER_VALUE_SEPS)) {
            LOG(("HTTP 101 Upgrade header mismatch req = %s, resp = %s\n",
                 upgradeReq, upgradeResp));
            Close(NS_ERROR_ABORT);
        }
        else {
            LOG(("HTTP Upgrade Response to %s\n", upgradeResp));
        }
    }

    return NS_OK;
}

bool
nsHttpConnection::IsReused()
{
    if (mIsReused)
        return true;
    if (!mConsiderReusedAfterInterval)
        return false;

    // ReusedAfter allows a socket to be consider reused only after a certain
    // interval of time has passed
    return (PR_IntervalNow() - mConsiderReusedAfterEpoch) >=
        mConsiderReusedAfterInterval;
}

void
nsHttpConnection::SetIsReusedAfter(uint32_t afterMilliseconds)
{
    mConsiderReusedAfterEpoch = PR_IntervalNow();
    mConsiderReusedAfterInterval = PR_MillisecondsToInterval(afterMilliseconds);
}

nsresult
nsHttpConnection::TakeTransport(nsISocketTransport  **aTransport,
                                nsIAsyncInputStream **aInputStream,
                                nsIAsyncOutputStream **aOutputStream)
{
    if (mUsingSpdyVersion)
        return NS_ERROR_FAILURE;
    if (mTransaction && !mTransaction->IsDone())
        return NS_ERROR_IN_PROGRESS;
    if (!(mSocketTransport && mSocketIn && mSocketOut))
        return NS_ERROR_NOT_INITIALIZED;

    if (mInputOverflow)
        mSocketIn = mInputOverflow.forget();

    NS_IF_ADDREF(*aTransport = mSocketTransport);
    NS_IF_ADDREF(*aInputStream = mSocketIn);
    NS_IF_ADDREF(*aOutputStream = mSocketOut);

    mSocketTransport->SetSecurityCallbacks(nullptr);
    mSocketTransport->SetEventSink(nullptr, nullptr);
    mSocketTransport = nullptr;
    mSocketIn = nullptr;
    mSocketOut = nullptr;

    return NS_OK;
}

void
nsHttpConnection::ReadTimeoutTick(PRIntervalTime now)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    // make sure timer didn't tick before Activate()
    if (!mTransaction)
        return;

    // Spdy implements some timeout handling using the SPDY ping frame.
    if (mSpdySession) {
        mSpdySession->ReadTimeoutTick(now);
        return;
    }

    if (!gHttpHandler->GetPipelineRescheduleOnTimeout())
        return;

    PRIntervalTime delta = now - mLastReadTime;

    // we replicate some of the checks both here and in OnSocketReadable() as
    // they will be discovered under different conditions. The ones here
    // will generally be discovered if we are totally hung and OSR does
    // not get called at all, however OSR discovers them with lower latency
    // if the issue is just very slow (but not stalled) reading.
    //
    // Right now we only take action if pipelining is involved, but this would
    // be the place to add general read timeout handling if it is desired.

    uint32_t pipelineDepth = mTransaction->PipelineDepth();

    if (delta >= gHttpHandler->GetPipelineRescheduleTimeout() &&
        pipelineDepth > 1) {

        // this just reschedules blocked transactions. no transaction
        // is aborted completely.
        LOG(("cancelling pipeline due to a %ums stall - depth %d\n",
             PR_IntervalToMilliseconds(delta), pipelineDepth));

        nsHttpPipeline *pipeline = mTransaction->QueryPipeline();
        MOZ_ASSERT(pipeline, "pipelinedepth > 1 without pipeline");
        // code this defensively for the moment and check for null in opt build
        // This will reschedule blocked members of the pipeline, but the
        // blocking transaction (i.e. response 0) will not be changed.
        if (pipeline) {
            pipeline->CancelPipeline(NS_ERROR_NET_TIMEOUT);
            LOG(("Rescheduling the head of line blocked members of a pipeline "
                 "because reschedule-timeout idle interval exceeded"));
        }
    }

    if (delta < gHttpHandler->GetPipelineTimeout())
        return;

    if (pipelineDepth <= 1 && !mTransaction->PipelinePosition())
        return;

    // nothing has transpired on this pipelined socket for many
    // seconds. Call that a total stall and close the transaction.
    // There is a chance the transaction will be restarted again
    // depending on its state.. that will come back araound
    // without pipelining on, so this won't loop.

    LOG(("canceling transaction stalled for %ums on a pipeline "
         "of depth %d and scheduled originally at pos %d\n",
         PR_IntervalToMilliseconds(delta),
         pipelineDepth, mTransaction->PipelinePosition()));

    // This will also close the connection
    CloseTransaction(mTransaction, NS_ERROR_NET_TIMEOUT);
}

void
nsHttpConnection::GetSecurityInfo(nsISupports **secinfo)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (mSocketTransport) {
        if (NS_FAILED(mSocketTransport->GetSecurityInfo(secinfo)))
            *secinfo = nullptr;
    }
}

void
nsHttpConnection::SetSecurityCallbacks(nsIInterfaceRequestor* aCallbacks)
{
    MutexAutoLock lock(mCallbacksLock);
    // This is called both on and off the main thread. For JS-implemented
    // callbacks, we requires that the call happen on the main thread, but
    // for C++-implemented callbacks we don't care. Use a pointer holder with
    // strict checking disabled.
    mCallbacks = new nsMainThreadPtrHolder<nsIInterfaceRequestor>(aCallbacks, false);
}

nsresult
nsHttpConnection::PushBack(const char *data, uint32_t length)
{
    LOG(("nsHttpConnection::PushBack [this=%p, length=%d]\n", this, length));

    if (mInputOverflow) {
        NS_ERROR("nsHttpConnection::PushBack only one buffer supported");
        return NS_ERROR_UNEXPECTED;
    }

    mInputOverflow = new nsPreloadedStream(mSocketIn, data, length);
    return NS_OK;
}

nsresult
nsHttpConnection::ResumeSend()
{
    LOG(("nsHttpConnection::ResumeSend [this=%p]\n", this));

    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (mSocketOut)
        return mSocketOut->AsyncWait(this, 0, 0, nullptr);

    NS_NOTREACHED("no socket output stream");
    return NS_ERROR_UNEXPECTED;
}

nsresult
nsHttpConnection::ResumeRecv()
{
    LOG(("nsHttpConnection::ResumeRecv [this=%p]\n", this));

    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    // the mLastReadTime timestamp is used for finding slowish readers
    // and can be pretty sensitive. For that reason we actually reset it
    // when we ask to read (resume recv()) so that when we get called back
    // with actual read data in OnSocketReadable() we are only measuring
    // the latency between those two acts and not all the processing that
    // may get done before the ResumeRecv() call
    mLastReadTime = PR_IntervalNow();

    if (mSocketIn)
        return mSocketIn->AsyncWait(this, 0, 0, nullptr);

    NS_NOTREACHED("no socket input stream");
    return NS_ERROR_UNEXPECTED;
}


class nsHttpConnectionForceRecv : public nsRunnable
{
public:
    nsHttpConnectionForceRecv(nsHttpConnection *aConn)
        : mConn(aConn) {}

    NS_IMETHOD Run()
    {
        MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

        if (!mConn->mSocketIn)
            return NS_OK;
        return mConn->OnInputStreamReady(mConn->mSocketIn);
    }
private:
    nsRefPtr<nsHttpConnection> mConn;
};

// trigger an asynchronous read
nsresult
nsHttpConnection::ForceRecv()
{
    LOG(("nsHttpConnection::ForceRecv [this=%p]\n", this));
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    return NS_DispatchToCurrentThread(new nsHttpConnectionForceRecv(this));
}

void
nsHttpConnection::BeginIdleMonitoring()
{
    LOG(("nsHttpConnection::BeginIdleMonitoring [this=%p]\n", this));
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(!mTransaction, "BeginIdleMonitoring() while active");
    MOZ_ASSERT(!mUsingSpdyVersion, "Idle monitoring of spdy not allowed");

    LOG(("Entering Idle Monitoring Mode [this=%p]", this));
    mIdleMonitoring = true;
    if (mSocketIn)
        mSocketIn->AsyncWait(this, 0, 0, nullptr);
}

void
nsHttpConnection::EndIdleMonitoring()
{
    LOG(("nsHttpConnection::EndIdleMonitoring [this=%p]\n", this));
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(!mTransaction, "EndIdleMonitoring() while active");

    if (mIdleMonitoring) {
        LOG(("Leaving Idle Monitoring Mode [this=%p]", this));
        mIdleMonitoring = false;
        if (mSocketIn)
            mSocketIn->AsyncWait(nullptr, 0, 0, nullptr);
    }
}

//-----------------------------------------------------------------------------
// nsHttpConnection <private>
//-----------------------------------------------------------------------------

void
nsHttpConnection::CloseTransaction(nsAHttpTransaction *trans, nsresult reason)
{
    LOG(("nsHttpConnection::CloseTransaction[this=%p trans=%x reason=%x]\n",
        this, trans, reason));

    MOZ_ASSERT(trans == mTransaction, "wrong transaction");
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (mCurrentBytesRead > mMaxBytesRead)
        mMaxBytesRead = mCurrentBytesRead;

    // mask this error code because its not a real error.
    if (reason == NS_BASE_STREAM_CLOSED)
        reason = NS_OK;

    if (mUsingSpdyVersion) {
        DontReuse();
        // if !mSpdySession then mUsingSpdyVersion must be false for canreuse()
        mUsingSpdyVersion = 0;
        mSpdySession = nullptr;
    }

    if (mTransaction) {
        mHttp1xTransactionCount += mTransaction->Http1xTransactionCount();

        mTransaction->Close(reason);
        mTransaction = nullptr;
    }

    {
        MutexAutoLock lock(mCallbacksLock);
        mCallbacks = nullptr;
    }

    if (NS_FAILED(reason))
        Close(reason);

    // flag the connection as reused here for convenience sake.  certainly
    // it might be going away instead ;-)
    mIsReused = true;
}

NS_METHOD
nsHttpConnection::ReadFromStream(nsIInputStream *input,
                                 void *closure,
                                 const char *buf,
                                 uint32_t offset,
                                 uint32_t count,
                                 uint32_t *countRead)
{
    // thunk for nsIInputStream instance
    nsHttpConnection *conn = (nsHttpConnection *) closure;
    return conn->OnReadSegment(buf, count, countRead);
}

nsresult
nsHttpConnection::OnReadSegment(const char *buf,
                                uint32_t count,
                                uint32_t *countRead)
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
    else {
        mSocketOutCondition = NS_OK; // reset condition
        if (!mProxyConnectInProgress)
            mTotalBytesWritten += *countRead;
    }

    return mSocketOutCondition;
}

nsresult
nsHttpConnection::OnSocketWritable()
{
    LOG(("nsHttpConnection::OnSocketWritable [this=%p] host=%s\n",
         this, mConnInfo->Host()));

    nsresult rv;
    uint32_t n;
    bool again = true;

    do {
        mSocketOutCondition = NS_OK;

        // If we're doing a proxy connect, then we need to bypass calling into
        // the transaction.
        //
        // NOTE: this code path can't be shared since the transaction doesn't
        // implement nsIInputStream.  doing so is not worth the added cost of
        // extra indirections during normal reading.
        //
        if (mProxyConnectStream) {
            LOG(("  writing CONNECT request stream\n"));
            rv = mProxyConnectStream->ReadSegments(ReadFromStream, this,
                                                      nsIOService::gDefaultSegmentSize,
                                                      &n);
        }
        else if (!EnsureNPNComplete()) {
            // When SPDY is disabled this branch is not executed because Activate()
            // sets mNPNComplete to true in that case.

            // We are ready to proceed with SSL but the handshake is not done.
            // When using NPN to negotiate between HTTPS and SPDY, we need to
            // see the results of the handshake to know what bytes to send, so
            // we cannot proceed with the request headers.

            rv = NS_OK;
            mSocketOutCondition = NS_BASE_STREAM_WOULD_BLOCK;
            n = 0;
        }
        else {
            if (!mReportedSpdy) {
                mReportedSpdy = true;
                gHttpHandler->ConnMgr()->ReportSpdyConnection(this, mEverUsedSpdy);
            }

            LOG(("  writing transaction request stream\n"));
            mProxyConnectInProgress = false;
            rv = mTransaction->ReadSegments(this, nsIOService::gDefaultSegmentSize, &n);
        }

        LOG(("  ReadSegments returned [rv=%x read=%u sock-cond=%x]\n",
            rv, n, mSocketOutCondition));

        // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
        if (rv == NS_BASE_STREAM_CLOSED && !mTransaction->IsDone()) {
            rv = NS_OK;
            n = 0;
        }

        if (NS_FAILED(rv)) {
            // if the transaction didn't want to write any more data, then
            // wait for the transaction to call ResumeSend.
            if (rv == NS_BASE_STREAM_WOULD_BLOCK)
                rv = NS_OK;
            again = false;
        }
        else if (NS_FAILED(mSocketOutCondition)) {
            if (mSocketOutCondition == NS_BASE_STREAM_WOULD_BLOCK)
                rv = mSocketOut->AsyncWait(this, 0, 0, nullptr); // continue writing
            else
                rv = mSocketOutCondition;
            again = false;
        }
        else if (n == 0) {
            rv = NS_OK;

            if (mTransaction) { // in case the ReadSegments stack called CloseTransaction()
                //
                // at this point we've written out the entire transaction, and now we
                // must wait for the server's response.  we manufacture a status message
                // here to reflect the fact that we are waiting.  this message will be
                // trumped (overwritten) if the server responds quickly.
                //
                mTransaction->OnTransportStatus(mSocketTransport,
                                                NS_NET_STATUS_WAITING_FOR,
                                                0);

                rv = ResumeRecv(); // start reading
            }
            again = false;
        }
        // write more to the socket until error or end-of-request...
    } while (again);

    return rv;
}

nsresult
nsHttpConnection::OnWriteSegment(char *buf,
                                 uint32_t count,
                                 uint32_t *countWritten)
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
    LOG(("nsHttpConnection::OnSocketReadable [this=%p]\n", this));

    PRIntervalTime now = PR_IntervalNow();
    PRIntervalTime delta = now - mLastReadTime;

    if (mKeepAliveMask && (delta >= mMaxHangTime)) {
        LOG(("max hang time exceeded!\n"));
        // give the handler a chance to create a new persistent connection to
        // this host if we've been busy for too long.
        mKeepAliveMask = false;
        gHttpHandler->ProcessPendingQ(mConnInfo);
    }

    // Look for data being sent in bursts with large pauses. If the pauses
    // are caused by server bottlenecks such as think-time, disk i/o, or
    // cpu exhaustion (as opposed to network latency) then we generate negative
    // pipelining feedback to prevent head of line problems

    // Reduce the estimate of the time since last read by up to 1 RTT to
    // accommodate exhausted sender TCP congestion windows or minor I/O delays.

    if (delta > mRtt)
        delta -= mRtt;
    else
        delta = 0;

    static const PRIntervalTime k400ms  = PR_MillisecondsToInterval(400);

    if (delta >= (mRtt + gHttpHandler->GetPipelineRescheduleTimeout())) {
        LOG(("Read delta ms of %u causing slow read major "
             "event and pipeline cancellation",
             PR_IntervalToMilliseconds(delta)));

        gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
            mConnInfo, nsHttpConnectionMgr::BadSlowReadMajor, this, 0);

        if (gHttpHandler->GetPipelineRescheduleOnTimeout() &&
            mTransaction->PipelineDepth() > 1) {
            nsHttpPipeline *pipeline = mTransaction->QueryPipeline();
            MOZ_ASSERT(pipeline, "pipelinedepth > 1 without pipeline");
            // code this defensively for the moment and check for null
            // This will reschedule blocked members of the pipeline, but the
            // blocking transaction (i.e. response 0) will not be changed.
            if (pipeline) {
                pipeline->CancelPipeline(NS_ERROR_NET_TIMEOUT);
                LOG(("Rescheduling the head of line blocked members of a "
                     "pipeline because reschedule-timeout idle interval "
                     "exceeded"));
            }
        }
    }
    else if (delta > k400ms) {
        gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
            mConnInfo, nsHttpConnectionMgr::BadSlowReadMinor, this, 0);
    }

    mLastReadTime = now;

    nsresult rv;
    uint32_t n;
    bool again = true;

    do {
        if (!mProxyConnectInProgress && !mNPNComplete) {
            // Unless we are setting up a tunnel via CONNECT, prevent reading
            // from the socket until the results of NPN
            // negotiation are known (which is determined from the write path).
            // If the server speaks SPDY it is likely the readable data here is
            // a spdy settings frame and without NPN it would be misinterpreted
            // as HTTP/*

            LOG(("nsHttpConnection::OnSocketReadable %p return due to inactive "
                 "tunnel setup but incomplete NPN state\n", this));
            rv = NS_OK;
            break;
        }

        rv = mTransaction->WriteSegments(this, nsIOService::gDefaultSegmentSize, &n);
        if (NS_FAILED(rv)) {
            // if the transaction didn't want to take any more data, then
            // wait for the transaction to call ResumeRecv.
            if (rv == NS_BASE_STREAM_WOULD_BLOCK)
                rv = NS_OK;
            again = false;
        }
        else {
            mCurrentBytesRead += n;
            mTotalBytesRead += n;
            if (NS_FAILED(mSocketInCondition)) {
                // continue waiting for the socket if necessary...
                if (mSocketInCondition == NS_BASE_STREAM_WOULD_BLOCK)
                    rv = ResumeRecv();
                else
                    rv = mSocketInCondition;
                again = false;
            }
        }
        // read more from the socket until error...
    } while (again);

    return rv;
}

nsresult
nsHttpConnection::SetupProxyConnect()
{
    const char *val;

    LOG(("nsHttpConnection::SetupProxyConnect [this=%p]\n", this));

    NS_ENSURE_TRUE(!mProxyConnectStream, NS_ERROR_ALREADY_INITIALIZED);
    MOZ_ASSERT(!mUsingSpdyVersion,
               "SPDY NPN Complete while using proxy connect stream");

    nsAutoCString buf;
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

    // a CONNECT is always persistent
    request.SetHeader(nsHttp::Proxy_Connection, NS_LITERAL_CSTRING("keep-alive"));
    request.SetHeader(nsHttp::Connection, NS_LITERAL_CSTRING("keep-alive"));

    val = mTransaction->RequestHead()->PeekHeader(nsHttp::Host);
    if (val) {
        // all HTTP/1.1 requests must include a Host header (even though it
        // may seem redundant in this case; see bug 82388).
        request.SetHeader(nsHttp::Host, nsDependentCString(val));
    }

    val = mTransaction->RequestHead()->PeekHeader(nsHttp::Proxy_Authorization);
    if (val) {
        // we don't know for sure if this authorization is intended for the
        // SSL proxy, so we add it just in case.
        request.SetHeader(nsHttp::Proxy_Authorization, nsDependentCString(val));
    }

    buf.Truncate();
    request.Flatten(buf, false);
    buf.AppendLiteral("\r\n");

    return NS_NewCStringInputStream(getter_AddRefs(mProxyConnectStream), buf);
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
    MOZ_ASSERT(in == mSocketIn, "unexpected stream");
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (mIdleMonitoring) {
        MOZ_ASSERT(!mTransaction, "Idle Input Event While Active");

        // The only read event that is protocol compliant for an idle connection
        // is an EOF, which we check for with CanReuse(). If the data is
        // something else then just ignore it and suspend checking for EOF -
        // our normal timers or protocol stack are the place to deal with
        // any exception logic.

        if (!CanReuse()) {
            LOG(("Server initiated close of idle conn %p\n", this));
            gHttpHandler->ConnMgr()->CloseIdleConnection(this);
            return NS_OK;
        }

        LOG(("Input data on idle conn %p, but not closing yet\n", this));
        return NS_OK;
    }

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
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);
    MOZ_ASSERT(out == mSocketOut, "unexpected socket");

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
                                    uint64_t progress,
                                    uint64_t progressMax)
{
    if (mTransaction)
        mTransaction->OnTransportStatus(trans, status, progress);
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

    // NOTE - there is a bug here, the call to getinterface is proxied off the
    // nss thread, not the ui thread as the above comment says. So there is
    // indeed a chance of mTransaction going away. bug 615342

    MOZ_ASSERT(PR_GetCurrentThread() != gSocketThread);

    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    {
        MutexAutoLock lock(mCallbacksLock);
        callbacks = mCallbacks;
    }
    if (callbacks)
        return callbacks->GetInterface(iid, result);
    return NS_ERROR_NO_INTERFACE;
}
