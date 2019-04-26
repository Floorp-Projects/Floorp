/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcProxyChannelWrapper.h"

#include "mozilla/net/WebrtcProxyChannelChild.h"
#include "ipc/WebrtcProxyChannel.h"
#include "mozilla/LoadInfo.h"

#include "nsIEventTarget.h"
#include "nsNetCID.h"
#include "nsProxyRelease.h"

#include "nr_socket_proxy_config.h"

namespace mozilla {
namespace net {

using std::shared_ptr;

WebrtcProxyChannelWrapper::WebrtcProxyChannelWrapper(
    WebrtcProxyChannelCallback* aCallbacks)
    : mProxyCallbacks(aCallbacks),
      mWebrtcProxyChannel(nullptr),
      mMainThread(nullptr),
      mSocketThread(nullptr) {
  mMainThread = GetMainThreadEventTarget();
  mSocketThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
  MOZ_RELEASE_ASSERT(mMainThread, "no main thread");
  MOZ_RELEASE_ASSERT(mSocketThread, "no socket thread");
}

WebrtcProxyChannelWrapper::~WebrtcProxyChannelWrapper() {
  MOZ_ASSERT(!mWebrtcProxyChannel, "webrtc proxy channel non-null");

  // If we're never opened then we never get an OnClose from our parent process.
  // We need to release our callbacks here safely.
  NS_ProxyRelease("WebrtcProxyChannelWrapper::CleanUpCallbacks", mSocketThread,
                  mProxyCallbacks.forget());
}

void WebrtcProxyChannelWrapper::AsyncOpen(
    const nsCString& aHost, const int& aPort,
    const shared_ptr<NrSocketProxyConfig>& aConfig) {
  if (!NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(mMainThread->Dispatch(
        NewRunnableMethod<const nsCString, const int,
                          const shared_ptr<NrSocketProxyConfig>>(
            "WebrtcProxyChannelWrapper::AsyncOpen", this,
            &WebrtcProxyChannelWrapper::AsyncOpen, aHost, aPort, aConfig)));
    return;
  }

  MOZ_ASSERT(!mWebrtcProxyChannel, "wrapper already open");
  mWebrtcProxyChannel = new WebrtcProxyChannelChild(this);
  mWebrtcProxyChannel->AsyncOpen(aHost, aPort, aConfig->GetLoadInfoArgs(),
                                 aConfig->GetAlpn(),
                                 dom::TabId(aConfig->GetTabId()));
}

void WebrtcProxyChannelWrapper::SendWrite(nsTArray<uint8_t>&& aReadData) {
  if (!NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(
        mMainThread->Dispatch(NewRunnableMethod<nsTArray<uint8_t>&&>(
            "WebrtcProxyChannelWrapper::SendWrite", this,
            &WebrtcProxyChannelWrapper::SendWrite, std::move(aReadData))));
    return;
  }

  MOZ_ASSERT(mWebrtcProxyChannel, "webrtc proxy channel should be non-null");
  mWebrtcProxyChannel->SendWrite(aReadData);
}

void WebrtcProxyChannelWrapper::Close() {
  if (!NS_IsMainThread()) {
    MOZ_ALWAYS_SUCCEEDS(mMainThread->Dispatch(
        NewRunnableMethod("WebrtcProxyChannelWrapper::Close", this,
                          &WebrtcProxyChannelWrapper::Close)));
    return;
  }

  // We're only open if we have a channel. Also Close() should be idempotent.
  if (mWebrtcProxyChannel) {
    RefPtr<WebrtcProxyChannelChild> child = mWebrtcProxyChannel;
    mWebrtcProxyChannel = nullptr;
    child->SendClose();
  }
}

void WebrtcProxyChannelWrapper::OnClose(nsresult aReason) {
  MOZ_ASSERT(NS_IsMainThread(), "not on main thread");
  MOZ_ASSERT(mProxyCallbacks, "webrtc proxy callbacks should be non-null");

  MOZ_ALWAYS_SUCCEEDS(mSocketThread->Dispatch(NewRunnableMethod<nsresult>(
      "WebrtcProxyChannelWrapper::OnClose", mProxyCallbacks,
      &WebrtcProxyChannelCallback::OnClose, aReason)));

  NS_ProxyRelease("WebrtcProxyChannelWrapper::CleanUpCallbacks", mSocketThread,
                  mProxyCallbacks.forget());
}

void WebrtcProxyChannelWrapper::OnRead(nsTArray<uint8_t>&& aReadData) {
  MOZ_ASSERT(NS_IsMainThread(), "not on main thread");
  MOZ_ASSERT(mProxyCallbacks, "webrtc proxy callbacks should be non-null");

  MOZ_ALWAYS_SUCCEEDS(
      mSocketThread->Dispatch(NewRunnableMethod<nsTArray<uint8_t>&&>(
          "WebrtcProxyChannelWrapper::OnRead", mProxyCallbacks,
          &WebrtcProxyChannelCallback::OnRead, std::move(aReadData))));
}

void WebrtcProxyChannelWrapper::OnConnected() {
  MOZ_ASSERT(NS_IsMainThread(), "not on main thread");
  MOZ_ASSERT(mProxyCallbacks, "webrtc proxy callbacks should be non-null");

  MOZ_ALWAYS_SUCCEEDS(mSocketThread->Dispatch(NewRunnableMethod(
      "WebrtcProxyChannelWrapper::OnConnected", mProxyCallbacks,
      &WebrtcProxyChannelCallback::OnConnected)));
}

}  // namespace net
}  // namespace mozilla
