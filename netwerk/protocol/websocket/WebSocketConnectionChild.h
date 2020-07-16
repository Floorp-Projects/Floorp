/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketConnectionChild_h
#define mozilla_net_WebSocketConnectionChild_h

#include "mozilla/net/PWebSocketConnectionChild.h"
#include "nsIHttpChannelInternal.h"
#include "nsISupportsImpl.h"
#include "nsIWebSocketConnection.h"

namespace mozilla {
namespace net {

class nsWebSocketConnection;

// WebSocketConnectionChild only lives in socket process and uses
// nsWebSocketConnection to send/read data from socket. Only IPDL holds a strong
// reference to WebSocketConnectionChild, so the life time of
// WebSocketConnectionChild is bound to the IPC actor.

class WebSocketConnectionChild final : public PWebSocketConnectionChild,
                                       public nsIHttpUpgradeListener,
                                       public nsIWebSocketConnectionListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHTTPUPGRADELISTENER
  NS_DECL_NSIWEBSOCKETCONNECTIONLISTENER

  WebSocketConnectionChild();

  mozilla::ipc::IPCResult RecvEnqueueOutgoingData(nsTArray<uint8_t>&& aHeader,
                                                  nsTArray<uint8_t>&& aPayload);
  mozilla::ipc::IPCResult RecvStartReading();
  mozilla::ipc::IPCResult RecvDrainSocketData();
  mozilla::ipc::IPCResult Recv__delete__() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  virtual ~WebSocketConnectionChild();

  RefPtr<nsWebSocketConnection> mConnection;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebSocketConnectionChild_h
