/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ChildInternal_h
#define mozilla_recordreplay_ChildInternal_h

#include "ChildIPC.h"
#include "JSControl.h"
#include "Monitor.h"

namespace mozilla {
namespace recordreplay {

// The navigation namespace has definitions for managing breakpoints and all
// other state that persists across rewinds, and for keeping track of the
// precise execution position of the child process. The middleman will send the
// child process Resume messages to travel forward and backward, but it is up
// to the child process to keep track of the rewinding and resuming necessary
// to find the next or previous point where a breakpoint or checkpoint is hit.
namespace navigation {

// Navigation state is initialized when the first checkpoint is reached.
bool IsInitialized();

// In a recording process, get the current execution point, aka the endpoint
// of the recording.
js::ExecutionPoint GetRecordingEndpoint();

// In a replaying process, set the recording endpoint. |index| is used to
// differentiate different endpoints that have been sequentially written to
// the recording file as it has been flushed.
void SetRecordingEndpoint(size_t aIndex, const js::ExecutionPoint& aEndpoint);

// Save temporary checkpoints at all opportunities during navigation.
void AlwaysSaveTemporaryCheckpoints();

// Process incoming requests from the middleman.
void DebuggerRequest(js::CharBuffer* aBuffer);
void SetBreakpoint(size_t aId, const js::BreakpointPosition& aPosition);
void Resume(bool aForward);
void RestoreCheckpoint(size_t aId);

// Attempt to diverge from the recording so that new recorded events cause
// the process to rewind. Returns false if the divergence failed: either we
// can't rewind, or already diverged here and then had an unhandled divergence.
bool MaybeDivergeFromRecording();

// Notify navigation that a position was hit.
void PositionHit(const js::BreakpointPosition& aPosition);

// Called when running forward, immediately before hitting a normal or
// temporary checkpoint.
void BeforeCheckpoint();

// Called immediately after hitting a normal or temporary checkpoint, either
// when running forward or immediately after rewinding.
void AfterCheckpoint(const CheckpointId& aCheckpoint);

} // namespace navigation

// IPC activity that can be triggered by navigation.
namespace child {

void RespondToRequest(const js::CharBuffer& aBuffer);

void HitCheckpoint(size_t aId, bool aRecordingEndpoint);

void HitBreakpoint(bool aRecordingEndpoint, const uint32_t* aBreakpoints, size_t aNumBreakpoints);

// Monitor used for various synchronization tasks.
extern Monitor* gMonitor;

} // namespace child

} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_ChildInternal_h
