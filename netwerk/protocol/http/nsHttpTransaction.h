/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpTransaction_h__
#define nsHttpTransaction_h__

#include "nsHttp.h"
#include "nsAHttpTransaction.h"
#include "nsAHttpConnection.h"
#include "EventTokenBucket.h"
#include "nsCOMPtr.h"
#include "nsThreadUtils.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "TimingStruct.h"

#ifdef MOZ_WIDGET_GONK
#include "nsINetworkManager.h"
#include "nsProxyRelease.h"
#endif

//-----------------------------------------------------------------------------

class nsIHttpActivityObserver;
class nsIEventTarget;
class nsIInputStream;
class nsIOutputStream;

namespace mozilla { namespace net {

class nsHttpChunkedDecoder;
class nsHttpRequestHead;
class nsHttpResponseHead;

//-----------------------------------------------------------------------------
// nsHttpTransaction represents a single HTTP transaction.  It is thread-safe,
// intended to run on the socket thread.
//-----------------------------------------------------------------------------

class nsHttpTransaction MOZ_FINAL : public nsAHttpTransaction
                                  , public ATokenBucketEvent
                                  , public nsIInputStreamCallback
                                  , public nsIOutputStreamCallback
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSAHTTPTRANSACTION
    NS_DECL_NSIINPUTSTREAMCALLBACK
    NS_DECL_NSIOUTPUTSTREAMCALLBACK

    nsHttpTransaction();

    //
    // called to initialize the transaction
    //
    // @param caps
    //        the transaction capabilities (see nsHttp.h)
    // @param connInfo
    //        the connection type for this transaction.
    // @param reqHeaders
    //        the request header struct
    // @param reqBody
    //        the request body (POST or PUT data stream)
    // @param reqBodyIncludesHeaders
    //        fun stuff to support NPAPI plugins.
    // @param target
    //        the dispatch target were notifications should be sent.
    // @param callbacks
    //        the notification callbacks to be given to PSM.
    // @param responseBody
    //        the input stream that will contain the response data.  async
    //        wait on this input stream for data.  on first notification,
    //        headers should be available (check transaction status).
    //
    nsresult Init(uint32_t               caps,
                  nsHttpConnectionInfo  *connInfo,
                  nsHttpRequestHead     *reqHeaders,
                  nsIInputStream        *reqBody,
                  bool                   reqBodyIncludesHeaders,
                  nsIEventTarget        *consumerTarget,
                  nsIInterfaceRequestor *callbacks,
                  nsITransportEventSink *eventsink,
                  nsIAsyncInputStream  **responseBody);

    // attributes
    nsHttpResponseHead    *ResponseHead()   { return mHaveAllHeaders ? mResponseHead : nullptr; }
    nsISupports           *SecurityInfo()   { return mSecurityInfo; }

    nsIEventTarget        *ConsumerTarget() { return mConsumerTarget; }

    void SetSecurityCallbacks(nsIInterfaceRequestor* aCallbacks);

    // Called to take ownership of the response headers; the transaction
    // will drop any reference to the response headers after this call.
    nsHttpResponseHead *TakeResponseHead();

    // Provides a thread safe reference of the connection
    // nsHttpTransaction::Connection should only be used on the socket thread
    already_AddRefed<nsAHttpConnection> GetConnectionReference();

    // Called to find out if the transaction generated a complete response.
    bool ResponseIsComplete() { return mResponseIsComplete; }

    bool      ProxyConnectFailed() { return mProxyConnectFailed; }

    // setting mDontRouteViaWildCard to true means the transaction should only
    // be dispatched on a specific ConnectionInfo Hash Key (as opposed to a
    // generic wild card one). That means in the specific case of carrying this
    // transaction on an HTTP/2 tunnel it will only be dispatched onto an
    // existing tunnel instead of triggering creation of a new one.
    void SetDontRouteViaWildCard(bool var) { mDontRouteViaWildCard = var; }
    bool DontRouteViaWildCard() { return mDontRouteViaWildCard; }
    void EnableKeepAlive() { mCaps |= NS_HTTP_ALLOW_KEEPALIVE; }
    void MakeSticky() { mCaps |= NS_HTTP_STICKY_CONNECTION; }

    // SetPriority() may only be used by the connection manager.
    void    SetPriority(int32_t priority) { mPriority = priority; }
    int32_t    Priority()                 { return mPriority; }

    const TimingStruct& Timings() const { return mTimings; }
    enum Classifier Classification() { return mClassification; }

    void PrintDiagnostics(nsCString &log);

    // Sets mPendingTime to the current time stamp or to a null time stamp (if now is false)
    void SetPendingTime(bool now = true) { mPendingTime = now ? TimeStamp::Now() : TimeStamp(); }
    const TimeStamp GetPendingTime() { return mPendingTime; }
    bool UsesPipelining() const { return mCaps & NS_HTTP_ALLOW_PIPELINING; }

    // overload of nsAHttpTransaction::LoadGroupConnectionInfo()
    nsILoadGroupConnectionInfo *LoadGroupConnectionInfo() { return mLoadGroupCI.get(); }
    void SetLoadGroupConnectionInfo(nsILoadGroupConnectionInfo *aLoadGroupCI) { mLoadGroupCI = aLoadGroupCI; }
    void DispatchedAsBlocking();
    void RemoveDispatchedAsBlocking();

    nsHttpTransaction *QueryHttpTransaction() MOZ_OVERRIDE { return this; }

private:
    friend class DeleteHttpTransaction;
    virtual ~nsHttpTransaction();

    nsresult Restart();
    nsresult RestartInProgress();
    char    *LocateHttpStart(char *buf, uint32_t len,
                             bool aAllowPartialMatch);
    nsresult ParseLine(char *line);
    nsresult ParseLineSegment(char *seg, uint32_t len);
    nsresult ParseHead(char *, uint32_t count, uint32_t *countRead);
    nsresult HandleContentStart();
    nsresult HandleContent(char *, uint32_t count, uint32_t *contentRead, uint32_t *contentRemaining);
    nsresult ProcessData(char *, uint32_t, uint32_t *);
    void     DeleteSelfOnConsumerThread();
    void     ReleaseBlockingTransaction();

    Classifier Classify();
    void       CancelPipeline(uint32_t reason);

    static NS_METHOD ReadRequestSegment(nsIInputStream *, void *, const char *,
                                        uint32_t, uint32_t, uint32_t *);
    static NS_METHOD WritePipeSegment(nsIOutputStream *, void *, char *,
                                      uint32_t, uint32_t, uint32_t *);

    bool TimingEnabled() const { return mCaps & NS_HTTP_TIMING_ENABLED; }

    bool ResponseTimeoutEnabled() const MOZ_FINAL;

private:
    class UpdateSecurityCallbacks : public nsRunnable
    {
      public:
        UpdateSecurityCallbacks(nsHttpTransaction* aTrans,
                                nsIInterfaceRequestor* aCallbacks)
        : mTrans(aTrans), mCallbacks(aCallbacks) {}

        NS_IMETHOD Run()
        {
            if (mTrans->mConnection)
                mTrans->mConnection->SetSecurityCallbacks(mCallbacks);
            return NS_OK;
        }
      private:
        nsRefPtr<nsHttpTransaction> mTrans;
        nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    };

    Mutex mLock;

    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    nsCOMPtr<nsITransportEventSink> mTransportSink;
    nsCOMPtr<nsIEventTarget>        mConsumerTarget;
    nsCOMPtr<nsISupports>           mSecurityInfo;
    nsCOMPtr<nsIAsyncInputStream>   mPipeIn;
    nsCOMPtr<nsIAsyncOutputStream>  mPipeOut;
    nsCOMPtr<nsILoadGroupConnectionInfo> mLoadGroupCI;

    nsCOMPtr<nsISupports>             mChannel;
    nsCOMPtr<nsIHttpActivityObserver> mActivityDistributor;

    nsCString                       mReqHeaderBuf;    // flattened request headers
    nsCOMPtr<nsIInputStream>        mRequestStream;
    uint64_t                        mRequestSize;

    nsRefPtr<nsAHttpConnection>     mConnection;
    nsRefPtr<nsHttpConnectionInfo>  mConnInfo;
    nsHttpRequestHead              *mRequestHead;     // weak ref
    nsHttpResponseHead             *mResponseHead;    // owning pointer

    nsAHttpSegmentReader           *mReader;
    nsAHttpSegmentWriter           *mWriter;

    nsCString                       mLineBuf;         // may contain a partial line

    int64_t                         mContentLength;   // equals -1 if unknown
    int64_t                         mContentRead;     // count of consumed content bytes

    // After a 304/204 or other "no-content" style response we will skip over
    // up to MAX_INVALID_RESPONSE_BODY_SZ bytes when looking for the next
    // response header to deal with servers that actually sent a response
    // body where they should not have. This member tracks how many bytes have
    // so far been skipped.
    uint32_t                        mInvalidResponseBytesRead;

    nsHttpChunkedDecoder           *mChunkedDecoder;

    TimingStruct                    mTimings;

    nsresult                        mStatus;

    int16_t                         mPriority;

    uint16_t                        mRestartCount;        // the number of times this transaction has been restarted
    uint32_t                        mCaps;
    // mCapsToClear holds flags that should be cleared in mCaps, e.g. unset
    // NS_HTTP_REFRESH_DNS when DNS refresh request has completed to avoid
    // redundant requests on the network. To deal with raciness, only unsetting
    // bitfields should be allowed: 'lost races' will thus err on the
    // conservative side, e.g. by going ahead with a 2nd DNS refresh.
    uint32_t                        mCapsToClear;
    enum Classifier                 mClassification;
    int32_t                         mPipelinePosition;
    int64_t                         mMaxPipelineObjectSize;

    nsHttpVersion                   mHttpVersion;

    // state flags, all logically boolean, but not packed together into a
    // bitfield so as to avoid bitfield-induced races.  See bug 560579.
    bool                            mClosed;
    bool                            mConnected;
    bool                            mHaveStatusLine;
    bool                            mHaveAllHeaders;
    bool                            mTransactionDone;
    bool                            mResponseIsComplete;
    bool                            mDidContentStart;
    bool                            mNoContent; // expecting an empty entity body
    bool                            mSentData;
    bool                            mReceivedData;
    bool                            mStatusEventPending;
    bool                            mHasRequestBody;
    bool                            mProxyConnectFailed;
    bool                            mHttpResponseMatched;
    bool                            mPreserveStream;
    bool                            mDispatchedAsBlocking;
    bool                            mResponseTimeoutEnabled;
    bool                            mDontRouteViaWildCard;
    bool                            mForceRestart;

    // mClosed           := transaction has been explicitly closed
    // mTransactionDone  := transaction ran to completion or was interrupted
    // mResponseComplete := transaction ran to completion

    // For Restart-In-Progress Functionality
    bool                            mReportedStart;
    bool                            mReportedResponseHeader;

    // protected by nsHttp::GetLock()
    nsHttpResponseHead             *mForTakeResponseHead;
    bool                            mResponseHeadTaken;

    // The time when the transaction was submitted to the Connection Manager
    TimeStamp                       mPendingTime;

    class RestartVerifier
    {

        // When a idemptotent transaction has received part of its response body
        // and incurs an error it can be restarted. To do this we mark the place
        // where we stopped feeding the body to the consumer and start the
        // network call over again. If everything we track (headers, length, etc..)
        // matches up to the place where we left off then the consumer starts being
        // fed data again with the new information. This can be done N times up
        // to the normal restart (i.e. with no response info) limit.

    public:
        RestartVerifier()
            : mContentLength(-1)
            , mAlreadyProcessed(0)
            , mToReadBeforeRestart(0)
            , mSetup(false)
        {}
        ~RestartVerifier() {}

        void Set(int64_t contentLength, nsHttpResponseHead *head);
        bool Verify(int64_t contentLength, nsHttpResponseHead *head);
        bool IsDiscardingContent() { return mToReadBeforeRestart != 0; }
        bool IsSetup() { return mSetup; }
        int64_t AlreadyProcessed() { return mAlreadyProcessed; }
        void SetAlreadyProcessed(int64_t val) {
            mAlreadyProcessed = val;
            mToReadBeforeRestart = val;
        }
        int64_t ToReadBeforeRestart() { return mToReadBeforeRestart; }
        void HaveReadBeforeRestart(uint32_t amt)
        {
            MOZ_ASSERT(amt <= mToReadBeforeRestart,
                       "too large of a HaveReadBeforeRestart deduction");
            mToReadBeforeRestart -= amt;
        }

    private:
        // This is the data from the first complete response header
        // used to make sure that all subsequent response headers match

        int64_t                         mContentLength;
        nsCString                       mETag;
        nsCString                       mLastModified;
        nsCString                       mContentRange;
        nsCString                       mContentEncoding;
        nsCString                       mTransferEncoding;

        // This is the amount of data that has been passed to the channel
        // from previous iterations of the transaction and must therefore
        // be skipped in the new one.
        int64_t                         mAlreadyProcessed;

        // The amount of data that must be discarded in the current iteration
        // (where iteration > 0) to reach the mAlreadyProcessed high water
        // mark.
        int64_t                         mToReadBeforeRestart;

        // true when ::Set has been called with a response header
        bool                            mSetup;
    } mRestartInProgressVerifier;

// For Rate Pacing via an EventTokenBucket
public:
    // called by the connection manager to run this transaction through the
    // token bucket. If the token bucket admits the transaction immediately it
    // returns true. The function is called repeatedly until it returns true.
    bool TryToRunPacedRequest();

    // ATokenBucketEvent pure virtual implementation. Called by the token bucket
    // when the transaction is ready to run. If this happens asynchrounously to
    // token bucket submission the transaction just posts an event that causes
    // the pending transaction queue to be rerun (and TryToRunPacedRequest() to
    // be run again.
    void OnTokenBucketAdmitted(); // ATokenBucketEvent

    // CancelPacing() can be used to tell the token bucket to remove this
    // transaction from the list of pending transactions. This is used when a
    // transaction is believed to be HTTP/1 (and thus subject to rate pacing)
    // but later can be dispatched via spdy (not subject to rate pacing).
    void CancelPacing(nsresult reason);

private:
    bool mSubmittedRatePacing;
    bool mPassedRatePacing;
    bool mSynchronousRatePaceRequest;
    nsCOMPtr<nsICancelable> mTokenBucketCancel;

// These members are used for network per-app metering (bug 746073)
// Currently, they are only available on gonk.
    uint64_t                           mCountRecv;
    uint64_t                           mCountSent;
    uint32_t                           mAppId;
#ifdef MOZ_WIDGET_GONK
    nsMainThreadPtrHandle<nsINetworkInterface> mActiveNetwork;
#endif
    nsresult                           SaveNetworkStats(bool);
    void                               CountRecvBytes(uint64_t recvBytes)
    {
        mCountRecv += recvBytes;
        SaveNetworkStats(false);
    }
    void                               CountSentBytes(uint64_t sentBytes)
    {
        mCountSent += sentBytes;
        SaveNetworkStats(false);
    }
};

}} // namespace mozilla::net

#endif // nsHttpTransaction_h__
