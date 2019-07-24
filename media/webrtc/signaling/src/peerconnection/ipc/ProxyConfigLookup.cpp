/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyConfigLookup.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsICancelable.h"
#include "nsIProtocolProxyService.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

// static
nsresult ProxyConfigLookup::Create(std::function<void(bool)>&& aCallback) {
  RefPtr<ProxyConfigLookup> helper = new ProxyConfigLookup();
  helper->mCallback = std::move(aCallback);

  nsresult rv;
  nsCOMPtr<nsIProtocolProxyService> pps =
      do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  // We use the following URL to find the "default" proxy address for all HTTPS
  // connections.  We will only attempt one HTTP(S) CONNECT per peer connection.
  // "example.com" is guaranteed to be unallocated and should return the best
  // default.
  nsCOMPtr<nsIURI> fakeHttpsLocation;
  rv = NS_NewURI(getter_AddRefs(fakeHttpsLocation), "https://example.com");
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), fakeHttpsLocation,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Network);
  nsCOMPtr<nsICancelable> proxyRequest;

  rv = pps->AsyncResolve(channel,
                         nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
                             nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL,
                         helper, target, getter_AddRefs(proxyRequest));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

ProxyConfigLookup::ProxyConfigLookup() = default;

ProxyConfigLookup::~ProxyConfigLookup() = default;

NS_IMETHODIMP ProxyConfigLookup::OnProxyAvailable(nsICancelable* aRequest,
                                                  nsIChannel* aChannel,
                                                  nsIProxyInfo* aProxyinfo,
                                                  nsresult aResult) {
  mCallback(NS_SUCCEEDED(aResult) && aProxyinfo);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(ProxyConfigLookup, nsIProtocolProxyCallback)

}  // namespace net
}  // namespace mozilla
