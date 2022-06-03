/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_X11_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_X11_H

#ifdef MOZ_X11

#  include "mozilla/widget/WindowSurface.h"
#  include "mozilla/gfx/Types.h"

#  include <X11/Xlib.h>
#  include "X11UndefineNone.h"

namespace mozilla::widget {

class WindowSurfaceX11 : public WindowSurface {
 public:
  WindowSurfaceX11(Display* aDisplay, Window aWindow, Visual* aVisual,
                   unsigned int aDepth);

 protected:
  static gfx::SurfaceFormat GetVisualFormat(const Visual* aVisual,
                                            unsigned int aDepth);

  Display* const mDisplay;
  const Window mWindow;
  Visual* const mVisual;
  const unsigned int mDepth;
  const gfx::SurfaceFormat mFormat;
};

}  // namespace mozilla::widget

#endif  // MOZ_X11
#endif  // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_X11_H
