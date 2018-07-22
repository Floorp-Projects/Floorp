/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file has the logic which the replayed process uses to communicate with
// the middleman process.

#include "ChildIPC.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/mach_ipc_mac.h"
#include "ipc/Channel.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/Sprintf.h"
#include "mozilla/VsyncDispatcher.h"

#include "InfallibleVector.h"
#include "MemorySnapshot.h"
#include "Monitor.h"
#include "ProcessRecordReplay.h"
#include "ProcessRedirect.h"
#include "ProcessRewind.h"
#include "Thread.h"
#include "Units.h"

#include <algorithm>
#include <mach/mach_vm.h>
#include <unistd.h>

namespace mozilla {
namespace recordreplay {
namespace child {

///////////////////////////////////////////////////////////////////////////////
// Record/Replay IPC
///////////////////////////////////////////////////////////////////////////////

// Monitor used for various synchronization tasks.
static Monitor* gMonitor;

// The singleton channel for communicating with the middleman.
Channel* gChannel;

static base::ProcessId gMiddlemanPid;
static base::ProcessId gParentPid;
static StaticInfallibleVector<char*> gParentArgv;

static char* gShmemPrefs;
static size_t gShmemPrefsLen;

// File descriptors used by a pipe to create checkpoints when instructed by the
// parent process.
static FileHandle gCheckpointWriteFd;
static FileHandle gCheckpointReadFd;

// Copy of the introduction message we got from the middleman. This is saved on
// receipt and then processed during InitRecordingOrReplayingProcess.
static IntroductionMessage* gIntroductionMessage;

// Processing routine for incoming channel messages.
static void
ChannelMessageHandler(Message* aMsg)
{
  MOZ_RELEASE_ASSERT(MainThreadShouldPause() ||
                     aMsg->mType == MessageType::CreateCheckpoint ||
                     aMsg->mType == MessageType::Terminate);

  switch (aMsg->mType) {
  case MessageType::Introduction: {
    MOZ_RELEASE_ASSERT(!gIntroductionMessage);
    gIntroductionMessage = (IntroductionMessage*) aMsg->Clone();
    break;
  }
  case MessageType::CreateCheckpoint: {
    MOZ_RELEASE_ASSERT(IsRecording());
    uint8_t data = 0;
    DirectWrite(gCheckpointWriteFd, &data, 1);
    break;
  }
  case MessageType::Terminate: {
    PrintSpew("Terminate message received, exiting...\n");
    MOZ_RELEASE_ASSERT(IsRecording());
    _exit(0);
  }
  case MessageType::SetIsActive: {
    const SetIsActiveMessage& nmsg = (const SetIsActiveMessage&) *aMsg;
    PauseMainThreadAndInvokeCallback([=]() { SetIsActiveChild(nmsg.mActive); });
    break;
  }
  case MessageType::SetAllowIntentionalCrashes: {
    const SetAllowIntentionalCrashesMessage& nmsg = (const SetAllowIntentionalCrashesMessage&) *aMsg;
    PauseMainThreadAndInvokeCallback([=]() { SetAllowIntentionalCrashes(nmsg.mAllowed); });
    break;
  }
  case MessageType::SetSaveCheckpoint: {
    const SetSaveCheckpointMessage& nmsg = (const SetSaveCheckpointMessage&) *aMsg;
    PauseMainThreadAndInvokeCallback([=]() { SetSaveCheckpoint(nmsg.mCheckpoint, nmsg.mSave); });
    break;
  }
  case MessageType::FlushRecording: {
    PauseMainThreadAndInvokeCallback(FlushRecording);
    break;
  }
  case MessageType::DebuggerRequest: {
    const DebuggerRequestMessage& nmsg = (const DebuggerRequestMessage&) *aMsg;
    JS::replay::CharBuffer* buf = js_new<JS::replay::CharBuffer>();
    if (!buf->append(nmsg.Buffer(), nmsg.BufferSize())) {
      MOZ_CRASH();
    }
    PauseMainThreadAndInvokeCallback([=]() { JS::replay::hooks.debugRequestReplay(buf); });
    break;
  }
  case MessageType::SetBreakpoint: {
    const SetBreakpointMessage& nmsg = (const SetBreakpointMessage&) *aMsg;
    PauseMainThreadAndInvokeCallback([=]() {
        JS::replay::hooks.setBreakpointReplay(nmsg.mId, nmsg.mPosition);
      });
    break;
  }
  case MessageType::Resume: {
    const ResumeMessage& nmsg = (const ResumeMessage&) *aMsg;
    PauseMainThreadAndInvokeCallback([=]() {
        // Inform the debugger about the request to resume execution. The hooks
        // will not have been set yet for the primordial resume, in which case
        // just continue executing forward.
        if (JS::replay::hooks.resumeReplay) {
          JS::replay::hooks.resumeReplay(nmsg.mForward);
        } else {
          ResumeExecution();
        }
      });
    break;
  }
  case MessageType::RestoreCheckpoint: {
    const RestoreCheckpointMessage& nmsg = (const RestoreCheckpointMessage&) *aMsg;
    PauseMainThreadAndInvokeCallback([=]() {
        JS::replay::hooks.restoreCheckpointReplay(nmsg.mCheckpoint);
      });
    break;
  }
  default:
    MOZ_CRASH();
  }

  free(aMsg);
}

char*
PrefsShmemContents(size_t aPrefsLen)
{
  MOZ_RELEASE_ASSERT(aPrefsLen == gShmemPrefsLen);
  return gShmemPrefs;
}

// Initialize hooks used by the replay debugger.
static void InitDebuggerHooks();

static void HitCheckpoint(size_t aId, bool aRecordingEndpoint);

// Main routine for a thread whose sole purpose is to listen to requests from
// the middleman process to create a new checkpoint. This is separate from the
// channel thread because this thread is recorded and the latter is not
// recorded. By communicating between the two threads with a pipe, this
// thread's behavior will be replicated exactly when replaying and new
// checkpoints will be created at the same point as during recording.
static void
ListenForCheckpointThreadMain(void*)
{
  while (true) {
    uint8_t data = 0;
    ssize_t rv = read(gCheckpointReadFd, &data, 1);
    if (rv > 0) {
      NS_DispatchToMainThread(NewRunnableFunction("NewCheckpoint", NewCheckpoint,
                                                  /* aTemporary = */ false));
    } else {
      MOZ_RELEASE_ASSERT(errno == EINTR);
    }
  }
}

void* gGraphicsShmem;

void
InitRecordingOrReplayingProcess(int* aArgc, char*** aArgv)
{
  if (!IsRecordingOrReplaying()) {
    return;
  }

  Maybe<int> middlemanPid;
  Maybe<int> channelID;
  for (int i = 0; i < *aArgc; i++) {
    if (!strcmp((*aArgv)[i], gMiddlemanPidOption)) {
      MOZ_RELEASE_ASSERT(middlemanPid.isNothing() && i + 1 < *aArgc);
      middlemanPid.emplace(atoi((*aArgv)[i + 1]));
    }
    if (!strcmp((*aArgv)[i], gChannelIDOption)) {
      MOZ_RELEASE_ASSERT(channelID.isNothing() && i + 1 < *aArgc);
      channelID.emplace(atoi((*aArgv)[i + 1]));
    }
  }
  MOZ_RELEASE_ASSERT(middlemanPid.isSome());
  MOZ_RELEASE_ASSERT(channelID.isSome());

  gMiddlemanPid = middlemanPid.ref();

  Maybe<AutoPassThroughThreadEvents> pt;
  pt.emplace();

  gMonitor = new Monitor();
  gChannel = new Channel(channelID.ref(), ChannelMessageHandler);

  pt.reset();

  DirectCreatePipe(&gCheckpointWriteFd, &gCheckpointReadFd);

  Thread::StartThread(ListenForCheckpointThreadMain, nullptr, false);

  InitDebuggerHooks();

  pt.emplace();

  // Setup a mach port to receive the graphics shmem handle over.
  char portName[128];
  SprintfLiteral(portName, "WebReplay.%d.%d", gMiddlemanPid, channelID.ref());
  ReceivePort receivePort(portName);

  pt.reset();

  // We are ready to receive initialization messages from the middleman, pause
  // so they can be sent.
  HitCheckpoint(InvalidCheckpointId, /* aRecordingEndpoint = */ false);

  pt.emplace();

  // The parent should have sent us a handle to the graphics shmem.
  MachReceiveMessage message;
  kern_return_t kr = receivePort.WaitForMessage(&message, 0);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);
  MOZ_RELEASE_ASSERT(message.GetMessageID() == GraphicsMessageId);
  mach_port_t graphicsPort = message.GetTranslatedPort(0);
  MOZ_RELEASE_ASSERT(graphicsPort != MACH_PORT_NULL);

  mach_vm_address_t address = 0;
  kr = mach_vm_map(mach_task_self(), &address, GraphicsMemorySize, 0, VM_FLAGS_ANYWHERE,
                   graphicsPort, 0, false,
                   VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE,
                   VM_INHERIT_NONE);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);

  gGraphicsShmem = (void*) address;

  pt.reset();

  // Process the introduction message to fill in arguments.
  MOZ_RELEASE_ASSERT(!gShmemPrefs);
  MOZ_RELEASE_ASSERT(gParentArgv.empty());

  gParentPid = gIntroductionMessage->mParentPid;

  // Record/replay the introduction message itself so we get consistent args
  // and prefs between recording and replaying.
  {
    IntroductionMessage* msg = IntroductionMessage::RecordReplay(*gIntroductionMessage);

    gShmemPrefs = new char[msg->mPrefsLen];
    memcpy(gShmemPrefs, msg->PrefsData(), msg->mPrefsLen);
    gShmemPrefsLen = msg->mPrefsLen;

    const char* pos = msg->ArgvString();
    for (size_t i = 0; i < msg->mArgc; i++) {
      gParentArgv.append(strdup(pos));
      pos += strlen(pos) + 1;
    }

    free(msg);
  }

  free(gIntroductionMessage);
  gIntroductionMessage = nullptr;

  // Some argument manipulation code expects a null pointer at the end.
  gParentArgv.append(nullptr);

  MOZ_RELEASE_ASSERT(*aArgc >= 1);
  MOZ_RELEASE_ASSERT(!strcmp((*aArgv)[0], gParentArgv[0]));
  MOZ_RELEASE_ASSERT(gParentArgv.back() == nullptr);

  *aArgc = gParentArgv.length() - 1; // For the trailing null.
  *aArgv = gParentArgv.begin();

  // If we failed to initialize then report it to the user.
  if (gInitializationFailureMessage) {
    ReportFatalError("%s", gInitializationFailureMessage);
    Unreachable();
  }
}

base::ProcessId
MiddlemanProcessId()
{
  return gMiddlemanPid;
}

base::ProcessId
ParentProcessId()
{
  return gParentPid;
}

void
ReportFatalError(const char* aFormat, ...)
{
  va_list ap;
  va_start(ap, aFormat);
  char buf[2048];
  VsprintfLiteral(buf, aFormat, ap);
  va_end(ap);

  // Construct a FatalErrorMessage on the stack, to avoid touching the heap.
  char msgBuf[4096];
  size_t header = sizeof(FatalErrorMessage);
  size_t len = std::min(strlen(buf) + 1, sizeof(msgBuf) - header);
  FatalErrorMessage* msg = new(msgBuf) FatalErrorMessage(header + len);
  memcpy(&msgBuf[header], buf, len);
  msgBuf[sizeof(msgBuf) - 1] = 0;

  // Don't take the message lock when sending this, to avoid touching the heap.
  gChannel->SendMessage(*msg);

  DirectPrint("***** Fatal Record/Replay Error *****\n");
  DirectPrint(buf);
  DirectPrint("\n");

  UnrecoverableSnapshotFailure();

  // Block until we get a terminate message and die.
  Thread::WaitForeverNoIdle();
}

void
NotifyFlushedRecording()
{
  gChannel->SendMessage(RecordingFlushedMessage());
}

void
NotifyAlwaysMarkMajorCheckpoints()
{
  if (IsActiveChild()) {
    gChannel->SendMessage(AlwaysMarkMajorCheckpointsMessage());
  }
}

///////////////////////////////////////////////////////////////////////////////
// Checkpoint Messages
///////////////////////////////////////////////////////////////////////////////

// When recording, the time when the last HitCheckpoint message was sent.
static double gLastCheckpointTime;

// When recording and we are idle, the time when we became idle.
static double gIdleTimeStart;

void
BeginIdleTime()
{
  MOZ_RELEASE_ASSERT(IsRecording() && NS_IsMainThread() && !gIdleTimeStart);
  gIdleTimeStart = CurrentTime();
}

void
EndIdleTime()
{
  MOZ_RELEASE_ASSERT(IsRecording() && NS_IsMainThread() && gIdleTimeStart);

  // Erase the idle time from our measurements by advancing the last checkpoint
  // time.
  gLastCheckpointTime += CurrentTime() - gIdleTimeStart;
  gIdleTimeStart = 0;
}

static void
HitCheckpoint(size_t aId, bool aRecordingEndpoint)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  double time = CurrentTime();
  PauseMainThreadAndInvokeCallback([=]() {
      double duration = 0;
      if (aId > FirstCheckpointId) {
        duration = time - gLastCheckpointTime;
        MOZ_RELEASE_ASSERT(duration > 0);
      }
      gChannel->SendMessage(HitCheckpointMessage(aId, aRecordingEndpoint, duration));
    });
  gLastCheckpointTime = time;
}

///////////////////////////////////////////////////////////////////////////////
// Debugger Messages
///////////////////////////////////////////////////////////////////////////////

static void
DebuggerResponseHook(const JS::replay::CharBuffer& aBuffer)
{
  DebuggerResponseMessage* msg =
    DebuggerResponseMessage::New(aBuffer.begin(), aBuffer.length());
  gChannel->SendMessage(*msg);
  free(msg);
}

static void
HitBreakpoint(bool aRecordingEndpoint, const uint32_t* aBreakpoints, size_t aNumBreakpoints)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  HitBreakpointMessage* msg =
    HitBreakpointMessage::New(aRecordingEndpoint, aBreakpoints, aNumBreakpoints);
  PauseMainThreadAndInvokeCallback([=]() {
      gChannel->SendMessage(*msg);
      free(msg);
    });
}

static void
PauseAfterRecoveringFromDivergence()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  PauseMainThreadAndInvokeCallback([=]() {
      JS::replay::hooks.respondAfterRecoveringFromDivergence();
    });
}

static void
InitDebuggerHooks()
{
  // Initialize hooks the JS debugger in a recording/replaying process can invoke.
  JS::replay::hooks.hitBreakpointReplay = HitBreakpoint;
  JS::replay::hooks.hitCheckpointReplay = HitCheckpoint;
  JS::replay::hooks.debugResponseReplay = DebuggerResponseHook;
  JS::replay::hooks.pauseAndRespondAfterRecoveringFromDivergence = PauseAfterRecoveringFromDivergence;
  JS::replay::hooks.hitCurrentRecordingEndpointReplay = HitRecordingEndpoint;
  JS::replay::hooks.canRewindReplay = HasSavedCheckpoint;
}

} // namespace child
} // namespace recordreplay
} // namespace mozilla
