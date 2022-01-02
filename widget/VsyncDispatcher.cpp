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
    : mVsyncSource(gfxPlatform::GetPlatform()->GetHardwareVsync()),
      mCompositorObserverLock("CompositorObserverLock"),
      mDidShutdown(false) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  mVsyncSource->RegisterCompositorVsyncDispatcher(this);
}

CompositorVsyncDispatcher::CompositorVsyncDispatcher(
    RefPtr<gfx::VsyncSource> aVsyncSource)
    : mVsyncSource(std::move(aVsyncSource)),
      mCompositorObserverLock("CompositorObserverLock"),
      mDidShutdown(false) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  mVsyncSource->RegisterCompositorVsyncDispatcher(this);
}

CompositorVsyncDispatcher::~CompositorVsyncDispatcher() {
  MOZ_ASSERT(XRE_IsParentProcess());
  // We auto remove this vsync dispatcher from the vsync source in the
  // nsBaseWidget
}

void CompositorVsyncDispatcher::NotifyVsync(const VsyncEvent& aVsync) {
  // In vsync thread
  layers::CompositorBridgeParent::PostInsertVsyncProfilerMarker(aVsync.mTime);

  MutexAutoLock lock(mCompositorObserverLock);
  if (mCompositorVsyncObserver) {
    mCompositorVsyncObserver->NotifyVsync(aVsync);
  }
}

void CompositorVsyncDispatcher::MoveToSource(
    const RefPtr<gfx::VsyncSource>& aVsyncSource) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  mVsyncSource = aVsyncSource;
}

void CompositorVsyncDispatcher::ObserveVsync(bool aEnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  if (mDidShutdown) {
    return;
  }

  if (aEnable) {
    mVsyncSource->EnableCompositorVsyncDispatcher(this);
  } else {
    mVsyncSource->DisableCompositorVsyncDispatcher(this);
  }
}

void CompositorVsyncDispatcher::SetCompositorVsyncObserver(
    VsyncObserver* aVsyncObserver) {
  // When remote compositing or running gtests, vsync observation is
  // initiated on the main thread. Otherwise, it is initiated from the
  // compositor thread.
  MOZ_ASSERT(NS_IsMainThread() ||
             CompositorThreadHolder::IsInCompositorThread());

  {  // scope lock
    MutexAutoLock lock(mCompositorObserverLock);
    mCompositorVsyncObserver = aVsyncObserver;
  }

  bool observeVsync = aVsyncObserver != nullptr;
  nsCOMPtr<nsIRunnable> vsyncControl = NewRunnableMethod<bool>(
      "CompositorVsyncDispatcher::ObserveVsync", this,
      &CompositorVsyncDispatcher::ObserveVsync, observeVsync);
  NS_DispatchToMainThread(vsyncControl);
}

void CompositorVsyncDispatcher::Shutdown() {
  // Need to explicitly remove CompositorVsyncDispatcher when the nsBaseWidget
  // shuts down. Otherwise, we would get dead vsync notifications between when
  // the nsBaseWidget shuts down and the CompositorBridgeParent shuts down.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mDidShutdown);
  ObserveVsync(false);
  mDidShutdown = true;
  {  // scope lock
    MutexAutoLock lock(mCompositorObserverLock);
    mCompositorVsyncObserver = nullptr;
  }
  mVsyncSource->DeregisterCompositorVsyncDispatcher(this);
  mVsyncSource = nullptr;
}

RefreshTimerVsyncDispatcher::RefreshTimerVsyncDispatcher(
    gfx::VsyncSource::Display* aDisplay)
    : mDisplay(aDisplay), mRefreshTimersLock("RefreshTimers lock") {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

RefreshTimerVsyncDispatcher::~RefreshTimerVsyncDispatcher() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

void RefreshTimerVsyncDispatcher::MoveToDisplay(
    gfx::VsyncSource::Display* aDisplay) {
  MOZ_ASSERT(NS_IsMainThread());
  mDisplay = aDisplay;
}

void RefreshTimerVsyncDispatcher::NotifyVsync(const VsyncEvent& aVsync) {
  MutexAutoLock lock(mRefreshTimersLock);

  for (size_t i = 0; i < mChildRefreshTimers.Length(); i++) {
    mChildRefreshTimers[i]->NotifyVsync(aVsync);
  }

  if (mParentRefreshTimer) {
    mParentRefreshTimer->NotifyVsync(aVsync);
  }
}

void RefreshTimerVsyncDispatcher::SetParentRefreshTimer(
    VsyncObserver* aVsyncObserver) {
  MOZ_ASSERT(NS_IsMainThread());
  {  // lock scope because UpdateVsyncStatus runs on main thread and will
     // deadlock
    MutexAutoLock lock(mRefreshTimersLock);
    mParentRefreshTimer = aVsyncObserver;
  }

  UpdateVsyncStatus();
}

void RefreshTimerVsyncDispatcher::AddChildRefreshTimer(
    VsyncObserver* aVsyncObserver) {
  {  // scope lock - called on pbackground thread
    MutexAutoLock lock(mRefreshTimersLock);
    MOZ_ASSERT(aVsyncObserver);
    if (!mChildRefreshTimers.Contains(aVsyncObserver)) {
      mChildRefreshTimers.AppendElement(aVsyncObserver);
    }
  }

  UpdateVsyncStatus();
}

void RefreshTimerVsyncDispatcher::RemoveChildRefreshTimer(
    VsyncObserver* aVsyncObserver) {
  {  // scope lock - called on pbackground thread
    MutexAutoLock lock(mRefreshTimersLock);
    MOZ_ASSERT(aVsyncObserver);
    mChildRefreshTimers.RemoveElement(aVsyncObserver);
  }

  UpdateVsyncStatus();
}

void RefreshTimerVsyncDispatcher::UpdateVsyncStatus() {
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NewRunnableMethod(
        "RefreshTimerVsyncDispatcher::UpdateVsyncStatus", this,
        &RefreshTimerVsyncDispatcher::UpdateVsyncStatus));
    return;
  }

  mDisplay->NotifyRefreshTimerVsyncStatus(NeedsVsync());
}

bool RefreshTimerVsyncDispatcher::NeedsVsync() {
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mRefreshTimersLock);
  return (mParentRefreshTimer != nullptr) || !mChildRefreshTimers.IsEmpty();
}

}  // namespace mozilla
