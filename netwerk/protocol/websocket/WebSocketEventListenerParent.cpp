/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketEventService.h"
#include "WebSocketEventListenerParent.h"
#include "mozilla/Unused.h"
#include "WebSocketFrame.h"

namespace mozilla {
namespace net {

NS_INTERFACE_MAP_BEGIN(WebSocketEventListenerParent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebSocketEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebSocketEventListenerParent)
NS_IMPL_RELEASE(WebSocketEventListenerParent)

WebSocketEventListenerParent::WebSocketEventListenerParent(
    uint64_t aInnerWindowID)
    : mService(WebSocketEventService::GetOrCreate()),
      mInnerWindowID(aInnerWindowID) {
  DebugOnly<nsresult> rv = mService->AddListener(mInnerWindowID, this);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

WebSocketEventListenerParent::~WebSocketEventListenerParent() {
  MOZ_ASSERT(!mService);
}

mozilla::ipc::IPCResult WebSocketEventListenerParent::RecvClose() {
  if (mService) {
    UnregisterListener();
    Unused << Send__delete__(this);
  }

  return IPC_OK();
}

void WebSocketEventListenerParent::ActorDestroy(ActorDestroyReason aWhy) {
  UnregisterListener();
}

void WebSocketEventListenerParent::UnregisterListener() {
  if (mService) {
    DebugOnly<nsresult> rv = mService->RemoveListener(mInnerWindowID, this);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    mService = nullptr;
  }
}

NS_IMETHODIMP
WebSocketEventListenerParent::WebSocketCreated(uint32_t aWebSocketSerialID,
                                               const nsAString& aURI,
                                               const nsACString& aProtocols) {
  Unused << SendWebSocketCreated(aWebSocketSerialID, aURI, aProtocols);
  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventListenerParent::WebSocketOpened(uint32_t aWebSocketSerialID,
                                              const nsAString& aEffectiveURI,
                                              const nsACString& aProtocols,
                                              const nsACString& aExtensions,
                                              uint64_t aHttpChannelId) {
  Unused << SendWebSocketOpened(aWebSocketSerialID, aEffectiveURI, aProtocols,
                                aExtensions, aHttpChannelId);
  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventListenerParent::WebSocketClosed(uint32_t aWebSocketSerialID,
                                              bool aWasClean, uint16_t aCode,
                                              const nsAString& aReason) {
  Unused << SendWebSocketClosed(aWebSocketSerialID, aWasClean, aCode, aReason);
  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventListenerParent::WebSocketMessageAvailable(
    uint32_t aWebSocketSerialID, const nsACString& aData,
    uint16_t aMessageType) {
  Unused << SendWebSocketMessageAvailable(aWebSocketSerialID, aData,
                                          aMessageType);
  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventListenerParent::FrameReceived(uint32_t aWebSocketSerialID,
                                            nsIWebSocketFrame* aFrame) {
  if (!aFrame) {
    return NS_ERROR_FAILURE;
  }

  WebSocketFrame* frame = static_cast<WebSocketFrame*>(aFrame);
  Unused << SendFrameReceived(aWebSocketSerialID, frame->Data());
  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventListenerParent::FrameSent(uint32_t aWebSocketSerialID,
                                        nsIWebSocketFrame* aFrame) {
  if (!aFrame) {
    return NS_ERROR_FAILURE;
  }

  WebSocketFrame* frame = static_cast<WebSocketFrame*>(aFrame);
  Unused << SendFrameSent(aWebSocketSerialID, frame->Data());
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
