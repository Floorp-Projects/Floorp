/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketEventListenerChild_h
#define mozilla_net_WebSocketEventListenerChild_h

#include "mozilla/net/NeckoTargetHolder.h"
#include "mozilla/net/PWebSocketEventListenerChild.h"

namespace mozilla {
namespace net {

class WebSocketEventService;

class WebSocketEventListenerChild final : public PWebSocketEventListenerChild,
                                          public NeckoTargetHolder {
 public:
  NS_INLINE_DECL_REFCOUNTING(WebSocketEventListenerChild)

  explicit WebSocketEventListenerChild(uint64_t aInnerWindowID,
                                       nsIEventTarget* aTarget);

  mozilla::ipc::IPCResult RecvWebSocketCreated(
      const uint32_t& aWebSocketSerialID, const nsString& aURI,
      const nsCString& aProtocols);

  mozilla::ipc::IPCResult RecvWebSocketOpened(
      const uint32_t& aWebSocketSerialID, const nsString& aEffectiveURI,
      const nsCString& aProtocols, const nsCString& aExtensions,
      const uint64_t& aHttpChannelId);

  mozilla::ipc::IPCResult RecvWebSocketMessageAvailable(
      const uint32_t& aWebSocketSerialID, const nsCString& aData,
      const uint16_t& aMessageType);

  mozilla::ipc::IPCResult RecvWebSocketClosed(
      const uint32_t& aWebSocketSerialID, const bool& aWasClean,
      const uint16_t& aCode, const nsString& aReason);

  mozilla::ipc::IPCResult RecvFrameReceived(
      const uint32_t& aWebSocketSerialID, const WebSocketFrameData& aFrameData);

  mozilla::ipc::IPCResult RecvFrameSent(const uint32_t& aWebSocketSerialID,
                                        const WebSocketFrameData& aFrameData);

  void Close();

 private:
  ~WebSocketEventListenerChild();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<WebSocketEventService> mService;
  uint64_t mInnerWindowID;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebSocketEventListenerChild_h
