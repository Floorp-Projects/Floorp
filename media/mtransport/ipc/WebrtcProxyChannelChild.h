/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebrtcProxyChannelChild_h
#define mozilla_net_WebrtcProxyChannelChild_h

#include "mozilla/net/PWebrtcProxyChannelChild.h"
#include "mozilla/dom/ipc/IdType.h"

namespace mozilla {

namespace net {

class WebrtcProxyChannelCallback;

class WebrtcProxyChannelChild : public PWebrtcProxyChannelChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcProxyChannelChild)

  mozilla::ipc::IPCResult RecvOnClose(const nsresult& aReason) override;

  mozilla::ipc::IPCResult RecvOnConnected() override;

  mozilla::ipc::IPCResult RecvOnRead(nsTArray<uint8_t>&& aReadData) override;

  explicit WebrtcProxyChannelChild(WebrtcProxyChannelCallback* aProxyCallbacks);

  void AsyncOpen(const nsCString& aHost, const int& aPort,
                 const net::LoadInfoArgs& aArgs, const nsCString& aAlpn,
                 const dom::TabId& aTabId);

  void AddIPDLReference() { AddRef(); }
  void ReleaseIPDLReference() { Release(); }

 protected:
  virtual ~WebrtcProxyChannelChild();

  RefPtr<WebrtcProxyChannelCallback> mProxyCallbacks;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebrtcProxyChannelChild_h
