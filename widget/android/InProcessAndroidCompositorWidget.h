/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_android_InProcessAndroidCompositorWidget_h
#define widget_android_InProcessAndroidCompositorWidget_h

#include "AndroidCompositorWidget.h"
#include "CompositorWidget.h"

class nsWindow;

namespace mozilla {
namespace widget {

class InProcessAndroidCompositorWidget final
    : public AndroidCompositorWidget,
      public PlatformCompositorWidgetDelegate {
 public:
  InProcessAndroidCompositorWidget(
      const AndroidCompositorWidgetInitData& aInitData,
      const layers::CompositorOptions& aOptions, nsWindow* aWidget);

  // CompositorWidget overrides

  void ObserveVsync(VsyncObserver* aObserver) override;
  nsIWidget* RealWidget() override;
  CompositorWidgetDelegate* AsDelegate() override { return this; }

  // PlatformCompositorWidgetDelegate overrides

  void NotifyClientSizeChanged(const LayoutDeviceIntSize& aClientSize) override;

 private:
  // AndroidCompositorWidget overrides
  void OnCompositorSurfaceChanged() override;

  nsWindow* mWindow;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_android_InProcessAndroidCompositorWidget_h
