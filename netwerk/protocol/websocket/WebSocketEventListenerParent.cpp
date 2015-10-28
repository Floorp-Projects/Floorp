/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketEventListenerParent.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace net {

NS_INTERFACE_MAP_BEGIN(WebSocketEventListenerParent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebSocketEventListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebSocketEventListenerParent)
NS_IMPL_RELEASE(WebSocketEventListenerParent)

WebSocketEventListenerParent::WebSocketEventListenerParent(uint64_t aInnerWindowID)
  : mService(WebSocketEventService::GetOrCreate())
  , mInnerWindowID(aInnerWindowID)
{
  mService->AddListener(mInnerWindowID, this);
}

WebSocketEventListenerParent::~WebSocketEventListenerParent()
{
  MOZ_ASSERT(!mService);
}

bool
WebSocketEventListenerParent::RecvClose()
{
  if (mService) {
    UnregisterListener();
    unused << Send__delete__(this);
  }

  return true;
}

void
WebSocketEventListenerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  UnregisterListener();
}

void
WebSocketEventListenerParent::UnregisterListener()
{
  if (mService) {
    mService->RemoveListener(mInnerWindowID, this);
    mService = nullptr;
  }
}

NS_IMETHODIMP
WebSocketEventListenerParent::WebSocketCreated(uint32_t aWebSocketSerialID,
                                               const nsAString& aURI,
                                               const nsACString& aProtocols)
{
  unused << SendWebSocketCreated(aWebSocketSerialID, nsString(aURI),
                                 nsCString(aProtocols));
  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventListenerParent::WebSocketOpened(uint32_t aWebSocketSerialID,
                                              const nsAString& aEffectiveURI,
                                              const nsACString& aProtocols,
                                              const nsACString& aExtensions)
{
  unused << SendWebSocketOpened(aWebSocketSerialID, nsString(aEffectiveURI),
                                nsCString(aProtocols), nsCString(aExtensions));
  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventListenerParent::WebSocketClosed(uint32_t aWebSocketSerialID,
                                              bool aWasClean,
                                              uint16_t aCode,
                                              const nsAString& aReason)
{
  unused << SendWebSocketClosed(aWebSocketSerialID, aWasClean, aCode,
                                nsString(aReason));
  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventListenerParent::WebSocketMessageAvailable(uint32_t aWebSocketSerialID,
                                                        const nsACString& aData,
                                                        uint16_t aMessageType)
{
  unused << SendWebSocketMessageAvailable(aWebSocketSerialID, nsCString(aData),
                                          aMessageType);
  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventListenerParent::FrameReceived(uint32_t aWebSocketSerialID,
                                            nsIWebSocketFrame* aFrame)
{
  if (!aFrame) {
    return NS_ERROR_FAILURE;
  }

  WebSocketFrame* frame = static_cast<WebSocketFrame*>(aFrame);
  unused << SendFrameReceived(aWebSocketSerialID, frame->Data());
  return NS_OK;
}

NS_IMETHODIMP
WebSocketEventListenerParent::FrameSent(uint32_t aWebSocketSerialID,
                                        nsIWebSocketFrame* aFrame)
{
  if (!aFrame) {
    return NS_ERROR_FAILURE;
  }

  WebSocketFrame* frame = static_cast<WebSocketFrame*>(aFrame);
  unused << SendFrameSent(aWebSocketSerialID, frame->Data());
  return NS_OK;
}

} // namespace net
} // namespace mozilla
