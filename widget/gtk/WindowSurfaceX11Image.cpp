/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceX11Image.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace widget {

WindowSurfaceX11Image::WindowSurfaceX11Image(Display* aDisplay,
                                             Window aWindow,
                                             Visual* aVisual,
                                             unsigned int aDepth)
  : WindowSurfaceX11(aDisplay, aWindow, aVisual, aDepth)
  , mImage(nullptr)
{
}

WindowSurfaceX11Image::~WindowSurfaceX11Image()
{
  if (mImage)
    XDestroyImage(mImage);
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceX11Image::Lock(const LayoutDeviceIntRegion& aRegion)
{
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize size(bounds.XMost(), bounds.YMost());

  if (!mImage || mImage->width < size.width || mImage->height < size.height) {
    if (mImage)
      XDestroyImage(mImage);

    int stride = gfx::GetAlignedStride<16>(size.width, gfx::BytesPerPixel(mFormat));
    if (stride == 0) {
        return nullptr;
    }
    char* data = static_cast<char*>(malloc(stride * size.height));
    if (!data)
      return nullptr;

    mImage = XCreateImage(mDisplay, mVisual, mDepth, ZPixmap, 0,
                          data, size.width, size.height,
                          8 * gfx::BytesPerPixel(mFormat), stride);
  }

  if (!mImage)
    return nullptr;

  unsigned char* data = (unsigned char*) mImage->data;
  return gfxPlatform::CreateDrawTargetForData(data,
                                              size,
                                              mImage->bytes_per_line,
                                              mFormat);
}

void
WindowSurfaceX11Image::CommitToDrawable(Drawable aDest, GC aGC,
                                        const LayoutDeviceIntRegion& aInvalidRegion)
{
  MOZ_ASSERT(mImage, "Attempted to commit invalid surface!");

  gfx::IntRect bounds = aInvalidRegion.GetBounds().ToUnknownRect();
  gfx::IntSize size(bounds.XMost(), bounds.YMost());
  XPutImage(mDisplay, aDest, aGC, mImage, bounds.x, bounds.y,
            bounds.x, bounds.y, size.width, size.height);
}

}  // namespace widget
}  // namespace mozilla
