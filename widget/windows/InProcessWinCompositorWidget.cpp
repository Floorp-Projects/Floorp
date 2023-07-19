/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InProcessWinCompositorWidget.h"

#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "gfxPlatform.h"
#include "HeadlessCompositorWidget.h"
#include "HeadlessWidget.h"
#include "nsIWidget.h"
#include "nsWindow.h"
#include "VsyncDispatcher.h"
#include "WinCompositorWindowThread.h"
#include "VRShMem.h"

#include <ddraw.h>

namespace mozilla::widget {

using namespace mozilla::gfx;
using namespace mozilla;

/* static */
RefPtr<CompositorWidget> CompositorWidget::CreateLocal(
    const CompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, nsIWidget* aWidget) {
  if (aInitData.type() ==
      CompositorWidgetInitData::THeadlessCompositorWidgetInitData) {
    return new HeadlessCompositorWidget(
        aInitData.get_HeadlessCompositorWidgetInitData(), aOptions,
        static_cast<HeadlessWidget*>(aWidget));
  } else {
    return new InProcessWinCompositorWidget(
        aInitData.get_WinCompositorWidgetInitData(), aOptions,
        static_cast<nsWindow*>(aWidget));
  }
}

InProcessWinCompositorWidget::InProcessWinCompositorWidget(
    const WinCompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, nsWindow* aWindow)
    : WinCompositorWidget(aInitData, aOptions),
      mWindow(aWindow),
      mWnd(reinterpret_cast<HWND>(aInitData.hWnd())),
      mTransparentSurfaceLock("mTransparentSurfaceLock"),
      mTransparencyMode(uint32_t(aInitData.transparencyMode())),
      mMemoryDC(nullptr),
      mCompositeDC(nullptr),
      mLockedBackBufferData(nullptr) {
  MOZ_ASSERT(mWindow);
  MOZ_ASSERT(mWnd && ::IsWindow(mWnd));

  // mNotDeferEndRemoteDrawing is set on the main thread during init,
  // but is only accessed after on the compositor thread.
  mNotDeferEndRemoteDrawing =
      StaticPrefs::layers_offmainthreadcomposition_frame_rate() == 0 ||
      gfxPlatform::IsInLayoutAsapMode() || gfxPlatform::ForceSoftwareVsync();
}

void InProcessWinCompositorWidget::OnDestroyWindow() {
  gfx::CriticalSectionAutoEnter presentLock(&mPresentLock);
  MutexAutoLock lock(mTransparentSurfaceLock);
  mTransparentSurface = nullptr;
  mMemoryDC = nullptr;
}

bool InProcessWinCompositorWidget::OnWindowResize(
    const LayoutDeviceIntSize& aSize) {
  return true;
}

void InProcessWinCompositorWidget::OnWindowModeChange(nsSizeMode aSizeMode) {}

bool InProcessWinCompositorWidget::PreRender(WidgetRenderingContext* aContext) {
  // This can block waiting for WM_SETTEXT to finish
  // Using PreRender is unnecessarily pessimistic because
  // we technically only need to block during the present call
  // not all of compositor rendering
  mPresentLock.Enter();
  return true;
}

void InProcessWinCompositorWidget::PostRender(
    WidgetRenderingContext* aContext) {
  mPresentLock.Leave();
}

LayoutDeviceIntSize InProcessWinCompositorWidget::GetClientSize() {
  RECT r;
  if (!::GetClientRect(mWnd, &r)) {
    return LayoutDeviceIntSize();
  }
  return LayoutDeviceIntSize(r.right - r.left, r.bottom - r.top);
}

already_AddRefed<gfx::DrawTarget>
InProcessWinCompositorWidget::StartRemoteDrawing() {
  MutexAutoLock lock(mTransparentSurfaceLock);

  MOZ_ASSERT(!mCompositeDC);

  RefPtr<gfxASurface> surf;
  if (TransparencyModeIs(TransparencyMode::Transparent)) {
    surf = EnsureTransparentSurface();
  }

  // Must call this after EnsureTransparentSurface(), since it could update
  // the DC.
  HDC dc = GetWindowSurface();
  if (!surf) {
    if (!dc) {
      return nullptr;
    }
    uint32_t flags = TransparencyModeIs(TransparencyMode::Opaque)
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

void InProcessWinCompositorWidget::EndRemoteDrawing() {
  MOZ_ASSERT(!mLockedBackBufferData);

  if (TransparencyModeIs(TransparencyMode::Transparent)) {
    MOZ_ASSERT(mTransparentSurface);
    RedrawTransparentWindow();
  }
  if (mCompositeDC) {
    FreeWindowSurface(mCompositeDC);
  }
  mCompositeDC = nullptr;
}

bool InProcessWinCompositorWidget::NeedsToDeferEndRemoteDrawing() {
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

already_AddRefed<gfx::DrawTarget>
InProcessWinCompositorWidget::GetBackBufferDrawTarget(
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
InProcessWinCompositorWidget::EndBackBufferDrawing() {
  if (mLockedBackBufferData) {
    MOZ_ASSERT(mLastBackBuffer);
    mLastBackBuffer->ReleaseBits(mLockedBackBufferData);
    mLockedBackBufferData = nullptr;
  }
  return CompositorWidget::EndBackBufferDrawing();
}

bool InProcessWinCompositorWidget::InitCompositor(
    layers::Compositor* aCompositor) {
  return true;
}

void InProcessWinCompositorWidget::EnterPresentLock() { mPresentLock.Enter(); }

void InProcessWinCompositorWidget::LeavePresentLock() { mPresentLock.Leave(); }

RefPtr<gfxASurface> InProcessWinCompositorWidget::EnsureTransparentSurface() {
  mTransparentSurfaceLock.AssertCurrentThreadOwns();
  MOZ_ASSERT(TransparencyModeIs(TransparencyMode::Transparent));

  IntSize size = GetClientSize().ToUnknownSize();
  if (!mTransparentSurface || mTransparentSurface->GetSize() != size) {
    mTransparentSurface = nullptr;
    mMemoryDC = nullptr;
    CreateTransparentSurface(size);
  }

  RefPtr<gfxASurface> surface = mTransparentSurface;
  return surface.forget();
}

void InProcessWinCompositorWidget::CreateTransparentSurface(
    const gfx::IntSize& aSize) {
  mTransparentSurfaceLock.AssertCurrentThreadOwns();
  MOZ_ASSERT(!mTransparentSurface && !mMemoryDC);
  RefPtr<gfxWindowsSurface> surface =
      new gfxWindowsSurface(aSize, SurfaceFormat::A8R8G8B8_UINT32);
  mTransparentSurface = surface;
  mMemoryDC = surface->GetDC();
}

void InProcessWinCompositorWidget::UpdateTransparency(TransparencyMode aMode) {
  gfx::CriticalSectionAutoEnter presentLock(&mPresentLock);
  MutexAutoLock lock(mTransparentSurfaceLock);
  if (TransparencyModeIs(aMode)) {
    return;
  }

  mTransparencyMode = uint32_t(aMode);
  mTransparentSurface = nullptr;
  mMemoryDC = nullptr;

  if (aMode == TransparencyMode::Transparent) {
    EnsureTransparentSurface();
  }
}

void InProcessWinCompositorWidget::NotifyVisibilityUpdated(
    nsSizeMode aSizeMode, bool aIsFullyOccluded) {
  mSizeMode = aSizeMode;
  mIsFullyOccluded = aIsFullyOccluded;
}

nsSizeMode InProcessWinCompositorWidget::GetWindowSizeMode() const {
  nsSizeMode sizeMode = mSizeMode;
  return sizeMode;
}

bool InProcessWinCompositorWidget::GetWindowIsFullyOccluded() const {
  bool isFullyOccluded = mIsFullyOccluded;
  return isFullyOccluded;
}

void InProcessWinCompositorWidget::ClearTransparentWindow() {
  gfx::CriticalSectionAutoEnter presentLock(&mPresentLock);
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

bool InProcessWinCompositorWidget::RedrawTransparentWindow() {
  MOZ_ASSERT(TransparencyModeIs(TransparencyMode::Transparent));

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

HDC InProcessWinCompositorWidget::GetWindowSurface() {
  return TransparencyModeIs(TransparencyMode::Transparent) ? mMemoryDC
                                                           : ::GetDC(mWnd);
}

void InProcessWinCompositorWidget::FreeWindowSurface(HDC dc) {
  if (!TransparencyModeIs(TransparencyMode::Transparent)) {
    ::ReleaseDC(mWnd, dc);
  }
}

bool InProcessWinCompositorWidget::IsHidden() const { return ::IsIconic(mWnd); }

nsIWidget* InProcessWinCompositorWidget::RealWidget() { return mWindow; }

void InProcessWinCompositorWidget::ObserveVsync(VsyncObserver* aObserver) {
  if (RefPtr<CompositorVsyncDispatcher> cvd =
          mWindow->GetCompositorVsyncDispatcher()) {
    cvd->SetCompositorVsyncObserver(aObserver);
  }
}

void InProcessWinCompositorWidget::UpdateCompositorWnd(
    const HWND aCompositorWnd, const HWND aParentWnd) {
  MOZ_ASSERT(layers::CompositorThreadHolder::IsInCompositorThread());
  MOZ_ASSERT(aCompositorWnd && aParentWnd);
  MOZ_ASSERT(aParentWnd == mWnd);

  // Since we're in the parent process anyway, we can just call SetParent
  // directly.
  ::SetParent(aCompositorWnd, aParentWnd);
  mSetParentCompleted = true;
}
}  // namespace mozilla::widget
