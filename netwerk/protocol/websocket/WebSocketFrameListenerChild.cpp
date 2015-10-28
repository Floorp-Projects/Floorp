/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocketFrameListenerChild.h"

#include "WebSocketFrame.h"
#include "WebSocketFrameService.h"

namespace mozilla {
namespace net {

WebSocketFrameListenerChild::WebSocketFrameListenerChild(uint64_t aInnerWindowID)
  : mService(WebSocketFrameService::GetOrCreate())
  , mInnerWindowID(aInnerWindowID)
{}

WebSocketFrameListenerChild::~WebSocketFrameListenerChild()
{
  MOZ_ASSERT(!mService);
}

bool
WebSocketFrameListenerChild::RecvFrameReceived(const uint32_t& aWebSocketSerialID,
                                               const WebSocketFrameData& aFrameData)
{
  if (mService) {
    RefPtr<WebSocketFrame> frame = new WebSocketFrame(aFrameData);
    mService->FrameReceived(aWebSocketSerialID, mInnerWindowID, frame);
  }

  return true;
}

bool
WebSocketFrameListenerChild::RecvFrameSent(const uint32_t& aWebSocketSerialID,
                                           const WebSocketFrameData& aFrameData)
{
  if (mService) {
    RefPtr<WebSocketFrame> frame = new WebSocketFrame(aFrameData);
    mService->FrameSent(aWebSocketSerialID, mInnerWindowID, frame);
  }

  return true;
}

void
WebSocketFrameListenerChild::Close()
{
  mService = nullptr;
  SendClose();
}

void
WebSocketFrameListenerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mService = nullptr;
}

} // namespace net
} // namespace mozilla
