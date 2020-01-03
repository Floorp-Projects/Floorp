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

// The most recent checkpoint which was encountered.
static size_t gLastCheckpoint = InvalidCheckpointId;

// Information about the current rewinding state. The contents of this structure
// are in untracked memory.
struct RewindInfo {
  // Thread stacks for snapshots which have been saved.
  InfallibleVector<AllSavedThreadStacks, 1024, AllocPolicy<MemoryKind::Generic>>
      mSnapshots;
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

  gMainThreadCallbackMonitor = new Monitor();
}

void RestoreSnapshotAndResume(size_t aNumSnapshots) {
  MOZ_RELEASE_ASSERT(IsReplaying());
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());
  MOZ_RELEASE_ASSERT(aNumSnapshots < gRewindInfo->mSnapshots.length());

  // Make sure we don't lose pending main thread callbacks due to rewinding.
  {
    MonitorAutoLock lock(*gMainThreadCallbackMonitor);
    MOZ_RELEASE_ASSERT(gMainThreadCallbacks.empty());
  }

  Thread::WaitForIdleThreads();

  double start = CurrentTime();

  {
    // Rewind heap memory to the target snapshot.
    AutoDisallowMemoryChanges disallow;
    RestoreMemoryToLastSnapshot();
    for (size_t i = 0; i < aNumSnapshots; i++) {
      gRewindInfo->mSnapshots.back().ReleaseContents();
      gRewindInfo->mSnapshots.popBack();
      RestoreMemoryToLastDiffSnapshot();
    }
  }

  FixupFreeRegionsAfterRewind();

  double end = CurrentTime();
  PrintSpew("Restore %.2fs\n", (end - start) / 1000000.0);

  // Finally, let threads restore themselves to their stacks at the snapshot
  // we are rewinding to.
  RestoreAllThreads(gRewindInfo->mSnapshots.back());
  Unreachable();
}

bool NewSnapshot() {
  if (IsRecording()) {
    return true;
  }

  Thread::WaitForIdleThreads();

  PrintSpew("Saving snapshot...\n");

  double start = CurrentTime();

  // Record either the first or a subsequent diff memory snapshot.
  if (gRewindInfo->mSnapshots.empty()) {
    TakeFirstMemorySnapshot();
  } else {
    TakeDiffMemorySnapshot();
  }
  gRewindInfo->mSnapshots.emplaceBack();

  double end = CurrentTime();

  bool reached = true;

  // Save all thread stacks for the snapshot. If we rewind here from a
  // later point of execution then this will return false.
  if (SaveAllThreads(gRewindInfo->mSnapshots.back())) {
    PrintSpew("Saved snapshot %.2fs\n", (end - start) / 1000000.0);
  } else {
    PrintSpew("Restored snapshot\n");

    reached = false;

    // After restoring, make sure all threads have updated their stacks
    // before letting any of them resume execution. Threads might have
    // pointers into each others' stacks.
    WaitForIdleThreadsToRestoreTheirStacks();
  }

  Thread::ResumeIdleThreads();

  return reached;
}

void NewCheckpoint() {
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());
  MOZ_RELEASE_ASSERT(!HasDivergedFromRecording());

  gLastCheckpoint++;

  js::HitCheckpoint(gLastCheckpoint);
}

static bool gUnhandledDivergeAllowed;

void DivergeFromRecording() {
  MOZ_RELEASE_ASSERT(IsReplaying());

  Thread* thread = Thread::Current();
  MOZ_RELEASE_ASSERT(thread->IsMainThread());

  if (!thread->HasDivergedFromRecording()) {
    // Reset middleman call state whenever we first diverge from the recording.
    child::SendResetMiddlemanCalls();

    thread->DivergeFromRecording();

    // Direct all other threads to diverge from the recording as well.
    Thread::WaitForIdleThreads();
    for (size_t i = MainThreadId + 1; i <= MaxRecordedThreadId; i++) {
      Thread::GetById(i)->SetShouldDivergeFromRecording();
    }
    Thread::ResumeIdleThreads();
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
  // handle, rewind to the last snapshot.
  AssertEventsAreNotPassedThrough();
  if (HasDivergedFromRecording()) {
    MOZ_RELEASE_ASSERT(gUnhandledDivergeAllowed);

    // Crash instead of rewinding if a repaint is about to fail and is not
    // allowed.
    if (child::CurrentRepaintCannotFail()) {
      MOZ_CRASH("Recording divergence while repainting");
    }

    PrintSpew("Unhandled recording divergence, restoring snapshot...\n");
    RestoreSnapshotAndResume(0);
    Unreachable();
  }
}

size_t NumSnapshots() {
  return gRewindInfo ? gRewindInfo->mSnapshots.length() : 0;
}

size_t GetLastCheckpoint() { return gLastCheckpoint; }

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

  // As for RestoreSnapshotAndResume, we shouldn't resume the main thread
  // while it still has callbacks to execute.
  MOZ_RELEASE_ASSERT(gMainThreadCallbacks.empty());

  // If we diverge from the recording the only way we can get back to resuming
  // normal execution is to rewind to a snapshot prior to the divergence.
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
