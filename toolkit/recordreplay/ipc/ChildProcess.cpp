/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ParentInternal.h"

#include "base/task.h"
#include "mozilla/dom/ContentChild.h"
#include "Thread.h"

namespace mozilla {
namespace recordreplay {
namespace parent {

// A saved introduction message for sending to all children.
static IntroductionMessage* gIntroductionMessage;

// How many channels have been constructed so far.
static size_t gNumChannels;

// Whether children might be debugged and should not be treated as hung.
static bool gChildrenAreDebugging;

// Whether we are allowed to restart crashed/hung child processes.
static bool gRestartEnabled;

/* static */ void
ChildProcessInfo::SetIntroductionMessage(IntroductionMessage* aMessage)
{
  gIntroductionMessage = aMessage;
}

ChildProcessInfo::ChildProcessInfo(UniquePtr<ChildRole> aRole, bool aRecording)
  : mChannel(nullptr)
  , mRecording(aRecording)
  , mRecoveryStage(RecoveryStage::None)
  , mPaused(false)
  , mPausedMessage(nullptr)
  , mLastCheckpoint(CheckpointId::Invalid)
  , mNumRecoveredMessages(0)
  , mNumRestarts(0)
  , mRole(std::move(aRole))
  , mPauseNeeded(false)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  static bool gFirst = false;
  if (!gFirst) {
    gFirst = true;
    gChildrenAreDebugging = !!getenv("WAIT_AT_START");
    gRestartEnabled = !getenv("NO_RESTARTS");
  }

  mRole->SetProcess(this);

  LaunchSubprocess();

  // Replaying processes always save the first checkpoint, if saving
  // checkpoints is allowed. This is currently assumed by the rewinding
  // mechanism in the replaying process, and would be nice to investigate
  // removing.
  if (!aRecording && CanRewind()) {
    SendMessage(SetSaveCheckpointMessage(CheckpointId::First, true));
  }

  mRole->Initialize();
}

ChildProcessInfo::~ChildProcessInfo()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (IsRecording()) {
    SendMessage(TerminateMessage());
  }
}

ChildProcessInfo::Disposition
ChildProcessInfo::GetDisposition()
{
  // We can determine the disposition of the child by looking at the first
  // resume message sent since the last time it reached a checkpoint.
  for (Message* msg : mMessages) {
    if (msg->mType == MessageType::Resume) {
      const ResumeMessage& nmsg = static_cast<const ResumeMessage&>(*msg);
      return nmsg.mForward ? AfterLastCheckpoint : BeforeLastCheckpoint;
    }
  }
  return AtLastCheckpoint;
}

bool
ChildProcessInfo::IsPausedAtCheckpoint()
{
  return IsPaused() && mPausedMessage->mType == MessageType::HitCheckpoint;
}

bool
ChildProcessInfo::IsPausedAtRecordingEndpoint()
{
  if (!IsPaused()) {
    return false;
  }
  if (mPausedMessage->mType == MessageType::HitCheckpoint) {
    return static_cast<HitCheckpointMessage*>(mPausedMessage)->mRecordingEndpoint;
  }
  if (mPausedMessage->mType == MessageType::HitBreakpoint) {
    return static_cast<HitBreakpointMessage*>(mPausedMessage)->mRecordingEndpoint;
  }
  return false;
}

bool
ChildProcessInfo::IsPausedAtMatchingBreakpoint(const BreakpointFilter& aFilter)
{
  if (!IsPaused() || mPausedMessage->mType != MessageType::HitBreakpoint) {
    return false;
  }

  HitBreakpointMessage* npaused = static_cast<HitBreakpointMessage*>(mPausedMessage);
  for (size_t i = 0; i < npaused->NumBreakpoints(); i++) {
    uint32_t breakpointId = npaused->Breakpoints()[i];

    // Find the last time we sent a SetBreakpoint message to this process for
    // this breakpoint ID.
    SetBreakpointMessage* lastSet = nullptr;
    for (Message* msg : mMessages) {
      if (msg->mType == MessageType::SetBreakpoint) {
        SetBreakpointMessage* nmsg = static_cast<SetBreakpointMessage*>(msg);
        if (nmsg->mId == breakpointId) {
          lastSet = nmsg;
        }
      }
    }
    MOZ_RELEASE_ASSERT(lastSet && lastSet->mPosition.IsValid());
    if (aFilter(lastSet->mPosition.mKind)) {
      return true;
    }
  }

  return false;
}

void
ChildProcessInfo::AddMajorCheckpoint(size_t aId)
{
  // Major checkpoints should be listed in order.
  MOZ_RELEASE_ASSERT(mMajorCheckpoints.empty() || aId > mMajorCheckpoints.back());
  mMajorCheckpoints.append(aId);
}

void
ChildProcessInfo::SetRole(UniquePtr<ChildRole> aRole)
{
  MOZ_RELEASE_ASSERT(!IsRecovering());

  PrintSpew("SetRole:%d %s\n", (int) GetId(), ChildRole::TypeString(aRole->GetType()));

  mRole = std::move(aRole);
  mRole->SetProcess(this);
  mRole->Initialize();
}

void
ChildProcessInfo::OnIncomingMessage(size_t aChannelId, const Message& aMsg)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Ignore messages from channels for subprocesses we terminated already.
  if (aChannelId != mChannel->GetId()) {
    return;
  }

  // Always handle fatal errors in the same way.
  if (aMsg.mType == MessageType::FatalError) {
    const FatalErrorMessage& nmsg = static_cast<const FatalErrorMessage&>(aMsg);
    AttemptRestart(nmsg.Error());
    return;
  }

  mLastMessageTime = TimeStamp::Now();

  if (IsRecovering()) {
    OnIncomingRecoveryMessage(aMsg);
    return;
  }

  // Update paused state.
  MOZ_RELEASE_ASSERT(!IsPaused());
  switch (aMsg.mType) {
  case MessageType::HitCheckpoint:
  case MessageType::HitBreakpoint:
    MOZ_RELEASE_ASSERT(!mPausedMessage);
    mPausedMessage = aMsg.Clone();
    MOZ_FALLTHROUGH;
  case MessageType::DebuggerResponse:
  case MessageType::RecordingFlushed:
    MOZ_RELEASE_ASSERT(mPausedMessage);
    mPaused = true;
    break;
  default:
    break;
  }

  if (aMsg.mType == MessageType::HitCheckpoint) {
    const HitCheckpointMessage& nmsg = static_cast<const HitCheckpointMessage&>(aMsg);
    mLastCheckpoint = nmsg.mCheckpointId;

    // All messages sent since the last checkpoint are now obsolete, except
    // SetBreakpoint messages.
    InfallibleVector<Message*> newMessages;
    for (Message* msg : mMessages) {
      if (msg->mType == MessageType::SetBreakpoint) {
        // Look for an older SetBreakpoint on the same ID to overwrite.
        bool found = false;
        for (Message*& older : newMessages) {
          if (static_cast<SetBreakpointMessage*>(msg)->mId ==
              static_cast<SetBreakpointMessage*>(older)->mId) {
            free(older);
            older = msg;
            found = true;
          }
        }
        if (!found) {
          newMessages.emplaceBack(msg);
        }
      } else {
        free(msg);
      }
    }
    mMessages = std::move(newMessages);
  }

  // The primordial HitCheckpoint messages is not forwarded to the role, as it
  // has not been initialized yet.
  if (aMsg.mType != MessageType::HitCheckpoint || mLastCheckpoint) {
    mRole->OnIncomingMessage(aMsg);
  }
}

void
ChildProcessInfo::SendMessage(const Message& aMsg)
{
  MOZ_RELEASE_ASSERT(!IsRecovering());
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Update paused state.
  MOZ_RELEASE_ASSERT(IsPaused() ||
                     aMsg.mType == MessageType::CreateCheckpoint ||
                     aMsg.mType == MessageType::Terminate);
  switch (aMsg.mType) {
  case MessageType::Resume:
  case MessageType::RestoreCheckpoint:
    free(mPausedMessage);
    mPausedMessage = nullptr;
    MOZ_FALLTHROUGH;
  case MessageType::DebuggerRequest:
  case MessageType::FlushRecording:
    mPaused = false;
    break;
  default:
    break;
  }

  // Keep track of messages which affect the child's behavior.
  switch (aMsg.mType) {
  case MessageType::Resume:
  case MessageType::RestoreCheckpoint:
  case MessageType::DebuggerRequest:
  case MessageType::SetBreakpoint:
    mMessages.emplaceBack(aMsg.Clone());
    break;
  default:
    break;
  }

  // Keep track of the checkpoints the process will save.
  if (aMsg.mType == MessageType::SetSaveCheckpoint) {
    const SetSaveCheckpointMessage& nmsg = static_cast<const SetSaveCheckpointMessage&>(aMsg);
    MOZ_RELEASE_ASSERT(nmsg.mCheckpoint > MostRecentCheckpoint());
    VectorAddOrRemoveEntry(mShouldSaveCheckpoints, nmsg.mCheckpoint, nmsg.mSave);
  }

  SendMessageRaw(aMsg);
}

void
ChildProcessInfo::SendMessageRaw(const Message& aMsg)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mLastMessageTime = TimeStamp::Now();
  mChannel->SendMessage(aMsg);
}

void
ChildProcessInfo::Recover(bool aPaused, Message* aPausedMessage, size_t aLastCheckpoint,
                          Message** aMessages, size_t aNumMessages)
{
  MOZ_RELEASE_ASSERT(IsPaused());

  SendMessageRaw(SetIsActiveMessage(false));

  size_t mostRecentCheckpoint = MostRecentCheckpoint();
  bool pausedAtCheckpoint = IsPausedAtCheckpoint();

  // Clear out all messages that have been sent to this process.
  for (Message* msg : mMessages) {
    if (msg->mType == MessageType::SetBreakpoint) {
      SetBreakpointMessage* nmsg = static_cast<SetBreakpointMessage*>(msg);
      SendMessageRaw(SetBreakpointMessage(nmsg->mId, js::BreakpointPosition()));
    }
    free(msg);
  }
  mMessages.clear();

  mPaused = aPaused;
  mPausedMessage = aPausedMessage;
  mLastCheckpoint = aLastCheckpoint;
  for (size_t i = 0; i < aNumMessages; i++) {
    mMessages.append(aMessages[i]->Clone());
  }

  mNumRecoveredMessages = 0;

  if (mostRecentCheckpoint < mLastCheckpoint) {
    mRecoveryStage = RecoveryStage::ReachingCheckpoint;
    SendMessageRaw(ResumeMessage(/* aForward = */ true));
  } else if (mostRecentCheckpoint > mLastCheckpoint || !pausedAtCheckpoint) {
    mRecoveryStage = RecoveryStage::ReachingCheckpoint;
    // Rewind to the last saved checkpoint at or prior to the target.
    size_t targetCheckpoint = CheckpointId::Invalid;
    for (size_t saved : mShouldSaveCheckpoints) {
      if (saved <= mLastCheckpoint && saved > targetCheckpoint) {
        targetCheckpoint = saved;
      }
    }
    MOZ_RELEASE_ASSERT(targetCheckpoint != CheckpointId::Invalid);
    SendMessageRaw(RestoreCheckpointMessage(targetCheckpoint));
  } else {
    mRecoveryStage = RecoveryStage::PlayingMessages;
    SendNextRecoveryMessage();
  }

  WaitUntil([=]() { return !IsRecovering(); });
}

void
ChildProcessInfo::Recover(ChildProcessInfo* aTargetProcess)
{
  MOZ_RELEASE_ASSERT(aTargetProcess->IsPaused());
  Recover(true, aTargetProcess->mPausedMessage->Clone(),
          aTargetProcess->mLastCheckpoint,
          aTargetProcess->mMessages.begin(), aTargetProcess->mMessages.length());
}

void
ChildProcessInfo::RecoverToCheckpoint(size_t aCheckpoint)
{
  HitCheckpointMessage pausedMessage(aCheckpoint,
                                     /* aRecordingEndpoint = */ false,
                                     /* aDuration = */ 0);
  Recover(true, pausedMessage.Clone(), aCheckpoint, nullptr, 0);
}

void
ChildProcessInfo::OnIncomingRecoveryMessage(const Message& aMsg)
{
  switch (aMsg.mType) {
  case MessageType::HitCheckpoint: {
    MOZ_RELEASE_ASSERT(mRecoveryStage == RecoveryStage::ReachingCheckpoint);
    const HitCheckpointMessage& nmsg = static_cast<const HitCheckpointMessage&>(aMsg);
    if (nmsg.mCheckpointId < mLastCheckpoint) {
      SendMessageRaw(ResumeMessage(/* aForward = */ true));
    } else {
      MOZ_RELEASE_ASSERT(nmsg.mCheckpointId == mLastCheckpoint);
      mRecoveryStage = RecoveryStage::PlayingMessages;
      SendNextRecoveryMessage();
    }
    break;
  }
  case MessageType::HitBreakpoint:
  case MessageType::DebuggerResponse:
    SendNextRecoveryMessage();
    break;
  default:
    MOZ_CRASH("Unexpected message during recovery");
  }
}

void
ChildProcessInfo::SendNextRecoveryMessage()
{
  MOZ_RELEASE_ASSERT(mRecoveryStage == RecoveryStage::PlayingMessages);

  // Keep sending messages to the child as long as it stays paused.
  Message* msg;
  do {
    // Check if we have recovered to the desired paused state.
    if (mNumRecoveredMessages == mMessages.length()) {
      MOZ_RELEASE_ASSERT(IsPaused());
      mRecoveryStage = RecoveryStage::None;
      return;
    }
    msg = mMessages[mNumRecoveredMessages++];
    SendMessageRaw(*msg);

    // If we just sent a SetBreakpoint message then the child process is still
    // paused, so keep sending more messages.
  } while (msg->mType == MessageType::SetBreakpoint);

  // If we have sent all messages and are in an unpaused state, we are done
  // recovering.
  if (mNumRecoveredMessages == mMessages.length() && !IsPaused()) {
    mRecoveryStage = RecoveryStage::None;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Subprocess Management
///////////////////////////////////////////////////////////////////////////////

ipc::GeckoChildProcessHost* gRecordingProcess;

void
GetArgumentsForChildProcess(base::ProcessId aMiddlemanPid, uint32_t aChannelId,
                            const char* aRecordingFile, bool aRecording,
                            std::vector<std::string>& aExtraArgs)
{
  MOZ_RELEASE_ASSERT(IsMiddleman() || XRE_IsParentProcess());

  aExtraArgs.push_back(gMiddlemanPidOption);
  aExtraArgs.push_back(nsPrintfCString("%d", aMiddlemanPid).get());

  aExtraArgs.push_back(gChannelIDOption);
  aExtraArgs.push_back(nsPrintfCString("%d", (int) aChannelId).get());

  aExtraArgs.push_back(gProcessKindOption);
  aExtraArgs.push_back(nsPrintfCString("%d", aRecording
                                             ? (int) ProcessKind::Recording
                                             : (int) ProcessKind::Replaying).get());

  aExtraArgs.push_back(gRecordingFileOption);
  aExtraArgs.push_back(aRecordingFile);
}

void
ChildProcessInfo::LaunchSubprocess()
{
  size_t channelId = gNumChannels++;
  MOZ_RELEASE_ASSERT((channelId == Channel::RecordingId) == IsRecording());

  // Create a new channel every time we launch a new subprocess, without
  // deleting or tearing down the old one's state. This is pretty lame and it
  // would be nice if we could do something better here, especially because
  // with restarts we could create any number of channels over time.
  mChannel = new Channel(channelId, [=](Message* aMsg) {
      ReceiveChildMessageOnMainThread(channelId, aMsg);
    });

  if (IsRecording()) {
    std::vector<std::string> extraArgs;
    GetArgumentsForChildProcess(base::GetCurrentProcId(), channelId,
                                gRecordingFilename, /* aRecording = */ true, extraArgs);

    MOZ_RELEASE_ASSERT(!gRecordingProcess);
    gRecordingProcess = new ipc::GeckoChildProcessHost(GeckoProcessType_Content);
    if (!gRecordingProcess->LaunchAndWaitForProcessHandle(extraArgs)) {
      MOZ_CRASH("ChildProcessInfo::LaunchSubprocess");
    }
  } else {
    dom::ContentChild::GetSingleton()->SendCreateReplayingProcess(channelId);
  }

  mLastMessageTime = TimeStamp::Now();

  SendGraphicsMemoryToChild();

  // The child should send us a HitCheckpoint with an invalid ID to pause.
  WaitUntilPaused();

  MOZ_RELEASE_ASSERT(gIntroductionMessage);
  SendMessage(*gIntroductionMessage);
}

///////////////////////////////////////////////////////////////////////////////
// Recovering Crashed / Hung Children
///////////////////////////////////////////////////////////////////////////////

// The number of times we will restart a process before giving up.
static const size_t MaxRestarts = 5;

bool
ChildProcessInfo::CanRestart()
{
  return gRestartEnabled
      && !IsRecording()
      && !IsPaused()
      && !IsRecovering()
      && mNumRestarts < MaxRestarts;
}

void
ChildProcessInfo::AttemptRestart(const char* aWhy)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  PrintSpew("Warning: Child process died [%d]: %s\n", (int) GetId(), aWhy);

  if (!CanRestart()) {
    nsAutoCString why(aWhy);
    dom::ContentChild::GetSingleton()->SendRecordReplayFatalError(why);
    Thread::WaitForeverNoIdle();
  }

  mNumRestarts++;

  dom::ContentChild::GetSingleton()->SendTerminateReplayingProcess(mChannel->GetId());

  bool newPaused = mPaused;
  Message* newPausedMessage = mPausedMessage;

  mPaused = false;
  mPausedMessage = nullptr;

  size_t newLastCheckpoint = mLastCheckpoint;
  mLastCheckpoint = CheckpointId::Invalid;

  InfallibleVector<Message*> newMessages;
  newMessages.append(mMessages.begin(), mMessages.length());
  mMessages.clear();

  InfallibleVector<size_t> newShouldSaveCheckpoints;
  newShouldSaveCheckpoints.append(mShouldSaveCheckpoints.begin(), mShouldSaveCheckpoints.length());
  mShouldSaveCheckpoints.clear();

  LaunchSubprocess();

  // Disallow child processes from intentionally crashing after restarting.
  SendMessage(SetAllowIntentionalCrashesMessage(false));

  for (size_t checkpoint : newShouldSaveCheckpoints) {
    SendMessage(SetSaveCheckpointMessage(checkpoint, true));
  }

  Recover(newPaused, newPausedMessage, newLastCheckpoint,
          newMessages.begin(), newMessages.length());
}

///////////////////////////////////////////////////////////////////////////////
// Handling Channel Messages
///////////////////////////////////////////////////////////////////////////////

// When messages are received from child processes, we want their handler to
// execute on the main thread. The main thread might be blocked in WaitUntil,
// so runnables associated with child processes have special handling.

// All messages received on a channel thread which the main thread has not
// processed yet. This is protected by gMonitor.
struct PendingMessage
{
  ChildProcessInfo* mProcess;
  size_t mChannelId;
  Message* mMsg;
};
static StaticInfallibleVector<PendingMessage> gPendingMessages;

// Whether there is a pending task on the main thread's message loop to handle
// all pending messages.
static bool gHasPendingMessageRunnable;

// Process a pending message from aProcess (or any process if aProcess is null)
// and return whether such a message was found. This must be called on the main
// thread with gMonitor held.
/* static */ bool
ChildProcessInfo::MaybeProcessPendingMessage(ChildProcessInfo* aProcess)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  for (size_t i = 0; i < gPendingMessages.length(); i++) {
    if (!aProcess || gPendingMessages[i].mProcess == aProcess) {
      PendingMessage copy = gPendingMessages[i];
      gPendingMessages.erase(&gPendingMessages[i]);

      MonitorAutoUnlock unlock(*gMonitor);
      copy.mProcess->OnIncomingMessage(copy.mChannelId, *copy.mMsg);
      free(copy.mMsg);
      return true;
    }
  }

  return false;
}

// How many seconds to wait without hearing from an unpaused child before
// considering that child to be hung.
static const size_t HangSeconds = 5;

void
ChildProcessInfo::WaitUntil(const std::function<bool()>& aCallback)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  while (!aCallback()) {
    MonitorAutoLock lock(*gMonitor);
    if (!MaybeProcessPendingMessage(this)) {
      if (gChildrenAreDebugging) {
        // Don't watch for hangs when children are being debugged.
        gMonitor->Wait();
      } else {
        TimeStamp deadline = mLastMessageTime + TimeDuration::FromSeconds(HangSeconds);
        if (TimeStamp::Now() >= deadline) {
          MonitorAutoUnlock unlock(*gMonitor);
          AttemptRestart("Child process non-responsive");
        }
        gMonitor->WaitUntil(deadline);
      }
    }
  }
}

// Runnable created on the main thread to handle any tasks sent by the replay
// message loop thread which were not handled while the main thread was blocked.
/* static */ void
ChildProcessInfo::MaybeProcessPendingMessageRunnable()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MonitorAutoLock lock(*gMonitor);
  MOZ_RELEASE_ASSERT(gHasPendingMessageRunnable);
  gHasPendingMessageRunnable = false;
  while (MaybeProcessPendingMessage(nullptr)) {}
}

// Execute a task that processes a message received from the child. This is
// called on a channel thread, and the function executes asynchronously on
// the main thread.
void
ChildProcessInfo::ReceiveChildMessageOnMainThread(size_t aChannelId, Message* aMsg)
{
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());

  MonitorAutoLock lock(*gMonitor);

  PendingMessage pending;
  pending.mProcess = this;
  pending.mChannelId = aChannelId;
  pending.mMsg = aMsg;
  gPendingMessages.append(pending);

  // Notify the main thread, if it is waiting in WaitUntil.
  gMonitor->NotifyAll();

  // Make sure there is a task on the main thread's message loop that can
  // process this task if necessary.
  if (!gHasPendingMessageRunnable) {
    gHasPendingMessageRunnable = true;
    MainThreadMessageLoop()->PostTask(NewRunnableFunction("MaybeProcessPendingMessageRunnable",
                                                          MaybeProcessPendingMessageRunnable));
  }
}

} // namespace parent
} // namespace recordreplay
} // namespace mozilla
