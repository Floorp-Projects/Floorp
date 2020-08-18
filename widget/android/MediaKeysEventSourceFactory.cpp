/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaKeysEventSourceFactory.h"

namespace mozilla {
namespace widget {

mozilla::dom::MediaControlKeySource* CreateMediaControlKeySource() {
  // GeckoView uses MediaController.webidl for media session events and control,
  // see bug 1623715.
  return nullptr;
}

}  // namespace widget
}  // namespace mozilla
