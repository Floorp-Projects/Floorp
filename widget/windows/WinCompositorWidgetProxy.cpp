/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinCompositorWidgetProxy.h"
#include "nsWindow.h"
#include "VsyncDispatcher.h"
#include "mozilla/gfx/Point.h"

namespace mozilla {
namespace widget {

using namespace mozilla::gfx;

WinCompositorWidgetProxy::WinCompositorWidgetProxy(nsWindow* aWindow)
 : mWindow(aWindow),
   mWidgetKey(reinterpret_cast<uintptr_t>(aWindow)),
   mWnd(reinterpret_cast<HWND>(aWindow->GetNativeData(NS_NATIVE_WINDOW))),
   mTransparencyMode(aWindow->GetTransparencyMode()),
   mMemoryDC(nullptr),
   mCompositeDC(nullptr)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(!aWindow->Destroyed());
  MOZ_ASSERT(mWnd && ::IsWindow(mWnd));
}

void
WinCompositorWidgetProxy::OnDestroyWindow()
{
  mTransparentSurface = nullptr;
  mMemoryDC = nullptr;
}

bool
WinCompositorWidgetProxy::PreRender(layers::LayerManagerComposite* aManager)
{
  // This can block waiting for WM_SETTEXT to finish
  // Using PreRender is unnecessarily pessimistic because
  // we technically only need to block during the present call
  // not all of compositor rendering
  mPresentLock.Enter();
  return true;
}

void
WinCompositorWidgetProxy::PostRender(layers::LayerManagerComposite* aManager)
{
  mPresentLock.Leave();
}

nsIWidget*
WinCompositorWidgetProxy::RealWidget()
{
  return mWindow;
}

LayoutDeviceIntSize
WinCompositorWidgetProxy::GetClientSize()
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
WinCompositorWidgetProxy::StartRemoteDrawing()
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
WinCompositorWidgetProxy::EndRemoteDrawing()
{
  if (mTransparencyMode == eTransparencyTransparent) {
    MOZ_ASSERT(mTransparentSurface);
    RedrawTransparentWindow();
  }
  if (mCompositeDC) {
    FreeWindowSurface(mCompositeDC);
  }
  mCompositeDC = nullptr;
}

already_AddRefed<CompositorVsyncDispatcher>
WinCompositorWidgetProxy::GetCompositorVsyncDispatcher()
{
  RefPtr<CompositorVsyncDispatcher> cvd = mWindow->GetCompositorVsyncDispatcher();
  return cvd.forget();
}

uintptr_t
WinCompositorWidgetProxy::GetWidgetKey()
{
  return mWidgetKey;
}

void
WinCompositorWidgetProxy::EnterPresentLock()
{
  mPresentLock.Enter();
}

void
WinCompositorWidgetProxy::LeavePresentLock()
{
  mPresentLock.Leave();
}

RefPtr<gfxASurface>
WinCompositorWidgetProxy::EnsureTransparentSurface()
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
WinCompositorWidgetProxy::CreateTransparentSurface(int32_t aWidth, int32_t aHeight)
{
  MOZ_ASSERT(!mTransparentSurface && !mMemoryDC);
  RefPtr<gfxWindowsSurface> surface =
    new gfxWindowsSurface(IntSize(aWidth, aHeight), SurfaceFormat::A8R8G8B8_UINT32);
  mTransparentSurface = surface;
  mMemoryDC = surface->GetDC();
}

void
WinCompositorWidgetProxy::UpdateTransparency(nsTransparencyMode aMode)
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
WinCompositorWidgetProxy::ClearTransparentWindow()
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
WinCompositorWidgetProxy::ResizeTransparentWindow(int32_t aNewWidth, int32_t aNewHeight)
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
WinCompositorWidgetProxy::RedrawTransparentWindow()
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
WinCompositorWidgetProxy::GetWindowSurface()
{
  return eTransparencyTransparent == mTransparencyMode
         ? mMemoryDC
         : ::GetDC(mWnd);
}

void
WinCompositorWidgetProxy::FreeWindowSurface(HDC dc)
{
  if (eTransparencyTransparent != mTransparencyMode)
    ::ReleaseDC(mWnd, dc);
}

} // namespace widget
} // namespace mozilla
