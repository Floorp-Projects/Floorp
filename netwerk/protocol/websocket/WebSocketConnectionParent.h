/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketConnectionParent_h
#define mozilla_net_WebSocketConnectionParent_h

#include "mozilla/net/PWebSocketConnectionParent.h"
#include "nsISupportsImpl.h"
#include "nsIWebSocketConnection.h"

class nsIHttpUpgradeListener;

namespace mozilla {
namespace net {

// WebSocketConnectionParent implements nsIWebSocketConnection and provides
// interface for WebSocketChannel to send/receive data. The ownership model for
// TransportProvider is that IPDL and WebSocketChannel hold strong reference of
// WebSocketConnectionParent. When nsIWebSocketConnection::Close is called, a
// __delete__ message will be sent and the IPC actor will be deallocated as
// well.

#define WEB_SOCKET_CONNECTION_PARENT_IID             \
  {                                                  \
    0x1cc3cb61, 0x0c09, 0x4f58, {                    \
      0x9a, 0x64, 0x44, 0xf7, 0x92, 0x86, 0xbc, 0x00 \
    }                                                \
  }

class WebSocketConnectionParent final : public PWebSocketConnectionParent,
                                        public nsIWebSocketConnection {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBSOCKETCONNECTION
  NS_DECLARE_STATIC_IID_ACCESSOR(WEB_SOCKET_CONNECTION_PARENT_IID)

  explicit WebSocketConnectionParent(nsIHttpUpgradeListener* aListener);

  mozilla::ipc::IPCResult RecvOnTransportAvailable(
      const nsCString& aSecurityInfoSerialization);
  mozilla::ipc::IPCResult RecvOnError(const nsresult& aStatus);
  mozilla::ipc::IPCResult RecvOnTCPClosed();
  mozilla::ipc::IPCResult RecvOnDataReceived(nsTArray<uint8_t>&& aData);
  mozilla::ipc::IPCResult RecvOnUpgradeFailed(const nsresult& aReason);

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  virtual ~WebSocketConnectionParent();

  nsCOMPtr<nsIHttpUpgradeListener> mUpgradeListener;
  nsCOMPtr<nsIWebSocketConnectionListener> mListener;
  nsCOMPtr<nsIEventTarget> mEventTarget;
  nsCOMPtr<nsISupports> mSecurityInfo;
  bool mClosed;
};

NS_DEFINE_STATIC_IID_ACCESSOR(WebSocketConnectionParent,
                              WEB_SOCKET_CONNECTION_PARENT_IID)

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebSocketConnectionParent_h
