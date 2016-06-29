/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinCompositorWidget.h"
#include "nsWindow.h"
#include "VsyncDispatcher.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/widget/PlatformWidgetTypes.h"

namespace mozilla {
namespace widget {

using namespace mozilla::gfx;

/* static */ RefPtr<CompositorWidget>
CompositorWidget::CreateLocal(const CompositorWidgetInitData& aInitData, nsIWidget* aWidget)
{
  return new WinCompositorWidget(aInitData, static_cast<nsWindow*>(aWidget));
}

WinCompositorWidget::WinCompositorWidget(const CompositorWidgetInitData& aInitData,
                                         nsWindow* aWindow)
 : mWindow(aWindow),
   mWidgetKey(aInitData.widgetKey()),
   mWnd(reinterpret_cast<HWND>(aInitData.hWnd())),
   mTransparencyMode(static_cast<nsTransparencyMode>(aInitData.transparencyMode())),
   mMemoryDC(nullptr),
   mCompositeDC(nullptr),
   mLockedBackBufferData(nullptr)
{
  MOZ_ASSERT(mWnd && ::IsWindow(mWnd));
}

void
WinCompositorWidget::OnDestroyWindow()
{
  mTransparentSurface = nullptr;
  mMemoryDC = nullptr;
}

bool
WinCompositorWidget::PreRender(layers::LayerManagerComposite* aManager)
{
  // This can block waiting for WM_SETTEXT to finish
  // Using PreRender is unnecessarily pessimistic because
  // we technically only need to block during the present call
  // not all of compositor rendering
  mPresentLock.Enter();
  return true;
}

void
WinCompositorWidget::PostRender(layers::LayerManagerComposite* aManager)
{
  mPresentLock.Leave();
}

nsIWidget*
WinCompositorWidget::RealWidget()
{
  MOZ_ASSERT(mWindow);
  return mWindow;
}

LayoutDeviceIntSize
WinCompositorWidget::GetClientSize()
{
  RECT r;
  if (!::GetClientRect(mWnd, &r)) {
    return LayoutDeviceIntSize();
  }
  return LayoutDeviceIntSize(
    r.right - r.left,
    r.bottom - r.top);
}

already_AddRefed<gfx::DrawTarget>
WinCompositorWidget::StartRemoteDrawing()
{
  MOZ_ASSERT(!mCompositeDC);

  RefPtr<gfxASurface> surf;
  if (mTransparencyMode == eTransparencyTransparent) {
    surf = EnsureTransparentSurface();
  }

  // Must call this after EnsureTransparentSurface(), since it could update
  // the DC.
  HDC dc = GetWindowSurface();
  if (!surf) {
    if (!dc) {
      return nullptr;
    }
    uint32_t flags = (mTransparencyMode == eTransparencyOpaque) ? 0 :
        gfxWindowsSurface::FLAG_IS_TRANSPARENT;
    surf = new gfxWindowsSurface(dc, flags);
  }

  IntSize size = surf->GetSize();
  if (size.width <= 0 || size.height <= 0) {
    if (dc) {
      FreeWindowSurface(dc);
    }
    return nullptr;
  }

  MOZ_ASSERT(!mCompositeDC);
  mCompositeDC = dc;

  return mozilla::gfx::Factory::CreateDrawTargetForCairoSurface(surf->CairoSurface(), size);
}

void
WinCompositorWidget::EndRemoteDrawing()
{
  MOZ_ASSERT(!mLockedBackBufferData);

  if (mTransparencyMode == eTransparencyTransparent) {
    MOZ_ASSERT(mTransparentSurface);
    RedrawTransparentWindow();
  }
  if (mCompositeDC) {
    FreeWindowSurface(mCompositeDC);
  }
  mCompositeDC = nullptr;
}

already_AddRefed<gfx::DrawTarget>
WinCompositorWidget::GetBackBufferDrawTarget(gfx::DrawTarget* aScreenTarget,
                                             const LayoutDeviceIntRect& aRect,
                                             const LayoutDeviceIntRect& aClearRect)
{
  MOZ_ASSERT(!mLockedBackBufferData);

  RefPtr<gfx::DrawTarget> target =
    CompositorWidget::GetBackBufferDrawTarget(aScreenTarget, aRect, aClearRect);
  if (!target) {
    return nullptr;
  }

  MOZ_ASSERT(target->GetBackendType() == BackendType::CAIRO);

  uint8_t* destData;
  IntSize destSize;
  int32_t destStride;
  SurfaceFormat destFormat;
  if (!target->LockBits(&destData, &destSize, &destStride, &destFormat)) {
    // LockBits is not supported. Use original DrawTarget.
    return target.forget();
  }

  RefPtr<gfx::DrawTarget> dataTarget =
    Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                     destData,
                                     destSize,
                                     destStride,
                                     destFormat);
  mLockedBackBufferData = destData;

  return dataTarget.forget();
}

already_AddRefed<gfx::SourceSurface>
WinCompositorWidget::EndBackBufferDrawing()
{
  if (mLockedBackBufferData) {
    MOZ_ASSERT(mLastBackBuffer);
    mLastBackBuffer->ReleaseBits(mLockedBackBufferData);
    mLockedBackBufferData = nullptr;
  }
  return CompositorWidget::EndBackBufferDrawing();
}

already_AddRefed<CompositorVsyncDispatcher>
WinCompositorWidget::GetCompositorVsyncDispatcher()
{
  RefPtr<CompositorVsyncDispatcher> cvd = mWindow->GetCompositorVsyncDispatcher();
  return cvd.forget();
}

uintptr_t
WinCompositorWidget::GetWidgetKey()
{
  return mWidgetKey;
}

void
WinCompositorWidget::EnterPresentLock()
{
  mPresentLock.Enter();
}

void
WinCompositorWidget::LeavePresentLock()
{
  mPresentLock.Leave();
}

RefPtr<gfxASurface>
WinCompositorWidget::EnsureTransparentSurface()
{
  MOZ_ASSERT(mTransparencyMode == eTransparencyTransparent);

  if (!mTransparentSurface) {
    LayoutDeviceIntSize size = GetClientSize();
    CreateTransparentSurface(size.width, size.height);
  }

  RefPtr<gfxASurface> surface = mTransparentSurface;
  return surface.forget();
}

void
WinCompositorWidget::CreateTransparentSurface(int32_t aWidth, int32_t aHeight)
{
  MOZ_ASSERT(!mTransparentSurface && !mMemoryDC);
  RefPtr<gfxWindowsSurface> surface =
    new gfxWindowsSurface(IntSize(aWidth, aHeight), SurfaceFormat::A8R8G8B8_UINT32);
  mTransparentSurface = surface;
  mMemoryDC = surface->GetDC();
}

void
WinCompositorWidget::UpdateTransparency(nsTransparencyMode aMode)
{
  if (mTransparencyMode == aMode) {
    return;
  }

  mTransparencyMode = aMode;
  mTransparentSurface = nullptr;
  mMemoryDC = nullptr;

  if (mTransparencyMode == eTransparencyTransparent) {
    EnsureTransparentSurface();
  }
}

void
WinCompositorWidget::ClearTransparentWindow()
{
  if (!mTransparentSurface) {
    return;
  }

  IntSize size = mTransparentSurface->GetSize();
  RefPtr<DrawTarget> drawTarget = gfxPlatform::GetPlatform()->
    CreateDrawTargetForSurface(mTransparentSurface, size);
  drawTarget->ClearRect(Rect(0, 0, size.width, size.height));
  RedrawTransparentWindow();
}

void
WinCompositorWidget::ResizeTransparentWindow(int32_t aNewWidth, int32_t aNewHeight)
{
  MOZ_ASSERT(mTransparencyMode == eTransparencyTransparent);

  if (mTransparentSurface &&
      mTransparentSurface->GetSize() == IntSize(aNewWidth, aNewHeight))
  {
    return;
  }

  // Destroy the old surface.
  mTransparentSurface = nullptr;
  mMemoryDC = nullptr;

  CreateTransparentSurface(aNewWidth, aNewHeight);
}

bool
WinCompositorWidget::RedrawTransparentWindow()
{
  MOZ_ASSERT(mTransparencyMode == eTransparencyTransparent);

  LayoutDeviceIntSize size = GetClientSize();

  ::GdiFlush();

  BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
  SIZE winSize = { size.width, size.height };
  POINT srcPos = { 0, 0 };
  HWND hWnd = WinUtils::GetTopLevelHWND(mWnd, true);
  RECT winRect;
  ::GetWindowRect(hWnd, &winRect);

  // perform the alpha blend
  return !!::UpdateLayeredWindow(
    hWnd, nullptr, (POINT*)&winRect, &winSize, mMemoryDC,
    &srcPos, 0, &bf, ULW_ALPHA);
}

HDC
WinCompositorWidget::GetWindowSurface()
{
  return eTransparencyTransparent == mTransparencyMode
         ? mMemoryDC
         : ::GetDC(mWnd);
}

void
WinCompositorWidget::FreeWindowSurface(HDC dc)
{
  if (eTransparencyTransparent != mTransparencyMode)
    ::ReleaseDC(mWnd, dc);
}

} // namespace widget
} // namespace mozilla
