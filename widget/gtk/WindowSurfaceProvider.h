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
#include "mozilla/ScopeExit.h"

#ifdef MOZ_X11
#  include <X11/Xlib.h>  // for Window, Display, Visual, etc.
#  include "X11UndefineNone.h"
#endif

class nsWindow;

namespace mozilla {
namespace widget {

class GtkCompositorWidget;

/*
 * Holds the logic for creating WindowSurface's for a GTK nsWindow.
 * The main purpose of this class is to allow sharing of logic between
 * nsWindow and GtkCompositorWidget, for when OMTC is enabled or disabled.
 */
class WindowSurfaceProvider final {
 public:
  WindowSurfaceProvider();
  ~WindowSurfaceProvider();

  /**
   * Initializes the WindowSurfaceProvider by giving it the window
   * handle and display to attach to. WindowSurfaceProvider doesn't
   * own the Display, Window, etc, and they must continue to exist
   * while WindowSurfaceProvider is used.
   */
#ifdef MOZ_WAYLAND
  bool Initialize(RefPtr<nsWindow> aWidget);
  bool Initialize(GtkCompositorWidget* aCompositorWidget);
#endif
#ifdef MOZ_X11
  bool Initialize(Window aWindow, bool aIsShaped);
  Window GetXWindow() const { return mXWindow; }
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
  void CleanupWindowSurface();

  RefPtr<WindowSurface> mWindowSurface;

  /* While CleanupResources() can be called from Main thread when nsWindow is
   * destroyed/hidden, StartRemoteDrawingInRegion()/EndRemoteDrawingInRegion()
   * is called from Compositor thread during rendering.
   *
   * As nsWindow CleanupResources() call comes from Gtk/X11 we can't synchronize
   * that with WebRender so we use lock to synchronize the access.
   */
  mozilla::Mutex mMutex MOZ_UNANNOTATED;
  // WindowSurface needs to be re-created as underlying window was changed.
  bool mWindowSurfaceValid;
#ifdef MOZ_WAYLAND
  RefPtr<nsWindow> mWidget;
  // WindowSurfaceProvider is owned by GtkCompositorWidget so we don't need
  // to reference it.
  GtkCompositorWidget* mCompositorWidget = nullptr;
#endif
#ifdef MOZ_X11
  bool mIsShaped;
  int mXDepth;
  // Make mXWindow atomic to allow it read from different threads
  // and make tsan happy.
  // We don't care much about actual mXWindow value (it may be valid XWindow or
  // nullptr) because we invalidate mXWindow at compositor/renderer thread
  // before it's release in unmap handler.
  Atomic<Window, Relaxed> mXWindow;
  Visual* mXVisual;
#endif
};

}  // namespace widget
}  // namespace mozilla

#endif  // _MOZILLA_WIDGET_GTK_WINDOW_SURFACE_PROVIDER_H
