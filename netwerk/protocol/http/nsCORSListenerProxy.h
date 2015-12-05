/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCORSListenerProxy_h__
#define nsCORSListenerProxy_h__

#include "nsIStreamListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsTArray.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "mozilla/Attributes.h"

class nsIURI;
class nsIPrincipal;
class nsINetworkInterceptController;
class nsICorsPreflightCallback;

namespace mozilla {
namespace net {
class HttpChannelParent;
class nsHttpChannel;
}
}

enum class DataURIHandling
{
  Allow,
  Disallow
};

class nsCORSListenerProxy final : public nsIStreamListener,
                                  public nsIInterfaceRequestor,
                                  public nsIChannelEventSink,
                                  public nsIThreadRetargetableStreamListener
{
public:
  nsCORSListenerProxy(nsIStreamListener* aOuter,
                      nsIPrincipal* aRequestingPrincipal,
                      bool aWithCredentials);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  // Must be called at startup.
  static void Startup();

  static void Shutdown();

  nsresult Init(nsIChannel* aChannel, DataURIHandling aAllowDataURI);

  void SetInterceptController(nsINetworkInterceptController* aInterceptController);

private:
  // Only HttpChannelParent can call RemoveFromCorsPreflightCache
  friend class mozilla::net::HttpChannelParent;
  // Only nsHttpChannel can invoke CORS preflights
  friend class mozilla::net::nsHttpChannel;

  static void RemoveFromCorsPreflightCache(nsIURI* aURI,
                                           nsIPrincipal* aRequestingPrincipal);
  static nsresult StartCORSPreflight(nsIChannel* aRequestChannel,
                                     nsICorsPreflightCallback* aCallback,
                                     nsTArray<nsCString>& aACUnsafeHeaders,
                                     nsIChannel** aPreflightChannel);

  ~nsCORSListenerProxy();

  nsresult UpdateChannel(nsIChannel* aChannel, DataURIHandling aAllowDataURI);
  nsresult CheckRequestApproved(nsIRequest* aRequest);

  nsCOMPtr<nsIStreamListener> mOuterListener;
  // The principal that originally kicked off the request
  nsCOMPtr<nsIPrincipal> mRequestingPrincipal;
  // The principal to use for our Origin header ("source origin" in spec terms).
  // This can get changed during redirects, unlike mRequestingPrincipal.
  nsCOMPtr<nsIPrincipal> mOriginHeaderPrincipal;
  nsCOMPtr<nsIInterfaceRequestor> mOuterNotificationCallbacks;
  nsCOMPtr<nsINetworkInterceptController> mInterceptController;
  bool mWithCredentials;
  bool mRequestApproved;
  // Please note that the member variable mHasBeenCrossSite may rely on the
  // promise that the CSP directive 'upgrade-insecure-requests' upgrades
  // an http: request to https: in nsHttpChannel::Connect() and hence
  // a request might not be marked as cross site request based on that promise.
  bool mHasBeenCrossSite;
#ifdef DEBUG
  bool mInited;
#endif
};

#endif
