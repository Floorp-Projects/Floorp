/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RecordReplay.h"

#include "js/GCAnnotations.h"
#include "mozilla/Atomics.h"
#include "mozilla/Casting.h"
#include "mozilla/Utf8.h"

#include <stdlib.h>

// Recording and replaying is only enabled on Mac nightlies.
#if defined(XP_MACOSX) && defined(NIGHTLY_BUILD)
#  define ENABLE_RECORD_REPLAY
#endif

#ifdef ENABLE_RECORD_REPLAY
#  include <dlfcn.h>
#endif

namespace mozilla {
namespace recordreplay {

#define FOR_EACH_INTERFACE(Macro)                                              \
  Macro(InternalAreThreadEventsPassedThrough, bool, (), ()) Macro(             \
      InternalAreThreadEventsDisallowed, bool, (),                             \
      ()) Macro(InternalRecordReplayValue, size_t, (size_t aValue), (aValue))  \
      Macro(InternalHasDivergedFromRecording, bool, (), ()) Macro(             \
          InternalGeneratePLDHashTableCallbacks, const PLDHashTableOps*,       \
          (const PLDHashTableOps* aOps),                                       \
          (aOps)) Macro(InternalUnwrapPLDHashTableCallbacks,                   \
                        const PLDHashTableOps*, (const PLDHashTableOps* aOps), \
                        (aOps))                                                \
          Macro(InternalThingIndex, size_t, (void* aThing), (aThing)) Macro(   \
              InternalVirtualThingName, const char*, (void* aThing), (aThing)) \
              Macro(ExecutionProgressCounter, ProgressCounter*, (), ())        \
                  Macro(NewTimeWarpTarget, ProgressCounter, (), ()) Macro(     \
                      ShouldUpdateProgressCounter, bool, (const char* aURL),   \
                      (aURL))                                                  \
                      Macro(DefineRecordReplayControlObject, bool,             \
                            (JSContext * aCx, JSObject * aObj), (aCx, aObj))

#define FOR_EACH_INTERFACE_VOID(Macro)                                         \
  Macro(                                                                       \
      InternalBeginOrderedAtomicAccess, (const void* aValue),                  \
      (aValue)) Macro(InternalEndOrderedAtomicAccess, (),                      \
                      ()) Macro(InternalBeginPassThroughThreadEvents, (),      \
                                ()) Macro(InternalEndPassThroughThreadEvents,  \
                                          (), ())                              \
      Macro(InternalBeginDisallowThreadEvents, (), ()) Macro(                  \
          InternalEndDisallowThreadEvents, (),                                 \
          ()) Macro(InternalRecordReplayBytes, (void* aData, size_t aSize),    \
                    (aData, aSize)) Macro(InternalInvalidateRecording,         \
                                          (const char* aWhy), (aWhy))          \
          Macro(InternalDestroyPLDHashTableCallbacks,                          \
                (const PLDHashTableOps* aOps),                                 \
                (aOps)) Macro(InternalMovePLDHashTableContents,                \
                              (const PLDHashTableOps* aFirstOps,               \
                               const PLDHashTableOps* aSecondOps),             \
                              (aFirstOps,                                      \
                               aSecondOps)) Macro(SetWeakPointerJSRoot,        \
                                                  (const void* aPtr,           \
                                                   JSObject* aJSObj),          \
                                                  (aPtr, aJSObj))              \
              Macro(RegisterTrigger,                                           \
                    (void* aObj, const std::function<void()>& aCallback),      \
                    (aObj, aCallback)) Macro(UnregisterTrigger, (void* aObj),  \
                                             (aObj))                           \
                  Macro(ActivateTrigger, (void* aObj), (aObj)) Macro(          \
                      ExecuteTriggers, (),                                     \
                      ()) Macro(InternalRecordReplayAssert,                    \
                                (const char* aFormat, va_list aArgs),          \
                                (aFormat, aArgs))                              \
                      Macro(InternalRecordReplayAssertBytes,                   \
                            (const void* aData, size_t aSize),                 \
                            (aData, aSize)) Macro(InternalRegisterThing,       \
                                                  (void* aThing), (aThing))    \
                          Macro(InternalUnregisterThing, (void* aThing),       \
                                (aThing))                                      \
                              Macro(BeginContentParse,                         \
                                    (const void* aToken, const char* aURL,     \
                                     const char* aContentType),                \
                                    (aToken, aURL, aContentType))              \
                                  Macro(AddContentParseData8,                  \
                                        (const void* aToken,                   \
                                         const mozilla::Utf8Unit* aUtf8Buffer, \
                                         size_t aLength),                      \
                                        (aToken, aUtf8Buffer, aLength))        \
                                      Macro(AddContentParseData16,             \
                                            (const void* aToken,               \
                                             const char16_t* aBuffer,          \
                                             size_t aLength),                  \
                                            (aToken, aBuffer, aLength))        \
                                          Macro(EndContentParse,               \
                                                (const void* aToken),          \
                                                (aToken))

#define DECLARE_SYMBOL(aName, aReturnType, aFormals, _) \
  static aReturnType(*gPtr##aName) aFormals;
#define DECLARE_SYMBOL_VOID(aName, aFormals, _) \
  DECLARE_SYMBOL(aName, void, aFormals, _)

FOR_EACH_INTERFACE(DECLARE_SYMBOL)
FOR_EACH_INTERFACE_VOID(DECLARE_SYMBOL_VOID)

#undef DECLARE_SYMBOL
#undef DECLARE_SYMBOL_VOID

static void* LoadSymbol(const char* aName) {
#ifdef ENABLE_RECORD_REPLAY
  void* rv = dlsym(RTLD_DEFAULT, aName);
  MOZ_RELEASE_ASSERT(rv);
  return rv;
#else
  return nullptr;
#endif
}

void Initialize(int aArgc, char* aArgv[]) {
  // Only initialize if the right command line option was specified.
  bool found = false;
  for (int i = 0; i < aArgc; i++) {
    if (!strcmp(aArgv[i], gProcessKindOption)) {
      found = true;
      break;
    }
  }
  if (!found) {
    return;
  }

  void (*initialize)(int, char**);
  BitwiseCast(LoadSymbol("RecordReplayInterface_Initialize"), &initialize);
  if (!initialize) {
    return;
  }

#define INIT_SYMBOL(aName, _1, _2, _3) \
  BitwiseCast(LoadSymbol("RecordReplayInterface_" #aName), &gPtr##aName);
#define INIT_SYMBOL_VOID(aName, _2, _3) INIT_SYMBOL(aName, void, _2, _3)

  FOR_EACH_INTERFACE(INIT_SYMBOL)
  FOR_EACH_INTERFACE_VOID(INIT_SYMBOL_VOID)

#undef INIT_SYMBOL
#undef INIT_SYMBOL_VOID

  initialize(aArgc, aArgv);
}

// Record/replay API functions can't GC, but we can't use
// JS::AutoSuppressGCAnalysis here due to linking issues.
struct AutoSuppressGCAnalysis {
  AutoSuppressGCAnalysis() {}
  ~AutoSuppressGCAnalysis() {
#ifdef DEBUG
    // Need nontrivial destructor.
    static Atomic<int, SequentiallyConsistent, Behavior::DontPreserve> dummy;
    dummy++;
#endif
  }
} JS_HAZ_GC_SUPPRESSED;

#define DEFINE_WRAPPER(aName, aReturnType, aFormals, aActuals) \
  aReturnType aName aFormals {                                 \
    AutoSuppressGCAnalysis suppress;                           \
    MOZ_ASSERT(IsRecordingOrReplaying() || IsMiddleman());     \
    return gPtr##aName aActuals;                               \
  }

#define DEFINE_WRAPPER_VOID(aName, aFormals, aActuals)     \
  void aName aFormals {                                    \
    AutoSuppressGCAnalysis suppress;                       \
    MOZ_ASSERT(IsRecordingOrReplaying() || IsMiddleman()); \
    gPtr##aName aActuals;                                  \
  }

FOR_EACH_INTERFACE(DEFINE_WRAPPER)
FOR_EACH_INTERFACE_VOID(DEFINE_WRAPPER_VOID)

#undef DEFINE_WRAPPER
#undef DEFINE_WRAPPER_VOID

#ifdef ENABLE_RECORD_REPLAY

bool gIsRecordingOrReplaying;
bool gIsRecording;
bool gIsReplaying;
bool gIsMiddleman;

#endif  // ENABLE_RECORD_REPLAY

#undef ENABLE_RECORD_REPLAY

}  // namespace recordreplay
}  // namespace mozilla
