/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoScreenOrientation_h
#define GeckoScreenOrientation_h

#include "nsAppShell.h"
#include "nsCOMPtr.h"

#include "mozilla/Hal.h"
#include "mozilla/java/GeckoScreenOrientationNatives.h"
#include "mozilla/widget/ScreenManager.h"

namespace mozilla {

class GeckoScreenOrientation final
    : public java::GeckoScreenOrientation::Natives<GeckoScreenOrientation> {
  GeckoScreenOrientation() = delete;

 public:
  static void OnOrientationChange(int16_t aOrientation, int16_t aAngle) {
    RefPtr<widget::Screen> screen =
        widget::ScreenManager::GetSingleton().GetPrimaryScreen();
    hal::ScreenConfiguration screenConfiguration =
        screen->ToScreenConfiguration();
    screenConfiguration.orientation() =
        static_cast<hal::ScreenOrientation>(aOrientation);
    screenConfiguration.angle() = aAngle;

    hal::NotifyScreenConfigurationChange(screenConfiguration);
  }
};

}  // namespace mozilla

#endif  // GeckoScreenOrientation_h
