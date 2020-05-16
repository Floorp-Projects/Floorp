/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FirefoxAccountsBridge_h_
#define mozilla_FirefoxAccountsBridge_h_

#include "mozIFirefoxAccountsBridge.h"
#include "nsCOMPtr.h"

extern "C" {

// Implemented in Rust, in the `firefox-accounts-bridge` crate.
nsresult NS_NewFirefoxAccountsBridge(mozIFirefoxAccountsBridge** aResult);

}  // extern "C"

namespace mozilla {

// A C++ XPCOM class constructor, for `components.conf`.
already_AddRefed<mozIFirefoxAccountsBridge> NewFirefoxAccountsBridge() {
  nsCOMPtr<mozIFirefoxAccountsBridge> bridge;
  if (NS_WARN_IF(
          NS_FAILED(NS_NewFirefoxAccountsBridge(getter_AddRefs(bridge))))) {
    return nullptr;
  }
  return bridge.forget();
}

}  // namespace mozilla

#endif  // mozilla_FirefoxAccountsBridge_h_
