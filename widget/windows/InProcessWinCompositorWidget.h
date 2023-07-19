/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_InProcessWinCompositorWidget_h
#define widget_windows_InProcessWinCompositorWidget_h

#include "WinCompositorWidget.h"

class nsWindow;
class gfxASurface;

namespace mozilla::widget {

// This is the Windows-specific implementation of CompositorWidget. For
// the most part it only requires an HWND, however it maintains extra state
// for transparent windows, as well as for synchronizing WM_SETTEXT messages
// with the compositor.
class InProcessWinCompositorWidget final
    : public WinCompositorWidget,
      public PlatformCompositorWidgetDelegate {
 public:
  InProcessWinCompositorWidget(const WinCompositorWidgetInitData& aInitData,
                               const layers::CompositorOptions& aOptions,
                               nsWindow* aWindow);

  bool PreRender(WidgetRenderingContext*) override;
  void PostRender(WidgetRenderingContext*) override;
  already_AddRefed<gfx::DrawTarget> StartRemoteDrawing() override;
  void EndRemoteDrawing() override;
  bool NeedsToDeferEndRemoteDrawing() override;
  LayoutDeviceIntSize GetClientSize() override;
  already_AddRefed<gfx::DrawTarget> GetBackBufferDrawTarget(
      gfx::DrawTarget* aScreenTarget, const gfx::IntRect& aRect,
      bool* aOutIsCleared) override;
  already_AddRefed<gfx::SourceSurface> EndBackBufferDrawing() override;
  bool InitCompositor(layers::Compositor* aCompositor) override;
  CompositorWidgetDelegate* AsDelegate() override { return this; }
  bool IsHidden() const override;

  // PlatformCompositorWidgetDelegate Overrides

  void EnterPresentLock() override;
  void LeavePresentLock() override;
  void OnDestroyWindow() override;
  bool OnWindowResize(const LayoutDeviceIntSize& aSize) override;
  void OnWindowModeChange(nsSizeMode aSizeMode) override;
  void UpdateTransparency(TransparencyMode aMode) override;
  void NotifyVisibilityUpdated(nsSizeMode aSizeMode,
                               bool aIsFullyOccluded) override;
  void ClearTransparentWindow() override;

  bool RedrawTransparentWindow();

  // Ensure that a transparent surface exists, then return it.
  RefPtr<gfxASurface> EnsureTransparentSurface();

  HDC GetTransparentDC() const { return mMemoryDC; }

  mozilla::Mutex& GetTransparentSurfaceLock() {
    return mTransparentSurfaceLock;
  }

  nsSizeMode GetWindowSizeMode() const override;
  bool GetWindowIsFullyOccluded() const override;

  void ObserveVsync(VsyncObserver* aObserver) override;
  nsIWidget* RealWidget() override;

  void UpdateCompositorWnd(const HWND aCompositorWnd,
                           const HWND aParentWnd) override;
  void SetRootLayerTreeID(const layers::LayersId& aRootLayerTreeId) override {}

 private:
  HDC GetWindowSurface();
  void FreeWindowSurface(HDC dc);

  void CreateTransparentSurface(const gfx::IntSize& aSize);

  nsWindow* mWindow;

  HWND mWnd;

  gfx::CriticalSection mPresentLock;

  // Transparency handling.
  mozilla::Mutex mTransparentSurfaceLock MOZ_UNANNOTATED;
  mozilla::Atomic<uint32_t, MemoryOrdering::Relaxed> mTransparencyMode;

  bool TransparencyModeIs(TransparencyMode aMode) const {
    return TransparencyMode(uint32_t(mTransparencyMode)) == aMode;
  }

  // Visibility handling.
  mozilla::Atomic<nsSizeMode, MemoryOrdering::Relaxed> mSizeMode;
  mozilla::Atomic<bool, MemoryOrdering::Relaxed> mIsFullyOccluded;

  RefPtr<gfxASurface> mTransparentSurface;
  HDC mMemoryDC;
  HDC mCompositeDC;

  // Locked back buffer of BasicCompositor
  uint8_t* mLockedBackBufferData;

  bool mNotDeferEndRemoteDrawing;
};

}  // namespace mozilla::widget

#endif  // widget_windows_InProcessWinCompositorWidget_h
