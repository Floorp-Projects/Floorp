/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadUtils.h"
#include "VsyncDispatcher.h"
#include "VsyncSource.h"
#include "gfxPlatform.h"
#include "mozilla/layers/CompositorParent.h"

#ifdef MOZ_ENABLE_PROFILER_SPS
#include "GeckoProfiler.h"
#include "ProfilerMarkers.h"
#endif

namespace mozilla {

CompositorVsyncDispatcher::CompositorVsyncDispatcher()
  : mCompositorObserverLock("CompositorObserverLock")
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  gfxPlatform::GetPlatform()->GetHardwareVsync()->AddCompositorVsyncDispatcher(this);
}

CompositorVsyncDispatcher::~CompositorVsyncDispatcher()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  // We auto remove this vsync dispatcher from the vsync source in the nsBaseWidget
  MOZ_ASSERT(NS_IsMainThread());
}

void
CompositorVsyncDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // In hardware vsync thread
#ifdef MOZ_ENABLE_PROFILER_SPS
    if (profiler_is_active()) {
        layers::CompositorParent::PostInsertVsyncProfilerMarker(aVsyncTimestamp);
    }
#endif

  MutexAutoLock lock(mCompositorObserverLock);
  if (mCompositorVsyncObserver) {
    mCompositorVsyncObserver->NotifyVsync(aVsyncTimestamp);
  }
}

void
CompositorVsyncDispatcher::SetCompositorVsyncObserver(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(layers::CompositorParent::IsInCompositorThread());
  MutexAutoLock lock(mCompositorObserverLock);
  mCompositorVsyncObserver = aVsyncObserver;
}

void
CompositorVsyncDispatcher::Shutdown()
{
  // Need to explicitly remove CompositorVsyncDispatcher when the nsBaseWidget shuts down.
  // Otherwise, we would get dead vsync notifications between when the nsBaseWidget
  // shuts down and the CompositorParent shuts down.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  gfxPlatform::GetPlatform()->GetHardwareVsync()->RemoveCompositorVsyncDispatcher(this);
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
  MutexAutoLock lock(mRefreshTimersLock);
  mParentRefreshTimer = aVsyncObserver;
}

void
RefreshTimerVsyncDispatcher::AddChildRefreshTimer(VsyncObserver* aVsyncObserver)
{
  MutexAutoLock lock(mRefreshTimersLock);
  MOZ_ASSERT(aVsyncObserver);
  if (!mChildRefreshTimers.Contains(aVsyncObserver)) {
    mChildRefreshTimers.AppendElement(aVsyncObserver);
  }
}

void
RefreshTimerVsyncDispatcher::RemoveChildRefreshTimer(VsyncObserver* aVsyncObserver)
{
  MutexAutoLock lock(mRefreshTimersLock);
  MOZ_ASSERT(aVsyncObserver);
  mChildRefreshTimers.RemoveElement(aVsyncObserver);
}

} // namespace mozilla
