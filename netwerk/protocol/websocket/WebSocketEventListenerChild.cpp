/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketEventListenerChild.h"

#include "WebSocketEventService.h"
#include "WebSocketFrame.h"

namespace mozilla {
namespace net {

WebSocketEventListenerChild::WebSocketEventListenerChild(
    uint64_t aInnerWindowID, nsIEventTarget* aTarget)
    : NeckoTargetHolder(aTarget),
      mService(WebSocketEventService::GetOrCreate()),
      mInnerWindowID(aInnerWindowID) {}

WebSocketEventListenerChild::~WebSocketEventListenerChild() {
  MOZ_ASSERT(!mService);
}

mozilla::ipc::IPCResult WebSocketEventListenerChild::RecvWebSocketCreated(
    const uint32_t& aWebSocketSerialID, const nsString& aURI,
    const nsCString& aProtocols) {
  if (mService) {
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    mService->WebSocketCreated(aWebSocketSerialID, mInnerWindowID, aURI,
                               aProtocols, target);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketEventListenerChild::RecvWebSocketOpened(
    const uint32_t& aWebSocketSerialID, const nsString& aEffectiveURI,
    const nsCString& aProtocols, const nsCString& aExtensions,
    const uint64_t& aHttpChannelId) {
  if (mService) {
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    mService->WebSocketOpened(aWebSocketSerialID, mInnerWindowID, aEffectiveURI,
                              aProtocols, aExtensions, aHttpChannelId, target);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
WebSocketEventListenerChild::RecvWebSocketMessageAvailable(
    const uint32_t& aWebSocketSerialID, const nsCString& aData,
    const uint16_t& aMessageType) {
  if (mService) {
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    mService->WebSocketMessageAvailable(aWebSocketSerialID, mInnerWindowID,
                                        aData, aMessageType, target);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketEventListenerChild::RecvWebSocketClosed(
    const uint32_t& aWebSocketSerialID, const bool& aWasClean,
    const uint16_t& aCode, const nsString& aReason) {
  if (mService) {
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    mService->WebSocketClosed(aWebSocketSerialID, mInnerWindowID, aWasClean,
                              aCode, aReason, target);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketEventListenerChild::RecvFrameReceived(
    const uint32_t& aWebSocketSerialID, const WebSocketFrameData& aFrameData) {
  if (mService) {
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    RefPtr<WebSocketFrame> frame = new WebSocketFrame(aFrameData);
    mService->FrameReceived(aWebSocketSerialID, mInnerWindowID, frame.forget(),
                            target);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebSocketEventListenerChild::RecvFrameSent(
    const uint32_t& aWebSocketSerialID, const WebSocketFrameData& aFrameData) {
  if (mService) {
    nsCOMPtr<nsIEventTarget> target = GetNeckoTarget();
    RefPtr<WebSocketFrame> frame = new WebSocketFrame(aFrameData);
    mService->FrameSent(aWebSocketSerialID, mInnerWindowID, frame.forget(),
                        target);
  }

  return IPC_OK();
}

void WebSocketEventListenerChild::Close() {
  mService = nullptr;
  SendClose();
}

void WebSocketEventListenerChild::ActorDestroy(ActorDestroyReason aWhy) {
  mService = nullptr;
}

}  // namespace net
}  // namespace mozilla
