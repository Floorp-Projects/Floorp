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
#include "mozilla/dom/ContentChild.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayerTransactionChild.h"
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
static IntroductionMessage* gIntroductionMessage;

// When recording, whether developer tools server code runs in the middleman.
static bool gDebuggerRunsInMiddleman;

// Any response received to the last MiddlemanCallRequest message.
static MiddlemanCallResponseMessage* gCallResponseMessage;

// Processing routine for incoming channel messages.
static void
ChannelMessageHandler(Message* aMsg)
{
  MOZ_RELEASE_ASSERT(MainThreadShouldPause() || aMsg->CanBeSentWhileUnpaused());

  switch (aMsg->mType) {
  case MessageType::Introduction: {
    MOZ_RELEASE_ASSERT(!gIntroductionMessage);
    gIntroductionMessage = (IntroductionMessage*) aMsg->Clone();
    break;
  }
  case MessageType::CreateCheckpoint: {
    MOZ_RELEASE_ASSERT(IsRecording());

    // Ignore requests to create checkpoints before we have reached the first
    // paint and finished initializing.
    if (navigation::IsInitialized()) {
      uint8_t data = 0;
      DirectWrite(gCheckpointWriteFd, &data, 1);
    }
    break;
  }
  case MessageType::SetDebuggerRunsInMiddleman: {
    MOZ_RELEASE_ASSERT(IsRecording());
    PauseMainThreadAndInvokeCallback([=]() { gDebuggerRunsInMiddleman = true; });
    break;
  }
  case MessageType::Terminate: {
    // Terminate messages behave differently in recording vs. replaying
    // processes. When sent to a recording process (which the middleman manages
    // directly) they signal that a clean shutdown is needed, while when sent
    // to a replaying process (which the UI process manages) they signal that
    // the process should crash, since it seems to be hanged.
    if (IsRecording()) {
      PrintSpew("Terminate message received, exiting...\n");
      _exit(0);
    } else {
      MOZ_CRASH("Hanged replaying process");
    }
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
    js::CharBuffer* buf = new js::CharBuffer();
    buf->append(nmsg.Buffer(), nmsg.BufferSize());
    PauseMainThreadAndInvokeCallback([=]() { navigation::DebuggerRequest(buf); });
    break;
  }
  case MessageType::SetBreakpoint: {
    const SetBreakpointMessage& nmsg = (const SetBreakpointMessage&) *aMsg;
    PauseMainThreadAndInvokeCallback([=]() {
        navigation::SetBreakpoint(nmsg.mId, nmsg.mPosition);
      });
    break;
  }
  case MessageType::Resume: {
    const ResumeMessage& nmsg = (const ResumeMessage&) *aMsg;
    PauseMainThreadAndInvokeCallback([=]() {
        navigation::Resume(nmsg.mForward);
      });
    break;
  }
  case MessageType::RestoreCheckpoint: {
    const RestoreCheckpointMessage& nmsg = (const RestoreCheckpointMessage&) *aMsg;
    PauseMainThreadAndInvokeCallback([=]() {
        navigation::RestoreCheckpoint(nmsg.mCheckpoint);
      });
    break;
  }
  case MessageType::RunToPoint: {
    const RunToPointMessage& nmsg = (const RunToPointMessage&) *aMsg;
    PauseMainThreadAndInvokeCallback([=]() {
        navigation::RunToPoint(nmsg.mTarget);
      });
    break;
  }
  case MessageType::MiddlemanCallResponse: {
    MonitorAutoLock lock(*gMonitor);
    MOZ_RELEASE_ASSERT(!gCallResponseMessage);
    gCallResponseMessage = (MiddlemanCallResponseMessage*) aMsg;
    aMsg = nullptr; // Avoid freeing the message below.
    gMonitor->NotifyAll();
    break;
  }
  default:
    MOZ_CRASH();
  }

  free(aMsg);
}

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

// Shared memory block for graphics data.
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
  gChannel = new Channel(channelID.ref(), /* aMiddlemanRecording = */ false, ChannelMessageHandler);

  pt.reset();

  // N.B. We can't spawn recorded threads when replaying if there was an
  // initialization failure.
  if (!gInitializationFailureMessage) {
    DirectCreatePipe(&gCheckpointWriteFd, &gCheckpointReadFd);
    Thread::StartThread(ListenForCheckpointThreadMain, nullptr, false);
  }

  pt.emplace();

  // Setup a mach port to receive the graphics shmem handle over.
  ReceivePort receivePort(nsPrintfCString("WebReplay.%d.%d", gMiddlemanPid, (int) channelID.ref()).get());

  MachSendMessage handshakeMessage(parent::GraphicsHandshakeMessageId);
  handshakeMessage.AddDescriptor(MachMsgPortDescriptor(receivePort.GetPort(), MACH_MSG_TYPE_COPY_SEND));

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
  kr = mach_vm_map(mach_task_self(), &address, parent::GraphicsMemorySize, 0, VM_FLAGS_ANYWHERE,
                   graphicsPort, 0, false,
                   VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE,
                   VM_INHERIT_NONE);
  MOZ_RELEASE_ASSERT(kr == KERN_SUCCESS);

  gGraphicsShmem = (void*) address;

  // The graphics shared memory contents are excluded from snapshots. We do not
  // want checkpoint restores in this child to interfere with drawing being
  // performed by another child.
  AddInitialUntrackedMemoryRegion((uint8_t*) gGraphicsShmem, parent::GraphicsMemorySize);

  pt.reset();

  // We are ready to receive initialization messages from the middleman, pause
  // so they can be sent.
  HitCheckpoint(CheckpointId::Invalid, /* aRecordingEndpoint = */ false);

  // If we failed to initialize then report it to the user.
  if (gInitializationFailureMessage) {
    ReportFatalError(Nothing(), "%s", gInitializationFailureMessage);
    Unreachable();
  }

  // Process the introduction message to fill in arguments.
  MOZ_RELEASE_ASSERT(gParentArgv.empty());

  gParentPid = gIntroductionMessage->mParentPid;

  // Record/replay the introduction message itself so we get consistent args
  // between recording and replaying.
  {
    IntroductionMessage* msg = IntroductionMessage::RecordReplay(*gIntroductionMessage);

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
  MOZ_RELEASE_ASSERT(gParentArgv.back() == nullptr);

  *aArgc = gParentArgv.length() - 1; // For the trailing null.
  *aArgv = gParentArgv.begin();
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

bool
DebuggerRunsInMiddleman()
{
  return RecordReplayValue(gDebuggerRunsInMiddleman);
}

void
CreateCheckpoint()
{
  if (!HasDivergedFromRecording()) {
    NewCheckpoint(/* aTemporary = */ false);
  }
}

void
ReportFatalError(const Maybe<MinidumpInfo>& aMinidump, const char* aFormat, ...)
{
  // Unprotect any memory which might be written while producing the minidump.
  UnrecoverableSnapshotFailure();

  AutoEnsurePassThroughThreadEvents pt;

#ifdef MOZ_CRASHREPORTER
  MinidumpInfo info = aMinidump.isSome()
                      ? aMinidump.ref()
                      : MinidumpInfo(EXC_CRASH, 1, 0, mach_thread_self());
  google_breakpad::ExceptionHandler::WriteForwardedExceptionMinidump
    (info.mExceptionType, info.mCode, info.mSubcode, info.mThread);
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
  FatalErrorMessage* msg = new(msgBuf) FatalErrorMessage(header + len);
  memcpy(&msgBuf[header], buf, len);
  msgBuf[sizeof(msgBuf) - 1] = 0;

  // Don't take the message lock when sending this, to avoid touching the heap.
  gChannel->SendMessage(*msg);

  DirectPrint("***** Fatal Record/Replay Error *****\n");
  DirectPrint(buf);
  DirectPrint("\n");

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
// Vsyncs
///////////////////////////////////////////////////////////////////////////////

static VsyncObserver* gVsyncObserver;

void
SetVsyncObserver(VsyncObserver* aObserver)
{
  MOZ_RELEASE_ASSERT(!gVsyncObserver || !aObserver);
  gVsyncObserver = aObserver;
}

static void
NotifyVsyncObserver()
{
  if (gVsyncObserver) {
    gVsyncObserver->NotifyVsync(TimeStamp::Now());
  }
}

// Whether an update has been sent to the compositor for a normal paint, and we
// haven't reached PaintFromMainThread yet. This is used to preserve the
// invariant that there can be at most one paint performed between two
// checkpoints, other than repaints triggered by the debugger.
static bool gHasActivePaint;

bool
OnVsync()
{
  // In the repainting stress mode, we create a new checkpoint on every vsync
  // message received from the UI process. When we notify the parent about the
  // new checkpoint it will trigger a repaint to make sure that all layout and
  // painting activity can occur when diverged from the recording.
  if (parent::InRepaintStressMode()) {
    CreateCheckpoint();
  }

  // After a paint starts, ignore incoming vsyncs until the paint completes.
  return !gHasActivePaint;
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
static Atomic<int32_t, SequentiallyConsistent, Behavior::DontPreserve> gNumPendingPaints;

// ID of the compositor thread.
static Atomic<size_t, SequentiallyConsistent, Behavior::DontPreserve> gCompositorThreadId;

already_AddRefed<gfx::DrawTarget>
DrawTargetForRemoteDrawing(LayoutDeviceIntSize aSize)
{
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
  size_t bufferSize = layers::ImageDataSerializer::ComputeRGBBufferSize(size, gSurfaceFormat);
  MOZ_RELEASE_ASSERT(bufferSize <= parent::GraphicsMemorySize);

  if (bufferSize != gDrawTargetBufferSize) {
    free(gDrawTargetBuffer);
    gDrawTargetBuffer = malloc(bufferSize);
    gDrawTargetBufferSize = bufferSize;
  }

  size_t stride = layers::ImageDataSerializer::ComputeRGBStride(gSurfaceFormat, aSize.width);
  RefPtr<gfx::DrawTarget> drawTarget =
    gfx::Factory::CreateDrawTargetForData(gfx::BackendType::SKIA, (uint8_t*) gDrawTargetBuffer,
                                          size, stride, gSurfaceFormat,
                                          /* aUninitialized = */ true);
  if (!drawTarget) {
    MOZ_CRASH();
  }

  return drawTarget.forget();
}

void
NotifyPaintStart()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // A new paint cannot be triggered until the last one finishes and has been
  // sent to the middleman.
  MOZ_RELEASE_ASSERT(HasDivergedFromRecording() || !gHasActivePaint);

  gNumPendingPaints++;
  gHasActivePaint = true;

  CreateCheckpoint();
}

static void
PaintFromMainThread()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  // There cannot not be any other in flight paints.
  MOZ_RELEASE_ASSERT(!gNumPendingPaints);

  // Clear the active flag now that we have completed the paint.
  MOZ_RELEASE_ASSERT(gHasActivePaint);
  gHasActivePaint = false;

  if (IsActiveChild() && gDrawTargetBuffer) {
    memcpy(gGraphicsShmem, gDrawTargetBuffer, gDrawTargetBufferSize);
    gChannel->SendMessage(PaintMessage(navigation::LastNormalCheckpoint(),
                                       gPaintWidth, gPaintHeight));
  }
}

void
NotifyPaintComplete()
{
  MOZ_RELEASE_ASSERT(Thread::Current()->Id() == gCompositorThreadId);

  // Notify the main thread in case it is waiting for this paint to complete.
  {
    MonitorAutoLock lock(*gMonitor);
    if (--gNumPendingPaints == 0) {
      gMonitor->Notify();
    }
  }

  // Notify the middleman about the completed paint from the main thread.
  NS_DispatchToMainThread(NewRunnableFunction("PaintFromMainThread", PaintFromMainThread));
}

void
Repaint(size_t* aWidth, size_t* aHeight)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(HasDivergedFromRecording());

  // Don't try to repaint if the first normal paint hasn't occurred yet.
  if (!gCompositorThreadId) {
    *aWidth = 0;
    *aHeight = 0;
    return;
  }

  // Ignore the request to repaint if the compositor thread has already
  // diverged from the recording. In this case we have already done a repaint
  // and the last graphics we sent will still be correct.
  Thread* compositorThread = Thread::GetById(gCompositorThreadId);
  if (!compositorThread->WillDivergeFromRecordingSoon()) {
    // Create an artifical vsync to see if graphics have changed since the last
    // paint and a new paint is needed.
    NotifyVsyncObserver();

    if (gNumPendingPaints) {
      // Allow the compositor to diverge from the recording so it can perform
      // any paint we just triggered, or finish any in flight paint that that
      // existed at the point we are paused at.
      Thread::GetById(gCompositorThreadId)->SetShouldDivergeFromRecording();

      // Wait for the compositor to finish all in flight paints, including any
      // one we just triggered.
      MonitorAutoLock lock(*gMonitor);
      while (gNumPendingPaints) {
        gMonitor->Wait();
      }
    }
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

static bool
CompositorCanPerformMiddlemanCalls()
{
  // After repainting finishes the compositor is not allowed to send call
  // requests to the middleman anymore.
  return !!gNumPendingPaints;
}

bool
SuppressMessageAfterDiverge(IPC::Message* aMsg)
{
  MOZ_RELEASE_ASSERT(HasDivergedFromRecording());

  // Only messages necessary for compositing can be sent after the sending
  // thread has diverged from the recording. Sending other messages can risk
  // deadlocking when a necessary lock is held by an idle thread (we probably
  // need a more robust way to deal with this problem).

  IPC::Message::msgid_t type = aMsg->type();
  if (type >= layers::PLayerTransaction::PLayerTransactionStart &&
      type <= layers::PLayerTransaction::PLayerTransactionEnd) {
    return false;
  }

  if (type == layers::PCompositorBridge::Msg_PTextureConstructor__ID) {
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Checkpoint Messages
///////////////////////////////////////////////////////////////////////////////

// The time when the last HitCheckpoint message was sent.
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

void
HitCheckpoint(size_t aId, bool aRecordingEndpoint)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  double time = CurrentTime();
  PauseMainThreadAndInvokeCallback([=]() {
      double duration = 0;
      if (aId > CheckpointId::First) {
        duration = time - gLastCheckpointTime;
        MOZ_RELEASE_ASSERT(duration > 0);
      }
      gChannel->SendMessage(HitCheckpointMessage(aId, aRecordingEndpoint, duration));
    });
  gLastCheckpointTime = time;
}

///////////////////////////////////////////////////////////////////////////////
// Message Helpers
///////////////////////////////////////////////////////////////////////////////

void
RespondToRequest(const js::CharBuffer& aBuffer)
{
  DebuggerResponseMessage* msg =
    DebuggerResponseMessage::New(aBuffer.begin(), aBuffer.length());
  gChannel->SendMessage(*msg);
  free(msg);
}

void
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

bool
SendMiddlemanCallRequest(const char* aInputData, size_t aInputSize,
                         InfallibleVector<char>* aOutputData)
{
  Thread* thread = Thread::Current();

  // Middleman calls can only be made from the main and compositor threads.
  // These two threads cannot simultaneously send call requests or other
  // messages, as doing so will race both here and in Channel::SendMessage.
  // CompositorCanPerformMiddlemanCalls() ensures that the main thread is
  // not actively sending messages at times when the compositor performs
  // middleman calls.
  MOZ_RELEASE_ASSERT(thread->IsMainThread() || thread->Id() == gCompositorThreadId);

  if (thread->Id() == gCompositorThreadId && !CompositorCanPerformMiddlemanCalls()) {
    return false;
  }

  MonitorAutoLock lock(*gMonitor);

  MOZ_RELEASE_ASSERT(!gCallResponseMessage);

  MiddlemanCallRequestMessage* msg = MiddlemanCallRequestMessage::New(aInputData, aInputSize);
  gChannel->SendMessage(*msg);
  free(msg);

  while (!gCallResponseMessage) {
    gMonitor->Wait();
  }

  aOutputData->append(gCallResponseMessage->BinaryData(), gCallResponseMessage->BinaryDataSize());

  free(gCallResponseMessage);
  gCallResponseMessage = nullptr;

  gMonitor->Notify();
  return true;
}

void
SendResetMiddlemanCalls()
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  gChannel->SendMessage(ResetMiddlemanCallsMessage());
}

} // namespace child
} // namespace recordreplay
} // namespace mozilla
