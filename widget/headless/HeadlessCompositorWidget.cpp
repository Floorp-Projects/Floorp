/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/widget/PlatformWidgetTypes.h"
#include "HeadlessCompositorWidget.h"
#include "VsyncDispatcher.h"

namespace mozilla {
namespace widget {

HeadlessCompositorWidget::HeadlessCompositorWidget(
    const HeadlessCompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, HeadlessWidget* aWindow)
    : CompositorWidget(aOptions),
      mWidget(aWindow),
      mClientSize(LayoutDeviceIntSize(aInitData.InitialClientSize()),
                  "HeadlessCompositorWidget::mClientSize") {}

void HeadlessCompositorWidget::ObserveVsync(VsyncObserver* aObserver) {
  if (RefPtr<CompositorVsyncDispatcher> cvd =
          mWidget->GetCompositorVsyncDispatcher()) {
    cvd->SetCompositorVsyncObserver(aObserver);
  }
}

nsIWidget* HeadlessCompositorWidget::RealWidget() { return mWidget; }

void HeadlessCompositorWidget::NotifyClientSizeChanged(
    const LayoutDeviceIntSize& aClientSize) {
  auto size = mClientSize.Lock();
  *size = aClientSize;
}

LayoutDeviceIntSize HeadlessCompositorWidget::GetClientSize() {
  auto size = mClientSize.Lock();
  return *size;
}

uintptr_t HeadlessCompositorWidget::GetWidgetKey() {
  return reinterpret_cast<uintptr_t>(mWidget);
}

}  // namespace widget
}  // namespace mozilla
