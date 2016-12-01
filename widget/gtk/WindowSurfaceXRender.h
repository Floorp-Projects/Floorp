/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_XRENDER_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_XRENDER_H

#ifdef MOZ_X11

#include "WindowSurfaceX11.h"
#include "gfxXlibSurface.h"

namespace mozilla {
namespace widget {

class WindowSurfaceXRender : public WindowSurfaceX11 {
public:
  WindowSurfaceXRender(Display* aDisplay, Window aWindow, Visual* aVisual,
                       unsigned int aDepth);
  ~WindowSurfaceXRender();

  already_AddRefed<gfx::DrawTarget> Lock(const LayoutDeviceIntRegion& aRegion) override;
  void Commit(const LayoutDeviceIntRegion& aInvalidRegion) override;

private:
  RefPtr<gfxXlibSurface> mXlibSurface;
  GC mGC;
};

}  // namespace widget
}  // namespace mozilla

#endif // MOZ_X11
#endif // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_XRENDER_H
