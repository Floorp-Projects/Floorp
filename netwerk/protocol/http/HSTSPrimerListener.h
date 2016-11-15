/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef HSTSPrimingListener_h__
#define HSTSPrimingListener_h__

#include "nsCOMPtr.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"

#include "mozilla/Attributes.h"

class nsIPrincipal;
class nsINetworkInterceptController;
class nsIHstsPrimingCallback;

namespace mozilla {
namespace net {

class HttpChannelParent;
class nsHttpChannel;

/*
 * How often do we get back an HSTS priming result which upgrades the connection to HTTPS?
 */
enum HSTSPrimingResult {
  // This site has been seen before and won't be upgraded
  eHSTS_PRIMING_CACHED_NO_UPGRADE = 0,
  // This site has been seen before and will be upgraded
  eHSTS_PRIMING_CACHED_DO_UPGRADE = 1,
  // This site has been seen before and will be blocked
  eHSTS_PRIMING_CACHED_BLOCK      = 2,
  // The request was already upgraded, probably through
  // upgrade-insecure-requests
  eHSTS_PRIMING_ALREADY_UPGRADED  = 3,
  // HSTS priming is successful and the connection will be upgraded to HTTPS
  eHSTS_PRIMING_SUCCEEDED         = 4,
  // When priming succeeds, but preferences require preservation of the order
  // of mixed-content and hsts, and mixed-content blocks the load
  eHSTS_PRIMING_SUCCEEDED_BLOCK   = 5,
  // When priming succeeds, but preferences require preservation of the order
  // of mixed-content and hsts, and mixed-content allows the load over http
  eHSTS_PRIMING_SUCCEEDED_HTTP    = 6,
  // HSTS priming failed, and the load is blocked by mixed-content
  eHSTS_PRIMING_FAILED_BLOCK      = 7,
  // HSTS priming failed, and the load is allowed by mixed-content
  eHSTS_PRIMING_FAILED_ACCEPT     = 8,
  // The HSTS Priming request timed out, and the load is blocked by
  // mixed-content
  eHSTS_PRIMING_TIMEOUT_BLOCK     = 9,
  // The HSTS Priming request timed out, and the load is allowed by
  // mixed-content
  eHSTS_PRIMING_TIMEOUT_ACCEPT    = 10
};

//////////////////////////////////////////////////////////////////////////
// Class used as streamlistener and notification callback when
// doing the HEAD request for an HSTS Priming check. Needs to be an
// nsIStreamListener in order to receive events from AsyncOpen2
class HSTSPrimingListener final : public nsIStreamListener,
                                  public nsIInterfaceRequestor,
                                  public nsITimerCallback
{
public:
  explicit HSTSPrimingListener(nsIHstsPrimingCallback* aCallback);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSITIMERCALLBACK

private:
  ~HSTSPrimingListener() {}

  // Only nsHttpChannel can invoke HSTS priming
  friend class mozilla::net::nsHttpChannel;

  /**
   * Start the HSTS priming request. This will send an anonymous HEAD request to
   * the URI aRequestChannel is attempting to load. On success, the new HSTS
   * priming channel is allocated in aHSTSPrimingChannel.
   *
   * @param aRequestChannel the reference channel used to initialze the HSTS
   *        priming channel
   * @param aCallback the callback stored to handle the results of HSTS priming.
   * @param aHSTSPrimingChannel if the new HSTS priming channel is allocated
   *        successfully, it will be placed here.
   */
  static nsresult StartHSTSPriming(nsIChannel* aRequestChannel,
                                   nsIHstsPrimingCallback* aCallback);

  /**
   * Given a request, return NS_OK if it has resulted in a cached HSTS update.
   * We don't need to check for the header as that has already been done for us.
   */
  nsresult CheckHSTSPrimingRequestStatus(nsIRequest* aRequest);

  // send telemetry about how long HSTS priming requests take
  void ReportTiming(nsresult aResult);

  /**
   * the nsIHttpChannel to notify with the result of HSTS priming.
   */
  nsCOMPtr<nsIHstsPrimingCallback> mCallback;

  /**
   * Keep a handle to the priming channel so we can cancel it on timeout
   */
  nsCOMPtr<nsIChannel> mPrimingChannel;

  /**
   * Keep a handle to the timer around so it can be canceled if we don't time
   * out.
   */
  nsCOMPtr<nsITimer> mHSTSPrimingTimer;

  /**
   * How long (in ms) before an HSTS Priming channel times out.
   * Preference: security.mixed_content.hsts_priming_request_timeout
   */
  static uint32_t sHSTSPrimingTimeout;
};


}} // mozilla::net

#endif // HSTSPrimingListener_h__
