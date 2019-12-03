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

  explicit HttpTransactionParent();

  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvOnStartRequest(
      const nsresult& aStatus, const Maybe<nsHttpResponseHead>& aResponseHead,
      const nsCString& aSecurityInfoSerialization,
      const bool& aProxyConnectFailed, const TimingStructArgs& aTimings);
  mozilla::ipc::IPCResult RecvOnTransportStatus(const nsresult& aStatus,
                                                const int64_t& aProgress,
                                                const int64_t& aProgressMax);
  mozilla::ipc::IPCResult RecvOnDataAvailable(const nsCString& aData,
                                              const uint64_t& aOffset,
                                              const uint32_t& aCount);
  mozilla::ipc::IPCResult RecvOnStopRequest(
      const nsresult& aStatus, const bool& aResponseIsComplete,
      const int64_t& aTransferSize, const TimingStructArgs& aTimings,
      const Maybe<nsHttpHeaderArray>& responseTrailers,
      const bool& aHasStickyConn);
  mozilla::ipc::IPCResult RecvOnNetAddrUpdate(const NetAddr& aSelfAddr,
                                              const NetAddr& aPeerAddr);
  mozilla::ipc::IPCResult RecvOnInitFailed(const nsresult& aStatus);

  already_AddRefed<nsIEventTarget> GetNeckoTarget();

 private:
  virtual ~HttpTransactionParent();

  void GetStructFromInfo(nsHttpConnectionInfo* aInfo,
                         HttpConnectionInfoCloneArgs& aArgs);
  void DoOnStartRequest(const nsresult& aStatus,
                        const Maybe<nsHttpResponseHead>& aResponseHead,
                        const nsCString& aSecurityInfoSerialization,
                        const bool& aProxyConnectFailed,
                        const TimingStructArgs& aTimings);
  void DoOnTransportStatus(const nsresult& aStatus, const int64_t& aProgress,
                           const int64_t& aProgressMax);
  void DoOnDataAvailable(const nsCString& aData, const uint64_t& aOffset,
                         const uint32_t& aCount);
  void DoOnStopRequest(const nsresult& aStatus, const bool& aResponseIsComplete,
                       const int64_t& aTransferSize,
                       const TimingStructArgs& aTimings,
                       const Maybe<nsHttpHeaderArray>& responseTrailers,
                       const bool& aHasStickyConn);
  void DoNotifyListener();

  nsCOMPtr<nsITransportEventSink> mEventsink;
  nsCOMPtr<nsIStreamListener> mChannel;
  nsCOMPtr<nsIEventTarget> mTargetThread;
  nsCOMPtr<nsISupports> mSecurityInfo;
  nsAutoPtr<nsHttpResponseHead> mResponseHead;
  nsAutoPtr<nsHttpHeaderArray> mResponseTrailers;
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

  NetAddr mSelfAddr;
  NetAddr mPeerAddr;
  TimingStruct mTimings;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpTransactionParent_h__
