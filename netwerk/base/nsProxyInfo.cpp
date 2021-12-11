/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProxyInfo.h"

#include "mozilla/net/NeckoChannelParams.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace net {

// Yes, we support QI to nsProxyInfo
NS_IMPL_ISUPPORTS(nsProxyInfo, nsProxyInfo, nsIProxyInfo)

// These pointers are declared in nsProtocolProxyService.cpp and
// comparison of mType by string pointer is valid within necko
extern const char kProxyType_HTTP[];
extern const char kProxyType_HTTPS[];
extern const char kProxyType_SOCKS[];
extern const char kProxyType_SOCKS4[];
extern const char kProxyType_SOCKS5[];
extern const char kProxyType_DIRECT[];

nsProxyInfo::nsProxyInfo(const nsACString& aType, const nsACString& aHost,
                         int32_t aPort, const nsACString& aUsername,
                         const nsACString& aPassword, uint32_t aFlags,
                         uint32_t aTimeout, uint32_t aResolveFlags,
                         const nsACString& aProxyAuthorizationHeader,
                         const nsACString& aConnectionIsolationKey)
    : mHost(aHost),
      mUsername(aUsername),
      mPassword(aPassword),
      mProxyAuthorizationHeader(aProxyAuthorizationHeader),
      mConnectionIsolationKey(aConnectionIsolationKey),
      mPort(aPort),
      mFlags(aFlags),
      mResolveFlags(aResolveFlags),
      mTimeout(aTimeout),
      mNext(nullptr) {
  if (aType.EqualsASCII(kProxyType_HTTP)) {
    mType = kProxyType_HTTP;
  } else if (aType.EqualsASCII(kProxyType_HTTPS)) {
    mType = kProxyType_HTTPS;
  } else if (aType.EqualsASCII(kProxyType_SOCKS)) {
    mType = kProxyType_SOCKS;
  } else if (aType.EqualsASCII(kProxyType_SOCKS4)) {
    mType = kProxyType_SOCKS4;
  } else if (aType.EqualsASCII(kProxyType_SOCKS5)) {
    mType = kProxyType_SOCKS5;
  } else if (aType.EqualsASCII(kProxyType_PROXY)) {
    mType = kProxyType_PROXY;
  } else {
    mType = kProxyType_DIRECT;
  }
}

NS_IMETHODIMP
nsProxyInfo::GetHost(nsACString& result) {
  result = mHost;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetPort(int32_t* result) {
  *result = mPort;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetType(nsACString& result) {
  result = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetFlags(uint32_t* result) {
  *result = mFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetResolveFlags(uint32_t* result) {
  *result = mResolveFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetUsername(nsACString& result) {
  result = mUsername;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetPassword(nsACString& result) {
  result = mPassword;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetProxyAuthorizationHeader(nsACString& result) {
  result = mProxyAuthorizationHeader;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetConnectionIsolationKey(nsACString& result) {
  result = mConnectionIsolationKey;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetFailoverTimeout(uint32_t* result) {
  *result = mTimeout;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetFailoverProxy(nsIProxyInfo** result) {
  NS_IF_ADDREF(*result = mNext);
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::SetFailoverProxy(nsIProxyInfo* proxy) {
  nsCOMPtr<nsProxyInfo> pi = do_QueryInterface(proxy);
  NS_ENSURE_ARG(pi);

  pi.swap(mNext);
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::GetSourceId(nsACString& result) {
  result = mSourceId;
  return NS_OK;
}

NS_IMETHODIMP
nsProxyInfo::SetSourceId(const nsACString& sourceId) {
  mSourceId = sourceId;
  return NS_OK;
}

bool nsProxyInfo::IsDirect() {
  if (!mType) return true;
  return mType == kProxyType_DIRECT;
}

bool nsProxyInfo::IsHTTP() { return mType == kProxyType_HTTP; }

bool nsProxyInfo::IsHTTPS() { return mType == kProxyType_HTTPS; }

bool nsProxyInfo::IsSOCKS() {
  return mType == kProxyType_SOCKS || mType == kProxyType_SOCKS4 ||
         mType == kProxyType_SOCKS5;
}

/* static */
void nsProxyInfo::SerializeProxyInfo(nsProxyInfo* aProxyInfo,
                                     nsTArray<ProxyInfoCloneArgs>& aResult) {
  for (nsProxyInfo* iter = aProxyInfo; iter; iter = iter->mNext) {
    ProxyInfoCloneArgs* arg = aResult.AppendElement();
    arg->type() = nsCString(iter->Type());
    arg->host() = iter->Host();
    arg->port() = iter->Port();
    arg->username() = iter->Username();
    arg->password() = iter->Password();
    arg->proxyAuthorizationHeader() = iter->ProxyAuthorizationHeader();
    arg->connectionIsolationKey() = iter->ConnectionIsolationKey();
    arg->flags() = iter->Flags();
    arg->timeout() = iter->Timeout();
    arg->resolveFlags() = iter->ResolveFlags();
  }
}

/* static */
nsProxyInfo* nsProxyInfo::DeserializeProxyInfo(
    const nsTArray<ProxyInfoCloneArgs>& aArgs) {
  nsProxyInfo *pi = nullptr, *first = nullptr, *last = nullptr;
  for (const ProxyInfoCloneArgs& info : aArgs) {
    pi = new nsProxyInfo(info.type(), info.host(), info.port(), info.username(),
                         info.password(), info.flags(), info.timeout(),
                         info.resolveFlags(), info.proxyAuthorizationHeader(),
                         info.connectionIsolationKey());
    if (last) {
      last->mNext = pi;
      // |mNext| will be released in |last|'s destructor.
      NS_IF_ADDREF(last->mNext);
    } else {
      first = pi;
    }
    last = pi;
  }

  return first;
}

already_AddRefed<nsProxyInfo> nsProxyInfo::CloneProxyInfoWithNewResolveFlags(
    uint32_t aResolveFlags) {
  nsTArray<ProxyInfoCloneArgs> args;
  SerializeProxyInfo(this, args);

  for (auto& arg : args) {
    arg.resolveFlags() = aResolveFlags;
  }

  RefPtr<nsProxyInfo> result = DeserializeProxyInfo(args);
  return result.forget();
}

}  // namespace net
}  // namespace mozilla
