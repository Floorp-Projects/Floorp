/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessRecordReplay.h"

#include "ipc/ChildInternal.h"
#include "mozilla/Compression.h"
#include "mozilla/Maybe.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticMutex.h"
#include "DirtyMemoryHandler.h"
#include "Lock.h"
#include "MemorySnapshot.h"
#include "ProcessRedirect.h"
#include "ProcessRewind.h"
#include "Trigger.h"
#include "ValueIndex.h"
#include "WeakPointer.h"
#include "pratom.h"

#include <fcntl.h>
#include <unistd.h>

namespace mozilla {
namespace recordreplay {

MOZ_NEVER_INLINE void
BusyWait()
{
  static volatile int value = 1;
  while (value) {}
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

// Whether to spew record/replay messages to stderr.
static bool gSpewEnabled;

extern "C" {

MOZ_EXPORT void
RecordReplayInterface_Initialize(int aArgc, char* aArgv[])
{
  // Parse command line options for the process kind and recording file.
  Maybe<ProcessKind> processKind;
  Maybe<char*> recordingFile;
  for (int i = 0; i < aArgc; i++) {
    if (!strcmp(aArgv[i], gProcessKindOption)) {
      MOZ_RELEASE_ASSERT(processKind.isNothing() && i + 1 < aArgc);
      processKind.emplace((ProcessKind) atoi(aArgv[i + 1]));
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

  if (IsRecordingOrReplaying() && TestEnv("WAIT_AT_START")) {
    BusyWait();
  }

  if (IsMiddleman() && TestEnv("MIDDLEMAN_WAIT_AT_START")) {
    BusyWait();
  }

  gPid = getpid();
  if (TestEnv("RECORD_REPLAY_SPEW")) {
    gSpewEnabled = true;
  }

  EarlyInitializeRedirections();

  if (!IsRecordingOrReplaying()) {
    return;
  }

  gSnapshotMemoryPrefix = mktemp(strdup("/tmp/SnapshotMemoryXXXXXX"));
  gSnapshotStackPrefix = mktemp(strdup("/tmp/SnapshotStackXXXXXX"));

  InitializeCurrentTime();

  gRecordingFile = new File();
  if (!gRecordingFile->Open(recordingFile.ref(), IsRecording() ? File::WRITE : File::READ)) {
    gInitializationFailureMessage = strdup("Bad recording file");
    return;
  }

  if (!InitializeRedirections()) {
    MOZ_RELEASE_ASSERT(gInitializationFailureMessage);
    return;
  }

  Thread::InitializeThreads();

  Thread* thread = Thread::GetById(MainThreadId);
  MOZ_ASSERT(thread->Id() == MainThreadId);

  thread->BindToCurrent();
  thread->SetPassThrough(true);

  InitializeTriggers();
  InitializeWeakPointers();
  InitializeMemorySnapshots();
  Thread::SpawnAllThreads();
  InitializeCountdownThread();
  SetupDirtyMemoryHandler();

  // Don't create a stylo thread pool when recording or replaying.
  putenv((char*) "STYLO_THREADS=1");

  thread->SetPassThrough(false);

  Lock::InitializeLocks();
  InitializeRewindState();

  gInitialized = true;
}

MOZ_EXPORT size_t
RecordReplayInterface_InternalRecordReplayValue(size_t aValue)
{
  Thread* thread = Thread::Current();
  if (thread->PassThroughEvents()) {
    return aValue;
  }
  EnsureNotDivergedFromRecording();

  MOZ_RELEASE_ASSERT(thread->CanAccessRecording());
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Value);
  thread->Events().RecordOrReplayValue(&aValue);
  return aValue;
}

MOZ_EXPORT void
RecordReplayInterface_InternalRecordReplayBytes(void* aData, size_t aSize)
{
  Thread* thread = Thread::Current();
  if (thread->PassThroughEvents()) {
    return;
  }
  EnsureNotDivergedFromRecording();

  MOZ_RELEASE_ASSERT(thread->CanAccessRecording());
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Bytes);
  thread->Events().CheckInput(aSize);
  thread->Events().RecordOrReplayBytes(aData, aSize);
}

MOZ_EXPORT void
RecordReplayInterface_InternalInvalidateRecording(const char* aWhy)
{
  if (IsRecording()) {
    child::ReportFatalError(Nothing(), "Recording invalidated: %s", aWhy);
  } else {
    child::ReportFatalError(Nothing(), "Recording invalidated while replaying: %s", aWhy);
  }
  Unreachable();
}

} // extern "C"

// How many recording endpoints have been flushed to the recording.
static size_t gNumEndpoints;

void
FlushRecording()
{
  MOZ_RELEASE_ASSERT(IsRecording());
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());

  // Save the endpoint of the recording.
  js::ExecutionPoint endpoint = navigation::GetRecordingEndpoint();
  Stream* endpointStream = gRecordingFile->OpenStream(StreamName::Main, 0);
  endpointStream->WriteScalar(++gNumEndpoints);
  endpointStream->WriteBytes(&endpoint, sizeof(endpoint));

  gRecordingFile->PreventStreamWrites();

  gRecordingFile->Flush();

  child::NotifyFlushedRecording();

  gRecordingFile->AllowStreamWrites();
}

// Try to load another recording index, returning whether one was found.
static bool
LoadNextRecordingIndex()
{
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

bool
HitRecordingEndpoint()
{
  MOZ_RELEASE_ASSERT(IsReplaying());
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());

  // The debugger will call this method in a loop, so we don't have to do
  // anything fancy to try to get the most up to date endpoint. As long as we
  // can make some progress in attempting to find a later endpoint, we can
  // return control to the debugger.

  // Check if there is a new endpoint in the endpoint data stream.
  Stream* endpointStream = gRecordingFile->OpenStream(StreamName::Main, 0);
  if (!endpointStream->AtEnd()) {
    js::ExecutionPoint endpoint;
    size_t index = endpointStream->ReadScalar();
    endpointStream->ReadBytes(&endpoint, sizeof(endpoint));
    navigation::SetRecordingEndpoint(index, endpoint);
    return true;
  }

  // Check if there is more data in the recording.
  if (LoadNextRecordingIndex()) {
    return true;
  }

  // OK, we hit the most up to date endpoint in the recording.
  return false;
}

void
HitEndOfRecording()
{
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

bool
SpewEnabled()
{
  return gSpewEnabled;
}

void
InternalPrint(const char* aFormat, va_list aArgs)
{
  char buf1[2048];
  VsprintfLiteral(buf1, aFormat, aArgs);
  char buf2[2048];
  SprintfLiteral(buf2, "Spew[%d]: %s", gPid, buf1);
  DirectPrint(buf2);
}

const char*
ThreadEventName(ThreadEvent aEvent)
{
  switch (aEvent) {
#define EnumToString(Kind) case ThreadEvent::Kind: return #Kind;
    ForEachThreadEvent(EnumToString)
#undef EnumToString
  case ThreadEvent::CallStart: break;
  }
  size_t callId = (size_t) aEvent - (size_t) ThreadEvent::CallStart;
  return gRedirections[callId].mName;
}

///////////////////////////////////////////////////////////////////////////////
// Record/Replay Assertions
///////////////////////////////////////////////////////////////////////////////

extern "C" {

MOZ_EXPORT void
RecordReplayInterface_InternalRecordReplayAssert(const char* aFormat, va_list aArgs)
{
  Thread* thread = Thread::Current();
  if (thread->PassThroughEvents() || thread->HasDivergedFromRecording()) {
    return;
  }
  MOZ_RELEASE_ASSERT(thread->CanAccessRecording());

  // Add the asserted string to the recording.
  char text[1024];
  VsprintfLiteral(text, aFormat, aArgs);

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Assert);
  thread->Events().CheckInput(text);
}

MOZ_EXPORT void
RecordReplayInterface_InternalRecordReplayAssertBytes(const void* aData, size_t aSize)
{
  Thread* thread = Thread::Current();
  if (thread->PassThroughEvents() || thread->HasDivergedFromRecording()) {
    return;
  }
  MOZ_RELEASE_ASSERT(thread->CanAccessRecording());

  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::AssertBytes);
  thread->Events().CheckInput(aData, aSize);
}

} // extern "C"

static ValueIndex* gGenericThings;
static StaticMutexNotRecorded gGenericThingsMutex;

extern "C" {

MOZ_EXPORT void
RecordReplayInterface_InternalRegisterThing(void* aThing)
{
  if (AreThreadEventsPassedThrough()) {
    return;
  }

  AutoOrderedAtomicAccess at;
  StaticMutexAutoLock lock(gGenericThingsMutex);
  if (!gGenericThings) {
    gGenericThings = new ValueIndex();
  }
  if (gGenericThings->Contains(aThing)) {
    gGenericThings->Remove(aThing);
  }
  gGenericThings->Insert(aThing);
}

MOZ_EXPORT void
RecordReplayInterface_InternalUnregisterThing(void* aThing)
{
  StaticMutexAutoLock lock(gGenericThingsMutex);
  if (gGenericThings) {
    gGenericThings->Remove(aThing);
  }
}

MOZ_EXPORT size_t
RecordReplayInterface_InternalThingIndex(void* aThing)
{
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

MOZ_EXPORT const char*
RecordReplayInterface_InternalVirtualThingName(void* aThing)
{
  void* vtable = *(void**)aThing;
  const char* name = SymbolNameRaw(vtable);
  return name ? name : "(unknown)";
}

} // extern "C"

} // namespace recordreplay
} // namespace mozilla
