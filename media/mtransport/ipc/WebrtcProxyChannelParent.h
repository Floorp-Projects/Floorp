/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_WebrtcProxyChannelParent_h
#define mozilla_net_WebrtcProxyChannelParent_h

#include "mozilla/net/PWebrtcProxyChannelParent.h"

#include "WebrtcProxyChannelCallback.h"

class nsIAuthPromptProvider;

namespace mozilla {
namespace net {

class WebrtcProxyChannel;

class WebrtcProxyChannelParent : public PWebrtcProxyChannelParent,
                                 public WebrtcProxyChannelCallback {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcProxyChannelParent, override)

  mozilla::ipc::IPCResult RecvAsyncOpen(const nsCString& aHost,
                                        const int& aPort,
                                        const LoadInfoArgs& aLoadInfoArgs,
                                        const nsCString& aAlpn) override;

  mozilla::ipc::IPCResult RecvWrite(nsTArray<uint8_t>&& aWriteData) override;

  mozilla::ipc::IPCResult RecvClose() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  explicit WebrtcProxyChannelParent(dom::TabId aTabId);

  // WebrtcProxyChannelCallback
  void OnClose(nsresult aReason) override;
  void OnConnected() override;
  void OnRead(nsTArray<uint8_t>&& bytes) override;

  void AddIPDLReference() { AddRef(); }
  void ReleaseIPDLReference() { Release(); }

 protected:
  virtual ~WebrtcProxyChannelParent();

 private:
  void CleanupChannel();

  // Indicates that IPC is open.
  RefPtr<WebrtcProxyChannel> mChannel;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_WebrtcProxyChannelParent_h
