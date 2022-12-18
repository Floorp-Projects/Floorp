/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EarlyHintPreconnect.h"

#include "mozilla/CORSMode.h"
#include "mozilla/dom/Element.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsIOService.h"
#include "nsIURI.h"

namespace mozilla::net {

void EarlyHintPreconnect::MaybePreconnect(const LinkHeader& aHeader,
                                          nsIURI* aBaseURI,
                                          nsIPrincipal* aPrincipal) {
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

  // Note that the http connection manager will limit the number of connections
  // we can make, so it should be fine we don't check duplicate preconnect
  // attempts here.
  CORSMode corsMode = dom::Element::StringToCORSMode(aHeader.mCrossOrigin);
  if (corsMode == CORS_ANONYMOUS) {
    gIOService->SpeculativeAnonymousConnect(uri, aPrincipal, nullptr);
  } else {
    gIOService->SpeculativeConnect(uri, aPrincipal, nullptr);
  }
}

}  // namespace mozilla::net
