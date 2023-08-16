/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketConnectionParent_h
#define mozilla_net_WebSocketConnectionParent_h

#include "mozilla/net/PWebSocketConnectionParent.h"
#include "mozilla/net/WebSocketConnectionBase.h"
#include "nsISupportsImpl.h"
#include "WebSocketConnectionBase.h"

class nsIHttpUpgradeListener;

namespace mozilla {
namespace net {

class WebSocketConnectionListener;

// WebSocketConnectionParent implements WebSocketConnectionBase and provides
// interface for WebSocketChannel to send/receive data. The ownership model for
// WebSocketConnectionParent is that IPDL and WebSocketChannel hold strong
// reference of this object. When Close() is called, a __delete__ message will
// be sent and the IPC actor will be deallocated as well.

class WebSocketConnectionParent final : public PWebSocketConnectionParent,
                                        public WebSocketConnectionBase {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit WebSocketConnectionParent(nsIHttpUpgradeListener* aListener);

  mozilla::ipc::IPCResult RecvOnTransportAvailable(
      nsITransportSecurityInfo* aSecurityInfo);
  mozilla::ipc::IPCResult RecvOnError(const nsresult& aStatus);
  mozilla::ipc::IPCResult RecvOnTCPClosed();
  mozilla::ipc::IPCResult RecvOnDataReceived(nsTArray<uint8_t>&& aData);
  mozilla::ipc::IPCResult RecvOnUpgradeFailed(const nsresult& aReason);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  nsresult Init(WebSocketConnectionListener* aListener) override;
  void GetIoTarget(nsIEventTarget** aTarget) override;
  void Close() override;
  nsresult WriteOutputData(const uint8_t* aHdrBuf, uint32_t aHdrBufLength,
                           const uint8_t* aPayloadBuf,
                           uint32_t aPayloadBufLength) override;
  nsresult StartReading() override;
  void DrainSocketData() override;
  nsresult GetSecurityInfo(nsITransportSecurityInfo** aSecurityInfo) override;

 private:
  virtual ~WebSocketConnectionParent();

  nsCOMPtr<nsIHttpUpgradeListener> mUpgradeListener;
  RefPtr<WebSocketConnectionListener> mListener;
  nsCOMPtr<nsIEventTarget> mBackgroundThread;
  nsCOMPtr<nsITransportSecurityInfo> mSecurityInfo;
  Atomic<bool> mClosed{false};
  Mutex mMutex MOZ_UNANNOTATED{"WebSocketConnectionParent::mMutex"};
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebSocketConnectionParent_h
