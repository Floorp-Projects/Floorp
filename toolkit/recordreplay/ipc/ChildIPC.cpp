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
#include "mozilla/Base64.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/Sprintf.h"
#include "mozilla/VsyncDispatcher.h"

#include "InfallibleVector.h"
#include "nsPrintfCString.h"
#include "ParentInternal.h"
#include "ProcessRecordReplay.h"
#include "ProcessRedirect.h"
#include "ProcessRewind.h"
#include "Thread.h"
#include "Units.h"

#include "imgIEncoder.h"

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

// Fork ID of this process.
static size_t gForkId;

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

// Data we've received which hasn't been incorporated into the recording yet.
static StaticInfallibleVector<char> gPendingRecordingData;

// When recording, whether developer tools server code runs in the middleman.
static bool gDebuggerRunsInMiddleman;

// Any response received to the last ExternalCallRequest message.
static UniquePtr<ExternalCallResponseMessage, Message::FreePolicy>
    gCallResponseMessage;

// Whether some thread has sent an ExternalCallRequest and is waiting for
// gCallResponseMessage to be filled in.
static bool gWaitingForCallResponse;

static void HandleMessageToForkedProcess(Message::UniquePtr aMsg);

// Processing routine for incoming channel messages.
static void ChannelMessageHandler(Message::UniquePtr aMsg) {
  MOZ_RELEASE_ASSERT(MainThreadShouldPause() || aMsg->CanBeSentWhileUnpaused());

  if (aMsg->mForkId != gForkId) {
    MOZ_RELEASE_ASSERT(!gForkId);
    HandleMessageToForkedProcess(std::move(aMsg));
    return;
  }

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
    case MessageType::Ping: {
      // The progress value included in a ping response reflects both the JS
      // execution progress counter and the progress that all threads have
      // made in their event streams. This accounts for an assortment of
      // scenarios which could be mistaken for a hang, such as a long-running
      // script that doesn't interact with the recording, or a long-running
      // operation running off the main thread.
      const PingMessage& nmsg = (const PingMessage&)*aMsg;
      uint64_t total =
          *ExecutionProgressCounter() + Thread::TotalEventProgress();
      gChannel->SendMessage(PingResponseMessage(gForkId, nmsg.mId, total));
      break;
    }
    case MessageType::Terminate: {
      PrintSpew("Terminate message received, exiting...\n");
      _exit(0);
      break;
    }
    case MessageType::Crash: {
      ReportFatalError("Hung replaying process");
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
    case MessageType::ExternalCallResponse: {
      MonitorAutoLock lock(*gMonitor);
      MOZ_RELEASE_ASSERT(gWaitingForCallResponse);
      MOZ_RELEASE_ASSERT(!gCallResponseMessage);
      gCallResponseMessage.reset(
          static_cast<ExternalCallResponseMessage*>(aMsg.release()));
      gMonitor->NotifyAll();
      break;
    }
    case MessageType::RecordingData: {
      MonitorAutoLock lock(*gMonitor);
      const RecordingDataMessage& nmsg = (const RecordingDataMessage&)*aMsg;
      MOZ_RELEASE_ASSERT(
          nmsg.mTag == gRecording->Size() + gPendingRecordingData.length());
      gPendingRecordingData.append(nmsg.BinaryData(), nmsg.BinaryDataSize());
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
      NS_DispatchToMainThread(
          NewRunnableFunction("NewCheckpoint", NewCheckpoint));
    } else {
      MOZ_RELEASE_ASSERT(errno == EIO);
      MOZ_RELEASE_ASSERT(HasDivergedFromRecording());
      Thread::WaitForever();
    }
  }
}

// Shared memory block for graphics data.
void* gGraphicsShmem;

static void WaitForGraphicsShmem() {
  // Setup a mach port to receive the graphics shmem handle over.
  nsPrintfCString portString("WebReplay.%d.%lu", gMiddlemanPid, GetId());
  ReceivePort receivePort(portString.get());

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
}

static void InitializeForkListener();

void SetupRecordReplayChannel(int aArgc, char* aArgv[]) {
  MOZ_RELEASE_ASSERT(IsRecordingOrReplaying() &&
                     AreThreadEventsPassedThrough());

  Maybe<int> channelID;
  for (int i = 0; i < aArgc; i++) {
    if (!strcmp(aArgv[i], gMiddlemanPidOption)) {
      MOZ_RELEASE_ASSERT(!gMiddlemanPid && i + 1 < aArgc);
      gMiddlemanPid = atoi(aArgv[i + 1]);
    }
    if (!strcmp(aArgv[i], gChannelIDOption)) {
      MOZ_RELEASE_ASSERT(channelID.isNothing() && i + 1 < aArgc);
      channelID.emplace(atoi(aArgv[i + 1]));
    }
  }
  MOZ_RELEASE_ASSERT(channelID.isSome());

  gMonitor = new Monitor();
  gChannel = new Channel(channelID.ref(), Channel::Kind::RecordReplay,
                         ChannelMessageHandler, gMiddlemanPid);

  // If we failed to initialize then report it to the user.
  if (gInitializationFailureMessage) {
    ReportFatalError("%s", gInitializationFailureMessage);
    Unreachable();
  }

  // Wait for the parent to send us the introduction message.
  MonitorAutoLock lock(*gMonitor);
  while (!gIntroductionMessage) {
    gMonitor->Wait();
  }

  // If we're replaying, we also need to wait for some recording data.
  if (IsReplaying()) {
    while (gPendingRecordingData.empty()) {
      gMonitor->Wait();
    }
  }
}

void InitRecordingOrReplayingProcess(int* aArgc, char*** aArgv) {
  if (!IsRecordingOrReplaying()) {
    return;
  }

  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  {
    AutoPassThroughThreadEvents pt;
    if (IsRecording()) {
      WaitForGraphicsShmem();
    } else {
      InitializeForkListener();
    }
  }

  DirectCreatePipe(&gCheckpointWriteFd, &gCheckpointReadFd);
  Thread::StartThread(ListenForCheckpointThreadMain, nullptr, false);

  // Process the introduction message to fill in arguments.
  MOZ_RELEASE_ASSERT(gParentArgv.empty());

  // Record/replay the introduction message itself so we get consistent args
  // between recording and replaying.
  {
    IntroductionMessage* msg =
        IntroductionMessage::RecordReplay(*gIntroductionMessage);

    gParentPid = gIntroductionMessage->mParentPid;

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

static void HandleMessageFromForkedProcess(Message::UniquePtr aMsg);

// Messages to send to forks that don't exist yet.
static StaticInfallibleVector<Message::UniquePtr> gPendingForkMessages;

struct ForkedProcess {
  base::ProcessId mPid;
  size_t mForkId;
  Channel* mChannel;
};

static StaticInfallibleVector<ForkedProcess> gForkedProcesses;
static FileHandle gForkWriteFd, gForkReadFd;
static char* gFatalErrorMemory;
static const size_t FatalErrorMemorySize = PageSize;

static void ForkListenerThread(void*) {
  while (true) {
    ForkedProcess process;
    int nbytes = read(gForkReadFd, &process, sizeof(process));
    MOZ_RELEASE_ASSERT(nbytes == sizeof(process));

    process.mChannel = new Channel(0, Channel::Kind::ReplayRoot,
                                   HandleMessageFromForkedProcess,
                                   process.mPid);

    // Send any messages destined for this fork.
    size_t i = 0;
    while (i < gPendingForkMessages.length()) {
      auto& pending = gPendingForkMessages[i];
      if (pending->mForkId == process.mForkId) {
        process.mChannel->SendMessage(std::move(*pending));
        gPendingForkMessages.erase(&pending);
      } else {
        i++;
      }
    }

    gForkedProcesses.emplaceBack(process);
  }
}

static void InitializeForkListener() {
  DirectCreatePipe(&gForkWriteFd, &gForkReadFd);

  Thread::SpawnNonRecordedThread(ForkListenerThread, nullptr);

  if (!ReplayingInCloud()) {
    gFatalErrorMemory = (char*) mmap(nullptr, FatalErrorMemorySize,
                                     PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
    MOZ_RELEASE_ASSERT(gFatalErrorMemory != MAP_FAILED);
  }
}

static void SendMessageToForkedProcess(Message::UniquePtr aMsg) {
  for (ForkedProcess& process : gForkedProcesses) {
    if (process.mForkId == aMsg->mForkId) {
      bool remove =
          aMsg->mType == MessageType::Terminate ||
          aMsg->mType == MessageType::Crash;
      process.mChannel->SendMessage(std::move(*aMsg));
      if (remove) {
        gForkedProcesses.erase(&process);
      }
      return;
    }
  }

  gPendingForkMessages.append(std::move(aMsg));
}

static bool MaybeHandleExternalCallResponse(const Message& aMsg) {
  // Remember the results of any external calls that have been made, in case
  // they show up again later.
  if (aMsg.mType == MessageType::ExternalCallResponse) {
    const auto& nmsg = static_cast<const ExternalCallResponseMessage&>(aMsg);
    AddExternalCallOutput(nmsg.mTag, nmsg.BinaryData(), nmsg.BinaryDataSize());
    return true;
  }
  return false;
}

static void HandleMessageToForkedProcess(Message::UniquePtr aMsg) {
  MaybeHandleExternalCallResponse(*aMsg);
  SendMessageToForkedProcess(std::move(aMsg));
}

static void HandleMessageFromForkedProcess(Message::UniquePtr aMsg) {
  // Try to handle external calls with data in this process, instead of
  // forwarding them (potentially across a network connection) to the middleman.
  if (aMsg->mType == MessageType::ExternalCallRequest) {
    const auto& nmsg = static_cast<const ExternalCallRequestMessage&>(*aMsg);

    InfallibleVector<char> outputData;
    if (HasExternalCallOutput(nmsg.mTag, &outputData)) {
      Message::UniquePtr response(ExternalCallResponseMessage::New(
          nmsg.mForkId, nmsg.mTag, outputData.begin(), outputData.length()));
      SendMessageToForkedProcess(std::move(response));
      return;
    }
  }

  if (MaybeHandleExternalCallResponse(*aMsg)) {
    // CallResponse messages from forked processes are intended for this one.
    // Don't notify the middleman.
    return;
  }

  gChannel->SendMessage(std::move(*aMsg));
}

static const size_t ForkTimeoutSeconds = 10;

void RegisterFork(size_t aForkId) {
  AutoPassThroughThreadEvents pt;

  gForkId = aForkId;
  gChannel = new Channel(0, Channel::Kind::ReplayForked, ChannelMessageHandler);

  ForkedProcess process;
  process.mPid = getpid();
  process.mForkId = aForkId;
  int nbytes = write(gForkWriteFd, &process, sizeof(process));
  MOZ_RELEASE_ASSERT(nbytes == sizeof(process));

  // If the root process is exiting while we are setting up the channel, it will
  // not connect to this process and we won't be able to shut down properly.
  // Set a timeout to avoid this situation.
  TimeStamp deadline =
      TimeStamp::Now() + TimeDuration::FromSeconds(ForkTimeoutSeconds);
  gChannel->ExitIfNotInitializedBefore(deadline);
}

void ReportCrash(const MinidumpInfo& aInfo, void* aFaultingAddress) {
  int pid;
  pid_for_task(aInfo.mTask, &pid);

  uint32_t forkId = UINT32_MAX;
  if (aInfo.mTask != mach_task_self()) {
    for (const ForkedProcess& fork : gForkedProcesses) {
      if (fork.mPid == pid) {
        forkId = fork.mForkId;
      }
    }
    if (forkId == UINT32_MAX) {
      Print("Could not find fork ID for crashing task\n");
    }
  }

  AutoEnsurePassThroughThreadEvents pt;

#ifdef MOZ_CRASHREPORTER
  google_breakpad::ExceptionHandler::WriteForwardedExceptionMinidump(
      aInfo.mExceptionType, aInfo.mCode, aInfo.mSubcode, aInfo.mThread,
      aInfo.mTask);
#endif

  char buf[2048];
  if (gFatalErrorMemory && gFatalErrorMemory[0]) {
    SprintfLiteral(buf, "%s", gFatalErrorMemory);
    memset(gFatalErrorMemory, 0, FatalErrorMemorySize);
  } else {
    SprintfLiteral(buf, "Fault %p", aFaultingAddress);
  }

  // Construct a FatalErrorMessage on the stack, to avoid touching the heap.
  char msgBuf[4096];
  size_t header = sizeof(FatalErrorMessage);
  size_t len = std::min(strlen(buf) + 1, sizeof(msgBuf) - header);
  FatalErrorMessage* msg = new (msgBuf) FatalErrorMessage(header + len, forkId);
  memcpy(&msgBuf[header], buf, len);
  msgBuf[sizeof(msgBuf) - 1] = 0;

  // Don't take the message lock when sending this, to avoid touching the heap.
  gChannel->SendMessage(std::move(*msg));

  Print("***** Fatal Record/Replay Error #%lu:%lu *****\n%s\n", GetId(), forkId,
        buf);
}

void ReportFatalError(const char* aFormat, ...) {
  if (!gFatalErrorMemory) {
    gFatalErrorMemory = new char[4096];
  }

  va_list ap;
  va_start(ap, aFormat);
  vsnprintf(gFatalErrorMemory, FatalErrorMemorySize - 1, aFormat, ap);
  va_end(ap);

  Print("FatalError: %s\n", gFatalErrorMemory);

  MOZ_CRASH("ReportFatalError");
}

void ReportUnhandledDivergence() {
  gChannel->SendMessage(UnhandledDivergenceMessage(gForkId));

  // Block until we get a terminate message and die.
  Thread::WaitForeverNoIdle();
}

size_t GetId() { return gChannel->GetId(); }
size_t GetForkId() { return gForkId; }

void AddPendingRecordingData() {
  Thread::WaitForIdleThreads();

  InfallibleVector<Stream*> updatedStreams;
  {
    MonitorAutoLock lock(*gMonitor);

    MOZ_RELEASE_ASSERT(!gPendingRecordingData.empty());

    gRecording->NewContents((const uint8_t*)gPendingRecordingData.begin(),
                            gPendingRecordingData.length(), &updatedStreams);
    gPendingRecordingData.clear();
  }

  for (Stream* stream : updatedStreams) {
    if (stream->Name() == StreamName::Lock) {
      Lock::LockAcquiresUpdated(stream->NameIndex());
    }
  }

  Thread::ResumeIdleThreads();
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

bool EncodeGraphics(nsACString& aData) {
  // Get an image encoder for the media type.
  nsCString encoderCID("@mozilla.org/image/encoder;2?type=image/png");
  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(encoderCID.get());

  size_t stride = layers::ImageDataSerializer::ComputeRGBStride(gSurfaceFormat,
                                                                gPaintWidth);

  nsString options;
  nsresult rv = encoder->InitFromData(
      (const uint8_t*)gDrawTargetBuffer, stride * gPaintHeight, gPaintWidth,
      gPaintHeight, stride, imgIEncoder::INPUT_FORMAT_HOSTARGB, options);
  if (NS_FAILED(rv)) {
    return false;
  }

  uint64_t count;
  rv = encoder->Available(&count);
  if (NS_FAILED(rv)) {
    return false;
  }

  rv = Base64EncodeInputStream(encoder, aData, count);
  return NS_SUCCEEDED(rv);
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
    if (IsRecording()) {
      memcpy(gGraphicsShmem, gDrawTargetBuffer, gDrawTargetBufferSize);
      gChannel->SendMessage(PaintMessage(gPaintWidth, gPaintHeight));
    } else {
      AutoPassThroughThreadEvents pt;

      nsAutoCString data;
      if (!EncodeGraphics(data)) {
        MOZ_CRASH("EncodeGraphics failed");
      }

      Message* msg = PaintEncodedMessage::New(gForkId, 0, data.BeginReading(),
                                              data.Length());
      gChannel->SendMessage(std::move(*msg));
      free(msg);
    }
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

bool Repaint(nsACString& aData) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(HasDivergedFromRecording());

  // Don't try to repaint if the first normal paint hasn't occurred yet.
  if (!gCompositorThreadId) {
    return false;
  }

  // Ignore the request to repaint if we already triggered a repaint, in which
  // case the last graphics we sent will still be correct.
  if (!gDidRepaint) {
    gDidRepaint = true;
    gRepainting = true;

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

    gRepainting = false;
  }

  if (!gDrawTargetBuffer) {
    return false;
  }

  return EncodeGraphics(aData);
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
      ManifestFinishedMessage::New(gForkId, aBuffer.begin(), aBuffer.length());
  PauseMainThreadAndInvokeCallback([=]() {
    gChannel->SendMessage(std::move(*msg));
    free(msg);
  });
}

void SendExternalCallRequest(ExternalCallId aId,
                             const char* aInputData, size_t aInputSize,
                             InfallibleVector<char>* aOutputData) {
  AutoPassThroughThreadEvents pt;
  MonitorAutoLock lock(*gMonitor);

  while (gWaitingForCallResponse) {
    gMonitor->Wait();
  }
  gWaitingForCallResponse = true;

  UniquePtr<ExternalCallRequestMessage> msg(ExternalCallRequestMessage::New(
      gForkId, aId, aInputData, aInputSize));
  gChannel->SendMessage(std::move(*msg));

  while (!gCallResponseMessage) {
    gMonitor->Wait();
  }

  aOutputData->append(gCallResponseMessage->BinaryData(),
                      gCallResponseMessage->BinaryDataSize());

  gCallResponseMessage = nullptr;
  gWaitingForCallResponse = false;

  gMonitor->Notify();
}

void SendExternalCallOutput(ExternalCallId aId,
                            const char* aOutputData, size_t aOutputSize) {
  Message::UniquePtr msg(ExternalCallResponseMessage::New(
      gForkId, aId, aOutputData, aOutputSize));
  gChannel->SendMessage(std::move(*msg));
}

void SendRecordingData(size_t aStart, const uint8_t* aData, size_t aSize) {
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  RecordingDataMessage* msg =
      RecordingDataMessage::New(gForkId, aStart, (const char*)aData, aSize);
  gChannel->SendMessage(std::move(*msg));
  free(msg);
}

}  // namespace child
}  // namespace recordreplay
}  // namespace mozilla
