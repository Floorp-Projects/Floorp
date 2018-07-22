/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ProcessRewind_h
#define mozilla_recordreplay_ProcessRewind_h

#include "mozilla/RecordReplay.h"

#include <functional>

namespace mozilla {
namespace recordreplay {

// This file is responsible for keeping track of and managing the current point
// of execution when replaying an execution, and in allowing the process to
// rewind its state to an earlier point of execution.

///////////////////////////////////////////////////////////////////////////////
// Checkpoints Overview.
//
// Checkpoints are reached periodically by the main thread of a recording or
// replaying process. Checkpoints must be reached at consistent points between
// different executions of the recording. Currently they are taken after XPCOM
// initialization and every time compositor updates are performed. Each
// checkpoint has an ID, which monotonically increases during the execution.
// Checkpoints form a basis for identifying a particular point in execution,
// and in allowing replaying processes to rewind themselves.
//
// A subset of checkpoints are saved: the contents of each thread's stack is
// copied, along with enough information to restore the contents of heap memory
// at the checkpoint.
//
// Saved checkpoints are in part represented as diffs vs the following
// saved checkpoint. This requires some different handling for the most recent
// saved checkpoint (whose diff has not been computed) and earlier saved
// checkpoints. See MemorySnapshot.h and Thread.h for more on how saved
// checkpoints are represented.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Controlling a Replaying Process.
//
// 1. While performing the replay, execution proceeds until the main thread
//    hits either a breakpoint or a checkpoint.
//
// 2. The main thread then calls a hook (JS::replay::hooks.hitBreakpointReplay
//    or gAfterCheckpointHook), which may decide to pause the main thread and
//    give it a callback to invoke using PauseMainThreadAndInvokeCallback.
//
// 3. Now that the main thread is paused, the replay message loop thread
//    (see ChildIPC.h) can give it additional callbacks to invoke using
//    PauseMainThreadAndInvokeCallback.
//
// 4. These callbacks can inspect the paused state, diverge from the recording
//    by calling DivergeFromRecording, and eventually can unpause the main
//    thread and allow execution to resume by calling ResumeExecution
//    (if DivergeFromRecording was not called) or RestoreCheckpointAndResume.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Recording Divergence.
//
// Callbacks invoked while debugging (during step 3 of the above comment) might
// try to interact with the system, triggering thread events and attempting to
// replay behaviors that never occurred while recording.
//
// To allow these callbacks the freedom to operate without bringing down the
// entire replay, the DivergeFromRecording API is provided; see RecordReplay.h
// After this is called, some thread events will happen as if events were
// passed through, but other events that require interacting with the system
// will trigger an unhandled divergence from the recording via
// EnsureNotDivergedFromRecording, causing the process to rewind to the most
// recent saved checkpoint. The debugger will recognize this rewind and play
// back in a way that restores the state when DivergeFromRecording() was
// called, but without performing the later operation that triggered the
// rewind.
///////////////////////////////////////////////////////////////////////////////

// The ID of a checkpoint in a child process. Checkpoints are either normal or
// temporary. Normal checkpoints occur at the same point in the recording and
// all replays, while temporary checkpoints are not used while recording and
// may be at different points in different replays.
struct CheckpointId
{
  // ID of the most recent normal checkpoint, which are numbered in sequence
  // starting at FirstCheckpointId.
  size_t mNormal;

  // Special IDs for normal checkpoints.
  static const size_t Invalid = 0;
  static const size_t First = 1;

  // How many temporary checkpoints have been generated since the most recent
  // normal checkpoint, zero if this represents the normal checkpoint itself.
  size_t mTemporary;

  explicit CheckpointId(size_t aNormal = Invalid, size_t aTemporary = 0)
    : mNormal(aNormal), mTemporary(aTemporary)
  {}

  inline bool operator==(const CheckpointId& o) const {
    return mNormal == o.mNormal && mTemporary == o.mTemporary;
  }

  inline bool operator!=(const CheckpointId& o) const {
    return mNormal != o.mNormal || mTemporary != o.mTemporary;
  }

  CheckpointId NextCheckpoint(bool aTemporary) const {
    return CheckpointId(aTemporary ? mNormal : mNormal + 1,
                        aTemporary ? mTemporary + 1 : 0);
  }
};

// Initialize state needed for rewinding.
void InitializeRewindState();

// Set whether this process should save a particular checkpoint.
void SetSaveCheckpoint(size_t aCheckpoint, bool aSave);

// Invoke a callback on the main thread, and pause it until ResumeExecution or
// RestoreCheckpointAndResume are called. When the main thread is not paused,
// this must be called on the main thread itself. When the main thread is
// already paused, this may be called from any thread.
void PauseMainThreadAndInvokeCallback(const std::function<void()>& aCallback);

// Return whether the main thread should be paused. This does not necessarily
// mean it is paused, but it will pause at the earliest opportunity.
bool MainThreadShouldPause();

// Pause the current main thread and service any callbacks until the thread no
// longer needs to pause.
void PauseMainThreadAndServiceCallbacks();

// Return whether any checkpoints have been saved.
bool HasSavedCheckpoint();

// Get the ID of the most recent saved checkpoint.
CheckpointId GetLastSavedCheckpoint();

// When paused at a breakpoint or at a checkpoint, restore a checkpoint that
// was saved earlier and resume execution.
void RestoreCheckpointAndResume(const CheckpointId& aCheckpoint);

// When paused at a breakpoint or at a checkpoint, unpause and proceed with
// execution.
void ResumeExecution();

// Allow execution after this point to diverge from the recording. Execution
// will remain diverged until an earlier checkpoint is restored.
//
// If an unhandled divergence occurs (see the 'Recording Divergence' comment
// in ProcessRewind.h) then the process rewinds to the most recent saved
// checkpoint.
void DivergeFromRecording();

// After a call to DivergeFromRecording(), this may be called to prevent future
// unhandled divergence from causing earlier checkpoints to be restored
// (the process will immediately crash instead). This state lasts until a new
// call to DivergeFromRecording, or to an explicit restore of an earlier
// checkpoint.
void DisallowUnhandledDivergeFromRecording();

// Make sure that execution has not diverged from the recording after a call to
// DivergeFromRecording, by rewinding to the last saved checkpoint if so.
void EnsureNotDivergedFromRecording();

// Access the flag for whether this is the active child process.
void SetIsActiveChild(bool aActive);
bool IsActiveChild();

// Note a checkpoint at the current execution position. This checkpoint will be
// saved if either (a) it is temporary, or (b) the middleman has instructed
// this process to save this normal checkpoint. This method returns true if the
// checkpoint was just saved, and false if it was just restored.
bool NewCheckpoint(bool aTemporary);

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_ProcessRewind_h
