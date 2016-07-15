/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_WINDOW_SURFACE_X11_SHM_H
#define _MOZILLA_WIDGET_WINDOW_SURFACE_X11_SHM_H

#ifdef MOZ_X11

#include "mozilla/widget/WindowSurface.h"
#include "nsShmImage.h"

namespace mozilla {
namespace widget {

class WindowSurfaceX11SHM : public WindowSurface {
public:
  WindowSurfaceX11SHM(Display* aDisplay, Drawable aWindow, Visual* aVisual,
                      unsigned int aDepth);

  already_AddRefed<gfx::DrawTarget> Lock(const LayoutDeviceIntRegion& aRegion) override;
  void Commit(const LayoutDeviceIntRegion& aInvalidRegion) override;

private:
  RefPtr<nsShmImage> mFrontImage;
  RefPtr<nsShmImage> mBackImage;
};

}  // namespace widget
}  // namespace mozilla

#endif // MOZ_X11
#endif // _MOZILLA_WIDGET_WINDOW_SURFACE_X11_SHM_H
