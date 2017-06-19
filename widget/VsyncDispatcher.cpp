/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadUtils.h"
#include "VsyncDispatcher.h"
#include "VsyncSource.h"
#include "gfxPlatform.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorThread.h"

using namespace mozilla::layers;

namespace mozilla {

CompositorVsyncDispatcher::CompositorVsyncDispatcher()
  : mCompositorObserverLock("CompositorObserverLock")
  , mDidShutdown(false)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

CompositorVsyncDispatcher::~CompositorVsyncDispatcher()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  // We auto remove this vsync dispatcher from the vsync source in the nsBaseWidget
}

void
CompositorVsyncDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // In vsync thread
#ifdef MOZ_GECKO_PROFILER
  layers::CompositorBridgeParent::PostInsertVsyncProfilerMarker(aVsyncTimestamp);
#endif

  MutexAutoLock lock(mCompositorObserverLock);
  if (mCompositorVsyncObserver) {
    mCompositorVsyncObserver->NotifyVsync(aVsyncTimestamp);
  }
}

void
CompositorVsyncDispatcher::ObserveVsync(bool aEnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  if (mDidShutdown) {
    return;
  }

  if (aEnable) {
    gfxPlatform::GetPlatform()->GetHardwareVsync()->AddCompositorVsyncDispatcher(this);
  } else {
    gfxPlatform::GetPlatform()->GetHardwareVsync()->RemoveCompositorVsyncDispatcher(this);
  }
}

void
CompositorVsyncDispatcher::SetCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  // When remote compositing or running gtests, vsync observation is
  // initiated on the main thread. Otherwise, it is initiated from the compositor
  // thread.
  MOZ_ASSERT(NS_IsMainThread() || CompositorThreadHolder::IsInCompositorThread());

  { // scope lock
    MutexAutoLock lock(mCompositorObserverLock);
    mCompositorVsyncObserver = aVsyncObserver;
  }

  bool observeVsync = aVsyncObserver != nullptr;
  nsCOMPtr<nsIRunnable> vsyncControl = NewRunnableMethod<bool>(this,
                                        &CompositorVsyncDispatcher::ObserveVsync,
                                        observeVsync);
  NS_DispatchToMainThread(vsyncControl);
}

void
CompositorVsyncDispatcher::Shutdown()
{
  // Need to explicitly remove CompositorVsyncDispatcher when the nsBaseWidget shuts down.
  // Otherwise, we would get dead vsync notifications between when the nsBaseWidget
  // shuts down and the CompositorBridgeParent shuts down.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  ObserveVsync(false);
  mDidShutdown = true;
  { // scope lock
    MutexAutoLock lock(mCompositorObserverLock);
    mCompositorVsyncObserver = nullptr;
  }
}

RefreshTimerVsyncDispatcher::RefreshTimerVsyncDispatcher()
  : mRefreshTimersLock("RefreshTimers lock")
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

RefreshTimerVsyncDispatcher::~RefreshTimerVsyncDispatcher()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

void
RefreshTimerVsyncDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  MutexAutoLock lock(mRefreshTimersLock);

  for (size_t i = 0; i < mChildRefreshTimers.Length(); i++) {
    mChildRefreshTimers[i]->NotifyVsync(aVsyncTimestamp);
  }

  if (mParentRefreshTimer) {
    mParentRefreshTimer->NotifyVsync(aVsyncTimestamp);
  }
}

void
RefreshTimerVsyncDispatcher::SetParentRefreshTimer(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(NS_IsMainThread());
  { // lock scope because UpdateVsyncStatus runs on main thread and will deadlock
    MutexAutoLock lock(mRefreshTimersLock);
    mParentRefreshTimer = aVsyncObserver;
  }

  UpdateVsyncStatus();
}

void
RefreshTimerVsyncDispatcher::AddChildRefreshTimer(VsyncObserver* aVsyncObserver)
{
  { // scope lock - called on pbackground thread
    MutexAutoLock lock(mRefreshTimersLock);
    MOZ_ASSERT(aVsyncObserver);
    if (!mChildRefreshTimers.Contains(aVsyncObserver)) {
      mChildRefreshTimers.AppendElement(aVsyncObserver);
    }
  }

  UpdateVsyncStatus();
}

void
RefreshTimerVsyncDispatcher::RemoveChildRefreshTimer(VsyncObserver* aVsyncObserver)
{
  { // scope lock - called on pbackground thread
    MutexAutoLock lock(mRefreshTimersLock);
    MOZ_ASSERT(aVsyncObserver);
    mChildRefreshTimers.RemoveElement(aVsyncObserver);
  }

  UpdateVsyncStatus();
}

void
RefreshTimerVsyncDispatcher::UpdateVsyncStatus()
{
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NewRunnableMethod(this,
                                              &RefreshTimerVsyncDispatcher::UpdateVsyncStatus));
    return;
  }

  gfx::VsyncSource::Display& display = gfxPlatform::GetPlatform()->GetHardwareVsync()->GetGlobalDisplay();
  display.NotifyRefreshTimerVsyncStatus(NeedsVsync());
}

bool
RefreshTimerVsyncDispatcher::NeedsVsync()
{
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mRefreshTimersLock);
  return (mParentRefreshTimer != nullptr) || !mChildRefreshTimers.IsEmpty();
}

} // namespace mozilla
