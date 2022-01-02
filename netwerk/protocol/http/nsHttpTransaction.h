/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpTransaction_h__
#define nsHttpTransaction_h__

#include "nsHttp.h"
#include "nsAHttpTransaction.h"
#include "HttpTransactionShell.h"
#include "nsAHttpConnection.h"
#include "EventTokenBucket.h"
#include "nsCOMPtr.h"
#include "nsIAsyncOutputStream.h"
#include "nsThreadUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIAsyncOutputStream.h"
#include "nsITimer.h"
#include "nsIEarlyHintObserver.h"
#include "nsTHashMap.h"
#include "TimingStruct.h"
#include "Http2Push.h"
#include "mozilla/net/DNS.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "ARefBase.h"

//-----------------------------------------------------------------------------

class nsIHttpActivityObserver;
class nsIDNSHTTPSSVCRecord;
class nsIEventTarget;
class nsIInputStream;
class nsIOutputStream;
class nsIRequestContext;
class nsISVCBRecord;

namespace mozilla {
namespace net {

class HTTPSRecordResolver;
class nsHttpChunkedDecoder;
class nsHttpHeaderArray;
class nsHttpRequestHead;
class nsHttpResponseHead;
class NullHttpTransaction;
class SpdyConnectTransaction;

//-----------------------------------------------------------------------------
// nsHttpTransaction represents a single HTTP transaction.  It is thread-safe,
// intended to run on the socket thread.
//-----------------------------------------------------------------------------

class nsHttpTransaction final : public nsAHttpTransaction,
                                public HttpTransactionShell,
                                public ATokenBucketEvent,
                                public nsIInputStreamCallback,
                                public nsIOutputStreamCallback,
                                public ARefBase,
                                public nsITimerCallback,
                                public nsINamed {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSAHTTPTRANSACTION
  NS_DECL_HTTPTRANSACTIONSHELL
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  nsHttpTransaction();

  void OnActivated() override;

  // attributes
  nsHttpResponseHead* ResponseHead() {
    return mHaveAllHeaders ? mResponseHead : nullptr;
  }

  nsIEventTarget* ConsumerTarget() { return mConsumerTarget; }

  // Called to set/find out if the transaction generated a complete response.
  void SetResponseIsComplete() { mResponseIsComplete = true; }

  void EnableKeepAlive() { mCaps |= NS_HTTP_ALLOW_KEEPALIVE; }
  void MakeSticky() { mCaps |= NS_HTTP_STICKY_CONNECTION; }
  void MakeNonSticky() override { mCaps &= ~NS_HTTP_STICKY_CONNECTION; }

  void MakeDontWaitHTTPSRR() { mCaps &= ~NS_HTTP_FORCE_WAIT_HTTP_RR; }

  // SetPriority() may only be used by the connection manager.
  void SetPriority(int32_t priority) { mPriority = priority; }
  int32_t Priority() { return mPriority; }

  void PrintDiagnostics(nsCString& log);

  // Sets mPendingTime to the current time stamp or to a null time stamp (if now
  // is false)
  void SetPendingTime(bool now = true) {
    if (!now && !mPendingTime.IsNull()) {
      // Remember how long it took. We will use this vaule to record
      // TRANSACTION_WAIT_TIME_HTTP2_SUP_HTTP3 telemetry, but we need to wait
      // for the response headers.
      mPendingDurationTime = TimeStamp::Now() - mPendingTime;
    }
    // Note that the transaction could be added in to a pending queue multiple
    // times (when the transaction is restarted or moved to a new conn entry due
    // to HTTPS RR), so we should only set the pending time once.
    if (mPendingTime.IsNull()) {
      mPendingTime = now ? TimeStamp::Now() : TimeStamp();
    }
  }
  TimeStamp GetPendingTime() { return mPendingTime; }

  // overload of nsAHttpTransaction::RequestContext()
  nsIRequestContext* RequestContext() override { return mRequestContext.get(); }
  void DispatchedAsBlocking();
  void RemoveDispatchedAsBlocking();

  void DisableSpdy() override;
  void DoNotRemoveAltSvc() override { mDoNotRemoveAltSvc = true; }
  void DisableHttp3(bool aAllowRetryHTTPSRR) override;

  nsHttpTransaction* QueryHttpTransaction() override { return this; }

  already_AddRefed<Http2PushedStreamWrapper> GetPushedStream() {
    return do_AddRef(mPushedStream);
  }
  already_AddRefed<Http2PushedStreamWrapper> TakePushedStream() {
    return mPushedStream.forget();
  }

  uint32_t InitialRwin() const { return mInitialRwin; };
  bool ChannelPipeFull() { return mWaitingOnPipeOut; }

  // Locked methods to get and set timing info
  void BootstrapTimings(TimingStruct times);
  void SetConnectStart(mozilla::TimeStamp timeStamp, bool onlyIfNull = false);
  void SetConnectEnd(mozilla::TimeStamp timeStamp, bool onlyIfNull = false);
  void SetRequestStart(mozilla::TimeStamp timeStamp, bool onlyIfNull = false);
  void SetResponseStart(mozilla::TimeStamp timeStamp, bool onlyIfNull = false);
  void SetResponseEnd(mozilla::TimeStamp timeStamp, bool onlyIfNull = false);

  [[nodiscard]] bool Do0RTT() override;
  [[nodiscard]] nsresult Finish0RTT(bool aRestart,
                                    bool aAlpnChanged /* ignored */) override;

  // After Finish0RTT early data may have failed but the caller did not request
  // restart - this indicates that state for dev tools
  void Refused0RTT();

  uint64_t TopBrowsingContextId() override { return mTopBrowsingContextId; }

  void SetHttpTrailers(nsCString& aTrailers);

  bool IsWebsocketUpgrade();
  void SetH2WSTransaction(SpdyConnectTransaction*);

  void OnProxyConnectComplete(int32_t aResponseCode) override;
  void SetFlat407Headers(const nsACString& aHeaders);

  // This is only called by Http2PushedStream::TryOnPush when a new pushed
  // stream is available. The newly added stream will be taken by another
  // transaction.
  void OnPush(Http2PushedStreamWrapper* aStream);

  void UpdateConnectionInfo(nsHttpConnectionInfo* aConnInfo);

  void SetClassOfService(uint32_t cos);

  virtual nsresult OnHTTPSRRAvailable(
      nsIDNSHTTPSSVCRecord* aHTTPSSVCRecord,
      nsISVCBRecord* aHighestPriorityRecord) override;

  void GetHashKeyOfConnectionEntry(nsACString& aResult);

 private:
  friend class DeleteHttpTransaction;
  virtual ~nsHttpTransaction();

  [[nodiscard]] nsresult Restart();
  char* LocateHttpStart(char* buf, uint32_t len, bool aAllowPartialMatch);
  [[nodiscard]] nsresult ParseLine(nsACString& line);
  [[nodiscard]] nsresult ParseLineSegment(char* seg, uint32_t len);
  [[nodiscard]] nsresult ParseHead(char*, uint32_t count, uint32_t* countRead);
  [[nodiscard]] nsresult HandleContentStart();
  [[nodiscard]] nsresult HandleContent(char*, uint32_t count,
                                       uint32_t* contentRead,
                                       uint32_t* contentRemaining);
  [[nodiscard]] nsresult ProcessData(char*, uint32_t, uint32_t*);
  void DeleteSelfOnConsumerThread();
  void ReleaseBlockingTransaction();

  [[nodiscard]] static nsresult ReadRequestSegment(nsIInputStream*, void*,
                                                   const char*, uint32_t,
                                                   uint32_t, uint32_t*);
  [[nodiscard]] static nsresult WritePipeSegment(nsIOutputStream*, void*, char*,
                                                 uint32_t, uint32_t, uint32_t*);

  bool TimingEnabled() const { return mCaps & NS_HTTP_TIMING_ENABLED; }

  bool ResponseTimeoutEnabled() const final;

  void ReuseConnectionOnRestartOK(bool reuseOk) override {
    mReuseOnRestart = reuseOk;
  }

  // Called right after we parsed the response head.  Checks for connection
  // based authentication schemes in reponse headers for WWW and Proxy
  // authentication. If such is found in any of them, NS_HTTP_STICKY_CONNECTION
  // is set in mCaps. We need the sticky flag be set early to keep the
  // connection from very start of the authentication process.
  void CheckForStickyAuthScheme();
  void CheckForStickyAuthSchemeAt(nsHttpAtom const& header);
  bool IsStickyAuthSchemeAt(nsACString const& auth);

  // Called from WriteSegments.  Checks for conditions whether to throttle
  // reading the content.  When this returns true, WriteSegments returns
  // WOULD_BLOCK.
  bool ShouldThrottle();

  void NotifyTransactionObserver(nsresult reason);

  // When echConfig is enabled, this function put other available records
  // in mRecordsForRetry. Returns true when mRecordsForRetry is not empty,
  // otherwise returns false.
  bool PrepareSVCBRecordsForRetry(const nsACString& aFailedDomainName,
                                  bool& aAllRecordsHaveEchConfig);
  // This function setups a new connection info for restarting this transaction.
  void PrepareConnInfoForRetry(nsresult aReason);
  // This function is used to select the next non http3 record and is only
  // executed when the fast fallback timer is triggered.
  already_AddRefed<nsHttpConnectionInfo> PrepareFastFallbackConnInfo(
      bool aEchConfigUsed);

  void MaybeReportFailedSVCDomain(nsresult aReason,
                                  nsHttpConnectionInfo* aFailedConnInfo);

  already_AddRefed<Http2PushedStreamWrapper> TakePushedStreamById(
      uint32_t aStreamId);

  // IMPORTANT: when adding new values, always add them to the end, otherwise
  // it will mess up telemetry.
  enum HTTPSSVC_CONNECTION_FAILED_REASON : uint32_t {
    HTTPSSVC_CONNECTION_OK = 0,
    HTTPSSVC_CONNECTION_UNKNOWN_HOST = 1,
    HTTPSSVC_CONNECTION_UNREACHABLE = 2,
    HTTPSSVC_CONNECTION_421_RECEIVED = 3,
    HTTPSSVC_CONNECTION_SECURITY_ERROR = 4,
    HTTPSSVC_CONNECTION_NO_USABLE_RECORD = 5,
    HTTPSSVC_CONNECTION_ALL_RECORDS_EXCLUDED = 6,
    HTTPSSVC_CONNECTION_OTHERS = 7,
  };
  HTTPSSVC_CONNECTION_FAILED_REASON ErrorCodeToFailedReason(
      nsresult aErrorCode);

  void OnHttp3BackupTimer();
  void OnBackupConnectionReady(bool aTriggeredByHTTPSRR);
  void OnFastFallbackTimer();
  void HandleFallback(nsHttpConnectionInfo* aFallbackConnInfo);
  void MaybeCancelFallbackTimer();

  // IMPORTANT: when adding new values, always add them to the end, otherwise
  // it will mess up telemetry.
  enum TRANSACTION_RESTART_REASON : uint32_t {
    TRANSACTION_RESTART_NONE = 0,  // The transacion was not restarted.
    TRANSACTION_RESTART_FORCED,    // The transaction was forced to restart.
    TRANSACTION_RESTART_NO_DATA_SENT,
    TRANSACTION_RESTART_DOWNGRADE_WITH_EARLY_DATA,
    TRANSACTION_RESTART_HTTPS_RR_NET_RESET,
    TRANSACTION_RESTART_HTTPS_RR_CONNECTION_REFUSED,
    TRANSACTION_RESTART_HTTPS_RR_UNKNOWN_HOST,
    TRANSACTION_RESTART_HTTPS_RR_NET_TIMEOUT,
    TRANSACTION_RESTART_HTTPS_RR_SEC_ERROR,
    TRANSACTION_RESTART_HTTPS_RR_FAST_FALLBACK,
    TRANSACTION_RESTART_HTTP3_FAST_FALLBACK,
    TRANSACTION_RESTART_OTHERS,
    TRANSACTION_RESTART_PROTOCOL_VERSION_ALERT,
  };
  void SetRestartReason(TRANSACTION_RESTART_REASON aReason);

 private:
  class UpdateSecurityCallbacks : public Runnable {
   public:
    UpdateSecurityCallbacks(nsHttpTransaction* aTrans,
                            nsIInterfaceRequestor* aCallbacks)
        : Runnable("net::nsHttpTransaction::UpdateSecurityCallbacks"),
          mTrans(aTrans),
          mCallbacks(aCallbacks) {}

    NS_IMETHOD Run() override {
      if (mTrans->mConnection) {
        mTrans->mConnection->SetSecurityCallbacks(mCallbacks);
      }
      return NS_OK;
    }

   private:
    RefPtr<nsHttpTransaction> mTrans;
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  };

  Mutex mLock{"transaction lock"};

  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsITransportEventSink> mTransportSink;
  nsCOMPtr<nsIEventTarget> mConsumerTarget;
  nsCOMPtr<nsISupports> mSecurityInfo;
  nsCOMPtr<nsIAsyncInputStream> mPipeIn;
  nsCOMPtr<nsIAsyncOutputStream> mPipeOut;
  nsCOMPtr<nsIRequestContext> mRequestContext;

  uint64_t mChannelId{0};
  nsCOMPtr<nsIHttpActivityObserver> mActivityDistributor;

  nsCString mReqHeaderBuf;  // flattened request headers
  nsCOMPtr<nsIInputStream> mRequestStream;
  int64_t mRequestSize{0};

  RefPtr<nsAHttpConnection> mConnection;
  RefPtr<nsHttpConnectionInfo> mConnInfo;
  // This is only set in UpdateConnectionInfo() when we have received a SVCB RR.
  // When echConfig is not used and the connection is failed, this transaction
  // will be restarted with this origin connection info directly.
  // When echConfig is enabled, there are two cases below.
  // 1. If all records have echConfig, we will retry other records except the
  // failed one. In the case all other records with echConfig are failed and the
  // pref network.dns.echconfig.fallback_to_origin_when_all_failed is true, this
  // origin connection info will be used.
  // 2. If only some records have echConfig and some not, we always fallback to
  // this origin conn info.
  RefPtr<nsHttpConnectionInfo> mOrigConnInfo;
  nsHttpRequestHead* mRequestHead{nullptr};    // weak ref
  nsHttpResponseHead* mResponseHead{nullptr};  // owning pointer

  nsAHttpSegmentReader* mReader{nullptr};
  nsAHttpSegmentWriter* mWriter{nullptr};

  nsCString mLineBuf;  // may contain a partial line

  int64_t mContentLength{-1};  // equals -1 if unknown
  int64_t mContentRead{0};     // count of consumed content bytes
  Atomic<int64_t, ReleaseAcquire> mTransferSize{0};  // count of received bytes

  // After a 304/204 or other "no-content" style response we will skip over
  // up to MAX_INVALID_RESPONSE_BODY_SZ bytes when looking for the next
  // response header to deal with servers that actually sent a response
  // body where they should not have. This member tracks how many bytes have
  // so far been skipped.
  uint32_t mInvalidResponseBytesRead{0};

  RefPtr<Http2PushedStreamWrapper> mPushedStream;
  uint32_t mInitialRwin{0};

  nsHttpChunkedDecoder* mChunkedDecoder{nullptr};

  TimingStruct mTimings;

  nsresult mStatus{NS_OK};

  int16_t mPriority{0};

  // the number of times this transaction has been restarted
  uint16_t mRestartCount{0};
  uint32_t mCaps{0};

  HttpVersion mHttpVersion{HttpVersion::UNKNOWN};
  uint16_t mHttpResponseCode{0};
  nsCString mFlat407Headers;

  uint32_t mCurrentHttpResponseHeaderSize{0};

  int32_t const THROTTLE_NO_LIMIT = -1;
  // This can have 3 possible values:
  // * THROTTLE_NO_LIMIT - this means the transaction is not in any way limited
  //                       to read the response, this is the default
  // * a positive number - a limit is set because the transaction is obligated
  //                       to throttle the response read, this is decresed with
  //                       every piece of data the transaction receives
  // * zero - when the transaction depletes the limit for reading, this makes it
  //          stop reading and return WOULD_BLOCK from WriteSegments;
  //          transaction then waits for a call of ResumeReading that resets
  //          this member back to THROTTLE_NO_LIMIT
  int32_t mThrottlingReadAllowance{THROTTLE_NO_LIMIT};

  // mCapsToClear holds flags that should be cleared in mCaps, e.g. unset
  // NS_HTTP_REFRESH_DNS when DNS refresh request has completed to avoid
  // redundant requests on the network. The member itself is atomic, but
  // access to it from the networking thread may happen either before or
  // after the main thread modifies it. To deal with raciness, only unsetting
  // bitfields should be allowed: 'lost races' will thus err on the
  // conservative side, e.g. by going ahead with a 2nd DNS refresh.
  Atomic<uint32_t> mCapsToClear{0};
  Atomic<bool, ReleaseAcquire> mResponseIsComplete{false};
  Atomic<bool, ReleaseAcquire> mClosed{false};
  Atomic<bool, Relaxed> mIsHttp3Used{false};

  // True iff WriteSegments was called while this transaction should be
  // throttled (stop reading) Used to resume read on unblock of reading.  Conn
  // manager is responsible for calling back to resume reading.
  bool mReadingStopped{false};

  // state flags, all logically boolean, but not packed together into a
  // bitfield so as to avoid bitfield-induced races.  See bug 560579.
  bool mConnected{false};
  bool mActivated{false};
  bool mHaveStatusLine{false};
  bool mHaveAllHeaders{false};
  bool mTransactionDone{false};
  bool mDidContentStart{false};
  bool mNoContent{false};  // expecting an empty entity body
  bool mSentData{false};
  bool mReceivedData{false};
  bool mStatusEventPending{false};
  bool mHasRequestBody{false};
  bool mProxyConnectFailed{false};
  bool mHttpResponseMatched{false};
  bool mPreserveStream{false};
  bool mDispatchedAsBlocking{false};
  bool mResponseTimeoutEnabled{true};
  bool mForceRestart{false};
  bool mReuseOnRestart{false};
  bool mContentDecoding{false};
  bool mContentDecodingCheck{false};
  bool mDeferredSendProgress{false};
  bool mWaitingOnPipeOut{false};
  bool mDoNotRemoveAltSvc{false};

  // mClosed           := transaction has been explicitly closed
  // mTransactionDone  := transaction ran to completion or was interrupted
  // mResponseComplete := transaction ran to completion

  // For Restart-In-Progress Functionality
  bool mReportedStart{false};
  bool mReportedResponseHeader{false};

  // protected by nsHttp::GetLock()
  bool mResponseHeadTaken{false};
  UniquePtr<nsHttpHeaderArray> mForTakeResponseTrailers;
  bool mResponseTrailersTaken{false};

  // Set when this transaction was restarted by call to Restart().  Used to tell
  // the http channel to reset proxy authentication.
  Atomic<bool> mRestarted{false};

  // The time when the transaction was submitted to the Connection Manager
  TimeStamp mPendingTime;
  TimeDuration mPendingDurationTime;

  uint64_t mTopBrowsingContextId{0};

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
  void OnTokenBucketAdmitted() override;  // ATokenBucketEvent

  // CancelPacing() can be used to tell the token bucket to remove this
  // transaction from the list of pending transactions. This is used when a
  // transaction is believed to be HTTP/1 (and thus subject to rate pacing)
  // but later can be dispatched via spdy (not subject to rate pacing).
  void CancelPacing(nsresult reason);

  // Called by the connetion manager on the socket thread when reading for this
  // previously throttled transaction has to be resumed.
  void ResumeReading();

  // This examins classification of this transaction whether the Throttleable
  // class has been set while Leader, Unblocked, DontThrottle has not.
  bool EligibleForThrottling() const;

 private:
  bool mSubmittedRatePacing{false};
  bool mPassedRatePacing{false};
  bool mSynchronousRatePaceRequest{false};
  nsCOMPtr<nsICancelable> mTokenBucketCancel;

  void CollectTelemetryForUploads();

 public:
  uint32_t ClassOfService() { return mClassOfService; }

 private:
  Atomic<uint32_t, Relaxed> mClassOfService{0};

 public:
  // setting TunnelProvider to non-null means the transaction should only
  // be dispatched on a specific ConnectionInfo Hash Key (as opposed to a
  // generic wild card one). That means in the specific case of carrying this
  // transaction on an HTTP/2 tunnel it will only be dispatched onto an
  // existing tunnel instead of triggering creation of a new one.
  // The tunnel provider is used for ASpdySession::MaybeReTunnel() checks.

  void SetTunnelProvider(ASpdySession* provider) { mTunnelProvider = provider; }
  ASpdySession* TunnelProvider() { return mTunnelProvider; }
  nsIInterfaceRequestor* SecurityCallbacks() { return mCallbacks; }
  // Called when this transaction is inserted in the pending queue.
  void OnPendingQueueInserted(const nsACString& aConnectionHashKey);

 private:
  RefPtr<ASpdySession> mTunnelProvider;
  TransactionObserverFunc mTransactionObserver;
  NetAddr mSelfAddr;
  NetAddr mPeerAddr;
  bool mResolvedByTRR{false};
  bool mEchConfigUsed = false;

  bool m0RTTInProgress{false};
  bool mDoNotTryEarlyData{false};
  enum {
    EARLY_NONE,
    EARLY_SENT,
    EARLY_ACCEPTED,
    EARLY_425
  } mEarlyDataDisposition{EARLY_NONE};

  // H2 websocket support
  RefPtr<SpdyConnectTransaction> mH2WSTransaction;

  HttpTrafficCategory mTrafficCategory{HttpTrafficCategory::eInvalid};
  bool mThroughCaptivePortal;
  Atomic<int32_t> mProxyConnectResponseCode{0};

  OnPushCallback mOnPushCallback;
  nsTHashMap<uint32_t, RefPtr<Http2PushedStreamWrapper>> mIDToStreamMap;

  nsCOMPtr<nsICancelable> mDNSRequest;
  Atomic<uint32_t, Relaxed> mHTTPSSVCReceivedStage{HTTPSSVC_NOT_USED};
  bool m421Received = false;
  nsCOMPtr<nsIDNSHTTPSSVCRecord> mHTTPSSVCRecord;
  nsTArray<RefPtr<nsISVCBRecord>> mRecordsForRetry;
  bool mDontRetryWithDirectRoute = false;
  bool mFastFallbackTriggered = false;
  bool mAllRecordsInH3ExcludedListBefore = false;
  bool mHttp3BackupTimerCreated = false;
  nsCOMPtr<nsITimer> mFastFallbackTimer;
  nsCOMPtr<nsITimer> mHttp3BackupTimer;
  RefPtr<nsHttpConnectionInfo> mBackupConnInfo;
  RefPtr<HTTPSRecordResolver> mResolver;
  TRANSACTION_RESTART_REASON mRestartReason = TRANSACTION_RESTART_NONE;

  nsTHashMap<nsUint32HashKey, uint32_t> mEchRetryCounterMap;

  bool mSupportsHTTP3 = false;

  bool mEarlyDataWasAvailable = false;
  bool ShouldRestartOn0RttError(nsresult reason);

  nsCOMPtr<nsIEarlyHintObserver> mEarlyHintObserver;
  // This hash key is set when a transaction is inserted into the connection
  // entry's pending queue.
  // See nsHttpConnectionMgr::GetOrCreateConnectionEntry(). A transaction could
  // be associated with the connection entry whose hash key is not the same as
  // this transaction's.
  nsCString mHashKeyOfConnectionEntry;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpTransaction_h__
