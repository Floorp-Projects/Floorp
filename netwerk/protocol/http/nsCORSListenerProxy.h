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
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"

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

enum class UpdateType
{
  Default,
  InternalOrHSTSRedirect
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

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  // Must be called at startup.
  static void Startup();

  static void Shutdown();

  MOZ_MUST_USE nsresult Init(nsIChannel* aChannel,
                             DataURIHandling aAllowDataURI);

  void SetInterceptController(nsINetworkInterceptController* aInterceptController);

  // When CORS blocks a request, log the message to the web console, or the
  // browser console if no valid inner window ID is found.
  static void LogBlockedCORSRequest(uint64_t aInnerWindowID,
                                    const nsAString& aMessage);
private:
  // Only HttpChannelParent can call RemoveFromCorsPreflightCache
  friend class mozilla::net::HttpChannelParent;
  // Only nsHttpChannel can invoke CORS preflights
  friend class mozilla::net::nsHttpChannel;

  static void RemoveFromCorsPreflightCache(nsIURI* aURI,
                                           nsIPrincipal* aRequestingPrincipal);
  static MOZ_MUST_USE nsresult
  StartCORSPreflight(nsIChannel* aRequestChannel,
                     nsICorsPreflightCallback* aCallback,
                     nsTArray<nsCString>& aACUnsafeHeaders,
                     nsIChannel** aPreflightChannel);

  ~nsCORSListenerProxy();

  MOZ_MUST_USE nsresult UpdateChannel(nsIChannel* aChannel,
                                      DataURIHandling aAllowDataURI,
                                      UpdateType aUpdateType);
  MOZ_MUST_USE nsresult CheckRequestApproved(nsIRequest* aRequest);
  MOZ_MUST_USE nsresult CheckPreflightNeeded(nsIChannel* aChannel,
                                             UpdateType aUpdateType);

  nsCOMPtr<nsIStreamListener> mOuterListener;
  // The principal that originally kicked off the request
  nsCOMPtr<nsIPrincipal> mRequestingPrincipal;
  // The principal to use for our Origin header ("source origin" in spec terms).
  // This can get changed during redirects, unlike mRequestingPrincipal.
  nsCOMPtr<nsIPrincipal> mOriginHeaderPrincipal;
  nsCOMPtr<nsIInterfaceRequestor> mOuterNotificationCallbacks;
  nsCOMPtr<nsINetworkInterceptController> mInterceptController;
  bool mWithCredentials;
  mozilla::Atomic<bool, mozilla::Relaxed> mRequestApproved;
  // Please note that the member variable mHasBeenCrossSite may rely on the
  // promise that the CSP directive 'upgrade-insecure-requests' upgrades
  // an http: request to https: in nsHttpChannel::Connect() and hence
  // a request might not be marked as cross site request based on that promise.
  bool mHasBeenCrossSite;
  // Under e10s, logging happens in the child process. Keep a reference to the
  // creator nsIHttpChannel in order to find the way back to the child. Released
  // in OnStopRequest().
  nsCOMPtr<nsIHttpChannel> mHttpChannel;
#ifdef DEBUG
  bool mInited;
#endif

  // only locking mOuterListener, because it can be used on different threads.
  // We guarantee that OnStartRequest, OnDataAvailable and OnStopReques will be
  // called in order, but to make tsan happy we will lock mOuterListener.
  mutable mozilla::Mutex mMutex;
};

#endif
