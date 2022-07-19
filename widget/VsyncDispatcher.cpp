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
#include "mozilla/StaticPrefs_gfx.h"

using namespace mozilla::layers;

namespace mozilla {

CompositorVsyncDispatcher::CompositorVsyncDispatcher(
    RefPtr<VsyncDispatcher> aVsyncDispatcher)
    : mVsyncDispatcher(std::move(aVsyncDispatcher)),
      mCompositorObserverLock("CompositorObserverLock"),
      mDidShutdown(false) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

CompositorVsyncDispatcher::~CompositorVsyncDispatcher() {
  MOZ_ASSERT(XRE_IsParentProcess());
}

void CompositorVsyncDispatcher::NotifyVsync(const VsyncEvent& aVsync) {
  // In vsync thread
  layers::CompositorBridgeParent::PostInsertVsyncProfilerMarker(aVsync.mTime);

  MutexAutoLock lock(mCompositorObserverLock);
  if (mCompositorVsyncObserver) {
    mCompositorVsyncObserver->NotifyVsync(aVsync);
  }
}

void CompositorVsyncDispatcher::ObserveVsync(bool aEnable) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  if (mDidShutdown) {
    return;
  }

  if (aEnable) {
    mVsyncDispatcher->AddVsyncObserver(this);
  } else {
    mVsyncDispatcher->RemoveVsyncObserver(this);
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
  mVsyncDispatcher = nullptr;
}

VsyncDispatcher::VsyncDispatcher(gfx::VsyncSource* aVsyncSource)
    : mState(State(aVsyncSource), "VsyncDispatcher::mState") {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncDispatcher::~VsyncDispatcher() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

void VsyncDispatcher::SetVsyncSource(gfx::VsyncSource* aVsyncSource) {
  MOZ_RELEASE_ASSERT(aVsyncSource);

  auto state = mState.Lock();
  if (aVsyncSource == state->mCurrentVsyncSource) {
    return;
  }

  if (state->mIsObservingVsync) {
    state->mCurrentVsyncSource->RemoveVsyncDispatcher(this);
    aVsyncSource->AddVsyncDispatcher(this);
  }
  state->mCurrentVsyncSource = aVsyncSource;
}

RefPtr<gfx::VsyncSource> VsyncDispatcher::GetCurrentVsyncSource() {
  auto state = mState.Lock();
  return state->mCurrentVsyncSource;
}

TimeDuration VsyncDispatcher::GetVsyncRate() {
  auto state = mState.Lock();
  return state->mCurrentVsyncSource->GetVsyncRate();
}

void VsyncDispatcher::NotifyVsync(const VsyncEvent& aVsync) {
  nsTArray<RefPtr<VsyncObserver>> observers;
  bool shouldDispatchToMainThread = false;
  {
    auto state = mState.Lock();

    // Respect the pref gfx.display.frame-rate-divisor.
    if (++state->mVsyncSkipCounter <
        StaticPrefs::gfx_display_frame_rate_divisor()) {
      return;
    }
    state->mVsyncSkipCounter = 0;

    // Copy out the observers so that we don't keep the mutex
    // locked while notifying vsync.
    observers = state->mObservers.Clone();
    shouldDispatchToMainThread = !state->mMainThreadObservers.IsEmpty() &&
                                 (state->mLastVsyncIdSentToMainThread ==
                                  state->mLastMainThreadProcessedVsyncId);
  }

  for (const auto& observer : observers) {
    observer->NotifyVsync(aVsync);
  }

  if (shouldDispatchToMainThread) {
    auto state = mState.Lock();
    state->mLastVsyncIdSentToMainThread = aVsync.mId;
    NS_DispatchToMainThread(NewRunnableMethod<VsyncEvent>(
        "VsyncDispatcher::NotifyMainThreadObservers", this,
        &VsyncDispatcher::NotifyMainThreadObservers, aVsync));
  }
}

void VsyncDispatcher::NotifyMainThreadObservers(VsyncEvent aEvent) {
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<RefPtr<VsyncObserver>> observers;
  {
    // Copy out the main thread observers so that we don't keep the mutex
    // locked while notifying vsync.
    auto state = mState.Lock();
    observers.AppendElements(state->mMainThreadObservers);
  }

  for (const auto& observer : observers) {
    observer->NotifyVsync(aEvent);
  }

  {  // Scope lock
    auto state = mState.Lock();
    state->mLastMainThreadProcessedVsyncId = aEvent.mId;
  }
}

void VsyncDispatcher::AddVsyncObserver(VsyncObserver* aVsyncObserver) {
  MOZ_ASSERT(aVsyncObserver);
  {  // scope lock - called on PBackground thread or main thread
    auto state = mState.Lock();
    if (!state->mObservers.Contains(aVsyncObserver)) {
      state->mObservers.AppendElement(aVsyncObserver);
    }
  }

  UpdateVsyncStatus();
}

void VsyncDispatcher::RemoveVsyncObserver(VsyncObserver* aVsyncObserver) {
  MOZ_ASSERT(aVsyncObserver);
  {  // scope lock - called on PBackground thread or main thread
    auto state = mState.Lock();
    state->mObservers.RemoveElement(aVsyncObserver);
  }

  UpdateVsyncStatus();
}

void VsyncDispatcher::AddMainThreadObserver(VsyncObserver* aObserver) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aObserver);
  {
    auto state = mState.Lock();
    state->mMainThreadObservers.AppendElement(aObserver);
  }

  UpdateVsyncStatus();
}

void VsyncDispatcher::RemoveMainThreadObserver(VsyncObserver* aObserver) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aObserver);
  {
    auto state = mState.Lock();
    state->mMainThreadObservers.RemoveElement(aObserver);
  }

  UpdateVsyncStatus();
}

void VsyncDispatcher::UpdateVsyncStatus() {
  bool wasObservingVsync = false;
  bool needVsync = false;
  RefPtr<VsyncSource> vsyncSource;

  {
    auto state = mState.Lock();
    wasObservingVsync = state->mIsObservingVsync;
    needVsync =
        !state->mObservers.IsEmpty() || !state->mMainThreadObservers.IsEmpty();
    state->mIsObservingVsync = needVsync;
    vsyncSource = state->mCurrentVsyncSource;
  }

  // Call Add/RemoveVsyncDispatcher outside the lock, because it can re-enter
  // into VsyncDispatcher::NotifyVsync.
  if (needVsync && !wasObservingVsync) {
    vsyncSource->AddVsyncDispatcher(this);
  } else if (!needVsync && wasObservingVsync) {
    vsyncSource->RemoveVsyncDispatcher(this);
  }
}

}  // namespace mozilla
