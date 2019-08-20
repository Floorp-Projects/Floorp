/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceProvider.h"

#include "gfxPlatformGtk.h"
#include "mozilla/layers/LayersTypes.h"
#include "WindowSurfaceX11Image.h"
#include "WindowSurfaceX11SHM.h"
#include "WindowSurfaceXRender.h"
#ifdef MOZ_WAYLAND
#  include "WindowSurfaceWayland.h"
#endif

namespace mozilla {
namespace widget {

using namespace mozilla::gfx;
using namespace mozilla::layers;

WindowSurfaceProvider::WindowSurfaceProvider()
    : mIsX11Display(false),
      mXDisplay(nullptr),
      mXWindow(0),
      mXVisual(nullptr),
      mXDepth(0),
      mWindowSurface(nullptr)
#ifdef MOZ_WAYLAND
      ,
      mWidget(nullptr)
#endif
      ,
      mIsShaped(false) {
}

void WindowSurfaceProvider::Initialize(Display* aDisplay, Window aWindow,
                                       Visual* aVisual, int aDepth,
                                       bool aIsShaped) {
  // We should not be initialized
  MOZ_ASSERT(!mXDisplay);

  // This should also be a valid initialization
  MOZ_ASSERT(aDisplay && aWindow != X11None && aVisual);

  mXDisplay = aDisplay;
  mXWindow = aWindow;
  mXVisual = aVisual;
  mXDepth = aDepth;
  mIsShaped = aIsShaped;
  mIsX11Display = true;
}

#ifdef MOZ_WAYLAND
void WindowSurfaceProvider::Initialize(nsWindow* aWidget) {
  mWidget = aWidget;
  mIsX11Display = false;
}
#endif

void WindowSurfaceProvider::CleanupResources() { mWindowSurface = nullptr; }

UniquePtr<WindowSurface> WindowSurfaceProvider::CreateWindowSurface() {
#ifdef MOZ_WAYLAND
  if (!mIsX11Display) {
    LOGDRAW(("Drawing to nsWindow %p using wl_surface\n", (void*)this));
    return MakeUnique<WindowSurfaceWayland>(mWidget);
  }
#endif

  // We should be initialized
  MOZ_ASSERT(mXDisplay);

  // Blit to the window with the following priority:
  // 1. XRender (iff XRender is enabled && we are in-process)
  // 2. MIT-SHM
  // 3. XPutImage

#ifdef MOZ_WIDGET_GTK
  if (!mIsShaped && gfxVars::UseXRender()) {
    LOGDRAW(("Drawing to nsWindow %p using XRender\n", (void*)this));
    return MakeUnique<WindowSurfaceXRender>(mXDisplay, mXWindow, mXVisual,
                                            mXDepth);
  }
#endif  // MOZ_WIDGET_GTK

#ifdef MOZ_HAVE_SHMIMAGE
  if (!mIsShaped && nsShmImage::UseShm()) {
    LOGDRAW(("Drawing to nsWindow %p using MIT-SHM\n", (void*)this));
    return MakeUnique<WindowSurfaceX11SHM>(mXDisplay, mXWindow, mXVisual,
                                           mXDepth);
  }
#endif  // MOZ_HAVE_SHMIMAGE

  LOGDRAW(("Drawing to nsWindow %p using XPutImage\n", (void*)this));
  return MakeUnique<WindowSurfaceX11Image>(mXDisplay, mXWindow, mXVisual,
                                           mXDepth, mIsShaped);
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceProvider::StartRemoteDrawingInRegion(
    LayoutDeviceIntRegion& aInvalidRegion, layers::BufferMode* aBufferMode) {
  if (aInvalidRegion.IsEmpty()) return nullptr;

  if (!mWindowSurface) {
    mWindowSurface = CreateWindowSurface();
    if (!mWindowSurface) return nullptr;
  }

  *aBufferMode = BufferMode::BUFFER_NONE;
  RefPtr<DrawTarget> dt = nullptr;
  if (!(dt = mWindowSurface->Lock(aInvalidRegion)) && mIsX11Display &&
      !mWindowSurface->IsFallback()) {
    // We can't use WindowSurfaceX11Image fallback on Wayland but
    // Lock() call on WindowSurfaceWayland should never fail.
    gfxWarningOnce()
        << "Failed to lock WindowSurface, falling back to XPutImage backend.";
    mWindowSurface = MakeUnique<WindowSurfaceX11Image>(
        mXDisplay, mXWindow, mXVisual, mXDepth, mIsShaped);
    dt = mWindowSurface->Lock(aInvalidRegion);
  }
  return dt.forget();
}

void WindowSurfaceProvider::EndRemoteDrawingInRegion(
    gfx::DrawTarget* aDrawTarget, const LayoutDeviceIntRegion& aInvalidRegion) {
  if (mWindowSurface) mWindowSurface->Commit(aInvalidRegion);
}

}  // namespace widget
}  // namespace mozilla
