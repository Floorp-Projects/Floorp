/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_EarlyHintPreconnect_h
#define mozilla_net_EarlyHintPreconnect_h

#include "nsNetUtil.h"

namespace mozilla::net {

class EarlyHintPreconnect final {
 public:
  static void MaybePreconnect(const LinkHeader& aHeader, nsIURI* aBaseURI,
                              OriginAttributes&& aOriginAttributes);

  EarlyHintPreconnect() = delete;
  EarlyHintPreconnect(const EarlyHintPreconnect&) = delete;
  EarlyHintPreconnect& operator=(const EarlyHintPreconnect&) = delete;
};

}  // namespace mozilla::net

#endif  // mozilla_net_EarlyHintPreconnect_h
