/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerChild.h"

#include "GeckoProfiler.h"
#include "platform.h"
#include "ProfilerControl.h"
#include "ProfilerParent.h"

#include "nsThreadUtils.h"

#include <memory>

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
    const ProfilerInitParams& params, StartResolver&& aResolve) {
  nsTArray<const char*> filterArray;
  for (size_t i = 0; i < params.filters().Length(); ++i) {
    filterArray.AppendElement(params.filters()[i].get());
  }

  profiler_start(PowerOfTwo32(params.entries()), params.interval(),
                 params.features(), filterArray.Elements(),
                 filterArray.Length(), params.activeTabID(), params.duration());

  SetupChunkManager();

  aResolve(/* unused */ true);
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvEnsureStarted(
    const ProfilerInitParams& params, EnsureStartedResolver&& aResolve) {
  nsTArray<const char*> filterArray;
  for (size_t i = 0; i < params.filters().Length(); ++i) {
    filterArray.AppendElement(params.filters()[i].get());
  }

  profiler_ensure_started(PowerOfTwo32(params.entries()), params.interval(),
                          params.features(), filterArray.Elements(),
                          filterArray.Length(), params.activeTabID(),
                          params.duration());

  SetupChunkManager();

  aResolve(/* unused */ true);
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvStop(StopResolver&& aResolve) {
  ResetChunkManager();
  profiler_stop();
  aResolve(/* unused */ true);
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvPause(PauseResolver&& aResolve) {
  profiler_pause();
  aResolve(/* unused */ true);
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvResume(ResumeResolver&& aResolve) {
  profiler_resume();
  aResolve(/* unused */ true);
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvPauseSampling(
    PauseSamplingResolver&& aResolve) {
  profiler_pause_sampling();
  aResolve(/* unused */ true);
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvResumeSampling(
    ResumeSamplingResolver&& aResolve) {
  profiler_resume_sampling();
  aResolve(/* unused */ true);
  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvWaitOnePeriodicSampling(
    WaitOnePeriodicSamplingResolver&& aResolve) {
  std::shared_ptr<WaitOnePeriodicSamplingResolver> resolve =
      std::make_shared<WaitOnePeriodicSamplingResolver>(std::move(aResolve));
  if (!profiler_callback_after_sampling(
          [self = RefPtr(this), resolve](SamplingState aSamplingState) mutable {
            if (self->mDestroyed) {
              return;
            }
            MOZ_RELEASE_ASSERT(self->mThread);
            self->mThread->Dispatch(NS_NewRunnableFunction(
                "nsProfiler::WaitOnePeriodicSampling result on main thread",
                [resolve = std::move(resolve), aSamplingState]() {
                  (*resolve)(aSamplingState ==
                                 SamplingState::SamplingCompleted ||
                             aSamplingState ==
                                 SamplingState::NoStackSamplingCompleted);
                }));
          })) {
    // Callback was not added (e.g., profiler is not running) and will never be
    // invoked, so we need to resolve the promise here.
    (*resolve)(false);
  }
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

struct GatherProfileThreadParameters
    : public external::AtomicRefCounted<GatherProfileThreadParameters> {
  MOZ_DECLARE_REFCOUNTED_TYPENAME(GatherProfileThreadParameters)

  GatherProfileThreadParameters(
      RefPtr<ProfilerChild> aProfilerChild,
      RefPtr<ProgressLogger::SharedProgress> aProgress,
      ProfilerChild::GatherProfileResolver&& aResolver)
      : profilerChild(std::move(aProfilerChild)),
        progress(std::move(aProgress)),
        resolver(std::move(aResolver)) {}

  RefPtr<ProfilerChild> profilerChild;

  // Separate RefPtr used when working on separate thread. This way, if the
  // "ProfilerChild" thread decides to overwrite its mGatherProfileProgress with
  // a new one, the work done here will still only use the old one.
  RefPtr<ProgressLogger::SharedProgress> progress;

  // Resolver for the GatherProfile promise. Must only be called on the
  // "ProfilerChild" thread.
  ProfilerChild::GatherProfileResolver resolver;
};

/* static */
void ProfilerChild::GatherProfileThreadFunction(
    void* already_AddRefedParameters) {
  PR_SetCurrentThreadName("GatherProfileThread");

  RefPtr<GatherProfileThreadParameters> parameters =
      already_AddRefed<GatherProfileThreadParameters>{
          static_cast<GatherProfileThreadParameters*>(
              already_AddRefedParameters)};

  ProgressLogger progressLogger(
      parameters->progress, "Gather-profile thread started", "Profile sent");
  using namespace mozilla::literals::ProportionValue_literals;  // For `1_pc`.

  auto writer = MakeUnique<SpliceableChunkedJSONWriter>();
  if (!profiler_get_profile_json(
          *writer,
          /* aSinceTime */ 0,
          /* aIsShuttingDown */ false,
          progressLogger.CreateSubLoggerFromTo(
              1_pc,
              "profiler_get_profile_json_into_lazily_allocated_buffer started",
              99_pc,
              "profiler_get_profile_json_into_lazily_allocated_buffer done"))) {
    // Failed to get a profile (profiler not running?), reset the writer
    // pointer, so that we'll send an empty string.
    writer.reset();
  }

  if (NS_WARN_IF(NS_FAILED(
          parameters->profilerChild->mThread->Dispatch(NS_NewRunnableFunction(
              "ProfilerChild::ProcessPendingUpdate",
              [parameters,
               // Forward progress logger to on-ProfilerChild-thread task, so
               // that it doesn't get marked as 100% done when this off-thread
               // function ends.
               progressLogger = std::move(progressLogger),
               writer = std::move(writer)]() mutable {
                // We are now on the ProfilerChild thread, about to send the
                // completed profile. Any incoming progress request will now be
                // handled after this task ends, so updating the progress is now
                // useless and we can just get rid of the progress storage.
                if (parameters->profilerChild->mGatherProfileProgress ==
                    parameters->progress) {
                  // The ProfilerChild progress is still the one we know.
                  parameters->profilerChild->mGatherProfileProgress = nullptr;
                }

                // Shmem allocation and promise resolution must be made on the
                // ProfilerChild thread, that's why this task was needed here.
                mozilla::ipc::Shmem shmem;
                if (writer) {
                  writer->ChunkedWriteFunc().CopyDataIntoLazilyAllocatedBuffer(
                      [&](size_t allocationSize) -> char* {
                        if (parameters->profilerChild->AllocShmem(
                                allocationSize,
                                mozilla::ipc::Shmem::SharedMemory::TYPE_BASIC,
                                &shmem)) {
                          return shmem.get<char>();
                        }
                        return nullptr;
                      });
                  writer = nullptr;
                } else {
                  // No profile, send an empty string.
                  if (parameters->profilerChild->AllocShmem(
                          1, mozilla::ipc::Shmem::SharedMemory::TYPE_BASIC,
                          &shmem)) {
                    shmem.get<char>()[0] = '\0';
                  }
                }

                parameters->resolver(std::move(shmem));
              }))))) {
    // Failed to dispatch the task to the ProfilerChild thread. The IPC cannot
    // be resolved on this thread, so it will never be resolved!
    // And it would be unsafe to modify mGatherProfileProgress; But the parent
    // should notice that's it's not advancing anymore.
  }
}

mozilla::ipc::IPCResult ProfilerChild::RecvGatherProfile(
    GatherProfileResolver&& aResolve) {
  mGatherProfileProgress = MakeRefPtr<ProgressLogger::SharedProgress>();
  mGatherProfileProgress->SetProgress(ProportionValue{0.0},
                                      "Received gather-profile request");

  auto parameters = MakeRefPtr<GatherProfileThreadParameters>(
      this, mGatherProfileProgress, std::move(aResolve));

  // The GatherProfileThreadFunction thread function will cast its void*
  // argument to already_AddRefed<GatherProfileThreadParameters>.
  parameters.get()->AddRef();
  PRThread* gatherProfileThread = PR_CreateThread(
      PR_SYSTEM_THREAD, GatherProfileThreadFunction, parameters.get(),
      PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);

  if (!gatherProfileThread) {
    // Failed to create and start worker thread, resolve with an empty profile.
    mozilla::ipc::Shmem shmem;
    if (AllocShmem(1, mozilla::ipc::Shmem::SharedMemory::TYPE_BASIC, &shmem)) {
      shmem.get<char>()[0] = '\0';
    }
    parameters->resolver(std::move(shmem));
    // And clean up.
    parameters.get()->Release();
    mGatherProfileProgress = nullptr;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult ProfilerChild::RecvGetGatherProfileProgress(
    GetGatherProfileProgressResolver&& aResolve) {
  if (mGatherProfileProgress) {
    aResolve(GatherProfileProgress{
        mGatherProfileProgress->Progress().ToUnderlyingType(),
        nsCString(mGatherProfileProgress->LastLocation())});
  } else {
    aResolve(
        GatherProfileProgress{ProportionValue::MakeInvalid().ToUnderlyingType(),
                              nsCString("No gather-profile in progress")});
  }
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
