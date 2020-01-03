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

/* static */
void ChildProcessInfo::SetIntroductionMessage(IntroductionMessage* aMessage) {
  gIntroductionMessage = aMessage;
}

ChildProcessInfo::ChildProcessInfo(
    const Maybe<RecordingProcessData>& aRecordingProcessData)
    : mRecording(aRecordingProcessData.isSome()) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  static bool gFirst = false;
  if (!gFirst) {
    gFirst = true;
    gChildrenAreDebugging = !!getenv("MOZ_REPLAYING_WAIT_AT_START");
  }

  LaunchSubprocess(aRecordingProcessData);
}

ChildProcessInfo::~ChildProcessInfo() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (IsRecording() && !HasCrashed()) {
    SendMessage(TerminateMessage());
  }
}

void ChildProcessInfo::OnIncomingMessage(const Message& aMsg) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  switch (aMsg.mType) {
    case MessageType::BeginFatalError:
      mHasBegunFatalError = true;
      return;
    case MessageType::FatalError: {
      mHasFatalError = true;
      const FatalErrorMessage& nmsg =
          static_cast<const FatalErrorMessage&>(aMsg);
      OnCrash(nmsg.Error());
      return;
    }
    case MessageType::Paint:
      UpdateGraphicsAfterPaint(static_cast<const PaintMessage&>(aMsg));
      break;
    case MessageType::ManifestFinished:
      mPaused = true;
      js::ForwardManifestFinished(this, aMsg);
      break;
    case MessageType::MiddlemanCallRequest: {
      const MiddlemanCallRequestMessage& nmsg =
          static_cast<const MiddlemanCallRequestMessage&>(aMsg);
      InfallibleVector<char> outputData;
      ProcessMiddlemanCall(GetId(), nmsg.BinaryData(), nmsg.BinaryDataSize(),
                           &outputData);
      Message::UniquePtr response(MiddlemanCallResponseMessage::New(
          outputData.begin(), outputData.length()));
      SendMessage(std::move(*response));
      break;
    }
    case MessageType::ResetMiddlemanCalls:
      ResetMiddlemanCalls(GetId());
      break;
    case MessageType::PingResponse:
      OnPingResponse(static_cast<const PingResponseMessage&>(aMsg));
      break;
    default:
      break;
  }
}

void ChildProcessInfo::SendMessage(Message&& aMsg) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(!HasCrashed());

  // Update paused state.
  MOZ_RELEASE_ASSERT(IsPaused() || aMsg.CanBeSentWhileUnpaused());
  if (aMsg.mType == MessageType::ManifestStart) {
    mPaused = false;
  }

  mChannel->SendMessage(std::move(aMsg));
}

///////////////////////////////////////////////////////////////////////////////
// Subprocess Management
///////////////////////////////////////////////////////////////////////////////

ipc::GeckoChildProcessHost* gRecordingProcess;

void GetArgumentsForChildProcess(base::ProcessId aMiddlemanPid,
                                 uint32_t aChannelId,
                                 const char* aRecordingFile, bool aRecording,
                                 std::vector<std::string>& aExtraArgs) {
  MOZ_RELEASE_ASSERT(IsMiddleman() || XRE_IsParentProcess());

  aExtraArgs.push_back(gMiddlemanPidOption);
  aExtraArgs.push_back(nsPrintfCString("%d", aMiddlemanPid).get());

  aExtraArgs.push_back(gChannelIDOption);
  aExtraArgs.push_back(nsPrintfCString("%d", (int)aChannelId).get());

  aExtraArgs.push_back(gProcessKindOption);
  aExtraArgs.push_back(nsPrintfCString("%d", aRecording
                                                 ? (int)ProcessKind::Recording
                                                 : (int)ProcessKind::Replaying)
                           .get());

  aExtraArgs.push_back(gRecordingFileOption);
  aExtraArgs.push_back(aRecordingFile);
}

void ChildProcessInfo::LaunchSubprocess(
    const Maybe<RecordingProcessData>& aRecordingProcessData) {
  size_t channelId = gNumChannels++;

  // Create a new channel every time we launch a new subprocess, without
  // deleting or tearing down the old one's state. This is pretty silly and it
  // would be nice if we could do something better here, especially because
  // with restarts we could create any number of channels over time.
  mChannel =
      new Channel(channelId, IsRecording(), [=](Message::UniquePtr aMsg) {
        ReceiveChildMessageOnMainThread(std::move(aMsg));
      });

  MOZ_RELEASE_ASSERT(IsRecording() == aRecordingProcessData.isSome());
  if (IsRecording()) {
    std::vector<std::string> extraArgs;
    GetArgumentsForChildProcess(base::GetCurrentProcId(), channelId,
                                gRecordingFilename, /* aRecording = */ true,
                                extraArgs);

    MOZ_RELEASE_ASSERT(!gRecordingProcess);
    gRecordingProcess =
        new ipc::GeckoChildProcessHost(GeckoProcessType_Content);

    // Preferences data is conveyed to the recording process via fixed file
    // descriptors on macOS.
    gRecordingProcess->AddFdToRemap(aRecordingProcessData.ref().mPrefsHandle.fd,
                                    kPrefsFileDescriptor);
    ipc::FileDescriptor::UniquePlatformHandle prefMapHandle =
        aRecordingProcessData.ref().mPrefMapHandle.ClonePlatformHandle();
    gRecordingProcess->AddFdToRemap(prefMapHandle.get(),
                                    kPrefMapFileDescriptor);

    if (!gRecordingProcess->LaunchAndWaitForProcessHandle(extraArgs)) {
      MOZ_CRASH("ChildProcessInfo::LaunchSubprocess");
    }
  } else {
    dom::ContentChild::GetSingleton()->SendCreateReplayingProcess(channelId);
  }

  SendGraphicsMemoryToChild();

  MOZ_RELEASE_ASSERT(gIntroductionMessage);
  SendMessage(std::move(*gIntroductionMessage));
}

void ChildProcessInfo::OnCrash(const char* aWhy) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // If a child process crashes or hangs then annotate the crash report.
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::RecordReplayError, nsAutoCString(aWhy));

  if (!IsRecording()) {
    // Notify the parent when a replaying process crashes so that a report can
    // be generated.
    dom::ContentChild::GetSingleton()->SendGenerateReplayCrashReport(GetId());

    // Continue execution if we were able to recover from the crash.
    if (js::RecoverFromCrash(this)) {
      // Mark this child as crashed so it can't be used again, even if it didn't
      // generate a minidump.
      mHasFatalError = true;
      return;
    }
  }

  // If we received a FatalError message then the child generated a minidump.
  // Shut down cleanly so that we don't mask the report with our own crash.
  if (mHasFatalError) {
    Shutdown();
  }

  // Indicate when we crash if the child tried to send us a fatal error message
  // but had a problem either unprotecting system memory or generating the
  // minidump.
  MOZ_RELEASE_ASSERT(!mHasBegunFatalError);

  // The child crashed without producing a minidump, produce one ourselves.
  MOZ_CRASH("Unexpected child crash");
}

///////////////////////////////////////////////////////////////////////////////
// Handling Channel Messages
///////////////////////////////////////////////////////////////////////////////

// When messages are received from child processes, we want their handler to
// execute on the main thread. The main thread might be blocked in WaitUntil,
// so runnables associated with child processes have special handling.

// All messages received on a channel thread which the main thread has not
// processed yet. This is protected by gMonitor.
struct PendingMessage {
  ChildProcessInfo* mProcess;
  Message::UniquePtr mMsg;

  PendingMessage() : mProcess(nullptr) {}

  PendingMessage& operator=(PendingMessage&& aOther) {
    mProcess = aOther.mProcess;
    mMsg = std::move(aOther.mMsg);
    return *this;
  }

  PendingMessage(PendingMessage&& aOther) { *this = std::move(aOther); }
};
static StaticInfallibleVector<PendingMessage> gPendingMessages;

static Message::UniquePtr ExtractChildMessage(ChildProcessInfo** aProcess) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  for (size_t i = 0; i < gPendingMessages.length(); i++) {
    PendingMessage& pending = gPendingMessages[i];
    if (!*aProcess || pending.mProcess == *aProcess) {
      *aProcess = pending.mProcess;
      Message::UniquePtr msg = std::move(pending.mMsg);
      gPendingMessages.erase(&pending);
      return msg;
    }
  }

  return nullptr;
}

// Hang Detection
//
// Replaying processes will be terminated if no execution progress has been made
// for some number of seconds. This generates a crash report for diagnosis and
// allows another replaying process to be spawned in its place. We detect that
// no progress is being made by periodically sending ping messages to the
// replaying process, and comparing the progress values returned by them.
// The ping messages are sent at least PingIntervalSeconds apart, and the
// process is considered hanged if at any point the last MaxStalledPings ping
// messages either did not respond or responded with the same progress value.
//
// Dividing our accounting between different ping messages avoids treating
// processes as hanged when the computer wakes up after sleeping: no pings will
// be sent while the computer is sleeping and processes are suspended, so the
// computer will need to be awake for some time before any processes are marked
// as hanged (before which they will hopefully be able to make progress).

static const size_t PingIntervalSeconds = 2;
static const size_t MaxStalledPings = 10;

bool ChildProcessInfo::IsHanged() {
  if (mPings.length() < MaxStalledPings) {
    return false;
  }

  size_t firstIndex = mPings.length() - MaxStalledPings;
  uint64_t firstValue = mPings[firstIndex].mProgress;
  if (!firstValue) {
    // The child hasn't responded to any of the pings.
    return true;
  }

  for (size_t i = firstIndex; i < mPings.length(); i++) {
    if (mPings[i].mProgress && mPings[i].mProgress != firstValue) {
      return false;
    }
  }

  return true;
}

void ChildProcessInfo::ResetPings(bool aMightRewind) {
  mMightRewind = aMightRewind;
  mPings.clear();
  mLastPingTime = TimeStamp::Now();
}

static uint32_t gNumPings;

void ChildProcessInfo::MaybePing() {
  if (IsRecording() || IsPaused() || gChildrenAreDebugging) {
    return;
  }

  TimeStamp now = TimeStamp::Now();

  // After sending a terminate message we don't ping the process anymore, just
  // make sure it eventually crashes instead of continuing to hang. Use this as
  // a fallback if we can't ping the process because it might be rewinding.
  if (mSentTerminateMessage || mMightRewind) {
    size_t TerminateSeconds = PingIntervalSeconds * MaxStalledPings;
    if (now >= mLastPingTime + TimeDuration::FromSeconds(TerminateSeconds)) {
      OnCrash(mMightRewind ? "Rewinding child process non-responsive"
                           : "Child process non-responsive");
    }
    return;
  }

  if (now < mLastPingTime + TimeDuration::FromSeconds(PingIntervalSeconds)) {
    return;
  }

  if (IsHanged()) {
    // Try to get the child to crash, so that we can get a minidump.
    CrashReporter::AnnotateCrashReport(
        CrashReporter::Annotation::RecordReplayHang, true);
    SendMessage(TerminateMessage());
    mSentTerminateMessage = true;
  } else {
    uint32_t id = ++gNumPings;
    mPings.emplaceBack(id);
    SendMessage(PingMessage(id));
  }

  mLastPingTime = now;
}

void ChildProcessInfo::OnPingResponse(const PingResponseMessage& aMsg) {
  for (size_t i = 0; i < mPings.length(); i++) {
    if (mPings[i].mId == aMsg.mId) {
      mPings[i].mProgress = aMsg.mProgress;
      break;
    }
  }
}

void ChildProcessInfo::WaitUntilPaused() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  while (!IsPaused()) {
    MaybePing();

    Maybe<MonitorAutoLock> lock;
    lock.emplace(*gMonitor);

    MaybeHandlePendingSyncMessage();

    // Search for the first message received from this process.
    ChildProcessInfo* process = this;
    Message::UniquePtr msg = ExtractChildMessage(&process);

    if (msg) {
      lock.reset();
      OnIncomingMessage(*msg);
    } else if (HasCrashed()) {
      // If the child crashed but we recovered, we don't have to keep waiting.
      break;
    } else {
      TimeStamp deadline =
          TimeStamp::Now() + TimeDuration::FromSeconds(PingIntervalSeconds);
      gMonitor->WaitUntil(deadline);
    }
  }
}

// Whether there is a pending task on the main thread's message loop to handle
// all pending messages.
static bool gHasPendingMessageRunnable;

// Runnable created on the main thread to handle any tasks sent by the replay
// message loop thread which were not handled while the main thread was blocked.
/* static */
void ChildProcessInfo::MaybeProcessPendingMessageRunnable() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MonitorAutoLock lock(*gMonitor);
  MOZ_RELEASE_ASSERT(gHasPendingMessageRunnable);
  gHasPendingMessageRunnable = false;
  while (true) {
    ChildProcessInfo* process = nullptr;
    Message::UniquePtr msg = ExtractChildMessage(&process);

    if (msg) {
      MonitorAutoUnlock unlock(*gMonitor);
      process->OnIncomingMessage(*msg);
    } else {
      break;
    }
  }
}

// Execute a task that processes a message received from the child. This is
// called on a channel thread, and the function executes asynchronously on
// the main thread.
void ChildProcessInfo::ReceiveChildMessageOnMainThread(
    Message::UniquePtr aMsg) {
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());

  MonitorAutoLock lock(*gMonitor);

  PendingMessage pending;
  pending.mProcess = this;
  pending.mMsg = std::move(aMsg);
  gPendingMessages.append(std::move(pending));

  // Notify the main thread, if it is waiting in WaitUntilPaused.
  gMonitor->NotifyAll();

  // Make sure there is a task on the main thread's message loop that can
  // process this task if necessary.
  if (!gHasPendingMessageRunnable) {
    gHasPendingMessageRunnable = true;
    MainThreadMessageLoop()->PostTask(
        NewRunnableFunction("MaybeProcessPendingMessageRunnable",
                            MaybeProcessPendingMessageRunnable));
  }
}

}  // namespace parent
}  // namespace recordreplay
}  // namespace mozilla
