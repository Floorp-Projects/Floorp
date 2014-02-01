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
#include "nsNetUtil.h"
#include "nsCRT.h"

#include "nsISeekableStream.h"
#include "nsMultiplexInputStream.h"
#include "nsStringStream.h"
#include "mozilla/VisualEventTracer.h"

#include "nsComponentManagerUtils.h" // do_CreateInstance
#include "nsServiceManagerUtils.h"   // do_GetService
#include "nsIHttpActivityObserver.h"
#include "nsSocketTransportService2.h"
#include "nsICancelable.h"
#include "nsIEventTarget.h"
#include "nsIHttpChannelInternal.h"
#include "nsIInputStream.h"
#include "nsITransport.h"
#include "nsIOService.h"
#include <algorithm>

#ifdef MOZ_WIDGET_GONK
#include "nsINetworkStatsServiceProxy.h"
#endif


//-----------------------------------------------------------------------------

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

//-----------------------------------------------------------------------------

static NS_DEFINE_CID(kMultiplexInputStream, NS_MULTIPLEXINPUTSTREAM_CID);

// Place a limit on how much non-compliant HTTP can be skipped while
// looking for a response header
#define MAX_INVALID_RESPONSE_BODY_SIZE (1024 * 128)

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

#if defined(PR_LOGGING)
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
#endif

//-----------------------------------------------------------------------------
// nsHttpTransaction <public>
//-----------------------------------------------------------------------------

nsHttpTransaction::nsHttpTransaction()
    : mCallbacksLock("transaction mCallbacks lock")
    , mRequestSize(0)
    , mConnection(nullptr)
    , mConnInfo(nullptr)
    , mRequestHead(nullptr)
    , mResponseHead(nullptr)
    , mContentLength(-1)
    , mContentRead(0)
    , mInvalidResponseBytesRead(0)
    , mChunkedDecoder(nullptr)
    , mStatus(NS_OK)
    , mPriority(0)
    , mRestartCount(0)
    , mCaps(0)
    , mCapsToClear(0)
    , mClassification(CLASS_GENERAL)
    , mPipelinePosition(0)
    , mHttpVersion(NS_HTTP_VERSION_UNKNOWN)
    , mClosed(false)
    , mConnected(false)
    , mHaveStatusLine(false)
    , mHaveAllHeaders(false)
    , mTransactionDone(false)
    , mResponseIsComplete(false)
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
    , mReportedStart(false)
    , mReportedResponseHeader(false)
    , mForTakeResponseHead(nullptr)
    , mResponseHeadTaken(false)
    , mSubmittedRatePacing(false)
    , mPassedRatePacing(false)
    , mSynchronousRatePaceRequest(false)
    , mCountRecv(0)
    , mCountSent(0)
    , mAppId(NECKO_NO_APP_ID)
{
    LOG(("Creating nsHttpTransaction @%p\n", this));
    gHttpHandler->GetMaxPipelineObjectSize(&mMaxPipelineObjectSize);
}

nsHttpTransaction::~nsHttpTransaction()
{
    LOG(("Destroying nsHttpTransaction @%p\n", this));

    if (mTokenBucketCancel) {
        mTokenBucketCancel->Cancel(NS_ERROR_ABORT);
        mTokenBucketCancel = nullptr;
    }

    // Force the callbacks to be released right now
    mCallbacks = nullptr;

    NS_IF_RELEASE(mConnection);
    NS_IF_RELEASE(mConnInfo);

    delete mResponseHead;
    delete mForTakeResponseHead;
    delete mChunkedDecoder;
    ReleaseBlockingTransaction();
}

nsHttpTransaction::Classifier
nsHttpTransaction::Classify()
{
    if (!(mCaps & NS_HTTP_ALLOW_PIPELINING))
        return (mClassification = CLASS_SOLO);

    if (mRequestHead->PeekHeader(nsHttp::If_Modified_Since) ||
        mRequestHead->PeekHeader(nsHttp::If_None_Match))
        return (mClassification = CLASS_REVALIDATION);

    const char *accept = mRequestHead->PeekHeader(nsHttp::Accept);
    if (accept && !PL_strncmp(accept, "image/", 6))
        return (mClassification = CLASS_IMAGE);

    if (accept && !PL_strncmp(accept, "text/css", 8))
        return (mClassification = CLASS_SCRIPT);

    mClassification = CLASS_GENERAL;

    int32_t queryPos = mRequestHead->RequestURI().FindChar('?');
    if (queryPos == kNotFound) {
        if (StringEndsWith(mRequestHead->RequestURI(),
                           NS_LITERAL_CSTRING(".js")))
            mClassification = CLASS_SCRIPT;
    }
    else if (queryPos >= 3 &&
             Substring(mRequestHead->RequestURI(), queryPos - 3, 3).
             EqualsLiteral(".js")) {
        mClassification = CLASS_SCRIPT;
    }

    return mClassification;
}

nsresult
nsHttpTransaction::Init(uint32_t caps,
                        nsHttpConnectionInfo *cinfo,
                        nsHttpRequestHead *requestHead,
                        nsIInputStream *requestBody,
                        bool requestBodyHasHeaders,
                        nsIEventTarget *target,
                        nsIInterfaceRequestor *callbacks,
                        nsITransportEventSink *eventsink,
                        nsIAsyncInputStream **responseBody)
{
    MOZ_EVENT_TRACER_COMPOUND_NAME(static_cast<nsAHttpTransaction*>(this),
                                   requestHead->PeekHeader(nsHttp::Host),
                                   requestHead->RequestURI().BeginReading());

    MOZ_EVENT_TRACER_WAIT(static_cast<nsAHttpTransaction*>(this),
                          "net::http::transaction");
    nsresult rv;

    LOG(("nsHttpTransaction::Init [this=%p caps=%x]\n", this, caps));

    MOZ_ASSERT(cinfo);
    MOZ_ASSERT(requestHead);
    MOZ_ASSERT(target);

    mActivityDistributor = do_GetService(NS_HTTPACTIVITYDISTRIBUTOR_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    bool activityDistributorActive;
    rv = mActivityDistributor->GetIsActive(&activityDistributorActive);
    if (NS_SUCCEEDED(rv) && activityDistributorActive) {
        // there are some observers registered at activity distributor, gather
        // nsISupports for the channel that called Init()
        mChannel = do_QueryInterface(eventsink);
        LOG(("nsHttpTransaction::Init() " \
             "mActivityDistributor is active " \
             "this=%p", this));
    } else {
        // there is no observer, so don't use it
        activityDistributorActive = false;
        mActivityDistributor = nullptr;
    }

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(eventsink);
    if (channel) {
        bool isInBrowser;
        NS_GetAppInfo(channel, &mAppId, &isInBrowser);
    }

#ifdef MOZ_WIDGET_GONK
    if (mAppId != NECKO_NO_APP_ID) {
        nsCOMPtr<nsINetworkInterface> activeNetwork;
        NS_GetActiveNetworkInterface(activeNetwork);
        mActiveNetwork =
            new nsMainThreadPtrHolder<nsINetworkInterface>(activeNetwork);
    }
#endif

    nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
        do_QueryInterface(eventsink);
    if (httpChannelInternal) {
        rv = httpChannelInternal->GetResponseTimeoutEnabled(
            &mResponseTimeoutEnabled);
        if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
        }
    }

    // create transport event sink proxy. it coalesces all events if and only
    // if the activity observer is not active. when the observer is active
    // we need not to coalesce any events to get all expected notifications
    // of the transaction state, necessary for correct debugging and logging.
    rv = net_NewTransportEventSinkProxy(getter_AddRefs(mTransportSink),
                                        eventsink, target,
                                        !activityDistributorActive);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(mConnInfo = cinfo);
    mCallbacks = callbacks;
    mConsumerTarget = target;
    mCaps = caps;

    if (requestHead->Method() == nsHttp::Head)
        mNoContent = true;

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
    if ((requestHead->Method() == nsHttp::Post || requestHead->Method() == nsHttp::Put) &&
        !requestBody && !requestHead->PeekHeader(nsHttp::Transfer_Encoding)) {
        requestHead->SetHeader(nsHttp::Content_Length, NS_LITERAL_CSTRING("0"));
    }

    // grab a weak reference to the request head
    mRequestHead = requestHead;

    // make sure we eliminate any proxy specific headers from
    // the request if we are using CONNECT
    bool pruneProxyHeaders = cinfo->UsingConnect();

    mReqHeaderBuf.Truncate();
    requestHead->Flatten(mReqHeaderBuf, pruneProxyHeaders);

#if defined(PR_LOGGING)
    if (LOG3_ENABLED()) {
        LOG3(("http request [\n"));
        LogHeaders(mReqHeaderBuf.get());
        LOG3(("]\n"));
    }
#endif

    // If the request body does not include headers or if there is no request
    // body, then we must add the header/body separator manually.
    if (!requestBodyHasHeaders || !requestBody)
        mReqHeaderBuf.AppendLiteral("\r\n");

    // report the request header
    if (mActivityDistributor)
        mActivityDistributor->ObserveActivity(
            mChannel,
            NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
            NS_HTTP_ACTIVITY_SUBTYPE_REQUEST_HEADER,
            PR_Now(), 0,
            mReqHeaderBuf);

    // Create a string stream for the request header buf (the stream holds
    // a non-owning reference to the request header data, so we MUST keep
    // mReqHeaderBuf around).
    nsCOMPtr<nsIInputStream> headers;
    rv = NS_NewByteInputStream(getter_AddRefs(headers),
                               mReqHeaderBuf.get(),
                               mReqHeaderBuf.Length());
    if (NS_FAILED(rv)) return rv;

    if (requestBody) {
        mHasRequestBody = true;

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
        rv = NS_NewBufferedInputStream(getter_AddRefs(mRequestStream), multi,
                                       nsIOService::gDefaultSegmentSize);
        if (NS_FAILED(rv)) return rv;
    }
    else
        mRequestStream = headers;

    rv = mRequestStream->Available(&mRequestSize);
    if (NS_FAILED(rv)) return rv;

    // create pipe for response stream
    rv = NS_NewPipe2(getter_AddRefs(mPipeIn),
                     getter_AddRefs(mPipeOut),
                     true, true,
                     nsIOService::gDefaultSegmentSize,
                     nsIOService::gDefaultSegmentCount);
    if (NS_FAILED(rv)) return rv;

    Classify();

    NS_ADDREF(*responseBody = mPipeIn);
    return NS_OK;
}

nsAHttpConnection *
nsHttpTransaction::Connection()
{
    return mConnection;
}

nsHttpResponseHead *
nsHttpTransaction::TakeResponseHead()
{
    MOZ_ASSERT(!mResponseHeadTaken, "TakeResponseHead called 2x");

    // Lock RestartInProgress() and TakeResponseHead() against main thread
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
    nsTArray<nsRefPtr<nsAHttpTransaction> > &outTransactions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------------
// nsHttpTransaction::nsAHttpTransaction
//----------------------------------------------------------------------------

void
nsHttpTransaction::SetConnection(nsAHttpConnection *conn)
{
    NS_IF_RELEASE(mConnection);
    NS_IF_ADDREF(mConnection = conn);

    if (conn) {
        MOZ_EVENT_TRACER_EXEC(static_cast<nsAHttpTransaction*>(this),
                              "net::http::transaction");
    }
}

void
nsHttpTransaction::GetSecurityCallbacks(nsIInterfaceRequestor **cb)
{
    MutexAutoLock lock(mCallbacksLock);
    NS_IF_ADDREF(*cb = mCallbacks);
}

void
nsHttpTransaction::SetSecurityCallbacks(nsIInterfaceRequestor* aCallbacks)
{
    {
        MutexAutoLock lock(mCallbacksLock);
        mCallbacks = aCallbacks;
    }

    if (gSocketTransportService) {
        nsRefPtr<UpdateSecurityCallbacks> event = new UpdateSecurityCallbacks(this, aCallbacks);
        gSocketTransportService->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
    }
}

void
nsHttpTransaction::OnTransportStatus(nsITransport* transport,
                                     nsresult status, uint64_t progress)
{
    LOG(("nsHttpTransaction::OnSocketStatus [this=%p status=%x progress=%llu]\n",
        this, status, progress));

    if (TimingEnabled()) {
        if (status == NS_NET_STATUS_RESOLVING_HOST) {
            mTimings.domainLookupStart = TimeStamp::Now();
        } else if (status == NS_NET_STATUS_RESOLVED_HOST) {
            mTimings.domainLookupEnd = TimeStamp::Now();
        } else if (status == NS_NET_STATUS_CONNECTING_TO) {
            mTimings.connectStart = TimeStamp::Now();
        } else if (status == NS_NET_STATUS_CONNECTED_TO) {
            mTimings.connectEnd = TimeStamp::Now();
        }
    }

    if (!mTransportSink)
        return;

    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    // Need to do this before the STATUS_RECEIVING_FROM check below, to make
    // sure that the activity distributor gets told about all status events.
    if (mActivityDistributor) {
        // upon STATUS_WAITING_FOR; report request body sent
        if ((mHasRequestBody) &&
            (status == NS_NET_STATUS_WAITING_FOR))
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_REQUEST_BODY_SENT,
                PR_Now(), 0, EmptyCString());

        // report the status and progress
        if (!mRestartInProgressVerifier.IsDiscardingContent())
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_SOCKET_TRANSPORT,
                static_cast<uint32_t>(status),
                PR_Now(),
                progress,
                EmptyCString());
    }

    // nsHttpChannel synthesizes progress events in OnDataAvailable
    if (status == NS_NET_STATUS_RECEIVING_FROM)
        return;

    uint64_t progressMax;

    if (status == NS_NET_STATUS_SENDING_TO) {
        // suppress progress when only writing request headers
        if (!mHasRequestBody)
            return;

        nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mRequestStream);
        MOZ_ASSERT(seekable, "Request stream isn't seekable?!?");

        int64_t prog = 0;
        seekable->Tell(&prog);
        progress = prog;

        // when uploading, we include the request headers in the progress
        // notifications.
        progressMax = mRequestSize; // XXX mRequestSize is 32-bit!
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

uint64_t
nsHttpTransaction::Available()
{
    uint64_t size;
    if (NS_FAILED(mRequestStream->Available(&size)))
        size = 0;
    return size;
}

NS_METHOD
nsHttpTransaction::ReadRequestSegment(nsIInputStream *stream,
                                      void *closure,
                                      const char *buf,
                                      uint32_t offset,
                                      uint32_t count,
                                      uint32_t *countRead)
{
    nsHttpTransaction *trans = (nsHttpTransaction *) closure;
    nsresult rv = trans->mReader->OnReadSegment(buf, count, countRead);
    if (NS_FAILED(rv)) return rv;

    if (trans->TimingEnabled() && trans->mTimings.requestStart.IsNull()) {
        // First data we're sending -> this is requestStart
        trans->mTimings.requestStart = TimeStamp::Now();
    }

    if (!trans->mSentData) {
        MOZ_EVENT_TRACER_MARK(static_cast<nsAHttpTransaction*>(trans),
                              "net::http::first-write");
    }

    trans->CountSentBytes(*countRead);
    trans->mSentData = true;
    return NS_OK;
}

nsresult
nsHttpTransaction::ReadSegments(nsAHttpSegmentReader *reader,
                                uint32_t count, uint32_t *countRead)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (mTransactionDone) {
        *countRead = 0;
        return mStatus;
    }

    if (!mConnected) {
        mConnected = true;
        mConnection->GetSecurityInfo(getter_AddRefs(mSecurityInfo));
    }

    mReader = reader;

    nsresult rv = mRequestStream->ReadSegments(ReadRequestSegment, this, count, countRead);

    mReader = nullptr;

    // if read would block then we need to AsyncWait on the request stream.
    // have callback occur on socket thread so we stay synchronized.
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        nsCOMPtr<nsIAsyncInputStream> asyncIn =
                do_QueryInterface(mRequestStream);
        if (asyncIn) {
            nsCOMPtr<nsIEventTarget> target;
            gHttpHandler->GetSocketThreadTarget(getter_AddRefs(target));
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

NS_METHOD
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

    if (trans->TimingEnabled() && trans->mTimings.responseStart.IsNull()) {
        trans->mTimings.responseStart = TimeStamp::Now();
    }

    nsresult rv;
    //
    // OK, now let the caller fill this segment with data.
    //
    if (!trans->mHaveAllHeaders) {
        // Reading too far ahead on a chunked encoding is expensive (see below)
        // so limit header reads to 1KB to minimize any potential damage while
        // we figure out the message delimiter.
        count = std::min(count, 1024U);
    } else if (trans->mChunkedDecoder) {
        // The API in use requires us to return entity data in 1 contiguous
        // buffer supplied by the caller. That means
        // all entity data needs to be moved up to cover gaps created by
        // stripping the chunk length. We can mitigate this problem by
        // doing short reads when we are parsing chunk lengths and limiting
        // reads in the middle of chunks to the remaining chunk size to
        // increase the frequency of chunk markers naturally aligning
        // at the start.
        if (!trans->mChunkedDecoder->GetChunkRemaining()) {
            count = std::min(count, 6U); // 6 is 4 hex chars + CRLF
        } else {
            // extra 2 bytes is for CRLF that terminates the chunk
            count = std::min(count,
                             trans->mChunkedDecoder->GetChunkRemaining() + 2);
        }
    }
    rv = trans->mWriter->OnWriteSegment(buf, count, countWritten);
    if (NS_FAILED(rv)) return rv; // caller didn't want to write anything

    if (!trans->mReceivedData) {
        MOZ_EVENT_TRACER_MARK(static_cast<nsAHttpTransaction*>(trans),
                              "net::http::first-read");
    }

    MOZ_ASSERT(*countWritten > 0, "bad writer");
    trans->CountRecvBytes(*countWritten);
    trans->mReceivedData = true;

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

nsresult
nsHttpTransaction::WriteSegments(nsAHttpSegmentWriter *writer,
                                 uint32_t count, uint32_t *countWritten)
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (mTransactionDone)
        return NS_SUCCEEDED(mStatus) ? NS_BASE_STREAM_CLOSED : mStatus;

    mWriter = writer;

    nsresult rv = mPipeOut->WriteSegments(WritePipeSegment, this, count, countWritten);

    mWriter = nullptr;

    // if pipe would block then we need to AsyncWait on it.  have callback
    // occur on socket thread so we stay synchronized.
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        nsCOMPtr<nsIEventTarget> target;
        gHttpHandler->GetSocketThreadTarget(getter_AddRefs(target));
        if (target)
            mPipeOut->AsyncWait(this, 0, 0, target);
        else {
            NS_ERROR("no socket thread event target");
            rv = NS_ERROR_UNEXPECTED;
        }
    }

    return rv;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction save network statistics event
//-----------------------------------------------------------------------------

#ifdef MOZ_WIDGET_GONK
namespace {
class SaveNetworkStatsEvent : public nsRunnable {
public:
    SaveNetworkStatsEvent(uint32_t aAppId,
                          nsMainThreadPtrHandle<nsINetworkInterface> &aActiveNetwork,
                          uint64_t aCountRecv,
                          uint64_t aCountSent)
        : mAppId(aAppId),
          mActiveNetwork(aActiveNetwork),
          mCountRecv(aCountRecv),
          mCountSent(aCountSent)
    {
        MOZ_ASSERT(mAppId != NECKO_NO_APP_ID);
        MOZ_ASSERT(mActiveNetwork);
    }

    NS_IMETHOD Run()
    {
        MOZ_ASSERT(NS_IsMainThread());

        nsresult rv;
        nsCOMPtr<nsINetworkStatsServiceProxy> mNetworkStatsServiceProxy =
            do_GetService("@mozilla.org/networkstatsServiceProxy;1", &rv);
        if (NS_FAILED(rv)) {
            return rv;
        }

        // save the network stats through NetworkStatsServiceProxy
        mNetworkStatsServiceProxy->SaveAppStats(mAppId,
                                                mActiveNetwork,
                                                PR_Now() / 1000,
                                                mCountRecv,
                                                mCountSent,
                                                false,
                                                nullptr);

        return NS_OK;
    }
private:
    uint32_t mAppId;
    nsMainThreadPtrHandle<nsINetworkInterface> mActiveNetwork;
    uint64_t mCountRecv;
    uint64_t mCountSent;
};
};
#endif

nsresult
nsHttpTransaction::SaveNetworkStats(bool enforce)
{
#ifdef MOZ_WIDGET_GONK
    // Check if active network and appid are valid.
    if (!mActiveNetwork || mAppId == NECKO_NO_APP_ID) {
        return NS_OK;
    }

    if (mCountRecv <= 0 && mCountSent <= 0) {
        // There is no traffic, no need to save.
        return NS_OK;
    }

    // If |enforce| is false, the traffic amount is saved
    // only when the total amount exceeds the predefined
    // threshold.
    uint64_t totalBytes = mCountRecv + mCountSent;
    if (!enforce && totalBytes < NETWORK_STATS_THRESHOLD) {
        return NS_OK;
    }

    // Create the event to save the network statistics.
    // the event is then dispathed to the main thread.
    nsRefPtr<nsRunnable> event =
        new SaveNetworkStatsEvent(mAppId, mActiveNetwork,
                                  mCountRecv, mCountSent);
    NS_DispatchToMainThread(event);

    // Reset the counters after saving.
    mCountSent = 0;
    mCountRecv = 0;

    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

void
nsHttpTransaction::Close(nsresult reason)
{
    LOG(("nsHttpTransaction::Close [this=%p reason=%x]\n", this, reason));

    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if (mClosed) {
        LOG(("  already closed\n"));
        return;
    }

    if (mTokenBucketCancel) {
        mTokenBucketCancel->Cancel(reason);
        mTokenBucketCancel = nullptr;
    }

    if (mActivityDistributor) {
        // report the reponse is complete if not already reported
        if (!mResponseIsComplete)
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_COMPLETE,
                PR_Now(),
                static_cast<uint64_t>(mContentRead),
                EmptyCString());

        // report that this transaction is closing
        mActivityDistributor->ObserveActivity(
            mChannel,
            NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
            NS_HTTP_ACTIVITY_SUBTYPE_TRANSACTION_CLOSE,
            PR_Now(), 0, EmptyCString());
    }

    // we must no longer reference the connection!  find out if the
    // connection was being reused before letting it go.
    bool connReused = false;
    if (mConnection)
        connReused = mConnection->IsReused();
    mConnected = false;

    //
    // if the connection was reset or closed before we wrote any part of the
    // request or if we wrote the request but didn't receive any part of the
    // response and the connection was being reused, then we can (and really
    // should) assume that we wrote to a stale connection and we must therefore
    // repeat the request over a new connection.
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
    if (reason == NS_ERROR_NET_RESET || reason == NS_OK) {

        // reallySentData is meant to separate the instances where data has
        // been sent by this transaction but buffered at a higher level while
        // a TLS session (perhaps via a tunnel) is setup.
        bool reallySentData =
            mSentData && (!mConnection || mConnection->BytesWritten());

        if (!mReceivedData &&
            (!reallySentData || connReused || mPipelinePosition)) {
            // if restarting fails, then we must proceed to close the pipe,
            // which will notify the channel that the transaction failed.

            if (mPipelinePosition) {
                gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
                    mConnInfo, nsHttpConnectionMgr::RedCanceledPipeline,
                    nullptr, 0);
            }
            if (NS_SUCCEEDED(Restart()))
                return;
        }
        else if (!mResponseIsComplete && mPipelinePosition &&
                 reason == NS_ERROR_NET_RESET) {
            // due to unhandled rst on a pipeline - safe to
            // restart as only idempotent is found there

            gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
                mConnInfo, nsHttpConnectionMgr::RedCorruptedContent, nullptr, 0);
            if (NS_SUCCEEDED(RestartInProgress()))
                return;
        }
    }

    bool relConn = true;
    if (NS_SUCCEEDED(reason)) {
        if (!mResponseIsComplete) {
            // The response has not been delimited with a high-confidence
            // algorithm like Content-Length or Chunked Encoding. We
            // need to use a strong framing mechanism to pipeline.
            gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
                mConnInfo, nsHttpConnectionMgr::BadInsufficientFraming,
                nullptr, mClassification);
        }
        else if (mPipelinePosition) {
            // report this success as feedback
            gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
                mConnInfo, nsHttpConnectionMgr::GoodCompletedOK,
                nullptr, mPipelinePosition);
        }

        // the server has not sent the final \r\n terminating the header
        // section, and there may still be a header line unparsed.  let's make
        // sure we parse the remaining header line, and then hopefully, the
        // response will be usable (see bug 88792).
        if (!mHaveAllHeaders) {
            char data = '\n';
            uint32_t unused;
            ParseHead(&data, 1, &unused);

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
    if (TimingEnabled() &&
        mTimings.responseEnd.IsNull() && !mTimings.responseStart.IsNull())
        mTimings.responseEnd = TimeStamp::Now();

    if (relConn && mConnection)
        NS_RELEASE(mConnection);

    // save network statistics in the end of transaction
    SaveNetworkStats(true);

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

    MOZ_EVENT_TRACER_DONE(static_cast<nsAHttpTransaction*>(this),
                          "net::http::transaction");
}

nsresult
nsHttpTransaction::AddTransaction(nsAHttpTransaction *trans)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t
nsHttpTransaction::PipelineDepth()
{
    return IsDone() ? 0 : 1;
}

nsresult
nsHttpTransaction::SetPipelinePosition(int32_t position)
{
    mPipelinePosition = position;
    return NS_OK;
}

int32_t
nsHttpTransaction::PipelinePosition()
{
    return mPipelinePosition;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpTransaction::RestartInProgress()
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    if ((mRestartCount + 1) >= gHttpHandler->MaxRequestAttempts()) {
        LOG(("nsHttpTransaction::RestartInProgress() "
             "reached max request attempts, failing transaction %p\n", this));
        return NS_ERROR_NET_RESET;
    }

    // Lock RestartInProgress() and TakeResponseHead() against main thread
    MutexAutoLock lock(*nsHttp::GetLock());

    // Don't try and RestartInProgress() things that haven't gotten a response
    // header yet. Those should be handled under the normal restart() path if
    // they are eligible.
    if (!mHaveAllHeaders)
        return NS_ERROR_NET_RESET;

    // don't try and restart 0.9 or non 200/Get HTTP/1
    if (!mRestartInProgressVerifier.IsSetup())
        return NS_ERROR_NET_RESET;

    LOG(("Will restart transaction %p and skip first %lld bytes, "
         "old Content-Length %lld",
         this, mContentRead, mContentLength));

    mRestartInProgressVerifier.SetAlreadyProcessed(
        std::max(mRestartInProgressVerifier.AlreadyProcessed(), mContentRead));

    if (!mResponseHeadTaken && !mForTakeResponseHead) {
        // TakeResponseHeader() has not been called yet and this
        // is the first restart. Store the resp headers exclusively
        // for TakeResponseHead() which is called from the main thread and
        // could happen at any time - so we can't continue to modify those
        // headers (which restarting will effectively do)
        mForTakeResponseHead = mResponseHead;
        mResponseHead = nullptr;
    }

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

    return Restart();
}

nsresult
nsHttpTransaction::Restart()
{
    MOZ_ASSERT(PR_GetCurrentThread() == gSocketThread);

    // limit the number of restart attempts - bug 92224
    if (++mRestartCount >= gHttpHandler->MaxRequestAttempts()) {
        LOG(("reached max request attempts, failing transaction @%p\n", this));
        return NS_ERROR_NET_RESET;
    }

    LOG(("restarting transaction @%p\n", this));

    // rewind streams in case we already wrote out the request
    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mRequestStream);
    if (seekable)
        seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);

    // clear old connection state...
    mSecurityInfo = 0;
    NS_IF_RELEASE(mConnection);

    // disable pipelining for the next attempt in case pipelining caused the
    // reset.  this is being overly cautious since we don't know if pipelining
    // was the problem here.
    mCaps &= ~NS_HTTP_ALLOW_PIPELINING;
    SetPipelinePosition(0);

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
            return 0;
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
                return 0;
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
    return 0;
}

nsresult
nsHttpTransaction::ParseLine(char *line)
{
    LOG(("nsHttpTransaction::ParseLine [%s]\n", line));
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
    NS_PRECONDITION(!mHaveAllHeaders, "already have all headers");

    if (!mLineBuf.IsEmpty() && mLineBuf.Last() == '\n') {
        // trim off the new line char, and if this segment is
        // not a continuation of the previous or if we haven't
        // parsed the status line yet, then parse the contents
        // of mLineBuf.
        mLineBuf.Truncate(mLineBuf.Length() - 1);
        if (!mHaveStatusLine || (*segment != ' ' && *segment != '\t')) {
            nsresult rv = ParseLine(mLineBuf.BeginWriting());
            mLineBuf.Truncate();
            if (NS_FAILED(rv)) {
                gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
                    mConnInfo, nsHttpConnectionMgr::RedCorruptedContent,
                    nullptr, 0);
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

    NS_PRECONDITION(!mHaveAllHeaders, "oops");

    // allocate the response head object if necessary
    if (!mResponseHead) {
        mResponseHead = new nsHttpResponseHead();
        if (!mResponseHead)
            return NS_ERROR_OUT_OF_MEMORY;

        // report that we have a least some of the response
        if (mActivityDistributor && !mReportedStart) {
            mReportedStart = true;
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_START,
                PR_Now(), 0, EmptyCString());
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
                if (mRequestHead->Method() == nsHttp::Put)
                    return NS_ERROR_ABORT;

                mResponseHead->ParseStatusLine("");
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

// called on the socket thread
nsresult
nsHttpTransaction::HandleContentStart()
{
    LOG(("nsHttpTransaction::HandleContentStart [this=%p]\n", this));

    if (mResponseHead) {
#if defined(PR_LOGGING)
        if (LOG3_ENABLED()) {
            LOG3(("http response [\n"));
            nsAutoCString headers;
            mResponseHead->Flatten(headers, false);
            LogHeaders(headers.get());
            LOG3(("]\n"));
        }
#endif
        // Save http version, mResponseHead isn't available anymore after
        // TakeResponseHead() is called
        mHttpVersion = mResponseHead->Version();

        // notify the connection, give it a chance to cause a reset.
        bool reset = false;
        if (!mRestartInProgressVerifier.IsSetup())
            mConnection->OnHeadersAvailable(this, mRequestHead, mResponseHead, &reset);

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
            mPreserveStream = true;    // fall through to other no content
        case 204:
        case 205:
        case 304:
            mNoContent = true;
            LOG(("this response should not contain a body.\n"));
            break;
        }

        if (mResponseHead->Status() == 200 &&
            mConnection->IsProxyConnectInProgress()) {
            // successful CONNECTs do not have response bodies
            mNoContent = true;
        }
        mConnection->SetLastTransactionExpectedNoContent(mNoContent);
        if (mInvalidResponseBytesRead)
            gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
                mConnInfo, nsHttpConnectionMgr::BadInsufficientFraming,
                nullptr, mClassification);

        if (mNoContent)
            mContentLength = 0;
        else {
            // grab the content-length from the response headers
            mContentLength = mResponseHead->ContentLength();

            if ((mClassification != CLASS_SOLO) &&
                (mContentLength > mMaxPipelineObjectSize))
                CancelPipeline(nsHttpConnectionMgr::BadUnexpectedLarge);

            // handle chunked encoding here, so we'll know immediately when
            // we're done with the socket.  please note that _all_ other
            // decoding is done when the channel receives the content data
            // so as not to block the socket transport thread too much.
            // ignore chunked responses from HTTP/1.0 servers and proxies.
            if (mResponseHead->Version() >= NS_HTTP_VERSION_1_1 &&
                mResponseHead->HasHeaderValue(nsHttp::Transfer_Encoding, "chunked")) {
                // we only support the "chunked" transfer encoding right now.
                mChunkedDecoder = new nsHttpChunkedDecoder();
                if (!mChunkedDecoder)
                    return NS_ERROR_OUT_OF_MEMORY;
                LOG(("chunked decoder created\n"));
                // Ignore server specified Content-Length.
                mContentLength = -1;
            }
#if defined(PR_LOGGING)
            else if (mContentLength == int64_t(-1))
                LOG(("waiting for the server to close the connection.\n"));
#endif
        }
        if (mRestartInProgressVerifier.IsSetup() &&
            !mRestartInProgressVerifier.Verify(mContentLength, mResponseHead)) {
            LOG(("Restart in progress subsequent transaction failed to match"));
            return NS_ERROR_ABORT;
        }
    }

    mDidContentStart = true;

    // The verifier only initializes itself once (from the first iteration of
    // a transaction that gets far enough to have response headers)
    if (mRequestHead->Method() == nsHttp::Get)
        mRestartInProgressVerifier.Set(mContentLength, mResponseHead);

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

    int64_t toReadBeforeRestart =
        mRestartInProgressVerifier.ToReadBeforeRestart();

    if (toReadBeforeRestart && *contentRead) {
        uint32_t ignore =
            static_cast<uint32_t>(std::min<int64_t>(toReadBeforeRestart, UINT32_MAX));
        ignore = std::min(*contentRead, ignore);
        LOG(("Due To Restart ignoring %d of remaining %ld",
             ignore, toReadBeforeRestart));
        *contentRead -= ignore;
        mContentRead += ignore;
        mRestartInProgressVerifier.HaveReadBeforeRestart(ignore);
        memmove(buf, buf + ignore, *contentRead + *contentRemaining);
    }

    if (*contentRead) {
        // update count of content bytes read and report progress...
        mContentRead += *contentRead;
        /* when uncommenting, take care of 64-bit integers w/ std::max...
        if (mProgressSink)
            mProgressSink->OnProgress(nullptr, nullptr, mContentRead, std::max(0, mContentLength));
        */
    }

    LOG(("nsHttpTransaction::HandleContent [this=%p count=%u read=%u mContentRead=%lld mContentLength=%lld]\n",
        this, count, *contentRead, mContentRead, mContentLength));

    // Check the size of chunked responses. If we exceed the max pipeline size
    // for this response reschedule the pipeline
    if ((mClassification != CLASS_SOLO) &&
        mChunkedDecoder &&
        ((mContentRead + mChunkedDecoder->GetChunkRemaining()) >
         mMaxPipelineObjectSize)) {
        CancelPipeline(nsHttpConnectionMgr::BadUnexpectedLarge);
    }

    // check for end-of-file
    if ((mContentRead == mContentLength) ||
        (mChunkedDecoder && mChunkedDecoder->ReachedEOF())) {
        // the transaction is done with a complete response.
        mTransactionDone = true;
        mResponseIsComplete = true;
        ReleaseBlockingTransaction();

        if (TimingEnabled())
            mTimings.responseEnd = TimeStamp::Now();

        // report the entire response has arrived
        if (mActivityDistributor)
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_COMPLETE,
                PR_Now(),
                static_cast<uint64_t>(mContentRead),
                EmptyCString());
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
            mActivityDistributor->ObserveActivity(
                mChannel,
                NS_HTTP_ACTIVITY_TYPE_HTTP_TRANSACTION,
                NS_HTTP_ACTIVITY_SUBTYPE_RESPONSE_HEADER,
                PR_Now(), 0,
                completeResponseHeaders);
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
        // countRemaining : bytes corresponding to next pipelined transaction
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
            mConnection->PushBack(buf + *countRead, countRemaining);
        }
    }

    return NS_OK;
}

void
nsHttpTransaction::CancelPipeline(uint32_t reason)
{
    // reason is casted through a uint to avoid compiler header deps
    gHttpHandler->ConnMgr()->PipelineFeedbackInfo(
        mConnInfo,
        static_cast<nsHttpConnectionMgr::PipelineFeedbackInfoType>(reason),
        nullptr, mClassification);

    mConnection->CancelPipeline(NS_ERROR_ABORT);

    // Avoid pipelining this transaction on restart by classifying it as solo.
    // This also prevents BadUnexpectedLarge from being reported more
    // than one time per transaction.
    mClassification = CLASS_SOLO;
}

// Called when the transaction marked for blocking is associated with a connection
// (i.e. added to a spdy session, an idle http connection, or placed into
// a http pipeline). It is safe to call this multiple times with it only
// having an effect once.
void
nsHttpTransaction::DispatchedAsBlocking()
{
    if (mDispatchedAsBlocking)
        return;

    LOG(("nsHttpTransaction %p dispatched as blocking\n", this));

    if (!mLoadGroupCI)
        return;

    LOG(("nsHttpTransaction adding blocking channel %p from "
         "loadgroup %p\n", this, mLoadGroupCI.get()));

    mLoadGroupCI->AddBlockingTransaction();
    mDispatchedAsBlocking = true;
}

void
nsHttpTransaction::RemoveDispatchedAsBlocking()
{
    if (!mLoadGroupCI || !mDispatchedAsBlocking)
        return;

    uint32_t blockers = 0;
    nsresult rv = mLoadGroupCI->RemoveBlockingTransaction(&blockers);

    LOG(("nsHttpTransaction removing blocking channel %p from "
         "loadgroup %p. %d blockers remain.\n", this,
         mLoadGroupCI.get(), blockers));

    if (NS_SUCCEEDED(rv) && !blockers) {
        LOG(("nsHttpTransaction %p triggering release of blocked channels.\n",
             this));
        gHttpHandler->ConnMgr()->ProcessPendingQ();
    }

    mDispatchedAsBlocking = false;
}

void
nsHttpTransaction::ReleaseBlockingTransaction()
{
    RemoveDispatchedAsBlocking();
    mLoadGroupCI = nullptr;
}

//-----------------------------------------------------------------------------
// nsHttpTransaction deletion event
//-----------------------------------------------------------------------------

class nsDeleteHttpTransaction : public nsRunnable {
public:
    nsDeleteHttpTransaction(nsHttpTransaction *trans)
        : mTrans(trans)
    {}

    NS_IMETHOD Run()
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
        nsCOMPtr<nsIRunnable> event = new nsDeleteHttpTransaction(this);
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
    gHttpHandler->SubmitPacedRequest(this, getter_AddRefs(mTokenBucketCancel));
    mSynchronousRatePaceRequest = false;
    return mPassedRatePacing;
}

void
nsHttpTransaction::OnTokenBucketAdmitted()
{
    mPassedRatePacing = true;
    mTokenBucketCancel = nullptr;

    if (!mSynchronousRatePaceRequest)
        gHttpHandler->ConnMgr()->ProcessPendingQ(mConnInfo);
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

NS_IMETHODIMP_(nsrefcnt)
nsHttpTransaction::Release()
{
    nsrefcnt count;
    NS_PRECONDITION(0 != mRefCnt, "dup release");
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

NS_IMPL_QUERY_INTERFACE2(nsHttpTransaction,
                         nsIInputStreamCallback,
                         nsIOutputStreamCallback)

//-----------------------------------------------------------------------------
// nsHttpTransaction::nsIInputStreamCallback
//-----------------------------------------------------------------------------

// called on the socket thread
NS_IMETHODIMP
nsHttpTransaction::OnInputStreamReady(nsIAsyncInputStream *out)
{
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
    if (mConnection) {
        nsresult rv = mConnection->ResumeRecv();
        if (NS_FAILED(rv))
            NS_ERROR("ResumeRecv failed");
    }
    return NS_OK;
}

// nsHttpTransaction::RestartVerifier

static bool
matchOld(nsHttpResponseHead *newHead, nsCString &old,
         nsHttpAtom headerAtom)
{
    const char *val;

    val = newHead->PeekHeader(headerAtom);
    if (val && old.IsEmpty())
        return false;
    if (!val && !old.IsEmpty())
        return false;
    if (val && !old.Equals(val))
        return false;
    return true;
}

bool
nsHttpTransaction::RestartVerifier::Verify(int64_t contentLength,
                                           nsHttpResponseHead *newHead)
{
    if (mContentLength != contentLength)
        return false;

    if (newHead->Status() != 200)
        return false;

    if (!matchOld(newHead, mContentRange, nsHttp::Content_Range))
        return false;

    if (!matchOld(newHead, mLastModified, nsHttp::Last_Modified))
        return false;

    if (!matchOld(newHead, mETag, nsHttp::ETag))
        return false;

    if (!matchOld(newHead, mContentEncoding, nsHttp::Content_Encoding))
        return false;

    if (!matchOld(newHead, mTransferEncoding, nsHttp::Transfer_Encoding))
        return false;

    return true;
}

void
nsHttpTransaction::RestartVerifier::Set(int64_t contentLength,
                                        nsHttpResponseHead *head)
{
    if (mSetup)
        return;

    // If mSetup does not transition to true RestartInPogress() is later
    // forbidden

    // Only RestartInProgress with 200 response code
    if (head->Status() != 200)
        return;

    mContentLength = contentLength;

    if (head) {
        const char *val;
        val = head->PeekHeader(nsHttp::ETag);
        if (val)
            mETag.Assign(val);
        val = head->PeekHeader(nsHttp::Last_Modified);
        if (val)
            mLastModified.Assign(val);
        val = head->PeekHeader(nsHttp::Content_Range);
        if (val)
            mContentRange.Assign(val);
        val = head->PeekHeader(nsHttp::Content_Encoding);
        if (val)
            mContentEncoding.Assign(val);
        val = head->PeekHeader(nsHttp::Transfer_Encoding);
        if (val)
            mTransferEncoding.Assign(val);

        // We can only restart with any confidence if we have a stored etag or
        // last-modified header
        if (mETag.IsEmpty() && mLastModified.IsEmpty())
            return;

        mSetup = true;
    }
}

} // namespace mozilla::net
} // namespace mozilla
