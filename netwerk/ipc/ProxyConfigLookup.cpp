/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyConfigLookup.h"
#include "ProxyConfigLookupChild.h"
#include "mozilla/Components.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"
#include "nsICancelable.h"
#include "nsIProtocolProxyService.h"
#include "nsIProtocolProxyService2.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsIChannel.h"

namespace mozilla {
namespace net {

// static
nsresult ProxyConfigLookup::Create(
    std::function<void(nsIProxyInfo*, nsresult)>&& aCallback, nsIURI* aURI,
    uint32_t aProxyResolveFlags, nsICancelable** aLookupCancellable) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ProxyConfigLookup> lookUp =
      new ProxyConfigLookup(std::move(aCallback), aURI, aProxyResolveFlags);
  return lookUp->DoProxyResolve(aLookupCancellable);
}

ProxyConfigLookup::ProxyConfigLookup(
    std::function<void(nsIProxyInfo*, nsresult)>&& aCallback, nsIURI* aURI,
    uint32_t aProxyResolveFlags)
    : mCallback(std::move(aCallback)),
      mURI(aURI),
      mProxyResolveFlags(aProxyResolveFlags) {}

ProxyConfigLookup::~ProxyConfigLookup() = default;

nsresult ProxyConfigLookup::DoProxyResolve(nsICancelable** aLookupCancellable) {
  if (!XRE_IsParentProcess()) {
    RefPtr<ProxyConfigLookup> self = this;
    bool result = ProxyConfigLookupChild::Create(
        mURI, mProxyResolveFlags,
        [self](nsIProxyInfo* aProxyinfo, nsresult aResult) {
          self->OnProxyAvailable(nullptr, nullptr, aProxyinfo, aResult);
        });
    return result ? NS_OK : NS_ERROR_FAILURE;
  }

  nsresult rv;
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), mURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIProtocolProxyService> pps;
  pps = mozilla::components::ProtocolProxy::Service(&rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // using the nsIProtocolProxyService2 allows a minor performance
  // optimization, but if an add-on has only provided the original interface
  // then it is ok to use that version.
  nsCOMPtr<nsICancelable> proxyRequest;
  nsCOMPtr<nsIProtocolProxyService2> pps2 = do_QueryInterface(pps);
  if (pps2) {
    rv = pps2->AsyncResolve2(channel, mProxyResolveFlags, this, nullptr,
                             getter_AddRefs(proxyRequest));
  } else {
    rv = pps->AsyncResolve(channel, mProxyResolveFlags, this, nullptr,
                           getter_AddRefs(proxyRequest));
  }

  if (aLookupCancellable) {
    proxyRequest.forget(aLookupCancellable);
  }

  return rv;
}

NS_IMETHODIMP ProxyConfigLookup::OnProxyAvailable(nsICancelable* aRequest,
                                                  nsIChannel* aChannel,
                                                  nsIProxyInfo* aProxyinfo,
                                                  nsresult aResult) {
  mCallback(aProxyinfo, aResult);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(ProxyConfigLookup, nsIProtocolProxyCallback)

}  // namespace net
}  // namespace mozilla
