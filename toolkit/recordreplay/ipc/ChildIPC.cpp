/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file has the logic which the replayed process uses to communicate with
// the middleman process.

#include "ChildInternal.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/mach_ipc_mac.h"
#include "ipc/Channel.h"
#include "mac/handler/exception_handler.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/Sprintf.h"
#include "mozilla/VsyncDispatcher.h"

#include "InfallibleVector.h"
#include "MemorySnapshot.h"
#include "nsPrintfCString.h"
#include "ParentInternal.h"
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
Monitor* gMonitor;

// The singleton channel for communicating with the middleman.
Channel* gChannel;

static base::ProcessId gMiddlemanPid;
static base::ProcessId gParentPid;
static StaticInfallibleVector<char*> gParentArgv;

// File descriptors used by a pipe to create checkpoints when instructed by the
// parent process.
static FileHandle gCheckpointWriteFd;
static FileHandle gCheckpointReadFd;

// Copy of the introduction message we got from the middleman. This is saved on
// receipt and then processed during InitRecordingOrReplayingProcess.
static UniquePtr<IntroductionMessage, Message::FreePolicy> gIntroductionMessage;

// When recording, whether developer tools server code runs in the middleman.
static bool gDebuggerRunsInMiddleman;

// Any response received to the last MiddlemanCallRequest message.
static UniquePtr<MiddlemanCallResponseMessage, Message::FreePolicy>
    gCallResponseMessage;

// Whether some thread has sent a MiddlemanCallRequest and is waiting for
// gCallResponseMessage to be filled in.
static bool gWaitingForCallResponse;

// Processing routine for incoming channel messages.
static void ChannelMessageHandler(Message::UniquePtr aMsg) {
  MOZ_RELEASE_ASSERT(MainThreadShouldPause() || aMsg->CanBeSentWhileUnpaused());

  switch (aMsg->mType) {
    case MessageType::Introduction: {
      MonitorAutoLock lock(*gMonitor);
      MOZ_RELEASE_ASSERT(!gIntroductionMessage);
      gIntroductionMessage.reset(
          static_cast<IntroductionMessage*>(aMsg.release()));
      gMonitor->NotifyAll();
      break;
    }
    case MessageType::CreateCheckpoint: {
      MOZ_RELEASE_ASSERT(IsRecording());

      // Ignore requests to create checkpoints before we have reached the first
      // paint and finished initializing.
      if (js::IsInitialized()) {
        uint8_t data = 0;
        DirectWrite(gCheckpointWriteFd, &data, 1);
      }
      break;
    }
    case MessageType::SetDebuggerRunsInMiddleman: {
      MOZ_RELEASE_ASSERT(IsRecording());
      PauseMainThreadAndInvokeCallback(
          [=]() { gDebuggerRunsInMiddleman = true; });
      break;
    }
    case MessageType::Terminate: {
      // Terminate messages behave differently in recording vs. replaying
      // processes. When sent to a recording process (which the middleman
      // manages directly) they signal that a clean shutdown is needed, while
      // when sent to a replaying process (which the UI process manages) they
      // signal that the process should crash, since it seems to be hanged.
      if (IsRecording()) {
        PrintSpew("Terminate message received, exiting...\n");
        _exit(0);
      } else {
        ReportFatalError(Nothing(), "Hung replaying process");
      }
      break;
    }
    case MessageType::ManifestStart: {
      const ManifestStartMessage& nmsg = (const ManifestStartMessage&)*aMsg;
      js::CharBuffer* buf = new js::CharBuffer();
      buf->append(nmsg.Buffer(), nmsg.BufferSize());
      PauseMainThreadAndInvokeCallback([=]() {
          js::ManifestStart(*buf);
          delete buf;
        });
      break;
    }
    case MessageType::MiddlemanCallResponse: {
      MonitorAutoLock lock(*gMonitor);
      MOZ_RELEASE_ASSERT(gWaitingForCallResponse);
      MOZ_RELEASE_ASSERT(!gCallResponseMessage);
      gCallResponseMessage.reset(
          static_cast<MiddlemanCallResponseMessage*>(aMsg.release()));
      gMonitor->NotifyAll();
      break;
    }
    default:
      MOZ_CRASH();
  }
}

// Main routine for a thread whose sole purpose is to listen to requests from
// the middleman process to create a new checkpoint. This is separate from the
// channel thread because this thread is recorded and the latter is not
// recorded. By communicating between the two threads with a pipe, this
// thread's behavior will be replicated exactly when replaying and new
// checkpoints will be created at the same point as during recording.
static void ListenForCheckpointThreadMain(void*) {
  while (true) {
    uint8_t data = 0;
    ssize_t rv = HANDLE_EINTR(read(gCheckpointReadFd, &data, 1));
    if (rv > 0) {
      NS_DispatchToMainThread(NewRunnableFunction("NewCheckpoint",
                                                  NewCheckpoint));
    } else {
      MOZ_RELEASE_ASSERT(errno == EIO);
      MOZ_RELEASE_ASSERT(HasDivergedFromRecording());
      Thread::WaitForever();
    }
  }
}

// Shared memory block for graphics data.
void* gGraphicsShmem;

void InitRecordingOrReplayingProcess(int* aArgc, char*** aArgv) {
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
  gChannel = new Channel(channelID.ref(), /* aMiddlemanRecording = */ false,
                         ChannelMessageHandler);

  pt.reset();

  // N.B. We can't spawn recorded threads when replaying if there was an
  // initialization failure.
  if (!gInitializationFailureMessage) {
    DirectCreatePipe(&gCheckpointWriteFd, &gCheckpointReadFd);
    Thread::StartThread(ListenForCheckpointThreadMain, nullptr, false);
  }

  pt.emplace();

  // Setup a mach port to receive the graphics shmem handle over.
  ReceivePort receivePort(
      nsPrintfCString("WebReplay.%d.%d", gMiddlemanPid, (int)channelID.ref())
          .get());

  MachSendMessage handshakeMessage(parent::GraphicsHandshakeMessageId);
  handshakeMessage.AddDescriptor(
      MachMsgPortDescriptor(receivePort.GetPort(), MACH_MSG_TYPE_COPY_SEND));

  MachPortSender sender(nsPrintfCString("WebReplay.%d", gMiddlemanPid).get());
  kern_return_t kr = sender.SendMessage(handshakeMessage, 1000);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);

  // The parent should send us a handle to the graphics shmem.
  MachReceiveMessage message;
  kr = receivePort.WaitForMessage(&message, 0);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);
  MOZ_RELEASE_ASSERT(message.GetMessageID() == parent::GraphicsMemoryMessageId);
  mach_port_t graphicsPort = message.GetTranslatedPort(0);
  MOZ_RELEASE_ASSERT(graphicsPort != MACH_PORT_NULL);

  mach_vm_address_t address = 0;
  kr = mach_vm_map(mach_task_self(), &address, parent::GraphicsMemorySize, 0,
                   VM_FLAGS_ANYWHERE, graphicsPort, 0, false,
                   VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE,
                   VM_INHERIT_NONE);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);

  gGraphicsShmem = (void*)address;

  // The graphics shared memory contents are excluded from snapshots. We do not
  // want checkpoint restores in this child to interfere with drawing being
  // performed by another child.
  AddInitialUntrackedMemoryRegion((uint8_t*)gGraphicsShmem,
                                  parent::GraphicsMemorySize);

  pt.reset();

  // If we failed to initialize then report it to the user.
  if (gInitializationFailureMessage) {
    ReportFatalError(Nothing(), "%s", gInitializationFailureMessage);
    Unreachable();
  }

  // Wait for the parent to send us the introduction message.
  {
    MonitorAutoLock lock(*gMonitor);
    while (!gIntroductionMessage) {
      gMonitor->Wait();
    }
  }

  // Process the introduction message to fill in arguments.
  MOZ_RELEASE_ASSERT(gParentArgv.empty());

  gParentPid = gIntroductionMessage->mParentPid;

  // Record/replay the introduction message itself so we get consistent args
  // between recording and replaying.
  {
    IntroductionMessage* msg =
        IntroductionMessage::RecordReplay(*gIntroductionMessage);

    const char* pos = msg->ArgvString();
    for (size_t i = 0; i < msg->mArgc; i++) {
      gParentArgv.append(strdup(pos));
      pos += strlen(pos) + 1;
    }

    free(msg);
  }

  gIntroductionMessage = nullptr;

  // Some argument manipulation code expects a null pointer at the end.
  gParentArgv.append(nullptr);

  MOZ_RELEASE_ASSERT(*aArgc >= 1);
  MOZ_RELEASE_ASSERT(gParentArgv.back() == nullptr);

  *aArgc = gParentArgv.length() - 1;  // For the trailing null.
  *aArgv = gParentArgv.begin();
}

base::ProcessId MiddlemanProcessId() { return gMiddlemanPid; }

base::ProcessId ParentProcessId() { return gParentPid; }

bool DebuggerRunsInMiddleman() {
  return RecordReplayValue(gDebuggerRunsInMiddleman);
}

void CreateCheckpoint() {
  if (!HasDivergedFromRecording()) {
    NewCheckpoint();
  }
}

void ReportFatalError(const Maybe<MinidumpInfo>& aMinidump, const char* aFormat,
                      ...) {
  // Notify the middleman that we are crashing and are going to try to write a
  // minidump.
  gChannel->SendMessage(BeginFatalErrorMessage());

  // Unprotect any memory which might be written while producing the minidump.
  UnrecoverableSnapshotFailure();

  AutoEnsurePassThroughThreadEvents pt;

#ifdef MOZ_CRASHREPORTER
  MinidumpInfo info = aMinidump.isSome()
                          ? aMinidump.ref()
                          : MinidumpInfo(EXC_CRASH, 1, 0, mach_thread_self());
  google_breakpad::ExceptionHandler::WriteForwardedExceptionMinidump(
      info.mExceptionType, info.mCode, info.mSubcode, info.mThread);
#endif

  va_list ap;
  va_start(ap, aFormat);
  char buf[2048];
  VsprintfLiteral(buf, aFormat, ap);
  va_end(ap);

  // Construct a FatalErrorMessage on the stack, to avoid touching the heap.
  char msgBuf[4096];
  size_t header = sizeof(FatalErrorMessage);
  size_t len = std::min(strlen(buf) + 1, sizeof(msgBuf) - header);
  FatalErrorMessage* msg = new (msgBuf) FatalErrorMessage(header + len);
  memcpy(&msgBuf[header], buf, len);
  msgBuf[sizeof(msgBuf) - 1] = 0;

  // Don't take the message lock when sending this, to avoid touching the heap.
  gChannel->SendMessage(std::move(*msg));

  DirectPrint("***** Fatal Record/Replay Error *****\n");
  DirectPrint(buf);
  DirectPrint("\n");

  // Block until we get a terminate message and die.
  Thread::WaitForeverNoIdle();
}

///////////////////////////////////////////////////////////////////////////////
// Vsyncs
///////////////////////////////////////////////////////////////////////////////

static VsyncObserver* gVsyncObserver;

void SetVsyncObserver(VsyncObserver* aObserver) {
  MOZ_RELEASE_ASSERT(!gVsyncObserver || !aObserver);
  gVsyncObserver = aObserver;
}

void NotifyVsyncObserver() {
  if (gVsyncObserver) {
    static VsyncId vsyncId;
    vsyncId = vsyncId.Next();
    VsyncEvent event(vsyncId, TimeStamp::Now());
    gVsyncObserver->NotifyVsync(event);
  }
}

// How many paints have been started and haven't reached PaintFromMainThread
// yet. Only accessed on the main thread.
static int32_t gNumPendingMainThreadPaints;

bool OnVsync() {
  // In the repainting stress mode, we create a new checkpoint on every vsync
  // message received from the UI process. When we notify the parent about the
  // new checkpoint it will trigger a repaint to make sure that all layout and
  // painting activity can occur when diverged from the recording.
  if (parent::InRepaintStressMode()) {
    CreateCheckpoint();
  }

  // After a paint starts, ignore incoming vsyncs until the paint completes.
  return gNumPendingMainThreadPaints == 0;
}

///////////////////////////////////////////////////////////////////////////////
// Painting
///////////////////////////////////////////////////////////////////////////////

// Target buffer for the draw target created by the child process widget, which
// the compositor thread writes to.
static void* gDrawTargetBuffer;
static size_t gDrawTargetBufferSize;

// Dimensions of the last paint which the compositor performed.
static size_t gPaintWidth, gPaintHeight;

// How many updates have been sent to the compositor thread and haven't been
// processed yet. This can briefly become negative if the main thread sends an
// update and the compositor processes it before the main thread reaches
// NotifyPaintStart. Outside of this window, the compositor can only write to
// gDrawTargetBuffer or update gPaintWidth/gPaintHeight if this is non-zero.
static Atomic<int32_t, SequentiallyConsistent, Behavior::DontPreserve>
    gNumPendingPaints;

// ID of the compositor thread.
static Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve>
    gCompositorThreadId;

// Whether repaint failures are allowed, or if the process should crash.
static bool gAllowRepaintFailures;

already_AddRefed<gfx::DrawTarget> DrawTargetForRemoteDrawing(
    LayoutDeviceIntSize aSize) {
  MOZ_RELEASE_ASSERT(!NS_IsMainThread());

  // Keep track of the compositor thread ID.
  size_t threadId = Thread::Current()->Id();
  if (gCompositorThreadId) {
    MOZ_RELEASE_ASSERT(threadId == gCompositorThreadId);
  } else {
    gCompositorThreadId = threadId;
  }

  if (aSize.IsEmpty()) {
    return nullptr;
  }

  gPaintWidth = aSize.width;
  gPaintHeight = aSize.height;

  gfx::IntSize size(aSize.width, aSize.height);
  size_t bufferSize =
      layers::ImageDataSerializer::ComputeRGBBufferSize(size, gSurfaceFormat);
  MOZ_RELEASE_ASSERT(bufferSize <= parent::GraphicsMemorySize);

  if (bufferSize != gDrawTargetBufferSize) {
    free(gDrawTargetBuffer);
    gDrawTargetBuffer = malloc(bufferSize);
    gDrawTargetBufferSize = bufferSize;
  }

  size_t stride = layers::ImageDataSerializer::ComputeRGBStride(gSurfaceFormat,
                                                                aSize.width);
  RefPtr<gfx::DrawTarget> drawTarget = gfx::Factory::CreateDrawTargetForData(
      gfx::BackendType::SKIA, (uint8_t*)gDrawTargetBuffer, size, stride,
      gSurfaceFormat,
      /* aUninitialized = */ true);
  if (!drawTarget) {
    MOZ_CRASH();
  }

  return drawTarget.forget();
}

void NotifyPaintStart() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // Initialize state on the first paint.
  static bool gPainted;
  if (!gPainted) {
    gPainted = true;

    // Repaint failures are not allowed in the repaint stress mode.
    gAllowRepaintFailures =
        Preferences::GetBool("devtools.recordreplay.allowRepaintFailures") &&
        !parent::InRepaintStressMode();
  }

  gNumPendingPaints++;
  gNumPendingMainThreadPaints++;
}

static void PaintFromMainThread() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  gNumPendingMainThreadPaints--;

  if (gNumPendingMainThreadPaints) {
    // Another paint started before we were able to finish it here. The draw
    // target buffer no longer reflects program state at the last checkpoint,
    // so don't send a Paint message.
    return;
  }

  // If all paints have completed, the compositor cannot be simultaneously
  // operating on the draw target buffer.
  MOZ_RELEASE_ASSERT(!gNumPendingPaints);

  if (IsMainChild() && gDrawTargetBuffer) {
    memcpy(gGraphicsShmem, gDrawTargetBuffer, gDrawTargetBufferSize);
    gChannel->SendMessage(PaintMessage(GetLastCheckpoint(),
                                       gPaintWidth, gPaintHeight));
  }
}

void NotifyPaintComplete() {
  MOZ_RELEASE_ASSERT(!gCompositorThreadId ||
                     Thread::Current()->Id() == gCompositorThreadId);

  // Notify the main thread in case it is waiting for this paint to complete.
  {
    MonitorAutoLock lock(*gMonitor);
    if (--gNumPendingPaints == 0) {
      gMonitor->Notify();
    }
  }

  // Notify the middleman about the completed paint from the main thread.
  NS_DispatchToMainThread(
      NewRunnableFunction("PaintFromMainThread", PaintFromMainThread));
}

// Whether we have repainted since diverging from the recording.
static bool gDidRepaint;

// Whether we are currently repainting.
static bool gRepainting;

void Repaint(size_t* aWidth, size_t* aHeight) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(HasDivergedFromRecording());

  // Don't try to repaint if the first normal paint hasn't occurred yet.
  if (!gCompositorThreadId) {
    *aWidth = 0;
    *aHeight = 0;
    return;
  }

  // Ignore the request to repaint if we already triggered a repaint, in which
  // case the last graphics we sent will still be correct.
  if (!gDidRepaint) {
    gDidRepaint = true;
    gRepainting = true;

    // Allow other threads to diverge from the recording so the compositor can
    // perform any paint we are about to trigger, or finish any in flight paint
    // that existed at the point we are paused at.
    for (size_t i = MainThreadId + 1; i <= MaxRecordedThreadId; i++) {
      Thread::GetById(i)->SetShouldDivergeFromRecording();
    }
    Thread::ResumeIdleThreads();

    // Create an artifical vsync to see if graphics have changed since the last
    // paint and a new paint is needed.
    NotifyVsyncObserver();

    // Wait for the compositor to finish all in flight paints, including any
    // one we just triggered.
    {
      MonitorAutoLock lock(*gMonitor);
      while (gNumPendingPaints) {
        gMonitor->Wait();
      }
    }

    Thread::WaitForIdleThreads();
    gRepainting = false;
  }

  if (gDrawTargetBuffer) {
    memcpy(gGraphicsShmem, gDrawTargetBuffer, gDrawTargetBufferSize);
    *aWidth = gPaintWidth;
    *aHeight = gPaintHeight;
  } else {
    *aWidth = 0;
    *aHeight = 0;
  }
}

bool CurrentRepaintCannotFail() {
  return gRepainting && !gAllowRepaintFailures;
}

///////////////////////////////////////////////////////////////////////////////
// Message Helpers
///////////////////////////////////////////////////////////////////////////////

void ManifestFinished(const js::CharBuffer& aBuffer) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  ManifestFinishedMessage* msg =
    ManifestFinishedMessage::New(aBuffer.begin(), aBuffer.length());
  PauseMainThreadAndInvokeCallback([=]() {
    gChannel->SendMessage(std::move(*msg));
    free(msg);
  });
}

void SendMiddlemanCallRequest(const char* aInputData, size_t aInputSize,
                              InfallibleVector<char>* aOutputData) {
  AutoPassThroughThreadEvents pt;
  MonitorAutoLock lock(*gMonitor);

  while (gWaitingForCallResponse) {
    gMonitor->Wait();
  }
  gWaitingForCallResponse = true;

  MiddlemanCallRequestMessage* msg =
      MiddlemanCallRequestMessage::New(aInputData, aInputSize);
  gChannel->SendMessage(std::move(*msg));
  free(msg);

  while (!gCallResponseMessage) {
    gMonitor->Wait();
  }

  aOutputData->append(gCallResponseMessage->BinaryData(),
                      gCallResponseMessage->BinaryDataSize());

  gCallResponseMessage = nullptr;
  gWaitingForCallResponse = false;

  gMonitor->Notify();
}

void SendResetMiddlemanCalls() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  gChannel->SendMessage(ResetMiddlemanCallsMessage());
}

}  // namespace child
}  // namespace recordreplay
}  // namespace mozilla
