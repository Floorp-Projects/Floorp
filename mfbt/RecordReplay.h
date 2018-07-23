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
struct JSContext;
class JSObject;

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
MFBT_API void SetWeakPointerJSRoot(const void* aPtr, JSObject* aJSObj);

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
// JS interface
///////////////////////////////////////////////////////////////////////////////

// Get the counter used to keep track of how much progress JS execution has
// made while running on the main thread. Progress must advance whenever a JS
// function is entered or loop entry point is reached, so that no script
// location may be hit twice while the progress counter is the same. See
// JSControl.h for more.
typedef uint64_t ProgressCounter;
MFBT_API ProgressCounter* ExecutionProgressCounter();

static inline void
AdvanceExecutionProgressCounter()
{
  ++*ExecutionProgressCounter();
}

// Return whether a script is internal to the record/replay infrastructure,
// may run non-deterministically between recording and replaying, and whose
// execution must not update the progress counter.
MFBT_API bool IsInternalScript(const char* aURL);

// Define a RecordReplayControl object on the specified global object, with
// methods specialized to the current recording/replaying or middleman process
// kind.
MFBT_API bool DefineRecordReplayControlObject(JSContext* aCx, JSObject* aObj);

// Notify the infrastructure that some URL which contains JavaScript is
// being parsed. This is used to provide the complete contents of the URL to
// devtools code when it is inspecting the state of this process; that devtools
// code can't simply fetch the URL itself since it may have been changed since
// the recording was made or may no longer exist. The token for a parse may not
// be used in other parses until after EndContentParse() is called.
MFBT_API void BeginContentParse(const void* aToken,
                                const char* aURL, const char* aContentType);

// Add some parse data to an existing content parse.
MFBT_API void AddContentParseData(const void* aToken,
                                  const char16_t* aBuffer, size_t aLength);

// Mark a content parse as having completed.
MFBT_API void EndContentParse(const void* aToken);

// Perform an entire content parse, when the entire URL is available at once.
static inline void
NoteContentParse(const void* aToken,
                 const char* aURL, const char* aContentType,
                 const char16_t* aBuffer, size_t aLength)
{
  BeginContentParse(aToken, aURL, aContentType);
  AddContentParseData(aToken, aBuffer, aLength);
  EndContentParse(aToken);
}

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

} // recordreplay
} // mozilla

#endif /* mozilla_RecordReplay_h */
