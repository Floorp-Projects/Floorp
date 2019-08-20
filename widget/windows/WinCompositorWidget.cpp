/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinCompositorWidget.h"

#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsWindow.h"
#include "VsyncDispatcher.h"
#include "WinCompositorWindowThread.h"

#include <ddraw.h>

namespace mozilla {
namespace widget {

using namespace mozilla::gfx;
using namespace mozilla;

WinCompositorWidget::WinCompositorWidget(
    const WinCompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions)
    : CompositorWidget(aOptions),
      mWidgetKey(aInitData.widgetKey()),
      mWnd(reinterpret_cast<HWND>(aInitData.hWnd())),
      mCompositorWnds(nullptr, nullptr),
      mTransparentSurfaceLock("mTransparentSurfaceLock"),
      mTransparencyMode(aInitData.transparencyMode()),
      mMemoryDC(nullptr),
      mCompositeDC(nullptr),
      mLockedBackBufferData(nullptr) {
  MOZ_ASSERT(mWnd && ::IsWindow(mWnd));

  // mNotDeferEndRemoteDrawing is set on the main thread during init,
  // but is only accessed after on the compositor thread.
  mNotDeferEndRemoteDrawing =
      StaticPrefs::layers_offmainthreadcomposition_frame_rate() == 0 ||
      gfxPlatform::IsInLayoutAsapMode() || gfxPlatform::ForceSoftwareVsync();
}

WinCompositorWidget::~WinCompositorWidget() { DestroyCompositorWindow(); }

void WinCompositorWidget::OnDestroyWindow() {
  MutexAutoLock lock(mTransparentSurfaceLock);
  mTransparentSurface = nullptr;
  mMemoryDC = nullptr;
}

bool WinCompositorWidget::PreRender(WidgetRenderingContext* aContext) {
  // This can block waiting for WM_SETTEXT to finish
  // Using PreRender is unnecessarily pessimistic because
  // we technically only need to block during the present call
  // not all of compositor rendering
  mPresentLock.Enter();
  return true;
}

void WinCompositorWidget::PostRender(WidgetRenderingContext* aContext) {
  mPresentLock.Leave();
}

LayoutDeviceIntSize WinCompositorWidget::GetClientSize() {
  RECT r;
  if (!::GetClientRect(mWnd, &r)) {
    return LayoutDeviceIntSize();
  }
  return LayoutDeviceIntSize(r.right - r.left, r.bottom - r.top);
}

already_AddRefed<gfx::DrawTarget> WinCompositorWidget::StartRemoteDrawing() {
  MutexAutoLock lock(mTransparentSurfaceLock);

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
    uint32_t flags = (mTransparencyMode == eTransparencyOpaque)
                         ? 0
                         : gfxWindowsSurface::FLAG_IS_TRANSPARENT;
    surf = new gfxWindowsSurface(dc, flags);
  }

  IntSize size = surf->GetSize();
  if (size.width <= 0 || size.height <= 0) {
    if (dc) {
      FreeWindowSurface(dc);
    }
    return nullptr;
  }

  RefPtr<DrawTarget> dt =
      mozilla::gfx::Factory::CreateDrawTargetForCairoSurface(
          surf->CairoSurface(), size);
  if (dt) {
    mCompositeDC = dc;
  } else {
    FreeWindowSurface(dc);
  }

  return dt.forget();
}

void WinCompositorWidget::EndRemoteDrawing() {
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

bool WinCompositorWidget::NeedsToDeferEndRemoteDrawing() {
  if (mNotDeferEndRemoteDrawing) {
    return false;
  }

  IDirectDraw7* ddraw = DeviceManagerDx::Get()->GetDirectDraw();
  if (!ddraw) {
    return false;
  }

  DWORD scanLine = 0;
  int height = ::GetSystemMetrics(SM_CYSCREEN);
  HRESULT ret = ddraw->GetScanLine(&scanLine);
  if (ret == DDERR_VERTICALBLANKINPROGRESS) {
    scanLine = 0;
  } else if (ret != DD_OK) {
    return false;
  }

  // Check if there is a risk of tearing with GDI.
  if (static_cast<int>(scanLine) > height / 2) {
    // No need to defer.
    return false;
  }

  return true;
}

already_AddRefed<gfx::DrawTarget> WinCompositorWidget::GetBackBufferDrawTarget(
    gfx::DrawTarget* aScreenTarget, const gfx::IntRect& aRect,
    bool* aOutIsCleared) {
  MOZ_ASSERT(!mLockedBackBufferData);

  RefPtr<gfx::DrawTarget> target = CompositorWidget::GetBackBufferDrawTarget(
      aScreenTarget, aRect, aOutIsCleared);
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

  RefPtr<gfx::DrawTarget> dataTarget = Factory::CreateDrawTargetForData(
      BackendType::CAIRO, destData, destSize, destStride, destFormat);
  mLockedBackBufferData = destData;

  return dataTarget.forget();
}

already_AddRefed<gfx::SourceSurface>
WinCompositorWidget::EndBackBufferDrawing() {
  if (mLockedBackBufferData) {
    MOZ_ASSERT(mLastBackBuffer);
    mLastBackBuffer->ReleaseBits(mLockedBackBufferData);
    mLockedBackBufferData = nullptr;
  }
  return CompositorWidget::EndBackBufferDrawing();
}

bool WinCompositorWidget::InitCompositor(layers::Compositor* aCompositor) {
  if (aCompositor->GetBackendType() == layers::LayersBackend::LAYERS_BASIC) {
    DeviceManagerDx::Get()->InitializeDirectDraw();
  }
  return true;
}

uintptr_t WinCompositorWidget::GetWidgetKey() { return mWidgetKey; }

void WinCompositorWidget::EnterPresentLock() { mPresentLock.Enter(); }

void WinCompositorWidget::LeavePresentLock() { mPresentLock.Leave(); }

RefPtr<gfxASurface> WinCompositorWidget::EnsureTransparentSurface() {
  mTransparentSurfaceLock.AssertCurrentThreadOwns();
  MOZ_ASSERT(mTransparencyMode == eTransparencyTransparent);

  IntSize size = GetClientSize().ToUnknownSize();
  if (!mTransparentSurface || mTransparentSurface->GetSize() != size) {
    mTransparentSurface = nullptr;
    mMemoryDC = nullptr;
    CreateTransparentSurface(size);
  }

  RefPtr<gfxASurface> surface = mTransparentSurface;
  return surface.forget();
}

void WinCompositorWidget::CreateTransparentSurface(const gfx::IntSize& aSize) {
  mTransparentSurfaceLock.AssertCurrentThreadOwns();
  MOZ_ASSERT(!mTransparentSurface && !mMemoryDC);
  RefPtr<gfxWindowsSurface> surface =
      new gfxWindowsSurface(aSize, SurfaceFormat::A8R8G8B8_UINT32);
  mTransparentSurface = surface;
  mMemoryDC = surface->GetDC();
}

void WinCompositorWidget::UpdateTransparency(nsTransparencyMode aMode) {
  MutexAutoLock lock(mTransparentSurfaceLock);
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

void WinCompositorWidget::ClearTransparentWindow() {
  MutexAutoLock lock(mTransparentSurfaceLock);
  if (!mTransparentSurface) {
    return;
  }

  EnsureTransparentSurface();

  IntSize size = mTransparentSurface->GetSize();
  if (!size.IsEmpty()) {
    RefPtr<DrawTarget> drawTarget =
        gfxPlatform::CreateDrawTargetForSurface(mTransparentSurface, size);
    if (!drawTarget) {
      return;
    }
    drawTarget->ClearRect(Rect(0, 0, size.width, size.height));
    RedrawTransparentWindow();
  }
}

bool WinCompositorWidget::RedrawTransparentWindow() {
  MOZ_ASSERT(mTransparencyMode == eTransparencyTransparent);

  LayoutDeviceIntSize size = GetClientSize();

  ::GdiFlush();

  BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  SIZE winSize = {size.width, size.height};
  POINT srcPos = {0, 0};
  HWND hWnd = WinUtils::GetTopLevelHWND(mWnd, true);
  RECT winRect;
  ::GetWindowRect(hWnd, &winRect);

  // perform the alpha blend
  return !!::UpdateLayeredWindow(hWnd, nullptr, (POINT*)&winRect, &winSize,
                                 mMemoryDC, &srcPos, 0, &bf, ULW_ALPHA);
}

HDC WinCompositorWidget::GetWindowSurface() {
  return eTransparencyTransparent == mTransparencyMode ? mMemoryDC
                                                       : ::GetDC(mWnd);
}

void WinCompositorWidget::FreeWindowSurface(HDC dc) {
  if (eTransparencyTransparent != mTransparencyMode) ::ReleaseDC(mWnd, dc);
}

bool WinCompositorWidget::IsHidden() const { return ::IsIconic(mWnd); }

void WinCompositorWidget::EnsureCompositorWindow() {
  if (mCompositorWnds.mCompositorWnd || mCompositorWnds.mInitialParentWnd) {
    return;
  }

  mCompositorWnds = WinCompositorWindowThread::CreateCompositorWindow();
  UpdateCompositorWnd(mCompositorWnds.mCompositorWnd, mWnd);

  MOZ_ASSERT(mCompositorWnds.mCompositorWnd);
  MOZ_ASSERT(mCompositorWnds.mInitialParentWnd);
}

void WinCompositorWidget::DestroyCompositorWindow() {
  if (!mCompositorWnds.mCompositorWnd && !mCompositorWnds.mInitialParentWnd) {
    return;
  }
  WinCompositorWindowThread::DestroyCompositorWindow(mCompositorWnds);
  mCompositorWnds = WinCompositorWnds(nullptr, nullptr);
}

void WinCompositorWidget::UpdateCompositorWndSizeIfNecessary() {
  if (!mCompositorWnds.mCompositorWnd) {
    return;
  }

  LayoutDeviceIntSize size = GetClientSize();
  if (mLastCompositorWndSize == size) {
    return;
  }

  // Force a resize and redraw (but not a move, activate, etc.).
  if (!::SetWindowPos(mCompositorWnds.mCompositorWnd, nullptr, 0, 0, size.width,
                      size.height,
                      SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                          SWP_NOOWNERZORDER | SWP_NOZORDER)) {
    return;
  }
  mLastCompositorWndSize = size;
}

}  // namespace widget
}  // namespace mozilla
