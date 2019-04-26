/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef webrtc_proxy_channel_h__
#define webrtc_proxy_channel_h__

#include <list>

#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIAuthPromptProvider.h"
#include "nsIHttpChannelInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "mozilla/dom/ipc/IdType.h"  // TabId

class nsISocketTransport;

namespace mozilla {
namespace net {
class LoadInfoArgs;

class WebrtcProxyChannelCallback;
class WebrtcProxyData;

class WebrtcProxyChannel : public nsIHttpUpgradeListener,
                           public nsIStreamListener,
                           public nsIInputStreamCallback,
                           public nsIOutputStreamCallback,
                           public nsIInterfaceRequestor,
                           public nsIAuthPromptProvider {
 public:
  NS_DECL_NSIHTTPUPGRADELISTENER
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_SAFE_NSIAUTHPROMPTPROVIDER(mAuthProvider)

  explicit WebrtcProxyChannel(WebrtcProxyChannelCallback* aCallbacks);

  void SetTabId(dom::TabId aTabId);
  nsresult Open(const nsCString& aHost, const int& aPort,
                const net::LoadInfoArgs& aArgs, const nsCString& aAlpn);
  nsresult Write(nsTArray<uint8_t>&& aBytes);
  nsresult Close();

  size_t CountUnwrittenBytes() const;

 protected:
  virtual ~WebrtcProxyChannel();

  // protected for gtests
  virtual void InvokeOnClose(nsresult aReason);
  virtual void InvokeOnConnected();
  virtual void InvokeOnRead(nsTArray<uint8_t>&& aReadData);

  RefPtr<WebrtcProxyChannelCallback> mProxyCallbacks;

 private:
  bool mClosed;
  bool mOpened;

  void EnqueueWrite_s(nsTArray<uint8_t>&& aWriteData);

  void CloseWithReason(nsresult aReason);

  size_t mWriteOffset;
  std::list<WebrtcProxyData> mWriteQueue;
  nsCOMPtr<nsIAuthPromptProvider> mAuthProvider;

  // Indicates that the channel is CONNECTed
  nsCOMPtr<nsISocketTransport> mTransport;
  nsCOMPtr<nsIAsyncInputStream> mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;
  nsCOMPtr<nsIEventTarget> mMainThread;
  nsCOMPtr<nsIEventTarget> mSocketThread;
};

}  // namespace net
}  // namespace mozilla

#endif  // webrtc_proxy_channel_h__
