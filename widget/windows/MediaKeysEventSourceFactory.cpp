/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaKeysEventSourceFactory.h"
#include "WindowsSMTCProvider.h"

namespace mozilla {
namespace widget {

mozilla::dom::MediaControlKeySource* CreateMediaControlKeySource() {
#ifndef __MINGW32__
  return new WindowsSMTCProvider();
#else
  return nullptr;  // MinGW doesn't support the required Windows 8.1+ APIs
#endif
}

}  // namespace widget
}  // namespace mozilla
