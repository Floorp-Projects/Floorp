/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _widget_windows_WinCompositorWidget_h__
#define _widget_windows_WinCompositorWidget_h__

#include "WinCompositorWidget.h"
#include "mozilla/widget/PCompositorWidgetParent.h"

namespace mozilla {
namespace widget {

class CompositorWidgetParent final
 : public PCompositorWidgetParent,
   public WinCompositorWidget
{
public:
  CompositorWidgetParent(const CompositorWidgetInitData& aInitData);
  ~CompositorWidgetParent() override;

  bool RecvEnterPresentLock() override;
  bool RecvLeavePresentLock() override;
  bool RecvUpdateTransparency(const int32_t& aMode) override;
  bool RecvClearTransparentWindow() override;
  bool RecvResizeTransparentWindow(const IntSize& aSize) override;
  void ActorDestroy(ActorDestroyReason aWhy) override;

  nsIWidget* RealWidget() override;
  void ObserveVsync(VsyncObserver* aObserver) override;
  RefPtr<VsyncObserver> GetVsyncObserver() const override;

private:
  RefPtr<VsyncObserver> mVsyncObserver;
};

} // namespace widget
} // namespace mozilla

#endif // _widget_windows_WinCompositorWidget_h__
