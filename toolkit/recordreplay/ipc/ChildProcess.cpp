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
    : mChannel(nullptr),
      mRecording(aRecordingProcessData.isSome()),
      mPaused(false),
      mHasBegunFatalError(false),
      mHasFatalError(false) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  static bool gFirst = false;
  if (!gFirst) {
    gFirst = true;
    gChildrenAreDebugging = !!getenv("WAIT_AT_START");
  }

  LaunchSubprocess(aRecordingProcessData);
}

ChildProcessInfo::~ChildProcessInfo() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (IsRecording()) {
    SendMessage(TerminateMessage());
  }
}

void ChildProcessInfo::OnIncomingMessage(const Message& aMsg) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mLastMessageTime = TimeStamp::Now();

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
      UpdateGraphicsInUIProcess(&static_cast<const PaintMessage&>(aMsg));
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
    default:
      break;
  }
}

void ChildProcessInfo::SendMessage(Message&& aMsg) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Update paused state.
  MOZ_RELEASE_ASSERT(IsPaused() || aMsg.CanBeSentWhileUnpaused());
  if (aMsg.mType == MessageType::ManifestStart) {
    mPaused = false;
  }

  mLastMessageTime = TimeStamp::Now();
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
  // deleting or tearing down the old one's state. This is pretty lame and it
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

  mLastMessageTime = TimeStamp::Now();

  SendGraphicsMemoryToChild();

  MOZ_RELEASE_ASSERT(gIntroductionMessage);
  SendMessage(std::move(*gIntroductionMessage));
}

void ChildProcessInfo::OnCrash(const char* aWhy) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // If a child process crashes or hangs then annotate the crash report.
  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::RecordReplayError, nsAutoCString(aWhy));

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

// Whether there is a pending task on the main thread's message loop to handle
// all pending messages.
static bool gHasPendingMessageRunnable;

// How many seconds to wait without hearing from an unpaused child before
// considering that child to be hung.
static const size_t HangSeconds = 30;

void ChildProcessInfo::WaitUntilPaused() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (IsPaused()) {
    return;
  }

  bool sentTerminateMessage = false;
  while (true) {
    MonitorAutoLock lock(*gMonitor);

    // Search for the first message received from this process.
    ChildProcessInfo* process = this;
    Message::UniquePtr msg = ExtractChildMessage(&process);

    if (msg) {
      OnIncomingMessage(*msg);
      if (IsPaused()) {
        return;
      }
    } else {
      if (gChildrenAreDebugging || IsRecording()) {
        // Don't watch for hangs when children are being debugged. Recording
        // children are never treated as hanged both because they cannot be
        // restarted and because they may just be idling.
        gMonitor->Wait();
      } else {
        TimeStamp deadline =
            mLastMessageTime + TimeDuration::FromSeconds(HangSeconds);
        if (TimeStamp::Now() >= deadline) {
          MonitorAutoUnlock unlock(*gMonitor);
          if (!sentTerminateMessage) {
            // Try to get the child to crash, so that we can get a minidump.
            // Sending the message will reset mLastMessageTime so we get to
            // wait another HangSeconds before hitting the restart case below.
            // Use SendMessageRaw to avoid problems if we are recovering.
            CrashReporter::AnnotateCrashReport(
                CrashReporter::Annotation::RecordReplayHang, true);
            SendMessage(TerminateMessage());
            sentTerminateMessage = true;
          } else {
            // The child is still non-responsive after sending the terminate
            // message.
            OnCrash("Child process non-responsive");
          }
        }
        gMonitor->WaitUntil(deadline);
      }
    }
  }
}

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
