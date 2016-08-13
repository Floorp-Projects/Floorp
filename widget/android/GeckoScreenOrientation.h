/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoScreenOrientation_h
#define GeckoScreenOrientation_h

#include "GeneratedJNINatives.h"
#include "nsAppShell.h"
#include "nsCOMPtr.h"
#include "nsIScreenManager.h"

#include "mozilla/Hal.h"
#include "mozilla/dom/ScreenOrientation.h"

namespace mozilla {

class GeckoScreenOrientation final
    : public java::GeckoScreenOrientation::Natives<GeckoScreenOrientation>
{
    GeckoScreenOrientation() = delete;

public:
    static void
    OnOrientationChange(int16_t aOrientation, int16_t aAngle)
    {
        nsCOMPtr<nsIScreenManager> screenMgr =
                do_GetService("@mozilla.org/gfx/screenmanager;1");
        nsCOMPtr<nsIScreen> screen;

        if (!screenMgr || NS_FAILED(screenMgr->GetPrimaryScreen(
                getter_AddRefs(screen))) || !screen) {
            return;
        }

        nsIntRect rect;
        int32_t colorDepth, pixelDepth;

        if (NS_FAILED(screen->GetRect(&rect.x, &rect.y,
                                      &rect.width, &rect.height)) ||
                NS_FAILED(screen->GetColorDepth(&colorDepth)) ||
                NS_FAILED(screen->GetPixelDepth(&pixelDepth))) {
            return;
        }

        hal::NotifyScreenConfigurationChange(hal::ScreenConfiguration(
                rect, static_cast<dom::ScreenOrientationInternal>(aOrientation),
                aAngle, colorDepth, pixelDepth));
    }
};

} // namespace mozilla

#endif // GeckoScreenOrientation_h
