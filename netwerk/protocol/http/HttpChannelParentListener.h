/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpChannelCallbackWrapper_h
#define mozilla_net_HttpChannelCallbackWrapper_h

#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIRedirectResultListener.h"
#include "nsINetworkInterceptController.h"
#include "nsIStreamListener.h"

namespace mozilla {
namespace net {

class HttpChannelParent;

#define HTTP_CHANNEL_PARENT_LISTENER_IID \
  { 0xe409da52, 0xda76, 0x4eb7, \
    { 0xa7, 0xf4, 0x03, 0x3d, 0x88, 0xac, 0x87, 0x6d } }

// Note: nsIInterfaceRequestor must be the first base so that do_QueryObject()
// works correctly on this object, as it's needed to compute a void* pointing to
// the beginning of this object.

class HttpChannelParentListener final : public nsIInterfaceRequestor
                                      , public nsIChannelEventSink
                                      , public nsIRedirectResultListener
                                      , public nsIStreamListener
                                      , public nsINetworkInterceptController
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIREDIRECTRESULTLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSINETWORKINTERCEPTCONTROLLER

  NS_DECLARE_STATIC_IID_ACCESSOR(HTTP_CHANNEL_PARENT_LISTENER_IID)

  explicit HttpChannelParentListener(HttpChannelParent* aInitialChannel);

  // For channel diversion from child to parent.
  MOZ_MUST_USE nsresult DivertTo(nsIStreamListener *aListener);
  MOZ_MUST_USE nsresult SuspendForDiversion();

  void SetupInterception(const nsHttpResponseHead& aResponseHead);
  void SetupInterceptionAfterRedirect(bool aShouldIntercept);
  void ClearInterceptedChannel(nsIStreamListener* aListener);

private:
  virtual ~HttpChannelParentListener() = default;

  // Private partner function to SuspendForDiversion.
  MOZ_MUST_USE nsresult ResumeForDiversion();

  // Can be the original HttpChannelParent that created this object (normal
  // case), a different {HTTP|FTP}ChannelParent that we've been redirected to,
  // or some other listener that we have been diverted to via
  // nsIDivertableChannel.
  nsCOMPtr<nsIStreamListener> mNextListener;
  uint32_t mRedirectChannelId;
  // When set, no OnStart/OnData/OnStop calls should be received.
  bool mSuspendedForDiversion;

  // Set if this channel should be intercepted before it sets up the HTTP transaction.
  bool mShouldIntercept;
  // Set if this channel should suspend on interception.
  bool mShouldSuspendIntercept;
  // Set if the channel interception has been canceled.  Can be set before
  // interception first occurs.  In this case cancelation is deferred until
  // the interception takes place.
  bool mInterceptCanceled;

  nsAutoPtr<nsHttpResponseHead> mSynthesizedResponseHead;

  // Handle to the channel wrapper if this channel has been intercepted.
  nsCOMPtr<nsIInterceptedChannel> mInterceptedChannel;

  // This will be populated with a real network controller if parent-side
  // interception is enabled.
  nsCOMPtr<nsINetworkInterceptController> mInterceptController;
};

NS_DEFINE_STATIC_IID_ACCESSOR(HttpChannelParentListener,
                              HTTP_CHANNEL_PARENT_LISTENER_IID)

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpChannelParent_h
