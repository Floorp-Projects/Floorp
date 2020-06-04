/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpTransactionParent_h__
#define HttpTransactionParent_h__

#include "mozilla/net/HttpTransactionShell.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/net/PHttpTransactionParent.h"
#include "nsHttp.h"
#include "nsCOMPtr.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsITransport.h"
#include "nsIRequest.h"

namespace mozilla {
namespace net {

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
  NS_DECL_ISUPPORTS
  NS_DECL_HTTPTRANSACTIONSHELL
  NS_DECL_NSIREQUEST
  NS_DECL_NSITHREADRETARGETABLEREQUEST
  NS_DECLARE_STATIC_IID_ACCESSOR(HTTP_TRANSACTION_PARENT_IID)

  explicit HttpTransactionParent(bool aIsDocumentLoad);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvOnStartRequest(
      const nsresult& aStatus, const Maybe<nsHttpResponseHead>& aResponseHead,
      const nsCString& aSecurityInfoSerialization,
      const bool& aProxyConnectFailed, const TimingStructArgs& aTimings,
      const int32_t& aProxyConnectResponseCode,
      nsTArray<uint8_t>&& aDataForSniffer);
  mozilla::ipc::IPCResult RecvOnTransportStatus(const nsresult& aStatus,
                                                const int64_t& aProgress,
                                                const int64_t& aProgressMax);
  mozilla::ipc::IPCResult RecvOnDataAvailable(
      const nsCString& aData, const uint64_t& aOffset, const uint32_t& aCount,
      const bool& aDataSentToChildProcess);
  mozilla::ipc::IPCResult RecvOnStopRequest(
      const nsresult& aStatus, const bool& aResponseIsComplete,
      const int64_t& aTransferSize, const TimingStructArgs& aTimings,
      const Maybe<nsHttpHeaderArray>& responseTrailers,
      const bool& aHasStickyConn,
      Maybe<TransactionObserverResult>&& aTransactionObserverResult);
  mozilla::ipc::IPCResult RecvOnNetAddrUpdate(const NetAddr& aSelfAddr,
                                              const NetAddr& aPeerAddr,
                                              const bool& aResolvedByTRR);
  mozilla::ipc::IPCResult RecvOnInitFailed(const nsresult& aStatus);

  mozilla::ipc::IPCResult RecvOnH2PushStream(const uint32_t& aPushedStreamId,
                                             const nsCString& aResourceUrl,
                                             const nsCString& aRequestString);

  already_AddRefed<nsIEventTarget> GetNeckoTarget();

  void SetSniffedTypeToChannel(
      nsInputStreamPump::PeekSegmentFun aCallTypeSniffers,
      nsIChannel* aChannel);

 private:
  virtual ~HttpTransactionParent();

  void GetStructFromInfo(nsHttpConnectionInfo* aInfo,
                         HttpConnectionInfoCloneArgs& aArgs);
  void DoOnStartRequest(const nsresult& aStatus,
                        const Maybe<nsHttpResponseHead>& aResponseHead,
                        const nsCString& aSecurityInfoSerialization,
                        const bool& aProxyConnectFailed,
                        const TimingStructArgs& aTimings,
                        const int32_t& aProxyConnectResponseCode,
                        nsTArray<uint8_t>&& aDataForSniffer);
  void DoOnTransportStatus(const nsresult& aStatus, const int64_t& aProgress,
                           const int64_t& aProgressMax);
  void DoOnDataAvailable(const nsCString& aData, const uint64_t& aOffset,
                         const uint32_t& aCount,
                         const bool& aDataSentToChildProcess);
  void DoOnStopRequest(
      const nsresult& aStatus, const bool& aResponseIsComplete,
      const int64_t& aTransferSize, const TimingStructArgs& aTimings,
      const Maybe<nsHttpHeaderArray>& responseTrailers,
      const bool& aHasStickyConn,
      Maybe<TransactionObserverResult>&& aTransactionObserverResult);
  void DoNotifyListener();

  nsCOMPtr<nsITransportEventSink> mEventsink;
  nsCOMPtr<nsIStreamListener> mChannel;
  nsCOMPtr<nsIEventTarget> mTargetThread;
  nsCOMPtr<nsISupports> mSecurityInfo;
  UniquePtr<nsHttpResponseHead> mResponseHead;
  UniquePtr<nsHttpHeaderArray> mResponseTrailers;
  RefPtr<ChannelEventQueue> mEventQ;

  bool mResponseIsComplete;
  int64_t mTransferSize;
  int64_t mRequestSize;
  bool mProxyConnectFailed;
  bool mCanceled;
  nsresult mStatus;
  int32_t mSuspendCount;
  bool mResponseHeadTaken;
  bool mResponseTrailersTaken;
  bool mHasStickyConnection;
  bool mOnStartRequestCalled;
  bool mOnStopRequestCalled;
  bool mResolvedByTRR;
  int32_t mProxyConnectResponseCode;
  uint64_t mChannelId;
  bool mDataAlreadySent;
  bool mIsDocumentLoad;

  NetAddr mSelfAddr;
  NetAddr mPeerAddr;
  TimingStruct mTimings;
  TimeStamp mDomainLookupStart;
  TimeStamp mDomainLookupEnd;
  TransactionObserverFunc mTransactionObserver;
  OnPushCallback mOnPushCallback;
  nsTArray<uint8_t> mDataForSniffer;
};

NS_DEFINE_STATIC_IID_ACCESSOR(HttpTransactionParent,
                              HTTP_TRANSACTION_PARENT_IID)

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpTransactionParent_h__
