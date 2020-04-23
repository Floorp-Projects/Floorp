/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidgetParent.h"
#include "mozilla/Unused.h"
#include "mozilla/widget/PlatformWidgetTypes.h"

namespace mozilla {
namespace widget {

CompositorWidgetParent::CompositorWidgetParent(
    const CompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions)
    : GtkCompositorWidget(aInitData.get_GtkCompositorWidgetInitData(), aOptions,
                          nullptr) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
}

CompositorWidgetParent::~CompositorWidgetParent() = default;

void CompositorWidgetParent::ObserveVsync(VsyncObserver* aObserver) {
  if (aObserver) {
    Unused << SendObserveVsync();
  } else {
    Unused << SendUnobserveVsync();
  }
  mVsyncObserver = aObserver;
}

RefPtr<VsyncObserver> CompositorWidgetParent::GetVsyncObserver() const {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
  return mVsyncObserver;
}

mozilla::ipc::IPCResult CompositorWidgetParent::RecvNotifyClientSizeChanged(
    const LayoutDeviceIntSize& aClientSize) {
  NotifyClientSizeChanged(aClientSize);
  return IPC_OK();
}

}  // namespace widget
}  // namespace mozilla
