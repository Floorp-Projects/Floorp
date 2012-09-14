/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProxyAutoConfig_h__
#define ProxyAutoConfig_h__

#include "nsString.h"
#include "jsapi.h"
#include "prio.h"
#include "nsITimer.h"
#include "nsAutoPtr.h"

namespace mozilla { namespace net {

class JSRuntimeWrapper;

// The ProxyAutoConfig class is meant to be created and run on a
// non main thread. It synchronously resolves PAC files by blocking that
// thread and running nested event loops. GetProxyForURI is not re-entrant.

class ProxyAutoConfig  {
public:
  ProxyAutoConfig()
    : mJSRuntime(nullptr)
    , mJSNeedsSetup(false)
    , mShutdown(false)
  {
    MOZ_COUNT_CTOR(ProxyAutoConfig);
  }
  ~ProxyAutoConfig();

  nsresult Init(const nsCString &aPACURI,
                const nsCString &aPACScript);
  void     Shutdown();
  void     GC();
  bool     MyIPAddress(jsval *vp);
  bool     ResolveAddress(const nsCString &aHostName,
                          PRNetAddr *aNetAddr, unsigned int aTimeout);

  /**
   * Get the proxy string for the specified URI.  The proxy string is
   * given by the following:
   *
   *   result      = proxy-spec *( proxy-sep proxy-spec )
   *   proxy-spec  = direct-type | proxy-type LWS proxy-host [":" proxy-port]
   *   direct-type = "DIRECT"
   *   proxy-type  = "PROXY" | "SOCKS" | "SOCKS4" | "SOCKS5"
   *   proxy-sep   = ";" LWS
   *   proxy-host  = hostname | ipv4-address-literal
   *   proxy-port  = <any 16-bit unsigned integer>
   *   LWS         = *( SP | HT )
   *   SP          = <US-ASCII SP, space (32)>
   *   HT          = <US-ASCII HT, horizontal-tab (9)>
   *
   * NOTE: direct-type and proxy-type are case insensitive
   * NOTE: SOCKS implies SOCKS4
   *
   * Examples:
   *   "PROXY proxy1.foo.com:8080; PROXY proxy2.foo.com:8080; DIRECT"
   *   "SOCKS socksproxy"
   *   "DIRECT"
   *
   * XXX add support for IPv6 address literals.
   * XXX quote whatever the official standard is for PAC.
   *
   * @param aTestURI
   *        The URI as an ASCII string to test.
   * @param aTestHost
   *        The ASCII hostname to test.
   *
   * @param result
   *        result string as defined above.
   */
  nsresult GetProxyForURI(const nsCString &aTestURI,
                          const nsCString &aTestHost,
                          nsACString &result);

private:
  const static unsigned int kTimeout = 1000; // ms to allow for myipaddress dns queries

  // used to compile the PAC file and setup the execution context
  nsresult SetupJS();

  bool SrcAddress(const PRNetAddr *remoteAddress, nsCString &localAddress);
  bool MyIPAddressTryHost(const nsCString &hostName, unsigned int timeout,
                          jsval *vp);

  JSRuntimeWrapper *mJSRuntime;
  bool              mJSNeedsSetup;
  bool              mShutdown;
  nsCString         mPACScript;
  nsCString         mPACURI;
  nsCString         mRunningHost;
  nsCOMPtr<nsITimer> mTimer;
};

}} // namespace mozilla::net

#endif  // ProxyAutoConfig_h__
