/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessRewind.h"

#include "nsString.h"
#include "ipc/ChildInternal.h"
#include "ipc/ParentInternal.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/StaticMutex.h"
#include "InfallibleVector.h"
#include "MemorySnapshot.h"
#include "Monitor.h"
#include "ProcessRecordReplay.h"
#include "ThreadSnapshot.h"

namespace mozilla {
namespace recordreplay {

// Information about the current rewinding state. The contents of this structure
// are in untracked memory.
struct RewindInfo {
  // The most recent checkpoint which was encountered.
  size_t mLastCheckpoint;

  // Checkpoints which have been saved. This includes only entries from
  // mShouldSaveCheckpoints, plus all temporary checkpoints.
  InfallibleVector<SavedCheckpoint, 1024, AllocPolicy<MemoryKind::Generic>>
      mSavedCheckpoints;

  // Unsorted list of checkpoints which the middleman has instructed us to
  // save. All those equal to or prior to mLastCheckpoint will have been saved.
  InfallibleVector<size_t, 1024, AllocPolicy<MemoryKind::Generic>>
      mShouldSaveCheckpoints;
};

static RewindInfo* gRewindInfo;

// Lock for managing pending main thread callbacks.
static Monitor* gMainThreadCallbackMonitor;

// Callbacks to execute on the main thread, in FIFO order. Protected by
// gMainThreadCallbackMonitor.
static StaticInfallibleVector<std::function<void()>> gMainThreadCallbacks;

void InitializeRewindState() {
  MOZ_RELEASE_ASSERT(gRewindInfo == nullptr);
  void* memory = AllocateMemory(sizeof(RewindInfo), MemoryKind::Generic);
  gRewindInfo = new (memory) RewindInfo();

  // The first checkpoint is implicitly saved while replaying: we won't be able
  // to get a manifest from the middleman telling us what to save until after
  // this checkpoint has been reached.
  if (IsReplaying()) {
    gRewindInfo->mShouldSaveCheckpoints.append(FirstCheckpointId);
  }

  gMainThreadCallbackMonitor = new Monitor();
}

void RestoreCheckpointAndResume(size_t aCheckpoint) {
  MOZ_RELEASE_ASSERT(IsReplaying());
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());
  MOZ_RELEASE_ASSERT(
      aCheckpoint == gRewindInfo->mLastCheckpoint ||
      aCheckpoint < gRewindInfo->mLastCheckpoint);

  // Make sure we don't lose pending main thread callbacks due to rewinding.
  {
    MonitorAutoLock lock(*gMainThreadCallbackMonitor);
    MOZ_RELEASE_ASSERT(gMainThreadCallbacks.empty());
  }

  Thread::WaitForIdleThreads();

  double start = CurrentTime();

  {
    // Rewind heap memory to the target checkpoint, which must have been saved.
    AutoDisallowMemoryChanges disallow;
    size_t newCheckpoint =
        gRewindInfo->mSavedCheckpoints.back().mCheckpoint;
    RestoreMemoryToLastSavedCheckpoint();
    while (aCheckpoint < newCheckpoint) {
      gRewindInfo->mSavedCheckpoints.back().ReleaseContents();
      gRewindInfo->mSavedCheckpoints.popBack();
      RestoreMemoryToLastSavedDiffCheckpoint();
      newCheckpoint = gRewindInfo->mSavedCheckpoints.back().mCheckpoint;
    }
    MOZ_RELEASE_ASSERT(newCheckpoint == aCheckpoint);
  }

  FixupFreeRegionsAfterRewind();

  double end = CurrentTime();
  PrintSpew("Restore #%d -> #%d %.2fs\n",
            (int)gRewindInfo->mLastCheckpoint, (int)aCheckpoint,
            (end - start) / 1000000.0);

  // Finally, let threads restore themselves to their stacks at the checkpoint
  // we are rewinding to.
  RestoreAllThreads(gRewindInfo->mSavedCheckpoints.back());
  Unreachable();
}

void SetSaveCheckpoint(size_t aCheckpoint, bool aSave) {
  MOZ_RELEASE_ASSERT(aCheckpoint > gRewindInfo->mLastCheckpoint);
  VectorAddOrRemoveEntry(gRewindInfo->mShouldSaveCheckpoints, aCheckpoint,
                         aSave);
}

bool NewCheckpoint() {
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());
  MOZ_RELEASE_ASSERT(!HasDivergedFromRecording());

  js::BeforeCheckpoint();

  // Get the ID of the new checkpoint.
  size_t checkpoint = gRewindInfo->mLastCheckpoint + 1;

  // Save all checkpoints the middleman tells us to, and temporary checkpoints
  // (which the middleman never knows about).
  bool save = VectorContains(gRewindInfo->mShouldSaveCheckpoints, checkpoint);
  bool reachedCheckpoint = true;

  if (save) {
    Thread::WaitForIdleThreads();

    PrintSpew("Starting checkpoint...\n");

    double start = CurrentTime();

    // Record either the first or a subsequent diff memory snapshot.
    if (gRewindInfo->mSavedCheckpoints.empty()) {
      TakeFirstMemorySnapshot();
    } else {
      TakeDiffMemorySnapshot();
    }
    gRewindInfo->mSavedCheckpoints.emplaceBack(checkpoint);

    double end = CurrentTime();

    // Save all thread stacks for the checkpoint. If we rewind here from a
    // later point of execution then this will return false.
    if (SaveAllThreads(gRewindInfo->mSavedCheckpoints.back())) {
      PrintSpew("Saved checkpoint #%d %.2fs\n", (int)checkpoint,
                (end - start) / 1000000.0);
    } else {
      PrintSpew("Restored checkpoint #%d\n", (int)checkpoint);

      reachedCheckpoint = false;

      // After restoring, make sure all threads have updated their stacks
      // before letting any of them resume execution. Threads might have
      // pointers into each others' stacks.
      WaitForIdleThreadsToRestoreTheirStacks();
    }

    Thread::ResumeIdleThreads();
  } else {
    PrintSpew("Skipping checkpoint #%d\n", (int)checkpoint);
  }

  gRewindInfo->mLastCheckpoint = checkpoint;

  js::AfterCheckpoint(checkpoint, !reachedCheckpoint);

  return reachedCheckpoint;
}

static bool gUnhandledDivergeAllowed;

void DivergeFromRecording() {
  MOZ_RELEASE_ASSERT(IsReplaying());

  Thread* thread = Thread::Current();
  MOZ_RELEASE_ASSERT(thread->IsMainThread());

  if (!thread->HasDivergedFromRecording()) {
    // Reset middleman call state whenever we first diverge from the recording.
    child::SendResetMiddlemanCalls();

    // Make sure all non-main threads are idle before we begin diverging. This
    // thread's new behavior can change values used by other threads and induce
    // recording mismatches.
    Thread::WaitForIdleThreads();

    thread->DivergeFromRecording();
  }

  gUnhandledDivergeAllowed = true;
}

extern "C" {

MOZ_EXPORT bool RecordReplayInterface_InternalHasDivergedFromRecording() {
  Thread* thread = Thread::Current();
  return thread && thread->HasDivergedFromRecording();
}

}  // extern "C"

void DisallowUnhandledDivergeFromRecording() {
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  gUnhandledDivergeAllowed = false;
}

void EnsureNotDivergedFromRecording() {
  // If we have diverged from the recording and encounter an operation we can't
  // handle, rewind to the last checkpoint.
  AssertEventsAreNotPassedThrough();
  if (HasDivergedFromRecording()) {
    MOZ_RELEASE_ASSERT(gUnhandledDivergeAllowed);

    // Crash instead of rewinding if a repaint is about to fail and is not
    // allowed.
    if (child::CurrentRepaintCannotFail()) {
      MOZ_CRASH("Recording divergence while repainting");
    }

    PrintSpew("Unhandled recording divergence, restoring checkpoint...\n");
    RestoreCheckpointAndResume(
        gRewindInfo->mSavedCheckpoints.back().mCheckpoint);
    Unreachable();
  }
}

bool HasSavedAnyCheckpoint() {
  return gRewindInfo && !gRewindInfo->mSavedCheckpoints.empty();
}

bool HasSavedCheckpoint(size_t aCheckpoint) {
  if (!gRewindInfo) {
    return false;
  }
  for (const SavedCheckpoint& saved : gRewindInfo->mSavedCheckpoints) {
    if (saved.mCheckpoint == aCheckpoint) {
      return true;
    }
  }
  return false;
}

size_t GetLastCheckpoint() {
  return gRewindInfo ? gRewindInfo->mLastCheckpoint : InvalidCheckpointId;
}

size_t GetLastSavedCheckpoint() {
  MOZ_RELEASE_ASSERT(HasSavedAnyCheckpoint());
  return gRewindInfo->mSavedCheckpoints.back().mCheckpoint;
}

static bool gMainThreadShouldPause = false;

bool MainThreadShouldPause() { return gMainThreadShouldPause; }

void PauseMainThreadAndServiceCallbacks() {
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  AssertEventsAreNotPassedThrough();

  // Whether there is a PauseMainThreadAndServiceCallbacks frame on the stack.
  static bool gMainThreadIsPaused = false;

  if (gMainThreadIsPaused) {
    return;
  }
  gMainThreadIsPaused = true;

  MOZ_RELEASE_ASSERT(!HasDivergedFromRecording());

  MonitorAutoLock lock(*gMainThreadCallbackMonitor);

  // Loop and invoke callbacks until one of them unpauses this thread.
  while (gMainThreadShouldPause) {
    if (!gMainThreadCallbacks.empty()) {
      std::function<void()> callback = gMainThreadCallbacks[0];
      gMainThreadCallbacks.erase(&gMainThreadCallbacks[0]);
      {
        MonitorAutoUnlock unlock(*gMainThreadCallbackMonitor);
        AutoDisallowThreadEvents disallow;
        callback();
      }
    } else {
      gMainThreadCallbackMonitor->Wait();
    }
  }

  // As for RestoreCheckpointAndResume, we shouldn't resume the main thread
  // while it still has callbacks to execute.
  MOZ_RELEASE_ASSERT(gMainThreadCallbacks.empty());

  // If we diverge from the recording the only way we can get back to resuming
  // normal execution is to rewind to a checkpoint prior to the divergence.
  MOZ_RELEASE_ASSERT(!HasDivergedFromRecording());

  gMainThreadIsPaused = false;
}

void PauseMainThreadAndInvokeCallback(const std::function<void()>& aCallback) {
  {
    MonitorAutoLock lock(*gMainThreadCallbackMonitor);
    gMainThreadShouldPause = true;
    gMainThreadCallbacks.append(aCallback);
    gMainThreadCallbackMonitor->Notify();
  }

  if (Thread::CurrentIsMainThread()) {
    PauseMainThreadAndServiceCallbacks();
  }
}

void ResumeExecution() {
  MonitorAutoLock lock(*gMainThreadCallbackMonitor);
  gMainThreadShouldPause = false;
  gMainThreadCallbackMonitor->Notify();
}

}  // namespace recordreplay
}  // namespace mozilla
