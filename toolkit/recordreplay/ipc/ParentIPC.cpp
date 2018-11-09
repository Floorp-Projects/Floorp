/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file has the logic which the middleman process uses to communicate with
// the parent process and with the replayed process.

#include "ParentInternal.h"

#include "base/task.h"
#include "ipc/Channel.h"
#include "js/Proxy.h"
#include "mozilla/dom/ContentProcessMessageManager.h"
#include "InfallibleVector.h"
#include "JSControl.h"
#include "Monitor.h"
#include "ProcessRecordReplay.h"
#include "ProcessRedirect.h"

#include <algorithm>

using std::min;

namespace mozilla {
namespace recordreplay {
namespace parent {

///////////////////////////////////////////////////////////////////////////////
// UI Process State
///////////////////////////////////////////////////////////////////////////////

const char* gSaveAllRecordingsDirectory = nullptr;

void
InitializeUIProcess(int aArgc, char** aArgv)
{
  for (int i = 0; i < aArgc; i++) {
    if (!strcmp(aArgv[i], "--save-recordings") && i + 1 < aArgc) {
      gSaveAllRecordingsDirectory = strdup(aArgv[i + 1]);
    }
  }
}

const char*
SaveAllRecordingsDirectory()
{
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return gSaveAllRecordingsDirectory;
}

///////////////////////////////////////////////////////////////////////////////
// Child Roles
///////////////////////////////////////////////////////////////////////////////

static const double FlushSeconds = .5;
static const double MajorCheckpointSeconds = 2;

// This section describes the strategy used for managing child processes. When
// recording, there is a single recording process and two replaying processes.
// When replaying, there are two replaying processes. The main advantage of
// using two replaying processes is to provide a smooth experience when
// rewinding.
//
// At any time there is one active child: the process which the user is
// interacting with. This may be any of the two or three children in existence,
// depending on the user's behavior. The other processes do not interact with
// the user: inactive recording processes are inert, and sit idle until
// recording is ready to resume, while inactive replaying processes are on
// standby, staying close to the active process in the recording's execution
// space and saving checkpoints in case the user starts rewinding.
//
// Below are some scenarios showing the state we attempt to keep the children
// in, and ways in which the active process switches from one to another.
// The execution diagrams show the position of each process, with '*' and '-'
// indicating checkpoints the process reached and, respectively, whether
// the checkpoint was saved or not.
//
// When the recording process is actively recording, flushes are issued to it
// every FlushSeconds to keep the recording reasonably current and allow the
// replaying processes to stay behind but close to the position of the
// recording process. Additionally, one replaying process saves a checkpoint
// every MajorCheckpointSeconds with the process saving the checkpoint
// alternating back and forth so that individual processes save checkpoints
// every MajorCheckpointSeconds*2. These are the major checkpoints for each
// replaying process.
//
// Active  Recording:    -----------------------
// Standby Replaying #1: *---------*---------*
// Standby Replaying #2: -----*---------*-----
//
// When the recording process is explicitly paused (via the debugger UI) at a
// checkpoint or breakpoint, it is flushed and the replaying processes will
// navigate around the recording to ensure all checkpoints going back at least
// MajorCheckpointSeconds have been saved. These are the intermediate
// checkpoints. No replaying process needs to rewind past its last major
// checkpoint, and a given intermediate checkpoint will only ever be saved by
// the replaying process with the most recent major checkpoint.
//
// Active  Recording:    -----------------------
// Standby Replaying #1: *---------*---------***
// Standby Replaying #2: -----*---------*****
//
// If the user starts rewinding, the replaying process with the most recent
// major checkpoint (and which has been saving the most recent intermediate
// checkpoints) becomes the active child.
//
// Inert   Recording:    -----------------------
// Active  Replaying #1: *---------*---------**
// Standby Replaying #2: -----*---------*****
//
// As the user continues rewinding, the replaying process stays active until it
// goes past its most recent major checkpoint. At that time the other replaying
// process (which has been saving checkpoints prior to that point) becomes the
// active child and allows continuous rewinding. The first replaying process
// rewinds to its last major checkpoint and begins saving older intermediate
// checkpoints, attempting to maintain the invariant that we have saved (or are
// saving) all checkpoints going back MajorCheckpointSeconds.
//
// Inert   Recording:    -----------------------
// Standby Replaying #1: *---------*****
// Active  Replaying #2: -----*---------**
//
// Rewinding continues in this manner, alternating back and forth between the
// replaying processes as the user continues going back in time.
//
// Inert   Recording:    -----------------------
// Active  Replaying #1: *---------**
// Standby Replaying #2: -----*****
//
// If the user starts navigating forward, the replaying processes both run
// forward and save checkpoints at the same major checkpoints as earlier.
// Note that this is how all forward execution works when there is no recording
// process (i.e. we started from a saved recording).
//
// Inert   Recording:    -----------------------
// Active  Replaying #1: *---------**------
// Standby Replaying #2: -----*****-----*--
//
// If the user pauses at a checkpoint or breakpoint in the replay, we again
// want to fill in all the checkpoints going back MajorCheckpointSeconds to
// allow smooth rewinding. This cannot be done simultaneously -- as it was when
// the recording process was active -- since we need to keep one of the
// replaying processes at an up to date point and be the active one. This falls
// on the one whose most recent major checkpoint is oldest, as the other is
// responsible for saving the most recent intermediate checkpoints.
//
// Inert   Recording:    -----------------------
// Active  Replaying #1: *---------**------
// Standby Replaying #2: -----*****-----***
//
// After the recent intermediate checkpoints have been saved the process which
// took them can become active so the older intermediate checkpoints can be
// saved.
//
// Inert   Recording:    -----------------------
// Standby Replaying #1: *---------*****
// Active  Replaying #2: -----*****-----***
//
// Finally, if the replay plays forward to the end of the recording (the point
// where the recording process is situated), the recording process takes over
// again as the active child and the user can resume interacting with a live
// process.
//
// Active  Recording:    ----------------------------------------
// Standby Replaying #1: *---------*****-----*---------*-------
// Standby Replaying #2: -----*****-----***-------*---------*--

// The current active child.
static ChildProcessInfo* gActiveChild;

// The single recording child process, or null.
static ChildProcessInfo* gRecordingChild;

// The two replaying child processes, null if they haven't been spawned yet.
// When rewinding is disabled, there is only a single replaying child, and zero
// replaying children if there is a recording child.
static ChildProcessInfo* gFirstReplayingChild;
static ChildProcessInfo* gSecondReplayingChild;

void
Shutdown()
{
  delete gRecordingChild;
  delete gFirstReplayingChild;
  delete gSecondReplayingChild;
  _exit(0);
}

bool
IsMiddlemanWithRecordingChild()
{
  return IsMiddleman() && gRecordingChild;
}

static ChildProcessInfo*
OtherReplayingChild(ChildProcessInfo* aChild)
{
  MOZ_RELEASE_ASSERT(!aChild->IsRecording() && gFirstReplayingChild && gSecondReplayingChild);
  return aChild == gFirstReplayingChild ? gSecondReplayingChild : gFirstReplayingChild;
}

static void
ForEachReplayingChild(const std::function<void(ChildProcessInfo*)>& aCallback)
{
  if (gFirstReplayingChild) {
    aCallback(gFirstReplayingChild);
  }
  if (gSecondReplayingChild) {
    aCallback(gSecondReplayingChild);
  }
}

static void
PokeChildren()
{
  ForEachReplayingChild([=](ChildProcessInfo* aChild) {
      if (aChild->IsPaused()) {
        aChild->Role()->Poke();
      }
    });
}

static void RecvHitCheckpoint(const HitCheckpointMessage& aMsg);
static void RecvHitBreakpoint(const HitBreakpointMessage& aMsg);
static void RecvDebuggerResponse(const DebuggerResponseMessage& aMsg);
static void RecvRecordingFlushed();
static void RecvAlwaysMarkMajorCheckpoints();
static void RecvMiddlemanCallRequest(const MiddlemanCallRequestMessage& aMsg);

// The role taken by the active child.
class ChildRoleActive final : public ChildRole
{
public:
  ChildRoleActive()
    : ChildRole(Active)
  {}

  void Initialize() override {
    gActiveChild = mProcess;

    mProcess->SendMessage(SetIsActiveMessage(true));

    // Always run forward from the primordial checkpoint. Otherwise, the
    // debugger hooks below determine how the active child changes.
    if (mProcess->LastCheckpoint() == CheckpointId::Invalid) {
      mProcess->SendMessage(ResumeMessage(/* aForward = */ true));
    }
  }

  void OnIncomingMessage(const Message& aMsg) override {
    switch (aMsg.mType) {
    case MessageType::Paint:
      MaybeUpdateGraphicsAtPaint((const PaintMessage&) aMsg);
      break;
    case MessageType::HitCheckpoint:
      RecvHitCheckpoint((const HitCheckpointMessage&) aMsg);
      break;
    case MessageType::HitBreakpoint:
      RecvHitBreakpoint((const HitBreakpointMessage&) aMsg);
      break;
    case MessageType::DebuggerResponse:
      RecvDebuggerResponse((const DebuggerResponseMessage&) aMsg);
      break;
    case MessageType::RecordingFlushed:
      RecvRecordingFlushed();
      break;
    case MessageType::AlwaysMarkMajorCheckpoints:
      RecvAlwaysMarkMajorCheckpoints();
      break;
    case MessageType::MiddlemanCallRequest:
      RecvMiddlemanCallRequest((const MiddlemanCallRequestMessage&) aMsg);
      break;
    case MessageType::ResetMiddlemanCalls:
      ResetMiddlemanCalls();
      break;
    default:
      MOZ_CRASH("Unexpected message");
    }
  }
};

bool
ActiveChildIsRecording()
{
  return gActiveChild && gActiveChild->IsRecording();
}

ChildProcessInfo*
ActiveRecordingChild()
{
  MOZ_RELEASE_ASSERT(ActiveChildIsRecording());
  return gActiveChild;
}

// The last checkpoint included in the recording.
static size_t gLastRecordingCheckpoint;

// The role taken by replaying children trying to stay close to the active
// child and save either major or intermediate checkpoints, depending on
// whether the active child is paused or rewinding.
class ChildRoleStandby final : public ChildRole
{
public:
  ChildRoleStandby()
    : ChildRole(Standby)
  {}

  void Initialize() override {
    MOZ_RELEASE_ASSERT(mProcess->IsPausedAtCheckpoint());
    mProcess->SendMessage(SetIsActiveMessage(false));
    Poke();
  }

  void OnIncomingMessage(const Message& aMsg) override {
    MOZ_RELEASE_ASSERT(aMsg.mType == MessageType::HitCheckpoint);
    Poke();
  }

  void Poke() override;
};

// The role taken by a recording child while another child is active.
class ChildRoleInert final : public ChildRole
{
public:
  ChildRoleInert()
    : ChildRole(Inert)
  {}

  void Initialize() override {
    MOZ_RELEASE_ASSERT(mProcess->IsRecording() && mProcess->IsPaused());
  }

  void OnIncomingMessage(const Message& aMsg) override {
    MOZ_CRASH("Unexpected message from inert recording child");
  }
};

// Get the last major checkpoint for a process at or before aId, or CheckpointId::Invalid.
static size_t
LastMajorCheckpointPreceding(ChildProcessInfo* aChild, size_t aId)
{
  size_t last = CheckpointId::Invalid;
  for (size_t majorCheckpoint : aChild->MajorCheckpoints()) {
    if (majorCheckpoint > aId) {
      break;
    }
    last = majorCheckpoint;
  }
  return last;
}

// Get the replaying process responsible for saving aId when rewinding: the one
// with the most recent major checkpoint preceding aId.
static ChildProcessInfo*
ReplayingChildResponsibleForSavingCheckpoint(size_t aId)
{
  MOZ_RELEASE_ASSERT(CanRewind() && gFirstReplayingChild && gSecondReplayingChild);
  size_t firstMajor = LastMajorCheckpointPreceding(gFirstReplayingChild, aId);
  size_t secondMajor = LastMajorCheckpointPreceding(gSecondReplayingChild, aId);
  return (firstMajor < secondMajor) ? gSecondReplayingChild : gFirstReplayingChild;
}

// Returns a checkpoint if the active child is explicitly paused somewhere,
// has started rewinding after being explicitly paused, or is attempting to
// warp to an execution point. The checkpoint returned is the latest one which
// should be saved, and standby roles must save all intermediate checkpoints
// they are responsible for, in the range from their most recent major
// checkpoint up to the returned checkpoint.
static Maybe<size_t> ActiveChildTargetCheckpoint();

// Ensure that a child will save aCheckpoint iff it is a major checkpoint.
static void
EnsureMajorCheckpointSaved(ChildProcessInfo* aChild, size_t aCheckpoint)
{
  // The first checkpoint is always saved, even if not marked as major.
  bool childShouldSave = aChild->IsMajorCheckpoint(aCheckpoint) || aCheckpoint == CheckpointId::First;
  bool childToldToSave = aChild->ShouldSaveCheckpoint(aCheckpoint);

  if (childShouldSave != childToldToSave) {
    aChild->SendMessage(SetSaveCheckpointMessage(aCheckpoint, childShouldSave));
  }
}

void
ChildRoleStandby::Poke()
{
  MOZ_RELEASE_ASSERT(mProcess->IsPausedAtCheckpoint());

  // Stay paused if we need to while the recording is flushed.
  if (mProcess->PauseNeeded()) {
    return;
  }

  // Check if we need to save a range of intermediate checkpoints.
  do {
    // Intermediate checkpoints are only saved when the active child is paused
    // or rewinding.
    Maybe<size_t> targetCheckpoint = ActiveChildTargetCheckpoint();
    if (targetCheckpoint.isNothing()) {
      break;
    }

    // The startpoint of the range is the most recent major checkpoint prior to
    // the target.
    size_t lastMajorCheckpoint = LastMajorCheckpointPreceding(mProcess, targetCheckpoint.ref());

    // If there is no major checkpoint prior to the target, just idle.
    if (lastMajorCheckpoint == CheckpointId::Invalid) {
      return;
    }

    // If we haven't reached the last major checkpoint, we need to run forward
    // without saving intermediate checkpoints.
    if (mProcess->LastCheckpoint() < lastMajorCheckpoint) {
      EnsureMajorCheckpointSaved(mProcess, mProcess->LastCheckpoint() + 1);
      mProcess->SendMessage(ResumeMessage(/* aForward = */ true));
      return;
    }

    // The endpoint of the range is the checkpoint prior to either the active
    // child's current position, or the other replaying child's most recent
    // major checkpoint.
    size_t otherMajorCheckpoint =
      LastMajorCheckpointPreceding(OtherReplayingChild(mProcess), targetCheckpoint.ref());
    if (otherMajorCheckpoint > lastMajorCheckpoint) {
      MOZ_RELEASE_ASSERT(otherMajorCheckpoint <= targetCheckpoint.ref());
      targetCheckpoint.ref() = otherMajorCheckpoint - 1;
    }

    // Find the first checkpoint in the fill range which we have not saved.
    Maybe<size_t> missingCheckpoint;
    for (size_t i = lastMajorCheckpoint; i <= targetCheckpoint.ref(); i++) {
      if (!mProcess->HasSavedCheckpoint(i)) {
        missingCheckpoint.emplace(i);
        break;
      }
    }

    // If we have already saved everything we need to, we can idle.
    if (!missingCheckpoint.isSome()) {
      return;
    }

    // We must have saved the checkpoint prior to the missing one and can
    // restore it. missingCheckpoint cannot be lastMajorCheckpoint, because we
    // always save major checkpoints, and the loop above checked that all
    // prior checkpoints going back to lastMajorCheckpoint have been saved.
    size_t restoreTarget = missingCheckpoint.ref() - 1;
    MOZ_RELEASE_ASSERT(mProcess->HasSavedCheckpoint(restoreTarget));

    // If we need to rewind to the restore target, do so.
    if (mProcess->LastCheckpoint() != restoreTarget) {
      mProcess->SendMessage(RestoreCheckpointMessage(restoreTarget));
      return;
    }

    // Make sure the process will save the next checkpoint.
    if (!mProcess->ShouldSaveCheckpoint(missingCheckpoint.ref())) {
      mProcess->SendMessage(SetSaveCheckpointMessage(missingCheckpoint.ref(), true));
    }

    // Run forward to the next checkpoint.
    mProcess->SendMessage(ResumeMessage(/* aForward = */ true));
    return;
  } while (false);

  // Run forward until we reach either the active child's position, or the last
  // checkpoint included in the on-disk recording. Only save major checkpoints.
  if ((mProcess->LastCheckpoint() < gActiveChild->LastCheckpoint()) &&
      (!gRecordingChild || mProcess->LastCheckpoint() < gLastRecordingCheckpoint)) {
    EnsureMajorCheckpointSaved(mProcess, mProcess->LastCheckpoint() + 1);
    mProcess->SendMessage(ResumeMessage(/* aForward = */ true));
  }
}

///////////////////////////////////////////////////////////////////////////////
// Major Checkpoints
///////////////////////////////////////////////////////////////////////////////

// For each checkpoint N, this vector keeps track of the time intervals taken
// for the active child (excluding idle time) to run from N to N+1.
static StaticInfallibleVector<TimeDuration> gCheckpointTimes;

// How much time has elapsed (per gCheckpointTimes) since the last flush or
// major checkpoint was noted.
static TimeDuration gTimeSinceLastFlush;
static TimeDuration gTimeSinceLastMajorCheckpoint;

// The replaying process that was given the last major checkpoint.
static ChildProcessInfo* gLastAssignedMajorCheckpoint;

// For testing, mark new major checkpoints as frequently as possible.
static bool gAlwaysMarkMajorCheckpoints;

static void
RecvAlwaysMarkMajorCheckpoints()
{
  gAlwaysMarkMajorCheckpoints = true;
}

static void
AssignMajorCheckpoint(ChildProcessInfo* aChild, size_t aId)
{
  PrintSpew("AssignMajorCheckpoint: Process %d Checkpoint %d\n",
            (int) aChild->GetId(), (int) aId);
  aChild->AddMajorCheckpoint(aId);
  gLastAssignedMajorCheckpoint = aChild;
}

static bool MaybeFlushRecording();

static void
UpdateCheckpointTimes(const HitCheckpointMessage& aMsg)
{
  if (!CanRewind() || (aMsg.mCheckpointId != gCheckpointTimes.length() + 1)) {
    return;
  }
  gCheckpointTimes.append(TimeDuration::FromMicroseconds(aMsg.mDurationMicroseconds));

  if (gActiveChild->IsRecording()) {
    gTimeSinceLastFlush += gCheckpointTimes.back();

    // Occasionally flush while recording so replaying processes stay
    // reasonably current.
    if (aMsg.mCheckpointId == CheckpointId::First ||
        gTimeSinceLastFlush >= TimeDuration::FromSeconds(FlushSeconds))
    {
      if (MaybeFlushRecording()) {
        gTimeSinceLastFlush = 0;
      }
    }
  }

  gTimeSinceLastMajorCheckpoint += gCheckpointTimes.back();
  if (gTimeSinceLastMajorCheckpoint >= TimeDuration::FromSeconds(MajorCheckpointSeconds) ||
      gAlwaysMarkMajorCheckpoints)
  {
    // Alternate back and forth between assigning major checkpoints to the
    // two replaying processes.
    MOZ_RELEASE_ASSERT(gLastAssignedMajorCheckpoint);
    ChildProcessInfo* child = OtherReplayingChild(gLastAssignedMajorCheckpoint);
    AssignMajorCheckpoint(child, aMsg.mCheckpointId + 1);
    gTimeSinceLastMajorCheckpoint = 0;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Role Management
///////////////////////////////////////////////////////////////////////////////

static void
SpawnRecordingChild(const RecordingProcessData& aRecordingProcessData)
{
  MOZ_RELEASE_ASSERT(!gRecordingChild && !gFirstReplayingChild && !gSecondReplayingChild);
  gRecordingChild =
    new ChildProcessInfo(MakeUnique<ChildRoleActive>(), Some(aRecordingProcessData));
}

static void
SpawnSingleReplayingChild()
{
  MOZ_RELEASE_ASSERT(!gRecordingChild && !gFirstReplayingChild && !gSecondReplayingChild);
  gFirstReplayingChild =
    new ChildProcessInfo(MakeUnique<ChildRoleActive>(), Nothing());
}

static void
SpawnReplayingChildren()
{
  MOZ_RELEASE_ASSERT(CanRewind() && !gFirstReplayingChild && !gSecondReplayingChild);
  UniquePtr<ChildRole> firstRole;
  if (gRecordingChild) {
    firstRole = MakeUnique<ChildRoleStandby>();
  } else {
    firstRole = MakeUnique<ChildRoleActive>();
  }
  gFirstReplayingChild =
    new ChildProcessInfo(std::move(firstRole), Nothing());
  gSecondReplayingChild =
    new ChildProcessInfo(MakeUnique<ChildRoleStandby>(), Nothing());
  AssignMajorCheckpoint(gSecondReplayingChild, CheckpointId::First);
}

// Hit any installed breakpoints with the specified kind.
static void HitBreakpointsWithKind(js::BreakpointPosition::Kind aKind,
                                   bool aRecordingBoundary = false);

// Change the current active child, and select a new role for the old one.
static void
SwitchActiveChild(ChildProcessInfo* aChild, bool aRecoverPosition = true)
{
  MOZ_RELEASE_ASSERT(aChild != gActiveChild);
  ChildProcessInfo* oldActiveChild = gActiveChild;
  aChild->WaitUntilPaused();
  if (!aChild->IsRecording()) {
    if (aRecoverPosition) {
      aChild->Recover(gActiveChild);
    } else {
      Vector<SetBreakpointMessage*> breakpoints;
      gActiveChild->GetInstalledBreakpoints(breakpoints);
      for (SetBreakpointMessage* msg : breakpoints) {
        aChild->SendMessage(*msg);
      }
    }
  }
  aChild->SetRole(MakeUnique<ChildRoleActive>());
  if (oldActiveChild->IsRecording()) {
    oldActiveChild->SetRole(MakeUnique<ChildRoleInert>());
  } else {
    oldActiveChild->RecoverToCheckpoint(oldActiveChild->MostRecentSavedCheckpoint());
    oldActiveChild->SetRole(MakeUnique<ChildRoleStandby>());
  }

  // Position state is affected when we switch between recording and
  // replaying children.
  if (aChild->IsRecording() != oldActiveChild->IsRecording()) {
    HitBreakpointsWithKind(js::BreakpointPosition::Kind::PositionChange);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Preferences
///////////////////////////////////////////////////////////////////////////////

static bool gPreferencesLoaded;
static bool gRewindingEnabled;

void
PreferencesLoaded()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  MOZ_RELEASE_ASSERT(!gPreferencesLoaded);
  gPreferencesLoaded = true;

  gRewindingEnabled = Preferences::GetBool("devtools.recordreplay.enableRewinding");

  // Force-disable rewinding and saving checkpoints with an env var for testing.
  if (getenv("NO_REWIND")) {
    gRewindingEnabled = false;
  }

  if (gRecordingChild) {
    // Inform the recording child if we will be running devtools server code in
    // this process.
    if (DebuggerRunsInMiddleman()) {
      gRecordingChild->SendMessage(SetDebuggerRunsInMiddlemanMessage());
    }
  } else {
    // If there is no recording child, we have now initialized enough state
    // that we can start spawning replaying children.
    if (CanRewind()) {
      SpawnReplayingChildren();
    } else {
      SpawnSingleReplayingChild();
    }
  }
}

bool
CanRewind()
{
  MOZ_RELEASE_ASSERT(gPreferencesLoaded);
  return gRewindingEnabled;
}

bool
DebuggerRunsInMiddleman()
{
  if (IsRecordingOrReplaying()) {
    // This can be called in recording/replaying processes as well as the
    // middleman. Fetch the value which the middleman informed us of.
    return child::DebuggerRunsInMiddleman();
  }

  // Middleman processes which are recording and can't rewind do not run
  // developer tools server code. This will run in the recording process
  // instead.
  MOZ_RELEASE_ASSERT(IsMiddleman());
  return !gRecordingChild || CanRewind();
}

///////////////////////////////////////////////////////////////////////////////
// Saving Recordings
///////////////////////////////////////////////////////////////////////////////

// Synchronously flush the recording to disk.
static void
FlushRecording()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gActiveChild->IsRecording() && gActiveChild->IsPaused());

  // All replaying children must be paused while the recording is flushed.
  ForEachReplayingChild([=](ChildProcessInfo* aChild) {
      aChild->SetPauseNeeded();
      aChild->WaitUntilPaused();
    });

  gActiveChild->SendMessage(FlushRecordingMessage());
  gActiveChild->WaitUntilPaused();

  gLastRecordingCheckpoint = gActiveChild->LastCheckpoint();

  // We now have a usable recording for replaying children.
  static bool gHasFlushed = false;
  if (!gHasFlushed && CanRewind()) {
    SpawnReplayingChildren();
  }
  gHasFlushed = true;
}

// Get the replaying children to pause, and flush the recording if they already are.
static bool
MaybeFlushRecording()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gActiveChild->IsRecording() && gActiveChild->IsPaused());

  bool allPaused = true;
  ForEachReplayingChild([&](ChildProcessInfo* aChild) {
      if (!aChild->IsPaused()) {
        aChild->SetPauseNeeded();
        allPaused = false;
      }
    });

  if (allPaused) {
    FlushRecording();
    return true;
  }
  return false;
}

static void
RecvRecordingFlushed()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  ForEachReplayingChild([=](ChildProcessInfo* aChild) { aChild->ClearPauseNeeded(); });
}

// Recording children can idle indefinitely while waiting for input, without
// creating a checkpoint. If this might be a problem, this method induces the
// child to create a new checkpoint and pause.
static void
MaybeCreateCheckpointInRecordingChild()
{
  if (gActiveChild->IsRecording() && !gActiveChild->IsPaused()) {
    gActiveChild->SendMessage(CreateCheckpointMessage());
  }
}

// Send a message to the message manager in the UI process. This is consumed by
// various tests.
static void
SendMessageToUIProcess(const char* aMessage)
{
  AutoSafeJSContext cx;
  auto* cpmm = dom::ContentProcessMessageManager::Get();
  ErrorResult err;
  nsAutoString message;
  message.Append(NS_ConvertUTF8toUTF16(aMessage));
  JS::Rooted<JS::Value> undefined(cx);
  cpmm->SendAsyncMessage(cx, message, undefined, nullptr, nullptr, undefined, err);
  MOZ_RELEASE_ASSERT(!err.Failed());
  err.SuppressException();
}

// Handle to the recording file opened at startup.
static FileHandle gRecordingFd;

static void
SaveRecordingInternal(const ipc::FileDescriptor& aFile)
{
  MOZ_RELEASE_ASSERT(gRecordingChild);

  if (gRecordingChild == gActiveChild) {
    // The recording might not be up to date, flush it now.
    MOZ_RELEASE_ASSERT(gRecordingChild == gActiveChild);
    MaybeCreateCheckpointInRecordingChild();
    gRecordingChild->WaitUntilPaused();
    FlushRecording();
  }

  // Copy the file's contents to the new file.
  DirectSeekFile(gRecordingFd, 0);
  ipc::FileDescriptor::UniquePlatformHandle writefd = aFile.ClonePlatformHandle();
  char buf[4096];
  while (true) {
    size_t n = DirectRead(gRecordingFd, buf, sizeof(buf));
    if (!n) {
      break;
    }
    DirectWrite(writefd.get(), buf, n);
  }

  PrintSpew("Saved Recording Copy.\n");
  SendMessageToUIProcess("SaveRecordingFinished");
}

void
SaveRecording(const ipc::FileDescriptor& aFile)
{
  MOZ_RELEASE_ASSERT(IsMiddleman());

  if (NS_IsMainThread()) {
    SaveRecordingInternal(aFile);
  } else {
    MainThreadMessageLoop()->PostTask(NewRunnableFunction("SaveRecordingInternal",
                                                          SaveRecordingInternal, aFile));
  }
}

///////////////////////////////////////////////////////////////////////////////
// Explicit Pauses
///////////////////////////////////////////////////////////////////////////////

// At the last time the active child was explicitly paused, the ID of the
// checkpoint that needs to be saved for the child to rewind.
static size_t gLastExplicitPause;

// Any checkpoint we are trying to warp to and pause.
static Maybe<size_t> gTimeWarpTarget;

static bool
HasSavedCheckpointsInRange(ChildProcessInfo* aChild, size_t aStart, size_t aEnd)
{
  for (size_t i = aStart; i <= aEnd; i++) {
    if (!aChild->HasSavedCheckpoint(i)) {
      return false;
    }
  }
  return true;
}

static void
MarkActiveChildExplicitPause()
{
  MOZ_RELEASE_ASSERT(gActiveChild->IsPaused());
  size_t targetCheckpoint = gActiveChild->RewindTargetCheckpoint();

  if (gActiveChild->IsRecording()) {
    // Make sure any replaying children can play forward to the same point as
    // the recording.
    FlushRecording();
  } else if (CanRewind()) {
    // Make sure we have a replaying child that can rewind from this point.
    // Switch to the other one if (a) this process is responsible for rewinding
    // from this point, and (b) this process has not saved all intermediate
    // checkpoints going back to its last major checkpoint.
    if (gActiveChild == ReplayingChildResponsibleForSavingCheckpoint(targetCheckpoint)) {
      size_t lastMajorCheckpoint = LastMajorCheckpointPreceding(gActiveChild, targetCheckpoint);
      if (!HasSavedCheckpointsInRange(gActiveChild, lastMajorCheckpoint, targetCheckpoint)) {
        SwitchActiveChild(OtherReplayingChild(gActiveChild));
      }
    }
  }

  gLastExplicitPause = targetCheckpoint;
  PrintSpew("MarkActiveChildExplicitPause %d\n", (int) gLastExplicitPause);

  PokeChildren();
}

static Maybe<size_t>
ActiveChildTargetCheckpoint()
{
  if (gTimeWarpTarget.isSome()) {
    return gTimeWarpTarget;
  }
  if (gActiveChild->RewindTargetCheckpoint() <= gLastExplicitPause) {
    return Some(gActiveChild->RewindTargetCheckpoint());
  }
  return Nothing();
}

void
MaybeSwitchToReplayingChild()
{
  if (gActiveChild->IsRecording() && CanRewind()) {
    FlushRecording();
    size_t checkpoint = gActiveChild->RewindTargetCheckpoint();
    ChildProcessInfo* child =
      OtherReplayingChild(ReplayingChildResponsibleForSavingCheckpoint(checkpoint));
    SwitchActiveChild(child);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Initialization
///////////////////////////////////////////////////////////////////////////////

// Message loop processed on the main thread.
static MessageLoop* gMainThreadMessageLoop;

MessageLoop*
MainThreadMessageLoop()
{
  return gMainThreadMessageLoop;
}

static base::ProcessId gParentPid;

base::ProcessId
ParentProcessId()
{
  return gParentPid;
}

void
InitializeMiddleman(int aArgc, char* aArgv[], base::ProcessId aParentPid,
                    const base::SharedMemoryHandle& aPrefsHandle,
                    const ipc::FileDescriptor& aPrefMapHandle)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  CrashReporter::AnnotateCrashReport(CrashReporter::Annotation::RecordReplay, true);

  gParentPid = aParentPid;

  // Construct the message that will be sent to each child when starting up.
  IntroductionMessage* msg =
    IntroductionMessage::New(aParentPid, aArgc, aArgv);
  ChildProcessInfo::SetIntroductionMessage(msg);

  MOZ_RELEASE_ASSERT(gProcessKind == ProcessKind::MiddlemanRecording ||
                     gProcessKind == ProcessKind::MiddlemanReplaying);

  InitializeGraphicsMemory();

  gMonitor = new Monitor();

  gMainThreadMessageLoop = MessageLoop::current();

  if (gProcessKind == ProcessKind::MiddlemanRecording) {
    RecordingProcessData data(aPrefsHandle, aPrefMapHandle);
    SpawnRecordingChild(data);
  }

  InitializeForwarding();

  // Open a file handle to the recording file we can use for saving recordings
  // later on.
  gRecordingFd = DirectOpenFile(gRecordingFilename, false);
}

///////////////////////////////////////////////////////////////////////////////
// Debugger Messages
///////////////////////////////////////////////////////////////////////////////

// Buffer for receiving the next debugger response.
static js::CharBuffer* gResponseBuffer;

static void
RecvDebuggerResponse(const DebuggerResponseMessage& aMsg)
{
  MOZ_RELEASE_ASSERT(gResponseBuffer && gResponseBuffer->empty());
  gResponseBuffer->append(aMsg.Buffer(), aMsg.BufferSize());
}

void
SendRequest(const js::CharBuffer& aBuffer, js::CharBuffer* aResponse)
{
  MaybeCreateCheckpointInRecordingChild();
  gActiveChild->WaitUntilPaused();

  MOZ_RELEASE_ASSERT(!gResponseBuffer);
  gResponseBuffer = aResponse;

  DebuggerRequestMessage* msg = DebuggerRequestMessage::New(aBuffer.begin(), aBuffer.length());
  gActiveChild->SendMessage(*msg);
  free(msg);

  // Wait for the child to respond to the query.
  gActiveChild->WaitUntilPaused();
  MOZ_RELEASE_ASSERT(gResponseBuffer == aResponse);
  MOZ_RELEASE_ASSERT(gResponseBuffer->length() != 0);
  gResponseBuffer = nullptr;
}

void
SetBreakpoint(size_t aId, const js::BreakpointPosition& aPosition)
{
  MaybeCreateCheckpointInRecordingChild();
  gActiveChild->WaitUntilPaused();

  gActiveChild->SendMessage(SetBreakpointMessage(aId, aPosition));

  // Also set breakpoints in any recording child that is not currently active.
  // We can't recover recording processes so need to keep their breakpoints up
  // to date.
  if (!gActiveChild->IsRecording() && gRecordingChild) {
    gRecordingChild->SendMessage(SetBreakpointMessage(aId, aPosition));
  }
}

// Flags for the preferred direction of travel when execution unpauses,
// according to the last direction we were explicitly given.
static bool gChildExecuteForward = true;
static bool gChildExecuteBackward = false;

// Whether there is a ResumeForwardOrBackward task which should execute on the
// main thread. This will continue execution in the preferred direction.
static bool gResumeForwardOrBackward = false;

static void
MaybeSendRepaintMessage()
{
  // In repaint stress mode, we want to trigger a repaint at every checkpoint,
  // so before resuming after the child pauses at each checkpoint, send it a
  // repaint message. There might not be a debugger open, so manually craft the
  // same message which the debugger would send to trigger a repaint and parse
  // the result.
  if (InRepaintStressMode()) {
    MaybeSwitchToReplayingChild();

    const char16_t contents[] = u"{\"type\":\"repaint\"}";

    js::CharBuffer request, response;
    request.append(contents, ArrayLength(contents) - 1);
    SendRequest(request, &response);

    AutoSafeJSContext cx;
    JS::RootedValue value(cx);
    if (JS_ParseJSON(cx, response.begin(), response.length(), &value)) {
      MOZ_RELEASE_ASSERT(value.isObject());
      JS::RootedObject obj(cx, &value.toObject());
      RootedValue width(cx), height(cx);
      if (JS_GetProperty(cx, obj, "width", &width) && width.isNumber() && width.toNumber() &&
          JS_GetProperty(cx, obj, "height", &height) && height.isNumber() && height.toNumber()) {
        PaintMessage message(CheckpointId::Invalid, width.toNumber(), height.toNumber());
        UpdateGraphicsInUIProcess(&message);
      }
    }
  }
}

void
Resume(bool aForward)
{
  gActiveChild->WaitUntilPaused();

  MaybeSendRepaintMessage();

  // Set the preferred direction of travel.
  gResumeForwardOrBackward = false;
  gChildExecuteForward = aForward;
  gChildExecuteBackward = !aForward;

  // When rewinding, make sure the active child can rewind to the previous
  // checkpoint.
  if (!aForward && !gActiveChild->HasSavedCheckpoint(gActiveChild->RewindTargetCheckpoint())) {
    size_t targetCheckpoint = gActiveChild->RewindTargetCheckpoint();

    // Don't rewind if we are at the beginning of the recording.
    if (targetCheckpoint == CheckpointId::Invalid) {
      SendMessageToUIProcess("HitRecordingBeginning");
      HitBreakpointsWithKind(js::BreakpointPosition::Kind::ForcedPause,
                             /* aRecordingBoundary = */ true);
      return;
    }

    // Find the replaying child responsible for saving the target checkpoint.
    // We should have explicitly paused before rewinding and given fill roles
    // to the replaying children.
    ChildProcessInfo* targetChild = ReplayingChildResponsibleForSavingCheckpoint(targetCheckpoint);
    MOZ_RELEASE_ASSERT(targetChild != gActiveChild);

    // This process will be the new active child, so make sure it has saved the
    // checkpoint we need it to.
    targetChild->WaitUntil([=]() {
        return targetChild->HasSavedCheckpoint(targetCheckpoint)
            && targetChild->IsPaused();
      });

    SwitchActiveChild(targetChild);
  }

  if (aForward) {
    // Don't send a replaying process past the recording endpoint.
    if (gActiveChild->IsPausedAtRecordingEndpoint()) {
      // Look for a recording child we can transition into.
      MOZ_RELEASE_ASSERT(!gActiveChild->IsRecording());
      if (!gRecordingChild) {
        SendMessageToUIProcess("HitRecordingEndpoint");
        HitBreakpointsWithKind(js::BreakpointPosition::Kind::ForcedPause,
                               /* aRecordingBoundary = */ true);
        return;
      }

      // Switch to the recording child as the active child and continue execution.
      SwitchActiveChild(gRecordingChild);
    }

    EnsureMajorCheckpointSaved(gActiveChild, gActiveChild->LastCheckpoint() + 1);

    // Idle children might change their behavior as we run forward.
    PokeChildren();
  }

  gActiveChild->SendMessage(ResumeMessage(aForward));
}

void
TimeWarp(const js::ExecutionPoint& aTarget)
{
  gActiveChild->WaitUntilPaused();

  // There is no preferred direction of travel after warping.
  gResumeForwardOrBackward = false;
  gChildExecuteForward = false;
  gChildExecuteBackward = false;

  // Make sure the active child can rewind to the checkpoint prior to the
  // warp target.
  MOZ_RELEASE_ASSERT(gTimeWarpTarget.isNothing());
  gTimeWarpTarget.emplace(aTarget.mCheckpoint);

  PokeChildren();

  if (!gActiveChild->HasSavedCheckpoint(aTarget.mCheckpoint)) {
    // Find the replaying child responsible for saving the target checkpoint.
    ChildProcessInfo* targetChild = ReplayingChildResponsibleForSavingCheckpoint(aTarget.mCheckpoint);

    if (targetChild == gActiveChild) {
      // Switch to the other replaying child while this one saves the necessary
      // checkpoint.
      SwitchActiveChild(OtherReplayingChild(gActiveChild));
    }

    // This process will be the new active child, so make sure it has saved the
    // checkpoint we need it to.
    targetChild->WaitUntil([=]() {
        return targetChild->HasSavedCheckpoint(aTarget.mCheckpoint)
            && targetChild->IsPaused();
      });

    SwitchActiveChild(targetChild, /* aRecoverPosition = */ false);
  }

  gTimeWarpTarget.reset();

  if (!gActiveChild->IsPausedAtCheckpoint() || gActiveChild->LastCheckpoint() != aTarget.mCheckpoint) {
    gActiveChild->SendMessage(RestoreCheckpointMessage(aTarget.mCheckpoint));
    gActiveChild->WaitUntilPaused();
  }

  gActiveChild->SendMessage(RunToPointMessage(aTarget));

  gActiveChild->WaitUntilPaused();
  SendMessageToUIProcess("TimeWarpFinished");
  HitBreakpointsWithKind(js::BreakpointPosition::Kind::ForcedPause);
}

void
Pause()
{
  MaybeCreateCheckpointInRecordingChild();
  gActiveChild->WaitUntilPaused();

  // If the debugger has explicitly paused then there is no preferred direction
  // of travel.
  gChildExecuteForward = false;
  gChildExecuteBackward = false;

  MarkActiveChildExplicitPause();
}

static void
ResumeForwardOrBackward()
{
  MOZ_RELEASE_ASSERT(!gChildExecuteForward || !gChildExecuteBackward);

  if (gResumeForwardOrBackward && (gChildExecuteForward || gChildExecuteBackward)) {
    Resume(gChildExecuteForward);
  }
}

void
ResumeBeforeWaitingForIPDLReply()
{
  MOZ_RELEASE_ASSERT(gActiveChild->IsRecording());

  // The main thread is about to block while it waits for a sync reply from the
  // recording child process. If the child is paused, resume it immediately so
  // that we don't deadlock.
  if (gActiveChild->IsPaused()) {
    MOZ_RELEASE_ASSERT(gChildExecuteForward);
    Resume(true);
  }
}

static void
RecvHitCheckpoint(const HitCheckpointMessage& aMsg)
{
  UpdateCheckpointTimes(aMsg);
  MaybeUpdateGraphicsAtCheckpoint(aMsg.mCheckpointId);

  // Position state is affected when new checkpoints are reached.
  HitBreakpointsWithKind(js::BreakpointPosition::Kind::PositionChange);

  // Resume either forwards or backwards. Break the resume off into a separate
  // runnable, to avoid starving any code already on the stack and waiting for
  // the process to pause. Immediately resume if the main thread is blocked.
  if (MainThreadIsWaitingForIPDLReply()) {
    MOZ_RELEASE_ASSERT(gChildExecuteForward);
    Resume(true);
  } else if (!gResumeForwardOrBackward) {
    gResumeForwardOrBackward = true;
    gMainThreadMessageLoop->PostTask(NewRunnableFunction("ResumeForwardOrBackward",
                                                         ResumeForwardOrBackward));
  }
}

static void
HitBreakpoint(uint32_t* aBreakpoints, size_t aNumBreakpoints, bool aRecordingBoundary)
{
  if (!gActiveChild->IsPaused()) {
    if (aNumBreakpoints) {
      Print("Warning: Process resumed before breakpoints were hit.\n");
    }
    delete[] aBreakpoints;
    return;
  }

  MarkActiveChildExplicitPause();

  gResumeForwardOrBackward = true;

  // Call breakpoint handlers until one of them explicitly resumes forward or
  // backward travel.
  for (size_t i = 0; i < aNumBreakpoints && gResumeForwardOrBackward; i++) {
    AutoSafeJSContext cx;
    if (!js::HitBreakpoint(cx, aBreakpoints[i])) {
      Print("Warning: hitBreakpoint hook threw an exception.\n");
    }
  }

  // If the child was not explicitly resumed by any breakpoint handler, and we
  // are not at a forced pause at the recording boundary, resume travel in
  // whichever direction it was going previously.
  if (gResumeForwardOrBackward && !aRecordingBoundary) {
    ResumeForwardOrBackward();
  }

  delete[] aBreakpoints;
}

static void
RecvHitBreakpoint(const HitBreakpointMessage& aMsg)
{
  uint32_t* breakpoints = new uint32_t[aMsg.NumBreakpoints()];
  PodCopy(breakpoints, aMsg.Breakpoints(), aMsg.NumBreakpoints());
  gMainThreadMessageLoop->PostTask(NewRunnableFunction("HitBreakpoint", HitBreakpoint,
                                                       breakpoints, aMsg.NumBreakpoints(),
                                                       /* aRecordingBoundary = */ false));
}

static void
HitBreakpointsWithKind(js::BreakpointPosition::Kind aKind, bool aRecordingBoundary)
{
  Vector<uint32_t> breakpoints;
  gActiveChild->GetMatchingInstalledBreakpoints([=](js::BreakpointPosition::Kind aInstalled) {
      return aInstalled == aKind;
    }, breakpoints);
  if (!breakpoints.empty()) {
    uint32_t* newBreakpoints = new uint32_t[breakpoints.length()];
    PodCopy(newBreakpoints, breakpoints.begin(), breakpoints.length());
    gMainThreadMessageLoop->PostTask(NewRunnableFunction("HitBreakpoint", HitBreakpoint,
                                                         newBreakpoints, breakpoints.length(),
                                                         aRecordingBoundary));
  }
}

static void
RecvMiddlemanCallRequest(const MiddlemanCallRequestMessage& aMsg)
{
  MiddlemanCallResponseMessage* response = ProcessMiddlemanCallMessage(aMsg);
  gActiveChild->SendMessage(*response);
  free(response);
}

} // namespace parent
} // namespace recordreplay
} // namespace mozilla
