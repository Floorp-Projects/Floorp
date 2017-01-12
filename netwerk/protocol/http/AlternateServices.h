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

#include "mozilla/DataStorage.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsISpeculativeConnect.h"
#include "mozilla/BasePrincipal.h"

class nsILoadInfo;

namespace mozilla { namespace net {

class nsProxyInfo;
class nsHttpConnectionInfo;
class nsHttpTransaction;
class nsHttpChannel;
class WellKnownChecker;

class AltSvcMapping
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AltSvcMapping)

private: // ctor from ProcessHeader
  AltSvcMapping(DataStorage *storage,
                int32_t storageEpoch,
                const nsACString &originScheme,
                const nsACString &originHost,
                int32_t originPort,
                const nsACString &username,
                bool privateBrowsing,
                uint32_t expiresAt,
                const nsACString &alternateHost,
                int32_t alternatePort,
                const nsACString &npnToken);
public:
  AltSvcMapping(DataStorage *storage, int32_t storageEpoch, const nsCString &serialized);

  static void ProcessHeader(const nsCString &buf, const nsCString &originScheme,
                            const nsCString &originHost, int32_t originPort,
                            const nsACString &username, bool privateBrowsing,
                            nsIInterfaceRequestor *callbacks, nsProxyInfo *proxyInfo,
                            uint32_t caps, const OriginAttributes &originAttributes);

  const nsCString &AlternateHost() const { return mAlternateHost; }
  const nsCString &OriginHost() const { return mOriginHost; }
  uint32_t OriginPort() const { return mOriginPort; }
  const nsCString &HashKey() const { return mHashKey; }
  uint32_t AlternatePort() const { return mAlternatePort; }
  bool Validated() { return mValidated; }
  int32_t GetExpiresAt() { return mExpiresAt; }
  bool RouteEquals(AltSvcMapping *map);
  bool HTTPS() { return mHttps; }

  void GetConnectionInfo(nsHttpConnectionInfo **outCI, nsProxyInfo *pi,
                         const OriginAttributes &originAttributes);

  int32_t TTL();
  int32_t StorageEpoch() { return mStorageEpoch; }
  bool    Private() { return mPrivate; }

  void SetValidated(bool val);
  void SetMixedScheme(bool val);
  void SetExpiresAt(int32_t val);
  void SetExpired();
  void Sync();

  static void MakeHashKey(nsCString &outKey,
                          const nsACString &originScheme,
                          const nsACString &originHost,
                          int32_t originPort,
                          bool privateBrowsing);

private:
  virtual ~AltSvcMapping() {};
  void     SyncString(const nsCString& val);
  RefPtr<DataStorage> mStorage;
  int32_t             mStorageEpoch;
  void Serialize (nsCString &out);

  nsCString mHashKey;

  // If you change any of these members, update Serialize()
  nsCString mAlternateHost;
  MOZ_INIT_OUTSIDE_CTOR int32_t mAlternatePort;

  nsCString mOriginHost;
  MOZ_INIT_OUTSIDE_CTOR int32_t mOriginPort;

  nsCString mUsername;
  MOZ_INIT_OUTSIDE_CTOR bool mPrivate;

  MOZ_INIT_OUTSIDE_CTOR uint32_t mExpiresAt; // alt-svc mappping

  MOZ_INIT_OUTSIDE_CTOR bool mValidated;
  MOZ_INIT_OUTSIDE_CTOR bool mHttps; // origin is https://
  MOZ_INIT_OUTSIDE_CTOR bool mMixedScheme; // .wk allows http and https on same con

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

class TransactionObserver : public nsIStreamListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  TransactionObserver(nsHttpChannel *channel, WellKnownChecker *checker);
  void Complete(nsHttpTransaction *, nsresult);
private:
  friend class WellKnownChecker;
  virtual ~TransactionObserver() {}

  nsCOMPtr<nsISupports> mChannelRef;
  nsHttpChannel        *mChannel;
  WellKnownChecker     *mChecker;
  nsCString             mWKResponse;

  bool mRanOnce;
  bool mAuthOK; // confirmed no TLS failure
  bool mVersionOK; // connection h2
  bool mStatusOK; // HTTP Status 200
};

class AltSvcCache
{
public:
  AltSvcCache() : mStorageEpoch(0) {}
  virtual ~AltSvcCache () {};
  void UpdateAltServiceMapping(AltSvcMapping *map, nsProxyInfo *pi,
                               nsIInterfaceRequestor *, uint32_t caps,
                               const OriginAttributes &originAttributes); // main thread
  already_AddRefed<AltSvcMapping> GetAltServiceMapping(const nsACString &scheme,
                                                       const nsACString &host,
                                                       int32_t port, bool pb);
  void ClearAltServiceMappings();
  void ClearHostMapping(const nsACString &host, int32_t port);
  void ClearHostMapping(nsHttpConnectionInfo *ci);
  DataStorage *GetStoragePtr() { return mStorage.get(); }
  int32_t      StorageEpoch()  { return mStorageEpoch; }

private:
  already_AddRefed<AltSvcMapping> LookupMapping(const nsCString &key, bool privateBrowsing);
  RefPtr<DataStorage>             mStorage;
  int32_t                         mStorageEpoch;
};

} // namespace net
} // namespace mozilla

#endif // include guard
