/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProxyConfigLookupParent.h"
#include "ProxyConfigLookup.h"
#include "mozilla/Unused.h"
#include "nsProxyInfo.h"

namespace mozilla {
namespace net {

ProxyConfigLookupParent::ProxyConfigLookupParent(nsIURI* aURI,
                                                 uint32_t aProxyResolveFlags)
    : mURI(aURI), mProxyResolveFlags(aProxyResolveFlags) {}

ProxyConfigLookupParent::~ProxyConfigLookupParent() = default;

void ProxyConfigLookupParent::DoProxyLookup() {
  RefPtr<ProxyConfigLookupParent> self = this;
  nsresult rv = ProxyConfigLookup::Create(
      [self](nsIProxyInfo* aProxyInfo, nsresult aStatus) {
        if (self->CanSend()) {
          nsTArray<ProxyInfoCloneArgs> proxyInfoArray;
          if (aProxyInfo && NS_SUCCEEDED(aStatus)) {
            nsProxyInfo::SerializeProxyInfo(
                static_cast<nsProxyInfo*>(aProxyInfo), proxyInfoArray);
          }
          Unused << Send__delete__(self, proxyInfoArray, aStatus);
        }
      },
      mURI, mProxyResolveFlags);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    nsTArray<ProxyInfoCloneArgs> emptyArray;
    Unused << Send__delete__(self, emptyArray, rv);
  }
}

}  // namespace net
}  // namespace mozilla
