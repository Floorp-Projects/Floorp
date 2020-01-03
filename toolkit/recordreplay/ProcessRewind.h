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
// In a replaying process, snapshots can be taken that retain enough information
// to restore the contents of heap memory and thread stacks at the point the
// snapshot was taken. Snapshots are usually taken when certain checkpoints are
// reached, but they can be taken at other points as well.
//
// See MemorySnapshot.h and ThreadSnapshot.h for information on how snapshots
// are represented.
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Controlling a Replaying Process.
//
// 1. While performing the replay, execution proceeds until the main thread
//    hits either a breakpoint or a checkpoint.
//
// 2. Control then enters the logic in devtools/server/actors/replay/replay.js,
//    which may decide to pause the main thread.
//
// 3. The replay logic can inspect the process state, diverge from the recording
//    by calling DivergeFromRecording, and eventually can unpause the main
//    thread and allow execution to resume by calling ResumeExecution
//    (if DivergeFromRecording was not called) or RestoreSnapshotAndResume.
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
// recent snapshot. The debugger will recognize this rewind and play
// back in a way that restores the state when DivergeFromRecording() was
// called, but without performing the later operation that triggered the
// rewind.
///////////////////////////////////////////////////////////////////////////////

// Special IDs for normal checkpoints.
static const size_t InvalidCheckpointId = 0;
static const size_t FirstCheckpointId = 1;

// Initialize state needed for rewinding.
void InitializeRewindState();

// Invoke a callback on the main thread, and pause it until ResumeExecution or
// RestoreSnapshotAndResume are called. When the main thread is not paused,
// this must be called on the main thread itself. When the main thread is
// already paused, this may be called from any thread.
void PauseMainThreadAndInvokeCallback(const std::function<void()>& aCallback);

// Return whether the main thread should be paused. This does not necessarily
// mean it is paused, but it will pause at the earliest opportunity.
bool MainThreadShouldPause();

// Pause the current main thread and service any callbacks until the thread no
// longer needs to pause.
void PauseMainThreadAndServiceCallbacks();

// Return how many snapshots have been taken.
size_t NumSnapshots();

// Get the ID of the most recently encountered checkpoint.
size_t GetLastCheckpoint();

// When paused at a breakpoint or at a checkpoint, restore a snapshot that
// was saved earlier. aNumSnapshots is the number of snapshots to skip over
// when restoring.
void RestoreSnapshotAndResume(size_t aNumSnapshots);

// When paused at a breakpoint or at a checkpoint, unpause and proceed with
// execution.
void ResumeExecution();

// Allow execution after this point to diverge from the recording. Execution
// will remain diverged until an earlier snapshot is restored.
//
// If an unhandled divergence occurs (see the 'Recording Divergence' comment
// in ProcessRewind.h) then the process rewinds to the most recent snapshot.
void DivergeFromRecording();

// After a call to DivergeFromRecording(), this may be called to prevent future
// unhandled divergence from causing earlier snapshots to be restored
// (the process will immediately crash instead). This state lasts until a new
// call to DivergeFromRecording, or to an explicit restore of an earlier
// snapshot.
void DisallowUnhandledDivergeFromRecording();

// Make sure that execution has not diverged from the recording after a call to
// DivergeFromRecording, by rewinding to the last snapshot if so.
void EnsureNotDivergedFromRecording();

// Note a checkpoint at the current execution position.
void NewCheckpoint();

// Create a new snapshot that can be restored later. This method returns true
// if the snapshot was just taken, and false if it was just restored.
bool NewSnapshot();

}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_ProcessRewind_h
