/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerChild.h"

#include "GeckoProfiler.h"
#include "platform.h"
#include "ProfilerParent.h"

#include "nsThreadUtils.h"

namespace mozilla {

/* static */ DataMutexBase<ProfilerChild::ProfilerChildAndUpdate,
                           baseprofiler::detail::BaseProfilerMutex>
    ProfilerChild::sPendingChunkManagerUpdate{
        "ProfilerChild::sPendingChunkManagerUpdate"};

ProfilerChild::ProfilerChild()
    : mThread(NS_GetCurrentThread()), mDestroyed(false) {
  MOZ_COUNT_CTOR(ProfilerChild);
}

ProfilerChild::~ProfilerChild() { MOZ_COUNT_DTOR(ProfilerChild); }

void ProfilerChild::ResolveChunkUpdate(
    PProfilerChild::AwaitNextChunkManagerUpdateResolver& aResolve) {
  MOZ_ASSERT(!!aResolve,
             "ResolveChunkUpdate should only be called when there's a pending "
             "resolver");
  MOZ_ASSERT(
      !mChunkManagerUpdate.IsNotUpdate(),
      "ResolveChunkUpdate should only be called with a real or final update");
  MOZ_ASSERT(
      !mDestroyed,
      "ResolveChunkUpdate should not be called if the actor was destroyed");
  if (mChunkManagerUpdate.IsFinal()) {
    // Final update, send a special "unreleased value", but don't clear the
    // local copy so we know we got the final update.
    std::move(aResolve)(ProfilerParent::MakeFinalUpdate());
  } else {
    // Optimization note: The ProfileBufferChunkManagerUpdate constructor takes
    // the newly-released chunks nsTArray by reference-to-const, therefore
    // constructing and then moving the array here would make a copy. So instead
    // we first give it an empty array, and then we can write the data directly
    // into the update's array.
    ProfileBufferChunkManagerUpdate update{
        mChunkManagerUpdate.UnreleasedBytes(),
        mChunkManagerUpdate.ReleasedBytes(),
        mChunkManagerUpdate.OldestDoneTimeStamp(),
        {}};
    update.newlyReleasedChunks().SetCapacity(
        mChunkManagerUpdate.NewlyReleasedChunksRef().size());
    for (const ProfileBufferControlledChunkManager::ChunkMetadata& chunk :
         mChunkManagerUpdate.NewlyReleasedChunksRef()) {
      update.newlyReleasedChunks().EmplaceBack(chunk.mDoneTimeStamp,
                                               chunk.mBufferBytes);
    }

    std::move(aResolve)(update);

    // Clear the update we just sent, so it's ready for later updates to be
    // folded into it.
    mChunkManagerUpdate.Clear();
  }

  // Discard the resolver, so it's empty next time there's a new request.
  aResolve = nullptr;
}

void ProfilerChild::ProcessChunkManagerUpdate(
    ProfileBufferControlledChunkManager::Update&& aUpdate) {
  if (mDestroyed) {
    return;
  }
  // Always store the data, it could be the final update.
  mChunkManagerUpdate.Fold(std::move(aUpdate));
  if (mAwaitNextChunkManagerUpdateResolver) {
    // There is already a pending resolver, give it the info now.
    ResolveChunkUpdate(mAwaitNextChunkManagerUpdateResolver);
  }
}

/* static */ void ProfilerChild::ProcessPendingUpdate() {
  auto lockedUpdate = sPendingChunkManagerUpdate.Lock();
  if (!lockedUpdate->mProfilerChild || lockedUpdate->mUpdate.IsNotUpdate()) {
    return;
  }
  lockedUpdate->mProfilerChild->mThread->Dispatch(NS_NewRunnableFunction(
      "ProfilerChild::ProcessPendingUpdate", []() mutable {
        auto lockedUpdate = sPendingChunkManagerUpdate.Lock();
        if (!lockedUpdate->mProfilerChild ||
            lockedUpdate->mUpdate.IsNotUpdate()) {
          return;
        }
        lockedUpdate->mProfilerChild->ProcessChunkManagerUpdate(
            std::move(lockedUpdate->mUpdate));
        lockedUpdate->mUpdate.Clear();
      }));
}

/* static */ bool ProfilerChild::IsLockedOnCurrentThread() {
  return sPendingChunkManagerUpdate.Mutex().IsLockedOnCurrentThread();
}

void ProfilerChild::SetupChunkManager() {
  mChunkManager = profiler_get_controlled_chunk_manager();
  if (NS_WARN_IF(!mChunkManager)) {
    return;
  }

  // Make sure there are no updates (from a previous run).
  mChunkManagerUpdate.Clear();
  {
    auto lockedUpdate = sPendingChunkManagerUpdate.Lock();
    lockedUpdate->mProfilerChild = this;
    lockedUpdate->mUpdate.Clear();
  }

  mChunkManager->SetUpdateCallback(
      [](ProfileBufferControlledChunkManager::Update&& aUpdate) {
        // Updates from the chunk manager are stored for later processing.
        // We avoid dispatching a task, as this could deadlock (if the queueing
        // mutex is held elsewhere).
        auto lockedUpdate = sPendingChunkManagerUpdate.Lock();
        if (!lockedUpdate->mProfilerChild) {
          return;
        }
        lockedUpdate->mUpdate.Fold(std::move(aUpdate));
      });
}

void ProfilerChild::ResetChunkManager() {
  if (!mChunkManager) {
    return;
  }

  // We have a chunk manager, reset the callback, which will add a final
  // pending update.
  mChunkManager->SetUpdateCallback({});

  // Clear the pending update.
  auto lockedUpdate = sPendingChunkManagerUpdate.Lock();
  lockedUpdate->mProfilerChild = nullptr;
  lockedUpdate->mUpdate.Clear();
  // And process a final update right now.
  ProcessChunkManagerUpdate(
      ProfileBufferControlledChunkManager::Update(nullptr));

  mChunkManager = nullptr;
  mAwaitNextChunkManagerUpdateResolver = nullptr;
}

mozilla::ipc::IPCResult ProfilerChild::RecvStart(
    const ProfilerInitParams& params) {
  nsTArray<const char*> filterArray;
  for (size_t i = 0; i < params.filters().Length(); ++i) {
    filterArray.AppendElement(params.filters()[i].get());
  }

  profiler_start(PowerOfTwo32(params.entries()), params.interval(),
                 params.features(), filterArray.Elements(),
                 filterArray.Length(), params.activeTabID(), params.duration());

  SetupChunkManager();

  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvEnsureStarted(
    const ProfilerInitParams& params) {
  nsTArray<const char*> filterArray;
  for (size_t i = 0; i < params.filters().Length(); ++i) {
    filterArray.AppendElement(params.filters()[i].get());
  }

  profiler_ensure_started(PowerOfTwo32(params.entries()), params.interval(),
                          params.features(), filterArray.Elements(),
                          filterArray.Length(), params.activeTabID(),
                          params.duration());

  SetupChunkManager();

  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvStop() {
  ResetChunkManager();
  profiler_stop();
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvPause() {
  profiler_pause();
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvResume() {
  profiler_resume();
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvPauseSampling() {
  profiler_pause_sampling();
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvResumeSampling() {
  profiler_resume_sampling();
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvClearAllPages() {
  profiler_clear_all_pages();
  return IPC_OK();
}

static nsCString CollectProfileOrEmptyString(bool aIsShuttingDown) {
  nsCString profileCString;
  UniquePtr<char[]> profile =
      profiler_get_profile(/* aSinceTime */ 0, aIsShuttingDown);
  if (profile) {
    size_t len = strlen(profile.get());
    profileCString.Adopt(profile.release(), len);
  }
  return profileCString;
}

mozilla::ipc::IPCResult ProfilerChild::RecvAwaitNextChunkManagerUpdate(
    AwaitNextChunkManagerUpdateResolver&& aResolve) {
  MOZ_ASSERT(!mDestroyed,
             "Recv... should not be called if the actor was destroyed");
  // Pick up pending updates if any.
  {
    auto lockedUpdate = sPendingChunkManagerUpdate.Lock();
    if (lockedUpdate->mProfilerChild && !lockedUpdate->mUpdate.IsNotUpdate()) {
      mChunkManagerUpdate.Fold(std::move(lockedUpdate->mUpdate));
      lockedUpdate->mUpdate.Clear();
    }
  }
  if (mChunkManagerUpdate.IsNotUpdate()) {
    // No data yet, store the resolver for later.
    mAwaitNextChunkManagerUpdateResolver = std::move(aResolve);
  } else {
    // We have data, send it now.
    ResolveChunkUpdate(aResolve);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvDestroyReleasedChunksAtOrBefore(
    const TimeStamp& aTimeStamp) {
  if (mChunkManager) {
    mChunkManager->DestroyChunksAtOrBefore(aTimeStamp);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvGatherProfile(
    GatherProfileResolver&& aResolve) {
  mozilla::ipc::Shmem shmem;
  profiler_get_profile_json_into_lazily_allocated_buffer(
      [&](size_t allocationSize) -> char* {
        if (AllocShmem(allocationSize,
                       mozilla::ipc::Shmem::SharedMemory::TYPE_BASIC, &shmem)) {
          return shmem.get<char>();
        }
        return nullptr;
      },
      /* aSinceTime */ 0,
      /* aIsShuttingDown */ false);
  aResolve(std::move(shmem));
  return IPC_OK();
}

void ProfilerChild::ActorDestroy(ActorDestroyReason aActorDestroyReason) {
  mDestroyed = true;
}

void ProfilerChild::Destroy() {
  ResetChunkManager();
  if (!mDestroyed) {
    Close();
  }
}

nsCString ProfilerChild::GrabShutdownProfile() {
  return CollectProfileOrEmptyString(/* aIsShuttingDown */ true);
}

}  // namespace mozilla
