/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_PROVIDER_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_PROVIDER_H

#include "mozilla/widget/WindowSurface.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/2D.h"
#include "Units.h"

#include <gdk/gdk.h>
#ifdef MOZ_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#include <X11/Xlib.h> // for Window, Display, Visual, etc.
#include "X11UndefineNone.h"

class nsWindow;

namespace mozilla {
namespace widget {

/*
 * Holds the logic for creating WindowSurface's for a GTK nsWindow.
 * The main purpose of this class is to allow sharing of logic between
 * nsWindow and GtkCompositorWidget, for when OMTC is enabled or disabled.
 */
class WindowSurfaceProvider final
{
public:
  WindowSurfaceProvider();

  /**
   * Initializes the WindowSurfaceProvider by giving it the window
   * handle and display to attach to. WindowSurfaceProvider doesn't
   * own the Display, Window, etc, and they must continue to exist
   * while WindowSurfaceProvider is used.
   */
  void Initialize(
      Display* aDisplay,
      Window aWindow,
      Visual* aVisual,
      int aDepth);

#ifdef MOZ_WAYLAND
   void Initialize(nsWindow *aWidget);
#endif

  /**
   * Releases any surfaces created by this provider.
   * This is used by GtkCompositorWidget to get rid
   * of resources before we close the display connection.
   */
  void CleanupResources();

  already_AddRefed<gfx::DrawTarget>
  StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion,
                             layers::BufferMode* aBufferMode);
  void EndRemoteDrawingInRegion(gfx::DrawTarget* aDrawTarget,
                                LayoutDeviceIntRegion& aInvalidRegion);

private:
  UniquePtr<WindowSurface> CreateWindowSurface();

  // Can we access X?
  bool        mIsX11Display;
  Display*    mXDisplay;
  Window      mXWindow;
  Visual*     mXVisual;
  int         mXDepth;
  UniquePtr<WindowSurface> mWindowSurface;
#ifdef MOZ_WAYLAND
  nsWindow*   mWidget;
#endif
};

}  // namespace widget
}  // namespace mozilla

#endif // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_PROVIDER_H
