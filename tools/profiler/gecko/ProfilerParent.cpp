/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerParent.h"

#include "nsProfiler.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/Maybe.h"
#include "mozilla/ProfileBufferControlledChunkManager.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace mozilla {

using namespace ipc;

class ProfilerParentTracker;

// This class is responsible for gathering updates from chunk managers in
// different process, and request for the oldest chunks to be destroyed whenever
// the given memory limit is reached.
class ProfileBufferGlobalController final {
 public:
  ProfileBufferGlobalController(ProfilerParentTracker& aTracker,
                                size_t aMaximumBytes);

  ~ProfileBufferGlobalController();

  void Clear();

  void HandleChunkManagerUpdate(
      base::ProcessId aProcessId,
      ProfileBufferControlledChunkManager::Update&& aUpdate);

 private:
  ProfilerParentTracker& mTracker;
  const size_t mMaximumBytes;

  const base::ProcessId mParentProcessId = base::GetCurrentProcId();

  // Set to null when we receive the final empty update.
  ProfileBufferControlledChunkManager* mParentChunkManager =
      profiler_get_controlled_chunk_manager();
};

// This singleton class tracks live ProfilerParent's (meaning there's a current
// connection with a child process).
// It also knows when the local profiler is running.
// And when both the profiler is running and at least one child is present, it
// creates a ProfileBufferGlobalController and forwards chunk updates to it.
class ProfilerParentTracker final {
 public:
  static void StartTracking(ProfilerParent* aParent);
  static void StopTracking(ProfilerParent* aParent);

  static void ProfilerStarted(uint32_t aEntries);
  static void ProfilerWillStopIfStarted();

  template <typename FuncType>
  static void Enumerate(FuncType&& aIterFunc);

  template <typename FuncType>
  static void ForChild(base::ProcessId aChildPid, FuncType&& aIterFunc);

  static void ForwardChunkManagerUpdate(
      base::ProcessId aProcessId,
      ProfileBufferControlledChunkManager::Update&& aUpdate);

  ProfilerParentTracker();
  ~ProfilerParentTracker();

 private:
  static void EnsureInstance();

  // List of parents for currently-connected child processes.
  nsTArray<ProfilerParent*> mProfilerParents;

  // If non-0, the parent profiler is running, with this limit (in number of
  // entries.) This is needed here, because the parent profiler may start
  // running before child processes are known (e.g., startup profiling).
  uint32_t mEntries = 0;

  // When the profiler is running and there is at least one parent-child
  // connection, this is the controller that should receive chunk updates.
  Maybe<ProfileBufferGlobalController> mMaybeController;

  // Singleton instance, created when first needed, destroyed at Firefox
  // shutdown.
  static UniquePtr<ProfilerParentTracker> sInstance;
};

ProfileBufferGlobalController::ProfileBufferGlobalController(
    ProfilerParentTracker& aTracker, size_t aMaximumBytes)
    : mTracker(aTracker), mMaximumBytes(aMaximumBytes) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  // This is the local chunk manager for this parent process, so updates can be
  // handled here.
  if (NS_WARN_IF(!mParentChunkManager)) {
    return;
  }
  mParentChunkManager->SetUpdateCallback(
      [parentProcessId = mParentProcessId](
          ProfileBufferControlledChunkManager::Update&& aUpdate) {
        MOZ_ASSERT(!aUpdate.IsNotUpdate(),
                   "Update callback should never be given a non-update");
        // Always dispatch the update, to reduce the time spent in the callback,
        // and to avoid potential re-entrancy issues.
        // Handled by the ProfilerParentTracker singleton, because the
        // Controller could be destroyed in the meantime.
        Unused << NS_DispatchToMainThread(NS_NewRunnableFunction(
            "ChunkManagerUpdate parent callback",
            [parentProcessId, update = std::move(aUpdate)]() mutable {
              ProfilerParentTracker::ForwardChunkManagerUpdate(
                  parentProcessId, std::move(update));
            }));
      });
}

ProfileBufferGlobalController ::~ProfileBufferGlobalController() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mParentChunkManager) {
    // We haven't handled a final update, so the chunk manager is still valid.
    // Reset the callback in the chunk manager, this will immediately invoke the
    // callback with the final empty update.
    mParentChunkManager->SetUpdateCallback({});
  }
}

void ProfileBufferGlobalController::HandleChunkManagerUpdate(
    base::ProcessId aProcessId,
    ProfileBufferControlledChunkManager::Update&& aUpdate) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mParentChunkManager) {
    return;
  }

  MOZ_ASSERT(!aUpdate.IsNotUpdate(),
             "HandleChunkManagerUpdate should not be given a non-update");

  // TODO, see following patches.
}

UniquePtr<ProfilerParentTracker> ProfilerParentTracker::sInstance;

/* static */
void ProfilerParentTracker::EnsureInstance() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (sInstance) {
    return;
  }

  sInstance = MakeUnique<ProfilerParentTracker>();
  ClearOnShutdown(&sInstance);
}

/* static */
void ProfilerParentTracker::StartTracking(ProfilerParent* aProfilerParent) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  EnsureInstance();

  if (sInstance->mMaybeController.isNothing() && sInstance->mEntries != 0) {
    // There is no controller yet, but the profiler has started.
    // Since we're adding a ProfilerParent, it's a good time to start
    // controlling the global memory usage of the profiler.
    // (And this helps delay the Controller startup, because the parent profiler
    // can start *very* early in the process, when some resources like threads
    // are not ready yet.)
    sInstance->mMaybeController.emplace(*sInstance,
                                        size_t(sInstance->mEntries) * 8u);
  }

  sInstance->mProfilerParents.AppendElement(aProfilerParent);
}

/* static */
void ProfilerParentTracker::StopTracking(ProfilerParent* aParent) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return;
  }

  sInstance->mProfilerParents.RemoveElement(aParent);
}

/* static */
void ProfilerParentTracker::ProfilerStarted(uint32_t aEntries) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  EnsureInstance();

  sInstance->mEntries = aEntries;

  if (sInstance->mMaybeController.isNothing() &&
      !sInstance->mProfilerParents.IsEmpty()) {
    // We are already tracking child processes, so it's a good time to start
    // controlling the global memory usage of the profiler.
    sInstance->mMaybeController.emplace(*sInstance,
                                        size_t(sInstance->mEntries) * 8u);
  }
}

/* static */
void ProfilerParentTracker::ProfilerWillStopIfStarted() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return;
  }

  sInstance->mEntries = 0;
  sInstance->mMaybeController = Nothing{};
}

template <typename FuncType>
/* static */
void ProfilerParentTracker::Enumerate(FuncType&& aIterFunc) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return;
  }

  for (ProfilerParent* profilerParent : sInstance->mProfilerParents) {
    if (!profilerParent->mDestroyed) {
      aIterFunc(profilerParent);
    }
  }
}

template <typename FuncType>
/* static */
void ProfilerParentTracker::ForChild(base::ProcessId aChildPid,
                                     FuncType&& aIterFunc) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return;
  }

  for (ProfilerParent* profilerParent : sInstance->mProfilerParents) {
    if (profilerParent->mChildPid == aChildPid) {
      if (!profilerParent->mDestroyed) {
        std::forward<FuncType>(aIterFunc)(profilerParent);
      }
      return;
    }
  }
}

/* static */
void ProfilerParentTracker::ForwardChunkManagerUpdate(
    base::ProcessId aProcessId,
    ProfileBufferControlledChunkManager::Update&& aUpdate) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance || sInstance->mMaybeController.isNothing()) {
    return;
  }

  MOZ_ASSERT(!aUpdate.IsNotUpdate(),
             "No process should ever send a non-update");
  sInstance->mMaybeController->HandleChunkManagerUpdate(aProcessId,
                                                        std::move(aUpdate));
}

ProfilerParentTracker::ProfilerParentTracker() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(ProfilerParentTracker);
}

ProfilerParentTracker::~ProfilerParentTracker() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_COUNT_DTOR(ProfilerParentTracker);

  // Close the channels of any profiler parents that haven't been destroyed.
  for (ProfilerParent* profilerParent : mProfilerParents.Clone()) {
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

  RefPtr<ProfilerParent> actor = new ProfilerParent(aOtherPid);
  if (!parent.Bind(actor)) {
    MOZ_CRASH("Failed to bind parent actor for PProfiler!");
  }

  // mSelfRef will be cleared in DeallocPProfilerParent.
  actor->mSelfRef = actor;
  actor->Init();

  return child;
}

ProfilerParent::ProfilerParent(base::ProcessId aChildPid)
    : mChildPid(aChildPid), mDestroyed(false) {
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
  uint64_t activeBrowsingContextID;
  profiler_get_start_params(&entries, &duration, &interval, &features, &filters,
                            &activeBrowsingContextID);

  if (entries != 0) {
    ProfilerInitParams ipcParams;
    ipcParams.enabled() = true;
    ipcParams.entries() = entries;
    ipcParams.duration() = duration;
    ipcParams.interval() = interval;
    ipcParams.features() = features;
    ipcParams.activeBrowsingContextID() = activeBrowsingContextID;

    for (uint32_t i = 0; i < filters.length(); ++i) {
      ipcParams.filters().AppendElement(filters[i]);
    }

    Unused << SendEnsureStarted(ipcParams);
    RequestChunkManagerUpdate();
  } else {
    Unused << SendStop();
  }
}

ProfilerParent::~ProfilerParent() {
  MOZ_COUNT_DTOR(ProfilerParent);

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  ProfilerParentTracker::StopTracking(this);
}

/* static */
nsTArray<RefPtr<ProfilerParent::SingleProcessProfilePromise>>
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

// Magic value for ProfileBufferChunkManagerUpdate::unreleasedBytes meaning
// that this is a final update from a child.
constexpr static uint64_t scUpdateUnreleasedBytesFINAL = uint64_t(-1);

/* static */
ProfileBufferChunkManagerUpdate ProfilerParent::MakeFinalUpdate() {
  return ProfileBufferChunkManagerUpdate{
      uint64_t(scUpdateUnreleasedBytesFINAL), 0, TimeStamp{},
      nsTArray<ProfileBufferChunkMetadata>{}};
}

void ProfilerParent::RequestChunkManagerUpdate() {
  if (mDestroyed) {
    return;
  }

  RefPtr<AwaitNextChunkManagerUpdatePromise> updatePromise =
      SendAwaitNextChunkManagerUpdate();
  updatePromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr<ProfilerParent>(this)](
          const ProfileBufferChunkManagerUpdate& aUpdate) {
        if (aUpdate.unreleasedBytes() == scUpdateUnreleasedBytesFINAL) {
          // Special value meaning it's the final update from that child.
          ProfilerParentTracker::ForwardChunkManagerUpdate(
              self->mChildPid,
              ProfileBufferControlledChunkManager::Update(nullptr));
        } else {
          // Not the final update, translate it.
          std::vector<ProfileBufferControlledChunkManager::ChunkMetadata>
              chunks;
          if (!aUpdate.newlyReleasedChunks().IsEmpty()) {
            chunks.reserve(aUpdate.newlyReleasedChunks().Length());
            for (const ProfileBufferChunkMetadata& chunk :
                 aUpdate.newlyReleasedChunks()) {
              chunks.emplace_back(chunk.doneTimeStamp(), chunk.bufferBytes());
            }
          }
          // Let the tracker handle it.
          ProfilerParentTracker::ForwardChunkManagerUpdate(
              self->mChildPid,
              ProfileBufferControlledChunkManager::Update(
                  aUpdate.unreleasedBytes(), aUpdate.releasedBytes(),
                  aUpdate.oldestDoneTimeStamp(), std::move(chunks)));
          // This was not a final update, so start a new request.
          self->RequestChunkManagerUpdate();
        }
      },
      [self = RefPtr<ProfilerParent>(this)](
          mozilla::ipc::ResponseRejectReason aReason) {
        // Rejection could be for a number of reasons, assume the child will
        // not respond anymore, so we pretend we received a final update.
        ProfilerParentTracker::ForwardChunkManagerUpdate(
            self->mChildPid,
            ProfileBufferControlledChunkManager::Update(nullptr));
      });
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
  ipcParams.filters() = aParams->GetFilters().Clone();
  aParams->GetActiveBrowsingContextID(&ipcParams.activeBrowsingContextID());

  ProfilerParentTracker::ProfilerStarted(ipcParams.entries());
  ProfilerParentTracker::Enumerate([&](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendStart(ipcParams);
    profilerParent->RequestChunkManagerUpdate();
  });
}

/* static */
void ProfilerParent::ProfilerWillStopIfStarted() {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerParentTracker::ProfilerWillStopIfStarted();
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

void ProfilerParent::ActorDealloc() { mSelfRef = nullptr; }

}  // namespace mozilla
