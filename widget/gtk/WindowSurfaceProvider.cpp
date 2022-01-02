/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceProvider.h"

#include "gfxPlatformGtk.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsWindow.h"

#ifdef MOZ_WAYLAND
#  include "mozilla/StaticPrefs_widget.h"
#  include "WindowSurfaceWaylandMultiBuffer.h"
#endif
#ifdef MOZ_X11
#  include "mozilla/X11Util.h"
#  include "WindowSurfaceX11Image.h"
#  include "WindowSurfaceX11SHM.h"
#endif

#undef LOG
#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
extern mozilla::LazyLogModule gWidgetLog;
#  define LOG(args) MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, args)
#else
#  define LOG(args)
#endif /* MOZ_LOGGING */

namespace mozilla {
namespace widget {

using namespace mozilla::layers;

WindowSurfaceProvider::WindowSurfaceProvider()
    : mWindowSurface(nullptr)
#ifdef MOZ_WAYLAND
      ,
      mWidget(nullptr)
#endif
#ifdef MOZ_X11
      ,
      mIsShaped(false),
      mXDepth(0),
      mXWindow(0),
      mXVisual(nullptr)
#endif
{
}

#ifdef MOZ_WAYLAND
void WindowSurfaceProvider::Initialize(RefPtr<nsWindow> aWidget) {
  mWidget = std::move(aWidget);
}
#endif
#ifdef MOZ_X11
void WindowSurfaceProvider::Initialize(Window aWindow, Visual* aVisual,
                                       int aDepth, bool aIsShaped) {
  mXWindow = aWindow;
  mXVisual = aVisual;
  mXDepth = aDepth;
  mIsShaped = aIsShaped;
}
#endif

void WindowSurfaceProvider::CleanupResources() {
  mWindowSurface = nullptr;
#ifdef MOZ_WAYLAND
  mWidget = nullptr;
#endif
#ifdef MOZ_X11
  mXWindow = 0;
  mXVisual = 0;
  mXDepth = 0;
  mIsShaped = false;
#endif
}

RefPtr<WindowSurface> WindowSurfaceProvider::CreateWindowSurface() {
#ifdef MOZ_WAYLAND
  if (GdkIsWaylandDisplay()) {
    // We're called too early or we're unmapped.
    if (!mWidget) {
      return nullptr;
    }
    return MakeRefPtr<WindowSurfaceWaylandMB>(mWidget);
  }
#endif
#ifdef MOZ_X11
  if (GdkIsX11Display()) {
    // We're called too early or we're unmapped.
    if (!mXWindow) {
      return nullptr;
    }
    // Blit to the window with the following priority:
    // 1. MIT-SHM
    // 2. XPutImage
#  ifdef MOZ_HAVE_SHMIMAGE
    if (!mIsShaped && nsShmImage::UseShm()) {
      LOG(("Drawing to Window 0x%lx will use MIT-SHM\n", mXWindow));
      return MakeRefPtr<WindowSurfaceX11SHM>(DefaultXDisplay(), mXWindow,
                                             mXVisual, mXDepth);
    }
#  endif  // MOZ_HAVE_SHMIMAGE

    LOG(("Drawing to Window 0x%lx will use XPutImage\n", mXWindow));
    return MakeRefPtr<WindowSurfaceX11Image>(DefaultXDisplay(), mXWindow,
                                             mXVisual, mXDepth, mIsShaped);
  }
#endif
  MOZ_RELEASE_ASSERT(false);
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceProvider::StartRemoteDrawingInRegion(
    const LayoutDeviceIntRegion& aInvalidRegion,
    layers::BufferMode* aBufferMode) {
  if (aInvalidRegion.IsEmpty()) {
    return nullptr;
  }

  if (!mWindowSurface) {
    mWindowSurface = CreateWindowSurface();
    if (!mWindowSurface) {
      return nullptr;
    }
  }

  *aBufferMode = BufferMode::BUFFER_NONE;
  RefPtr<gfx::DrawTarget> dt = mWindowSurface->Lock(aInvalidRegion);
#ifdef MOZ_X11
  if (!dt && GdkIsX11Display() && !mWindowSurface->IsFallback()) {
    // We can't use WindowSurfaceX11Image fallback on Wayland but
    // Lock() call on WindowSurfaceWayland should never fail.
    gfxWarningOnce()
        << "Failed to lock WindowSurface, falling back to XPutImage backend.";
    mWindowSurface = MakeRefPtr<WindowSurfaceX11Image>(
        DefaultXDisplay(), mXWindow, mXVisual, mXDepth, mIsShaped);
    dt = mWindowSurface->Lock(aInvalidRegion);
  }
#endif
  return dt.forget();
}

void WindowSurfaceProvider::EndRemoteDrawingInRegion(
    gfx::DrawTarget* aDrawTarget, const LayoutDeviceIntRegion& aInvalidRegion) {
  if (mWindowSurface) mWindowSurface->Commit(aInvalidRegion);
}

}  // namespace widget
}  // namespace mozilla
