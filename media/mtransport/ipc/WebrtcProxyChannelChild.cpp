/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcProxyChannelChild.h"

#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/SocketProcessChild.h"

#include "LoadInfo.h"

#include "WebrtcProxyLog.h"
#include "WebrtcProxyChannelCallback.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

mozilla::ipc::IPCResult WebrtcProxyChannelChild::RecvOnClose(
    const nsresult& aReason) {
  LOG(("WebrtcProxyChannelChild::RecvOnClose %p\n", this));

  MOZ_ASSERT(mProxyCallbacks, "webrtc proxy callbacks should be non-null");
  mProxyCallbacks->OnClose(aReason);
  mProxyCallbacks = nullptr;

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcProxyChannelChild::RecvOnConnected() {
  LOG(("WebrtcProxyChannelChild::RecvOnConnected %p\n", this));

  MOZ_ASSERT(mProxyCallbacks, "webrtc proxy callbacks should be non-null");
  mProxyCallbacks->OnConnected();

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcProxyChannelChild::RecvOnRead(
    nsTArray<uint8_t>&& aReadData) {
  LOG(("WebrtcProxyChannelChild::RecvOnRead %p\n", this));

  MOZ_ASSERT(mProxyCallbacks, "webrtc proxy callbacks should be non-null");
  mProxyCallbacks->OnRead(std::move(aReadData));

  return IPC_OK();
}

WebrtcProxyChannelChild::WebrtcProxyChannelChild(
    WebrtcProxyChannelCallback* aProxyCallbacks)
    : mProxyCallbacks(aProxyCallbacks) {
  MOZ_COUNT_CTOR(WebrtcProxyChannelChild);

  LOG(("WebrtcProxyChannelChild::WebrtcProxyChannelChild %p\n", this));
}

WebrtcProxyChannelChild::~WebrtcProxyChannelChild() {
  MOZ_COUNT_DTOR(WebrtcProxyChannelChild);

  LOG(("WebrtcProxyChannelChild::~WebrtcProxyChannelChild %p\n", this));
}

void WebrtcProxyChannelChild::AsyncOpen(const nsCString& aHost,
                                        const int& aPort,
                                        const net::LoadInfoArgs& aArgs,
                                        const nsCString& aAlpn,
                                        const dom::TabId& aTabId) {
  LOG(("WebrtcProxyChannelChild::AsyncOpen %p %s:%d\n", this, aHost.get(),
       aPort));

  MOZ_ASSERT(NS_IsMainThread(), "not main thread");

  AddIPDLReference();

  if (IsNeckoChild()) {
    // We're on a content process
    gNeckoChild->SetEventTargetForActor(this, GetMainThreadEventTarget());
    gNeckoChild->SendPWebrtcProxyChannelConstructor(this, aTabId);
  } else if (IsSocketProcessChild()) {
    // We're on a socket process
    SocketProcessChild::GetSingleton()->SetEventTargetForActor(
        this, GetMainThreadEventTarget());
    SocketProcessChild::GetSingleton()->SendPWebrtcProxyChannelConstructor(
        this, aTabId);
  }

  SendAsyncOpen(aHost, aPort, aArgs, aAlpn);
}

}  // namespace net
}  // namespace mozilla
