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

  NS_IMETHOD QueryInterface(const nsIID & uuid, void **result) override = 0;
  NS_IMETHOD_(MozExternalRefCountType ) AddRef(void) override = 0;
  NS_IMETHOD_(MozExternalRefCountType ) Release(void) override = 0;

  // Partial implementation of nsIWebSocketChannel
  //
  NS_IMETHOD GetOriginalURI(nsIURI **aOriginalURI) override;
  NS_IMETHOD GetURI(nsIURI **aURI) override;
  NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor **aNotificationCallbacks) override;
  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor *aNotificationCallbacks) override;
  NS_IMETHOD GetLoadGroup(nsILoadGroup **aLoadGroup) override;
  NS_IMETHOD SetLoadGroup(nsILoadGroup *aLoadGroup) override;
  NS_IMETHOD SetLoadInfo(nsILoadInfo *aLoadInfo) override;
  NS_IMETHOD GetLoadInfo(nsILoadInfo **aLoadInfo) override;
  NS_IMETHOD GetExtensions(nsACString &aExtensions) override;
  NS_IMETHOD GetProtocol(nsACString &aProtocol) override;
  NS_IMETHOD SetProtocol(const nsACString &aProtocol) override;
  NS_IMETHOD GetPingInterval(uint32_t *aSeconds) override;
  NS_IMETHOD SetPingInterval(uint32_t aSeconds) override;
  NS_IMETHOD GetPingTimeout(uint32_t *aSeconds) override;
  NS_IMETHOD SetPingTimeout(uint32_t aSeconds) override;
  NS_IMETHOD InitLoadInfo(nsINode* aLoadingNode, nsIPrincipal* aLoadingPrincipal,
                          nsIPrincipal* aTriggeringPrincipal, uint32_t aSecurityFlags,
                          uint32_t aContentPolicyType) override;
  NS_IMETHOD GetSerial(uint32_t* aSerial) override;
  NS_IMETHOD SetSerial(uint32_t aSerial) override;
  NS_IMETHOD SetServerParameters(nsITransportProvider* aProvider,
                                 const nsACString& aNegotiatedExtensions) override;

  // Off main thread URI access.
  virtual void GetEffectiveURL(nsAString& aEffectiveURL) const = 0;
  virtual bool IsEncrypted() const = 0;

  class ListenerAndContextContainer final
  {
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ListenerAndContextContainer)

    ListenerAndContextContainer(nsIWebSocketListener* aListener,
                                nsISupports* aContext);

    nsCOMPtr<nsIWebSocketListener> mListener;
    nsCOMPtr<nsISupports>          mContext;

  private:
    ~ListenerAndContextContainer();
  };

 protected:
  nsCOMPtr<nsIURI>                mOriginalURI;
  nsCOMPtr<nsIURI>                mURI;
  RefPtr<ListenerAndContextContainer> mListenerMT;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsILoadGroup>          mLoadGroup;
  nsCOMPtr<nsILoadInfo>           mLoadInfo;
  nsCOMPtr<nsIEventTarget>        mTargetThread;
  nsCOMPtr<nsITransportProvider>  mServerTransportProvider;

  nsCString                       mProtocol;
  nsCString                       mOrigin;

  nsCString                       mNegotiatedExtensions;

  uint32_t                        mWasOpened                 : 1;
  uint32_t                        mClientSetPingInterval     : 1;
  uint32_t                        mClientSetPingTimeout      : 1;

  Atomic<bool>                    mEncrypted;
  bool                            mPingForced;
  bool                            mIsServerSide;

  uint32_t                        mPingInterval;         /* milliseconds */
  uint32_t                        mPingResponseTimeout;  /* milliseconds */

  uint32_t                        mSerial;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_BaseWebSocketChannel_h
