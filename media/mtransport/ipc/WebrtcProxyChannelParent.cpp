/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcProxyChannelParent.h"

#include "mozilla/net/NeckoParent.h"

#include "WebrtcProxyChannel.h"
#include "WebrtcProxyLog.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

mozilla::ipc::IPCResult WebrtcProxyChannelParent::RecvAsyncOpen(
    const nsCString& aHost, const int& aPort, const LoadInfoArgs& aLoadInfoArgs,
    const nsCString& aAlpn) {
  LOG(("WebrtcProxyChannelParent::RecvAsyncOpen %p to %s:%d\n", this,
       aHost.get(), aPort));

  MOZ_ASSERT(mChannel, "webrtc proxy channel should be non-null");
  mChannel->Open(aHost, aPort, aLoadInfoArgs, aAlpn);

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcProxyChannelParent::RecvWrite(
    nsTArray<uint8_t>&& aWriteData) {
  LOG(("WebrtcProxyChannelParent::RecvWrite %p for %zu\n", this,
       aWriteData.Length()));

  // Need to check this here in case there are Writes in the queue after OnClose
  if (mChannel) {
    mChannel->Write(std::move(aWriteData));
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WebrtcProxyChannelParent::RecvClose() {
  LOG(("WebrtcProxyChannelParent::RecvClose %p\n", this));

  CleanupChannel();

  IProtocol* mgr = Manager();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }

  return IPC_OK();
}

void WebrtcProxyChannelParent::ActorDestroy(ActorDestroyReason aWhy) {
  LOG(("WebrtcProxyChannelParent::ActorDestroy %p for %d\n", this, aWhy));

  CleanupChannel();
}

WebrtcProxyChannelParent::WebrtcProxyChannelParent(dom::TabId aTabId) {
  MOZ_COUNT_CTOR(WebrtcProxyChannelParent);

  LOG(("WebrtcProxyChannelParent::WebrtcProxyChannelParent %p\n", this));

  mChannel = new WebrtcProxyChannel(this);
  mChannel->SetTabId(aTabId);
}

WebrtcProxyChannelParent::~WebrtcProxyChannelParent() {
  MOZ_COUNT_DTOR(WebrtcProxyChannelParent);

  LOG(("WebrtcProxyChannelParent::~WebrtcProxyChannelParent %p\n", this));

  CleanupChannel();
}

// WebrtcProxyChannelCallback
void WebrtcProxyChannelParent::OnClose(nsresult aReason) {
  LOG(("WebrtcProxyChannelParent::OnClose %p\n", this));

  if (mChannel) {
    Unused << SendOnClose(aReason);
  }

  CleanupChannel();
}

void WebrtcProxyChannelParent::OnRead(nsTArray<uint8_t>&& aReadData) {
  LOG(("WebrtcProxyChannelParent::OnRead %p %zu\n", this, aReadData.Length()));

  if (mChannel && !SendOnRead(std::move(aReadData))) {
    CleanupChannel();
  }
}

void WebrtcProxyChannelParent::OnConnected() {
  LOG(("WebrtcProxyChannelParent::OnConnected %p\n", this));

  if (mChannel && !SendOnConnected()) {
    CleanupChannel();
  }
}

void WebrtcProxyChannelParent::CleanupChannel() {
  if (mChannel) {
    mChannel->Close();
    mChannel = nullptr;
  }
}

}  // namespace net
}  // namespace mozilla
