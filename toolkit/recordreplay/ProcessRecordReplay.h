/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ProcessRecordReplay_h
#define mozilla_recordreplay_ProcessRecordReplay_h

#include "mozilla/PodOperations.h"
#include "mozilla/RecordReplay.h"

#include <algorithm>

namespace mozilla {
namespace recordreplay {

// Record/Replay Internal API
//
// See mfbt/RecordReplay.h for the main record/replay public API and a high
// level description of the record/replay system.
//
// This directory contains files used for recording, replaying, and rewinding a
// process. The ipc subdirectory contains files used for IPC between a
// replaying and middleman process, and between a middleman and chrome process.

// ID of an event in a thread's event stream. Each ID in the stream is followed
// by data associated with the event (see File::RecordOrReplayThreadEvent).
enum class ThreadEvent : uint32_t
{
  // Spawned another thread.
  CreateThread,

  // Created a recorded lock.
  CreateLock,

  // Acquired a recorded lock.
  Lock,

  // Wait for a condition variable with a timeout.
  WaitForCvarUntil,

  // Called RecordReplayValue.
  Value,

  // Called RecordReplayBytes.
  Bytes,

  // Executed a nested callback (see Callback.h).
  ExecuteCallback,

  // Finished executing nested callbacks in a library API (see Callback.h).
  CallbacksFinished,

  // Restoring a data pointer used in a callback (see Callback.h).
  RestoreCallbackData,

  // Executed a trigger within a call to ExecuteTriggers.
  ExecuteTrigger,

  // Finished executing triggers within a call to ExecuteTriggers.
  ExecuteTriggersFinished,

  // Encoded information about an argument/rval used by a graphics call.
  GraphicsArgument,
  GraphicsRval,

  // The start of event IDs for redirected call events. Event IDs after this
  // point are platform specific.
  CallStart
};

class File;

// File used during recording and replay.
extern File* gRecordingFile;

// Whether record/replay state has finished initialization.
extern bool gInitialized;

// If we failed to initialize, any associated message.
extern char* gInitializationFailureMessage;

// Whether record/replay assertions should be performed.
//#ifdef DEBUG
#define INCLUDE_RECORD_REPLAY_ASSERTIONS 1
//#endif

// Flush any new recording data to disk.
void FlushRecording();

// Called when any thread hits the end of its event stream.
void HitEndOfRecording();

// Called when the main thread hits the latest recording endpoint it knows
// about.
bool HitRecordingEndpoint();

// Possible directives to give via the RecordReplayDirective function.
enum class Directive
{
  // Crash at the next use of MaybeCrash.
  CrashSoon = 1,

  // Irrevocably crash if CrashSoon has ever been used on the process.
  MaybeCrash = 2,

  // Always save temporary checkpoints when stepping around in the debugger.
  AlwaysSaveTemporaryCheckpoints = 3,

  // Mark all future checkpoints as major checkpoints in the middleman.
  AlwaysMarkMajorCheckpoints = 4
};

// Get the process kind and recording file specified at the command line.
// These are available in the middleman as well as while recording/replaying.
extern ProcessKind gProcessKind;
extern char* gRecordingFilename;

///////////////////////////////////////////////////////////////////////////////
// Helper Functions
///////////////////////////////////////////////////////////////////////////////

// Wait indefinitely for a debugger to be attached.
void BusyWait();

static inline void Unreachable() { MOZ_CRASH("Unreachable"); }

// Get the symbol name for a function pointer address, if available.
const char* SymbolNameRaw(void* aAddress);

static inline bool
MemoryContains(void* aBase, size_t aSize, void* aPtr, size_t aPtrSize = 1)
{
  MOZ_ASSERT(aPtrSize);
  return (uint8_t*) aPtr >= (uint8_t*) aBase
      && (uint8_t*) aPtr + aPtrSize <= (uint8_t*) aBase + aSize;
}

static inline bool
MemoryIntersects(void* aBase0, size_t aSize0, void* aBase1, size_t aSize1)
{
  MOZ_ASSERT(aSize0 && aSize1);
  return MemoryContains(aBase0, aSize0, aBase1)
      || MemoryContains(aBase0, aSize0, (uint8_t*) aBase1 + aSize1 - 1)
      || MemoryContains(aBase1, aSize1, aBase0);
}

static const size_t PageSize = 4096;

static inline uint8_t*
PageBase(void* aAddress)
{
  return (uint8_t*)aAddress - ((size_t)aAddress % PageSize);
}

static inline size_t
RoundupSizeToPageBoundary(size_t aSize)
{
  if (aSize % PageSize) {
    return aSize + PageSize - (aSize % PageSize);
  }
  return aSize;
}

static inline bool
TestEnv(const char* env)
{
  const char* value = getenv(env);
  return value && value[0];
}

// Check for membership in a vector.
template <typename Vector, typename Entry>
inline bool
VectorContains(const Vector& aVector, const Entry& aEntry)
{
  return std::find(aVector.begin(), aVector.end(), aEntry) != aVector.end();
}

// Add or remove a unique entry to an unsorted vector.
template <typename Vector, typename Entry>
inline void
VectorAddOrRemoveEntry(Vector& aVector, const Entry& aEntry, bool aAdding)
{
  for (Entry& existing : aVector) {
    if (existing == aEntry) {
      MOZ_RELEASE_ASSERT(!aAdding);
      aVector.erase(&existing);
      return;
    }
  }
  MOZ_RELEASE_ASSERT(aAdding);
  aVector.append(aEntry);
}

///////////////////////////////////////////////////////////////////////////////
// Profiling
///////////////////////////////////////////////////////////////////////////////

void InitializeCurrentTime();

// Get a current timestamp, in microseconds.
double CurrentTime();

#define ForEachTimerKind(Macro)                 \
  Macro(Default)

enum class TimerKind {
#define DefineTimerKind(aKind) aKind,
  ForEachTimerKind(DefineTimerKind)
#undef DefineTimerKind
  Count
};

struct AutoTimer
{
  explicit AutoTimer(TimerKind aKind);
  ~AutoTimer();

private:
  TimerKind mKind;
  double mStart;
};

void DumpTimers();

// Different kinds of untracked memory used in the system.
namespace UntrackedMemoryKind {
  // Note: 0 is TrackedMemoryKind, 1 is DebuggerAllocatedMemoryKind.
  static const AllocatedMemoryKind Generic = 2;

  // Memory used by untracked files.
  static const AllocatedMemoryKind File = 3;

  // Memory used for thread snapshots.
  static const AllocatedMemoryKind ThreadSnapshot = 4;

  // Memory used by various parts of the memory snapshot system.
  static const AllocatedMemoryKind TrackedRegions = 5;
  static const AllocatedMemoryKind FreeRegions = 6;
  static const AllocatedMemoryKind DirtyPageSet = 7;
  static const AllocatedMemoryKind SortedDirtyPageSet = 8;
  static const AllocatedMemoryKind PageCopy = 9;

  static const size_t Count = 10;
}

///////////////////////////////////////////////////////////////////////////////
// Redirection Bypassing
///////////////////////////////////////////////////////////////////////////////

// The functions below bypass any redirections and give access to the system
// even if events are not passed through in the current thread. These are
// implemented in the various platform ProcessRedirect*.cpp files, and will
// crash on errors which can't be handled internally.

// Generic typedef for a system file handle.
typedef size_t FileHandle;

// Allocate/deallocate a block of memory.
void* DirectAllocateMemory(void* aAddress, size_t aSize);
void DirectDeallocateMemory(void* aAddress, size_t aSize);

// Give a block of memory R or RX access.
void DirectWriteProtectMemory(void* aAddress, size_t aSize, bool aExecutable,
                              bool aIgnoreFailures = false);

// Give a block of memory RW or RWX access.
void DirectUnprotectMemory(void* aAddress, size_t aSize, bool aExecutable,
                           bool aIgnoreFailures = false);

// Open an existing file for reading or a new file for writing, clobbering any
// existing file.
FileHandle DirectOpenFile(const char* aFilename, bool aWriting);

// Seek to an offset within a file open for reading.
void DirectSeekFile(FileHandle aFd, uint64_t aOffset);

// Close or delete a file.
void DirectCloseFile(FileHandle aFd);
void DirectDeleteFile(const char* aFilename);

// Append data to a file open for writing, blocking until the write completes.
void DirectWrite(FileHandle aFd, const void* aData, size_t aSize);

// Print a string directly to stderr.
void DirectPrint(const char* aString);

// Read data from a file, blocking until the read completes.
size_t DirectRead(FileHandle aFd, void* aData, size_t aSize);

// Create a new pipe.
void DirectCreatePipe(FileHandle* aWriteFd, FileHandle* aReadFd);

// Spawn a new thread.
void DirectSpawnThread(void (*aFunction)(void*), void* aArgument);

} // recordreplay
} // mozilla

#endif // mozilla_recordreplay_ProcessRecordReplay_h
