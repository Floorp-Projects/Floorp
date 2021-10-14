/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ProxyConfigLookup_h
#define mozilla_net_ProxyConfigLookup_h

#include <functional>
#include "nsIProtocolProxyCallback.h"
#include "nsCOMPtr.h"

class nsIURI;

namespace mozilla {
namespace net {

class ProxyConfigLookup final : public nsIProtocolProxyCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROTOCOLPROXYCALLBACK

  static nsresult Create(
      std::function<void(nsIProxyInfo*, nsresult)>&& aCallback, nsIURI* aURI,
      uint32_t aProxyResolveFlags,
      nsICancelable** aLookupCancellable = nullptr);

 private:
  explicit ProxyConfigLookup(
      std::function<void(nsIProxyInfo*, nsresult)>&& aCallback, nsIURI* aURI,
      uint32_t aProxyResolveFlags);
  virtual ~ProxyConfigLookup();
  nsresult DoProxyResolve(nsICancelable** aLookupCancellable);

  std::function<void(nsIProxyInfo*, nsresult)> mCallback;
  nsCOMPtr<nsIURI> mURI;
  uint32_t mProxyResolveFlags;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_ProxyConfigLookup_h
