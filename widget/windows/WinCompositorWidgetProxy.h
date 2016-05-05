/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _widget_windows_WinCompositorWidget_h__
#define _widget_windows_WinCompositorWidget_h__

#include "CompositorWidgetProxy.h"
#include "mozilla/gfx/CriticalSection.h"

class nsWindow;

namespace mozilla {
namespace widget {
 
// This is the Windows-specific implementation of CompositorWidgetProxy. For
// the most part it only requires an HWND, however it maintains extra state
// for transparent windows, as well as for synchronizing WM_SETTEXT messages
// with the compositor.
class WinCompositorWidgetProxy : public CompositorWidgetProxy
{
public:
  WinCompositorWidgetProxy(nsWindow* aWindow);

  bool PreRender(layers::LayerManagerComposite*) override;
  void PostRender(layers::LayerManagerComposite*) override;
  already_AddRefed<gfx::DrawTarget> StartRemoteDrawing() override;
  void EndRemoteDrawing() override;
  LayoutDeviceIntSize GetClientSize() override;
  already_AddRefed<CompositorVsyncDispatcher> GetCompositorVsyncDispatcher() override;
  uintptr_t GetWidgetKey() override;
  nsIWidget* RealWidget() override;
  WinCompositorWidgetProxy* AsWindowsProxy() override {
    return this;
  }

  // Callbacks for nsWindow.
  void EnterPresentLock();
  void LeavePresentLock();
  void OnDestroyWindow();

  // Transparency handling.
  void UpdateTransparency(nsTransparencyMode aMode);
  void ClearTransparentWindow();
  bool RedrawTransparentWindow();

  // Update the bounds of the transparent surface.
  void ResizeTransparentWindow(int32_t aNewWidth, int32_t aNewHeight);

  // Ensure that a transparent surface exists, then return it.
  RefPtr<gfxASurface> EnsureTransparentSurface();

  HDC GetTransparentDC() const {
    return mMemoryDC;
  }
  HWND GetHwnd() const {
    return mWnd;
  }

private:
  HDC GetWindowSurface();
  void FreeWindowSurface(HDC dc);

  void CreateTransparentSurface(int32_t aWidth, int32_t aHeight);

private:
  nsWindow* mWindow;
  uintptr_t mWidgetKey;
  HWND mWnd;
  gfx::CriticalSection mPresentLock;

  // Transparency handling.
  nsTransparencyMode mTransparencyMode;
  RefPtr<gfxASurface> mTransparentSurface;
  HDC mMemoryDC;
  HDC mCompositeDC;
};

} // namespace widget
} // namespace mozilla

#endif // _widget_windows_WinCompositorWidget_h__
