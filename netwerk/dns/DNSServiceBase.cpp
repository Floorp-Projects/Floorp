/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DNSServiceBase.h"

#include "DNS.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsIDNSService.h"
#include "nsIProtocolProxyService2.h"
#include "nsIPrefBranch.h"

namespace mozilla::net {

static const char kPrefProxyType[] = "network.proxy.type";
static const char kPrefDisablePrefetch[] = "network.dns.disablePrefetch";
static const char kPrefNetworkProxySOCKS[] = "network.proxy.socks";

NS_IMPL_ISUPPORTS(DNSServiceBase, nsIObserver)

void DNSServiceBase::AddPrefObserver(nsIPrefBranch* aPrefs) {
  aPrefs->AddObserver(kPrefProxyType, this, false);
  aPrefs->AddObserver(kPrefDisablePrefetch, this, false);
  // Monitor these to see if there is a change in proxy configuration
  aPrefs->AddObserver(kPrefNetworkProxySOCKS, this, false);
}

void DNSServiceBase::ReadPrefs(const char* aName) {
  if (!aName || !strcmp(aName, kPrefNetworkProxySOCKS)) {
    nsAutoCString socks;
    if (NS_SUCCEEDED(Preferences::GetCString(kPrefNetworkProxySOCKS, socks))) {
      mHasSocksProxy = !socks.IsEmpty();
    }
  }
  if (!aName || !strcmp(aName, kPrefDisablePrefetch) ||
      !strcmp(aName, kPrefProxyType)) {
    mDisablePrefetch = Preferences::GetBool(kPrefDisablePrefetch, false) ||
                       (StaticPrefs::network_proxy_type() ==
                        nsIProtocolProxyService::PROXYCONFIG_MANUAL);
  }
}

bool DNSServiceBase::DNSForbiddenByActiveProxy(const nsACString& aHostname,
                                               uint32_t aFlags) {
  if (aFlags & nsIDNSService::RESOLVE_IGNORE_SOCKS_DNS) {
    return false;
  }

  // We should avoid doing DNS when a proxy is in use.
  if (StaticPrefs::network_proxy_type() ==
          nsIProtocolProxyService::PROXYCONFIG_MANUAL &&
      mHasSocksProxy && StaticPrefs::network_proxy_socks_remote_dns()) {
    // Allow IP lookups through, but nothing else.
    if (!HostIsIPLiteral(aHostname)) {
      return true;
    }
  }
  return false;
}

}  // namespace mozilla::net
