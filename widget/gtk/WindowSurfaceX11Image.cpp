/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowSurfaceX11Image.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/gfx/gfxVars.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"

namespace mozilla {
namespace widget {

WindowSurfaceX11Image::WindowSurfaceX11Image(Display* aDisplay,
                                             Window aWindow,
                                             Visual* aVisual,
                                             unsigned int aDepth)
  : WindowSurfaceX11(aDisplay, aWindow, aVisual, aDepth)
{
}

WindowSurfaceX11Image::~WindowSurfaceX11Image()
{
}

already_AddRefed<gfx::DrawTarget>
WindowSurfaceX11Image::Lock(const LayoutDeviceIntRegion& aRegion)
{
  gfx::IntRect bounds = aRegion.GetBounds().ToUnknownRect();
  gfx::IntSize size(bounds.XMost(), bounds.YMost());

  if (!mWindowSurface || mWindowSurface->CairoStatus() ||
      !(size <= mWindowSurface->GetSize())) {
    mWindowSurface = new gfxXlibSurface(mDisplay, mWindow, mVisual, size);
  }
  if (mWindowSurface->CairoStatus()) {
    return nullptr;
  }

  if (!mImageSurface || mImageSurface->CairoStatus() ||
      !(size <= mImageSurface->GetSize())) {
    gfxImageFormat format = SurfaceFormatToImageFormat(mFormat);
    if (format == gfx::SurfaceFormat::UNKNOWN) {
      format = mDepth == 32 ?
                 gfx::SurfaceFormat::A8R8G8B8_UINT32 :
                 gfx::SurfaceFormat::X8R8G8B8_UINT32;
    }

    mImageSurface = new gfxImageSurface(size, format);
    if (mImageSurface->CairoStatus()) {
      return nullptr;
    }
  }

  gfxImageFormat format = mImageSurface->Format();
  // Cairo prefers compositing to BGRX instead of BGRA where possible.
  if (format == gfx::SurfaceFormat::X8R8G8B8_UINT32) {
    gfx::BackendType backend = gfxVars::ContentBackend();
    if (!gfx::Factory::DoesBackendSupportDataDrawtarget(backend)) {
#ifdef USE_SKIA
      backend = gfx::BackendType::SKIA;
#else
      backend = gfx::BackendType::CAIRO;
#endif
    }
    if (backend != gfx::BackendType::CAIRO) {
      format = gfx::SurfaceFormat::A8R8G8B8_UINT32;
    }
  }

  return gfxPlatform::CreateDrawTargetForData(mImageSurface->Data(),
                                              mImageSurface->GetSize(),
                                              mImageSurface->Stride(),
                                              ImageFormatToSurfaceFormat(format));
}

void
WindowSurfaceX11Image::Commit(const LayoutDeviceIntRegion& aInvalidRegion)
{
  RefPtr<gfx::DrawTarget> dt =
    gfx::Factory::CreateDrawTargetForCairoSurface(mWindowSurface->CairoSurface(),
                                                  mWindowSurface->GetSize());
  RefPtr<gfx::SourceSurface> surf =
    gfx::Factory::CreateSourceSurfaceForCairoSurface(mImageSurface->CairoSurface(),
                                                     mImageSurface->GetSize(),
                                                     mImageSurface->Format());
  if (!dt || !surf) {
    return;
  }

  gfx::IntRect bounds = aInvalidRegion.GetBounds().ToUnknownRect();
  gfx::Rect rect(0, 0, bounds.XMost(), bounds.YMost());
  if (rect.IsEmpty()) {
    return;
  }

  uint32_t numRects = aInvalidRegion.GetNumRects();
  if (numRects != 1) {
    AutoTArray<IntRect, 32> rects;
    rects.SetCapacity(numRects);
    for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
      rects.AppendElement(iter.Get().ToUnknownRect());
    }
    dt->PushDeviceSpaceClipRects(rects.Elements(), rects.Length());
  }

  dt->DrawSurface(surf, rect, rect);

  if (numRects != 1) {
    dt->PopClip();
  }
}

}  // namespace widget
}  // namespace mozilla
