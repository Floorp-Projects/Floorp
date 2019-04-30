/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerParent.h"

#include "nsProfiler.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace mozilla {

using namespace ipc;

class ProfilerParentTracker final {
 public:
  static void StartTracking(ProfilerParent* aParent);
  static void StopTracking(ProfilerParent* aParent);

  template <typename FuncType>
  static void Enumerate(FuncType aIterFunc);

  ProfilerParentTracker();
  ~ProfilerParentTracker();

 private:
  nsTArray<ProfilerParent*> mProfilerParents;
  static UniquePtr<ProfilerParentTracker> sInstance;
};

UniquePtr<ProfilerParentTracker> ProfilerParentTracker::sInstance;

/* static */
void ProfilerParentTracker::StartTracking(ProfilerParent* aProfilerParent) {
  if (!sInstance) {
    sInstance = MakeUnique<ProfilerParentTracker>();
    ClearOnShutdown(&sInstance);
  }
  sInstance->mProfilerParents.AppendElement(aProfilerParent);
}

/* static */
void ProfilerParentTracker::StopTracking(ProfilerParent* aParent) {
  if (sInstance) {
    sInstance->mProfilerParents.RemoveElement(aParent);
  }
}

template <typename FuncType>
/* static */
void ProfilerParentTracker::Enumerate(FuncType aIterFunc) {
  if (sInstance) {
    for (ProfilerParent* profilerParent : sInstance->mProfilerParents) {
      if (!profilerParent->mDestroyed) {
        aIterFunc(profilerParent);
      }
    }
  }
}

ProfilerParentTracker::ProfilerParentTracker() {
  MOZ_COUNT_CTOR(ProfilerParentTracker);
}

ProfilerParentTracker::~ProfilerParentTracker() {
  MOZ_COUNT_DTOR(ProfilerParentTracker);

  nsTArray<ProfilerParent*> parents;
  parents = mProfilerParents;
  // Close the channels of any profiler parents that haven't been destroyed.
  for (ProfilerParent* profilerParent : parents) {
    if (!profilerParent->mDestroyed) {
      // Keep the object alive until the call to Close() has completed.
      // Close() will trigger a call to DeallocPProfilerParent.
      RefPtr<ProfilerParent> actor = profilerParent;
      actor->Close();
    }
  }
}

/* static */
Endpoint<PProfilerChild> ProfilerParent::CreateForProcess(
    base::ProcessId aOtherPid) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  Endpoint<PProfilerParent> parent;
  Endpoint<PProfilerChild> child;
  nsresult rv = PProfiler::CreateEndpoints(base::GetCurrentProcId(), aOtherPid,
                                           &parent, &child);

  if (NS_FAILED(rv)) {
    MOZ_CRASH("Failed to create top level actor for PProfiler!");
  }

  RefPtr<ProfilerParent> actor = new ProfilerParent();
  if (!parent.Bind(actor)) {
    MOZ_CRASH("Failed to bind parent actor for PProfiler!");
  }

  // mSelfRef will be cleared in DeallocPProfilerParent.
  actor->mSelfRef = actor;
  actor->Init();

  return child;
}

ProfilerParent::ProfilerParent() : mDestroyed(false) {
  MOZ_COUNT_CTOR(ProfilerParent);

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
}

void ProfilerParent::Init() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ProfilerParentTracker::StartTracking(this);

  // We propagated the profiler state from the parent process to the child
  // process through MOZ_PROFILER_STARTUP* environment variables.
  // However, the profiler state might have changed in this process since then,
  // and now that an active communication channel has been established with the
  // child process, it's a good time to sync up the two profilers again.

  int entries = 0;
  Maybe<double> duration = Nothing();
  double interval = 0;
  mozilla::Vector<const char*> filters;
  uint32_t features;
  profiler_get_start_params(&entries, &duration, &interval, &features,
                            &filters);

  if (entries != 0) {
    ProfilerInitParams ipcParams;
    ipcParams.enabled() = true;
    ipcParams.entries() = entries;
    ipcParams.duration() = duration;
    ipcParams.interval() = interval;
    ipcParams.features() = features;

    for (uint32_t i = 0; i < filters.length(); ++i) {
      ipcParams.filters().AppendElement(filters[i]);
    }

    Unused << SendEnsureStarted(ipcParams);
  } else {
    Unused << SendStop();
  }
}

ProfilerParent::~ProfilerParent() {
  MOZ_COUNT_DTOR(ProfilerParent);

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  ProfilerParentTracker::StopTracking(this);
}

/* static */ nsTArray<RefPtr<ProfilerParent::SingleProcessProfilePromise>>
ProfilerParent::GatherProfiles() {
  if (!NS_IsMainThread()) {
    return nsTArray<RefPtr<ProfilerParent::SingleProcessProfilePromise>>();
  }

  nsTArray<RefPtr<SingleProcessProfilePromise>> results;
  ProfilerParentTracker::Enumerate([&](ProfilerParent* profilerParent) {
    results.AppendElement(profilerParent->SendGatherProfile());
  });
  return results;
}

/* static */
void ProfilerParent::ProfilerStarted(nsIProfilerStartParams* aParams) {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerInitParams ipcParams;
  double duration;
  ipcParams.enabled() = true;
  aParams->GetEntries(&ipcParams.entries());
  aParams->GetDuration(&duration);
  if (duration > 0.0) {
    ipcParams.duration() = Some(duration);
  } else {
    ipcParams.duration() = Nothing();
  }
  aParams->GetInterval(&ipcParams.interval());
  aParams->GetFeatures(&ipcParams.features());
  ipcParams.filters() = aParams->GetFilters();

  ProfilerParentTracker::Enumerate([&](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendStart(ipcParams);
  });
}

/* static */
void ProfilerParent::ProfilerStopped() {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerParentTracker::Enumerate([](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendStop();
  });
}

/* static */
void ProfilerParent::ProfilerPaused() {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerParentTracker::Enumerate([](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendPause();
  });
}

/* static */
void ProfilerParent::ProfilerResumed() {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerParentTracker::Enumerate([](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendResume();
  });
}

/* static */
void ProfilerParent::ClearAllPages() {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerParentTracker::Enumerate([](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendClearAllPages();
  });
}

void ProfilerParent::ActorDestroy(ActorDestroyReason aActorDestroyReason) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mDestroyed = true;
}

void ProfilerParent::DeallocPProfilerParent() { mSelfRef = nullptr; }

}  // namespace mozilla
