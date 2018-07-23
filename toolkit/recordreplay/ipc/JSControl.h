/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_JSControl_h
#define mozilla_recordreplay_JSControl_h

#include "jsapi.h"

#include "InfallibleVector.h"
#include "ProcessRewind.h"

#include "mozilla/DefineEnum.h"

namespace mozilla {
namespace recordreplay {
namespace js {

// This file manages interactions between the record/replay infrastructure and
// JS code. This interaction can occur in two ways:
//
// - In the middleman process, devtools server code can use the
//   RecordReplayControl object to send requests to the recording/replaying
//   child process and control its behavior.
//
// - In the recording/replaying process, a JS sandbox is created before the
//   first checkpoint is reached, which responds to the middleman's requests.
//   The RecordReplayControl object is also provided here, but has a different
//   interface which allows the JS to query the current process.

// Identification for a position where a breakpoint can be installed in a child
// process. Breakpoint positions describe all places between checkpoints where
// the child process can pause and be inspected by the middleman. A particular
// BreakpointPosition can be reached any number of times during execution of
// the process.
struct BreakpointPosition
{
  MOZ_DEFINE_ENUM_AT_CLASS_SCOPE(Kind, (
    Invalid,

    // Break at a script offset. Requires script/offset.
    Break,

    // Break for an on-step handler within a frame.
    // Requires script/offset/frameIndex.
    OnStep,

    // Break either when any frame is popped, or when a specific frame is
    // popped. Requires script/frameIndex in the latter case.
    OnPop,

    // Break when entering any frame.
    EnterFrame,

    // Break when a new top-level script is created.
    NewScript
  ));

  Kind mKind;

  // Optional information associated with the breakpoint.
  uint32_t mScript;
  uint32_t mOffset;
  uint32_t mFrameIndex;

  static const uint32_t EMPTY_SCRIPT = (uint32_t) -1;
  static const uint32_t EMPTY_OFFSET = (uint32_t) -1;
  static const uint32_t EMPTY_FRAME_INDEX = (uint32_t) -1;

  BreakpointPosition()
    : mKind(Invalid), mScript(EMPTY_SCRIPT), mOffset(EMPTY_OFFSET), mFrameIndex(EMPTY_FRAME_INDEX)
  {}

  explicit BreakpointPosition(Kind aKind,
                              uint32_t aScript = EMPTY_SCRIPT,
                              uint32_t aOffset = EMPTY_OFFSET,
                              uint32_t aFrameIndex = EMPTY_FRAME_INDEX)
    : mKind(aKind), mScript(aScript), mOffset(aOffset), mFrameIndex(aFrameIndex)
  {}

  bool IsValid() const { return mKind != Invalid; }

  inline bool operator==(const BreakpointPosition& o) const {
    return mKind == o.mKind
        && mScript == o.mScript
        && mOffset == o.mOffset
        && mFrameIndex == o.mFrameIndex;
  }

  inline bool operator!=(const BreakpointPosition& o) const { return !(*this == o); }

  // Return whether an execution point matching |o| also matches this.
  inline bool Subsumes(const BreakpointPosition& o) const {
    return (*this == o)
        || (mKind == OnPop && o.mKind == OnPop && mScript == EMPTY_SCRIPT)
        || (mKind == Break && o.mKind == OnStep && mScript == o.mScript && mOffset == o.mOffset);
  }

  static const char* StaticKindString(Kind aKind) {
    switch (aKind) {
    case Invalid: return "Invalid";
    case Break: return "Break";
    case OnStep: return "OnStep";
    case OnPop: return "OnPop";
    case EnterFrame: return "EnterFrame";
    case NewScript: return "NewScript";
    }
    MOZ_CRASH("Bad BreakpointPosition kind");
  }

  const char* KindString() const {
    return StaticKindString(mKind);
  }

  JSObject* Encode(JSContext* aCx) const;
  bool Decode(JSContext* aCx, JS::HandleObject aObject);
};

// Identification for a point in the execution of a child process where it may
// pause and be inspected by the middleman. A particular execution point will
// be reached exactly once during the execution of the process.
struct ExecutionPoint
{
  // ID of the last normal checkpoint prior to this point.
  size_t mCheckpoint;

  // How much progress execution has made prior to reaching the point,
  // or zero if the execution point refers to the checkpoint itself.
  //
  // A given BreakpointPosition may not be reached twice without an intervening
  // increment of the global progress counter.
  ProgressCounter mProgress;

  // The position reached after making the specified amount of progress,
  // invalid if the execution point refers to the checkpoint itself.
  BreakpointPosition mPosition;

  ExecutionPoint()
    : mCheckpoint(CheckpointId::Invalid)
    , mProgress(0)
  {}

  explicit ExecutionPoint(size_t aCheckpoint)
    : mCheckpoint(aCheckpoint)
    , mProgress(0)
  {}

  ExecutionPoint(size_t aCheckpoint, ProgressCounter aProgress,
                 const BreakpointPosition& aPosition)
    : mCheckpoint(aCheckpoint), mProgress(aProgress), mPosition(aPosition)
  {
    // ExecutionPoint positions must be as precise as possible, and cannot
    // subsume other positions.
    MOZ_RELEASE_ASSERT(aPosition.IsValid());
    MOZ_RELEASE_ASSERT(aPosition.mKind != BreakpointPosition::OnPop ||
                       aPosition.mScript != BreakpointPosition::EMPTY_SCRIPT);
    MOZ_RELEASE_ASSERT(aPosition.mKind != BreakpointPosition::Break);
  }

  bool HasPosition() const { return mPosition.IsValid(); }

  inline bool operator==(const ExecutionPoint& o) const {
    return mCheckpoint == o.mCheckpoint
        && mProgress == o.mProgress
        && mPosition == o.mPosition;
  }

  inline bool operator!=(const ExecutionPoint& o) const { return !(*this == o); }
};

// Buffer type used for encoding object data.
typedef InfallibleVector<char16_t> CharBuffer;

// Called in the middleman when a breakpoint with the specified id has been hit.
bool HitBreakpoint(JSContext* aCx, size_t id);

// Set up the JS sandbox in the current recording/replaying process and load
// its target script.
void SetupDevtoolsSandbox();

// The following hooks are used in the recording/replaying process to
// call methods defined by the JS sandbox.

// Handle an incoming request from the middleman.
void ProcessRequest(const char16_t* aRequest, size_t aRequestLength,
                    CharBuffer* aResponse);

// Ensure there is a handler in place that will call RecordReplayControl.positionHit
// whenever the specified execution position is reached.
void EnsurePositionHandler(const BreakpointPosition& aPosition);

// Clear all installed position handlers.
void ClearPositionHandlers();

// Clear all state that is kept while execution is paused.
void ClearPausedState();

// Given an execution position inside a script, get an execution position for
// the entry point of that script, otherwise return nothing.
Maybe<BreakpointPosition> GetEntryPosition(const BreakpointPosition& aPosition);

} // namespace js
} // namespace recordreplay
} // namespace mozilla

#endif // mozilla_recordreplay_JSControl_h
