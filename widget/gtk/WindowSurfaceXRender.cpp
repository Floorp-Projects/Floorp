/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceXRender.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Types.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace widget {

WindowSurfaceXRender::WindowSurfaceXRender(Display* aDisplay,
                                           Window aWindow,
                                           Visual* aVisual,
                                           unsigned int aDepth)
  : WindowSurfaceX11(aDisplay, aWindow, aVisual, aDepth)
  , mXlibSurface(nullptr)
{
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceXRender::Lock(const LayoutDeviceIntRegion& aRegion)
{
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize size(bounds.XMost(), bounds.YMost());
  if (!mXlibSurface || mXlibSurface->CairoStatus() != 0 ||
      mXlibSurface->GetSize().width < size.width ||
      mXlibSurface->GetSize().height < size.height)
  {
    mXlibSurface = gfxXlibSurface::Create(DefaultScreenOfDisplay(mDisplay),
                                          mVisual,
                                          size,
                                          mWindow);
  }

  if (mXlibSurface && mXlibSurface->CairoStatus() == 0) {
    return gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(mXlibSurface.get(), size);
  }
  return nullptr;
}

void
WindowSurfaceXRender::CommitToDrawable(Drawable aDest, GC aGC,
                                       const LayoutDeviceIntRegion& aInvalidRegion)
{
  MOZ_ASSERT(mXlibSurface && mXlibSurface->CairoStatus() == 0,
             "Attempted to commit invalid surface!");
  gfx::IntRect bounds = aInvalidRegion.GetBounds().ToUnknownRect();
  gfx::IntSize size(bounds.XMost(), bounds.YMost());
  XCopyArea(mDisplay, mXlibSurface->XDrawable(), aDest, aGC, bounds.x, bounds.y,
            size.width, size.height, bounds.x, bounds.y);
}

}  // namespace widget
}  // namespace mozilla
