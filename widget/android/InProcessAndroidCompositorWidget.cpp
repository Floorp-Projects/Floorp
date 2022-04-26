/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessCompositorWidget.h"
#include "HeadlessWidget.h"
#include "mozilla/widget/PlatformWidgetTypes.h"

#include "InProcessAndroidCompositorWidget.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

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
    return new InProcessAndroidCompositorWidget(
        aInitData.get_AndroidCompositorWidgetInitData(), aOptions,
        static_cast<nsWindow*>(aWidget));
  }
}

InProcessAndroidCompositorWidget::InProcessAndroidCompositorWidget(
    const AndroidCompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, nsWindow* aWindow)
    : AndroidCompositorWidget(aInitData, aOptions), mWindow(aWindow) {}

void InProcessAndroidCompositorWidget::ObserveVsync(VsyncObserver* aObserver) {
  if (RefPtr<CompositorVsyncDispatcher> cvd =
          mWindow->GetCompositorVsyncDispatcher()) {
    cvd->SetCompositorVsyncObserver(aObserver);
  }
}

nsIWidget* InProcessAndroidCompositorWidget::RealWidget() { return mWindow; }

void InProcessAndroidCompositorWidget::OnCompositorSurfaceChanged() {
  mSurface = java::sdk::Surface::Ref::From(
      static_cast<jobject>(mWindow->GetNativeData(NS_JAVA_SURFACE)));
}

void InProcessAndroidCompositorWidget::NotifyClientSizeChanged(
    const LayoutDeviceIntSize& aClientSize) {
  AndroidCompositorWidget::NotifyClientSizeChanged(aClientSize);
}

}  // namespace widget
}  // namespace mozilla
