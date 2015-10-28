/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebSocketFrameListenerParent_h
#define mozilla_net_WebSocketFrameListenerParent_h

#include "mozilla/net/PWebSocketFrameListenerParent.h"
#include "nsIWebSocketFrameService.h"

namespace mozilla {
namespace net {

class WebSocketFrameService;

class WebSocketFrameListenerParent final : public PWebSocketFrameListenerParent
                                         , public nsIWebSocketFrameListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBSOCKETFRAMELISTENER

  explicit WebSocketFrameListenerParent(uint64_t aInnerWindowID);

private:
  ~WebSocketFrameListenerParent();

  virtual bool RecvClose() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  void UnregisterListener();

  RefPtr<WebSocketFrameService> mService;
  uint64_t mInnerWindowID;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_WebSocketFrameListenerParent_h
