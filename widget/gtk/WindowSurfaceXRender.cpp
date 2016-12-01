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
  , mGC(X11None)
{
}

WindowSurfaceXRender::~WindowSurfaceXRender()
{
  if (mGC != X11None) {
    XFreeGC(mDisplay, mGC);
  }
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceXRender::Lock(const LayoutDeviceIntRegion& aRegion)
{
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize size(bounds.XMost(), bounds.YMost());
  if (!mXlibSurface || mXlibSurface->CairoStatus() ||
      !(size <= mXlibSurface->GetSize())) {
    mXlibSurface = gfxXlibSurface::Create(DefaultScreenOfDisplay(mDisplay),
                                          mVisual,
                                          size,
                                          mWindow);
  }
  if (!mXlibSurface || mXlibSurface->CairoStatus()) {
    return nullptr;
  }

  return gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(mXlibSurface, size);
}

void
WindowSurfaceXRender::Commit(const LayoutDeviceIntRegion& aInvalidRegion)
{
  AutoTArray<XRectangle, 32> xrects;
  xrects.SetCapacity(aInvalidRegion.GetNumRects());

  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    const LayoutDeviceIntRect &r = iter.Get();
    XRectangle xrect = { (short)r.x, (short)r.y, (unsigned short)r.width, (unsigned short)r.height };
    xrects.AppendElement(xrect);
  }

  if (!mGC) {
    mGC = XCreateGC(mDisplay, mWindow, 0, nullptr);
    if (!mGC) {
      NS_WARNING("Couldn't create X11 graphics context for window!");
      return;
    }
  }

  XSetClipRectangles(mDisplay, mGC, 0, 0, xrects.Elements(), xrects.Length(), YXBanded);

  MOZ_ASSERT(mXlibSurface && mXlibSurface->CairoStatus() == 0,
             "Attempted to commit invalid surface!");
  gfx::IntRect bounds = aInvalidRegion.GetBounds().ToUnknownRect();
  gfx::IntSize size(bounds.XMost(), bounds.YMost());
  XCopyArea(mDisplay, mXlibSurface->XDrawable(), mWindow, mGC, bounds.x, bounds.y,
            size.width, size.height, bounds.x, bounds.y);
}

}  // namespace widget
}  // namespace mozilla
