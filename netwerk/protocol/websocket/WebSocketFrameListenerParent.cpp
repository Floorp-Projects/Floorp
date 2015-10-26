/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketFrameListenerParent.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace net {

NS_INTERFACE_MAP_BEGIN(WebSocketFrameListenerParent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebSocketFrameListener)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketFrameListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebSocketFrameListenerParent)
NS_IMPL_RELEASE(WebSocketFrameListenerParent)

WebSocketFrameListenerParent::WebSocketFrameListenerParent(uint64_t aInnerWindowID)
  : mService(WebSocketFrameService::GetOrCreate())
  , mInnerWindowID(aInnerWindowID)
{
  mService->AddListener(mInnerWindowID, this);
}

WebSocketFrameListenerParent::~WebSocketFrameListenerParent()
{
  MOZ_ASSERT(!mService);
}

bool
WebSocketFrameListenerParent::RecvClose()
{
  if (mService) {
    UnregisterListener();
    unused << Send__delete__(this);
  }

  return true;
}

void
WebSocketFrameListenerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  UnregisterListener();
}

void
WebSocketFrameListenerParent::UnregisterListener()
{
  if (mService) {
    mService->RemoveListener(mInnerWindowID, this);
    mService = nullptr;
  }
}

NS_IMETHODIMP
WebSocketFrameListenerParent::FrameReceived(uint32_t aWebSocketSerialID,
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
WebSocketFrameListenerParent::FrameSent(uint32_t aWebSocketSerialID,
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
