/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessRecordReplay.h"

#include "ipc/ChildInternal.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/Compression.h"
#include "mozilla/Maybe.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticMutex.h"
#include "Lock.h"
#include "ProcessRedirect.h"
#include "ProcessRewind.h"
#include "ValueIndex.h"
#include "pratom.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <mach/exc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/ndr.h>
#include <sys/time.h>

namespace mozilla {
namespace recordreplay {

MOZ_NEVER_INLINE void BusyWait() {
  static volatile int value = 1;
  while (value) {
  }
}

///////////////////////////////////////////////////////////////////////////////
// Basic interface
///////////////////////////////////////////////////////////////////////////////

Recording* gRecording;

char* gInitializationFailureMessage;

bool gInitialized;
ProcessKind gProcessKind;
char* gRecordingFilename;

// Current process ID.
static int gPid;

// ID of the process which produced the recording.
static int gRecordingPid;

// Whether to spew record/replay messages to stderr.
static bool gSpewEnabled;

// Whether this is the main child.
static bool gMainChild;

// Whether we are replaying on a cloud machine.
static bool gReplayingInCloud;

// Firefox installation directory.
static char* gInstallDirectory;

// Location within the installation directory where the current executable
// should be running.
static const char gExecutableSuffix[] =
    "Contents/MacOS/plugin-container.app/Contents/MacOS/plugin-container";

static void InitializeCrashDetector();

extern "C" {

MOZ_EXPORT void RecordReplayInterface_Initialize(int aArgc, char* aArgv[]) {
  // Parse command line options for the process kind and recording file.
  Maybe<ProcessKind> processKind;
  Maybe<char*> recordingFile;
  for (int i = 0; i < aArgc; i++) {
    if (!strcmp(aArgv[i], gProcessKindOption)) {
      MOZ_RELEASE_ASSERT(processKind.isNothing() && i + 1 < aArgc);
      processKind.emplace((ProcessKind)atoi(aArgv[i + 1]));
    }
    if (!strcmp(aArgv[i], gRecordingFileOption)) {
      MOZ_RELEASE_ASSERT(recordingFile.isNothing() && i + 1 < aArgc);
      recordingFile.emplace(aArgv[i + 1]);
    }
  }
  MOZ_RELEASE_ASSERT(processKind.isSome());

  gProcessKind = processKind.ref();
  if (recordingFile.isSome()) {
    gRecordingFilename = strdup(recordingFile.ref());
  }

  switch (processKind.ref()) {
    case ProcessKind::Recording:
      gIsRecording = gIsRecordingOrReplaying = true;
      fprintf(stderr, "RECORDING %d %s\n", getpid(), recordingFile.ref());
      break;
    case ProcessKind::Replaying:
      gIsReplaying = gIsRecordingOrReplaying = true;
      fprintf(stderr, "REPLAYING %d %s\n", getpid(), recordingFile.ref());
      break;
    case ProcessKind::MiddlemanRecording:
    case ProcessKind::MiddlemanReplaying:
      gIsMiddleman = true;
      fprintf(stderr, "MIDDLEMAN %d %s\n", getpid(), recordingFile.ref());
      break;
    default:
      MOZ_CRASH("Bad ProcessKind");
  }

  if (IsRecording() && TestEnv("MOZ_RECORDING_WAIT_AT_START")) {
    BusyWait();
  }

  if (IsReplaying() && TestEnv("MOZ_REPLAYING_WAIT_AT_START")) {
    BusyWait();
  }

  if (IsMiddleman() && TestEnv("MOZ_MIDDLEMAN_WAIT_AT_START")) {
    BusyWait();
  }

  gPid = getpid();
  if (TestEnv("MOZ_RECORD_REPLAY_SPEW")) {
    gSpewEnabled = true;
  }

  InitializeRedirections();

  if (!IsRecordingOrReplaying()) {
    InitializeExternalCalls();
    return;
  }

  // Try to determine the install directory from the current executable path.
  const char* executable = aArgv[0];
  size_t executableLength = strlen(executable);
  size_t suffixLength = strlen(gExecutableSuffix);
  if (executableLength >= suffixLength) {
    const char* suffixStart = executable + executableLength - suffixLength;
    if (!strcmp(suffixStart, gExecutableSuffix)) {
      size_t directoryLength = suffixStart - executable;
      gInstallDirectory = new char[directoryLength + 1];
      memcpy(gInstallDirectory, executable, directoryLength);
      gInstallDirectory[directoryLength] = 0;
    }
  }

  InitializeCurrentTime();

  gRecording = new Recording();

  ApplyLibraryRedirections(nullptr);

  Thread::InitializeThreads();

  Thread* thread = Thread::GetById(MainThreadId);
  MOZ_ASSERT(thread->Id() == MainThreadId);

  thread->SetPassThrough(true);

  // The translation layer we are running under in the cloud will intercept this
  // and return a non-zero symbol address.
  gReplayingInCloud = !!dlsym(RTLD_DEFAULT, "RecordReplay_ReplayingInCloud");

  Thread::SpawnAllThreads();
  InitializeExternalCalls();
  if (!gReplayingInCloud) {
    // The crash detector is only useful when we have a local parent process to
    // report crashes to. Avoid initializing it when running in the cloud
    // so that we avoid calling mach interfaces with events passed through.
    InitializeCrashDetector();
  }
  Lock::InitializeLocks();

  // Don't create a stylo thread pool when recording or replaying.
  putenv((char*)"STYLO_THREADS=1");

  child::SetupRecordReplayChannel(aArgc, aArgv);

  thread->SetPassThrough(false);

  InitializeRewindState();
  gRecordingPid = RecordReplayValue(gPid);

  gMainChild = IsRecording();

  gInitialized = true;
}

MOZ_EXPORT size_t
RecordReplayInterface_InternalRecordReplayValue(size_t aValue) {
  Thread* thread = Thread::Current();
  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    return aValue;
  }

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Value);
  thread->Events().RecordOrReplayValue(&aValue);
  return aValue;
}

MOZ_EXPORT void RecordReplayInterface_InternalRecordReplayBytes(void* aData,
                                                                size_t aSize) {
  Thread* thread = Thread::Current();
  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    return;
  }

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Bytes);
  thread->Events().CheckInput(aSize);
  thread->Events().RecordOrReplayBytes(aData, aSize);
}

MOZ_EXPORT void RecordReplayInterface_InternalInvalidateRecording(
    const char* aWhy) {
  if (IsRecording()) {
    child::ReportFatalError("Recording invalidated: %s", aWhy);
  } else {
    child::ReportFatalError("Recording invalidated while replaying: %s", aWhy);
  }
  Unreachable();
}

MOZ_EXPORT void RecordReplayInterface_InternalBeginPassThroughThreadEventsWithLocalReplay() {
  if (IsReplaying() && !gReplayingInCloud) {
    BeginPassThroughThreadEvents();
  }
}

MOZ_EXPORT void RecordReplayInterface_InternalEndPassThroughThreadEventsWithLocalReplay() {
  // If we are replaying locally we will be skipping over a section of the
  // recording while events are passed through. Include the current stream
  // position in the recording so that we will know how much to skip over.
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  Stream* localReplayStream = gRecording->OpenStream(StreamName::LocalReplaySkip, 0);
  Stream& events = Thread::Current()->Events();

  size_t position = IsRecording() ? events.StreamPosition() : 0;
  localReplayStream->RecordOrReplayScalar(&position);

  if (IsReplaying() && !ReplayingInCloud()) {
    EndPassThroughThreadEvents();
    MOZ_RELEASE_ASSERT(events.StreamPosition() <= position);
    size_t nbytes = position - events.StreamPosition();
    void* buf = malloc(nbytes);
    events.ReadBytes(buf, nbytes);
    free(buf);
    MOZ_RELEASE_ASSERT(events.StreamPosition() == position);
  }
}

}  // extern "C"

// How many bytes have been sent from the recording to the middleman.
size_t gRecordingDataSentToMiddleman;

void FlushRecording() {
  MOZ_RELEASE_ASSERT(IsRecording());
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());

  // The recording can only be flushed when we are at a checkpoint.
  // Save this endpoint to the recording.
  size_t endpoint = GetLastCheckpoint();
  Stream* endpointStream = gRecording->OpenStream(StreamName::Endpoint, 0);
  endpointStream->WriteScalar(endpoint);

  gRecording->PreventStreamWrites();
  gRecording->Flush();
  gRecording->AllowStreamWrites();

  if (gRecording->Size() > gRecordingDataSentToMiddleman) {
    child::SendRecordingData(gRecordingDataSentToMiddleman,
                             gRecording->Data() + gRecordingDataSentToMiddleman,
                             gRecording->Size() - gRecordingDataSentToMiddleman);
    gRecordingDataSentToMiddleman = gRecording->Size();
  }
}

void HitEndOfRecording() {
  MOZ_RELEASE_ASSERT(IsReplaying());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  if (Thread::CurrentIsMainThread()) {
    // We should have been provided with all the data needed to run forward in
    // the replay. Check to see if there is any pending data.
    child::AddPendingRecordingData();
  } else {
    // Non-main threads may wait until more recording data is added.
    Thread::Wait();
  }
}

// When replaying, the last endpoint loaded from the recording.
static size_t gRecordingEndpoint;

size_t RecordingEndpoint() {
  MOZ_RELEASE_ASSERT(IsReplaying());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  Stream* endpointStream = gRecording->OpenStream(StreamName::Endpoint, 0);
  while (!endpointStream->AtEnd()) {
    gRecordingEndpoint = endpointStream->ReadScalar();
  }

  return gRecordingEndpoint;
}

bool SpewEnabled() { return gSpewEnabled; }

void InternalPrint(const char* aFormat, va_list aArgs) {
  char buf1[2048];
  VsprintfLiteral(buf1, aFormat, aArgs);
  char buf2[2048];
  SprintfLiteral(buf2, "Spew[%d]: %s", gPid, buf1);
  DirectPrint(buf2);
}

const char* ThreadEventName(ThreadEvent aEvent) {
  switch (aEvent) {
#define EnumToString(Kind) \
  case ThreadEvent::Kind:  \
    return #Kind;
    ForEachThreadEvent(EnumToString)
#undef EnumToString
        case ThreadEvent::CallStart : break;
  }
  size_t callId = (size_t)aEvent - (size_t)ThreadEvent::CallStart;
  return GetRedirection(callId).mName;
}

int GetRecordingPid() { return gRecordingPid; }

void ResetPid() { gPid = getpid(); }

bool IsMainChild() { return gMainChild; }
void SetMainChild() { gMainChild = true; }
bool ReplayingInCloud() { return gReplayingInCloud; }
const char* InstallDirectory() { return gInstallDirectory; }

///////////////////////////////////////////////////////////////////////////////
// Record/Replay Assertions
///////////////////////////////////////////////////////////////////////////////

extern "C" {

MOZ_EXPORT void RecordReplayInterface_InternalRecordReplayAssert(
    const char* aFormat, va_list aArgs) {
  Thread* thread = Thread::Current();
  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    return;
  }

  // Add the asserted string to the recording.
  char text[1024];
  VsprintfLiteral(text, aFormat, aArgs);

  // This must be kept in sync with Stream::RecordOrReplayThreadEvent, which
  // peeks at the input string written after the thread event.
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Assert, text);
  thread->Events().CheckInput(text);
}

MOZ_EXPORT void RecordReplayInterface_InternalRecordReplayAssertBytes(
    const void* aData, size_t aSize) {
  Thread* thread = Thread::Current();
  RecordingEventSection res(thread);
  if (!res.CanAccessEvents()) {
    return;
  }

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::AssertBytes);
  thread->Events().CheckInput(aData, aSize);
}

}  // extern "C"

static ValueIndex* gGenericThings;
static StaticMutexNotRecorded gGenericThingsMutex;

extern "C" {

MOZ_EXPORT void RecordReplayInterface_InternalRegisterThing(void* aThing) {
  if (AreThreadEventsPassedThrough()) {
    return;
  }

  AutoOrderedAtomicAccess at(&gGenericThings);
  StaticMutexAutoLock lock(gGenericThingsMutex);
  if (!gGenericThings) {
    gGenericThings = new ValueIndex();
  }
  if (gGenericThings->Contains(aThing)) {
    gGenericThings->Remove(aThing);
  }
  gGenericThings->Insert(aThing);
}

MOZ_EXPORT void RecordReplayInterface_InternalUnregisterThing(void* aThing) {
  StaticMutexAutoLock lock(gGenericThingsMutex);
  if (gGenericThings) {
    gGenericThings->Remove(aThing);
  }
}

MOZ_EXPORT size_t RecordReplayInterface_InternalThingIndex(void* aThing) {
  if (!aThing) {
    return 0;
  }
  StaticMutexAutoLock lock(gGenericThingsMutex);
  size_t index = 0;
  if (gGenericThings) {
    gGenericThings->MaybeGetIndex(aThing, &index);
  }
  return index;
}

MOZ_EXPORT const char* RecordReplayInterface_InternalVirtualThingName(
    void* aThing) {
  void* vtable = *(void**)aThing;
  const char* name = SymbolNameRaw(vtable);
  return name ? name : "(unknown)";
}

MOZ_EXPORT void RecordReplayInterface_InternalHoldJSObject(void* aJSObj) {
  if (aJSObj) {
    JSContext* cx = dom::danger::GetJSContext();
    JS::PersistentRootedObject* root = new JS::PersistentRootedObject(cx);
    *root = static_cast<JSObject*>(aJSObj);
  }
}

}  // extern "C"

static mach_port_t gCrashDetectorExceptionPort;

// See AsmJSSignalHandlers.cpp.
static const mach_msg_id_t sExceptionId = 2405;

// This definition was generated by mig (the Mach Interface Generator) for the
// routine 'exception_raise' (exc.defs). See js/src/wasm/WasmSignalHandlers.cpp.
#pragma pack(4)
typedef struct {
  mach_msg_header_t Head;
  /* start of the kernel processed data */
  mach_msg_body_t msgh_body;
  mach_msg_port_descriptor_t thread;
  mach_msg_port_descriptor_t task;
  /* end of the kernel processed data */
  NDR_record_t NDR;
  exception_type_t exception;
  mach_msg_type_number_t codeCnt;
  int64_t code[2];
} Request__mach_exception_raise_t;
#pragma pack()

typedef struct {
  Request__mach_exception_raise_t body;
  mach_msg_trailer_t trailer;
} ExceptionRequest;

static void CrashDetectorThread(void*) {
  kern_return_t kret;

  while (true) {
    ExceptionRequest request;
    kret = mach_msg(&request.body.Head, MACH_RCV_MSG, 0, sizeof(request),
                    gCrashDetectorExceptionPort, MACH_MSG_TIMEOUT_NONE,
                    MACH_PORT_NULL);
    Print("Crashing: %s\n", gMozCrashReason);

    kern_return_t replyCode = KERN_FAILURE;
    if (kret == KERN_SUCCESS && request.body.Head.msgh_id == sExceptionId &&
        request.body.exception == EXC_BAD_ACCESS && request.body.codeCnt == 2) {
      uint8_t* faultingAddress = (uint8_t*)request.body.code[1];
      child::MinidumpInfo info(request.body.exception, request.body.code[0],
                               request.body.code[1],
                               request.body.thread.name,
                               request.body.task.name);
      child::ReportCrash(info, faultingAddress);
    } else {
      child::ReportFatalError("CrashDetectorThread mach_msg "
                              "returned unexpected data");
    }

    __Reply__exception_raise_t reply;
    reply.Head.msgh_bits =
        MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request.body.Head.msgh_bits), 0);
    reply.Head.msgh_size = sizeof(reply);
    reply.Head.msgh_remote_port = request.body.Head.msgh_remote_port;
    reply.Head.msgh_local_port = MACH_PORT_NULL;
    reply.Head.msgh_id = request.body.Head.msgh_id + 100;
    reply.NDR = NDR_record;
    reply.RetCode = replyCode;
    mach_msg(&reply.Head, MACH_SEND_MSG, sizeof(reply), 0, MACH_PORT_NULL,
             MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  }
}

static void InitializeCrashDetector() {
  MOZ_RELEASE_ASSERT(AreThreadEventsPassedThrough());
  kern_return_t kret;

  // Get a port which can send and receive data.
  kret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                            &gCrashDetectorExceptionPort);
  MOZ_RELEASE_ASSERT(kret == KERN_SUCCESS);

  kret = mach_port_insert_right(mach_task_self(), gCrashDetectorExceptionPort,
                                gCrashDetectorExceptionPort,
                                MACH_MSG_TYPE_MAKE_SEND);
  MOZ_RELEASE_ASSERT(kret == KERN_SUCCESS);

  // Create a thread to block on reading the port.
  Thread::SpawnNonRecordedThread(CrashDetectorThread, nullptr);

  // Set exception ports on the entire task. Unfortunately, this clobbers any
  // other exception ports for the task, and forwarding to those other ports
  // is not easy to get right.
  kret = task_set_exception_ports(
      mach_task_self(), EXC_MASK_BAD_ACCESS, gCrashDetectorExceptionPort,
      EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, THREAD_STATE_NONE);
  MOZ_RELEASE_ASSERT(kret == KERN_SUCCESS);
}

}  // namespace recordreplay
}  // namespace mozilla
