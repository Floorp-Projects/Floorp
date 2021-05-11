/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_PROVIDER_H
#define _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_PROVIDER_H

#include <gdk/gdk.h>

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/widget/WindowSurface.h"
#include "Units.h"

#ifdef MOZ_X11
#  include <X11/Xlib.h>  // for Window, Display, Visual, etc.
#  include "X11UndefineNone.h"
#endif

class nsWindow;

namespace mozilla {
namespace widget {

/*
 * Holds the logic for creating WindowSurface's for a GTK nsWindow.
 * The main purpose of this class is to allow sharing of logic between
 * nsWindow and GtkCompositorWidget, for when OMTC is enabled or disabled.
 */
class WindowSurfaceProvider final {
 public:
  WindowSurfaceProvider();

  /**
   * Initializes the WindowSurfaceProvider by giving it the window
   * handle and display to attach to. WindowSurfaceProvider doesn't
   * own the Display, Window, etc, and they must continue to exist
   * while WindowSurfaceProvider is used.
   */
#ifdef MOZ_WAYLAND
  void Initialize(nsWindow* aWidget);
#endif
#ifdef MOZ_X11
  void Initialize(Window aWindow, Visual* aVisual, int aDepth, bool aIsShaped);
#endif

  /**
   * Releases any surfaces created by this provider.
   * This is used by GtkCompositorWidget to get rid
   * of resources.
   */
  void CleanupResources();

  already_AddRefed<gfx::DrawTarget> StartRemoteDrawingInRegion(
      const LayoutDeviceIntRegion& aInvalidRegion,
      layers::BufferMode* aBufferMode);
  void EndRemoteDrawingInRegion(gfx::DrawTarget* aDrawTarget,
                                const LayoutDeviceIntRegion& aInvalidRegion);

 private:
  RefPtr<WindowSurface> CreateWindowSurface();

  RefPtr<WindowSurface> mWindowSurface;
#ifdef MOZ_WAYLAND
  nsWindow* mWidget;
#endif
#ifdef MOZ_X11
  bool mIsShaped;
  int mXDepth;
  Window mXWindow;
  Visual* mXVisual;
#endif
};

}  // namespace widget
}  // namespace mozilla

#endif  // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_PROVIDER_H
