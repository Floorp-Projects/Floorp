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

WebSocketEventListenerChild::WebSocketEventListenerChild(uint64_t aInnerWindowID)
  : mService(WebSocketEventService::GetOrCreate())
  , mInnerWindowID(aInnerWindowID)
{}

WebSocketEventListenerChild::~WebSocketEventListenerChild()
{
  MOZ_ASSERT(!mService);
}

bool
WebSocketEventListenerChild::RecvFrameReceived(const uint32_t& aWebSocketSerialID,
                                               const WebSocketFrameData& aFrameData)
{
  if (mService) {
    RefPtr<WebSocketFrame> frame = new WebSocketFrame(aFrameData);
    mService->FrameReceived(aWebSocketSerialID, mInnerWindowID, frame);
  }

  return true;
}

bool
WebSocketEventListenerChild::RecvFrameSent(const uint32_t& aWebSocketSerialID,
                                           const WebSocketFrameData& aFrameData)
{
  if (mService) {
    RefPtr<WebSocketFrame> frame = new WebSocketFrame(aFrameData);
    mService->FrameSent(aWebSocketSerialID, mInnerWindowID, frame);
  }

  return true;
}

void
WebSocketEventListenerChild::Close()
{
  mService = nullptr;
  SendClose();
}

void
WebSocketEventListenerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mService = nullptr;
}

} // namespace net
} // namespace mozilla
