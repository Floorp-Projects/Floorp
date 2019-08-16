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
#include "mozilla/Utf8.h"

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

#else  // XP_MACOSX && NIGHTLY_BUILD

// On unsupported platforms, getting the kind of process is a no-op.
static inline bool IsRecordingOrReplaying() { return false; }
static inline bool IsRecording() { return false; }
static inline bool IsReplaying() { return false; }
static inline bool IsMiddleman() { return false; }

#endif  // XP_MACOSX && NIGHTLY_BUILD

// Mark a region which occurs atomically wrt the recording. No two threads can
// be in an atomic region at once, and the order in which atomic sections are
// executed by the various threads for the same aValue will be the same in the
// replay as in the recording. These calls have no effect when not recording or
// replaying.
static inline void BeginOrderedAtomicAccess(const void* aValue);
static inline void EndOrderedAtomicAccess();

// RAII class for an atomic access.
struct MOZ_RAII AutoOrderedAtomicAccess {
  explicit AutoOrderedAtomicAccess(const void* aValue) {
    BeginOrderedAtomicAccess(aValue);
  }
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
struct MOZ_RAII AutoPassThroughThreadEvents {
  AutoPassThroughThreadEvents() { BeginPassThroughThreadEvents(); }
  ~AutoPassThroughThreadEvents() { EndPassThroughThreadEvents(); }
};

// As for AutoPassThroughThreadEvents, but may be used when events are already
// passed through.
struct MOZ_RAII AutoEnsurePassThroughThreadEvents {
  AutoEnsurePassThroughThreadEvents()
      : mPassedThrough(AreThreadEventsPassedThrough()) {
    if (!mPassedThrough) BeginPassThroughThreadEvents();
  }

  ~AutoEnsurePassThroughThreadEvents() {
    if (!mPassedThrough) EndPassThroughThreadEvents();
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
struct MOZ_RAII AutoDisallowThreadEvents {
  AutoDisallowThreadEvents() { BeginDisallowThreadEvents(); }
  ~AutoDisallowThreadEvents() { EndDisallowThreadEvents(); }
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
static inline const PLDHashTableOps* GeneratePLDHashTableCallbacks(
    const PLDHashTableOps* aOps);
static inline const PLDHashTableOps* UnwrapPLDHashTableCallbacks(
    const PLDHashTableOps* aOps);
static inline void DestroyPLDHashTableCallbacks(const PLDHashTableOps* aOps);
static inline void MovePLDHashTableContents(const PLDHashTableOps* aFirstOps,
                                            const PLDHashTableOps* aSecondOps);

// Prevent a JS object from ever being collected while recording or replaying.
// GC behavior is non-deterministic when recording/replaying, and preventing
// an object from being collected ensures that finalizers which might interact
// with the recording will not execute.
static inline void HoldJSObject(JSObject* aJSObj);

// Some devtools operations which execute in a replaying process can cause code
// to run which did not run while recording. For example, the JS debugger can
// run arbitrary JS while paused at a breakpoint, by doing an eval(). In such
// cases we say that execution has diverged from the recording, and if recorded
// events are encountered the associated devtools operation fails. This API can
// be used to test for such cases and avoid causing the operation to fail.
static inline bool HasDivergedFromRecording();

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

// Helper for record/replay asserts, try to determine a name for a C++ object
// with virtual methods based on its vtable.
static inline const char* VirtualThingName(void* aThing);

// Enum which describes whether to preserve behavior between recording and
// replay sessions.
enum class Behavior { DontPreserve, Preserve };

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

static inline void AdvanceExecutionProgressCounter() {
  ++*ExecutionProgressCounter();
}

// Get an identifier for the current execution point which can be used to warp
// here later.
MFBT_API ProgressCounter NewTimeWarpTarget();

// Return whether a script should update the progress counter when it runs.
MFBT_API bool ShouldUpdateProgressCounter(const char* aURL);

// Define a RecordReplayControl object on the specified global object, with
// methods specialized to the current recording/replaying or middleman process
// kind.
MFBT_API bool DefineRecordReplayControlObject(JSContext* aCx, JSObject* aObj);

// Notify the infrastructure that some URL which contains JavaScript or CSS is
// being parsed. This is used to provide the complete contents of the URL to
// devtools code when it is inspecting the state of this process; that devtools
// code can't simply fetch the URL itself since it may have been changed since
// the recording was made or may no longer exist. The token for a parse may not
// be used in other parses until after EndContentParse() is called.
MFBT_API void BeginContentParse(const void* aToken, const char* aURL,
                                const char* aContentType);

// Add some UTF-8 parse data to an existing content parse.
MFBT_API void AddContentParseData8(const void* aToken,
                                   const Utf8Unit* aUtf8Buffer, size_t aLength);

// Add some UTF-16 parse data to an existing content parse.
MFBT_API void AddContentParseData16(const void* aToken, const char16_t* aBuffer,
                                    size_t aLength);

// Mark a content parse as having completed.
MFBT_API void EndContentParse(const void* aToken);

// Perform an entire content parse of UTF-8 data.
static inline void NoteContentParse(const void* aToken, const char* aURL,
                                    const char* aContentType,
                                    const Utf8Unit* aUtf8Buffer,
                                    size_t aLength) {
  BeginContentParse(aToken, aURL, aContentType);
  AddContentParseData8(aToken, aUtf8Buffer, aLength);
  EndContentParse(aToken);
}

// Perform an entire content parse of UTF-16 data.
static inline void NoteContentParse(const void* aToken, const char* aURL,
                                    const char* aContentType,
                                    const char16_t* aBuffer, size_t aLength) {
  BeginContentParse(aToken, aURL, aContentType);
  AddContentParseData16(aToken, aBuffer, aLength);
  EndContentParse(aToken);
}

///////////////////////////////////////////////////////////////////////////////
// API inline function implementation
///////////////////////////////////////////////////////////////////////////////

// Define inline wrappers on builds where recording/replaying is enabled.
#if defined(XP_MACOSX) && defined(NIGHTLY_BUILD)

#  define MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(aName, aFormals, aActuals) \
    MFBT_API void Internal##aName aFormals;                              \
    static inline void aName aFormals {                                  \
      if (IsRecordingOrReplaying()) {                                    \
        Internal##aName aActuals;                                        \
      }                                                                  \
    }

#  define MOZ_MAKE_RECORD_REPLAY_WRAPPER(aName, aReturnType, aDefaultValue, \
                                         aFormals, aActuals)                \
    MFBT_API aReturnType Internal##aName aFormals;                          \
    static inline aReturnType aName aFormals {                              \
      if (IsRecordingOrReplaying()) {                                       \
        return Internal##aName aActuals;                                    \
      }                                                                     \
      return aDefaultValue;                                                 \
    }

// Define inline wrappers on other builds. Avoiding references to the out of
// line method avoids link errors when e.g. using Atomic<> but not linking
// against MFBT.
#else

#  define MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(aName, aFormals, aActuals) \
    static inline void aName aFormals {}

#  define MOZ_MAKE_RECORD_REPLAY_WRAPPER(aName, aReturnType, aDefaultValue, \
                                         aFormals, aActuals)                \
    static inline aReturnType aName aFormals { return aDefaultValue; }

#endif

MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(BeginOrderedAtomicAccess,
                                    (const void* aValue), (aValue))
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(EndOrderedAtomicAccess, (), ())
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(BeginPassThroughThreadEvents, (), ())
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(EndPassThroughThreadEvents, (), ())
MOZ_MAKE_RECORD_REPLAY_WRAPPER(AreThreadEventsPassedThrough, bool, false, (),
                               ())
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(BeginDisallowThreadEvents, (), ())
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(EndDisallowThreadEvents, (), ())
MOZ_MAKE_RECORD_REPLAY_WRAPPER(AreThreadEventsDisallowed, bool, false, (), ())
MOZ_MAKE_RECORD_REPLAY_WRAPPER(RecordReplayValue, size_t, aValue,
                               (size_t aValue), (aValue))
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(RecordReplayBytes,
                                    (void* aData, size_t aSize), (aData, aSize))
MOZ_MAKE_RECORD_REPLAY_WRAPPER(HasDivergedFromRecording, bool, false, (), ())
MOZ_MAKE_RECORD_REPLAY_WRAPPER(GeneratePLDHashTableCallbacks,
                               const PLDHashTableOps*, aOps,
                               (const PLDHashTableOps* aOps), (aOps))
MOZ_MAKE_RECORD_REPLAY_WRAPPER(UnwrapPLDHashTableCallbacks,
                               const PLDHashTableOps*, aOps,
                               (const PLDHashTableOps* aOps), (aOps))
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(DestroyPLDHashTableCallbacks,
                                    (const PLDHashTableOps* aOps), (aOps))
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(MovePLDHashTableContents,
                                    (const PLDHashTableOps* aFirstOps,
                                     const PLDHashTableOps* aSecondOps),
                                    (aFirstOps, aSecondOps))
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(InvalidateRecording, (const char* aWhy),
                                    (aWhy))
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(HoldJSObject, (JSObject* aObject),
                                    (aObject))
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(RecordReplayAssertBytes,
                                    (const void* aData, size_t aSize),
                                    (aData, aSize))
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(RegisterThing, (void* aThing), (aThing))
MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID(UnregisterThing, (void* aThing), (aThing))
MOZ_MAKE_RECORD_REPLAY_WRAPPER(ThingIndex, size_t, 0, (void* aThing), (aThing))
MOZ_MAKE_RECORD_REPLAY_WRAPPER(VirtualThingName, const char*, nullptr,
                               (void* aThing), (aThing))

#undef MOZ_MAKE_RECORD_REPLAY_WRAPPER_VOID
#undef MOZ_MAKERECORDREPLAYWRAPPER

MFBT_API void InternalRecordReplayAssert(const char* aFormat, va_list aArgs);

static inline void RecordReplayAssert(const char* aFormat, ...) {
  if (IsRecordingOrReplaying()) {
    va_list ap;
    va_start(ap, aFormat);
    InternalRecordReplayAssert(aFormat, ap);
    va_end(ap);
  }
}

}  // namespace recordreplay
}  // namespace mozilla

#endif /* mozilla_RecordReplay_h */
