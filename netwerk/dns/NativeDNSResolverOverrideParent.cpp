/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeDNSResolverOverrideParent.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/net/SocketProcessParent.h"
#include "nsIOService.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(NativeDNSResolverOverrideParent, nsINativeDNSResolverOverride)

static StaticRefPtr<NativeDNSResolverOverrideParent>
    gNativeDNSResolverOverrideParent;

// static
already_AddRefed<nsINativeDNSResolverOverride>
NativeDNSResolverOverrideParent::GetSingleton() {
  if (gNativeDNSResolverOverrideParent) {
    return do_AddRef(gNativeDNSResolverOverrideParent);
  }

  if (!gIOService) {
    return nullptr;
  }

  gNativeDNSResolverOverrideParent = new NativeDNSResolverOverrideParent();
  ClearOnShutdown(&gNativeDNSResolverOverrideParent);

  auto initTask = []() {
    Unused << SocketProcessParent::GetSingleton()
                  ->SendPNativeDNSResolverOverrideConstructor(
                      gNativeDNSResolverOverrideParent);
  };
  gIOService->CallOrWaitForSocketProcess(initTask);
  return do_AddRef(gNativeDNSResolverOverrideParent);
}

NS_IMETHODIMP NativeDNSResolverOverrideParent::AddIPOverride(
    const nsACString& aHost, const nsACString& aIPLiteral) {
  NetAddr tempAddr;
  if (!aIPLiteral.Equals("N/A"_ns) &&
      NS_FAILED(tempAddr.InitFromString(aIPLiteral))) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<NativeDNSResolverOverrideParent> self = this;
  nsCString host(aHost);
  nsCString ip(aIPLiteral);
  auto task = [self{std::move(self)}, host, ip]() {
    Unused << self->SendAddIPOverride(host, ip);
  };
  gIOService->CallOrWaitForSocketProcess(task);
  return NS_OK;
}

NS_IMETHODIMP NativeDNSResolverOverrideParent::SetCnameOverride(
    const nsACString& aHost, const nsACString& aCNAME) {
  if (aCNAME.IsEmpty()) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<NativeDNSResolverOverrideParent> self = this;
  nsCString host(aHost);
  nsCString cname(aCNAME);
  auto task = [self{std::move(self)}, host, cname]() {
    Unused << self->SendSetCnameOverride(host, cname);
  };
  gIOService->CallOrWaitForSocketProcess(task);
  return NS_OK;
}

NS_IMETHODIMP NativeDNSResolverOverrideParent::ClearHostOverride(
    const nsACString& aHost) {
  RefPtr<NativeDNSResolverOverrideParent> self = this;
  nsCString host(aHost);
  auto task = [self{std::move(self)}, host]() {
    Unused << self->SendClearHostOverride(host);
  };
  gIOService->CallOrWaitForSocketProcess(task);
  return NS_OK;
}

NS_IMETHODIMP NativeDNSResolverOverrideParent::ClearOverrides() {
  RefPtr<NativeDNSResolverOverrideParent> self = this;
  auto task = [self{std::move(self)}]() {
    Unused << self->SendClearOverrides();
  };
  gIOService->CallOrWaitForSocketProcess(task);
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
