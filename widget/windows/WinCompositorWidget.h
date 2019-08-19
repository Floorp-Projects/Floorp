/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_WinCompositorWidget_h
#define widget_windows_WinCompositorWidget_h

#include "CompositorWidget.h"
#include "gfxASurface.h"
#include "mozilla/gfx/CriticalSection.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/Mutex.h"
#include "mozilla/widget/WinCompositorWindowThread.h"
#include "nsIWidget.h"

class nsWindow;

namespace mozilla {
namespace widget {

class PlatformCompositorWidgetDelegate : public CompositorWidgetDelegate {
 public:
  // Callbacks for nsWindow.
  virtual void EnterPresentLock() = 0;
  virtual void LeavePresentLock() = 0;
  virtual void OnDestroyWindow() = 0;

  // Transparency handling.
  virtual void UpdateTransparency(nsTransparencyMode aMode) = 0;
  virtual void ClearTransparentWindow() = 0;

  // If in-process and using software rendering, return the backing transparent
  // DC.
  virtual HDC GetTransparentDC() const = 0;
  virtual void SetParentWnd(const HWND aParentWnd) {}
  virtual void UpdateCompositorWnd(const HWND aCompositorWnd,
                                   const HWND aParentWnd) {}

  // CompositorWidgetDelegate Overrides

  PlatformCompositorWidgetDelegate* AsPlatformSpecificDelegate() override {
    return this;
  }
};

class WinCompositorWidgetInitData;

// This is the Windows-specific implementation of CompositorWidget. For
// the most part it only requires an HWND, however it maintains extra state
// for transparent windows, as well as for synchronizing WM_SETTEXT messages
// with the compositor.
class WinCompositorWidget : public CompositorWidget,
                            public PlatformCompositorWidgetDelegate {
 public:
  WinCompositorWidget(const WinCompositorWidgetInitData& aInitData,
                      const layers::CompositorOptions& aOptions);
  ~WinCompositorWidget() override;

  // CompositorWidget Overrides

  bool PreRender(WidgetRenderingContext*) override;
  void PostRender(WidgetRenderingContext*) override;
  already_AddRefed<gfx::DrawTarget> StartRemoteDrawing() override;
  void EndRemoteDrawing() override;
  bool NeedsToDeferEndRemoteDrawing() override;
  LayoutDeviceIntSize GetClientSize() override;
  already_AddRefed<gfx::DrawTarget> GetBackBufferDrawTarget(
      gfx::DrawTarget* aScreenTarget, const LayoutDeviceIntRect& aRect,
      bool* aOutIsCleared) override;
  already_AddRefed<gfx::SourceSurface> EndBackBufferDrawing() override;
  bool InitCompositor(layers::Compositor* aCompositor) override;
  uintptr_t GetWidgetKey() override;
  WinCompositorWidget* AsWindows() override { return this; }
  CompositorWidgetDelegate* AsDelegate() override { return this; }
  bool IsHidden() const override;

  // PlatformCompositorWidgetDelegate Overrides

  void EnterPresentLock() override;
  void LeavePresentLock() override;
  void OnDestroyWindow() override;
  void UpdateTransparency(nsTransparencyMode aMode) override;
  void ClearTransparentWindow() override;

  bool RedrawTransparentWindow();

  // Ensure that a transparent surface exists, then return it.
  RefPtr<gfxASurface> EnsureTransparentSurface();

  HDC GetTransparentDC() const override { return mMemoryDC; }
  HWND GetHwnd() const {
    return mCompositorWnds.mCompositorWnd ? mCompositorWnds.mCompositorWnd
                                          : mWnd;
  }

  HWND GetCompositorHwnd() const { return mCompositorWnds.mCompositorWnd; }

  void EnsureCompositorWindow();
  void DestroyCompositorWindow();
  void UpdateCompositorWndSizeIfNecessary();

  mozilla::Mutex& GetTransparentSurfaceLock() {
    return mTransparentSurfaceLock;
  }

 protected:
 private:
  HDC GetWindowSurface();
  void FreeWindowSurface(HDC dc);

  void CreateTransparentSurface(const gfx::IntSize& aSize);

 private:
  uintptr_t mWidgetKey;
  HWND mWnd;

  WinCompositorWnds mCompositorWnds;
  LayoutDeviceIntSize mLastCompositorWndSize;

  gfx::CriticalSection mPresentLock;

  // Transparency handling.
  mozilla::Mutex mTransparentSurfaceLock;
  nsTransparencyMode mTransparencyMode;
  RefPtr<gfxASurface> mTransparentSurface;
  HDC mMemoryDC;
  HDC mCompositeDC;

  // Locked back buffer of BasicCompositor
  uint8_t* mLockedBackBufferData;

  bool mNotDeferEndRemoteDrawing;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_windows_WinCompositorWidget_h
