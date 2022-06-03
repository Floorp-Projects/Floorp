/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceX11.h"
#include "gfxPlatform.h"

namespace mozilla::widget {

WindowSurfaceX11::WindowSurfaceX11(Display* aDisplay, Window aWindow,
                                   Visual* aVisual, unsigned int aDepth)
    : mDisplay(aDisplay),
      mWindow(aWindow),
      mVisual(aVisual),
      mDepth(aDepth),
      mFormat(GetVisualFormat(aVisual, aDepth)) {}

/* static */
gfx::SurfaceFormat WindowSurfaceX11::GetVisualFormat(const Visual* aVisual,
                                                     unsigned int aDepth) {
  switch (aDepth) {
    case 32:
      if (aVisual->red_mask == 0xff0000 && aVisual->green_mask == 0xff00 &&
          aVisual->blue_mask == 0xff) {
        return gfx::SurfaceFormat::B8G8R8A8;
      }
      break;
    case 24:
      if (aVisual->red_mask == 0xff0000 && aVisual->green_mask == 0xff00 &&
          aVisual->blue_mask == 0xff) {
        return gfx::SurfaceFormat::B8G8R8X8;
      }
      break;
    case 16:
      if (aVisual->red_mask == 0xf800 && aVisual->green_mask == 0x07e0 &&
          aVisual->blue_mask == 0x1f) {
        return gfx::SurfaceFormat::R5G6B5_UINT16;
      }
      break;
  }

  return gfx::SurfaceFormat::UNKNOWN;
}

}  // namespace mozilla::widget
