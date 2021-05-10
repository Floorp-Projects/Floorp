/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

#include "nsHttpConnectionInfo.h"

#include "mozilla/net/DNS.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "nsComponentManagerUtils.h"
#include "nsICryptoHash.h"
#include "nsIDNSByTypeRecord.h"
#include "nsIProtocolProxyService.h"
#include "nsHttpHandler.h"
#include "nsNetCID.h"
#include "nsProxyInfo.h"
#include "prnetdb.h"

static nsresult SHA256(const char* aPlainText, nsAutoCString& aResult) {
  nsresult rv;
  nsCOMPtr<nsICryptoHash> hasher =
      do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    LOG(("nsHttpDigestAuth: no crypto hash!\n"));
    return rv;
  }
  rv = hasher->Init(nsICryptoHash::SHA256);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = hasher->Update((unsigned char*)aPlainText, strlen(aPlainText));
  NS_ENSURE_SUCCESS(rv, rv);
  return hasher->Finish(false, aResult);
}

namespace mozilla {
namespace net {

nsHttpConnectionInfo::nsHttpConnectionInfo(
    const nsACString& originHost, int32_t originPort,
    const nsACString& npnToken, const nsACString& username,
    nsProxyInfo* proxyInfo, const OriginAttributes& originAttributes,
    bool endToEndSSL, bool aIsHttp3)
    : mRoutedPort(443), mLessThanTls13(false) {
  Init(originHost, originPort, npnToken, username, proxyInfo, originAttributes,
       endToEndSSL, aIsHttp3);
}

nsHttpConnectionInfo::nsHttpConnectionInfo(
    const nsACString& originHost, int32_t originPort,
    const nsACString& npnToken, const nsACString& username,
    nsProxyInfo* proxyInfo, const OriginAttributes& originAttributes,
    const nsACString& routedHost, int32_t routedPort, bool aIsHttp3)
    : mLessThanTls13(false) {
  mEndToEndSSL = true;  // so DefaultPort() works
  mRoutedPort = routedPort == -1 ? DefaultPort() : routedPort;

  if (!originHost.Equals(routedHost) || (originPort != routedPort) ||
      aIsHttp3) {
    mRoutedHost = routedHost;
  }
  Init(originHost, originPort, npnToken, username, proxyInfo, originAttributes,
       true, aIsHttp3);
}

void nsHttpConnectionInfo::Init(const nsACString& host, int32_t port,
                                const nsACString& npnToken,
                                const nsACString& username,
                                nsProxyInfo* proxyInfo,
                                const OriginAttributes& originAttributes,
                                bool e2eSSL, bool aIsHttp3) {
  LOG(("Init nsHttpConnectionInfo @%p\n", this));

  mUsername = username;
  mProxyInfo = proxyInfo;
  mEndToEndSSL = e2eSSL;
  mUsingConnect = false;
  mNPNToken = npnToken;
  mIsHttp3 = aIsHttp3;
  mOriginAttributes = originAttributes;
  mTlsFlags = 0x0;
  mIsTrrServiceChannel = false;
  mTRRMode = nsIRequest::TRR_DEFAULT_MODE;
  mIPv4Disabled = false;
  mIPv6Disabled = false;
  mHasIPHintAddress = false;

  mUsingHttpsProxy = (proxyInfo && proxyInfo->IsHTTPS());
  mUsingHttpProxy = mUsingHttpsProxy || (proxyInfo && proxyInfo->IsHTTP());

  if (mUsingHttpProxy) {
    mUsingConnect = mEndToEndSSL;  // SSL always uses CONNECT
    uint32_t resolveFlags = 0;
    if (NS_SUCCEEDED(mProxyInfo->GetResolveFlags(&resolveFlags)) &&
        resolveFlags & nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL) {
      mUsingConnect = true;
    }
  }

  SetOriginServer(host, port);
}

void nsHttpConnectionInfo::BuildHashKey() {
  //
  // build hash key:
  //
  // the hash key uniquely identifies the connection type.  two connections
  // are "equal" if they end up talking the same protocol to the same server
  // and are both used for anonymous or non-anonymous connection only;
  // anonymity of the connection is setup later from nsHttpChannel::AsyncOpen
  // where we know we use anonymous connection (LOAD_ANONYMOUS load flag)
  //

  const char* keyHost;
  int32_t keyPort;

  if (mUsingHttpProxy && !mUsingConnect) {
    keyHost = ProxyHost();
    keyPort = ProxyPort();
  } else {
    keyHost = Origin();
    keyPort = OriginPort();
  }

  // The hashkey has 4 fields followed by host connection info
  // byte 0 is P/T/. {P,T} for Plaintext/TLS Proxy over HTTP
  // byte 1 is S/. S is for end to end ssl such as https:// uris
  // byte 2 is A/. A is for an anonymous channel (no cookies, etc..)
  // byte 3 is P/. P is for a private browising channel
  // byte 4 is I/. I is for insecure scheme on TLS for http:// uris
  // byte 5 is X/. X is for disallow_spdy flag
  // byte 6 is C/. C is for be Conservative
  // byte 7 is B/. B is for allowing client certs on an anonymous channel
  // Note: when adding/removing fields from this list which do not have
  // corresponding data fields on the object itself, you may also need to
  // modify RebuildHashKey.

  mHashKey.AssignLiteral("........[tlsflags0x00000000]");

  mHashKey.Append(keyHost);
  mHashKey.Append(':');
  mHashKey.AppendInt(keyPort);
  if (!mUsername.IsEmpty()) {
    mHashKey.Append('[');
    mHashKey.Append(mUsername);
    mHashKey.Append(']');
  }

  if (mUsingHttpsProxy) {
    mHashKey.SetCharAt('T', 0);
  } else if (mUsingHttpProxy) {
    mHashKey.SetCharAt('P', 0);
  }
  if (mEndToEndSSL) {
    mHashKey.SetCharAt('S', 1);
  }

  // NOTE: for transparent proxies (e.g., SOCKS) we need to encode the proxy
  // info in the hash key (this ensures that we will continue to speak the
  // right protocol even if our proxy preferences change).
  //
  // NOTE: for SSL tunnels add the proxy information to the cache key.
  // We cannot use the proxy as the host parameter (as we do for non SSL)
  // because this is a single host tunnel, but we need to include the proxy
  // information so that a change in proxy config will mean this connection
  // is not reused

  // NOTE: Adding the username and the password provides a means to isolate
  // keep-alive to the URL bar domain as well: If the username is the URL bar
  // domain, keep-alive connections are not reused by resources bound to
  // different URL bar domains as the respective hash keys are not matching.

  if ((!mUsingHttpProxy && ProxyHost()) || (mUsingHttpProxy && mUsingConnect)) {
    mHashKey.AppendLiteral(" (");
    mHashKey.Append(ProxyType());
    mHashKey.Append(':');
    mHashKey.Append(ProxyHost());
    mHashKey.Append(':');
    mHashKey.AppendInt(ProxyPort());
    mHashKey.Append(')');
    mHashKey.Append('[');
    mHashKey.Append(ProxyUsername());
    mHashKey.Append(':');
    const char* password = ProxyPassword();
    if (strlen(password) > 0) {
      nsAutoCString digestedPassword;
      nsresult rv = SHA256(password, digestedPassword);
      if (rv == NS_OK) {
        mHashKey.Append(digestedPassword);
      }
    }
    mHashKey.Append(']');
  }

  if (!mRoutedHost.IsEmpty()) {
    mHashKey.AppendLiteral(" <ROUTE-via ");
    mHashKey.Append(mRoutedHost);
    mHashKey.Append(':');
    mHashKey.AppendInt(mRoutedPort);
    mHashKey.Append('>');
  }

  if (!mNPNToken.IsEmpty()) {
    mHashKey.AppendLiteral(" {NPN-TOKEN ");
    mHashKey.Append(mNPNToken);
    mHashKey.AppendLiteral("}");
  }

  if (GetTRRMode() != nsIRequest::TRR_DEFAULT_MODE) {
    // When connecting with another TRR mode, we enforce a separate connection
    // hashkey so that we also can trigger a fresh DNS resolver that then
    // doesn't use TRR as the previous connection might have.
    mHashKey.AppendLiteral("[TRR:");
    mHashKey.AppendInt(GetTRRMode());
    mHashKey.AppendLiteral("]");
  }

  if (GetIPv4Disabled()) {
    mHashKey.AppendLiteral("[!v4]");
  }

  if (GetIPv6Disabled()) {
    mHashKey.AppendLiteral("[!v6]");
  }

  if (mProxyInfo) {
    const nsCString& connectionIsolationKey =
        mProxyInfo->ConnectionIsolationKey();
    if (!connectionIsolationKey.IsEmpty()) {
      mHashKey.AppendLiteral("{CIK ");
      mHashKey.Append(connectionIsolationKey);
      mHashKey.AppendLiteral("}");
    }
    if (mProxyInfo->Flags() & nsIProxyInfo::TRANSPARENT_PROXY_RESOLVES_HOST) {
      mHashKey.AppendLiteral("{TPRH}");
    }
  }

  nsAutoCString originAttributes;
  mOriginAttributes.CreateSuffix(originAttributes);
  mHashKey.Append(originAttributes);
}

void nsHttpConnectionInfo::RebuildHashKey() {
  // Create copies of all properties stored in our hash key.
  bool isAnonymous = GetAnonymous();
  bool isPrivate = GetPrivate();
  bool isInsecureScheme = GetInsecureScheme();
  bool isNoSpdy = GetNoSpdy();
  bool isBeConservative = GetBeConservative();
  bool isAnonymousAllowClientCert = GetAnonymousAllowClientCert();

  BuildHashKey();

  // Restore all of those properties.
  SetAnonymous(isAnonymous);
  SetPrivate(isPrivate);
  SetInsecureScheme(isInsecureScheme);
  SetNoSpdy(isNoSpdy);
  SetBeConservative(isBeConservative);
  SetAnonymousAllowClientCert(isAnonymousAllowClientCert);
}

void nsHttpConnectionInfo::SetOriginServer(const nsACString& host,
                                           int32_t port) {
  mOrigin = host;
  mOriginPort = port == -1 ? DefaultPort() : port;
  // Use BuildHashKey() since this can only be called when constructing an
  // nsHttpConnectionInfo object.
  MOZ_DIAGNOSTIC_ASSERT(mHashKey.IsEmpty());
  BuildHashKey();
}

// Note that this function needs to be synced with
// nsHttpConnectionInfo::DeserializeHttpConnectionInfoCloneArgs to make sure
// nsHttpConnectionInfo can be serialized/deserialized.
already_AddRefed<nsHttpConnectionInfo> nsHttpConnectionInfo::Clone() const {
  RefPtr<nsHttpConnectionInfo> clone;
  if (mRoutedHost.IsEmpty()) {
    clone = new nsHttpConnectionInfo(mOrigin, mOriginPort, mNPNToken, mUsername,
                                     mProxyInfo, mOriginAttributes,
                                     mEndToEndSSL, mIsHttp3);
  } else {
    MOZ_ASSERT(mEndToEndSSL);
    clone = new nsHttpConnectionInfo(mOrigin, mOriginPort, mNPNToken, mUsername,
                                     mProxyInfo, mOriginAttributes, mRoutedHost,
                                     mRoutedPort, mIsHttp3);
  }

  // Make sure the anonymous, insecure-scheme, and private flags are transferred
  clone->SetAnonymous(GetAnonymous());
  clone->SetPrivate(GetPrivate());
  clone->SetInsecureScheme(GetInsecureScheme());
  clone->SetNoSpdy(GetNoSpdy());
  clone->SetBeConservative(GetBeConservative());
  clone->SetAnonymousAllowClientCert(GetAnonymousAllowClientCert());
  clone->SetTlsFlags(GetTlsFlags());
  clone->SetIsTrrServiceChannel(GetIsTrrServiceChannel());
  clone->SetTRRMode(GetTRRMode());
  clone->SetIPv4Disabled(GetIPv4Disabled());
  clone->SetIPv6Disabled(GetIPv6Disabled());
  clone->SetHasIPHintAddress(HasIPHintAddress());
  clone->SetEchConfig(GetEchConfig());
  MOZ_ASSERT(clone->Equals(this));

  return clone.forget();
}

already_AddRefed<nsHttpConnectionInfo>
nsHttpConnectionInfo::CloneAndAdoptHTTPSSVCRecord(
    nsISVCBRecord* aRecord) const {
  MOZ_ASSERT(aRecord);

  // Get the domain name of this HTTPS RR. This name will be assigned to
  // mRoutedHost in the new connection info.
  nsAutoCString name;
  aRecord->GetName(name);

  // Try to get the port and Alpn. If this record has SvcParamKeyPort defined,
  // the new port will be used as mRoutedPort.
  Maybe<uint16_t> port = aRecord->GetPort();
  Maybe<Tuple<nsCString, bool>> alpn = aRecord->GetAlpn();

  // Let the new conn info learn h3 will be used.
  bool isHttp3 = alpn ? Get<1>(*alpn) : false;

  LOG(("HTTPSSVC: use new routed host (%s) and new npnToken (%s)", name.get(),
       alpn ? Get<0>(*alpn).get() : "None"));

  RefPtr<nsHttpConnectionInfo> clone;
  if (name.IsEmpty()) {
    clone = new nsHttpConnectionInfo(
        mOrigin, mOriginPort, alpn ? Get<0>(*alpn) : EmptyCString(), mUsername,
        mProxyInfo, mOriginAttributes, mEndToEndSSL, isHttp3);
  } else {
    MOZ_ASSERT(mEndToEndSSL);
    clone = new nsHttpConnectionInfo(mOrigin, mOriginPort,
                                     alpn ? Get<0>(*alpn) : EmptyCString(),
                                     mUsername, mProxyInfo, mOriginAttributes,
                                     name, port ? *port : mOriginPort, isHttp3);
  }

  // Make sure the anonymous, insecure-scheme, and private flags are transferred
  clone->SetAnonymous(GetAnonymous());
  clone->SetPrivate(GetPrivate());
  clone->SetInsecureScheme(GetInsecureScheme());
  clone->SetNoSpdy(GetNoSpdy());
  clone->SetBeConservative(GetBeConservative());
  clone->SetAnonymousAllowClientCert(GetAnonymousAllowClientCert());
  clone->SetTlsFlags(GetTlsFlags());
  clone->SetIsTrrServiceChannel(GetIsTrrServiceChannel());
  clone->SetTRRMode(GetTRRMode());
  clone->SetIPv4Disabled(GetIPv4Disabled());
  clone->SetIPv6Disabled(GetIPv6Disabled());

  bool hasIPHint = false;
  Unused << aRecord->GetHasIPHintAddress(&hasIPHint);
  if (hasIPHint) {
    clone->SetHasIPHintAddress(hasIPHint);
  }

  nsAutoCString echConfig;
  Unused << aRecord->GetEchConfig(echConfig);
  clone->SetEchConfig(echConfig);

  return clone.forget();
}

/* static */
void nsHttpConnectionInfo::SerializeHttpConnectionInfo(
    nsHttpConnectionInfo* aInfo, HttpConnectionInfoCloneArgs& aArgs) {
  aArgs.host() = aInfo->GetOrigin();
  aArgs.port() = aInfo->OriginPort();
  aArgs.npnToken() = aInfo->GetNPNToken();
  aArgs.username() = aInfo->GetUsername();
  aArgs.originAttributes() = aInfo->GetOriginAttributes();
  aArgs.endToEndSSL() = aInfo->EndToEndSSL();
  aArgs.routedHost() = aInfo->GetRoutedHost();
  aArgs.routedPort() = aInfo->RoutedPort();
  aArgs.anonymous() = aInfo->GetAnonymous();
  aArgs.aPrivate() = aInfo->GetPrivate();
  aArgs.insecureScheme() = aInfo->GetInsecureScheme();
  aArgs.noSpdy() = aInfo->GetNoSpdy();
  aArgs.beConservative() = aInfo->GetBeConservative();
  aArgs.anonymousAllowClientCert() = aInfo->GetAnonymousAllowClientCert();
  aArgs.tlsFlags() = aInfo->GetTlsFlags();
  aArgs.isTrrServiceChannel() = aInfo->GetTRRMode();
  aArgs.trrMode() = aInfo->GetTRRMode();
  aArgs.isIPv4Disabled() = aInfo->GetIPv4Disabled();
  aArgs.isIPv6Disabled() = aInfo->GetIPv6Disabled();
  aArgs.isHttp3() = aInfo->IsHttp3();
  aArgs.hasIPHintAddress() = aInfo->HasIPHintAddress();
  aArgs.echConfig() = aInfo->GetEchConfig();

  if (!aInfo->ProxyInfo()) {
    return;
  }

  nsTArray<ProxyInfoCloneArgs> proxyInfoArray;
  nsProxyInfo::SerializeProxyInfo(aInfo->ProxyInfo(), proxyInfoArray);
  aArgs.proxyInfo() = std::move(proxyInfoArray);
}

// This function needs to be synced with nsHttpConnectionInfo::Clone.
/* static */
already_AddRefed<nsHttpConnectionInfo>
nsHttpConnectionInfo::DeserializeHttpConnectionInfoCloneArgs(
    const HttpConnectionInfoCloneArgs& aInfoArgs) {
  nsProxyInfo* pi = nsProxyInfo::DeserializeProxyInfo(aInfoArgs.proxyInfo());
  RefPtr<nsHttpConnectionInfo> cinfo;
  if (aInfoArgs.routedHost().IsEmpty()) {
    cinfo = new nsHttpConnectionInfo(
        aInfoArgs.host(), aInfoArgs.port(), aInfoArgs.npnToken(),
        aInfoArgs.username(), pi, aInfoArgs.originAttributes(),
        aInfoArgs.endToEndSSL(), aInfoArgs.isHttp3());
  } else {
    MOZ_ASSERT(aInfoArgs.endToEndSSL());
    cinfo = new nsHttpConnectionInfo(
        aInfoArgs.host(), aInfoArgs.port(), aInfoArgs.npnToken(),
        aInfoArgs.username(), pi, aInfoArgs.originAttributes(),
        aInfoArgs.routedHost(), aInfoArgs.routedPort(), aInfoArgs.isHttp3());
  }

  // Make sure the anonymous, insecure-scheme, and private flags are transferred
  cinfo->SetAnonymous(aInfoArgs.anonymous());
  cinfo->SetPrivate(aInfoArgs.aPrivate());
  cinfo->SetInsecureScheme(aInfoArgs.insecureScheme());
  cinfo->SetNoSpdy(aInfoArgs.noSpdy());
  cinfo->SetBeConservative(aInfoArgs.beConservative());
  cinfo->SetAnonymousAllowClientCert(aInfoArgs.anonymousAllowClientCert());
  cinfo->SetTlsFlags(aInfoArgs.tlsFlags());
  cinfo->SetIsTrrServiceChannel(aInfoArgs.isTrrServiceChannel());
  cinfo->SetTRRMode(static_cast<nsIRequest::TRRMode>(aInfoArgs.trrMode()));
  cinfo->SetIPv4Disabled(aInfoArgs.isIPv4Disabled());
  cinfo->SetIPv6Disabled(aInfoArgs.isIPv6Disabled());
  cinfo->SetHasIPHintAddress(aInfoArgs.hasIPHintAddress());
  cinfo->SetEchConfig(aInfoArgs.echConfig());

  return cinfo.forget();
}

void nsHttpConnectionInfo::CloneAsDirectRoute(nsHttpConnectionInfo** outCI) {
  // Explicitly use an empty npnToken when |mIsHttp3| is true, since we want to
  // create a non-http3 connection info.
  RefPtr<nsHttpConnectionInfo> clone = new nsHttpConnectionInfo(
      mOrigin, mOriginPort,
      (mRoutedHost.IsEmpty() && !mIsHttp3) ? mNPNToken : ""_ns, mUsername,
      mProxyInfo, mOriginAttributes, mEndToEndSSL);
  // Make sure the anonymous, insecure-scheme, and private flags are transferred
  clone->SetAnonymous(GetAnonymous());
  clone->SetPrivate(GetPrivate());
  clone->SetInsecureScheme(GetInsecureScheme());
  clone->SetNoSpdy(GetNoSpdy());
  clone->SetBeConservative(GetBeConservative());
  clone->SetAnonymousAllowClientCert(GetAnonymousAllowClientCert());
  clone->SetTlsFlags(GetTlsFlags());
  clone->SetIsTrrServiceChannel(GetIsTrrServiceChannel());
  clone->SetTRRMode(GetTRRMode());
  clone->SetIPv4Disabled(GetIPv4Disabled());
  clone->SetIPv6Disabled(GetIPv6Disabled());
  clone->SetHasIPHintAddress(HasIPHintAddress());
  clone->SetEchConfig(GetEchConfig());

  clone.forget(outCI);
}

nsresult nsHttpConnectionInfo::CreateWildCard(nsHttpConnectionInfo** outParam) {
  // T???mozilla.org:443 (https:proxy.ducksong.com:3128) [specifc form]
  // TS??*:0 (https:proxy.ducksong.com:3128)   [wildcard form]

  if (!mUsingHttpsProxy) {
    MOZ_ASSERT(false);
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  RefPtr<nsHttpConnectionInfo> clone;
  clone = new nsHttpConnectionInfo("*"_ns, 0, mNPNToken, mUsername, mProxyInfo,
                                   mOriginAttributes, true, mIsHttp3);
  // Make sure the anonymous and private flags are transferred!
  clone->SetAnonymous(GetAnonymous());
  clone->SetPrivate(GetPrivate());
  clone.forget(outParam);
  return NS_OK;
}

void nsHttpConnectionInfo::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  if (mTRRMode != aTRRMode) {
    mTRRMode = aTRRMode;
    RebuildHashKey();
  }
}

void nsHttpConnectionInfo::SetIPv4Disabled(bool aNoIPv4) {
  if (mIPv4Disabled != aNoIPv4) {
    mIPv4Disabled = aNoIPv4;
    RebuildHashKey();
  }
}

void nsHttpConnectionInfo::SetIPv6Disabled(bool aNoIPv6) {
  if (mIPv6Disabled != aNoIPv6) {
    mIPv6Disabled = aNoIPv6;
    RebuildHashKey();
  }
}

void nsHttpConnectionInfo::SetTlsFlags(uint32_t aTlsFlags) {
  mTlsFlags = aTlsFlags;

  mHashKey.Replace(19, 8, nsPrintfCString("%08x", mTlsFlags));
}

bool nsHttpConnectionInfo::UsingProxy() {
  if (!mProxyInfo) return false;
  return !mProxyInfo->IsDirect();
}

bool nsHttpConnectionInfo::HostIsLocalIPLiteral() const {
  NetAddr netAddr;
  // If the host/proxy host is not an IP address literal, return false.
  nsAutoCString host(ProxyHost() ? ProxyHost() : Origin());
  if (NS_FAILED(netAddr.InitFromString(host))) {
    return false;
  }
  return netAddr.IsIPAddrLocal();
}

}  // namespace net
}  // namespace mozilla
