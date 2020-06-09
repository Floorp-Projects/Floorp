/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaKeysEventSourceFactory.h"

#include "MediaHardwareKeysEventSourceMac.h"
#include "MediaHardwareKeysEventSourceMacMediaCenter.h"
#include "nsCocoaFeatures.h"

namespace mozilla {
namespace widget {

mozilla::dom::MediaControlKeySource* CreateMediaControlKeySource() {
  if (nsCocoaFeatures::IsAtLeastVersion(10, 12, 2)) {
    return new MediaHardwareKeysEventSourceMacMediaCenter();
  } else {
    return new MediaHardwareKeysEventSourceMac();
  }
}

}  // namespace widget
}  // namespace mozilla
