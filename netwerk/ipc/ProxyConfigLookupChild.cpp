/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyConfigLookupChild.h"

#include "mozilla/net/SocketProcessChild.h"
#include "nsProxyInfo.h"

namespace mozilla {
namespace net {

// static
bool ProxyConfigLookupChild::Create(
    nsIURI* aURI, uint32_t aProxyResolveFlags,
    std::function<void(nsIProxyInfo*, nsresult)>&& aCallback) {
  SocketProcessChild* socketChild = SocketProcessChild::GetSingleton();
  if (!socketChild) {
    return false;
  }

  RefPtr<ProxyConfigLookupChild> child =
      new ProxyConfigLookupChild(std::move(aCallback));
  return socketChild->SendPProxyConfigLookupConstructor(child, aURI,
                                                        aProxyResolveFlags);
}

ProxyConfigLookupChild::ProxyConfigLookupChild(
    std::function<void(nsIProxyInfo*, nsresult)>&& aCallback)
    : mCallback(std::move(aCallback)) {}

mozilla::ipc::IPCResult ProxyConfigLookupChild::Recv__delete__(
    nsTArray<ProxyInfoCloneArgs>&& aProxyInfo, const nsresult& aResult) {
  nsCOMPtr<nsIProxyInfo> proxyInfo =
      nsProxyInfo::DeserializeProxyInfo(aProxyInfo);
  mCallback(proxyInfo, aResult);
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
