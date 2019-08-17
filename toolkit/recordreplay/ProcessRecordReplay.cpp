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
#include "DirtyMemoryHandler.h"
#include "Lock.h"
#include "MemorySnapshot.h"
#include "ProcessRedirect.h"
#include "ProcessRewind.h"
#include "ValueIndex.h"
#include "pratom.h"

#include <fcntl.h>
#include <unistd.h>

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

File* gRecordingFile;
const char* gSnapshotMemoryPrefix;
const char* gSnapshotStackPrefix;

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
  MOZ_RELEASE_ASSERT(processKind.isSome() && recordingFile.isSome());

  gProcessKind = processKind.ref();
  gRecordingFilename = strdup(recordingFile.ref());

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

  EarlyInitializeRedirections();

  if (!IsRecordingOrReplaying()) {
    InitializeMiddlemanCalls();
    return;
  }

  gSnapshotMemoryPrefix = mktemp(strdup("/tmp/SnapshotMemoryXXXXXX"));
  gSnapshotStackPrefix = mktemp(strdup("/tmp/SnapshotStackXXXXXX"));

  InitializeCurrentTime();

  gRecordingFile = new File();
  if (gRecordingFile->Open(recordingFile.ref(),
                           IsRecording() ? File::WRITE : File::READ)) {
    InitializeRedirections();
  } else {
    gInitializationFailureMessage = strdup("Bad recording file");
  }

  if (gInitializationFailureMessage) {
    fprintf(stderr, "Initialization Failure: %s\n",
            gInitializationFailureMessage);
  }

  LateInitializeRedirections();
  Thread::InitializeThreads();

  Thread* thread = Thread::GetById(MainThreadId);
  MOZ_ASSERT(thread->Id() == MainThreadId);

  thread->BindToCurrent();
  thread->SetPassThrough(true);

  InitializeMemorySnapshots();
  Thread::SpawnAllThreads();
  InitializeCountdownThread();
  SetupDirtyMemoryHandler();
  InitializeMiddlemanCalls();
  Lock::InitializeLocks();

  // Don't create a stylo thread pool when recording or replaying.
  putenv((char*)"STYLO_THREADS=1");

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
    child::ReportFatalError(Nothing(), "Recording invalidated: %s", aWhy);
  } else {
    child::ReportFatalError(Nothing(),
                            "Recording invalidated while replaying: %s", aWhy);
  }
  Unreachable();
}

}  // extern "C"

void FlushRecording() {
  MOZ_RELEASE_ASSERT(IsRecording());
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());

  // The recording can only be flushed when we are at a checkpoint.
  // Save this endpoint to the recording.
  size_t endpoint = GetLastCheckpoint();
  Stream* endpointStream = gRecordingFile->OpenStream(StreamName::Main, 0);
  endpointStream->WriteScalar(endpoint);

  gRecordingFile->PreventStreamWrites();
  gRecordingFile->Flush();
  gRecordingFile->AllowStreamWrites();
}

// Try to load another recording index, returning whether one was found.
static bool LoadNextRecordingIndex() {
  Thread::WaitForIdleThreads();

  InfallibleVector<Stream*> updatedStreams;
  File::ReadIndexResult result = gRecordingFile->ReadNextIndex(&updatedStreams);
  if (result == File::ReadIndexResult::InvalidFile) {
    MOZ_CRASH("Bad recording file");
  }

  bool found = result == File::ReadIndexResult::FoundIndex;
  if (found) {
    for (Stream* stream : updatedStreams) {
      if (stream->Name() == StreamName::Lock) {
        Lock::LockAquiresUpdated(stream->NameIndex());
      }
    }
  }

  Thread::ResumeIdleThreads();
  return found;
}

void HitEndOfRecording() {
  MOZ_RELEASE_ASSERT(IsReplaying());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  if (Thread::CurrentIsMainThread()) {
    // Load more data from the recording. The debugger is not allowed to let us
    // go past the recording endpoint, so there should be more data.
    bool found = LoadNextRecordingIndex();
    MOZ_RELEASE_ASSERT(found);
  } else {
    // Non-main threads may wait until more recording data is loaded by the
    // main thread.
    Thread::Wait();
  }
}

// When replaying, the last endpoint loaded from the recording.
static size_t gRecordingEndpoint;

size_t RecordingEndpoint() {
  MOZ_RELEASE_ASSERT(IsReplaying());
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough());

  Stream* endpointStream = gRecordingFile->OpenStream(StreamName::Main, 0);
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

bool IsMainChild() { return gMainChild; }
void SetMainChild() { gMainChild = true; }

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
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Assert);
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

MOZ_EXPORT void RecordReplayInterface_InternalHoldJSObject(JSObject* aJSObj) {
  if (aJSObj) {
    JSContext* cx = dom::danger::GetJSContext();
    JS::PersistentRootedObject* root = new JS::PersistentRootedObject(cx);
    *root = aJSObj;
  }
}

}  // extern "C"

}  // namespace recordreplay
}  // namespace mozilla
