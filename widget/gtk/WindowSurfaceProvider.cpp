/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

namespace mozilla {
namespace widget {

using namespace mozilla::gfx;
using namespace mozilla::layers;

WindowSurfaceProvider::WindowSurfaceProvider()
    : mXDisplay(nullptr)
    , mXWindow(0)
    , mXVisual(nullptr)
    , mXDepth(0)
    , mWindowSurface(nullptr)
{
}

void WindowSurfaceProvider::Initialize(
      Display* aDisplay,
      Window aWindow,
      Visual* aVisual,
      int aDepth)
{
  // We should not be initialized
  MOZ_ASSERT(!mXDisplay);

  // This should also be a valid initialization
  MOZ_ASSERT(aDisplay && aWindow != X11None && aVisual);

  mXDisplay = aDisplay;
  mXWindow = aWindow;
  mXVisual = aVisual;
  mXDepth = aDepth;
}
void WindowSurfaceProvider::CleanupResources()
{
  mWindowSurface = nullptr;
}

UniquePtr<WindowSurface>
WindowSurfaceProvider::CreateWindowSurface()
{
  // We should be initialized
  MOZ_ASSERT(mXDisplay);

  // Blit to the window with the following priority:
  // 1. XRender (iff XRender is enabled && we are in-process)
  // 2. MIT-SHM
  // 3. XPutImage

#ifdef MOZ_WIDGET_GTK
  if (gfxVars::UseXRender()) {
    LOGDRAW(("Drawing to nsWindow %p using XRender\n", (void*)this));
    return MakeUnique<WindowSurfaceXRender>(mXDisplay, mXWindow, mXVisual, mXDepth);
  }
#endif // MOZ_WIDGET_GTK

#ifdef MOZ_HAVE_SHMIMAGE
  if (nsShmImage::UseShm()) {
    LOGDRAW(("Drawing to nsWindow %p using MIT-SHM\n", (void*)this));
    return MakeUnique<WindowSurfaceX11SHM>(mXDisplay, mXWindow, mXVisual, mXDepth);
  }
#endif // MOZ_HAVE_SHMIMAGE

  LOGDRAW(("Drawing to nsWindow %p using XPutImage\n", (void*)this));
  return MakeUnique<WindowSurfaceX11Image>(mXDisplay, mXWindow, mXVisual, mXDepth);
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceProvider::StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion,
                                                layers::BufferMode* aBufferMode)
{
  if (aInvalidRegion.IsEmpty())
    return nullptr;

  if (!mWindowSurface) {
    mWindowSurface = CreateWindowSurface();
    if (!mWindowSurface)
      return nullptr;
  }

  *aBufferMode = BufferMode::BUFFER_NONE;
  RefPtr<DrawTarget> dt = nullptr;
  if (!(dt = mWindowSurface->Lock(aInvalidRegion)) &&
      !mWindowSurface->IsFallback()) {
    gfxWarningOnce() << "Failed to lock WindowSurface, falling back to XPutImage backend.";
    mWindowSurface = MakeUnique<WindowSurfaceX11Image>(mXDisplay, mXWindow, mXVisual, mXDepth);
    dt = mWindowSurface->Lock(aInvalidRegion);
  }
  return dt.forget();
}

void
WindowSurfaceProvider::EndRemoteDrawingInRegion(gfx::DrawTarget* aDrawTarget,
                                              LayoutDeviceIntRegion& aInvalidRegion)
{
  if (mWindowSurface)
    mWindowSurface->Commit(aInvalidRegion);
}

} // namespace mozilla
} // namespace widget
