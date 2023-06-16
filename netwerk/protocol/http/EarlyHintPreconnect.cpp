/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EarlyHintPreconnect.h"

#include "mozilla/CORSMode.h"
#include "mozilla/dom/Element.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsIOService.h"
#include "nsIURI.h"
#include "SpeculativeTransaction.h"

namespace mozilla::net {

namespace {
class EarlyHintsPreConnectOverride : public nsIInterfaceRequestor,
                                     public nsISpeculativeConnectionOverrider {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISPECULATIVECONNECTIONOVERRIDER
  NS_DECL_NSIINTERFACEREQUESTOR

  explicit EarlyHintsPreConnectOverride(uint32_t aConnectionLimit)
      : mConnectionLimit(aConnectionLimit) {}

 private:
  virtual ~EarlyHintsPreConnectOverride() = default;

  // Only set once on main thread and can be read on multiple threads.
  const uint32_t mConnectionLimit;
};

NS_IMPL_ISUPPORTS(EarlyHintsPreConnectOverride, nsIInterfaceRequestor,
                  nsISpeculativeConnectionOverrider)

NS_IMETHODIMP
EarlyHintsPreConnectOverride::GetInterface(const nsIID& iid, void** result) {
  if (NS_SUCCEEDED(QueryInterface(iid, result)) && *result) {
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
EarlyHintsPreConnectOverride::GetIgnoreIdle(bool* ignoreIdle) {
  *ignoreIdle = true;
  return NS_OK;
}

NS_IMETHODIMP
EarlyHintsPreConnectOverride::GetParallelSpeculativeConnectLimit(
    uint32_t* parallelSpeculativeConnectLimit) {
  *parallelSpeculativeConnectLimit = mConnectionLimit;
  return NS_OK;
}

NS_IMETHODIMP
EarlyHintsPreConnectOverride::GetIsFromPredictor(bool* isFromPredictor) {
  *isFromPredictor = false;
  return NS_OK;
}

NS_IMETHODIMP
EarlyHintsPreConnectOverride::GetAllow1918(bool* allow) {
  *allow = false;
  return NS_OK;
}

}  // namespace

void EarlyHintPreconnect::MaybePreconnect(
    const LinkHeader& aHeader, nsIURI* aBaseURI,
    OriginAttributes&& aOriginAttributes) {
  if (!StaticPrefs::network_early_hints_preconnect_enabled()) {
    return;
  }

  if (!gIOService) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(aHeader.NewResolveHref(getter_AddRefs(uri), aBaseURI))) {
    return;
  }

  // only preconnect secure context urls
  if (!uri->SchemeIs("https")) {
    return;
  }

  uint32_t maxPreconnectCount =
      StaticPrefs::network_early_hints_preconnect_max_connections();
  nsCOMPtr<nsIInterfaceRequestor> callbacks =
      new EarlyHintsPreConnectOverride(maxPreconnectCount);
  // Note that the http connection manager will limit the number of
  // connections we can make, so it should be fine we don't check duplicate
  // preconnect attempts here.
  CORSMode corsMode = dom::Element::StringToCORSMode(aHeader.mCrossOrigin);
  gIOService->SpeculativeConnectWithOriginAttributesNative(
      uri, std::move(aOriginAttributes), callbacks, corsMode == CORS_ANONYMOUS);
}

}  // namespace mozilla::net
