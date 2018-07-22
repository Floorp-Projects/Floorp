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
#include "mozilla/dom/ProcessGlobal.h"
#include "InfallibleVector.h"
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

// Whether to delete the recording file when finishing.
static bool gRecordingFileIsTemporary;

void
Shutdown()
{
  if (gRecordingFileIsTemporary) {
    DirectDeleteFile(gRecordingFilename);
  }

  delete gRecordingChild;
  delete gFirstReplayingChild;
  delete gSecondReplayingChild;
  _exit(0);
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
      UpdateGraphicsInUIProcess((const PaintMessage*) &aMsg);
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
    default:
      MOZ_CRASH("Unexpected message");
    }
  }
};

bool
ActiveChildIsRecording()
{
  return gActiveChild->IsRecording();
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

// Return whether the active child is explicitly paused somewhere, or has
// started rewinding after being explicitly paused. Standby roles must save all
// intermediate checkpoints they are responsible for, in the range from their
// most recent major checkpoint up to the checkpoint where the active child can
// rewind to.
static bool ActiveChildIsPausedOrRewinding();

// Notify a child it does not need to save aCheckpoint, unless it is a major
// checkpoint for the child.
static void
ClearIfSavedNonMajorCheckpoint(ChildProcessInfo* aChild, size_t aCheckpoint)
{
  if (aChild->ShouldSaveCheckpoint(aCheckpoint) &&
      !aChild->IsMajorCheckpoint(aCheckpoint) &&
      aCheckpoint != CheckpointId::First)
  {
    aChild->SendMessage(SetSaveCheckpointMessage(aCheckpoint, false));
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
    if (!ActiveChildIsPausedOrRewinding()) {
      break;
    }

    // The startpoint of the range is the most recent major checkpoint prior to
    // the active child's position.
    size_t targetCheckpoint = gActiveChild->RewindTargetCheckpoint();
    size_t lastMajorCheckpoint = LastMajorCheckpointPreceding(mProcess, targetCheckpoint);

    // If there is no major checkpoint prior to the active child's position,
    // just idle.
    if (lastMajorCheckpoint == CheckpointId::Invalid) {
      return;
    }

    // If we haven't reached the last major checkpoint, we need to run forward
    // without saving intermediate checkpoints.
    if (mProcess->LastCheckpoint() < lastMajorCheckpoint) {
      break;
    }

    // The endpoint of the range is the checkpoint prior to either the active
    // child's current position, or the other replaying child's most recent
    // major checkpoint.
    size_t otherMajorCheckpoint =
      LastMajorCheckpointPreceding(OtherReplayingChild(mProcess), targetCheckpoint);
    if (otherMajorCheckpoint > lastMajorCheckpoint) {
      MOZ_RELEASE_ASSERT(otherMajorCheckpoint <= targetCheckpoint);
      targetCheckpoint = otherMajorCheckpoint - 1;
    }

    // Find the first checkpoint in the fill range which we have not saved.
    Maybe<size_t> missingCheckpoint;
    for (size_t i = lastMajorCheckpoint; i <= targetCheckpoint; i++) {
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
    ClearIfSavedNonMajorCheckpoint(mProcess, mProcess->LastCheckpoint() + 1);
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
  if (aId != CheckpointId::First) {
    aChild->WaitUntilPaused();
    aChild->SendMessage(SetSaveCheckpointMessage(aId, true));
  }
  gLastAssignedMajorCheckpoint = aChild;
}

static void FlushRecording();

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
      FlushRecording();
      gTimeSinceLastFlush = 0;
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
SpawnRecordingChild()
{
  MOZ_RELEASE_ASSERT(!gRecordingChild && !gFirstReplayingChild && !gSecondReplayingChild);
  gRecordingChild =
    new ChildProcessInfo(MakeUnique<ChildRoleActive>(), /* aRecording = */ true);
}

static void
SpawnSingleReplayingChild()
{
  MOZ_RELEASE_ASSERT(!gRecordingChild && !gFirstReplayingChild && !gSecondReplayingChild);
  gFirstReplayingChild =
    new ChildProcessInfo(MakeUnique<ChildRoleActive>(), /* aRecording = */ false);
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
    new ChildProcessInfo(std::move(firstRole), /* aRecording = */ false);
  gSecondReplayingChild =
    new ChildProcessInfo(MakeUnique<ChildRoleStandby>(), /* aRecording = */ false);
  AssignMajorCheckpoint(gSecondReplayingChild, CheckpointId::First);
}

// Change the current active child, and select a new role for the old one.
static void
SwitchActiveChild(ChildProcessInfo* aChild)
{
  MOZ_RELEASE_ASSERT(aChild != gActiveChild);
  ChildProcessInfo* oldActiveChild = gActiveChild;
  aChild->WaitUntilPaused();
  if (!aChild->IsRecording()) {
    aChild->Recover(gActiveChild);
  }
  aChild->SetRole(MakeUnique<ChildRoleActive>());
  if (oldActiveChild->IsRecording()) {
    oldActiveChild->SetRole(MakeUnique<ChildRoleInert>());
  } else {
    oldActiveChild->RecoverToCheckpoint(oldActiveChild->MostRecentSavedCheckpoint());
    oldActiveChild->SetRole(MakeUnique<ChildRoleStandby>());
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

  // If there is no recording child, we have now initialized enough state
  // that we can start spawning replaying children.
  if (!gRecordingChild) {
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

///////////////////////////////////////////////////////////////////////////////
// Saving Recordings
///////////////////////////////////////////////////////////////////////////////

static void
FlushRecording()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gActiveChild->IsRecording() && gActiveChild->IsPaused());

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
  dom::ProcessGlobal* cpmm = dom::ProcessGlobal::Get();
  ErrorResult err;
  nsAutoString message;
  message.Append(NS_ConvertUTF8toUTF16(aMessage));
  JS::Rooted<JS::Value> undefined(cx);
  cpmm->SendAsyncMessage(cx, message, undefined, nullptr, nullptr, undefined, err);
  MOZ_RELEASE_ASSERT(!err.Failed());
  err.SuppressException();
}

static void
SaveRecordingInternal(char* aFilename)
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
  int readfd = DirectOpenFile(gRecordingFilename, /* aWriting = */ false);
  int writefd = DirectOpenFile(aFilename, /* aWriting = */ true);
  char buf[4096];
  while (true) {
    size_t n = DirectRead(readfd, buf, sizeof(buf));
    if (!n) {
      break;
    }
    DirectWrite(writefd, buf, n);
  }
  DirectCloseFile(readfd);
  DirectCloseFile(writefd);

  PrintSpew("Copied Recording %s\n", aFilename);
  SendMessageToUIProcess("SaveRecordingFinished");
  free(aFilename);
}

void
SaveRecording(const nsCString& aFilename)
{
  MOZ_RELEASE_ASSERT(IsMiddleman());

  char* filename = strdup(aFilename.get());
  if (NS_IsMainThread()) {
    SaveRecordingInternal(filename);
  } else {
    MainThreadMessageLoop()->PostTask(NewRunnableFunction("SaveRecordingInternal",
                                                          SaveRecordingInternal, filename));
  }
}

///////////////////////////////////////////////////////////////////////////////
// Explicit Pauses
///////////////////////////////////////////////////////////////////////////////

// At the last time the active child was explicitly paused, the ID of the
// checkpoint that needs to be saved for the child to rewind.
static size_t gLastExplicitPause;

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

// Return whether a child is paused at a breakpoint set by the user or by
// stepping around, at which point the debugger will send requests to the
// child to inspect its state. This excludes breakpoints set for things
// internal to the debugger.
static bool
IsUserBreakpoint(JS::replay::ExecutionPosition::Kind aKind)
{
  MOZ_RELEASE_ASSERT(aKind != JS::replay::ExecutionPosition::Invalid);
  return aKind != JS::replay::ExecutionPosition::NewScript;
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

    // When paused at a breakpoint, the JS debugger may (indeed, will) send
    // requests to the recording child which can affect the recording. These
    // side effects won't be replayed later on, so the C++ side of the debugger
    // will not provide a useful answer to these requests, reporting an
    // unhandled divergence instead. To avoid this issue and provide a
    // consistent debugger experience whether still recording or replaying, we
    // switch the active child to a replaying child when pausing at a
    // breakpoint.
    if (CanRewind() && gActiveChild->IsPausedAtMatchingBreakpoint(IsUserBreakpoint)) {
      ChildProcessInfo* child =
        OtherReplayingChild(ReplayingChildResponsibleForSavingCheckpoint(targetCheckpoint));
      SwitchActiveChild(child);
    }
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

static bool
ActiveChildIsPausedOrRewinding()
{
  return gActiveChild->RewindTargetCheckpoint() <= gLastExplicitPause;
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

// Initialize hooks used by the debugger.
static void InitDebuggerHooks();

// Contents of the prefs shmem block that is sent to the child on startup.
static char* gShmemPrefs;
static size_t gShmemPrefsLen;

void
NotePrefsShmemContents(char* aPrefs, size_t aPrefsLen)
{
  MOZ_RELEASE_ASSERT(!gShmemPrefs);
  gShmemPrefs = new char[aPrefsLen];
  memcpy(gShmemPrefs, aPrefs, aPrefsLen);
  gShmemPrefsLen = aPrefsLen;
}

void
InitializeMiddleman(int aArgc, char* aArgv[], base::ProcessId aParentPid)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(gShmemPrefs);

  // Construct the message that will be sent to each child when starting up.
  IntroductionMessage* msg =
    IntroductionMessage::New(aParentPid, gShmemPrefs, gShmemPrefsLen, aArgc, aArgv);
  ChildProcessInfo::SetIntroductionMessage(msg);

  MOZ_RELEASE_ASSERT(gProcessKind == ProcessKind::MiddlemanRecording ||
                     gProcessKind == ProcessKind::MiddlemanReplaying);

  // Use a temporary file for the recording if the filename is unspecified.
  if (!strcmp(gRecordingFilename, "*")) {
    MOZ_RELEASE_ASSERT(gProcessKind == ProcessKind::MiddlemanRecording);
    free(gRecordingFilename);
    gRecordingFilename = mktemp(strdup("/tmp/RecordingXXXXXX"));
    gRecordingFileIsTemporary = true;
  }

  InitDebuggerHooks();
  InitializeGraphicsMemory();

  gMonitor = new Monitor();

  gMainThreadMessageLoop = MessageLoop::current();

  if (gProcessKind == ProcessKind::MiddlemanRecording) {
    SpawnRecordingChild();
  }

  InitializeForwarding();
}

///////////////////////////////////////////////////////////////////////////////
// Debugger Messages
///////////////////////////////////////////////////////////////////////////////

// Buffer for receiving the next debugger response.
static JS::replay::CharBuffer* gResponseBuffer;

static void
RecvDebuggerResponse(const DebuggerResponseMessage& aMsg)
{
  MOZ_RELEASE_ASSERT(gResponseBuffer && gResponseBuffer->empty());
  if (!gResponseBuffer->append(aMsg.Buffer(), aMsg.BufferSize())) {
    MOZ_CRASH("RecvDebuggerResponse");
  }
}

static void
HookDebuggerRequest(const JS::replay::CharBuffer& aBuffer, JS::replay::CharBuffer* aResponse)
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

static void
HookSetBreakpoint(size_t aId, const JS::replay::ExecutionPosition& aPosition)
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
HookResume(bool aForward)
{
  gActiveChild->WaitUntilPaused();

  // Set the preferred direction of travel.
  gResumeForwardOrBackward = false;
  gChildExecuteForward = aForward;
  gChildExecuteBackward = !aForward;

  // When rewinding, make sure the active child can rewind to the previous
  // checkpoint.
  if (!aForward && !gActiveChild->HasSavedCheckpoint(gActiveChild->RewindTargetCheckpoint())) {
    MOZ_RELEASE_ASSERT(ActiveChildIsPausedOrRewinding());
    size_t targetCheckpoint = gActiveChild->RewindTargetCheckpoint();

    // Don't rewind if we are at the beginning of the recording.
    if (targetCheckpoint == CheckpointId::Invalid) {
      SendMessageToUIProcess("HitRecordingBeginning");
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
        MarkActiveChildExplicitPause();
        SendMessageToUIProcess("HitRecordingEndpoint");
        return;
      }

      // Switch to the recording child as the active child and continue execution.
      SwitchActiveChild(gRecordingChild);
    }

    ClearIfSavedNonMajorCheckpoint(gActiveChild, gActiveChild->LastCheckpoint() + 1);

    // Idle children might change their behavior as we run forward.
    PokeChildren();
  }

  gActiveChild->SendMessage(ResumeMessage(aForward));
}

static void
HookPause()
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
    HookResume(gChildExecuteForward);
  }
}

static void
RecvHitCheckpoint(const HitCheckpointMessage& aMsg)
{
  UpdateCheckpointTimes(aMsg);

  // Resume either forwards or backwards. Break the resume off into a separate
  // runnable, to avoid starving any code already on the stack and waiting for
  // the process to pause. Immediately resume if the main thread is blocked.
  if (MainThreadIsWaitingForIPDLReply()) {
    MOZ_RELEASE_ASSERT(gChildExecuteForward);
    HookResume(true);
  } else if (!gResumeForwardOrBackward) {
    gResumeForwardOrBackward = true;
    gMainThreadMessageLoop->PostTask(NewRunnableFunction("ResumeForwardOrBackward",
                                                         ResumeForwardOrBackward));
  }
}

static void
HitBreakpoint(uint32_t* aBreakpoints, size_t aNumBreakpoints)
{
  MarkActiveChildExplicitPause();

  MOZ_RELEASE_ASSERT(!gResumeForwardOrBackward);
  gResumeForwardOrBackward = true;

  // Call breakpoint handlers until one of them explicitly resumes forward or
  // backward travel.
  for (size_t i = 0; i < aNumBreakpoints && gResumeForwardOrBackward; i++) {
    AutoSafeJSContext cx;
    if (!JS::replay::hooks.hitBreakpointMiddleman(cx, aBreakpoints[i])) {
      Print("Warning: hitBreakpoint hook threw an exception.\n");
    }
  }

  // If the child was not explicitly resumed by any breakpoint handler, resume
  // travel in whichever direction it was going previously.
  if (gResumeForwardOrBackward) {
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
                                                       breakpoints, aMsg.NumBreakpoints()));
}

static void
InitDebuggerHooks()
{
  JS::replay::hooks.debugRequestMiddleman = HookDebuggerRequest;
  JS::replay::hooks.setBreakpointMiddleman = HookSetBreakpoint;
  JS::replay::hooks.resumeMiddleman = HookResume;
  JS::replay::hooks.pauseMiddleman = HookPause;
  JS::replay::hooks.canRewindMiddleman = CanRewind;
}

} // namespace parent
} // namespace recordreplay
} // namespace mozilla
