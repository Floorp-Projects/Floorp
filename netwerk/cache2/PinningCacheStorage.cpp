/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PinningCacheStorage.h"

namespace mozilla {
namespace net {

bool PinningCacheStorage::IsPinning() const
{
  if (LoadInfo()->AppId() == nsILoadContextInfo::NO_APP_ID) {
    return false;
  }

  if (LoadInfo()->IsPrivate()) {
    return false;
  }

  // We are a non-private app load, pin!
  return true;
}

} // net
} // mozilla
