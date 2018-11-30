/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef webrtc_proxy_channel_wrapper__
#define webrtc_proxy_channel_wrapper__

#include <memory>

#include "nsCOMPtr.h"
#include "nsTArray.h"

#include "mozilla/net/WebrtcProxyChannelCallback.h"

class nsIEventTarget;

namespace mozilla {

class NrSocketProxyConfig;

namespace net {

class WebrtcProxyChannelChild;

/**
 * WebrtcProxyChannelWrapper is a protector class for mtransport and IPDL.
 * mtransport and IPDL cannot include headers from each other due to conflicting
 * typedefs. Also it helps users by dispatching calls to the appropriate thread
 * based on mtransport's and IPDL's threading requirements.
 *
 * WebrtcProxyChannelWrapper is only used in the child process.
 * WebrtcProxyChannelWrapper does not dispatch for the parent process.
 * WebrtcProxyChannelCallback calls are dispatched to the STS thread.
 * IPDL calls are dispatched to the main thread.
 */
class WebrtcProxyChannelWrapper : public WebrtcProxyChannelCallback {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcProxyChannelWrapper, override)

  explicit WebrtcProxyChannelWrapper(WebrtcProxyChannelCallback* aCallbacks);

  virtual void AsyncOpen(const nsCString& aHost, const int& aPort,
                         const std::shared_ptr<NrSocketProxyConfig>& aConfig);
  virtual void SendWrite(nsTArray<uint8_t>&& aReadData);
  virtual void Close();

  // WebrtcProxyChannelCallback
  virtual void OnClose(nsresult aReason) override;
  virtual void OnConnected() override;
  virtual void OnRead(nsTArray<uint8_t>&& aReadData) override;

 protected:
  RefPtr<WebrtcProxyChannelCallback> mProxyCallbacks;
  RefPtr<WebrtcProxyChannelChild> mWebrtcProxyChannel;

  nsCOMPtr<nsIEventTarget> mMainThread;
  nsCOMPtr<nsIEventTarget> mSocketThread;

  virtual ~WebrtcProxyChannelWrapper();
};

}  // namespace net
}  // namespace mozilla

#endif  // webrtc_proxy_channel_wrapper__
