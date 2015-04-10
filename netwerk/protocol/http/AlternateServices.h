/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
Alt-Svc allows separation of transport routing from the origin host without
using a proxy. See https://httpwg.github.io/http-extensions/alt-svc.html and
https://tools.ietf.org/html/draft-ietf-httpbis-alt-svc-06

 Nice To Have Future Enhancements::
 * flush on network change event when we have an indicator
 * use established https channel for http instead separate of conninfo hash
 * pin via http-tls header
 * clear based on origin when a random fail happens not just 421
 * upon establishment of channel, cancel and retry trans that have not yet written anything
 * persistent storage (including private browsing filter)
 * memory reporter for cache, but this is rather tiny
*/

#ifndef mozilla_net_AlternateServices_h
#define mozilla_net_AlternateServices_h

#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsISpeculativeConnect.h"

class nsProxyInfo;

namespace mozilla { namespace net {

class nsHttpConnectionInfo;

class AltSvcMapping
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AltSvcMapping)
  friend class AltSvcCache;

private: // ctor from ProcessHeader
  AltSvcMapping(const nsACString &originScheme,
                const nsACString &originHost,
                int32_t originPort,
                const nsACString &username,
                bool privateBrowsing,
                uint32_t expiresAt,
                const nsACString &alternateHost,
                int32_t alternatePort,
                const nsACString &npnToken);

public:
  static void ProcessHeader(const nsCString &buf, const nsCString &originScheme,
                            const nsCString &originHost, int32_t originPort,
                            const nsACString &username, bool privateBrowsing,
                            nsIInterfaceRequestor *callbacks, nsProxyInfo *proxyInfo,
                            uint32_t caps);

  const nsCString &AlternateHost() const { return mAlternateHost; }
  const nsCString &OriginHost() const { return mOriginHost; }
  const nsCString &HashKey() const { return mHashKey; }
  uint32_t AlternatePort() const { return mAlternatePort; }
  bool Validated() { return mValidated; }
  void SetValidated(bool val) { mValidated = val; }
  bool IsRunning() { return mRunning; }
  void SetRunning(bool val) { mRunning = val; }
  int32_t GetExpiresAt() { return mExpiresAt; }
  void SetExpiresAt(int32_t val) { mExpiresAt = val; }
  void SetExpired();
  bool RouteEquals(AltSvcMapping *map);

  void GetConnectionInfo(nsHttpConnectionInfo **outCI, nsProxyInfo *pi);
  int32_t TTL();

private:
  virtual ~AltSvcMapping() {};
  static void MakeHashKey(nsCString &outKey,
                          const nsACString &originScheme,
                          const nsACString &originHost,
                          int32_t originPort,
                          bool privateBrowsing);

  nsCString mHashKey;

  nsCString mAlternateHost;
  int32_t mAlternatePort;

  nsCString mOriginHost;
  int32_t mOriginPort;

  nsCString mUsername;
  bool mPrivate;

  uint32_t mExpiresAt;

  bool mValidated;
  bool mRunning;
  bool mHttps;

  nsCString mNPNToken;
};

class AltSvcOverride : public nsIInterfaceRequestor
                     , public nsISpeculativeConnectionOverrider
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISPECULATIVECONNECTIONOVERRIDER
    NS_DECL_NSIINTERFACEREQUESTOR

    explicit AltSvcOverride(nsIInterfaceRequestor *aRequestor)
      : mCallbacks(aRequestor) {}

private:
    virtual ~AltSvcOverride() {}
    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
};

class AltSvcCache
{
public:
  void UpdateAltServiceMapping(AltSvcMapping *map, nsProxyInfo *pi,
                               nsIInterfaceRequestor *, uint32_t caps); // main thread
  AltSvcMapping *GetAltServiceMapping(const nsACString &scheme,
                                      const nsACString &host,
                                      int32_t port, bool pb);
  void ClearAltServiceMappings();
  void ClearHostMapping(const nsACString &host, int32_t port);
  void ClearHostMapping(nsHttpConnectionInfo *ci);

private:
  nsRefPtrHashtable<nsCStringHashKey, AltSvcMapping> mHash;
};

}} // namespace mozilla::net

#endif // include guard
