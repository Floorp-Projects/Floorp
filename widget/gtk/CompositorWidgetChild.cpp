/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidgetChild.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace widget {

CompositorWidgetChild::CompositorWidgetChild(RefPtr<CompositorVsyncDispatcher> aVsyncDispatcher,
                                             RefPtr<CompositorWidgetVsyncObserver> aVsyncObserver)
  : mVsyncDispatcher(aVsyncDispatcher)
  , mVsyncObserver(aVsyncObserver)
{
  MOZ_ASSERT(XRE_IsParentProcess());
}

CompositorWidgetChild::~CompositorWidgetChild()
{
}

mozilla::ipc::IPCResult
CompositorWidgetChild::RecvObserveVsync()
{
  mVsyncDispatcher->SetCompositorVsyncObserver(mVsyncObserver);
  return IPC_OK();
}

mozilla::ipc::IPCResult
CompositorWidgetChild::RecvUnobserveVsync()
{
  mVsyncDispatcher->SetCompositorVsyncObserver(nullptr);
  return IPC_OK();
}

void
CompositorWidgetChild::NotifyClientSizeChanged(const LayoutDeviceIntSize& aClientSize)
{
  Unused << SendNotifyClientSizeChanged(aClientSize);
}

} // namespace widget
} // namespace mozilla
