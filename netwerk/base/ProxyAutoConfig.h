/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProxyAutoConfig_h__
#define ProxyAutoConfig_h__

#include <functional>
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

class nsIEventTarget;
class nsITimer;
class nsIThread;
namespace JS {
class CallArgs;
}  // namespace JS

namespace mozilla {
namespace net {

class JSContextWrapper;
class ProxyAutoConfigParent;
union NetAddr;

class ProxyAutoConfigBase {
 public:
  virtual ~ProxyAutoConfigBase() = default;
  virtual nsresult Init(nsIThread* aPACThread) { return NS_OK; }
  virtual nsresult ConfigurePAC(const nsACString& aPACURI,
                                const nsACString& aPACScriptData,
                                bool aIncludePath, uint32_t aExtraHeapSize,
                                nsIEventTarget* aEventTarget) = 0;
  virtual void SetThreadLocalIndex(uint32_t index) {}
  virtual void Shutdown() = 0;
  virtual void GC() = 0;
  virtual void GetProxyForURIWithCallback(
      const nsACString& aTestURI, const nsACString& aTestHost,
      std::function<void(nsresult aStatus, const nsACString& aResult)>&&
          aCallback) = 0;
};

// The ProxyAutoConfig class is meant to be created and run on a
// non main thread. It synchronously resolves PAC files by blocking that
// thread and running nested event loops. GetProxyForURI is not re-entrant.

class ProxyAutoConfig : public ProxyAutoConfigBase {
 public:
  ProxyAutoConfig();
  virtual ~ProxyAutoConfig();

  nsresult ConfigurePAC(const nsACString& aPACURI,
                        const nsACString& aPACScriptData, bool aIncludePath,
                        uint32_t aExtraHeapSize,
                        nsIEventTarget* aEventTarget) override;
  void SetThreadLocalIndex(uint32_t index) override;
  void Shutdown() override;
  void GC() override;
  bool MyIPAddress(const JS::CallArgs& aArgs);
  bool ResolveAddress(const nsACString& aHostName, NetAddr* aNetAddr,
                      unsigned int aTimeout);

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
  nsresult GetProxyForURI(const nsACString& aTestURI,
                          const nsACString& aTestHost, nsACString& result);

  void GetProxyForURIWithCallback(
      const nsACString& aTestURI, const nsACString& aTestHost,
      std::function<void(nsresult aStatus, const nsACString& aResult)>&&
          aCallback) override;

 private:
  // allow 665ms for myipaddress dns queries. That's 95th percentile.
  const static unsigned int kTimeout = 665;

  // used to compile the PAC file and setup the execution context
  nsresult SetupJS();

  bool SrcAddress(const NetAddr* remoteAddress, nsCString& localAddress);
  bool MyIPAddressTryHost(const nsACString& hostName, unsigned int timeout,
                          const JS::CallArgs& aArgs, bool* aResult);

  JSContextWrapper* mJSContext{nullptr};
  bool mJSNeedsSetup{false};
  bool mShutdown{true};
  nsCString mConcatenatedPACData;
  nsCString mPACURI;
  bool mIncludePath{false};
  uint32_t mExtraHeapSize{0};
  nsCString mRunningHost;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;
};

class RemoteProxyAutoConfig : public ProxyAutoConfigBase {
 public:
  RemoteProxyAutoConfig();
  virtual ~RemoteProxyAutoConfig();

  nsresult Init(nsIThread* aPACThread) override;
  nsresult ConfigurePAC(const nsACString& aPACURI,
                        const nsACString& aPACScriptData, bool aIncludePath,
                        uint32_t aExtraHeapSize,
                        nsIEventTarget* aEventTarget) override;
  void Shutdown() override;
  void GC() override;
  void GetProxyForURIWithCallback(
      const nsACString& aTestURI, const nsACString& aTestHost,
      std::function<void(nsresult aStatus, const nsACString& aResult)>&&
          aCallback) override;

 private:
  RefPtr<ProxyAutoConfigParent> mProxyAutoConfigParent;
};

}  // namespace net
}  // namespace mozilla

#endif  // ProxyAutoConfig_h__
