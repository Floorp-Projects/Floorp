/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _widget_windows_WinCompositorWidget_h__
#define _widget_windows_WinCompositorWidget_h__

#include "CompositorWidgetProxy.h"

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
  nsIWidget* RealWidget() override;

  NS_INLINE_DECL_REFCOUNTING(mozilla::Widget::WinCompositorWidgetProxy)

private:
  nsWindow* mWindow;
  HWND mWnd;
};

} // namespace widget
} // namespace mozilla

#endif // _widget_windows_WinCompositorWidget_h__
