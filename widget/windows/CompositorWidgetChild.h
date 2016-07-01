/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_windows_CompositorWidgetChild_h
#define widget_windows_CompositorWidgetChild_h

#include "WinCompositorWidget.h"
#include "mozilla/widget/PCompositorWidgetChild.h"

namespace mozilla {
namespace widget {

class CompositorWidgetChild final
 : public PCompositorWidgetChild,
   public CompositorWidgetDelegate
{
public:
  CompositorWidgetChild(nsIWidget* aWidget);
  ~CompositorWidgetChild() override;

  void EnterPresentLock() override;
  void LeavePresentLock() override;
  void OnDestroyWindow() override;
  void UpdateTransparency(nsTransparencyMode aMode) override;
  void ClearTransparentWindow() override;
  void ResizeTransparentWindow(const gfx::IntSize& aSize) override;
  HDC GetTransparentDC() const override;
};

} // namespace widget
} // namespace mozilla

#endif // widget_windows_CompositorWidgetChild_h
