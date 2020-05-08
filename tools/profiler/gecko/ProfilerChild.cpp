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

void ProfilerChild::ChunkManagerUpdateCallback(
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

void ProfilerChild::SetupChunkManager() {
  mChunkManager = profiler_get_controlled_chunk_manager();
  if (NS_WARN_IF(!mChunkManager)) {
    return;
  }

  // The update may be in any state from a previous profiling session.
  // In case there is already a task in-flight with an update from that previous
  // session, we need to dispatch a task to clear the update afterwards, but
  // before the first update which will be dispatched from SetUpdateCallback()
  // below.
  mThread->Dispatch(NS_NewRunnableFunction(
      "ChunkManagerUpdate Callback", [profilerChild = RefPtr(this)]() mutable {
        profilerChild->mChunkManagerUpdate.Clear();
      }));

  // `ProfilerChild` should only be used on its `mThread`.
  // But the chunk manager update callback may happen on any thread, so we need
  // to manually keep the `ProfilerChild` alive until after the final update has
  // been handled on `mThread`.
  // Using manual AddRef/Release, because ref-counting is single-threaded, so we
  // cannot have a RefPtr stored in the callback which will be destroyed in
  // another thread. The callback (where `Release()` happens) is guaranteed to
  // always be called, at the latest when the calback is reset during shutdown.
  AddRef();
  mChunkManager->SetUpdateCallback(
      // Cast to `void*` to evade refcounted security!
      [profilerChildPtr = static_cast<void*>(this)](
          ProfileBufferControlledChunkManager::Update&& aUpdate) {
        // Always dispatch, even if we're already on the `mThread`, to avoid
        // reentrancy issues.
        ProfilerChild* profilerChild =
            static_cast<ProfilerChild*>(profilerChildPtr);
        profilerChild->mThread->Dispatch(NS_NewRunnableFunction(
            "ChunkManagerUpdate Callback",
            [profilerChildPtr, update = std::move(aUpdate)]() mutable {
              ProfilerChild* profilerChild =
                  static_cast<ProfilerChild*>(profilerChildPtr);
              const bool isFinal = update.IsFinal();
              profilerChild->ChunkManagerUpdateCallback(std::move(update));
              if (isFinal) {
                profilerChild->Release();
              }
            }));
      });
}

void ProfilerChild::ResetChunkManager() {
  if (mChunkManager) {
    // We have a chunk manager, reset the callback, which will send a final
    // update.
    mChunkManager->SetUpdateCallback({});
  } else if (!mChunkManagerUpdate.IsFinal()) {
    // No chunk manager, just make sure the update as final now.
    mChunkManagerUpdate.Fold(
        ProfileBufferControlledChunkManager::Update(nullptr));
  }
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
                 filterArray.Length(), params.activeBrowsingContextID(),
                 params.duration());

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
                          filterArray.Length(),
                          params.activeBrowsingContextID(), params.duration());

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
  } else {
    profileCString = EmptyCString();
  }
  return profileCString;
}

mozilla::ipc::IPCResult ProfilerChild::RecvAwaitNextChunkManagerUpdate(
    AwaitNextChunkManagerUpdateResolver&& aResolve) {
  MOZ_ASSERT(!mDestroyed,
             "Recv... should not be called if the actor was destroyed");
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
