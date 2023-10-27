/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpTransactionChild_h__
#define HttpTransactionChild_h__

#include "mozilla/Atomics.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/net/PHttpTransactionChild.h"
#include "nsHttpRequestHead.h"
#include "nsIEarlyHintObserver.h"
#include "nsIRequest.h"
#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsIThrottledInputChannel.h"
#include "nsITransport.h"

class nsInputStreamPump;

namespace mozilla::net {

class BackgroundDataBridgeParent;
class InputChannelThrottleQueueChild;
class nsHttpConnectionInfo;
class nsHttpTransaction;
class nsProxyInfo;

//-----------------------------------------------------------------------------
// HttpTransactionChild commutes between parent process and socket process,
// manages the real nsHttpTransaction and transaction pump.
//-----------------------------------------------------------------------------
class HttpTransactionChild final : public PHttpTransactionChild,
                                   public nsITransportEventSink,
                                   public nsIThrottledInputChannel,
                                   public nsIThreadRetargetableStreamListener,
                                   public nsIEarlyHintObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITRANSPORTEVENTSINK
  NS_DECL_NSITHROTTLEDINPUTCHANNEL
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
  NS_DECL_NSIEARLYHINTOBSERVER

  explicit HttpTransactionChild();

  mozilla::ipc::IPCResult RecvInit(
      const uint32_t& aCaps, const HttpConnectionInfoCloneArgs& aArgs,
      const nsHttpRequestHead& aReqHeaders,
      const Maybe<IPCStream>& aRequestBody, const uint64_t& aReqContentLength,
      const bool& aReqBodyIncludesHeaders,
      const uint64_t& aTopLevelOuterContentWindowId,
      const uint8_t& aHttpTrafficCategory, const uint64_t& aRequestContextID,
      const ClassOfService& aClassOfService, const uint32_t& aInitialRwin,
      const bool& aResponseTimeoutEnabled, const uint64_t& aChannelId,
      const bool& aHasTransactionObserver,
      const Maybe<H2PushedStreamArg>& aPushedStreamArg,
      const mozilla::Maybe<PInputChannelThrottleQueueChild*>& aThrottleQueue,
      const bool& aIsDocumentLoad, const TimeStamp& aRedirectStart,
      const TimeStamp& aRedirectEnd);
  mozilla::ipc::IPCResult RecvCancelPump(const nsresult& aStatus);
  mozilla::ipc::IPCResult RecvSuspendPump();
  mozilla::ipc::IPCResult RecvResumePump();
  mozilla::ipc::IPCResult RecvSetDNSWasRefreshed();
  mozilla::ipc::IPCResult RecvDontReuseConnection();
  mozilla::ipc::IPCResult RecvSetH2WSConnRefTaken();
  void ActorDestroy(ActorDestroyReason aWhy) override;

  nsHttpTransaction* GetHttpTransaction();

 private:
  virtual ~HttpTransactionChild();

  nsProxyInfo* ProxyInfoCloneArgsToProxyInfo(
      const nsTArray<ProxyInfoCloneArgs>& aArgs);
  already_AddRefed<nsHttpConnectionInfo> DeserializeHttpConnectionInfoCloneArgs(
      const HttpConnectionInfoCloneArgs& aInfoArgs);
  // Initialize the *real* nsHttpTransaction. See |nsHttpTransaction::Init|
  // for the parameters.
  [[nodiscard]] nsresult InitInternal(
      uint32_t caps, const HttpConnectionInfoCloneArgs& infoArgs,
      nsHttpRequestHead* requestHead,
      nsIInputStream* requestBody,  // use the trick in bug 1277681
      uint64_t requestContentLength, bool requestBodyHasHeaders,
      uint64_t topLevelOuterContentWindowId, uint8_t httpTrafficCategory,
      uint64_t requestContextID, ClassOfService classOfService,
      uint32_t initialRwin, bool responseTimeoutEnabled, uint64_t channelId,
      bool aHasTransactionObserver,
      const Maybe<H2PushedStreamArg>& aPushedStreamArg);

  void CancelInternal(nsresult aStatus);

  bool CanSendODAToContentProcessDirectly(
      const Maybe<nsHttpResponseHead>& aHead);

  ResourceTimingStructArgs GetTimingAttributes();

  // Use Release-Acquire ordering to ensure the OMT ODA is ignored while
  // transaction is canceled on main thread.
  Atomic<bool, ReleaseAcquire> mCanceled{false};
  Atomic<nsresult, ReleaseAcquire> mStatus{NS_OK};
  uint64_t mChannelId{0};
  nsHttpRequestHead mRequestHead;
  bool mIsDocumentLoad{false};
  uint64_t mLogicalOffset{0};
  TimeStamp mRedirectStart;
  TimeStamp mRedirectEnd;
  nsCString mProtocolVersion;

  nsCOMPtr<nsIInputStream> mUploadStream;
  RefPtr<nsHttpTransaction> mTransaction;
  nsCOMPtr<nsIRequest> mTransactionPump;
  Maybe<TransactionObserverResult> mTransactionObserverResult;
  RefPtr<InputChannelThrottleQueueChild> mThrottleQueue;
  RefPtr<BackgroundDataBridgeParent> mDataBridgeParent;
};

}  // namespace mozilla::net

inline nsISupports* ToSupports(mozilla::net::HttpTransactionChild* p) {
  return static_cast<nsIStreamListener*>(p);
}

#endif  // nsHttpTransactionChild_h__
