/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_BaseWebSocketChannel_h
#define mozilla_net_BaseWebSocketChannel_h

#include "nsIWebSocketChannel.h"
#include "nsIWebSocketListener.h"
#include "nsIProtocolHandler.h"
#include "nsIThread.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsCOMPtr.h"
#include "nsString.h"

namespace mozilla {
namespace net {

const static int32_t kDefaultWSPort     = 80;
const static int32_t kDefaultWSSPort    = 443;

class BaseWebSocketChannel : public nsIWebSocketChannel,
                             public nsIProtocolHandler,
                             public nsIThreadRetargetableRequest
{
 public:
  BaseWebSocketChannel();

  NS_DECL_NSIPROTOCOLHANDLER
  NS_DECL_NSITHREADRETARGETABLEREQUEST

  NS_IMETHOD QueryInterface(const nsIID & uuid, void **result) = 0;
  NS_IMETHOD_(MozExternalRefCountType ) AddRef(void) = 0;
  NS_IMETHOD_(MozExternalRefCountType ) Release(void) = 0;

  // Partial implementation of nsIWebSocketChannel
  //
  NS_IMETHOD GetOriginalURI(nsIURI **aOriginalURI);
  NS_IMETHOD GetURI(nsIURI **aURI);
  NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor **aNotificationCallbacks);
  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor *aNotificationCallbacks);
  NS_IMETHOD GetLoadGroup(nsILoadGroup **aLoadGroup);
  NS_IMETHOD SetLoadGroup(nsILoadGroup *aLoadGroup);
  NS_IMETHOD SetLoadInfo(nsILoadInfo *aLoadInfo);
  NS_IMETHOD GetLoadInfo(nsILoadInfo **aLoadInfo);
  NS_IMETHOD GetExtensions(nsACString &aExtensions);
  NS_IMETHOD GetProtocol(nsACString &aProtocol);
  NS_IMETHOD SetProtocol(const nsACString &aProtocol);
  NS_IMETHOD GetPingInterval(uint32_t *aSeconds);
  NS_IMETHOD SetPingInterval(uint32_t aSeconds);
  NS_IMETHOD GetPingTimeout(uint32_t *aSeconds);
  NS_IMETHOD SetPingTimeout(uint32_t aSeconds);

  // Off main thread URI access.
  virtual void GetEffectiveURL(nsAString& aEffectiveURL) const = 0;
  virtual bool IsEncrypted() const = 0;

 protected:
  nsCOMPtr<nsIURI>                mOriginalURI;
  nsCOMPtr<nsIURI>                mURI;
  nsCOMPtr<nsIWebSocketListener>  mListener;
  nsCOMPtr<nsISupports>           mContext;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsILoadGroup>          mLoadGroup;
  nsCOMPtr<nsILoadInfo>           mLoadInfo;
  nsCOMPtr<nsIEventTarget>        mTargetThread;

  nsCString                       mProtocol;
  nsCString                       mOrigin;

  nsCString                       mNegotiatedExtensions;

  uint32_t                        mEncrypted                 : 1;
  uint32_t                        mWasOpened                 : 1;
  uint32_t                        mClientSetPingInterval     : 1;
  uint32_t                        mClientSetPingTimeout      : 1;
  uint32_t                        mPingForced                : 1;

  uint32_t                        mPingInterval;         /* milliseconds */
  uint32_t                        mPingResponseTimeout;  /* milliseconds */
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_BaseWebSocketChannel_h
