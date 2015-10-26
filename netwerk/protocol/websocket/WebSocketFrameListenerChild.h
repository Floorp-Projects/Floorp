/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketFrameListenerChild_h
#define mozilla_net_WebSocketFrameListenerChild_h

#include "mozilla/net/PWebSocketFrameListenerChild.h"

namespace mozilla {
namespace net {

class WebSocketFrameService;

class WebSocketFrameListenerChild final : public PWebSocketFrameListenerChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(WebSocketFrameListenerChild)

  explicit WebSocketFrameListenerChild(uint64_t aInnerWindowID);

  bool RecvFrameReceived(const uint32_t& aWebSocketSerialID,
                         const WebSocketFrameData& aFrameData) override;

  bool RecvFrameSent(const uint32_t& aWebSocketSerialID,
                     const WebSocketFrameData& aFrameData) override;

  void Close();

private:
  ~WebSocketFrameListenerChild();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<WebSocketFrameService> mService;
  uint64_t mInnerWindowID;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_WebSocketFrameListenerChild_h
