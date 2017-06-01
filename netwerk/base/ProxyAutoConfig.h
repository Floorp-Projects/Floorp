/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProxyAutoConfig_h__
#define ProxyAutoConfig_h__

#include "nsString.h"
#include "nsCOMPtr.h"

class nsITimer;
namespace JS {
class CallArgs;
} // namespace JS

namespace mozilla { namespace net {

class JSContextWrapper;
union NetAddr;

// The ProxyAutoConfig class is meant to be created and run on a
// non main thread. It synchronously resolves PAC files by blocking that
// thread and running nested event loops. GetProxyForURI is not re-entrant.

class ProxyAutoConfig  {
public:
  ProxyAutoConfig();
  ~ProxyAutoConfig();

  nsresult Init(const nsCString &aPACURI,
                const nsCString &aPACScript,
                bool aIncludePath,
                uint32_t aExtraHeapSize);
  void     SetThreadLocalIndex(uint32_t index);
  void     Shutdown();
  void     GC();
  bool     MyIPAddress(const JS::CallArgs &aArgs);
  bool     ResolveAddress(const nsCString &aHostName,
                          NetAddr *aNetAddr, unsigned int aTimeout);

  /**
   * Get the proxy string for the specified URI.  The proxy string is
   * given by the following:
   *
   *   result      = proxy-spec *( proxy-sep proxy-spec )
   *   proxy-spec  = direct-type | proxy-type LWS proxy-host [":" proxy-port]
   *   direct-type = "DIRECT"
   *   proxy-type  = "PROXY" | "HTTP" | "HTTPS" | "SOCKS" | "SOCKS4" | "SOCKS5"
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
  // allow 665ms for myipaddress dns queries. That's 95th percentile.
  const static unsigned int kTimeout = 665;

  // used to compile the PAC file and setup the execution context
  nsresult SetupJS();

  bool SrcAddress(const NetAddr *remoteAddress, nsCString &localAddress);
  bool MyIPAddressTryHost(const nsCString &hostName, unsigned int timeout,
                          const JS::CallArgs &aArgs, bool* aResult);

  JSContextWrapper *mJSContext;
  bool              mJSNeedsSetup;
  bool              mShutdown;
  nsCString         mPACScript;
  nsCString         mPACURI;
  bool              mIncludePath;
  uint32_t          mExtraHeapSize;
  nsCString         mRunningHost;
  nsCOMPtr<nsITimer> mTimer;
};

} // namespace net
} // namespace mozilla

#endif  // ProxyAutoConfig_h__
