/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <X11/extensions/shape.h>

namespace mozilla {
namespace widget {

using namespace mozilla::gfx;

// gfxImageSurface pixel format configuration.
#define SHAPED_IMAGE_SURFACE_BPP 4
#ifdef IS_BIG_ENDIAN
#  define SHAPED_IMAGE_SURFACE_ALPHA_INDEX 0
#else
#  define SHAPED_IMAGE_SURFACE_ALPHA_INDEX 3
#endif

WindowSurfaceX11Image::WindowSurfaceX11Image(Display* aDisplay, Window aWindow,
                                             Visual* aVisual,
                                             unsigned int aDepth,
                                             bool aIsShaped)
    : WindowSurfaceX11(aDisplay, aWindow, aVisual, aDepth),
      mTransparencyBitmap(nullptr),
      mTransparencyBitmapWidth(0),
      mTransparencyBitmapHeight(0),
      mIsShaped(aIsShaped),
      mWindowParent(0) {
  if (!mIsShaped) {
    return;
  }

  Window root, *children = nullptr;
  unsigned int childrenNum;
  if (XQueryTree(mDisplay, mWindow, &root, &mWindowParent, &children,
                 &childrenNum)) {
    if (children) {
      XFree((char*)children);
    }
  }
}

WindowSurfaceX11Image::~WindowSurfaceX11Image() {
  if (mTransparencyBitmap) {
    delete[] mTransparencyBitmap;
  }
}

already_AddRefed<gfx::DrawTarget> WindowSurfaceX11Image::Lock(
    const LayoutDeviceIntRegion& aRegion) {
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
      format = mDepth == 32 ? gfx::SurfaceFormat::A8R8G8B8_UINT32
                            : gfx::SurfaceFormat::X8R8G8B8_UINT32;
    }

    // Use alpha image format for shaped window as we derive
    // the shape bitmap from alpha channel. Must match SHAPED_IMAGE_SURFACE_BPP
    // and SHAPED_IMAGE_SURFACE_ALPHA_INDEX.
    if (mIsShaped) {
      format = gfx::SurfaceFormat::A8R8G8B8_UINT32;
    }

    mImageSurface = new gfxImageSurface(size, format);
    if (mImageSurface->CairoStatus()) {
      return nullptr;
    }
  }

  gfxImageFormat format = mImageSurface->Format();
  // Cairo prefers compositing to BGRX instead of BGRA where possible.
  // Cairo/pixman lacks some fast paths for compositing BGRX onto BGRA, so
  // just report it as BGRX directly in that case.
  // Otherwise, for Skia, report it as BGRA to the compositor. The alpha
  // channel will be discarded when we put the image.
  if (format == gfx::SurfaceFormat::X8R8G8B8_UINT32) {
    gfx::BackendType backend = gfxVars::ContentBackend();
    if (!gfx::Factory::DoesBackendSupportDataDrawtarget(backend)) {
      backend = gfx::BackendType::SKIA;
    }
    if (backend != gfx::BackendType::CAIRO) {
      format = gfx::SurfaceFormat::A8R8G8B8_UINT32;
    }
  }

  return gfxPlatform::CreateDrawTargetForData(
      mImageSurface->Data(), mImageSurface->GetSize(), mImageSurface->Stride(),
      ImageFormatToSurfaceFormat(format));
}

// The transparency bitmap routines are derived form the ones at nsWindow.cpp.
// The difference here is that we compose to RGBA image and then create
// the shape mask from final image alpha channel.
static inline int32_t GetBitmapStride(int32_t width) { return (width + 7) / 8; }

static bool ChangedMaskBits(gchar* aMaskBits, int32_t aMaskWidth,
                            int32_t aMaskHeight, const nsIntRect& aRect,
                            uint8_t* aImageData) {
  int32_t stride = aMaskWidth * SHAPED_IMAGE_SURFACE_BPP;
  int32_t x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
  int32_t maskBytesPerRow = GetBitmapStride(aMaskWidth);
  for (y = aRect.y; y < yMax; y++) {
    gchar* maskBytes = aMaskBits + y * maskBytesPerRow;
    uint8_t* alphas = aImageData;
    for (x = aRect.x; x < xMax; x++) {
      bool newBit = *(alphas + SHAPED_IMAGE_SURFACE_ALPHA_INDEX) > 0x7f;
      alphas += SHAPED_IMAGE_SURFACE_BPP;

      gchar maskByte = maskBytes[x >> 3];
      bool maskBit = (maskByte & (1 << (x & 7))) != 0;

      if (maskBit != newBit) {
        return true;
      }
    }
    aImageData += stride;
  }

  return false;
}

static void UpdateMaskBits(gchar* aMaskBits, int32_t aMaskWidth,
                           int32_t aMaskHeight, const nsIntRect& aRect,
                           uint8_t* aImageData) {
  int32_t stride = aMaskWidth * SHAPED_IMAGE_SURFACE_BPP;
  int32_t x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
  int32_t maskBytesPerRow = GetBitmapStride(aMaskWidth);
  for (y = aRect.y; y < yMax; y++) {
    gchar* maskBytes = aMaskBits + y * maskBytesPerRow;
    uint8_t* alphas = aImageData;
    for (x = aRect.x; x < xMax; x++) {
      bool newBit = *(alphas + SHAPED_IMAGE_SURFACE_ALPHA_INDEX) > 0x7f;
      alphas += SHAPED_IMAGE_SURFACE_BPP;

      gchar mask = 1 << (x & 7);
      gchar maskByte = maskBytes[x >> 3];
      // Note: '-newBit' turns 0 into 00...00 and 1 into 11...11
      maskBytes[x >> 3] = (maskByte & ~mask) | (-newBit & mask);
    }
    aImageData += stride;
  }
}

void WindowSurfaceX11Image::ResizeTransparencyBitmap(int aWidth, int aHeight) {
  int32_t actualSize =
      GetBitmapStride(mTransparencyBitmapWidth) * mTransparencyBitmapHeight;
  int32_t newSize = GetBitmapStride(aWidth) * aHeight;

  if (actualSize < newSize) {
    delete[] mTransparencyBitmap;
    mTransparencyBitmap = new gchar[newSize];
  }

  mTransparencyBitmapWidth = aWidth;
  mTransparencyBitmapHeight = aHeight;
}

void WindowSurfaceX11Image::ApplyTransparencyBitmap() {
  gfx::IntSize size = mWindowSurface->GetSize();
  bool maskChanged = true;

  if (!mTransparencyBitmap) {
    mTransparencyBitmapWidth = size.width;
    mTransparencyBitmapHeight = size.height;

    int32_t byteSize =
        GetBitmapStride(mTransparencyBitmapWidth) * mTransparencyBitmapHeight;
    mTransparencyBitmap = new gchar[byteSize];
  } else {
    bool sizeChanged = (size.width != mTransparencyBitmapWidth ||
                        size.height != mTransparencyBitmapHeight);

    if (sizeChanged) {
      ResizeTransparencyBitmap(size.width, size.height);
    } else {
      maskChanged = ChangedMaskBits(
          mTransparencyBitmap, mTransparencyBitmapWidth,
          mTransparencyBitmapHeight, nsIntRect(0, 0, size.width, size.height),
          (uint8_t*)mImageSurface->Data());
    }
  }

  if (maskChanged) {
    UpdateMaskBits(mTransparencyBitmap, mTransparencyBitmapWidth,
                   mTransparencyBitmapHeight,
                   nsIntRect(0, 0, size.width, size.height),
                   (uint8_t*)mImageSurface->Data());

    // We use X11 calls where possible, because GDK handles expose events
    // for shaped windows in a way that's incompatible with us (Bug 635903).
    // It doesn't occur when the shapes are set through X.
    Display* xDisplay = mWindowSurface->XDisplay();
    Window xDrawable = mWindowSurface->XDrawable();
    Pixmap maskPixmap = XCreateBitmapFromData(
        xDisplay, xDrawable, mTransparencyBitmap, mTransparencyBitmapWidth,
        mTransparencyBitmapHeight);
    XShapeCombineMask(xDisplay, xDrawable, ShapeBounding, 0, 0, maskPixmap,
                      ShapeSet);
    if (mWindowParent) {
      XShapeCombineMask(mDisplay, mWindowParent, ShapeBounding, 0, 0,
                        maskPixmap, ShapeSet);
    }
    XFreePixmap(xDisplay, maskPixmap);
  }
}

void WindowSurfaceX11Image::Commit(
    const LayoutDeviceIntRegion& aInvalidRegion) {
  RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateDrawTargetForCairoSurface(
      mWindowSurface->CairoSurface(), mWindowSurface->GetSize());
  RefPtr<gfx::SourceSurface> surf =
      gfx::Factory::CreateSourceSurfaceForCairoSurface(
          mImageSurface->CairoSurface(), mImageSurface->GetSize(),
          mImageSurface->Format());
  if (!dt || !surf) {
    return;
  }

  gfx::IntRect bounds = aInvalidRegion.GetBounds().ToUnknownRect();
  if (bounds.IsEmpty()) {
    return;
  }

  if (mIsShaped) {
    ApplyTransparencyBitmap();
  }

  uint32_t numRects = aInvalidRegion.GetNumRects();
  if (numRects == 1) {
    dt->CopySurface(surf, bounds, bounds.TopLeft());
  } else {
    AutoTArray<IntRect, 32> rects;
    rects.SetCapacity(numRects);
    for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
      rects.AppendElement(iter.Get().ToUnknownRect());
    }
    dt->PushDeviceSpaceClipRects(rects.Elements(), rects.Length());

    dt->DrawSurface(surf, gfx::Rect(bounds), gfx::Rect(bounds),
                    DrawSurfaceOptions(),
                    DrawOptions(1.0f, CompositionOp::OP_SOURCE));

    dt->PopClip();
  }
}

}  // namespace widget
}  // namespace mozilla
