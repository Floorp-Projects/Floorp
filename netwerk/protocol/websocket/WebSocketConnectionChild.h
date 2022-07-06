/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketConnectionChild_h
#define mozilla_net_WebSocketConnectionChild_h

#include "mozilla/net/PWebSocketConnectionChild.h"
#include "mozilla/net/WebSocketConnectionListener.h"
#include "nsIHttpChannelInternal.h"

namespace mozilla {
namespace net {

class WebSocketConnection;

// WebSocketConnectionChild only lives in socket process and uses
// WebSocketConnection to send/read data from socket. Only IPDL holds a strong
// reference to WebSocketConnectionChild, so the life time of
// WebSocketConnectionChild is bound to the IPC actor.

class WebSocketConnectionChild final : public PWebSocketConnectionChild,
                                       public nsIHttpUpgradeListener,
                                       public WebSocketConnectionListener {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIHTTPUPGRADELISTENER

  WebSocketConnectionChild();
  void Init(uint32_t aListenerId);
  mozilla::ipc::IPCResult RecvWriteOutputData(nsTArray<uint8_t>&& aData);
  mozilla::ipc::IPCResult RecvStartReading();
  mozilla::ipc::IPCResult RecvDrainSocketData();
  mozilla::ipc::IPCResult Recv__delete__() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  void OnError(nsresult aStatus) override;
  void OnTCPClosed() override;
  nsresult OnDataReceived(uint8_t* aData, uint32_t aCount) override;

 private:
  virtual ~WebSocketConnectionChild();

  RefPtr<WebSocketConnection> mConnection;
  nsCOMPtr<nsIEventTarget> mSocketThread;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebSocketConnectionChild_h
