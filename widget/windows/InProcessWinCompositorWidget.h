/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_InProcessCompositorWidgetParent_h
#define widget_windows_InProcessCompositorWidgetParent_h

#include "WinCompositorWidget.h"

class nsWindow;

namespace mozilla {
namespace widget {

// This is the Windows-specific implementation of CompositorWidget. For
// the most part it only requires an HWND, however it maintains extra state
// for transparent windows, as well as for synchronizing WM_SETTEXT messages
// with the compositor.
class InProcessWinCompositorWidget final : public WinCompositorWidget
{
public:
  InProcessWinCompositorWidget(const CompositorWidgetInitData& aInitData, nsWindow* aWindow);

  void ObserveVsync(VsyncObserver* aObserver) override;
  nsIWidget* RealWidget() override;

private:
  nsWindow* mWindow;
};

} // namespace widget
} // namespace mozilla

#endif // widget_windows_InProcessCompositorWidgetParent_h
