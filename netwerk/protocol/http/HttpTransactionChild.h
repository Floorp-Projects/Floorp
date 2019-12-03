/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HttpTransactionChild_h__
#define HttpTransactionChild_h__

#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/net/PHttpTransactionChild.h"
#include "nsHttpRequestHead.h"
#include "nsIRequest.h"
#include "nsIStreamListener.h"
#include "nsITransport.h"

class nsInputStreamPump;

namespace mozilla {
namespace net {

class nsHttpConnectionInfo;
class nsHttpTransaction;
class nsProxyInfo;

//-----------------------------------------------------------------------------
// HttpTransactionChild commutes between parent process and socket process,
// manages the real nsHttpTransaction and transaction pump.
//-----------------------------------------------------------------------------
class HttpTransactionChild final : public PHttpTransactionChild,
                                   public nsIStreamListener,
                                   public nsITransportEventSink {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITRANSPORTEVENTSINK

  explicit HttpTransactionChild();

  mozilla::ipc::IPCResult RecvInit(
      const uint32_t& aCaps, const HttpConnectionInfoCloneArgs& aArgs,
      const nsHttpRequestHead& aReqHeaders,
      const Maybe<IPCStream>& aRequestBody, const uint64_t& aReqContentLength,
      const bool& aReqBodyIncludesHeaders,
      const uint64_t& aTopLevelOuterContentWindowId,
      const uint8_t& aHttpTrafficCategory);
  mozilla::ipc::IPCResult RecvUpdateClassOfService(
      const uint32_t& classOfService);
  mozilla::ipc::IPCResult RecvCancelPump(const nsresult& aStatus);
  mozilla::ipc::IPCResult RecvSuspendPump();
  mozilla::ipc::IPCResult RecvResumePump();
  mozilla::ipc::IPCResult RecvSetDNSWasRefreshed();
  mozilla::ipc::IPCResult RecvDontReuseConnection();
  mozilla::ipc::IPCResult RecvSetH2WSConnRefTaken();
  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  virtual ~HttpTransactionChild();

  nsProxyInfo* ProxyInfoCloneArgsToProxyInfo(
      const nsTArray<ProxyInfoCloneArgs>& aArgs);
  already_AddRefed<nsHttpConnectionInfo> DeserializeHttpConnectionInfoCloneArgs(
      const HttpConnectionInfoCloneArgs& aInfoArgs);
  // Initialize the *real* nsHttpTransaction. See |nsHttpTransaction::Init|
  // for the parameters.
  MOZ_MUST_USE nsresult InitInternal(
      uint32_t caps, const HttpConnectionInfoCloneArgs& aArgs,
      nsHttpRequestHead* reqHeaders,
      nsIInputStream* reqBody,  // use the trick in bug 1277681
      uint64_t reqContentLength, bool reqBodyIncludesHeaders,
      uint64_t topLevelOuterContentWindowId, uint8_t httpTrafficCategory);

  bool mCanceled;
  nsresult mStatus;
  nsHttpRequestHead mRequestHead;

  nsCOMPtr<nsIInputStream> mUploadStream;
  RefPtr<nsHttpTransaction> mTransaction;
  nsCOMPtr<nsIRequest> mTransactionPump;
};

}  // namespace net
}  // namespace mozilla

inline nsISupports* ToSupports(mozilla::net::HttpTransactionChild* p) {
  return static_cast<nsIStreamListener*>(p);
}

#endif  // nsHttpTransactionChild_h__
