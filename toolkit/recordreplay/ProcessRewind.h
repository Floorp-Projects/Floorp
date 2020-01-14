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
//    (if DivergeFromRecording was not called).
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
// EnsureNotDivergedFromRecording. The debugger will recognize this rewind and
// play back in a way that restores the state when DivergeFromRecording() was
// called, but without performing the later operation that triggered the
// rewind.
///////////////////////////////////////////////////////////////////////////////

// Special IDs for normal checkpoints.
static const size_t InvalidCheckpointId = 0;
static const size_t FirstCheckpointId = 1;

// Initialize state needed for rewinding.
void InitializeRewindState();

// Invoke a callback on the main thread, and pause it until ResumeExecution is
// called. When the main thread is not paused, this must be called on the main
// thread itself. When the main thread is already paused, this may be called
// from any thread.
void PauseMainThreadAndInvokeCallback(const std::function<void()>& aCallback);

// Return whether the main thread should be paused. This does not necessarily
// mean it is paused, but it will pause at the earliest opportunity.
bool MainThreadShouldPause();

// Pause the current main thread and service any callbacks until the thread no
// longer needs to pause.
void PauseMainThreadAndServiceCallbacks();

// Get the ID of the most recently encountered checkpoint.
size_t GetLastCheckpoint();

// Fork a new processs from this one. Returns true if this is the original
// process, or false if this is the fork.
bool ForkProcess();

// Ensure that non-main threads have been respawned after a fork.
void EnsureNonMainThreadsAreSpawned();

// When paused at a breakpoint or at a checkpoint, unpause and proceed with
// execution.
void ResumeExecution();

// Allow execution after this point to diverge from the recording.
void DivergeFromRecording();

// After a call to DivergeFromRecording(), this may be called to prevent future
// unhandled divergence from following the normal bailouot path
// (the process will immediately crash instead). This state lasts until a new
// call to DivergeFromRecording.
void DisallowUnhandledDivergeFromRecording();

// Make sure that execution has not diverged from the recording after a call to
// DivergeFromRecording.
void EnsureNotDivergedFromRecording(const Maybe<int>& aCallId);

// Note a checkpoint at the current execution position.
void NewCheckpoint();

}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_ProcessRewind_h
