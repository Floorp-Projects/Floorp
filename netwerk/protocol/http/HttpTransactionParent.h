/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpTransactionParent_h__
#define HttpTransactionParent_h__

#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"
#include "mozilla/net/HttpTransactionShell.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/net/PHttpTransactionParent.h"
#include "nsCOMPtr.h"
#include "nsHttp.h"
#include "nsIRequest.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsITransport.h"
#include "nsITransportSecurityInfo.h"

namespace mozilla::net {

class ChannelEventQueue;
class nsHttpConnectionInfo;

#define HTTP_TRANSACTION_PARENT_IID                  \
  {                                                  \
    0xb83695cb, 0xc24b, 0x4c53, {                    \
      0x85, 0x9b, 0x77, 0x77, 0x3e, 0xc5, 0x44, 0xe5 \
    }                                                \
  }

// HttpTransactionParent plays the role of nsHttpTransaction and delegates the
// work to the nsHttpTransaction in socket process.
class HttpTransactionParent final : public PHttpTransactionParent,
                                    public HttpTransactionShell,
                                    public nsIRequest,
                                    public nsIThreadRetargetableRequest {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_HTTPTRANSACTIONSHELL
  NS_DECL_NSIREQUEST
  NS_DECL_NSITHREADRETARGETABLEREQUEST
  NS_DECLARE_STATIC_IID_ACCESSOR(HTTP_TRANSACTION_PARENT_IID)

  explicit HttpTransactionParent(bool aIsDocumentLoad);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvOnStartRequest(
      const nsresult& aStatus, const Maybe<nsHttpResponseHead>& aResponseHead,
      nsITransportSecurityInfo* aSecurityInfo, const bool& aProxyConnectFailed,
      const TimingStructArgs& aTimings,
      const int32_t& aProxyConnectResponseCode,
      nsTArray<uint8_t>&& aDataForSniffer, const Maybe<nsCString>& aAltSvcUsed,
      const bool& aDataToChildProcess, const bool& aRestarted,
      const uint32_t& aHTTPSSVCReceivedStage, const bool& aSupportsHttp3,
      const nsIRequest::TRRMode& aMode, const TRRSkippedReason& aSkipReason,
      const uint32_t& aCaps, const TimeStamp& aOnStartRequestStartTime);
  mozilla::ipc::IPCResult RecvOnTransportStatus(
      const nsresult& aStatus, const int64_t& aProgress,
      const int64_t& aProgressMax,
      Maybe<NetworkAddressArg>&& aNetworkAddressArg);
  mozilla::ipc::IPCResult RecvOnDataAvailable(
      const nsCString& aData, const uint64_t& aOffset, const uint32_t& aCount,
      const TimeStamp& aOnDataAvailableStartTime);
  mozilla::ipc::IPCResult RecvOnStopRequest(
      const nsresult& aStatus, const bool& aResponseIsComplete,
      const int64_t& aTransferSize, const TimingStructArgs& aTimings,
      const Maybe<nsHttpHeaderArray>& responseTrailers,
      Maybe<TransactionObserverResult>&& aTransactionObserverResult,
      const TimeStamp& aLastActiveTabOptHit,
      const HttpConnectionInfoCloneArgs& aArgs,
      const TimeStamp& aOnStopRequestStartTime);
  mozilla::ipc::IPCResult RecvOnInitFailed(const nsresult& aStatus);

  mozilla::ipc::IPCResult RecvOnH2PushStream(const uint32_t& aPushedStreamId,
                                             const nsCString& aResourceUrl,
                                             const nsCString& aRequestString);
  mozilla::ipc::IPCResult RecvEarlyHint(const nsCString& aValue,
                                        const nsACString& aReferrerPolicy,
                                        const nsACString& aCSPHeader);

  virtual mozilla::TimeStamp GetPendingTime() override;

  already_AddRefed<nsIEventTarget> GetNeckoTarget();

  void SetSniffedTypeToChannel(
      nsInputStreamPump::PeekSegmentFun aCallTypeSniffers,
      nsIChannel* aChannel);

  void SetRedirectTimestamp(TimeStamp aRedirectStart, TimeStamp aRedirectEnd) {
    mRedirectStart = aRedirectStart;
    mRedirectEnd = aRedirectEnd;
  }

  virtual TimeStamp GetOnStartRequestStartTime() const override {
    return mOnStartRequestStartTime;
  }
  virtual TimeStamp GetDataAvailableStartTime() const override {
    return mOnDataAvailableStartTime;
  }
  virtual TimeStamp GetOnStopRequestStartTime() const override {
    return mOnStopRequestStartTime;
  }

 private:
  virtual ~HttpTransactionParent();

  void GetStructFromInfo(nsHttpConnectionInfo* aInfo,
                         HttpConnectionInfoCloneArgs& aArgs);
  void DoOnStartRequest(
      const nsresult& aStatus, const Maybe<nsHttpResponseHead>& aResponseHead,
      nsITransportSecurityInfo* aSecurityInfo, const bool& aProxyConnectFailed,
      const TimingStructArgs& aTimings,
      const int32_t& aProxyConnectResponseCode,
      nsTArray<uint8_t>&& aDataForSniffer, const Maybe<nsCString>& aAltSvcUsed,
      const bool& aDataToChildProcess, const bool& aRestarted,
      const uint32_t& aHTTPSSVCReceivedStage, const bool& aSupportsHttp3,
      const nsIRequest::TRRMode& aMode, const TRRSkippedReason& aSkipReason,
      const uint32_t& aCaps, const TimeStamp& aOnStartRequestStartTime);
  void DoOnDataAvailable(const nsCString& aData, const uint64_t& aOffset,
                         const uint32_t& aCount,
                         const TimeStamp& aOnDataAvailableStartTime);
  void DoOnStopRequest(
      const nsresult& aStatus, const bool& aResponseIsComplete,
      const int64_t& aTransferSize, const TimingStructArgs& aTimings,
      const Maybe<nsHttpHeaderArray>& responseTrailers,
      Maybe<TransactionObserverResult>&& aTransactionObserverResult,
      nsHttpConnectionInfo* aConnInfo,
      const TimeStamp& aOnStopRequestStartTime);
  void DoNotifyListener();
  void ContinueDoNotifyListener();
  // Get event target for ODA.
  already_AddRefed<nsISerialEventTarget> GetODATarget();
  void CancelOnMainThread(nsresult aRv);
  void HandleAsyncAbort();

  nsCOMPtr<nsITransportEventSink> mEventsink;
  nsCOMPtr<nsIStreamListener> mChannel;
  nsCOMPtr<nsISerialEventTarget> mTargetThread;
  nsCOMPtr<nsISerialEventTarget> mODATarget;
  Mutex mEventTargetMutex MOZ_UNANNOTATED{
      "HttpTransactionParent::EventTargetMutex"};
  nsCOMPtr<nsITransportSecurityInfo> mSecurityInfo;
  UniquePtr<nsHttpResponseHead> mResponseHead;
  UniquePtr<nsHttpHeaderArray> mResponseTrailers;
  RefPtr<ChannelEventQueue> mEventQ;

  bool mResponseIsComplete{false};
  int64_t mTransferSize{0};
  int64_t mRequestSize{0};
  bool mIsHttp3Used = false;
  bool mProxyConnectFailed{false};
  Atomic<bool, ReleaseAcquire> mCanceled{false};
  Atomic<nsresult, ReleaseAcquire> mStatus{NS_OK};
  int32_t mSuspendCount{0};
  bool mResponseHeadTaken{false};
  bool mResponseTrailersTaken{false};
  bool mOnStartRequestCalled{false};
  bool mOnStopRequestCalled{false};
  bool mResolvedByTRR{false};
  nsIRequest::TRRMode mEffectiveTRRMode{nsIRequest::TRR_DEFAULT_MODE};
  TRRSkippedReason mTRRSkipReason{nsITRRSkipReason::TRR_UNSET};
  bool mEchConfigUsed = false;
  int32_t mProxyConnectResponseCode{0};
  uint64_t mChannelId{0};
  bool mDataSentToChildProcess{false};
  bool mIsDocumentLoad;
  bool mRestarted{false};
  Atomic<uint32_t, ReleaseAcquire> mCaps{0};
  TimeStamp mRedirectStart;
  TimeStamp mRedirectEnd;

  NetAddr mSelfAddr;
  NetAddr mPeerAddr;
  TimingStruct mTimings;
  TimeStamp mDomainLookupStart;
  TimeStamp mDomainLookupEnd;
  TimeStamp mOnStartRequestStartTime;
  TimeStamp mOnDataAvailableStartTime;
  TimeStamp mOnStopRequestStartTime;
  TransactionObserverFunc mTransactionObserver;
  OnPushCallback mOnPushCallback;
  nsTArray<uint8_t> mDataForSniffer;
  std::function<void()> mCallOnResume;
  uint32_t mHTTPSSVCReceivedStage{};
  RefPtr<nsHttpConnectionInfo> mConnInfo;
  bool mSupportsHTTP3 = false;
};

NS_DEFINE_STATIC_IID_ACCESSOR(HttpTransactionParent,
                              HTTP_TRANSACTION_PARENT_IID)

}  // namespace mozilla::net

#endif  // nsHttpTransactionParent_h__
