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
#include "mozilla/StackWalk.h"
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

static void DumpRecordingAssertions();

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

  if (IsReplaying() && TestEnv("DUMP_RECORDING")) {
    DumpRecordingAssertions();
  }

  InitializeTriggers();
  InitializeWeakPointers();
  InitializeMemorySnapshots();
  Thread::SpawnAllThreads();
  InitializeCountdownThread();
  SetupDirtyMemoryHandler();

  thread->SetPassThrough(false);

  Lock::InitializeLocks();
  InitializeRewindState();

  gInitialized = true;
}

MOZ_EXPORT size_t
RecordReplayInterface_InternalRecordReplayValue(size_t aValue)
{
  MOZ_ASSERT(IsRecordingOrReplaying());

  if (AreThreadEventsPassedThrough()) {
    return aValue;
  }
  EnsureNotDivergedFromRecording();

  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());
  Thread* thread = Thread::Current();

  RecordReplayAssert("Value");
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Value);
  thread->Events().RecordOrReplayValue(&aValue);
  return aValue;
}

MOZ_EXPORT void
RecordReplayInterface_InternalRecordReplayBytes(void* aData, size_t aSize)
{
  MOZ_ASSERT(IsRecordingOrReplaying());

  if (AreThreadEventsPassedThrough()) {
    return;
  }
  EnsureNotDivergedFromRecording();

  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());
  Thread* thread = Thread::Current();

  RecordReplayAssert("Bytes %d", (int) aSize);
  thread->Events().RecordOrReplayThreadEvent(ThreadEvent::Bytes);
  thread->Events().CheckInput(aSize);
  thread->Events().RecordOrReplayBytes(aData, aSize);
}

MOZ_EXPORT void
RecordReplayInterface_InternalInvalidateRecording(const char* aWhy)
{
  if (IsRecording()) {
    child::ReportFatalError("Recording invalidated: %s", aWhy);
  } else {
    child::ReportFatalError("Recording invalidated while replaying: %s", aWhy);
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

///////////////////////////////////////////////////////////////////////////////
// Record/Replay Assertions
///////////////////////////////////////////////////////////////////////////////

struct StackWalkData
{
  char* mBuf;
  size_t mSize;

  StackWalkData(char* aBuf, size_t aSize)
    : mBuf(aBuf), mSize(aSize)
  {}

  void append(const char* aText) {
    size_t len = strlen(aText);
    if (len <= mSize) {
      strcpy(mBuf, aText);
      mBuf += len;
      mSize -= len;
    }
  }
};

static void
StackWalkCallback(uint32_t aFrameNumber, void* aPC, void* aSP, void* aClosure)
{
  StackWalkData* data = (StackWalkData*) aClosure;

  MozCodeAddressDetails details;
  MozDescribeCodeAddress(aPC, &details);

  data->append(" ### ");
  data->append(details.function[0] ? details.function : "???");
}

static void
SetCurrentStackString(const char* aAssertion, char* aBuf, size_t aSize)
{
  size_t frameCount = 12;

  // Locking operations usually have extra stack goop.
  if (!strcmp(aAssertion, "Lock 1")) {
    frameCount += 8;
  } else if (!strncmp(aAssertion, "Lock ", 5)) {
    frameCount += 4;
  }

  StackWalkData data(aBuf, aSize);
  MozStackWalk(StackWalkCallback, /* aSkipFrames = */ 2, frameCount, &data);
}

// For debugging.
char*
PrintCurrentStackString()
{
  AutoEnsurePassThroughThreadEvents pt;
  char* buf = new char[1000];
  SetCurrentStackString("", buf, 1000);
  return buf;
}

static inline bool
AlwaysCaptureEventStack(const char* aText)
{
  return false;
}

// Bit included in assertion stream when the assertion is a text assert, rather
// than a byte sequence.
static const size_t AssertionBit = 1;

extern "C" {

MOZ_EXPORT void
RecordReplayInterface_InternalRecordReplayAssert(const char* aFormat, va_list aArgs)
{
#ifdef INCLUDE_RECORD_REPLAY_ASSERTIONS
  if (AreThreadEventsPassedThrough() || HasDivergedFromRecording()) {
    return;
  }

  MOZ_RELEASE_ASSERT(!AreThreadEventsDisallowed());
  Thread* thread = Thread::Current();

  // Record an assertion string consisting of the name of the assertion and
  // stack information about the current point of execution.
  char text[1024];
  VsprintfLiteral(text, aFormat, aArgs);
  if (IsRecording() && (thread->ShouldCaptureEventStacks() || AlwaysCaptureEventStack(text))) {
    AutoPassThroughThreadEvents pt;
    SetCurrentStackString(text, text + strlen(text), sizeof(text) - strlen(text));
  }

  size_t textLen = strlen(text);

  if (IsRecording()) {
    thread->Asserts().WriteScalar(thread->Events().StreamPosition());
    if (thread->IsMainThread()) {
      thread->Asserts().WriteScalar(*ExecutionProgressCounter());
    }
    thread->Asserts().WriteScalar((textLen << 1) | AssertionBit);
    thread->Asserts().WriteBytes(text, textLen);
  } else {
    // While replaying, both the assertion's name and the current position in
    // the thread's events need to match up with what was recorded. The stack
    // portion of the assertion text does not need to match, it is used to help
    // track down the reason for the mismatch.
    bool match = true;
    size_t streamPos = thread->Asserts().ReadScalar();
    if (streamPos != thread->Events().StreamPosition()) {
      match = false;
    }
    size_t progress = 0;
    if (thread->IsMainThread()) {
      progress = thread->Asserts().ReadScalar();
      if (progress != *ExecutionProgressCounter()) {
        match = false;
      }
    }
    size_t assertLen = thread->Asserts().ReadScalar() >> 1;

    char* buffer = thread->TakeBuffer(assertLen + 1);

    thread->Asserts().ReadBytes(buffer, assertLen);
    buffer[assertLen] = 0;

    if (assertLen < textLen || memcmp(buffer, text, textLen) != 0) {
      match = false;
    }

    if (!match) {
      for (int i = Thread::NumRecentAsserts - 1; i >= 0; i--) {
        if (thread->RecentAssert(i).mText) {
          Print("Thread %d Recent %d: %s [%d]\n",
                (int) thread->Id(), (int) i,
                thread->RecentAssert(i).mText, (int) thread->RecentAssert(i).mPosition);
        }
      }

      {
        AutoPassThroughThreadEvents pt;
        SetCurrentStackString(text, text + strlen(text), sizeof(text) - strlen(text));
      }

      child::ReportFatalError("Assertion Mismatch: Thread %d\n"
                              "Recorded: %s [%d,%d]\n"
                              "Replayed: %s [%d,%d]\n",
                              (int) thread->Id(), buffer, (int) streamPos, (int) progress, text,
                              (int) thread->Events().StreamPosition(),
                              (int) (thread->IsMainThread() ? *ExecutionProgressCounter() : 0));
      Unreachable();
    }

    thread->RestoreBuffer(buffer);

    // Push this assert onto the recent assertions in the thread.
    free(thread->RecentAssert(Thread::NumRecentAsserts - 1).mText);
    for (size_t i = Thread::NumRecentAsserts - 1; i >= 1; i--) {
      thread->RecentAssert(i) = thread->RecentAssert(i - 1);
    }
    thread->RecentAssert(0).mText = strdup(text);
    thread->RecentAssert(0).mPosition = thread->Events().StreamPosition();
  }
#endif // INCLUDE_RECORD_REPLAY_ASSERTIONS
}

MOZ_EXPORT void
RecordReplayInterface_InternalRecordReplayAssertBytes(const void* aData, size_t aSize)
{
#ifdef INCLUDE_RECORD_REPLAY_ASSERTIONS
  RecordReplayAssert("AssertBytes");

  if (AreThreadEventsPassedThrough() || HasDivergedFromRecording()) {
    return;
  }

  MOZ_ASSERT(!AreThreadEventsDisallowed());
  Thread* thread = Thread::Current();

  if (IsRecording()) {
    thread->Asserts().WriteScalar(thread->Events().StreamPosition());
    thread->Asserts().WriteScalar(aSize << 1);
    thread->Asserts().WriteBytes(aData, aSize);
  } else {
    bool match = true;
    size_t streamPos = thread->Asserts().ReadScalar();
    if (streamPos != thread->Events().StreamPosition()) {
      match = false;
    }
    size_t oldSize = thread->Asserts().ReadScalar() >> 1;
    if (oldSize != aSize) {
      match = false;
    }

    char* buffer = thread->TakeBuffer(oldSize);

    thread->Asserts().ReadBytes(buffer, oldSize);
    if (match && memcmp(buffer, aData, oldSize) != 0) {
      match = false;
    }

    if (!match) {
      // On a byte mismatch, print out some of the mismatched bytes, up to a
      // cutoff in case there are many mismatched bytes.
      if (oldSize == aSize) {
        static const size_t MAX_MISMATCHES = 100;
        size_t mismatches = 0;
        for (size_t i = 0; i < aSize; i++) {
          if (((char*)aData)[i] != buffer[i]) {
            Print("Position %d: %d %d\n", (int) i, (int) buffer[i], (int) ((char*)aData)[i]);
            if (++mismatches == MAX_MISMATCHES) {
              break;
            }
          }
        }
        if (mismatches == MAX_MISMATCHES) {
          Print("Position ...\n");
        }
      }

      child::ReportFatalError("Byte Comparison Check Failed: Position %d %d Length %d %d\n",
                              (int) streamPos, (int) thread->Events().StreamPosition(),
                              (int) oldSize, (int) aSize);
      Unreachable();
    }

    thread->RestoreBuffer(buffer);
  }
#endif // INCLUDE_RECORD_REPLAY_ASSERTIONS
}

MOZ_EXPORT void
RecordReplayRust_Assert(const uint8_t* aBuffer)
{
  RecordReplayAssert("%s", (const char*) aBuffer);
}

MOZ_EXPORT void
RecordReplayRust_BeginPassThroughThreadEvents()
{
  BeginPassThroughThreadEvents();
}

MOZ_EXPORT void
RecordReplayRust_EndPassThroughThreadEvents()
{
  EndPassThroughThreadEvents();
}

} // extern "C"

static void
DumpRecordingAssertions()
{
  Thread* thread = Thread::Current();

  for (size_t id = MainThreadId; id <= MaxRecordedThreadId; id++) {
    Stream* asserts = gRecordingFile->OpenStream(StreamName::Assert, id);
    if (asserts->AtEnd()) {
      continue;
    }

    fprintf(stderr, "Thread Assertions %d:\n", (int) id);
    while (!asserts->AtEnd()) {
      (void) asserts->ReadScalar();
      size_t shiftedLen = asserts->ReadScalar();
      size_t assertLen = shiftedLen >> 1;

      char* buffer = thread->TakeBuffer(assertLen + 1);
      asserts->ReadBytes(buffer, assertLen);
      buffer[assertLen] = 0;

      if (shiftedLen & AssertionBit) {
        fprintf(stderr, "%s\n", buffer);
      }

      thread->RestoreBuffer(buffer);
    }
  }

  fprintf(stderr, "Done with assertions, exiting...\n");
  _exit(0);
}

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
