/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_X11_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_X11_H

#ifdef MOZ_X11

#include "mozilla/widget/WindowSurface.h"
#include "mozilla/gfx/Types.h"

#include <X11/Xlib.h>

namespace mozilla {
namespace widget {

class WindowSurfaceX11 : public WindowSurface {
public:
  WindowSurfaceX11(Display* aDisplay, Window aWindow, Visual* aVisual,
                   unsigned int aDepth);
  ~WindowSurfaceX11();

  void Commit(const LayoutDeviceIntRegion& aInvalidRegion) final;

  virtual already_AddRefed<gfx::DrawTarget> Lock(const LayoutDeviceIntRegion& aRegion) override = 0;

  // Draws the buffered image onto aDest using the given GC.
  // The GC provided has been clipped to aInvalidRegion.
  virtual void CommitToDrawable(Drawable aDest, GC aGC,
                                const LayoutDeviceIntRegion& aInvalidRegion) = 0;

protected:
  static gfx::SurfaceFormat GetVisualFormat(const Visual* aVisual, unsigned int aDepth);

  Display* const mDisplay;
  const Window mWindow;
  Visual* const mVisual;
  const unsigned int mDepth;
  const gfx::SurfaceFormat mFormat;

  GC mGC;
};

}  // namespace widget
}  // namespace mozilla

#endif // MOZ_X11
#endif // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_X11_H
