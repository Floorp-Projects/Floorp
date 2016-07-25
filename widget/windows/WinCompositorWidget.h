/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_CompositorWidgetParent_h
#define widget_windows_CompositorWidgetParent_h

#include "CompositorWidget.h"
#include "gfxASurface.h"
#include "mozilla/gfx/CriticalSection.h"
#include "mozilla/gfx/Point.h"
#include "nsIWidget.h"

class nsWindow;

namespace mozilla {
namespace widget {

class CompositorWidgetDelegate
{
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
};
 
// This is the Windows-specific implementation of CompositorWidget. For
// the most part it only requires an HWND, however it maintains extra state
// for transparent windows, as well as for synchronizing WM_SETTEXT messages
// with the compositor.
class WinCompositorWidget
 : public CompositorWidget,
   public CompositorWidgetDelegate
{
public:
  WinCompositorWidget(const CompositorWidgetInitData& aInitData);

  bool PreRender(layers::LayerManagerComposite*) override;
  void PostRender(layers::LayerManagerComposite*) override;
  already_AddRefed<gfx::DrawTarget> StartRemoteDrawing() override;
  void EndRemoteDrawing() override;
  LayoutDeviceIntSize GetClientSize() override;
  already_AddRefed<gfx::DrawTarget> GetBackBufferDrawTarget(gfx::DrawTarget* aScreenTarget,
                                                            const LayoutDeviceIntRect& aRect,
                                                            const LayoutDeviceIntRect& aClearRect) override;
  already_AddRefed<gfx::SourceSurface> EndBackBufferDrawing() override;
  uintptr_t GetWidgetKey() override;
  WinCompositorWidget* AsWindows() override {
    return this;
  }
  CompositorWidgetDelegate* AsDelegate() override {
    return this;
  }

  // CompositorWidgetDelegate overrides.
  void EnterPresentLock() override;
  void LeavePresentLock() override;
  void OnDestroyWindow() override;
  void UpdateTransparency(nsTransparencyMode aMode) override;
  void ClearTransparentWindow() override;

  bool RedrawTransparentWindow();

  // Ensure that a transparent surface exists, then return it.
  RefPtr<gfxASurface> EnsureTransparentSurface();

  HDC GetTransparentDC() const override {
    return mMemoryDC;
  }
  HWND GetHwnd() const {
    return mWnd;
  }

private:
  HDC GetWindowSurface();
  void FreeWindowSurface(HDC dc);

  void CreateTransparentSurface(const gfx::IntSize& aSize);

private:
  uintptr_t mWidgetKey;
  HWND mWnd;
  gfx::CriticalSection mPresentLock;

  // Transparency handling.
  nsTransparencyMode mTransparencyMode;
  RefPtr<gfxASurface> mTransparentSurface;
  HDC mMemoryDC;
  HDC mCompositeDC;

  // Locked back buffer of BasicCompositor
  uint8_t* mLockedBackBufferData;
};

} // namespace widget
} // namespace mozilla

#endif // widget_windows_CompositorWidgetParent_h
