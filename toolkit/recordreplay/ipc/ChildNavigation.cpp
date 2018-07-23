/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildInternal.h"

#include "ProcessRecordReplay.h"

namespace mozilla {
namespace recordreplay {
namespace navigation {

typedef js::BreakpointPosition BreakpointPosition;
typedef js::ExecutionPoint ExecutionPoint;

static void
BreakpointPositionToString(const BreakpointPosition& aPos, nsAutoCString& aStr)
{
  aStr.AppendPrintf("{ Kind: %s, Script: %d, Offset: %d, Frame: %d }",
                    aPos.KindString(), (int) aPos.mScript, (int) aPos.mOffset, (int) aPos.mFrameIndex);
}

static void
ExecutionPointToString(const ExecutionPoint& aPoint, nsAutoCString& aStr)
{
  aStr.AppendPrintf("{ Checkpoint %d", (int) aPoint.mCheckpoint);
  if (aPoint.HasPosition()) {
    aStr.AppendPrintf(" Progress %llu Position ", aPoint.mProgress);
    BreakpointPositionToString(aPoint.mPosition, aStr);
  }
  aStr.AppendPrintf(" }");
}

///////////////////////////////////////////////////////////////////////////////
// Navigation State
///////////////////////////////////////////////////////////////////////////////

// The navigation state of a recording/replaying process describes where the
// process currently is and what it is doing in order to respond to messages
// from the middleman process.
//
// At all times, the navigation state will be in exactly one of the following
// phases:
//
// - BreakpointPaused: The process is paused somewhere between two checkpoints.
// - CheckpointPaused: The process is paused at a normal checkpoint.
// - Forward: The process is running forward and scanning for breakpoint hits.
// - ReachBreakpoint: The process is running forward from a checkpoint to a
//     particular execution point before the next checkpoint.
// - FindLastHit: The process is running forward and keeping track of the last
//     point a breakpoint was hit within an execution region.
//
// This file manages data associated with each of these phases and the
// transitions that occur between them as the process executes or new
// messages are received from the middleman.

typedef AllocPolicy<MemoryKind::Navigation> UntrackedAllocPolicy;

// Abstract class for where we are at in the navigation state machine.
// Each subclass has a single instance contained in NavigationState (see below)
// and it and all its data are allocated using untracked memory that is not
// affected by restoring earlier checkpoints.
class NavigationPhase
{
  // All virtual members should only be accessed through NavigationState.
  friend class NavigationState;

private:
  MOZ_NORETURN void Unsupported(const char* aOperation) {
    nsAutoCString str;
    ToString(str);

    Print("Operation %s not supported: %s\n", aOperation, str.get());
    MOZ_CRASH("Unsupported navigation operation");
  }

public:
  virtual void ToString(nsAutoCString& aStr) = 0;

  // The process has just reached or rewound to a checkpoint.
  virtual void AfterCheckpoint(const CheckpointId& aCheckpoint) {
    Unsupported("AfterCheckpoint");
  }

  // Called when some position with an installed handler has been reached.
  virtual void PositionHit(const ExecutionPoint& aPoint) {
    Unsupported("PositionHit");
  }

  // Called after receiving a resume command from the middleman.
  virtual void Resume(bool aForward) {
    Unsupported("Resume");
  }

  // Called after the middleman tells us to rewind to a specific checkpoint.
  virtual void RestoreCheckpoint(size_t aCheckpoint) {
    Unsupported("RestoreCheckpoint");
  }

  // Process an incoming debugger request from the middleman.
  virtual void HandleDebuggerRequest(js::CharBuffer* aRequestBuffer) {
    Unsupported("HandleDebuggerRequest");
  }

  // Called when a debugger request wants to try an operation that may
  // trigger an unhandled divergence from the recording.
  virtual bool MaybeDivergeFromRecording() {
    Unsupported("MaybeDivergeFromRecording");
  }

  // Get the current execution point when recording.
  virtual ExecutionPoint GetRecordingEndpoint() {
    Unsupported("GetRecordingEndpoint");
  }

  // Called when execution reaches the endpoint of the recording.
  virtual void HitRecordingEndpoint(const ExecutionPoint& aPoint) {
    Unsupported("HitRecordingEndpoint");
  }
};

// Information about a debugger request sent by the middleman.
struct RequestInfo
{
  // JSON contents for the request and response.
  InfallibleVector<char16_t, 0, UntrackedAllocPolicy> mRequestBuffer;
  InfallibleVector<char16_t, 0, UntrackedAllocPolicy> mResponseBuffer;

  // Whether processing this request triggered an unhandled divergence.
  bool mUnhandledDivergence;

  RequestInfo() : mUnhandledDivergence(false) {}

  RequestInfo(const RequestInfo& o)
    : mUnhandledDivergence(o.mUnhandledDivergence)
  {
    mRequestBuffer.append(o.mRequestBuffer.begin(), o.mRequestBuffer.length());
    mResponseBuffer.append(o.mResponseBuffer.begin(), o.mResponseBuffer.length());
  }
};
typedef InfallibleVector<RequestInfo, 4, UntrackedAllocPolicy> UntrackedRequestVector;

typedef InfallibleVector<uint32_t> BreakpointVector;

// Phase when the replaying process is paused at a breakpoint.
class BreakpointPausedPhase final : public NavigationPhase
{
  // Where the pause is at.
  ExecutionPoint mPoint;

  // All debugger requests we have seen while paused here.
  UntrackedRequestVector mRequests;

  // Whether we had to restore a checkpoint to deal with an unhandled
  // recording divergence, and haven't finished rehandling old requests.
  bool mRecoveringFromDivergence;

  // Index of the request currently being processed. Normally this is the
  // last entry in |mRequests|, though may be earlier if we are recovering
  // from an unhandled divergence.
  size_t mRequestIndex;

  // Set when we were told to resume forward and need to clean up our state.
  bool mResumeForward;

public:
  void Enter(const ExecutionPoint& aPoint, const BreakpointVector& aBreakpoints);

  void ToString(nsAutoCString& aStr) override {
    aStr.AppendPrintf("BreakpointPaused RecoveringFromDivergence %d", mRecoveringFromDivergence);
  }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) override;
  void PositionHit(const ExecutionPoint& aPoint) override;
  void Resume(bool aForward) override;
  void RestoreCheckpoint(size_t aCheckpoint) override;
  void HandleDebuggerRequest(js::CharBuffer* aRequestBuffer) override;
  bool MaybeDivergeFromRecording() override;
  ExecutionPoint GetRecordingEndpoint() override;

  void RespondAfterRecoveringFromDivergence();
};

// Phase when the replaying process is paused at a normal checkpoint.
class CheckpointPausedPhase final : public NavigationPhase
{
  size_t mCheckpoint;
  bool mAtRecordingEndpoint;

public:
  void Enter(size_t aCheckpoint, bool aRewind, bool aAtRecordingEndpoint);

  void ToString(nsAutoCString& aStr) override {
    aStr.AppendPrintf("CheckpointPaused");
  }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) override;
  void PositionHit(const ExecutionPoint& aPoint) override;
  void Resume(bool aForward) override;
  void RestoreCheckpoint(size_t aCheckpoint) override;
  void HandleDebuggerRequest(js::CharBuffer* aRequestBuffer) override;
  ExecutionPoint GetRecordingEndpoint() override;
};

// Phase when execution is proceeding forwards in search of breakpoint hits.
class ForwardPhase final : public NavigationPhase
{
  // Some execution point in the recent past. There are no checkpoints or
  // breakpoint hits between this point and the current point of execution.
  ExecutionPoint mPoint;

public:
  void Enter(const ExecutionPoint& aPoint);

  void ToString(nsAutoCString& aStr) override {
    aStr.AppendPrintf("Forward");
  }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) override;
  void PositionHit(const ExecutionPoint& aPoint) override;
  void HitRecordingEndpoint(const ExecutionPoint& aPoint) override;
};

// Phase when the replaying process is running forward from a checkpoint to a
// breakpoint at a particular execution point.
class ReachBreakpointPhase final : public NavigationPhase
{
private:
  // Where to start running from.
  CheckpointId mStart;

  // The point we are running to.
  ExecutionPoint mPoint;

  // Point at which to decide whether to save a temporary checkpoint.
  Maybe<ExecutionPoint> mTemporaryCheckpoint;

  // Whether we have saved a temporary checkpoint at the specified point.
  bool mSavedTemporaryCheckpoint;

  // The time at which we started running forward from the initial
  // checkpoint, in microseconds.
  double mStartTime;

public:
  // Note: this always rewinds.
  void Enter(const CheckpointId& aStart,
             const ExecutionPoint& aPoint,
             const Maybe<ExecutionPoint>& aTemporaryCheckpoint);

  void ToString(nsAutoCString& aStr) override {
    aStr.AppendPrintf("ReachBreakpoint: ");
    ExecutionPointToString(mPoint, aStr);
    if (mTemporaryCheckpoint.isSome()) {
      aStr.AppendPrintf(" TemporaryCheckpoint: ");
      ExecutionPointToString(mTemporaryCheckpoint.ref(), aStr);
    }
  }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) override;
  void PositionHit(const ExecutionPoint& aPoint) override;
};

// Phase when the replaying process is searching forward from a checkpoint to
// find the last point a breakpoint is hit before reaching an execution point.
class FindLastHitPhase final : public NavigationPhase
{
  // Where we started searching from.
  CheckpointId mStart;

  // Endpoint of the search, nothing if the endpoint is the next checkpoint.
  Maybe<ExecutionPoint> mEnd;

  // Counter that increases as we run forward, for ordering hits.
  size_t mCounter;

  // All positions we are interested in hits for, including all breakpoint
  // positions (and possibly other positions).
  struct TrackedPosition {
    BreakpointPosition mPosition;

    // The last time this was hit so far, or invalid.
    ExecutionPoint mLastHit;

    // The value of the counter when the last hit occurred.
    size_t mLastHitCount;

    explicit TrackedPosition(const BreakpointPosition& aPosition)
      : mPosition(aPosition), mLastHitCount(0)
    {}
  };
  InfallibleVector<TrackedPosition, 4, UntrackedAllocPolicy> mTrackedPositions;

  const TrackedPosition& FindTrackedPosition(const BreakpointPosition& aPos);
  void OnRegionEnd();

public:
  // Note: this always rewinds.
  void Enter(const CheckpointId& aStart, const Maybe<ExecutionPoint>& aEnd);

  void ToString(nsAutoCString& aStr) override {
    aStr.AppendPrintf("FindLastHit");
  }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) override;
  void PositionHit(const ExecutionPoint& aPoint) override;
  void HitRecordingEndpoint(const ExecutionPoint& aPoint) override;
};

// Structure which manages state about the breakpoints in existence and about
// how the process is being navigated through. This is allocated in untracked
// memory and its contents will not change when restoring an earlier
// checkpoint.
class NavigationState
{
  // When replaying, the last known recording endpoint. There may be other,
  // later endpoints we haven't been informed about.
  ExecutionPoint mRecordingEndpoint;
  size_t mRecordingEndpointIndex;

  // The last checkpoint we ran forward or rewound to.
  CheckpointId mLastCheckpoint;

  // The locations of all temporary checkpoints we have saved. Temporary
  // checkpoints are taken immediately prior to reaching these points.
  InfallibleVector<ExecutionPoint, 0, UntrackedAllocPolicy> mTemporaryCheckpoints;

public:
  // All the currently installed breakpoints, indexed by their ID.
  InfallibleVector<BreakpointPosition, 4, UntrackedAllocPolicy> mBreakpoints;

  BreakpointPosition& GetBreakpoint(size_t id) {
    while (id >= mBreakpoints.length()) {
      mBreakpoints.emplaceBack();
    }
    return mBreakpoints[id];
  }

  CheckpointId LastCheckpoint() {
    return mLastCheckpoint;
  }

  // The current phase of the process.
  NavigationPhase* mPhase;

  void SetPhase(NavigationPhase* phase) {
    mPhase = phase;

    if (SpewEnabled()) {
      nsAutoCString str;
      mPhase->ToString(str);

      PrintSpew("SetNavigationPhase %s\n", str.get());
    }
  }

  BreakpointPausedPhase mBreakpointPausedPhase;
  CheckpointPausedPhase mCheckpointPausedPhase;
  ForwardPhase mForwardPhase;
  ReachBreakpointPhase mReachBreakpointPhase;
  FindLastHitPhase mFindLastHitPhase;

  // For testing, specify that temporary checkpoints should be taken regardless
  // of how much time has elapsed.
  bool mAlwaysSaveTemporaryCheckpoints;

  // Note: NavigationState is initially zeroed.
  NavigationState()
    : mPhase(&mForwardPhase)
  {
    if (IsReplaying()) {
      // The recording must include everything up to the first
      // checkpoint. After that point we will ask the record/replay
      // system to notify us about any further endpoints.
      mRecordingEndpoint = ExecutionPoint(CheckpointId::First);
    }
  }

  void AfterCheckpoint(const CheckpointId& aCheckpoint) {
    mLastCheckpoint = aCheckpoint;

    // Forget any temporary checkpoints we just rewound past, or made
    // obsolete by reaching the next normal checkpoint.
    while (mTemporaryCheckpoints.length() > aCheckpoint.mTemporary) {
      mTemporaryCheckpoints.popBack();
    }

    mPhase->AfterCheckpoint(aCheckpoint);

    // Make sure we don't run past the end of the recording.
    if (!aCheckpoint.mTemporary) {
      ExecutionPoint point(aCheckpoint.mNormal);
      CheckForRecordingEndpoint(point);
    }

    MOZ_RELEASE_ASSERT(IsRecording() ||
                       aCheckpoint.mNormal <= mRecordingEndpoint.mCheckpoint);
    if (aCheckpoint.mNormal == mRecordingEndpoint.mCheckpoint) {
      MOZ_RELEASE_ASSERT(mRecordingEndpoint.HasPosition());
      js::EnsurePositionHandler(mRecordingEndpoint.mPosition);
    }
  }

  void PositionHit(const ExecutionPoint& aPoint) {
    mPhase->PositionHit(aPoint);
    CheckForRecordingEndpoint(aPoint);
  }

  void Resume(bool aForward) {
    mPhase->Resume(aForward);
  }

  void RestoreCheckpoint(size_t aCheckpoint) {
    mPhase->RestoreCheckpoint(aCheckpoint);
  }

  void HandleDebuggerRequest(js::CharBuffer* aRequestBuffer) {
    mPhase->HandleDebuggerRequest(aRequestBuffer);
  }

  bool MaybeDivergeFromRecording() {
    return mPhase->MaybeDivergeFromRecording();
  }

  ExecutionPoint GetRecordingEndpoint() {
    return mPhase->GetRecordingEndpoint();
  }

  void SetRecordingEndpoint(size_t aIndex, const ExecutionPoint& aEndpoint) {
    // Ignore endpoints older than the last one we know about.
    if (aIndex <= mRecordingEndpointIndex) {
      return;
    }
    MOZ_RELEASE_ASSERT(mRecordingEndpoint.mCheckpoint <= aEndpoint.mCheckpoint);
    mRecordingEndpointIndex = aIndex;
    mRecordingEndpoint = aEndpoint;
    if (aEndpoint.HasPosition()) {
      js::EnsurePositionHandler(aEndpoint.mPosition);
    }
  }

  void CheckForRecordingEndpoint(const ExecutionPoint& aPoint) {
    while (aPoint == mRecordingEndpoint) {
      // The recording ended after the checkpoint, but maybe there is
      // another, later endpoint now. This may call back into
      // setRecordingEndpoint and notify us there is more recording data
      // available.
      if (!recordreplay::HitRecordingEndpoint()) {
        mPhase->HitRecordingEndpoint(mRecordingEndpoint);
      }
    }
  }

  size_t NumTemporaryCheckpoints() {
    return mTemporaryCheckpoints.length();
  }

  bool SaveTemporaryCheckpoint(const ExecutionPoint& aPoint) {
    MOZ_RELEASE_ASSERT(aPoint.mCheckpoint == mLastCheckpoint.mNormal);
    mTemporaryCheckpoints.append(aPoint);
    return NewCheckpoint(/* aTemporary = */ true);
  }

  ExecutionPoint LastTemporaryCheckpointLocation() {
    MOZ_RELEASE_ASSERT(!mTemporaryCheckpoints.empty());
    return mTemporaryCheckpoints.back();
  }

  CheckpointId LastTemporaryCheckpointId() {
    MOZ_RELEASE_ASSERT(!mTemporaryCheckpoints.empty());
    size_t normal = mTemporaryCheckpoints.back().mCheckpoint;
    size_t temporary = mTemporaryCheckpoints.length();
    return CheckpointId(normal, temporary);
  }
};

static NavigationState* gNavigation;

static void
GetAllBreakpointHits(const ExecutionPoint& aPoint, BreakpointVector& aHitBreakpoints)
{
  MOZ_RELEASE_ASSERT(aPoint.HasPosition());
  for (size_t id = 0; id < gNavigation->mBreakpoints.length(); id++) {
    const BreakpointPosition& breakpoint = gNavigation->mBreakpoints[id];
    if (breakpoint.IsValid() && breakpoint.Subsumes(aPoint.mPosition)) {
      aHitBreakpoints.append(id);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// BreakpointPaused Phase
///////////////////////////////////////////////////////////////////////////////

static bool
ThisProcessCanRewind()
{
  return HasSavedCheckpoint();
}

void
BreakpointPausedPhase::Enter(const ExecutionPoint& aPoint, const BreakpointVector& aBreakpoints)
{
  MOZ_RELEASE_ASSERT(aPoint.HasPosition());

  mPoint = aPoint;
  mRequests.clear();
  mRecoveringFromDivergence = false;
  mRequestIndex = 0;
  mResumeForward = false;

  gNavigation->SetPhase(this);

  if (ThisProcessCanRewind()) {
    // Immediately save a temporary checkpoint and update the point to be
    // in relation to this checkpoint. If we rewind due to a recording
    // divergence we will end up here.
    if (!gNavigation->SaveTemporaryCheckpoint(aPoint)) {
      // We just restored the checkpoint, and could be in any phase,
      // including this one.
      if (gNavigation->mPhase == this) {
        MOZ_RELEASE_ASSERT(!mRecoveringFromDivergence);
        // If we are transitioning to the forward phase, avoid hitting
        // breakpoints at this point.
        if (mResumeForward) {
          gNavigation->mForwardPhase.Enter(aPoint);
          return;
        }
        // Otherwise we restored after hitting an unhandled recording
        // divergence.
        mRecoveringFromDivergence = true;
        PauseMainThreadAndInvokeCallback([=]() {
            RespondAfterRecoveringFromDivergence();
          });
        Unreachable();
      }
      gNavigation->PositionHit(aPoint);
      return;
    }
  }

  bool endpoint = aBreakpoints.empty();
  child::HitBreakpoint(endpoint, aBreakpoints.begin(), aBreakpoints.length());

  // When rewinding is allowed we will rewind before resuming to erase side effects.
  MOZ_RELEASE_ASSERT(!ThisProcessCanRewind());
}

void
BreakpointPausedPhase::AfterCheckpoint(const CheckpointId& aCheckpoint)
{
  // We just saved or restored the temporary checkpoint before reaching the
  // breakpoint.
  MOZ_RELEASE_ASSERT(ThisProcessCanRewind());
  MOZ_RELEASE_ASSERT(aCheckpoint == gNavigation->LastTemporaryCheckpointId());
}

void
BreakpointPausedPhase::PositionHit(const ExecutionPoint& aPoint)
{
  // Ignore positions hit while paused (we're probably doing an eval).
}

void
BreakpointPausedPhase::Resume(bool aForward)
{
  MOZ_RELEASE_ASSERT(!mRecoveringFromDivergence);

  if (aForward) {
    // If we are paused at a breakpoint and can rewind, we may have
    // diverged from the recording. We have to clear any unwanted changes
    // induced by evals and so forth by restoring the temporary checkpoint
    // we saved before pausing here.
    if (ThisProcessCanRewind()) {
      mResumeForward = true;
      RestoreCheckpointAndResume(gNavigation->LastTemporaryCheckpointId());
      Unreachable();
    }

    js::ClearPausedState();

    // Run forward from the current execution point.
    gNavigation->mForwardPhase.Enter(mPoint);
    return;
  }

  // Search backwards in the execution space.
  CheckpointId start = gNavigation->LastTemporaryCheckpointId();
  start.mTemporary--;
  gNavigation->mFindLastHitPhase.Enter(start, Some(mPoint));
  Unreachable();
}

void
BreakpointPausedPhase::RestoreCheckpoint(size_t aCheckpoint)
{
  gNavigation->mCheckpointPausedPhase.Enter(aCheckpoint, /* aRewind = */ true,
                                            /* aAtRecordingEndpoint = */ false);
}

void
BreakpointPausedPhase::HandleDebuggerRequest(js::CharBuffer* aRequestBuffer)
{
  MOZ_RELEASE_ASSERT(!mRecoveringFromDivergence);

  mRequests.emplaceBack();
  RequestInfo& info = mRequests.back();
  mRequestIndex = mRequests.length() - 1;

  info.mRequestBuffer.append(aRequestBuffer->begin(), aRequestBuffer->length());

  js::CharBuffer responseBuffer;
  js::ProcessRequest(aRequestBuffer->begin(), aRequestBuffer->length(), &responseBuffer);

  delete aRequestBuffer;

  info.mResponseBuffer.append(responseBuffer.begin(), responseBuffer.length());
  child::RespondToRequest(responseBuffer);
}

void
BreakpointPausedPhase::RespondAfterRecoveringFromDivergence()
{
  MOZ_RELEASE_ASSERT(mRecoveringFromDivergence);
  MOZ_RELEASE_ASSERT(mRequests.length());

  // Remember that the last request has triggered an unhandled divergence.
  MOZ_RELEASE_ASSERT(!mRequests.back().mUnhandledDivergence);
  mRequests.back().mUnhandledDivergence = true;

  // Redo all existing requests.
  for (size_t i = 0; i < mRequests.length(); i++) {
    RequestInfo& info = mRequests[i];
    mRequestIndex = i;

    js::CharBuffer responseBuffer;
    js::ProcessRequest(info.mRequestBuffer.begin(), info.mRequestBuffer.length(),
                       &responseBuffer);

    if (i < mRequests.length() - 1) {
      // This is an old request, and we don't need to send another
      // response to it. Make sure the response we just generated matched
      // the earlier one we sent, though.
      MOZ_RELEASE_ASSERT(responseBuffer.length() == info.mResponseBuffer.length());
      MOZ_RELEASE_ASSERT(memcmp(responseBuffer.begin(), info.mResponseBuffer.begin(),
                                responseBuffer.length() * sizeof(char16_t)) == 0);
    } else {
      // This is the current request we need to respond to.
      MOZ_RELEASE_ASSERT(info.mResponseBuffer.empty());
      info.mResponseBuffer.append(responseBuffer.begin(), responseBuffer.length());
      child::RespondToRequest(responseBuffer);
    }
  }

  // We've finished recovering, and can now process new incoming requests.
  mRecoveringFromDivergence = false;
}

bool
BreakpointPausedPhase::MaybeDivergeFromRecording()
{
  if (!ThisProcessCanRewind()) {
    // Recording divergence is not supported if we can't rewind. We can't
    // simply allow execution to proceed from here as if we were not
    // diverged, since any events or other activity that show up afterwards
    // will not be reflected in the recording.
    return false;
  }
  if (mRequests[mRequestIndex].mUnhandledDivergence) {
    return false;
  }
  DivergeFromRecording();
  return true;
}

ExecutionPoint
BreakpointPausedPhase::GetRecordingEndpoint()
{
  MOZ_RELEASE_ASSERT(IsRecording());
  return mPoint;
}

///////////////////////////////////////////////////////////////////////////////
// CheckpointPausedPhase
///////////////////////////////////////////////////////////////////////////////

void
CheckpointPausedPhase::Enter(size_t aCheckpoint, bool aRewind, bool aAtRecordingEndpoint)
{
  mCheckpoint = aCheckpoint;
  mAtRecordingEndpoint = aAtRecordingEndpoint;

  gNavigation->SetPhase(this);

  if (aRewind) {
    RestoreCheckpointAndResume(CheckpointId(mCheckpoint));
    Unreachable();
  }

  AfterCheckpoint(CheckpointId(mCheckpoint));
}

void
CheckpointPausedPhase::AfterCheckpoint(const CheckpointId& aCheckpoint)
{
  MOZ_RELEASE_ASSERT(aCheckpoint == CheckpointId(mCheckpoint));
  child::HitCheckpoint(mCheckpoint, mAtRecordingEndpoint);
}

void
CheckpointPausedPhase::PositionHit(const ExecutionPoint& aPoint)
{
  // Ignore positions hit while paused (we're probably doing an eval).
}

void
CheckpointPausedPhase::Resume(bool aForward)
{
  // We can't rewind past the beginning of the replay.
  MOZ_RELEASE_ASSERT(aForward || mCheckpoint != CheckpointId::First);

  if (aForward) {
    // Run forward from the current execution point.
    js::ClearPausedState();
    ExecutionPoint search(mCheckpoint);
    gNavigation->mForwardPhase.Enter(search);
  } else {
    CheckpointId start(mCheckpoint - 1);
    gNavigation->mFindLastHitPhase.Enter(start, Nothing());
    Unreachable();
  }
}

void
CheckpointPausedPhase::RestoreCheckpoint(size_t aCheckpoint)
{
  Enter(aCheckpoint, /* aRewind = */ true, /* aAtRecordingEndpoint = */ false);
}

void
CheckpointPausedPhase::HandleDebuggerRequest(js::CharBuffer* aRequestBuffer)
{
  js::CharBuffer responseBuffer;
  js::ProcessRequest(aRequestBuffer->begin(), aRequestBuffer->length(), &responseBuffer);

  delete aRequestBuffer;

  child::RespondToRequest(responseBuffer);
}

ExecutionPoint
CheckpointPausedPhase::GetRecordingEndpoint()
{
  return ExecutionPoint(mCheckpoint);
}

///////////////////////////////////////////////////////////////////////////////
// ForwardPhase
///////////////////////////////////////////////////////////////////////////////

void
ForwardPhase::Enter(const ExecutionPoint& aPoint)
{
  mPoint = aPoint;

  gNavigation->SetPhase(this);

  // Install handlers for all breakpoints.
  for (const BreakpointPosition& breakpoint : gNavigation->mBreakpoints) {
    if (breakpoint.IsValid()) {
      js::EnsurePositionHandler(breakpoint);
    }
  }

  ResumeExecution();
}

void
ForwardPhase::AfterCheckpoint(const CheckpointId& aCheckpoint)
{
  MOZ_RELEASE_ASSERT(!aCheckpoint.mTemporary &&
                     aCheckpoint.mNormal == mPoint.mCheckpoint + 1);
  gNavigation->mCheckpointPausedPhase.Enter(aCheckpoint.mNormal, /* aRewind = */ false,
                                            /* aAtRecordingEndpoint = */ false);
}

void
ForwardPhase::PositionHit(const ExecutionPoint& aPoint)
{
  BreakpointVector hitBreakpoints;
  GetAllBreakpointHits(aPoint, hitBreakpoints);

  if (!hitBreakpoints.empty()) {
    gNavigation->mBreakpointPausedPhase.Enter(aPoint, hitBreakpoints);
  }
}

void
ForwardPhase::HitRecordingEndpoint(const ExecutionPoint& aPoint)
{
  if (aPoint.HasPosition()) {
    BreakpointVector emptyBreakpoints;
    gNavigation->mBreakpointPausedPhase.Enter(aPoint, emptyBreakpoints);
  } else {
    gNavigation->mCheckpointPausedPhase.Enter(aPoint.mCheckpoint, /* aRewind = */ false,
                                              /* aAtRecordingEndpoint = */ true);
  }
}

///////////////////////////////////////////////////////////////////////////////
// ReachBreakpointPhase
///////////////////////////////////////////////////////////////////////////////

void
ReachBreakpointPhase::Enter(const CheckpointId& aStart,
                            const ExecutionPoint& aPoint,
                            const Maybe<ExecutionPoint>& aTemporaryCheckpoint)
{
  MOZ_RELEASE_ASSERT(aPoint.HasPosition());
  MOZ_RELEASE_ASSERT(aTemporaryCheckpoint.isNothing() ||
                     (aTemporaryCheckpoint.ref().HasPosition() &&
                      aTemporaryCheckpoint.ref() != aPoint));
  mStart = aStart;
  mPoint = aPoint;
  mTemporaryCheckpoint = aTemporaryCheckpoint;
  mSavedTemporaryCheckpoint = false;

  gNavigation->SetPhase(this);

  RestoreCheckpointAndResume(aStart);
  Unreachable();
}

void
ReachBreakpointPhase::AfterCheckpoint(const CheckpointId& aCheckpoint)
{
  if (aCheckpoint == mStart && mTemporaryCheckpoint.isSome()) {
    js::EnsurePositionHandler(mTemporaryCheckpoint.ref().mPosition);

    // Remember the time we started running forwards from the initial checkpoint.
    mStartTime = CurrentTime();
  } else {
    MOZ_RELEASE_ASSERT((aCheckpoint == mStart && mTemporaryCheckpoint.isNothing()) ||
                       (aCheckpoint == mStart.NextCheckpoint(/* aTemporary = */ true) &&
                        mSavedTemporaryCheckpoint));
  }

  js::EnsurePositionHandler(mPoint.mPosition);
}

// The number of milliseconds to elapse during a ReachBreakpoint search before
// we will save a temporary checkpoint.
static const double kTemporaryCheckpointThresholdMs = 10;

void
AlwaysSaveTemporaryCheckpoints()
{
  gNavigation->mAlwaysSaveTemporaryCheckpoints = true;
}

void
ReachBreakpointPhase::PositionHit(const ExecutionPoint& aPoint)
{
  if (mTemporaryCheckpoint.isSome() && mTemporaryCheckpoint.ref() == aPoint) {
    // We've reached the point at which we have the option of saving a
    // temporary checkpoint.
    double elapsedMs = (CurrentTime() - mStartTime) / 1000.0;
    if (elapsedMs >= kTemporaryCheckpointThresholdMs ||
        gNavigation->mAlwaysSaveTemporaryCheckpoints)
    {
      MOZ_RELEASE_ASSERT(!mSavedTemporaryCheckpoint);
      mSavedTemporaryCheckpoint = true;

      if (!gNavigation->SaveTemporaryCheckpoint(aPoint)) {
        // We just restored the checkpoint, and could be in any phase.
        gNavigation->PositionHit(aPoint);
        return;
      }
    }
  }

  if (mPoint == aPoint) {
    BreakpointVector hitBreakpoints;
    GetAllBreakpointHits(aPoint, hitBreakpoints);
    MOZ_RELEASE_ASSERT(!hitBreakpoints.empty());

    gNavigation->mBreakpointPausedPhase.Enter(aPoint, hitBreakpoints);
  }
}

///////////////////////////////////////////////////////////////////////////////
// FindLastHitPhase
///////////////////////////////////////////////////////////////////////////////

void
FindLastHitPhase::Enter(const CheckpointId& aStart, const Maybe<ExecutionPoint>& aEnd)
{
  MOZ_RELEASE_ASSERT(aEnd.isNothing() || aEnd.ref().HasPosition());

  mStart = aStart;
  mEnd = aEnd;
  mCounter = 0;
  mTrackedPositions.clear();

  gNavigation->SetPhase(this);

  // All breakpoints are tracked positions.
  for (const BreakpointPosition& breakpoint : gNavigation->mBreakpoints) {
    if (breakpoint.IsValid()) {
      mTrackedPositions.emplaceBack(breakpoint);
    }
  }

  // Entry points to scripts containing breakpoints are tracked positions.
  for (const BreakpointPosition& breakpoint : gNavigation->mBreakpoints) {
    Maybe<BreakpointPosition> entry = GetEntryPosition(breakpoint);
    if (entry.isSome()) {
      mTrackedPositions.emplaceBack(entry.ref());
    }
  }

  RestoreCheckpointAndResume(mStart);
  Unreachable();
}

void
FindLastHitPhase::AfterCheckpoint(const CheckpointId& aCheckpoint)
{
  if (aCheckpoint == mStart.NextCheckpoint(/* aTemporary = */ false)) {
    // We reached the next checkpoint, and are done searching.
    MOZ_RELEASE_ASSERT(mEnd.isNothing());
    OnRegionEnd();
    Unreachable();
  }

  // We are at the start of the search.
  MOZ_RELEASE_ASSERT(aCheckpoint == mStart);

  for (const TrackedPosition& tracked : mTrackedPositions) {
    js::EnsurePositionHandler(tracked.mPosition);
  }

  if (mEnd.isSome()) {
    js::EnsurePositionHandler(mEnd.ref().mPosition);
  }
}

void
FindLastHitPhase::PositionHit(const ExecutionPoint& aPoint)
{
  if (mEnd.isSome() && mEnd.ref() == aPoint) {
    OnRegionEnd();
    Unreachable();
  }

  ++mCounter;

  for (TrackedPosition& tracked : mTrackedPositions) {
    if (tracked.mPosition.Subsumes(aPoint.mPosition)) {
      tracked.mLastHit = aPoint;
      tracked.mLastHitCount = mCounter;
      break;
    }
  }
}

void
FindLastHitPhase::HitRecordingEndpoint(const ExecutionPoint& aPoint)
{
  OnRegionEnd();
  Unreachable();
}

const FindLastHitPhase::TrackedPosition&
FindLastHitPhase::FindTrackedPosition(const BreakpointPosition& aPos)
{
  for (const TrackedPosition& tracked : mTrackedPositions) {
    if (tracked.mPosition == aPos) {
      return tracked;
    }
  }
  MOZ_CRASH("Could not find tracked position");
}

void
FindLastHitPhase::OnRegionEnd()
{
  // Find the point of the last hit which coincides with a breakpoint.
  Maybe<TrackedPosition> lastBreakpoint;
  for (const BreakpointPosition& breakpoint : gNavigation->mBreakpoints) {
    if (!breakpoint.IsValid()) {
      continue;
    }
    const TrackedPosition& tracked = FindTrackedPosition(breakpoint);
    if (tracked.mLastHit.HasPosition() &&
        (lastBreakpoint.isNothing() ||
         lastBreakpoint.ref().mLastHitCount < tracked.mLastHitCount))
    {
      lastBreakpoint = Some(tracked);
    }
  }

  if (lastBreakpoint.isNothing()) {
    // No breakpoints were encountered in the search space.
    if (mStart.mTemporary) {
      // We started searching forwards from a temporary checkpoint.
      // Continue searching backwards without notifying the middleman.
      CheckpointId start = mStart;
      start.mTemporary--;
      ExecutionPoint end = gNavigation->LastTemporaryCheckpointLocation();
      gNavigation->mFindLastHitPhase.Enter(start, Some(end));
      Unreachable();
    } else {
      // Rewind to the last normal checkpoint and pause.
      gNavigation->mCheckpointPausedPhase.Enter(mStart.mNormal, /* aRewind = */ true,
                                                /* aAtRecordingEndpoint = */ false);
      Unreachable();
    }
  }

  // When running backwards, we don't want to place temporary checkpoints at
  // the breakpoint where we are going to stop at. If the user continues
  // rewinding then we will just have to discard the checkpoint and waste the
  // work we did in saving it.
  //
  // Instead, try to place a temporary checkpoint at the last time the
  // breakpoint's script was entered. This optimizes for the case of stepping
  // around within a frame.
  Maybe<BreakpointPosition> baseEntry = GetEntryPosition(lastBreakpoint.ref().mPosition);
  if (baseEntry.isSome()) {
    const TrackedPosition& tracked = FindTrackedPosition(baseEntry.ref());
    if (tracked.mLastHit.HasPosition() &&
        tracked.mLastHitCount < lastBreakpoint.ref().mLastHitCount)
    {
      gNavigation->mReachBreakpointPhase.Enter(mStart, lastBreakpoint.ref().mLastHit,
                                               Some(tracked.mLastHit));
      Unreachable();
    }
  }

  // There was no suitable place for a temporary checkpoint, so rewind to the
  // last checkpoint and play forward to the last breakpoint hit we found.
  gNavigation->mReachBreakpointPhase.Enter(mStart, lastBreakpoint.ref().mLastHit, Nothing());
  Unreachable();
}

///////////////////////////////////////////////////////////////////////////////
// Hooks
///////////////////////////////////////////////////////////////////////////////

bool
IsInitialized()
{
  return !!gNavigation;
}

void
BeforeCheckpoint()
{
  if (!IsInitialized()) {
    void* navigationMem =
      AllocateMemory(sizeof(NavigationState), MemoryKind::Navigation);
    gNavigation = new (navigationMem) NavigationState();

    js::SetupDevtoolsSandbox();
  }

  AutoDisallowThreadEvents disallow;

  // Reset the debugger to a consistent state before each checkpoint.
  js::ClearPositionHandlers();
}

void
AfterCheckpoint(const CheckpointId& aCheckpoint)
{
  AutoDisallowThreadEvents disallow;

  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying());
  gNavigation->AfterCheckpoint(aCheckpoint);
}

void
DebuggerRequest(js::CharBuffer* aRequestBuffer)
{
  gNavigation->HandleDebuggerRequest(aRequestBuffer);
}

void
SetBreakpoint(size_t aId, const BreakpointPosition& aPosition)
{
  gNavigation->GetBreakpoint(aId) = aPosition;
}

void
Resume(bool aForward)
{
  // For the primordial resume sent at startup, the navigation state will not
  // have been initialized yet.
  if (!gNavigation) {
    ResumeExecution();
    return;
  }
  gNavigation->Resume(aForward);
}

void
RestoreCheckpoint(size_t aId)
{
  gNavigation->RestoreCheckpoint(aId);
}

ExecutionPoint
GetRecordingEndpoint()
{
  MOZ_RELEASE_ASSERT(IsRecording());
  return gNavigation->GetRecordingEndpoint();
}

void
SetRecordingEndpoint(size_t aIndex, const ExecutionPoint& aEndpoint)
{
  MOZ_RELEASE_ASSERT(IsReplaying());
  gNavigation->SetRecordingEndpoint(aIndex, aEndpoint);
}

static ProgressCounter gProgressCounter;

extern "C" {

MOZ_EXPORT ProgressCounter*
RecordReplayInterface_ExecutionProgressCounter()
{
  return &gProgressCounter;
}

} // extern "C"

static ExecutionPoint
NewExecutionPoint(const BreakpointPosition& aPosition)
{
  return ExecutionPoint(gNavigation->LastCheckpoint().mNormal,
                        gProgressCounter, aPosition);
}

void
PositionHit(const BreakpointPosition& position)
{
  AutoDisallowThreadEvents disallow;
  gNavigation->PositionHit(NewExecutionPoint(position));
}

bool
MaybeDivergeFromRecording()
{
  return gNavigation->MaybeDivergeFromRecording();
}

} // namespace navigation
} // namespace recordreplay
} // namespace mozilla
