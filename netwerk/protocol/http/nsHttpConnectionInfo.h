/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpConnectionInfo_h__
#define nsHttpConnectionInfo_h__

#include "nsHttp.h"
#include "nsProxyInfo.h"
#include "nsCOMPtr.h"
#include "nsStringFwd.h"
#include "mozilla/Logging.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/AlreadyAddRefed.h"
#include "ARefBase.h"
#include "nsIRequest.h"

//-----------------------------------------------------------------------------
// nsHttpConnectionInfo - holds the properties of a connection
//-----------------------------------------------------------------------------

// http:// uris through a proxy will all share the same CI, because they can
// all use the same connection. (modulo pb and anonymous flags). They just use
// the proxy as the origin host name.
// however, https:// uris tunnel through the proxy so they will have different
// CIs - the CI reflects both the proxy and the origin.
// however, proxy conenctions made with http/2 (or spdy) can tunnel to the
// origin and multiplex non tunneled transactions at the same time, so they have
// a special wildcard CI that accepts all origins through that proxy.

class nsISVCBRecord;

namespace mozilla {
namespace net {

extern LazyLogModule gHttpLog;
class HttpConnectionInfoCloneArgs;
class nsHttpTransaction;

class nsHttpConnectionInfo final : public ARefBase {
 public:
  nsHttpConnectionInfo(const nsACString& originHost, int32_t originPort,
                       const nsACString& npnToken, const nsACString& username,
                       nsProxyInfo* proxyInfo,
                       const OriginAttributes& originAttributes,
                       bool endToEndSSL = false, bool aIsHttp3 = false,
                       bool aWebTransport = false);

  // this version must use TLS and you may supply separate
  // connection (aka routing) information than the authenticated
  // origin information
  nsHttpConnectionInfo(const nsACString& originHost, int32_t originPort,
                       const nsACString& npnToken, const nsACString& username,
                       nsProxyInfo* proxyInfo,
                       const OriginAttributes& originAttributes,
                       const nsACString& routedHost, int32_t routedPort,
                       bool aIsHttp3, bool aWebTransport = false);

  static void SerializeHttpConnectionInfo(nsHttpConnectionInfo* aInfo,
                                          HttpConnectionInfoCloneArgs& aArgs);
  static already_AddRefed<nsHttpConnectionInfo>
  DeserializeHttpConnectionInfoCloneArgs(
      const HttpConnectionInfoCloneArgs& aInfoArgs);

 private:
  virtual ~nsHttpConnectionInfo() {
    MOZ_LOG(gHttpLog, LogLevel::Debug,
            ("Destroying nsHttpConnectionInfo @%p\n", this));
  }

  void BuildHashKey();
  void RebuildHashKey();

  // See comments in nsHttpConnectionInfo::BuildHashKey for the meaning of each
  // field.
  enum class HashKeyIndex : uint32_t {
    Proxy = 0,
    EndToEndSSL,
    Anonymous,
    Private,
    InsecureScheme,
    NoSpdy,
    BeConservative,
    AnonymousAllowClientCert,
    FallbackConnection,
    WebTransport,
    End,
  };
  constexpr inline auto UnderlyingIndex(HashKeyIndex aIndex) const {
    return std::underlying_type_t<HashKeyIndex>(aIndex);
  }

 public:
  const nsCString& HashKey() const { return mHashKey; }

  const nsCString& GetOrigin() const { return mOrigin; }
  const char* Origin() const { return mOrigin.get(); }
  int32_t OriginPort() const { return mOriginPort; }

  const nsCString& GetRoutedHost() const { return mRoutedHost; }
  const char* RoutedHost() const { return mRoutedHost.get(); }
  int32_t RoutedPort() const { return mRoutedPort; }

  // OK to treat these as an infalible allocation
  already_AddRefed<nsHttpConnectionInfo> Clone() const;
  // This main prupose of this function is to clone this connection info, but
  // replace mRoutedHost with SvcDomainName in the given SVCB record. Note that
  // if SvcParamKeyPort and SvcParamKeyAlpn are presented in the SVCB record,
  // mRoutedPort and mNPNToken will be replaced as well.
  already_AddRefed<nsHttpConnectionInfo> CloneAndAdoptHTTPSSVCRecord(
      nsISVCBRecord* aRecord) const;
  void CloneAsDirectRoute(nsHttpConnectionInfo** outCI);
  [[nodiscard]] nsresult CreateWildCard(nsHttpConnectionInfo** outParam);

  const char* ProxyHost() const {
    return mProxyInfo ? mProxyInfo->Host().get() : nullptr;
  }
  int32_t ProxyPort() const { return mProxyInfo ? mProxyInfo->Port() : -1; }
  const char* ProxyType() const {
    return mProxyInfo ? mProxyInfo->Type() : nullptr;
  }
  const char* ProxyUsername() const {
    return mProxyInfo ? mProxyInfo->Username().get() : nullptr;
  }
  const char* ProxyPassword() const {
    return mProxyInfo ? mProxyInfo->Password().get() : nullptr;
  }

  const nsCString& ProxyAuthorizationHeader() const {
    return mProxyInfo ? mProxyInfo->ProxyAuthorizationHeader() : EmptyCString();
  }
  const nsCString& ConnectionIsolationKey() const {
    return mProxyInfo ? mProxyInfo->ConnectionIsolationKey() : EmptyCString();
  }

  // Compare this connection info to another...
  // Two connections are 'equal' if they end up talking the same
  // protocol to the same server. This is needed to properly manage
  // persistent connections to proxies
  // Note that we don't care about transparent proxies -
  // it doesn't matter if we're talking via socks or not, since
  // a request will end up at the same host.
  bool Equals(const nsHttpConnectionInfo* info) {
    return mHashKey.Equals(info->HashKey());
  }

  const char* Username() const { return mUsername.get(); }
  nsProxyInfo* ProxyInfo() const { return mProxyInfo; }
  int32_t DefaultPort() const {
    return mEndToEndSSL ? NS_HTTPS_DEFAULT_PORT : NS_HTTP_DEFAULT_PORT;
  }
  void SetAnonymous(bool anon) {
    SetHashCharAt(anon ? 'A' : '.', HashKeyIndex::Anonymous);
  }
  bool GetAnonymous() const {
    return GetHashCharAt(HashKeyIndex::Anonymous) == 'A';
  }
  void SetPrivate(bool priv) {
    SetHashCharAt(priv ? 'P' : '.', HashKeyIndex::Private);
  }
  bool GetPrivate() const {
    return GetHashCharAt(HashKeyIndex::Private) == 'P';
  }
  void SetInsecureScheme(bool insecureScheme) {
    SetHashCharAt(insecureScheme ? 'I' : '.', HashKeyIndex::InsecureScheme);
  }
  bool GetInsecureScheme() const {
    return GetHashCharAt(HashKeyIndex::InsecureScheme) == 'I';
  }

  void SetNoSpdy(bool aNoSpdy) {
    SetHashCharAt(aNoSpdy ? 'X' : '.', HashKeyIndex::NoSpdy);
    if (aNoSpdy && mNPNToken == "h2"_ns) {
      mNPNToken.Truncate();
      RebuildHashKey();
    }
  }
  bool GetNoSpdy() const { return GetHashCharAt(HashKeyIndex::NoSpdy) == 'X'; }

  void SetBeConservative(bool aBeConservative) {
    SetHashCharAt(aBeConservative ? 'C' : '.', HashKeyIndex::BeConservative);
  }
  bool GetBeConservative() const {
    return GetHashCharAt(HashKeyIndex::BeConservative) == 'C';
  }

  void SetAnonymousAllowClientCert(bool anon) {
    SetHashCharAt(anon ? 'B' : '.', HashKeyIndex::AnonymousAllowClientCert);
  }
  bool GetAnonymousAllowClientCert() const {
    return GetHashCharAt(HashKeyIndex::AnonymousAllowClientCert) == 'B';
  }

  void SetFallbackConnection(bool aFallback) {
    SetHashCharAt(aFallback ? 'F' : '.', HashKeyIndex::FallbackConnection);
  }
  bool GetFallbackConnection() const {
    return GetHashCharAt(HashKeyIndex::FallbackConnection) == 'F';
  }

  void SetTlsFlags(uint32_t aTlsFlags);
  uint32_t GetTlsFlags() const { return mTlsFlags; }

  // IsTrrServiceChannel means that this connection is used to send TRR requests
  // over
  void SetIsTrrServiceChannel(bool aIsTRRChannel) {
    mIsTrrServiceChannel = aIsTRRChannel;
  }
  bool GetIsTrrServiceChannel() const { return mIsTrrServiceChannel; }

  void SetTRRMode(nsIRequest::TRRMode aTRRMode);
  nsIRequest::TRRMode GetTRRMode() const { return mTRRMode; }

  void SetIPv4Disabled(bool aNoIPv4);
  bool GetIPv4Disabled() const { return mIPv4Disabled; }

  void SetIPv6Disabled(bool aNoIPv6);
  bool GetIPv6Disabled() const { return mIPv6Disabled; }

  void SetWebTransport(bool aWebTransport);
  bool GetWebTransport() const { return mWebTransport; }

  void SetWebTransportId(uint64_t id);
  uint32_t GetWebTransportId() const { return mWebTransportId; };

  const nsCString& GetNPNToken() { return mNPNToken; }
  const nsCString& GetUsername() { return mUsername; }

  const OriginAttributes& GetOriginAttributes() { return mOriginAttributes; }

  // Returns true for any kind of proxy (http, socks, https, etc..)
  bool UsingProxy();

  // Returns true when proxying over HTTP or HTTPS
  bool UsingHttpProxy() const { return mUsingHttpProxy || mUsingHttpsProxy; }

  // Returns true when only proxying over HTTP
  bool UsingOnlyHttpProxy() const { return mUsingHttpProxy; }

  // Returns true when proxying over HTTPS
  bool UsingHttpsProxy() const { return mUsingHttpsProxy; }

  // Returns true when a resource is in SSL end to end (e.g. https:// uri)
  bool EndToEndSSL() const { return mEndToEndSSL; }

  // Returns true when at least first hop is SSL (e.g. proxy over https or https
  // uri)
  bool FirstHopSSL() const { return mEndToEndSSL || mUsingHttpsProxy; }

  // Returns true when CONNECT is used to tunnel through the proxy (e.g.
  // https:// or ws://)
  bool UsingConnect() const { return mUsingConnect; }

  // Returns true when origin/proxy is an RFC1918 literal.
  bool HostIsLocalIPLiteral() const;

  bool GetLessThanTls13() const { return mLessThanTls13; }
  void SetLessThanTls13(bool aLessThanTls13) {
    mLessThanTls13 = aLessThanTls13;
  }

  bool IsHttp3() const { return mIsHttp3; }

  void SetHasIPHintAddress(bool aHasIPHint) { mHasIPHintAddress = aHasIPHint; }
  bool HasIPHintAddress() const { return mHasIPHintAddress; }

  void SetEchConfig(const nsACString& aEchConfig) { mEchConfig = aEchConfig; }
  const nsCString& GetEchConfig() const { return mEchConfig; }

 private:
  void Init(const nsACString& host, int32_t port, const nsACString& npnToken,
            const nsACString& username, nsProxyInfo* proxyInfo,
            const OriginAttributes& originAttributes, bool e2eSSL,
            bool aIsHttp3, bool aWebTransport);
  void SetOriginServer(const nsACString& host, int32_t port);
  nsCString::char_type GetHashCharAt(HashKeyIndex aIndex) const {
    return mHashKey.CharAt(UnderlyingIndex(aIndex));
  }
  void SetHashCharAt(nsCString::char_type aValue, HashKeyIndex aIndex) {
    mHashKey.SetCharAt(aValue, UnderlyingIndex(aIndex));
  }

  nsCString mOrigin;
  int32_t mOriginPort = 0;
  nsCString mRoutedHost;
  int32_t mRoutedPort;

  nsCString mHashKey;
  nsCString mUsername;
  nsCOMPtr<nsProxyInfo> mProxyInfo;
  bool mUsingHttpProxy = false;
  bool mUsingHttpsProxy = false;
  bool mEndToEndSSL = false;
  // if will use CONNECT with http proxy
  bool mUsingConnect = false;
  nsCString mNPNToken;
  OriginAttributes mOriginAttributes;
  nsIRequest::TRRMode mTRRMode;

  uint32_t mTlsFlags = 0;
  uint16_t mIsTrrServiceChannel : 1;
  uint16_t mIPv4Disabled : 1;
  uint16_t mIPv6Disabled : 1;

  bool mLessThanTls13;  // This will be set to true if we negotiate less than
                        // tls1.3. If the tls version is till not know or it
                        // is 1.3 or greater the value will be false.
  bool mIsHttp3 = false;
  bool mWebTransport = false;

  bool mHasIPHintAddress = false;
  nsCString mEchConfig;

  uint64_t mWebTransportId = 0;  // current dedicated Id only used for
                                 // Webtransport, zero means not dedicated

  // for RefPtr
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsHttpConnectionInfo, override)
};

}  // namespace net
}  // namespace mozilla

#endif  // nsHttpConnectionInfo_h__
