/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Public API for Web Replay. */

#ifndef mozilla_RecordReplay_h
#define mozilla_RecordReplay_h

#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/TemplateLib.h"
#include "mozilla/Types.h"

#include <functional>
#include <stdarg.h>

struct PLDHashTableOps;

namespace mozilla {
namespace recordreplay {

// Record/Replay Overview.
//
// Firefox content processes can be specified to record or replay their
// behavior. Whether a process is recording or replaying is initialized at the
// start of the main() routine, and is afterward invariant for the process.
//
// Recording and replaying works by controlling non-determinism in the browser:
// non-deterministic behaviors are initially recorded, then later replayed
// exactly to force the browser to behave deterministically. Two types of
// non-deterministic behaviors are captured: intra-thread and inter-thread.
// Intra-thread non-deterministic behaviors are non-deterministic even in the
// absence of actions by other threads, and inter-thread non-deterministic
// behaviors are those affected by interleaving execution with other threads.
//
// Intra-thread non-determinism is recorded and replayed as a stream of events
// for each thread. Most events originate from calls to system library
// functions (for i/o and such); the record/replay system handles these
// internally by redirecting these library functions so that code can be
// injected and the event recorded/replayed. Events can also be manually
// performed using the RecordReplayValue and RecordReplayBytes APIs below.
//
// Inter-thread non-determinism is recorded and replayed by keeping track of
// the order in which threads acquire locks or perform atomic accesses. If the
// program is data race free, then reproducing the order of these operations
// will give an interleaving that is functionally (if not exactly) the same
// as during the recording. As for intra-thread non-determinism, system library
// redirections are used to capture most inter-thread non-determinism, but the
// {Begin,End}OrderedAtomicAccess APIs below can be used to add new ordering
// constraints.
//
// Some behaviors can differ between recording and replay. Mainly, pointer
// values can differ, and JS GCs can occur at different points (a more complete
// list is at the URL below). Some of the APIs below are used to accommodate
// these behaviors and keep the replaying process on track.
//
// A third process type, middleman processes, are normal content processes
// which facilitate communication with recording and replaying processes,
// managing the graphics data they generate, and running devtools code that
// interacts with them.
//
// This file contains the main public API for places where mozilla code needs
// to interact with the record/replay system. There are a few additional public
// APIs in toolkit/recordreplay/ipc, for the IPC performed by
// recording/replaying processes and middleman processes.
//
// A more complete description of Web Replay can be found at this URL:
// https://developer.mozilla.org/en-US/docs/WebReplay

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

// Recording and replaying is only enabled on Mac nightlies.
#if defined(XP_MACOSX) && defined(NIGHTLY_BUILD)

extern MFBT_DATA bool gIsRecordingOrReplaying;
extern MFBT_DATA bool gIsRecording;
extern MFBT_DATA bool gIsReplaying;
extern MFBT_DATA bool gIsMiddleman;

// Get the kind of recording/replaying process this is, if any.
static inline bool IsRecordingOrReplaying() { return gIsRecordingOrReplaying; }
static inline bool IsRecording() { return gIsRecording; }
static inline bool IsReplaying() { return gIsReplaying; }
static inline bool IsMiddleman() { return gIsMiddleman; }

#else // XP_MACOSX && NIGHTLY_BUILD

// On unsupported platforms, getting the kind of process is a no-op.
static inline bool IsRecordingOrReplaying() { return false; }
static inline bool IsRecording() { return false; }
static inline bool IsReplaying() { return false; }
static inline bool IsMiddleman() { return false; }

#endif // XP_MACOSX && NIGHTLY_BUILD

// Mark a region which occurs atomically wrt the recording. No two threads can
// be in an atomic region at once, and the order in which atomic sections are
// executed by the various threads will be the same in the replay as in the
// recording. These calls have no effect when not recording/replaying.
static inline void BeginOrderedAtomicAccess();
static inline void EndOrderedAtomicAccess();

// RAII class for an atomic access.
struct MOZ_RAII AutoOrderedAtomicAccess
{
  AutoOrderedAtomicAccess() { BeginOrderedAtomicAccess(); }
  ~AutoOrderedAtomicAccess() { EndOrderedAtomicAccess(); }
};

// Mark a region where thread events are passed through the record/replay
// system. While recording, no information from system calls or other events
// will be recorded for the thread. While replaying, system calls and other
// events are performed normally.
static inline void BeginPassThroughThreadEvents();
static inline void EndPassThroughThreadEvents();

// Whether events in this thread are passed through.
static inline bool AreThreadEventsPassedThrough();

// RAII class for regions where thread events are passed through.
struct MOZ_RAII AutoPassThroughThreadEvents
{
  AutoPassThroughThreadEvents() { BeginPassThroughThreadEvents(); }
  ~AutoPassThroughThreadEvents() { EndPassThroughThreadEvents(); }
};

// As for AutoPassThroughThreadEvents, but may be used when events are already
// passed through.
struct MOZ_RAII AutoEnsurePassThroughThreadEvents
{
  AutoEnsurePassThroughThreadEvents()
    : mPassedThrough(AreThreadEventsPassedThrough())
  {
    if (!mPassedThrough)
      BeginPassThroughThreadEvents();
  }

  ~AutoEnsurePassThroughThreadEvents()
  {
    if (!mPassedThrough)
      EndPassThroughThreadEvents();
  }

private:
  bool mPassedThrough;
};

// Mark a region where thread events are not allowed to occur. The process will
// crash immediately if an event does happen.
static inline void BeginDisallowThreadEvents();
static inline void EndDisallowThreadEvents();

// Whether events in this thread are disallowed.
static inline bool AreThreadEventsDisallowed();

// RAII class for a region where thread events are disallowed.
struct MOZ_RAII AutoDisallowThreadEvents
{
  AutoDisallowThreadEvents() { BeginDisallowThreadEvents(); }
  ~AutoDisallowThreadEvents() { EndDisallowThreadEvents(); }
};

// Mark a region where thread events should have stack information captured.
// These stacks help in tracking down record/replay inconsistencies.
static inline void BeginCaptureEventStacks();
static inline void EndCaptureEventStacks();

// RAII class for a region where thread event stacks should be captured.
struct MOZ_RAII AutoCaptureEventStacks
{
  AutoCaptureEventStacks() { BeginCaptureEventStacks(); }
  ~AutoCaptureEventStacks() { EndCaptureEventStacks(); }
};

// Record or replay a value in the current thread's event stream.
static inline size_t RecordReplayValue(size_t aValue);

// Record or replay the contents of a range of memory in the current thread's
// event stream.
static inline void RecordReplayBytes(void* aData, size_t aSize);

// During recording or replay, mark the recording as unusable. There are some
// behaviors that can't be reliably recorded or replayed. For more information,
// see 'Unrecordable Executions' in the URL above.
static inline void InvalidateRecording(const char* aWhy);

// API for ensuring deterministic recording and replaying of PLDHashTables.
// This allows PLDHashTables to behave deterministically by generating a custom
// set of operations for each table and requiring no other instrumentation.
// (PLHashTables have a similar mechanism, though it is not exposed here.)
static inline const PLDHashTableOps* GeneratePLDHashTableCallbacks(const PLDHashTableOps* aOps);
static inline const PLDHashTableOps* UnwrapPLDHashTableCallbacks(const PLDHashTableOps* aOps);
static inline void DestroyPLDHashTableCallbacks(const PLDHashTableOps* aOps);
static inline void MovePLDHashTableContents(const PLDHashTableOps* aFirstOps,
                                            const PLDHashTableOps* aSecondOps);

// Associate an arbitrary pointer with a JS object root while replaying. This
// is useful for replaying the behavior of weak pointers.
MFBT_API void SetWeakPointerJSRoot(const void* aPtr, /*JSObject*/void* aJSObj);

// API for ensuring that a function executes at a consistent point when
// recording or replaying. This is primarily needed for finalizers and other
// activity during a GC that can perform recorded events (because GCs can
// occur at different times and behave differently between recording and
// replay, thread events are disallowed during a GC). Triggers can be
// registered at a point where thread events are allowed, then activated at
// a point where thread events are not allowed. When recording, the trigger's
// callback will execute at the next point when ExecuteTriggers is called on
// the thread which originally registered the trigger (typically at the top of
// the thread's event loop), and when replaying the callback will execute at
// the same point, even if it was never activated.
//
// Below is an example of how this API can be used.
//
// // This structure's lifetime is managed by the GC.
// struct GarbageCollectedHolder {
//   GarbageCollectedHolder() {
//     RegisterTrigger(this, [=]() { this->DestroyContents(); });
//   }
//   ~GarbageCollectedHolder() {
//     UnregisterTrigger(this);
//   }
//
//   void Finalize() {
//     // During finalization, thread events are disallowed.
//     if (IsRecordingOrReplaying()) {
//       ActivateTrigger(this);
//     } else {
//       DestroyContents();
//     }
//   }
//
//   // This is free to release resources held by the system, communicate with
//   // other threads or processes, and so forth. When replaying, this may
//   // be called before the GC has actually collected this object, but since
//   // the GC will have already collected this object at this point in the
//   // recording, this object will never be accessed again.
//   void DestroyContents();
// };
MFBT_API void RegisterTrigger(void* aObj, const std::function<void()>& aCallback);
MFBT_API void UnregisterTrigger(void* aObj);
MFBT_API void ActivateTrigger(void* aObj);
MFBT_API void ExecuteTriggers();

// Some devtools operations which execute in a replaying process can cause code
// to run which did not run while recording. For example, the JS debugger can
// run arbitrary JS while paused at a breakpoint, by doing an eval(). In such
// cases we say that execution has diverged from the recording, and if recorded
// events are encountered the associated devtools operation fails. This API can
// be used to test for such cases and avoid causing the operation to fail.
static inline bool HasDivergedFromRecording();

// API for handling unrecorded waits. During replay, periodically all threads
// must enter a specific idle state so that checkpoints may be saved or
// restored for rewinding. For threads which block on recorded resources
// --- they wait on a recorded lock (one which was created when events were not
// passed through) or an associated cvar --- this is handled automatically.
//
// Threads which block indefinitely on unrecorded resources must call
// NotifyUnrecordedWait first.
//
// The callback passed to NotifyUnrecordedWait will be invoked at most once
// by the main thread whenever the main thread is waiting for other threads to
// become idle, and at most once after the call to NotifyUnrecordedWait if the
// main thread is already waiting for other threads to become idle.
//
// The callback should poke the thread so that it is no longer blocked on the
// resource. The thread must call MaybeWaitForCheckpointSave before blocking
// again.
MFBT_API void NotifyUnrecordedWait(const std::function<void()>& aCallback);
MFBT_API void MaybeWaitForCheckpointSave();

// API for debugging inconsistent behavior between recording and replay.
// By calling Assert or AssertBytes a thread event will be inserted and any
// inconsistent execution order of events will be detected (as for normal
// thread events) and reported to the console.
//
// RegisterThing/UnregisterThing associate arbitrary pointers with indexes that
// will be consistent between recording/replaying and can be used in assertion
// strings.
static inline void RecordReplayAssert(const char* aFormat, ...);
static inline void RecordReplayAssertBytes(const void* aData, size_t aSize);
static inline void RegisterThing(void* aThing);
static inline void UnregisterThing(void* aThing);
static inline size_t ThingIndex(void* aThing);

// Give a directive to the record/replay system. For possible values for
// aDirective, see ProcessRecordReplay.h. This is used for testing purposes.
static inline void RecordReplayDirective(long aDirective);

// Helper for record/replay asserts, try to determine a name for a C++ object
// with virtual methods based on its vtable.
static inline const char* VirtualThingName(void* aThing);

// Enum which describes whether to preserve behavior between recording and
// replay sessions.
enum class Behavior {
  DontPreserve,
  Preserve
};

// Determine whether this is a recording/replaying or middleman process, and
// initialize record/replay state if so.
MFBT_API void Initialize(int aArgc, char* aArgv[]);

// Kinds of recording/replaying processes that can be spawned.
enum class ProcessKind {
  Recording,
  Replaying,
  MiddlemanRecording,
  MiddlemanReplaying
};

// Command line option for specifying the record/replay kind of a process.
static const char gProcessKindOption[] = "-recordReplayKind";

// Command line option for specifying the recording file to use.
static const char gRecordingFileOption[] = "-recordReplayFile";

///////////////////////////////////////////////////////////////////////////////
// Devtools API
///////////////////////////////////////////////////////////////////////////////

// This interface is used by devtools C++ code (e.g. the JS Debugger) running
// in a child or middleman process.

// The ID of a checkpoint in a child process. Checkpoints are either normal or
// temporary. Normal checkpoints occur at the same point in the recording and
// all replays, while temporary checkpoints are not used while recording and
// may be at different points in different replays.
struct CheckpointId
{
  // ID of the most recent normal checkpoint, which are numbered in sequence
  // starting at FirstCheckpointId.
  size_t mNormal;

  // Special IDs for normal checkpoints.
  static const size_t Invalid = 0;
  static const size_t First = 1;

  // How many temporary checkpoints have been generated since the most recent
  // normal checkpoint, zero if this represents the normal checkpoint itself.
  size_t mTemporary;

  explicit CheckpointId(size_t aNormal = Invalid, size_t aTemporary = 0)
    : mNormal(aNormal), mTemporary(aTemporary)
  {}

  inline bool operator==(const CheckpointId& o) const {
    return mNormal == o.mNormal && mTemporary == o.mTemporary;
  }

  inline bool operator!=(const CheckpointId& o) const {
    return mNormal != o.mNormal || mTemporary != o.mTemporary;
  }
};

// Signature for the hook called when running forward, immediately before
// hitting a normal or temporary checkpoint.
typedef void (*BeforeCheckpointHook)();

// Signature for the hook called immediately after hitting a normal or
// temporary checkpoint, either when running forward or after rewinding.
typedef void (*AfterCheckpointHook)(const CheckpointId& aCheckpoint);

// Set hooks to call when encountering checkpoints.
MFBT_API void SetCheckpointHooks(BeforeCheckpointHook aBeforeCheckpoint,
                                 AfterCheckpointHook aAfterCheckpoint);

// When paused at a breakpoint or at a checkpoint, unpause and proceed with
// execution.
MFBT_API void ResumeExecution();

// When paused at a breakpoint or at a checkpoint, restore a checkpoint that
// was saved earlier and resume execution.
MFBT_API void RestoreCheckpointAndResume(const CheckpointId& aCheckpoint);

// Allow execution after this point to diverge from the recording. Execution
// will remain diverged until an earlier checkpoint is restored.
//
// If an unhandled divergence occurs (see the 'Recording Divergence' comment
// in ProcessRewind.h) then the process rewinds to the most recent saved
// checkpoint.
MFBT_API void DivergeFromRecording();

// After a call to DivergeFromRecording(), this may be called to prevent future
// unhandled divergence from causing earlier checkpoints to be restored
// (the process will immediately crash instead). This state lasts until a new
// call to DivergeFromRecording, or to an explicit restore of an earlier
// checkpoint.
MFBT_API void DisallowUnhandledDivergeFromRecording();

// Note a checkpoint at the current execution position. This checkpoint will be
// saved if either (a) it is temporary, or (b) the middleman has instructed
// this process to save this normal checkpoint. This method returns true if the
// checkpoint was just saved, and false if it was just restored.
MFBT_API bool NewCheckpoint(bool aTemporary);

// Print information about record/replay state. Printing is independent from
// the recording and will be printed by any recording, replaying, or middleman
// process. Spew is only printed when enabled via the RECORD_REPLAY_SPEW
// environment variable.
static inline void Print(const char* aFormat, ...);
static inline void PrintSpew(const char* aFormat, ...);
MFBT_API bool SpewEnabled();

///////////////////////////////////////////////////////////////////////////////
// Allocation policies
///////////////////////////////////////////////////////////////////////////////

// Type describing what kind of memory to allocate/deallocate by APIs below.
// TrackedMemoryKind is reserved for memory that is saved and restored when
// saving or restoring checkpoints. All other values refer to memory that is
// untracked, and whose contents are preserved when restoring checkpoints.
// Different values are used to distinguish different classes of memory for
// diagnosing leaks and reporting memory usage.
typedef size_t AllocatedMemoryKind;
static const AllocatedMemoryKind TrackedMemoryKind = 0;

// Memory kind to use for untracked debugger memory.
static const AllocatedMemoryKind DebuggerAllocatedMemoryKind = 1;

// Allocate or deallocate a block of memory of a particular kind. Allocated
// memory is initially zeroed.
MFBT_API void* AllocateMemory(size_t aSize, AllocatedMemoryKind aKind);
MFBT_API void DeallocateMemory(void* aAddress, size_t aSize, AllocatedMemoryKind aKind);

// Allocation policy for managing memory of a particular kind.
template <AllocatedMemoryKind Kind>
class AllocPolicy
{
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
  T* maybe_pod_malloc(size_t aNumElems) { return maybe_pod_calloc<T>(aNumElems); }

  template <typename T>
  T* pod_malloc(size_t aNumElems) { return maybe_pod_malloc<T>(aNumElems); }

  template <typename T>
  T* pod_calloc(size_t aNumElems) { return maybe_pod_calloc<T>(aNumElems); }

  template <typename T>
  T* pod_realloc(T* aPtr, size_t aOldSize, size_t aNewSize) {
    return maybe_pod_realloc<T>(aPtr, aOldSize, aNewSize);
  }

  void reportAllocOverflow() const {}

  MOZ_MUST_USE bool checkSimulatedOOM() const {
    return true;
  }
};

///////////////////////////////////////////////////////////////////////////////
// API inline function implementation
///////////////////////////////////////////////////////////////////////////////

// Define inline wrappers on builds where recording/replaying is enabled.
#if defined(XP_MACOSX) && defined(NIGHTLY_BUILD)

#define MOZ_MakeRecordReplayWrapperVoid(aName, aFormals, aActuals)      \
  MFBT_API void Internal ##aName aFormals;                              \
  static inline void aName aFormals                                     \
  {                                                                     \
    if (IsRecordingOrReplaying()) {                                     \
      Internal ##aName aActuals;                                        \
    }                                                                   \
  }

#define MOZ_MakeRecordReplayWrapper(aName, aReturnType, aDefaultValue, aFormals, aActuals) \
  MFBT_API aReturnType Internal ##aName aFormals;                       \
  static inline aReturnType aName aFormals                              \
  {                                                                     \
    if (IsRecordingOrReplaying()) {                                     \
      return Internal ##aName aActuals;                                 \
    }                                                                   \
    return aDefaultValue;                                               \
  }

// Define inline wrappers on other builds. Avoiding references to the out of
// line method avoids link errors when e.g. using Atomic<> but not linking
// against MFBT.
#else

#define MOZ_MakeRecordReplayWrapperVoid(aName, aFormals, aActuals)      \
  static inline void aName aFormals {}

#define MOZ_MakeRecordReplayWrapper(aName, aReturnType, aDefaultValue, aFormals, aActuals) \
  static inline aReturnType aName aFormals { return aDefaultValue; }

#endif

MOZ_MakeRecordReplayWrapperVoid(BeginOrderedAtomicAccess, (), ())
MOZ_MakeRecordReplayWrapperVoid(EndOrderedAtomicAccess, (), ())
MOZ_MakeRecordReplayWrapperVoid(BeginPassThroughThreadEvents, (), ())
MOZ_MakeRecordReplayWrapperVoid(EndPassThroughThreadEvents, (), ())
MOZ_MakeRecordReplayWrapper(AreThreadEventsPassedThrough, bool, false, (), ())
MOZ_MakeRecordReplayWrapperVoid(BeginDisallowThreadEvents, (), ())
MOZ_MakeRecordReplayWrapperVoid(EndDisallowThreadEvents, (), ())
MOZ_MakeRecordReplayWrapper(AreThreadEventsDisallowed, bool, false, (), ())
MOZ_MakeRecordReplayWrapperVoid(BeginCaptureEventStacks, (), ())
MOZ_MakeRecordReplayWrapperVoid(EndCaptureEventStacks, (), ())
MOZ_MakeRecordReplayWrapper(RecordReplayValue, size_t, aValue, (size_t aValue), (aValue))
MOZ_MakeRecordReplayWrapperVoid(RecordReplayBytes, (void* aData, size_t aSize), (aData, aSize))
MOZ_MakeRecordReplayWrapper(HasDivergedFromRecording, bool, false, (), ())
MOZ_MakeRecordReplayWrapper(GeneratePLDHashTableCallbacks,
                            const PLDHashTableOps*, aOps, (const PLDHashTableOps* aOps), (aOps))
MOZ_MakeRecordReplayWrapper(UnwrapPLDHashTableCallbacks,
                            const PLDHashTableOps*, aOps, (const PLDHashTableOps* aOps), (aOps))
MOZ_MakeRecordReplayWrapperVoid(DestroyPLDHashTableCallbacks,
                                (const PLDHashTableOps* aOps), (aOps))
MOZ_MakeRecordReplayWrapperVoid(MovePLDHashTableContents,
                                (const PLDHashTableOps* aFirstOps,
                                 const PLDHashTableOps* aSecondOps),
                                (aFirstOps, aSecondOps))
MOZ_MakeRecordReplayWrapperVoid(InvalidateRecording, (const char* aWhy), (aWhy))
MOZ_MakeRecordReplayWrapperVoid(RegisterWeakPointer,
                                (const void* aPtr, const std::function<void(bool)>& aCallback),
                                (aPtr, aCallback))
MOZ_MakeRecordReplayWrapperVoid(UnregisterWeakPointer, (const void* aPtr), (aPtr))
MOZ_MakeRecordReplayWrapperVoid(WeakPointerAccess,
                                (const void* aPtr, bool aSuccess), (aPtr, aSuccess))
MOZ_MakeRecordReplayWrapperVoid(RecordReplayAssertBytes,
                                (const void* aData, size_t aSize), (aData, aSize))
MOZ_MakeRecordReplayWrapperVoid(RegisterThing, (void* aThing), (aThing))
MOZ_MakeRecordReplayWrapperVoid(UnregisterThing, (void* aThing), (aThing))
MOZ_MakeRecordReplayWrapper(ThingIndex, size_t, 0, (void* aThing), (aThing))
MOZ_MakeRecordReplayWrapper(VirtualThingName, const char*, nullptr, (void* aThing), (aThing))
MOZ_MakeRecordReplayWrapperVoid(RecordReplayDirective, (long aDirective), (aDirective))

#undef MOZ_MakeRecordReplayWrapperVoid
#undef MOZ_MakeRecordReplayWrapper

MFBT_API void InternalRecordReplayAssert(const char* aFormat, va_list aArgs);

static inline void
RecordReplayAssert(const char* aFormat, ...)
{
  if (IsRecordingOrReplaying()) {
    va_list ap;
    va_start(ap, aFormat);
    InternalRecordReplayAssert(aFormat, ap);
    va_end(ap);
  }
}

MFBT_API void InternalPrint(const char* aFormat, va_list aArgs);

#define MOZ_MakeRecordReplayPrinter(aName, aSpewing)            \
  static inline void                                            \
  aName(const char* aFormat, ...)                               \
  {                                                             \
    if ((IsRecordingOrReplaying() || IsMiddleman()) && (!aSpewing || SpewEnabled())) { \
      va_list ap;                                               \
      va_start(ap, aFormat);                                    \
      InternalPrint(aFormat, ap);                               \
      va_end(ap);                                               \
    }                                                           \
  }

MOZ_MakeRecordReplayPrinter(Print, false)
MOZ_MakeRecordReplayPrinter(PrintSpew, true)

#undef MOZ_MakeRecordReplayPrinter

} // recordreplay
} // mozilla

#endif /* mozilla_RecordReplay_h */
