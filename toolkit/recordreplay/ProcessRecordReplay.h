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

// Instantiate _Macro for each of the platform independent thread events.
#define ForEachThreadEvent(_Macro)                                        \
  /* Spawned another thread. */                                           \
  _Macro(CreateThread)                                                    \
                                                                          \
      /* Created a recorded lock. */                                      \
      _Macro(CreateLock)                                                  \
                                                                          \
      /* Acquired a recorded lock. */                                     \
      _Macro(Lock)                                                        \
                                                                          \
      /* Called RecordReplayValue. */                                     \
      _Macro(Value)                                                       \
                                                                          \
      /* Called RecordReplayBytes. */                                     \
      _Macro(Bytes)                                                       \
                                                                          \
      /* Called RecordReplayAssert or RecordReplayAssertBytes. */         \
      _Macro(Assert) _Macro(AssertBytes)                                  \
                                                                          \
      /* Performed an atomic access. */                                   \
      _Macro(AtomicAccess)                                                \
                                                                          \
      /* Executed a nested callback (see Callback.h). */                  \
      _Macro(ExecuteCallback)                                             \
                                                                          \
      /* Finished executing nested callbacks in a library API (see        \
         Callback.h). */                                                  \
      _Macro(CallbacksFinished)                                           \
                                                                          \
      /* Restoring a data pointer used in a callback (see Callback.h). */ \
      _Macro(RestoreCallbackData)                                         \
                                                                          \
      /* Called RegisterTrigger. */                                       \
      _Macro(RegisterTrigger)                                             \
                                                                          \
      /* Executed a trigger within a call to ExecuteTriggers. */          \
      _Macro(ExecuteTrigger)                                              \
                                                                          \
      /* Finished executing triggers within a call to ExecuteTriggers. */ \
      _Macro(ExecuteTriggersFinished)

// ID of an event in a thread's event stream. Each ID in the stream is followed
// by data associated with the event.
enum class ThreadEvent : uint32_t {
#define DefineEnum(Kind) Kind,
  ForEachThreadEvent(DefineEnum)
#undef DefineEnum

  // The start of event IDs for redirected call events. Event IDs after this
  // point are platform specific.
  CallStart
};

// Get the printable name for a thread event.
const char* ThreadEventName(ThreadEvent aEvent);

class File;

// File used during recording and replay.
extern File* gRecordingFile;

// Whether record/replay state has finished initialization.
extern bool gInitialized;

// If we failed to initialize, any associated message. On an initialization
// failure, events will be passed through until we have connected with the
// middleman, reported the failure, and crashed.
extern char* gInitializationFailureMessage;

// For places where events will normally not be passed through, unless there
// was an initialization failure.
static inline void AssertEventsAreNotPassedThrough() {
  MOZ_RELEASE_ASSERT(!AreThreadEventsPassedThrough() ||
                     gInitializationFailureMessage);
}

// Flush any new recording data to disk.
void FlushRecording();

// Called when any thread hits the end of its event stream.
void HitEndOfRecording();

// Called in a replaying process to load the last checkpoint in the recording.
size_t RecordingEndpoint();

// Access the flag for whether this is the main child. The main child never
// rewinds and sends graphics updates to the middleman while running forward.
bool IsMainChild();
void SetMainChild();

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

static inline bool MemoryContains(void* aBase, size_t aSize, void* aPtr,
                                  size_t aPtrSize = 1) {
  MOZ_ASSERT(aPtrSize);
  return (uint8_t*)aPtr >= (uint8_t*)aBase &&
         (uint8_t*)aPtr + aPtrSize <= (uint8_t*)aBase + aSize;
}

static inline bool MemoryIntersects(void* aBase0, size_t aSize0, void* aBase1,
                                    size_t aSize1) {
  MOZ_ASSERT(aSize0 && aSize1);
  return MemoryContains(aBase0, aSize0, aBase1) ||
         MemoryContains(aBase0, aSize0, (uint8_t*)aBase1 + aSize1 - 1) ||
         MemoryContains(aBase1, aSize1, aBase0);
}

static const size_t PageSize = 4096;

static inline uint8_t* PageBase(void* aAddress) {
  return (uint8_t*)aAddress - ((size_t)aAddress % PageSize);
}

static inline size_t RoundupSizeToPageBoundary(size_t aSize) {
  if (aSize % PageSize) {
    return aSize + PageSize - (aSize % PageSize);
  }
  return aSize;
}

static inline bool TestEnv(const char* env) {
  const char* value = getenv(env);
  return value && value[0];
}

// Check for membership in a vector.
template <typename Vector, typename Entry>
inline bool VectorContains(const Vector& aVector, const Entry& aEntry) {
  return std::find(aVector.begin(), aVector.end(), aEntry) != aVector.end();
}

// Add or remove a unique entry to an unsorted vector.
template <typename Vector, typename Entry>
inline void VectorAddOrRemoveEntry(Vector& aVector, const Entry& aEntry,
                                   bool aAdding) {
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

bool SpewEnabled();
void InternalPrint(const char* aFormat, va_list aArgs);

#define MOZ_MakeRecordReplayPrinter(aName, aSpewing)   \
  static inline void aName(const char* aFormat, ...) { \
    if ((IsRecordingOrReplaying() || IsMiddleman()) && \
        (!aSpewing || SpewEnabled())) {                \
      va_list ap;                                      \
      va_start(ap, aFormat);                           \
      InternalPrint(aFormat, ap);                      \
      va_end(ap);                                      \
    }                                                  \
  }

// Print information about record/replay state. Printing is independent from
// the recording and will be printed by any recording, replaying, or middleman
// process. Spew is only printed when enabled via the RECORD_REPLAY_SPEW
// environment variable.
MOZ_MakeRecordReplayPrinter(Print, false)
    MOZ_MakeRecordReplayPrinter(PrintSpew, true)

#undef MOZ_MakeRecordReplayPrinter

    // Get the ID of the process that produced the recording.
    int GetRecordingPid();

///////////////////////////////////////////////////////////////////////////////
// Profiling
///////////////////////////////////////////////////////////////////////////////

void InitializeCurrentTime();

// Get a current timestamp, in microseconds.
double CurrentTime();

#define ForEachTimerKind(Macro) Macro(Default)

enum class TimerKind {
#define DefineTimerKind(aKind) aKind,
  ForEachTimerKind(DefineTimerKind)
#undef DefineTimerKind
      Count
};

struct AutoTimer {
  explicit AutoTimer(TimerKind aKind);
  ~AutoTimer();

 private:
  TimerKind mKind;
  double mStart;
};

void DumpTimers();

///////////////////////////////////////////////////////////////////////////////
// Memory Management
///////////////////////////////////////////////////////////////////////////////

// In cases where memory is tracked and should be saved/restored with
// checkoints, malloc and other standard library functions suffice to allocate
// memory in the record/replay system. The routines below are used for handling
// redirections for the raw system calls underlying the standard libraries, and
// for cases where allocated memory should be untracked: the contents are
// ignored when saving/restoring checkpoints.

// Different kinds of memory used in the system.
enum class MemoryKind {
  // Memory whose contents are saved/restored with checkpoints.
  Tracked,

  // All remaining memory kinds refer to untracked memory.

  // Memory not fitting into one of the categories below.
  Generic,

  // Memory used for thread snapshots.
  ThreadSnapshot,

  // Memory used by various parts of the memory snapshot system.
  TrackedRegions,
  FreeRegions,
  DirtyPageSet,
  SortedDirtyPageSet,
  PageCopy,

  // Memory used by various parts of JS integration.
  ScriptHits,

  Count
};

// Allocate or deallocate a block of memory of a particular kind. Allocated
// memory is initially zeroed.
void* AllocateMemory(size_t aSize, MemoryKind aKind);
void DeallocateMemory(void* aAddress, size_t aSize, MemoryKind aKind);

// Allocation policy for managing memory of a particular kind.
template <MemoryKind Kind>
class AllocPolicy {
 public:
  template <typename T>
  T* maybe_pod_calloc(size_t aNumElems) {
    if (aNumElems & tl::MulOverflowMask<sizeof(T)>::value) {
      MOZ_CRASH();
    }
    // Note: AllocateMemory always returns zeroed memory.
    return static_cast<T*>(AllocateMemory(aNumElems * sizeof(T), Kind));
  }

  template <typename T>
  void free_(T* aPtr, size_t aSize) {
    DeallocateMemory(aPtr, aSize * sizeof(T), Kind);
  }

  template <typename T>
  T* maybe_pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    T* res = maybe_pod_calloc<T>(aNewSize);
    memcpy(res, aPtr, aOldSize * sizeof(T));
    free_<T>(aPtr, aOldSize);
    return res;
  }

  template <typename T>
  T* maybe_pod_malloc(size_t aNumElems) {
    return maybe_pod_calloc<T>(aNumElems);
  }

  template <typename T>
  T* pod_malloc(size_t aNumElems) {
    return maybe_pod_malloc<T>(aNumElems);
  }

  template <typename T>
  T* pod_calloc(size_t aNumElems) {
    return maybe_pod_calloc<T>(aNumElems);
  }

  template <typename T>
  T* pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    return maybe_pod_realloc<T>(aPtr, aOldSize, aNewSize);
  }

  void reportAllocOverflow() const {}

  MOZ_MUST_USE bool checkSimulatedOOM() const { return true; }
};

///////////////////////////////////////////////////////////////////////////////
// Redirection Bypassing
///////////////////////////////////////////////////////////////////////////////

// The functions below bypass any redirections and give access to the system
// even if events are not passed through in the current thread. These are
// implemented in the various platform ProcessRedirect*.cpp files, and will
// crash on errors which can't be handled internally.

// Generic typedef for a system file handle.
typedef size_t FileHandle;

// Allocate/deallocate a block of memory directly from the system.
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

}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_ProcessRecordReplay_h
