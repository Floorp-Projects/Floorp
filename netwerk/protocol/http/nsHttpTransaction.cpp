/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "base/basictypes.h"

#include "nsHttpHandler.h"
#include "nsHttpTransaction.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsHttpChunkedDecoder.h"
#include "nsTransportUtils.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"
#include "nsIPipe.h"
#include "nsCRT.h"
#include "mozilla/Tokenizer.h"
#include "TCPFastOpenLayer.h"

#include "nsISeekableStream.h"
#include "nsMultiplexInputStream.h"
#include "nsStringStream.h"

#include "nsComponentManagerUtils.h" // do_CreateInstance
#include "nsIHttpActivityObserver.h"
#include "nsSocketTransportService2.h"
#include "nsICancelable.h"
#include "nsIClassOfService.h"
#include "nsIEventTarget.h"
#include "nsIHttpChannelInternal.h"
#include "nsIInputStream.h"
#include "nsIThrottledInputChannel.h"
#include "nsITransport.h"
#include "nsIOService.h"
#include "nsIRequestContext.h"
#include "nsIHttpAuthenticator.h"
#include "NSSErrorsService.h"
#include "sslerr.h"
#include <algorithm>

//-----------------------------------------------------------------------------

static NS_DEFINE_CID(kMultiplexInputStream, NS_MULTIPLEXINPUTSTREAM_CID);

// Place a limit on how much non-compliant HTTP can be skipped while
// looking for a response header
#define MAX_INVALID_RESPONSE_BODY_SIZE (1024 * 128)

using namespace mozilla::net;

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

static void
LogHeaders(const char *lineStart)
{
    nsAutoCString buf;
    char *endOfLine;
    while ((endOfLine = PL_strstr(lineStart, "\r\n"))) {
        buf.Assign(lineStart, endOfLine - lineStart);
        if (PL_strcasestr(buf.get(), "authorization: ") ||
            PL_strcasestr(buf.get(), "proxy-authorization: ")) {
            char *p = PL_strchr(PL_strchr(buf.get(), ' ') + 1, ' ');
            while (p && *++p)
                *p = '*';
        }
        LOG3(("  %s\n", buf.get()));
        lineStart = endOfLine + 2;
    }
}

//-----------------------------------------------------------------------------
// nsHttpTransaction <public>
//-----------------------------------------------------------------------------

nsHttpTransaction::nsHttpTransaction()
    : mLock("transaction lock")
    , mRequestSize(0)
    , mRequestHead(nullptr)
    , mResponseHead(nullptr)
    , mReader(nullptr)
    , mWriter(nullptr)
    , mContentLength(-1)
    , mContentRead(0)
    , mTransferSize(0)
    , mInvalidResponseBytesRead(0)
    , mPushedStream(nullptr)
    , mInitialRwin(0)
    , mChunkedDecoder(nullptr)
    , mStatus(NS_OK)
    , mPriority(0)
    , mRestartCount(0)
    , mCaps(0)
    , mHttpVersion(NS_HTTP_VERSION_UNKNOWN)
    , mHttpResponseCode(0)
    , mCurrentHttpResponseHeaderSize(0)
    , mThrottlingReadAllowance(THROTTLE_NO_LIMIT)
    , mCapsToClear(0)
    , mResponseIsComplete(false)
    , mReadingStopped(false)
    , mClosed(false)
    , mConnected(false)
    , mActivated(false)
    , mHaveStatusLine(false)
    , mHaveAllHeaders(false)
    , mTransactionDone(false)
    , mDidContentStart(false)
    , mNoContent(false)
    , mSentData(false)
    , mReceivedData(false)
    , mStatusEventPending(false)
    , mHasRequestBody(false)
    , mProxyConnectFailed(false)
    , mHttpResponseMatched(false)
    , mPreserveStream(false)
    , mDispatchedAsBlocking(false)
    , mResponseTimeoutEnabled(true)
    , mForceRestart(false)
    , mReuseOnRestart(false)
    , mContentDecoding(false)
    , mContentDecodingCheck(false)
    , mDeferredSendProgress(false)
    , mWaitingOnPipeOut(false)
    , mReportedStart(false)
    , mReportedResponseHeader(false)
    , mForTakeResponseHead(nullptr)
    , mResponseHeadTaken(false)
    , mForTakeResponseTrailers(nullptr)
    , mResponseTrailersTaken(false)
    , mTopLevelOuterContentWindowId(0)
    , mSubmittedRatePacing(false)
    , mPassedRatePacing(false)
    , mSynchronousRatePaceRequest(false)
    , mClassOfService(0)
    , m0RTTInProgress(false)
    , mDoNotTryEarlyData(false)
    , mEarlyDataDisposition(EARLY_NONE)
    , mFastOpenStatus(TFO_NOT_TRIED)
{
    LOG(("Creating nsHttpTransaction @%p\n", this));

#ifdef MOZ_VALGRIND
    memset(&mSelfAddr, 0, sizeof(NetAddr));
    memset(&mPeerAddr, 0, sizeof(NetAddr));
#endif
    mSelfAddr.raw.family = PR_AF_UNSPEC;
    mPeerAddr.raw.family = PR_AF_UNSPEC;
}

void nsHttpTransaction::ResumeReading()
{
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    if (!mReadingStopped) {
        return;
    }

    LOG(("nsHttpTransaction::ResumeReading %p", this));

    mReadingStopped = false;

    // This with either reengage the limit when still throttled in WriteSegments or
    // simply reset to allow unlimeted reading again.
    mThrottlingReadAllowance = THROTTLE_NO_LIMIT;

    if (mConnection) {
        mConnection->TransactionHasDataToRecv(this);
        nsresult rv = mConnection->ResumeRecv();
        if (NS_FAILED(rv)) {
            LOG(("  resume failed with rv=%" PRIx32, static_cast<uint32_t>(rv)));
        }
    }
}

bool nsHttpTransaction::EligibleForThrottling() const
{
    return (mClassOfService & (nsIClassOfService::Throttleable |
                               nsIClassOfService::DontThrottle |
                               nsIClassOfService::Leader |
                               nsIClassOfService::Unblocked)) ==
        nsIClassOfService::Throttleable;
}

void nsHttpTransaction::SetClassOfService(uint32_t cos)
{
    bool wasThrottling = EligibleForThrottling();
    mClassOfService = cos;
    bool isThrottling = EligibleForThrottling();

    if (mConnection && wasThrottling != isThrottling) {
        // Do nothing until we are actually activated.  For now
        // only remember the throttle flag.  Call to UpdateActiveTransaction
        // would add this transaction to the list too early.
        gHttpHandler->ConnMgr()->UpdateActiveTransaction(this);

        if (mReadingStopped && !isThrottling) {
            ResumeReading();
        }
    }
}

nsHttpTransaction::~nsHttpTransaction()
{
    LOG(("Destroying nsHttpTransaction @%p\n", this));
    if (mTransactionObserver) {
        mTransactionObserver->Complete(this, NS_OK);
    }
    if (mPushedStream) {
        mPushedStream->OnPushFailed();
        mPushedStream = nullptr;
    }

    if (mTokenBucketCancel) {
        mTokenBucketCancel->Cancel(NS_ERROR_ABORT);
        mTokenBucketCancel = nullptr;
    }

    // Force the callbacks and connection to be released right now
    mCallbacks = nullptr;
    mConnection = nullptr;

    delete mResponseHead;
    delete mForTakeResponseHead;
    delete mChunkedDecoder;
    ReleaseBlockingTransaction();
}

nsresult
nsHttpTransaction::Init(uint32_t caps,
                        nsHttpConnectionInfo *cinfo,
                        nsHttpRequestHead *requestHead,
                        nsIInputStream *requestBody,
                        uint64_t requestContentLength,
                        bool requestBodyHasHeaders,
                        nsIEventTarget *target,
                        nsIInterfaceRequestor *callbacks,
                        nsITransportEventSink *eventsink,
                        uint64_t topLevelOuterContentWindowId,
                        nsIAsyncInputStream **responseBody)
{
    nsresult rv;

    LOG(("nsHttpTransaction::Init [this=%p caps=%x]\n", this, caps));

    MOZ_ASSERT(cinfo);
    MOZ_ASSERT(requestHead);
    MOZ_ASSERT(target);
    MOZ_ASSERT(NS_IsMainThread());

    mTopLevelOuterContentWindowId = topLevelOuterContentWindowId;
    LOG(("  window-id = %" PRIx64, mTopLevelOuterContentWindowId));

    mActivityDistributor = services::GetActivityDistributor();
    if (!mActivityDistributor) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    bool activityDistributorActive;
    rv = mActivityDistributor->GetIsActive(&activityDistributorActive);
    if (NS_SUCCEEDED(rv) && activityDistributorActive) {
        // there are some observers registered at activity distributor, gather
        // nsISupports for the channel that called Init()
        LOG(("nsHttpTransaction::Init() " \
             "mActivityDistributor is active " \
             "this=%p", this));
    } else {
        // there is no observer, so don't use it
        activityDistributorActive = false;
        mActivityDistributor = nullptr;
    }
    mChannel = do_QueryInterface(eventsink);

    nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
        do_QueryInterface(eventsink);
    if (httpChannelInternal) {
        rv = httpChannelInternal->GetResponseTimeoutEnabled(
            &mResponseTimeoutEnabled);
        if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
        }
        rv = httpChannelInternal->GetInitialRwin(&mInitialRwin);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
    }

    // create transport event sink proxy. it coalesces consecutive
    // events of the same status type.
    rv = net_NewTransportEventSinkProxy(getter_AddRefs(mTransportSink),
                                        eventsink, target);

    if (NS_FAILED(rv)) return rv;

    mConnInfo = cinfo;
    mCallbacks = callbacks;
    mConsumerTarget = target;
    mCaps = caps;

    if (requestHead->IsHead()) {
        mNoContent = true;
    }

    // Make sure that there is "Content-Length: 0" header in the requestHead
    // in case of POST and PUT methods when there is no requestBody and
    // requestHead doesn't contain "Transfer-Encoding" header.
    //
    // RFC1945 section 7.2.2:
    //   HTTP/1.0 requests containing an entity body must include a valid
    //   Content-Length header field.
    //
    // RFC2616 section 4.4:
    //   For compatibility with HTTP/1.0 applications, HTTP/1.1 requests
    //   containing a message-body MUST include a valid Content-Length header
    //   field unless the server is known to be HTTP/1.1 compliant.
    if ((requestHead->IsPost() || requestHead->IsPut()) &&
        !requestBody && !requestHead->HasHeader(nsHttp::Transfer_Encoding)) {
        rv = requestHead->SetHeader(nsHttp::Content_Length, NS_LITERAL_CSTRING("0"));
        MOZ_ASSERT(NS_SUCCEEDED(rv));
    }

    // grab a weak reference to the request head
    mRequestHead = requestHead;

    // make sure we eliminate any proxy specific headers from
    // the request if we are using CONNECT
    bool pruneProxyHeaders = cinfo->UsingConnect();

    mReqHeaderBuf.Truncate();
    requestHead->Flatten(mReqHeaderBuf, pruneProxyHeaders);

    if (LOG3_ENABLED()) {
        LOG3(("http request [\n"));
        LogHeaders(mReqHeaderBuf.get());
        LOG3(("]\n"));
    }

    // If the request body does not include headers or if there is no request
    // body, then we must add the header/body separator manually.
    if (!requestBodyHasHeaders || !requestBody)
        mReqHeaderBuf.AppendLiteral("\r\n");

    // report the request header
    if (mActivityDistributor) {
        rv = mActivityDistributor->ObserveActivity(
            mChannel,
            NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
            NS_HTTP_ACTIVITY_SUBTYPE_REQUEST_HEADER,
            PR_Now(), 0,
            mReqHeaderBuf);
        if (NS_FAILED(rv)) {
            LOG3(("ObserveActivity failed (%08x)", static_cast<uint32_t>(rv)));
        }
    }

    // Create a string stream for the request header buf (the stream holds
    // a non-owning reference to the request header data, so we MUST keep
    // mReqHeaderBuf around).
    nsCOMPtr<nsIInputStream> headers;
    rv = NS_NewByteInputStream(getter_AddRefs(headers),
                               mReqHeaderBuf.get(),
                               mReqHeaderBuf.Length());
    if (NS_FAILED(rv)) return rv;

    mHasRequestBody = !!requestBody;
    if (mHasRequestBody && !requestContentLength) {
        mHasRequestBody = false;
    }

    requestContentLength += mReqHeaderBuf.Length();

    if (mHasRequestBody) {
        // wrap the headers and request body in a multiplexed input stream.
        nsCOMPtr<nsIMultiplexInputStream> multi =
            do_CreateInstance(kMultiplexInputStream, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = multi->AppendStream(headers);
        if (NS_FAILED(rv)) return rv;

        rv = multi->AppendStream(requestBody);
        if (NS_FAILED(rv)) return rv;

        // wrap the multiplexed input stream with a buffered input stream, so
        // that we write data in the largest chunks possible.  this is actually
        // necessary to workaround some common server bugs (see bug 137155).
        nsCOMPtr<nsIInputStream> stream(do_QueryInterface(multi));
        rv = NS_NewBufferedInputStream(getter_AddRefs(mRequestStream),
                                       stream.forget(),
                                       nsIOService::gDefaultSegmentSize);
        if (NS_FAILED(rv)) return rv;
    } else {
        mRequestStream = headers;
    }

    nsCOMPtr<nsIThrottledInputChannel> throttled = do_QueryInterface(mChannel);
    nsIInputChannelThrottleQueue* queue;
    if (throttled) {
        rv = throttled->GetThrottleQueue(&queue);
        // In case of failure, just carry on without throttling.
        if (NS_SUCCEEDED(rv) && queue) {
            nsCOMPtr<nsIAsyncInputStream> wrappedStream;
            rv = queue->WrapStream(mRequestStream, getter_AddRefs(wrappedStream));
            // Failure to throttle isn't sufficient reason to fail
            // initialization
            if (NS_SUCCEEDED(rv)) {
                MOZ_ASSERT(wrappedStream != nullptr);
                LOG(("nsHttpTransaction::Init %p wrapping input stream using throttle queue %p\n",
                     this, queue));
                mRequestStream = do_QueryInterface(wrappedStream);
            }
        }
    }

    // make sure request content-length fits within js MAX_SAFE_INTEGER
    mRequestSize = InScriptableRange(requestContentLength) ? static_cast<int64_t>(requestContentLength) : -1;

    // create pipe for response stream
    rv = NS_NewPipe2(getter_AddRefs(mPipeIn),
                     getter_AddRefs(mPipeOut),
                     true, true,
                     nsIOService::gDefaultSegmentSize,
                     nsIOService::gDefaultSegmentCount);
    if (NS_FAILED(rv)) return rv;

#ifdef WIN32 // bug 1153929
    MOZ_DIAGNOSTIC_ASSERT(mPipeOut);
    uint32_t * vtable = (uint32_t *) mPipeOut.get();
    MOZ_DIAGNOSTIC_ASSERT(*vtable != 0);
#endif // WIN32

    nsCOMPtr<nsIAsyncInputStream> tmp(mPipeIn);
    tmp.forget(responseBody);
    return NS_OK;
}

// This method should only be used on the socket thread
nsAHttpConnection *
nsHttpTransaction::Connection()
{
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    return mConnection.get();
}

already_AddRefed<nsAHttpConnection>
nsHttpTransaction::GetConnectionReference()
{
    MutexAutoLock lock(mLock);
    RefPtr<nsAHttpConnection> connection(mConnection);
    return connection.forget();
}

nsHttpResponseHead *
nsHttpTransaction::TakeResponseHead()
{
    MOZ_ASSERT(!mResponseHeadTaken, "TakeResponseHead called 2x");

    // Lock TakeResponseHead() against main thread
    MutexAutoLock lock(*nsHttp::GetLock());

    mResponseHeadTaken = true;

    // Prefer mForTakeResponseHead over mResponseHead. It is always a complete
    // set of headers.
    nsHttpResponseHead *head;
    if (mForTakeResponseHead) {
        head = mForTakeResponseHead;
        mForTakeResponseHead = nullptr;
        return head;
    }

    // Even in OnStartRequest() the headers won't be available if we were
    // canceled
    if (!mHaveAllHeaders) {
        NS_WARNING("response headers not available or incomplete");
        return nullptr;
    }

    head = mResponseHead;
    mResponseHead = nullptr;
    return head;
}

nsHttpHeaderArray *
nsHttpTransaction::TakeResponseTrailers()
{
    MOZ_ASSERT(!mResponseTrailersTaken, "TakeResponseTrailers called 2x");

    // Lock TakeResponseTrailers() against main thread
    MutexAutoLock lock(*nsHttp::GetLock());

    mResponseTrailersTaken = true;
    return mForTakeResponseTrailers.forget();
}

void
nsHttpTransaction::SetProxyConnectFailed()
{
    mProxyConnectFailed = true;
}

nsHttpRequestHead *
nsHttpTransaction::RequestHead()
{
    return mRequestHead;
}

uint32_t
nsHttpTransaction::Http1xTransactionCount()
{
  return 1;
}

nsresult
nsHttpTransaction::TakeSubTransactions(
    nsTArray<RefPtr<nsAHttpTransaction> > &outTransactions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------
// nsHttpTransaction::nsAHttpTransaction
//----------------------------------------------------------------------------

void
nsHttpTransaction::SetConnection(nsAHttpConnection *conn)
{
    {
        MutexAutoLock lock(mLock);
        mConnection = conn;
    }
}

void
nsHttpTransaction::OnActivated()
{
    MOZ_ASSERT(OnSocketThread());

    if (mActivated) {
        return;
    }

    if (mConnection && mRequestHead) {
        // So this is fun. On http/2, we want to send TE: Trailers, to be
        // spec-compliant. So we add it to the request head here. The fun part
        // is that adding a header to the request head at this point has no
        // effect on what we send on the wire, as the headers are already
        // flattened (in Init()) by the time we get here. So the *real* adding
        // of the header happens in the h2 compression code. We still have to
        // add the header to the request head here, though, so that devtools can
        // show that we sent the header. FUN!
        // Oh, and we can't just check for version >= NS_HTTP_VERSION_2_0 because
        // right now, mConnection->Version() returns HTTP_VERSION_2 (=5) instead
        // of NS_HTTP_VERSION_2_0 (=20) for... reasons.
        bool isOldHttp = (mConnection->Version() == NS_HTTP_VERSION_0_9 ||
                          mConnection->Version() == NS_HTTP_VERSION_1_0 ||
                          mConnection->Version() == NS_HTTP_VERSION_1_1 ||
                          mConnection->Version() == NS_HTTP_VERSION_UNKNOWN);
        if (!isOldHttp) {
            Unused << mRequestHead->SetHeader(nsHttp::TE, NS_LITERAL_CSTRING("Trailers"));
        }
    }

    mActivated = true;
    gHttpHandler->ConnMgr()->AddActiveTransaction(this);
}

void
nsHttpTransaction::GetSecurityCallbacks(nsIInterfaceRequestor **cb)
{
    MutexAutoLock lock(mLock);
    nsCOMPtr<nsIInterfaceRequestor> tmp(mCallbacks);
    tmp.forget(cb);
}

void
nsHttpTransaction::SetSecurityCallbacks(nsIInterfaceRequestor* aCallbacks)
{
    {
        MutexAutoLock lock(mLock);
        mCallbacks = aCallbacks;
    }

    if (gSocketTransportService) {
        RefPtr<UpdateSecurityCallbacks> event = new UpdateSecurityCallbacks(this, aCallbacks);
        gSocketTransportService->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
    }
}

void
nsHttpTransaction::OnTransportStatus(nsITransport* transport,
                                     nsresult status, int64_t progress)
{
    LOG(("nsHttpTransaction::OnSocketStatus [this=%p status=%" PRIx32 " progress=%" PRId64 "]\n",
         this, static_cast<uint32_t>(status), progress));

    if (status == NS_NET_STATUS_CONNECTED_TO ||
        status == NS_NET_STATUS_WAITING_FOR) {
        nsISocketTransport *socketTransport =
            mConnection ? mConnection->Transport() : nullptr;
        if (socketTransport) {
            MutexAutoLock lock(mLock);
            socketTransport->GetSelfAddr(&mSelfAddr);
            socketTransport->GetPeerAddr(&mPeerAddr);
        }
    }

    // If the timing is enabled, and we are not using a persistent connection
    // then the requestStart timestamp will be null, so we mark the timestamps
    // for domainLookupStart/End and connectStart/End
    // If we are using a persistent connection they will remain null,
    // and the correct value will be returned in Performance.
    if (TimingEnabled() && GetRequestStart().IsNull()) {
        if (status == NS_NET_STATUS_RESOLVING_HOST) {
            SetDomainLookupStart(TimeStamp::Now(), true);
        } else if (status == NS_NET_STATUS_RESOLVED_HOST) {
            SetDomainLookupEnd(TimeStamp::Now());
        } else if (status == NS_NET_STATUS_CONNECTING_TO) {
            SetConnectStart(TimeStamp::Now());
        } else if (status == NS_NET_STATUS_CONNECTED_TO) {
            TimeStamp tnow = TimeStamp::Now();
            SetConnectEnd(tnow, true);
            {
                MutexAutoLock lock(mLock);
                mTimings.tcpConnectEnd = tnow;
                // After a socket is connected we know for sure whether data
                // has been sent on SYN packet and if not we should update TLS
                // start timing.
                if ((mFastOpenStatus != TFO_DATA_SENT) && 
                    !mTimings.secureConnectionStart.IsNull()) {
                    mTimings.secureConnectionStart = tnow;
                }
            }
        } else if (status == NS_NET_STATUS_TLS_HANDSHAKE_STARTING) {
            {
                MutexAutoLock lock(mLock);
                mTimings.secureConnectionStart = TimeStamp::Now();
            }
        } else if (status == NS_NET_STATUS_TLS_HANDSHAKE_ENDED) {
            SetConnectEnd(TimeStamp::Now(), false);
        } else if (status == NS_NET_STATUS_SENDING_TO) {
            // Set the timestamp to Now(), only if it null
            SetRequestStart(TimeStamp::Now(), true);
        }
    }

    if (!mTransportSink)
        return;

    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    // Need to do this before the STATUS_RECEIVING_FROM check below, to make
    // sure that the activity distributor gets told about all status events.
    if (mActivityDistributor) {
        // upon STATUS_WAITING_FOR; report request body sent
        if ((mHasRequestBody) &&
            (status == NS_NET_STATUS_WAITING_FOR)) {
            nsresult rv = mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_REQUEST_BODY_SENT,
                PR_Now(), 0, EmptyCString());
            if (NS_FAILED(rv)) {
                LOG3(("ObserveActivity failed (%08x)", static_cast<uint32_t>(rv)));
            }
        }

        // report the status and progress
        nsresult rv = mActivityDistributor->ObserveActivity(
            mChannel,
            NS_HTTP_ACTIVITY_TYPE_SOCKET_TRANSPORT,
            static_cast<uint32_t>(status),
            PR_Now(),
            progress,
            EmptyCString());
        if (NS_FAILED(rv)) {
            LOG3(("ObserveActivity failed (%08x)", static_cast<uint32_t>(rv)));
        }
    }

    // nsHttpChannel synthesizes progress events in OnDataAvailable
    if (status == NS_NET_STATUS_RECEIVING_FROM)
        return;

    int64_t progressMax;

    if (status == NS_NET_STATUS_SENDING_TO) {
        // suppress progress when only writing request headers
        if (!mHasRequestBody) {
            LOG(("nsHttpTransaction::OnTransportStatus %p "
                 "SENDING_TO without request body\n", this));
            return;
        }

        if (mReader) {
            // A mRequestStream method is on the stack - wait.
            LOG(("nsHttpTransaction::OnSocketStatus [this=%p] "
                 "Skipping Re-Entrant NS_NET_STATUS_SENDING_TO\n", this));
            // its ok to coalesce several of these into one deferred event
            mDeferredSendProgress = true;
            return;
        }

        nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mRequestStream);
        if (!seekable) {
            LOG(("nsHttpTransaction::OnTransportStatus %p "
                 "SENDING_TO without seekable request stream\n", this));
            progress = 0;
        } else {
            int64_t prog = 0;
            seekable->Tell(&prog);
            progress = prog;
        }

        // when uploading, we include the request headers in the progress
        // notifications.
        progressMax = mRequestSize;
    }
    else {
        progress = 0;
        progressMax = 0;
    }

    mTransportSink->OnTransportStatus(transport, status, progress, progressMax);
}

bool
nsHttpTransaction::IsDone()
{
    return mTransactionDone;
}

nsresult
nsHttpTransaction::Status()
{
    return mStatus;
}

uint32_t
nsHttpTransaction::Caps()
{
    return mCaps & ~mCapsToClear;
}

void
nsHttpTransaction::SetDNSWasRefreshed()
{
    MOZ_ASSERT(NS_IsMainThread(), "SetDNSWasRefreshed on main thread only!");
    mCapsToClear |= NS_HTTP_REFRESH_DNS;
}

nsresult
nsHttpTransaction::ReadRequestSegment(nsIInputStream *stream,
                                      void *closure,
                                      const char *buf,
                                      uint32_t offset,
                                      uint32_t count,
                                      uint32_t *countRead)
{
    // For the tracking of sent bytes that we used to do for the networkstats
    // API, please see bug 1318883 where it was removed.

    nsHttpTransaction *trans = (nsHttpTransaction *) closure;
    nsresult rv = trans->mReader->OnReadSegment(buf, count, countRead);
    if (NS_FAILED(rv)) return rv;

    LOG(("nsHttpTransaction::ReadRequestSegment %p read=%u", trans, *countRead));

    trans->mSentData = true;
    return NS_OK;
}

nsresult
nsHttpTransaction::ReadSegments(nsAHttpSegmentReader *reader,
                                uint32_t count, uint32_t *countRead)
{
    LOG(("nsHttpTransaction::ReadSegments %p", this));

    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    if (mTransactionDone) {
        *countRead = 0;
        return mStatus;
    }

    if (!mConnected && !m0RTTInProgress) {
        mConnected = true;
        mConnection->GetSecurityInfo(getter_AddRefs(mSecurityInfo));
    }

    mDeferredSendProgress = false;
    mReader = reader;
    nsresult rv = mRequestStream->ReadSegments(ReadRequestSegment, this, count, countRead);
    mReader = nullptr;

    if (m0RTTInProgress && (mEarlyDataDisposition == EARLY_NONE) &&
        NS_SUCCEEDED(rv) && (*countRead > 0)) {
        mEarlyDataDisposition = EARLY_SENT;
    }

    if (mDeferredSendProgress && mConnection && mConnection->Transport()) {
        // to avoid using mRequestStream concurrently, OnTransportStatus()
        // did not report upload status off the ReadSegments() stack from nsSocketTransport
        // do it now.
        OnTransportStatus(mConnection->Transport(), NS_NET_STATUS_SENDING_TO, 0);
    }
    mDeferredSendProgress = false;

    if (mForceRestart) {
        // The forceRestart condition was dealt with on the stack, but it did not
        // clear the flag because nsPipe in the readsegment stack clears out
        // return codes, so we need to use the flag here as a cue to return ERETARGETED
        if (NS_SUCCEEDED(rv)) {
            rv = NS_BINDING_RETARGETED;
        }
        mForceRestart = false;
    }

    // if read would block then we need to AsyncWait on the request stream.
    // have callback occur on socket thread so we stay synchronized.
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        nsCOMPtr<nsIAsyncInputStream> asyncIn =
                do_QueryInterface(mRequestStream);
        if (asyncIn) {
            nsCOMPtr<nsIEventTarget> target;
            Unused << gHttpHandler->GetSocketThreadTarget(getter_AddRefs(target));
            if (target)
                asyncIn->AsyncWait(this, 0, 0, target);
            else {
                NS_ERROR("no socket thread event target");
                rv = NS_ERROR_UNEXPECTED;
            }
        }
    }

    return rv;
}

nsresult
nsHttpTransaction::WritePipeSegment(nsIOutputStream *stream,
                                    void *closure,
                                    char *buf,
                                    uint32_t offset,
                                    uint32_t count,
                                    uint32_t *countWritten)
{
    nsHttpTransaction *trans = (nsHttpTransaction *) closure;

    if (trans->mTransactionDone)
        return NS_BASE_STREAM_CLOSED; // stop iterating

    if (trans->TimingEnabled()) {
        // Set the timestamp to Now(), only if it null
        trans->SetResponseStart(TimeStamp::Now(), true);
    }

    // Bug 1153929 - add checks to fix windows crash
    MOZ_ASSERT(trans->mWriter);
    if (!trans->mWriter) {
        return NS_ERROR_UNEXPECTED;
    }

    nsresult rv;
    //
    // OK, now let the caller fill this segment with data.
    //
    rv = trans->mWriter->OnWriteSegment(buf, count, countWritten);
    if (NS_FAILED(rv)) return rv; // caller didn't want to write anything

    LOG(("nsHttpTransaction::WritePipeSegment %p written=%u", trans, *countWritten));

    MOZ_ASSERT(*countWritten > 0, "bad writer");
    trans->mReceivedData = true;
    trans->mTransferSize += *countWritten;

    // Let the transaction "play" with the buffer.  It is free to modify
    // the contents of the buffer and/or modify countWritten.
    // - Bytes in HTTP headers don't count towards countWritten, so the input
    // side of pipe (aka nsHttpChannel's mTransactionPump) won't hit
    // OnInputStreamReady until all headers have been parsed.
    //
    rv = trans->ProcessData(buf, *countWritten, countWritten);
    if (NS_FAILED(rv))
        trans->Close(rv);

    return rv; // failure code only stops WriteSegments; it is not propagated.
}

bool nsHttpTransaction::ShouldThrottle()
{
    if (mClassOfService & nsIClassOfService::DontThrottle) {
        // We deliberately don't touch the throttling window here since
        // DontThrottle requests are expected to be long-standing media
        // streams and would just unnecessarily block running downloads.
        // If we want to ballance bandwidth for media responses against
        // running downloads, we need to find something smarter like 
        // changing the suspend/resume throttling intervals at-runtime.
        return false;
    }

    if (!gHttpHandler->ConnMgr()->ShouldThrottle(this)) {
        // We are not obligated to throttle
        return false;
    }

    if (mContentRead < 16000) {
        // Let the first bytes go, it may also well be all the content we get
        LOG(("nsHttpTransaction::ShouldThrottle too few content (%" PRIi64 ") this=%p", mContentRead, this));
        return false;
    }

    if (!(mClassOfService & nsIClassOfService::Throttleable) &&
        gHttpHandler->ConnMgr()->IsConnEntryUnderPressure(mConnInfo)) {
        LOG(("nsHttpTransaction::ShouldThrottle entry pressure this=%p", this));
        // This is expensive to check (two hashtable lookups) but may help
        // freeing connections for active tab transactions.
        // Checking this only for transactions that are not explicitly marked
        // as throttleable because trackers and (specially) downloads should
        // keep throttling even under pressure.
        return false;
    }

    return true;
}

nsresult
nsHttpTransaction::WriteSegments(nsAHttpSegmentWriter *writer,
                                 uint32_t count, uint32_t *countWritten)
{
    LOG(("nsHttpTransaction::WriteSegments %p", this));

    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    if (mTransactionDone) {
        return NS_SUCCEEDED(mStatus) ? NS_BASE_STREAM_CLOSED : mStatus;
    }

    if (ShouldThrottle()) {
        if (mThrottlingReadAllowance == THROTTLE_NO_LIMIT) { // no limit set
            // V1: ThrottlingReadLimit() returns 0
            mThrottlingReadAllowance = gHttpHandler->ThrottlingReadLimit();
        }
    } else {
        mThrottlingReadAllowance = THROTTLE_NO_LIMIT; // don't limit
    }

    if (mThrottlingReadAllowance == 0) { // depleted
        if (gHttpHandler->ConnMgr()->CurrentTopLevelOuterContentWindowId() !=
            mTopLevelOuterContentWindowId) {
            nsHttp::NotifyActiveTabLoadOptimization();
        }

        // Must remember that we have to call ResumeRecv() on our connection when
        // called back by the conn manager to resume reading.
        LOG(("nsHttpTransaction::WriteSegments %p response throttled", this));
        mReadingStopped = true;
        // This makes the underlaying connection or stream wait for explicit resume.
        // For h1 this means we stop reading from the socket.
        // For h2 this means we stop updating recv window for the stream.
        return NS_BASE_STREAM_WOULD_BLOCK;
    }

    mWriter = writer;

#ifdef WIN32 // bug 1153929
    MOZ_DIAGNOSTIC_ASSERT(mPipeOut);
    uint32_t * vtable = (uint32_t *) mPipeOut.get();
    MOZ_DIAGNOSTIC_ASSERT(*vtable != 0);
#endif // WIN32

    if (!mPipeOut) {
        return NS_ERROR_UNEXPECTED;
    }

    if (mThrottlingReadAllowance > 0) {
        LOG(("nsHttpTransaction::WriteSegments %p limiting read from %u to %d",
             this, count, mThrottlingReadAllowance));
        count = std::min(count, static_cast<uint32_t>(mThrottlingReadAllowance));
    }

    nsresult rv = mPipeOut->WriteSegments(WritePipeSegment, this, count, countWritten);

    mWriter = nullptr;

    if (mForceRestart) {
        // The forceRestart condition was dealt with on the stack, but it did not
        // clear the flag because nsPipe in the writesegment stack clears out
        // return codes, so we need to use the flag here as a cue to return ERETARGETED
        if (NS_SUCCEEDED(rv)) {
            rv = NS_BINDING_RETARGETED;
        }
        mForceRestart = false;
    }

    // if pipe would block then we need to AsyncWait on it.  have callback
    // occur on socket thread so we stay synchronized.
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        nsCOMPtr<nsIEventTarget> target;
        Unused << gHttpHandler->GetSocketThreadTarget(getter_AddRefs(target));
        if (target) {
            mPipeOut->AsyncWait(this, 0, 0, target);
            mWaitingOnPipeOut = true;
        } else {
            NS_ERROR("no socket thread event target");
            rv = NS_ERROR_UNEXPECTED;
        }
    } else if (mThrottlingReadAllowance > 0 && NS_SUCCEEDED(rv)) {
        MOZ_ASSERT(count >= *countWritten);
        mThrottlingReadAllowance -= *countWritten;
    }

    return rv;
}

void
nsHttpTransaction::Close(nsresult reason)
{
    LOG(("nsHttpTransaction::Close [this=%p reason=%" PRIx32 "]\n",
         this, static_cast<uint32_t>(reason)));

    if (!mClosed) {
        gHttpHandler->ConnMgr()->RemoveActiveTransaction(this);
        mActivated = false;
    }

    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    if (reason == NS_BINDING_RETARGETED) {
        LOG(("  close %p skipped due to ERETARGETED\n", this));
        return;
    }

    if (mClosed) {
        LOG(("  already closed\n"));
        return;
    }

    if (mTransactionObserver) {
        mTransactionObserver->Complete(this, reason);
        mTransactionObserver = nullptr;
    }

    if (mTokenBucketCancel) {
        mTokenBucketCancel->Cancel(reason);
        mTokenBucketCancel = nullptr;
    }

    if (mActivityDistributor) {
        // report the reponse is complete if not already reported
        if (!mResponseIsComplete) {
            nsresult rv = mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_COMPLETE,
                PR_Now(),
                static_cast<uint64_t>(mContentRead),
                EmptyCString());
            if (NS_FAILED(rv)) {
                LOG3(("ObserveActivity failed (%08x)", static_cast<uint32_t>(rv)));
            }
        }

        // report that this transaction is closing
        nsresult rv = mActivityDistributor->ObserveActivity(
            mChannel,
            NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
            NS_HTTP_ACTIVITY_SUBTYPE_TRANSACTION_CLOSE,
            PR_Now(), 0, EmptyCString());
        if (NS_FAILED(rv)) {
            LOG3(("ObserveActivity failed (%08x)", static_cast<uint32_t>(rv)));
        }
    }

    // we must no longer reference the connection!  find out if the
    // connection was being reused before letting it go.
    bool connReused = false;
    if (mConnection) {
        connReused = mConnection->IsReused();
    }
    mConnected = false;
    mTunnelProvider = nullptr;

    //
    // if the connection was reset or closed before we wrote any part of the
    // request or if we wrote the request but didn't receive any part of the
    // response and the connection was being reused, then we can (and really
    // should) assume that we wrote to a stale connection and we must therefore
    // repeat the request over a new connection.
    //
    // We have decided to retry not only in case of the reused connections, but
    // all safe methods(bug 1236277).
    //
    // NOTE: the conditions under which we will automatically retry the HTTP
    // request have to be carefully selected to avoid duplication of the
    // request from the point-of-view of the server.  such duplication could
    // have dire consequences including repeated purchases, etc.
    //
    // NOTE: because of the way SSL proxy CONNECT is implemented, it is
    // possible that the transaction may have received data without having
    // sent any data.  for this reason, mSendData == FALSE does not imply
    // mReceivedData == FALSE.  (see bug 203057 for more info.)
    //
    // Never restart transactions that are marked as sticky to their conenction.
    // We use that capability to identify transactions bound to connection based
    // authentication.  Reissuing them on a different connections will break
    // this bondage.  Major issue may arise when there is an NTLM message auth
    // header on the transaction and we send it to a different NTLM authenticated
    // connection.  It will break that connection and also confuse the channel's
    // auth provider, beliving the cached credentials are wrong and asking for
    // the password mistakenly again from the user.
    if ((reason == NS_ERROR_NET_RESET ||
         reason == NS_OK ||
         reason == psm::GetXPCOMFromNSSError(SSL_ERROR_DOWNGRADE_WITH_EARLY_DATA)) &&
        (!(mCaps & NS_HTTP_STICKY_CONNECTION) || (mCaps & NS_HTTP_CONNECTION_RESTARTABLE))) {

        if (mForceRestart && NS_SUCCEEDED(Restart())) {
            if (mResponseHead) {
                mResponseHead->Reset();
            }
            mContentRead = 0;
            mContentLength = -1;
            delete mChunkedDecoder;
            mChunkedDecoder = nullptr;
            mHaveStatusLine = false;
            mHaveAllHeaders = false;
            mHttpResponseMatched = false;
            mResponseIsComplete = false;
            mDidContentStart = false;
            mNoContent = false;
            mSentData = false;
            mReceivedData = false;
            LOG(("transaction force restarted\n"));
            return;
        }

        // reallySentData is meant to separate the instances where data has
        // been sent by this transaction but buffered at a higher level while
        // a TLS session (perhaps via a tunnel) is setup.
        bool reallySentData =
            mSentData && (!mConnection || mConnection->BytesWritten());

        if (reason == psm::GetXPCOMFromNSSError(SSL_ERROR_DOWNGRADE_WITH_EARLY_DATA) ||
            (!mReceivedData &&
            ((mRequestHead && mRequestHead->IsSafeMethod()) ||
             !reallySentData || connReused))) {
            // if restarting fails, then we must proceed to close the pipe,
            // which will notify the channel that the transaction failed.

            if (NS_SUCCEEDED(Restart()))
                return;
        }
    }

    if ((mChunkedDecoder || (mContentLength >= int64_t(0))) &&
        (NS_SUCCEEDED(reason) && !mResponseIsComplete)) {

        NS_WARNING("Partial transfer, incomplete HTTP response received");

        if ((mHttpResponseCode / 100 == 2) &&
            (mHttpVersion >= NS_HTTP_VERSION_1_1)) {
            FrameCheckLevel clevel = gHttpHandler->GetEnforceH1Framing();
            if (clevel >= FRAMECHECK_BARELY) {
                if ((clevel == FRAMECHECK_STRICT) ||
                    (mChunkedDecoder && mChunkedDecoder->GetChunkRemaining()) ||
                    (!mChunkedDecoder && !mContentDecoding && mContentDecodingCheck) ) {
                    reason = NS_ERROR_NET_PARTIAL_TRANSFER;
                    LOG(("Partial transfer, incomplete HTTP response received: %s",
                         mChunkedDecoder ? "broken chunk" : "c-l underrun"));
                }
            }
        }

        if (mConnection) {
            // whether or not we generate an error for the transaction
            // bad framing means we don't want a pconn
            mConnection->DontReuse();
        }
    }

    bool relConn = true;
    if (NS_SUCCEEDED(reason)) {

        // the server has not sent the final \r\n terminating the header
        // section, and there may still be a header line unparsed.  let's make
        // sure we parse the remaining header line, and then hopefully, the
        // response will be usable (see bug 88792).
        if (!mHaveAllHeaders) {
            char data = '\n';
            uint32_t unused;
            Unused << ParseHead(&data, 1, &unused);

            if (mResponseHead->Version() == NS_HTTP_VERSION_0_9) {
                // Reject 0 byte HTTP/0.9 Responses - bug 423506
                LOG(("nsHttpTransaction::Close %p 0 Byte 0.9 Response", this));
                reason = NS_ERROR_NET_RESET;
            }
        }

        // honor the sticky connection flag...
        if (mCaps & NS_HTTP_STICKY_CONNECTION)
            relConn = false;
    }

    // mTimings.responseEnd is normally recorded based on the end of a
    // HTTP delimiter such as chunked-encodings or content-length. However,
    // EOF or an error still require an end time be recorded.
    if (TimingEnabled()) {
        const TimingStruct timings = Timings();
        if (timings.responseEnd.IsNull() && !timings.responseStart.IsNull()) {
            SetResponseEnd(TimeStamp::Now());
        }
    }

    if (relConn && mConnection) {
        MutexAutoLock lock(mLock);
        mConnection = nullptr;
    }

    mStatus = reason;
    mTransactionDone = true; // forcibly flag the transaction as complete
    mClosed = true;
    ReleaseBlockingTransaction();

    // release some resources that we no longer need
    mRequestStream = nullptr;
    mReqHeaderBuf.Truncate();
    mLineBuf.Truncate();
    if (mChunkedDecoder) {
        delete mChunkedDecoder;
        mChunkedDecoder = nullptr;
    }

    // closing this pipe triggers the channel's OnStopRequest method.
    mPipeOut->CloseWithStatus(reason);

#ifdef WIN32 // bug 1153929
    MOZ_DIAGNOSTIC_ASSERT(mPipeOut);
    uint32_t * vtable = (uint32_t *) mPipeOut.get();
    MOZ_DIAGNOSTIC_ASSERT(*vtable != 0);
    mPipeOut = nullptr; // just in case
#endif // WIN32
}

nsHttpConnectionInfo *
nsHttpTransaction::ConnectionInfo()
{
    return mConnInfo.get();
}

bool // NOTE BASE CLASS
nsAHttpTransaction::ResponseTimeoutEnabled() const
{
    return false;
}

PRIntervalTime // NOTE BASE CLASS
nsAHttpTransaction::ResponseTimeout()
{
    return gHttpHandler->ResponseTimeout();
}

bool
nsHttpTransaction::ResponseTimeoutEnabled() const
{
    return mResponseTimeoutEnabled;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpTransaction::Restart()
{
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    // limit the number of restart attempts - bug 92224
    if (++mRestartCount >= gHttpHandler->MaxRequestAttempts()) {
        LOG(("reached max request attempts, failing transaction @%p\n", this));
        return NS_ERROR_NET_RESET;
    }

    LOG(("restarting transaction @%p\n", this));
    mTunnelProvider = nullptr;

    // rewind streams in case we already wrote out the request
    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mRequestStream);
    if (seekable)
        seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);

    // clear old connection state...
    mSecurityInfo = nullptr;
    if (mConnection) {
        if (!mReuseOnRestart) {
            mConnection->DontReuse();
        }
        MutexAutoLock lock(mLock);
        mConnection = nullptr;
    }

    // Reset this to our default state, since this may change from one restart
    // to the next
    mReuseOnRestart = false;

    if (!mConnInfo->GetRoutedHost().IsEmpty()) {
        MutexAutoLock lock(*nsHttp::GetLock());
        RefPtr<nsHttpConnectionInfo> ci;
         mConnInfo->CloneAsDirectRoute(getter_AddRefs(ci));
         mConnInfo = ci;
        if (mRequestHead) {
            DebugOnly<nsresult> rv =
                mRequestHead->SetHeader(nsHttp::Alternate_Service_Used,
                                        NS_LITERAL_CSTRING("0"));
            MOZ_ASSERT(NS_SUCCEEDED(rv));
        }
    }

    return gHttpHandler->InitiateTransaction(this, mPriority);
}

char *
nsHttpTransaction::LocateHttpStart(char *buf, uint32_t len,
                                   bool aAllowPartialMatch)
{
    MOZ_ASSERT(!aAllowPartialMatch || mLineBuf.IsEmpty());

    static const char HTTPHeader[] = "HTTP/1.";
    static const uint32_t HTTPHeaderLen = sizeof(HTTPHeader) - 1;
    static const char HTTP2Header[] = "HTTP/2.0";
    static const uint32_t HTTP2HeaderLen = sizeof(HTTP2Header) - 1;
    // ShoutCast ICY is treated as HTTP/1.0
    static const char ICYHeader[] = "ICY ";
    static const uint32_t ICYHeaderLen = sizeof(ICYHeader) - 1;

    if (aAllowPartialMatch && (len < HTTPHeaderLen))
        return (PL_strncasecmp(buf, HTTPHeader, len) == 0) ? buf : nullptr;

    // mLineBuf can contain partial match from previous search
    if (!mLineBuf.IsEmpty()) {
        MOZ_ASSERT(mLineBuf.Length() < HTTPHeaderLen);
        int32_t checkChars = std::min(len, HTTPHeaderLen - mLineBuf.Length());
        if (PL_strncasecmp(buf, HTTPHeader + mLineBuf.Length(),
                           checkChars) == 0) {
            mLineBuf.Append(buf, checkChars);
            if (mLineBuf.Length() == HTTPHeaderLen) {
                // We've found whole HTTPHeader sequence. Return pointer at the
                // end of matched sequence since it is stored in mLineBuf.
                return (buf + checkChars);
            }
            // Response matches pattern but is still incomplete.
            return nullptr;
        }
        // Previous partial match together with new data doesn't match the
        // pattern. Start the search again.
        mLineBuf.Truncate();
    }

    bool firstByte = true;
    while (len > 0) {
        if (PL_strncasecmp(buf, HTTPHeader, std::min<uint32_t>(len, HTTPHeaderLen)) == 0) {
            if (len < HTTPHeaderLen) {
                // partial HTTPHeader sequence found
                // save partial match to mLineBuf
                mLineBuf.Assign(buf, len);
                return nullptr;
            }

            // whole HTTPHeader sequence found
            return buf;
        }

        // At least "SmarterTools/2.0.3974.16813" generates nonsensical
        // HTTP/2.0 responses to our HTTP/1 requests. Treat the minimal case of
        // it as HTTP/1.1 to be compatible with old versions of ourselves and
        // other browsers

        if (firstByte && !mInvalidResponseBytesRead && len >= HTTP2HeaderLen &&
            (PL_strncasecmp(buf, HTTP2Header, HTTP2HeaderLen) == 0)) {
            LOG(("nsHttpTransaction:: Identified HTTP/2.0 treating as 1.x\n"));
            return buf;
        }

        // Treat ICY (AOL/Nullsoft ShoutCast) non-standard header in same fashion
        // as HTTP/2.0 is treated above. This will allow "ICY " to be interpretted
        // as HTTP/1.0 in nsHttpResponseHead::ParseVersion

        if (firstByte && !mInvalidResponseBytesRead && len >= ICYHeaderLen &&
            (PL_strncasecmp(buf, ICYHeader, ICYHeaderLen) == 0)) {
            LOG(("nsHttpTransaction:: Identified ICY treating as HTTP/1.0\n"));
            return buf;
        }

        if (!nsCRT::IsAsciiSpace(*buf))
            firstByte = false;
        buf++;
        len--;
    }
    return nullptr;
}

nsresult
nsHttpTransaction::ParseLine(nsACString &line)
{
    LOG(("nsHttpTransaction::ParseLine [%s]\n", PromiseFlatCString(line).get()));
    nsresult rv = NS_OK;

    if (!mHaveStatusLine) {
        mResponseHead->ParseStatusLine(line);
        mHaveStatusLine = true;
        // XXX this should probably never happen
        if (mResponseHead->Version() == NS_HTTP_VERSION_0_9)
            mHaveAllHeaders = true;
    }
    else {
        rv = mResponseHead->ParseHeaderLine(line);
    }
    return rv;
}

nsresult
nsHttpTransaction::ParseLineSegment(char *segment, uint32_t len)
{
    MOZ_ASSERT(!mHaveAllHeaders, "already have all headers");

    if (!mLineBuf.IsEmpty() && mLineBuf.Last() == '\n') {
        // trim off the new line char, and if this segment is
        // not a continuation of the previous or if we haven't
        // parsed the status line yet, then parse the contents
        // of mLineBuf.
        mLineBuf.Truncate(mLineBuf.Length() - 1);
        if (!mHaveStatusLine || (*segment != ' ' && *segment != '\t')) {
            nsresult rv = ParseLine(mLineBuf);
            mLineBuf.Truncate();
            if (NS_FAILED(rv)) {
                return rv;
            }
        }
    }

    // append segment to mLineBuf...
    mLineBuf.Append(segment, len);

    // a line buf with only a new line char signifies the end of headers.
    if (mLineBuf.First() == '\n') {
        mLineBuf.Truncate();
        // discard this response if it is a 100 continue or other 1xx status.
        uint16_t status = mResponseHead->Status();
        if ((status != 101) && (status / 100 == 1)) {
            LOG(("ignoring 1xx response\n"));
            mHaveStatusLine = false;
            mHttpResponseMatched = false;
            mConnection->SetLastTransactionExpectedNoContent(true);
            mResponseHead->Reset();
            return NS_OK;
        }
        mHaveAllHeaders = true;
    }
    return NS_OK;
}

nsresult
nsHttpTransaction::ParseHead(char *buf,
                             uint32_t count,
                             uint32_t *countRead)
{
    nsresult rv;
    uint32_t len;
    char *eol;

    LOG(("nsHttpTransaction::ParseHead [count=%u]\n", count));

    *countRead = 0;

    MOZ_ASSERT(!mHaveAllHeaders, "oops");

    // allocate the response head object if necessary
    if (!mResponseHead) {
        mResponseHead = new nsHttpResponseHead();
        if (!mResponseHead)
            return NS_ERROR_OUT_OF_MEMORY;

        // report that we have a least some of the response
        if (mActivityDistributor && !mReportedStart) {
            mReportedStart = true;
            rv = mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_START,
                PR_Now(), 0, EmptyCString());
            if (NS_FAILED(rv)) {
                LOG3(("ObserveActivity failed (%08x)",
                      static_cast<uint32_t>(rv)));
            }
        }
    }

    if (!mHttpResponseMatched) {
        // Normally we insist on seeing HTTP/1.x in the first few bytes,
        // but if we are on a persistent connection and the previous transaction
        // was not supposed to have any content then we need to be prepared
        // to skip over a response body that the server may have sent even
        // though it wasn't allowed.
        if (!mConnection || !mConnection->LastTransactionExpectedNoContent()) {
            // tolerate only minor junk before the status line
            mHttpResponseMatched = true;
            char *p = LocateHttpStart(buf, std::min<uint32_t>(count, 11), true);
            if (!p) {
                // Treat any 0.9 style response of a put as a failure.
                if (mRequestHead->IsPut())
                    return NS_ERROR_ABORT;

                mResponseHead->ParseStatusLine(EmptyCString());
                mHaveStatusLine = true;
                mHaveAllHeaders = true;
                return NS_OK;
            }
            if (p > buf) {
                // skip over the junk
                mInvalidResponseBytesRead += p - buf;
                *countRead = p - buf;
                buf = p;
            }
        }
        else {
            char *p = LocateHttpStart(buf, count, false);
            if (p) {
                mInvalidResponseBytesRead += p - buf;
                *countRead = p - buf;
                buf = p;
                mHttpResponseMatched = true;
            } else {
                mInvalidResponseBytesRead += count;
                *countRead = count;
                if (mInvalidResponseBytesRead > MAX_INVALID_RESPONSE_BODY_SIZE) {
                    LOG(("nsHttpTransaction::ParseHead() "
                         "Cannot find Response Header\n"));
                    // cannot go back and call this 0.9 anymore as we
                    // have thrown away a lot of the leading junk
                    return NS_ERROR_ABORT;
                }
                return NS_OK;
            }
        }
    }
    // otherwise we can assume that we don't have a HTTP/0.9 response.

    MOZ_ASSERT (mHttpResponseMatched);
    while ((eol = static_cast<char *>(memchr(buf, '\n', count - *countRead))) != nullptr) {
        // found line in range [buf:eol]
        len = eol - buf + 1;

        *countRead += len;

        // actually, the line is in the range [buf:eol-1]
        if ((eol > buf) && (*(eol-1) == '\r'))
            len--;

        buf[len-1] = '\n';
        rv = ParseLineSegment(buf, len);
        if (NS_FAILED(rv))
            return rv;

        if (mHaveAllHeaders)
            return NS_OK;

        // skip over line
        buf = eol + 1;

        if (!mHttpResponseMatched) {
            // a 100 class response has caused us to throw away that set of
            // response headers and look for the next response
            return NS_ERROR_NET_INTERRUPT;
        }
    }

    // do something about a partial header line
    if (!mHaveAllHeaders && (len = count - *countRead)) {
        *countRead = count;
        // ignore a trailing carriage return, and don't bother calling
        // ParseLineSegment if buf only contains a carriage return.
        if ((buf[len-1] == '\r') && (--len == 0))
            return NS_OK;
        rv = ParseLineSegment(buf, len);
        if (NS_FAILED(rv))
            return rv;
    }
    return NS_OK;
}

nsresult
nsHttpTransaction::HandleContentStart()
{
    LOG(("nsHttpTransaction::HandleContentStart [this=%p]\n", this));
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    if (mResponseHead) {
        if (mEarlyDataDisposition == EARLY_ACCEPTED) {
            if (mResponseHead->Status() == 425) {
                // We will report this state when the final responce arrives.
                mEarlyDataDisposition = EARLY_425;
            } else {
                Unused << mResponseHead->SetHeader(nsHttp::X_Firefox_Early_Data, NS_LITERAL_CSTRING("accepted"));
            }
        } else if (mEarlyDataDisposition == EARLY_SENT) {
            Unused << mResponseHead->SetHeader(nsHttp::X_Firefox_Early_Data, NS_LITERAL_CSTRING("sent"));
        } else if (mEarlyDataDisposition == EARLY_425) {
            Unused << mResponseHead->SetHeader(nsHttp::X_Firefox_Early_Data, NS_LITERAL_CSTRING("received 425"));
            mEarlyDataDisposition = EARLY_NONE;
        } // no header on NONE case

        if (mFastOpenStatus == TFO_DATA_SENT) {
            Unused << mResponseHead->SetHeader(nsHttp::X_Firefox_TCP_Fast_Open, NS_LITERAL_CSTRING("data sent"));
        } else if (mFastOpenStatus == TFO_TRIED) {
            Unused << mResponseHead->SetHeader(nsHttp::X_Firefox_TCP_Fast_Open, NS_LITERAL_CSTRING("tried negotiating"));
        } else if (mFastOpenStatus == TFO_FAILED) {
            Unused << mResponseHead->SetHeader(nsHttp::X_Firefox_TCP_Fast_Open, NS_LITERAL_CSTRING("failed"));
        } // no header on TFO_NOT_TRIED case

        if (LOG3_ENABLED()) {
            LOG3(("http response [\n"));
            nsAutoCString headers;
            mResponseHead->Flatten(headers, false);
            headers.AppendLiteral("  OriginalHeaders");
            headers.AppendLiteral("\r\n");
            mResponseHead->FlattenNetworkOriginalHeaders(headers);
            LogHeaders(headers.get());
            LOG3(("]\n"));
        }

        CheckForStickyAuthScheme();

        // Save http version, mResponseHead isn't available anymore after
        // TakeResponseHead() is called
        mHttpVersion = mResponseHead->Version();
        mHttpResponseCode = mResponseHead->Status();

        // notify the connection, give it a chance to cause a reset.
        bool reset = false;
        nsresult rv = mConnection->OnHeadersAvailable(this, mRequestHead,
                                                      mResponseHead, &reset);
        NS_ENSURE_SUCCESS(rv, rv);

        // looks like we should ignore this response, resetting...
        if (reset) {
            LOG(("resetting transaction's response head\n"));
            mHaveAllHeaders = false;
            mHaveStatusLine = false;
            mReceivedData = false;
            mSentData = false;
            mHttpResponseMatched = false;
            mResponseHead->Reset();
            // wait to be called again...
            return NS_OK;
        }

        // check if this is a no-content response
        switch (mResponseHead->Status()) {
        case 101:
            mPreserveStream = true;
            MOZ_FALLTHROUGH; // to other no content cases:
        case 204:
        case 205:
        case 304:
            mNoContent = true;
            LOG(("this response should not contain a body.\n"));
            break;
        case 421:
            LOG(("Misdirected Request.\n"));
            gHttpHandler->ConnMgr()->ClearHostMapping(mConnInfo);

            // retry on a new connection - just in case
            if (!mRestartCount) {
                mCaps &= ~NS_HTTP_ALLOW_KEEPALIVE;
                mForceRestart = true; // force restart has built in loop protection
                return NS_ERROR_NET_RESET;
            }
            break;
        case 425:
            LOG(("Too Early."));
            if ((mEarlyDataDisposition == EARLY_425) && !mDoNotTryEarlyData) {
                mDoNotTryEarlyData = true;
                mForceRestart = true; // force restart has built in loop protection
                if (mConnection->Version() == HTTP_VERSION_2) {
                    mReuseOnRestart = true;
                }
                return NS_ERROR_NET_RESET;
            }
            break;
        }

        if (mResponseHead->Status() == 200 &&
            mConnection->IsProxyConnectInProgress()) {
            // successful CONNECTs do not have response bodies
            mNoContent = true;
        }
        mConnection->SetLastTransactionExpectedNoContent(mNoContent);

        if (mNoContent) {
            mContentLength = 0;
        } else {
            // grab the content-length from the response headers
            mContentLength = mResponseHead->ContentLength();

            // handle chunked encoding here, so we'll know immediately when
            // we're done with the socket.  please note that _all_ other
            // decoding is done when the channel receives the content data
            // so as not to block the socket transport thread too much.
            if (mResponseHead->Version() >= NS_HTTP_VERSION_1_0 &&
                mResponseHead->HasHeaderValue(nsHttp::Transfer_Encoding, "chunked")) {
                // we only support the "chunked" transfer encoding right now.
                mChunkedDecoder = new nsHttpChunkedDecoder();
                LOG(("nsHttpTransaction %p chunked decoder created\n", this));
                // Ignore server specified Content-Length.
                if (mContentLength != int64_t(-1)) {
                    LOG(("nsHttpTransaction %p chunked with C-L ignores C-L\n", this));
                    mContentLength = -1;
                    if (mConnection) {
                        mConnection->DontReuse();
                    }
                }
            }
            else if (mContentLength == int64_t(-1))
                LOG(("waiting for the server to close the connection.\n"));
        }
    }

    mDidContentStart = true;
    return NS_OK;
}

// called on the socket thread
nsresult
nsHttpTransaction::HandleContent(char *buf,
                                 uint32_t count,
                                 uint32_t *contentRead,
                                 uint32_t *contentRemaining)
{
    nsresult rv;

    LOG(("nsHttpTransaction::HandleContent [this=%p count=%u]\n", this, count));

    *contentRead = 0;
    *contentRemaining = 0;

    MOZ_ASSERT(mConnection);

    if (!mDidContentStart) {
        rv = HandleContentStart();
        if (NS_FAILED(rv)) return rv;
        // Do not write content to the pipe if we haven't started streaming yet
        if (!mDidContentStart)
            return NS_OK;
    }

    if (mChunkedDecoder) {
        // give the buf over to the chunked decoder so it can reformat the
        // data and tell us how much is really there.
        rv = mChunkedDecoder->HandleChunkedContent(buf, count, contentRead, contentRemaining);
        if (NS_FAILED(rv)) return rv;
    }
    else if (mContentLength >= int64_t(0)) {
        // HTTP/1.0 servers have been known to send erroneous Content-Length
        // headers. So, unless the connection is persistent, we must make
        // allowances for a possibly invalid Content-Length header. Thus, if
        // NOT persistent, we simply accept everything in |buf|.
        if (mConnection->IsPersistent() || mPreserveStream ||
            mHttpVersion >= NS_HTTP_VERSION_1_1) {
            int64_t remaining = mContentLength - mContentRead;
            *contentRead = uint32_t(std::min<int64_t>(count, remaining));
            *contentRemaining = count - *contentRead;
        }
        else {
            *contentRead = count;
            // mContentLength might need to be increased...
            int64_t position = mContentRead + int64_t(count);
            if (position > mContentLength) {
                mContentLength = position;
                //mResponseHead->SetContentLength(mContentLength);
            }
        }
    }
    else {
        // when we are just waiting for the server to close the connection...
        // (no explicit content-length given)
        *contentRead = count;
    }

    if (*contentRead) {
        // update count of content bytes read and report progress...
        mContentRead += *contentRead;
    }

    LOG(("nsHttpTransaction::HandleContent [this=%p count=%u read=%u mContentRead=%" PRId64 " mContentLength=%" PRId64 "]\n",
        this, count, *contentRead, mContentRead, mContentLength));

    // check for end-of-file
    if ((mContentRead == mContentLength) ||
        (mChunkedDecoder && mChunkedDecoder->ReachedEOF())) {
        MutexAutoLock lock(*nsHttp::GetLock());
        if (mChunkedDecoder) {
            mForTakeResponseTrailers = mChunkedDecoder->TakeTrailers();
        }

        // the transaction is done with a complete response.
        mTransactionDone = true;
        mResponseIsComplete = true;
        ReleaseBlockingTransaction();

        if (TimingEnabled()) {
            SetResponseEnd(TimeStamp::Now());
        }

        // report the entire response has arrived
        if (mActivityDistributor) {
            rv = mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_COMPLETE,
                PR_Now(),
                static_cast<uint64_t>(mContentRead),
                EmptyCString());
            if (NS_FAILED(rv)) {
                LOG3(("ObserveActivity failed (%08x)",
                      static_cast<uint32_t>(rv)));
            }
        }
    }

    return NS_OK;
}

nsresult
nsHttpTransaction::ProcessData(char *buf, uint32_t count, uint32_t *countRead)
{
    nsresult rv;

    LOG(("nsHttpTransaction::ProcessData [this=%p count=%u]\n", this, count));

    *countRead = 0;

    // we may not have read all of the headers yet...
    if (!mHaveAllHeaders) {
        uint32_t bytesConsumed = 0;

        do {
            uint32_t localBytesConsumed = 0;
            char *localBuf = buf + bytesConsumed;
            uint32_t localCount = count - bytesConsumed;

            rv = ParseHead(localBuf, localCount, &localBytesConsumed);
            if (NS_FAILED(rv) && rv != NS_ERROR_NET_INTERRUPT)
                return rv;
            bytesConsumed += localBytesConsumed;
        } while (rv == NS_ERROR_NET_INTERRUPT);

        mCurrentHttpResponseHeaderSize += bytesConsumed;
        if (mCurrentHttpResponseHeaderSize >
            gHttpHandler->MaxHttpResponseHeaderSize()) {
            LOG(("nsHttpTransaction %p The response header exceeds the limit.\n",
                 this));
            return NS_ERROR_FILE_TOO_BIG;
        }
        count -= bytesConsumed;

        // if buf has some content in it, shift bytes to top of buf.
        if (count && bytesConsumed)
            memmove(buf, buf + bytesConsumed, count);

        // report the completed response header
        if (mActivityDistributor && mResponseHead && mHaveAllHeaders &&
            !mReportedResponseHeader) {
            mReportedResponseHeader = true;
            nsAutoCString completeResponseHeaders;
            mResponseHead->Flatten(completeResponseHeaders, false);
            completeResponseHeaders.AppendLiteral("\r\n");
            rv = mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_HEADER,
                PR_Now(), 0,
                completeResponseHeaders);
            if (NS_FAILED(rv)) {
                LOG3(("ObserveActivity failed (%08x)",
                      static_cast<uint32_t>(rv)));
            }
        }
    }

    // even though count may be 0, we still want to call HandleContent
    // so it can complete the transaction if this is a "no-content" response.
    if (mHaveAllHeaders) {
        uint32_t countRemaining = 0;
        //
        // buf layout:
        //
        // +--------------------------------------+----------------+-----+
        // |              countRead               | countRemaining |     |
        // +--------------------------------------+----------------+-----+
        //
        // count          : bytes read from the socket
        // countRead      : bytes corresponding to this transaction
        // countRemaining : bytes corresponding to next transaction on conn
        //
        // NOTE:
        // count > countRead + countRemaining <==> chunked transfer encoding
        //
        rv = HandleContent(buf, count, countRead, &countRemaining);
        if (NS_FAILED(rv)) return rv;
        // we may have read more than our share, in which case we must give
        // the excess bytes back to the connection
        if (mResponseIsComplete && countRemaining) {
            MOZ_ASSERT(mConnection);
            rv = mConnection->PushBack(buf + *countRead, countRemaining);
            NS_ENSURE_SUCCESS(rv, rv);
        }

        if (!mContentDecodingCheck && mResponseHead) {
            mContentDecoding =
                mResponseHead->HasHeader(nsHttp::Content_Encoding);
            mContentDecodingCheck = true;
        }
    }

    return NS_OK;
}

void
nsHttpTransaction::SetRequestContext(nsIRequestContext *aRequestContext)
{
    LOG(("nsHttpTransaction %p SetRequestContext %p\n", this, aRequestContext));
    mRequestContext = aRequestContext;
}

// Called when the transaction marked for blocking is associated with a connection
// (i.e. added to a new h1 conn, an idle http connection, etc..)
// It is safe to call this multiple times with it only
// having an effect once.
void
nsHttpTransaction::DispatchedAsBlocking()
{
    if (mDispatchedAsBlocking)
        return;

    LOG(("nsHttpTransaction %p dispatched as blocking\n", this));

    if (!mRequestContext)
        return;

    LOG(("nsHttpTransaction adding blocking transaction %p from "
         "request context %p\n", this, mRequestContext.get()));

    mRequestContext->AddBlockingTransaction();
    mDispatchedAsBlocking = true;
}

void
nsHttpTransaction::RemoveDispatchedAsBlocking()
{
    if (!mRequestContext || !mDispatchedAsBlocking) {
        LOG(("nsHttpTransaction::RemoveDispatchedAsBlocking this=%p not blocking",
             this));
        return;
    }

    uint32_t blockers = 0;
    nsresult rv = mRequestContext->RemoveBlockingTransaction(&blockers);

    LOG(("nsHttpTransaction removing blocking transaction %p from "
         "request context %p. %d blockers remain.\n", this,
         mRequestContext.get(), blockers));

    if (NS_SUCCEEDED(rv) && !blockers) {
        LOG(("nsHttpTransaction %p triggering release of blocked channels "
             " with request context=%p\n", this, mRequestContext.get()));
        rv = gHttpHandler->ConnMgr()->ProcessPendingQ();
        if (NS_FAILED(rv)) {
            LOG(("nsHttpTransaction::RemoveDispatchedAsBlocking\n"
                 "    failed to process pending queue\n"));
        }
    }

    mDispatchedAsBlocking = false;
}

void
nsHttpTransaction::ReleaseBlockingTransaction()
{
    RemoveDispatchedAsBlocking();
    LOG(("nsHttpTransaction %p request context set to null "
         "in ReleaseBlockingTransaction() - was %p\n", this, mRequestContext.get()));
    mRequestContext = nullptr;
}

void
nsHttpTransaction::DisableSpdy()
{
    mCaps |= NS_HTTP_DISALLOW_SPDY;
    if (mConnInfo) {
        // This is our clone of the connection info, not the persistent one that
        // is owned by the connection manager, so we're safe to change this here
        mConnInfo->SetNoSpdy(true);
    }
}

void
nsHttpTransaction::CheckForStickyAuthScheme()
{
  LOG(("nsHttpTransaction::CheckForStickyAuthScheme this=%p", this));

  MOZ_ASSERT(mHaveAllHeaders);
  MOZ_ASSERT(mResponseHead);
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  CheckForStickyAuthSchemeAt(nsHttp::WWW_Authenticate);
  CheckForStickyAuthSchemeAt(nsHttp::Proxy_Authenticate);
}

void
nsHttpTransaction::CheckForStickyAuthSchemeAt(nsHttpAtom const& header)
{
  if (mCaps & NS_HTTP_STICKY_CONNECTION) {
      LOG(("  already sticky"));
      return;
  }

  nsAutoCString auth;
  if (NS_FAILED(mResponseHead->GetHeader(header, auth))) {
      return;
  }

  Tokenizer p(auth);
  nsAutoCString schema;
  while (p.ReadWord(schema)) {
      ToLowerCase(schema);

      nsAutoCString contractid;
      contractid.AssignLiteral(NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX);
      contractid.Append(schema);

      // using a new instance because of thread safety of auth modules refcnt
      nsCOMPtr<nsIHttpAuthenticator> authenticator(do_CreateInstance(contractid.get()));
      if (authenticator) {
          uint32_t flags;
          nsresult rv = authenticator->GetAuthFlags(&flags);
          if (NS_SUCCEEDED(rv) && (flags & nsIHttpAuthenticator::CONNECTION_BASED)) {
              LOG(("  connection made sticky, found %s auth shema", schema.get()));
              // This is enough to make this transaction keep it's current connection,
              // prevents the connection from being released back to the pool.
              mCaps |= NS_HTTP_STICKY_CONNECTION;
              break;
          }
      }

      // schemes are separated with LFs, nsHttpHeaderArray::MergeHeader
      p.SkipUntil(Tokenizer::Token::NewLine());
      p.SkipWhites(Tokenizer::INCLUDE_NEW_LINE);
  }
}

const TimingStruct
nsHttpTransaction::Timings()
{
    mozilla::MutexAutoLock lock(mLock);
    TimingStruct timings = mTimings;
    return timings;
}

void
nsHttpTransaction::BootstrapTimings(TimingStruct times)
{
    mozilla::MutexAutoLock lock(mLock);
    mTimings = times;
}

void
nsHttpTransaction::SetDomainLookupStart(mozilla::TimeStamp timeStamp, bool onlyIfNull)
{
    mozilla::MutexAutoLock lock(mLock);
    if (onlyIfNull && !mTimings.domainLookupStart.IsNull()) {
        return; // We only set the timestamp if it was previously null
    }
    mTimings.domainLookupStart = timeStamp;
}

void
nsHttpTransaction::SetDomainLookupEnd(mozilla::TimeStamp timeStamp, bool onlyIfNull)
{
    mozilla::MutexAutoLock lock(mLock);
    if (onlyIfNull && !mTimings.domainLookupEnd.IsNull()) {
        return; // We only set the timestamp if it was previously null
    }
    mTimings.domainLookupEnd = timeStamp;
}

void
nsHttpTransaction::SetConnectStart(mozilla::TimeStamp timeStamp, bool onlyIfNull)
{
    mozilla::MutexAutoLock lock(mLock);
    if (onlyIfNull && !mTimings.connectStart.IsNull()) {
        return; // We only set the timestamp if it was previously null
    }
    mTimings.connectStart = timeStamp;
}

void
nsHttpTransaction::SetConnectEnd(mozilla::TimeStamp timeStamp, bool onlyIfNull)
{
    mozilla::MutexAutoLock lock(mLock);
    if (onlyIfNull && !mTimings.connectEnd.IsNull()) {
        return; // We only set the timestamp if it was previously null
    }
    mTimings.connectEnd = timeStamp;
}

void
nsHttpTransaction::SetRequestStart(mozilla::TimeStamp timeStamp, bool onlyIfNull)
{
    mozilla::MutexAutoLock lock(mLock);
    if (onlyIfNull && !mTimings.requestStart.IsNull()) {
        return; // We only set the timestamp if it was previously null
    }
    mTimings.requestStart = timeStamp;
}

void
nsHttpTransaction::SetResponseStart(mozilla::TimeStamp timeStamp, bool onlyIfNull)
{
    mozilla::MutexAutoLock lock(mLock);
    if (onlyIfNull && !mTimings.responseStart.IsNull()) {
        return; // We only set the timestamp if it was previously null
    }
    mTimings.responseStart = timeStamp;
}

void
nsHttpTransaction::SetResponseEnd(mozilla::TimeStamp timeStamp, bool onlyIfNull)
{
    mozilla::MutexAutoLock lock(mLock);
    if (onlyIfNull && !mTimings.responseEnd.IsNull()) {
        return; // We only set the timestamp if it was previously null
    }
    mTimings.responseEnd = timeStamp;
}

mozilla::TimeStamp
nsHttpTransaction::GetDomainLookupStart()
{
    mozilla::MutexAutoLock lock(mLock);
    return mTimings.domainLookupStart;
}

mozilla::TimeStamp
nsHttpTransaction::GetDomainLookupEnd()
{
    mozilla::MutexAutoLock lock(mLock);
    return mTimings.domainLookupEnd;
}

mozilla::TimeStamp
nsHttpTransaction::GetConnectStart()
{
    mozilla::MutexAutoLock lock(mLock);
    return mTimings.connectStart;
}

mozilla::TimeStamp
nsHttpTransaction::GetTcpConnectEnd()
{
    mozilla::MutexAutoLock lock(mLock);
    return mTimings.tcpConnectEnd;
}

mozilla::TimeStamp
nsHttpTransaction::GetSecureConnectionStart()
{
    mozilla::MutexAutoLock lock(mLock);
    return mTimings.secureConnectionStart;
}

mozilla::TimeStamp
nsHttpTransaction::GetConnectEnd()
{
    mozilla::MutexAutoLock lock(mLock);
    return mTimings.connectEnd;
}

mozilla::TimeStamp
nsHttpTransaction::GetRequestStart()
{
    mozilla::MutexAutoLock lock(mLock);
    return mTimings.requestStart;
}

mozilla::TimeStamp
nsHttpTransaction::GetResponseStart()
{
    mozilla::MutexAutoLock lock(mLock);
    return mTimings.responseStart;
}

mozilla::TimeStamp
nsHttpTransaction::GetResponseEnd()
{
    mozilla::MutexAutoLock lock(mLock);
    return mTimings.responseEnd;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction deletion event
//-----------------------------------------------------------------------------

class DeleteHttpTransaction : public Runnable {
public:
  explicit DeleteHttpTransaction(nsHttpTransaction* trans)
    : Runnable("net::DeleteHttpTransaction")
    , mTrans(trans)
  {
  }

  NS_IMETHOD Run() override
  {
    delete mTrans;
    return NS_OK;
    }
private:
    nsHttpTransaction *mTrans;
};

void
nsHttpTransaction::DeleteSelfOnConsumerThread()
{
    LOG(("nsHttpTransaction::DeleteSelfOnConsumerThread [this=%p]\n", this));

    bool val;
    if (!mConsumerTarget ||
        (NS_SUCCEEDED(mConsumerTarget->IsOnCurrentThread(&val)) && val)) {
        delete this;
    } else {
        LOG(("proxying delete to consumer thread...\n"));
        nsCOMPtr<nsIRunnable> event = new DeleteHttpTransaction(this);
        if (NS_FAILED(mConsumerTarget->Dispatch(event, NS_DISPATCH_NORMAL)))
            NS_WARNING("failed to dispatch nsHttpDeleteTransaction event");
    }
}

bool
nsHttpTransaction::TryToRunPacedRequest()
{
    if (mSubmittedRatePacing)
        return mPassedRatePacing;

    mSubmittedRatePacing = true;
    mSynchronousRatePaceRequest = true;
    Unused << gHttpHandler->SubmitPacedRequest(this, getter_AddRefs(mTokenBucketCancel));
    mSynchronousRatePaceRequest = false;
    return mPassedRatePacing;
}

void
nsHttpTransaction::OnTokenBucketAdmitted()
{
    mPassedRatePacing = true;
    mTokenBucketCancel = nullptr;

    if (!mSynchronousRatePaceRequest) {
        nsresult rv = gHttpHandler->ConnMgr()->ProcessPendingQ(mConnInfo);
        if (NS_FAILED(rv)) {
            LOG(("nsHttpTransaction::OnTokenBucketAdmitted\n"
                 "    failed to process pending queue\n"));
        }
    }
}

void
nsHttpTransaction::CancelPacing(nsresult reason)
{
    if (mTokenBucketCancel) {
        mTokenBucketCancel->Cancel(reason);
        mTokenBucketCancel = nullptr;
    }
}

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(nsHttpTransaction)

NS_IMETHODIMP_(MozExternalRefCountType)
nsHttpTransaction::Release()
{
    nsrefcnt count;
    MOZ_ASSERT(0 != mRefCnt, "dup release");
    count = --mRefCnt;
    NS_LOG_RELEASE(this, count, "nsHttpTransaction");
    if (0 == count) {
        mRefCnt = 1; /* stablize */
        // it is essential that the transaction be destroyed on the consumer
        // thread (we could be holding the last reference to our consumer).
        DeleteSelfOnConsumerThread();
        return 0;
    }
    return count;
}

NS_IMPL_QUERY_INTERFACE(nsHttpTransaction,
                        nsIInputStreamCallback,
                        nsIOutputStreamCallback)

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsIInputStreamCallback
//-----------------------------------------------------------------------------

// called on the socket thread
NS_IMETHODIMP
nsHttpTransaction::OnInputStreamReady(nsIAsyncInputStream *out)
{
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    if (mConnection) {
        mConnection->TransactionHasDataToWrite(this);
        nsresult rv = mConnection->ResumeSend();
        if (NS_FAILED(rv))
            NS_ERROR("ResumeSend failed");
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsIOutputStreamCallback
//-----------------------------------------------------------------------------

// called on the socket thread
NS_IMETHODIMP
nsHttpTransaction::OnOutputStreamReady(nsIAsyncOutputStream *out)
{
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");
    mWaitingOnPipeOut = false;
    if (mConnection) {
        mConnection->TransactionHasDataToRecv(this);
        nsresult rv = mConnection->ResumeRecv();
        if (NS_FAILED(rv))
            NS_ERROR("ResumeRecv failed");
    }
    return NS_OK;
}

void
nsHttpTransaction::GetNetworkAddresses(NetAddr &self, NetAddr &peer)
{
    MutexAutoLock lock(mLock);
    self = mSelfAddr;
    peer = mPeerAddr;
}

bool
nsHttpTransaction::CanDo0RTT()
{
    if (mRequestHead->IsSafeMethod() &&
        !mDoNotTryEarlyData &&
        (!mConnection ||
         !mConnection->IsProxyConnectInProgress())) {
        return true;
    }
    return false;
}

bool
nsHttpTransaction::Do0RTT()
{
   if (mRequestHead->IsSafeMethod() &&
       !mDoNotTryEarlyData &&
       (!mConnection ||
       !mConnection->IsProxyConnectInProgress())) {
     m0RTTInProgress = true;
   }
   return m0RTTInProgress;
}

nsresult
nsHttpTransaction::Finish0RTT(bool aRestart, bool aAlpnChanged /* ignored */)
{
    LOG(("nsHttpTransaction::Finish0RTT %p %d %d\n", this, aRestart, aAlpnChanged));
    MOZ_ASSERT(m0RTTInProgress);
    m0RTTInProgress = false;
    if (!aRestart && (mEarlyDataDisposition == EARLY_SENT)) {
        // note that if this is invoked by a 3 param version of finish0rtt this
        // disposition might be reverted
        mEarlyDataDisposition = EARLY_ACCEPTED;
    }
    if (aRestart) {
        // Reset request headers to be sent again.
        nsCOMPtr<nsISeekableStream> seekable =
            do_QueryInterface(mRequestStream);
        if (seekable) {
            seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
        } else {
            return NS_ERROR_FAILURE;
        }
    } else if (!mConnected) {
        // this is code that was skipped in ::ReadSegments while in 0RTT
        mConnected = true;
        mConnection->GetSecurityInfo(getter_AddRefs(mSecurityInfo));
    }
    return NS_OK;
}

nsresult
nsHttpTransaction::RestartOnFastOpenError()
{
    // This will happen on connection error during Fast Open or if connect
    // during Fast Open takes too long. So we should not have received any
    // data!
    MOZ_ASSERT(!mReceivedData);
    MOZ_ASSERT(OnSocketThread(), "not on socket thread");

    LOG(("nsHttpTransaction::RestartOnFastOpenError - restarting transaction "
         "%p\n", this));

    // rewind streams in case we already wrote out the request
    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mRequestStream);
    if (seekable)
        seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
        // clear old connection state...
    mSecurityInfo = nullptr;

    if (!mConnInfo->GetRoutedHost().IsEmpty()) {
        MutexAutoLock lock(*nsHttp::GetLock());
        RefPtr<nsHttpConnectionInfo> ci;
        mConnInfo->CloneAsDirectRoute(getter_AddRefs(ci));
        mConnInfo = ci;
    }
    mEarlyDataDisposition = EARLY_NONE;
    m0RTTInProgress = false;
    mFastOpenStatus = TFO_FAILED;
    mTimings = TimingStruct();
    return NS_OK;
}

void
nsHttpTransaction::SetFastOpenStatus(uint8_t aStatus)
{
    LOG(("nsHttpTransaction::SetFastOpenStatus %d [this=%p]\n",
         aStatus, this));
    mFastOpenStatus = aStatus;
}

void
nsHttpTransaction::Refused0RTT()
{
    LOG(("nsHttpTransaction::Refused0RTT %p\n", this));
    if (mEarlyDataDisposition == EARLY_ACCEPTED) {
        mEarlyDataDisposition = EARLY_SENT; // undo accepted state
    }
}

void
nsHttpTransaction::SetHttpTrailers(nsCString &aTrailers)
{
    LOG(("nsHttpTransaction::SetHttpTrailers %p", this));
    LOG(("[\n    %s\n]", aTrailers.BeginReading()));
    if (!mForTakeResponseTrailers) {
        mForTakeResponseTrailers = new nsHttpHeaderArray();
    }

    int32_t cur = 0;
    int32_t len = aTrailers.Length();
    while (cur < len) {
        int32_t newline = aTrailers.FindCharInSet("\n", cur);
        if (newline == -1) {
            newline = len;
        }

        int32_t end = aTrailers[newline - 1] == '\r' ? newline - 1 : newline;
        nsDependentCSubstring line(aTrailers, cur, end);
        nsHttpAtom hdr = {nullptr};
        nsAutoCString hdrNameOriginal;
        nsAutoCString val;
        if (NS_SUCCEEDED(mForTakeResponseTrailers->ParseHeaderLine(line, &hdr, &hdrNameOriginal, &val))) {
            if (hdr == nsHttp::Server_Timing) {
                Unused << mForTakeResponseTrailers->SetHeaderFromNet(hdr, hdrNameOriginal, val, true);
            }
        }

        cur = newline + 1;
    }

    if (mForTakeResponseTrailers->Count() == 0) {
        // Didn't find a Server-Timing header, so get rid of this.
        mForTakeResponseTrailers = nullptr;
    }
}

} // namespace net
} // namespace mozilla
