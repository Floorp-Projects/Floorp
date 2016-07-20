/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidgetParent.h"

namespace mozilla {
namespace widget {

CompositorWidgetParent::CompositorWidgetParent(const CompositorWidgetInitData& aInitData)
 : WinCompositorWidget(aInitData)
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
}

CompositorWidgetParent::~CompositorWidgetParent()
{
}

bool
CompositorWidgetParent::RecvEnterPresentLock()
{
  EnterPresentLock();
  return true;
}

bool
CompositorWidgetParent::RecvLeavePresentLock()
{
  LeavePresentLock();
  return true;
}

bool
CompositorWidgetParent::RecvUpdateTransparency(const int32_t& aMode)
{
  UpdateTransparency(static_cast<nsTransparencyMode>(aMode));
  return true;
}

bool
CompositorWidgetParent::RecvClearTransparentWindow()
{
  ClearTransparentWindow();
  return true;
}

bool
CompositorWidgetParent::RecvResizeTransparentWindow(const IntSize& aSize)
{
  ResizeTransparentWindow(aSize);
  return true;
}

nsIWidget*
CompositorWidgetParent::RealWidget()
{
  return nullptr;
}

void
CompositorWidgetParent::ObserveVsync(VsyncObserver* aObserver)
{
  if (aObserver) {
    SendObserveVsync();
  } else {
    SendUnobserveVsync();
  }
  mVsyncObserver = aObserver;
}

RefPtr<VsyncObserver>
CompositorWidgetParent::GetVsyncObserver() const
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_GPU);
  return mVsyncObserver;
}

void
CompositorWidgetParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace widget
} // namespace mozilla
