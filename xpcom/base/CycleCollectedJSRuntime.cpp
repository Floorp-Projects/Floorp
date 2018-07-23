/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We're dividing JS objects into 3 categories:
//
// 1. "real" roots, held by the JS engine itself or rooted through the root
//    and lock JS APIs. Roots from this category are considered black in the
//    cycle collector, any cycle they participate in is uncollectable.
//
// 2. certain roots held by C++ objects that are guaranteed to be alive.
//    Roots from this category are considered black in the cycle collector,
//    and any cycle they participate in is uncollectable. These roots are
//    traced from TraceNativeBlackRoots.
//
// 3. all other roots held by C++ objects that participate in cycle
//    collection, held by us (see TraceNativeGrayRoots). Roots from this
//    category are considered grey in the cycle collector; whether or not
//    they are collected depends on the objects that hold them.
//
// Note that if a root is in multiple categories the fact that it is in
// category 1 or 2 that takes precedence, so it will be considered black.
//
// During garbage collection we switch to an additional mark color (gray)
// when tracing inside TraceNativeGrayRoots. This allows us to walk those
// roots later on and add all objects reachable only from them to the
// cycle collector.
//
// Phases:
//
// 1. marking of the roots in category 1 by having the JS GC do its marking
// 2. marking of the roots in category 2 by having the JS GC call us back
//    (via JS_SetExtraGCRootsTracer) and running TraceNativeBlackRoots
// 3. marking of the roots in category 3 by TraceNativeGrayRoots using an
//    additional color (gray).
// 4. end of GC, GC can sweep its heap
//
// At some later point, when the cycle collector runs:
//
// 5. walk gray objects and add them to the cycle collector, cycle collect
//
// JS objects that are part of cycles the cycle collector breaks will be
// collected by the next JS GC.
//
// If WantAllTraces() is false the cycle collector will not traverse roots
// from category 1 or any JS objects held by them. Any JS objects they hold
// will already be marked by the JS GC and will thus be colored black
// themselves. Any C++ objects they hold will have a missing (untraversed)
// edge from the JS object to the C++ object and so it will be marked black
// too. This decreases the number of objects that the cycle collector has to
// deal with.
// To improve debugging, if WantAllTraces() is true all JS objects are
// traversed.

#include "mozilla/CycleCollectedJSRuntime.h"
#include <algorithm>
#include "mozilla/ArrayUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/Move.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimelineConsumers.h"
#include "mozilla/TimelineMarker.h"
#include "mozilla/Unused.h"
#include "mozilla/DebuggerOnGCRunnable.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/ScriptSettings.h"
#include "js/Debug.h"
#include "js/GCAPI.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"
#include "nsDOMJSUtils.h"
#include "nsExceptionHandler.h"
#include "nsJSUtils.h"
#include "nsWrapperCache.h"
#include "nsStringBuffer.h"
#include "GeckoProfiler.h"

#ifdef MOZ_GECKO_PROFILER
#include "ProfilerMarkerPayload.h"
#endif

#include "nsIException.h"
#include "nsIPlatformInfo.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"

#ifdef NIGHTLY_BUILD
// For performance reasons, we make the JS Dev Error Interceptor a Nightly-only feature.
#define MOZ_JS_DEV_ERROR_INTERCEPTOR = 1
#endif // NIGHTLY_BUILD

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

struct DeferredFinalizeFunctionHolder
{
  DeferredFinalizeFunction run;
  void* data;
};

class IncrementalFinalizeRunnable : public CancelableRunnable
{
  typedef AutoTArray<DeferredFinalizeFunctionHolder, 16> DeferredFinalizeArray;
  typedef CycleCollectedJSRuntime::DeferredFinalizerTable DeferredFinalizerTable;

  CycleCollectedJSRuntime* mRuntime;
  DeferredFinalizeArray mDeferredFinalizeFunctions;
  uint32_t mFinalizeFunctionToRun;
  bool mReleasing;

  static const PRTime SliceMillis = 5; /* ms */

public:
  IncrementalFinalizeRunnable(CycleCollectedJSRuntime* aRt,
                              DeferredFinalizerTable& aFinalizerTable);
  virtual ~IncrementalFinalizeRunnable();

  void ReleaseNow(bool aLimited);

  NS_DECL_NSIRUNNABLE
};

} // namespace mozilla

struct NoteWeakMapChildrenTracer : public JS::CallbackTracer
{
  NoteWeakMapChildrenTracer(JSRuntime* aRt,
                            nsCycleCollectionNoteRootCallback& aCb)
    : JS::CallbackTracer(aRt), mCb(aCb), mTracedAny(false), mMap(nullptr),
      mKey(nullptr), mKeyDelegate(nullptr)
  {
    setCanSkipJsids(true);
  }
  void onChild(const JS::GCCellPtr& aThing) override;
  nsCycleCollectionNoteRootCallback& mCb;
  bool mTracedAny;
  JSObject* mMap;
  JS::GCCellPtr mKey;
  JSObject* mKeyDelegate;
};

void
NoteWeakMapChildrenTracer::onChild(const JS::GCCellPtr& aThing)
{
  if (aThing.is<JSString>()) {
    return;
  }

  if (!JS::GCThingIsMarkedGray(aThing) && !mCb.WantAllTraces()) {
    return;
  }

  if (AddToCCKind(aThing.kind())) {
    mCb.NoteWeakMapping(mMap, mKey, mKeyDelegate, aThing);
    mTracedAny = true;
  } else {
    JS::TraceChildren(this, aThing);
  }
}

struct NoteWeakMapsTracer : public js::WeakMapTracer
{
  NoteWeakMapsTracer(JSRuntime* aRt, nsCycleCollectionNoteRootCallback& aCccb)
    : js::WeakMapTracer(aRt), mCb(aCccb), mChildTracer(aRt, aCccb)
  {
  }
  void trace(JSObject* aMap, JS::GCCellPtr aKey, JS::GCCellPtr aValue) override;
  nsCycleCollectionNoteRootCallback& mCb;
  NoteWeakMapChildrenTracer mChildTracer;
};

void
NoteWeakMapsTracer::trace(JSObject* aMap, JS::GCCellPtr aKey,
                          JS::GCCellPtr aValue)
{
  // If nothing that could be held alive by this entry is marked gray, return.
  if ((!aKey || !JS::GCThingIsMarkedGray(aKey)) &&
      MOZ_LIKELY(!mCb.WantAllTraces())) {
    if (!aValue || !JS::GCThingIsMarkedGray(aValue) || aValue.is<JSString>()) {
      return;
    }
  }

  // The cycle collector can only properly reason about weak maps if it can
  // reason about the liveness of their keys, which in turn requires that
  // the key can be represented in the cycle collector graph.  All existing
  // uses of weak maps use either objects or scripts as keys, which are okay.
  MOZ_ASSERT(AddToCCKind(aKey.kind()));

  // As an emergency fallback for non-debug builds, if the key is not
  // representable in the cycle collector graph, we treat it as marked.  This
  // can cause leaks, but is preferable to ignoring the binding, which could
  // cause the cycle collector to free live objects.
  if (!AddToCCKind(aKey.kind())) {
    aKey = nullptr;
  }

  JSObject* kdelegate = nullptr;
  if (aKey.is<JSObject>()) {
    kdelegate = js::GetWeakmapKeyDelegate(&aKey.as<JSObject>());
  }

  if (AddToCCKind(aValue.kind())) {
    mCb.NoteWeakMapping(aMap, aKey, kdelegate, aValue);
  } else {
    mChildTracer.mTracedAny = false;
    mChildTracer.mMap = aMap;
    mChildTracer.mKey = aKey;
    mChildTracer.mKeyDelegate = kdelegate;

    if (!aValue.is<JSString>()) {
      JS::TraceChildren(&mChildTracer, aValue);
    }

    // The delegate could hold alive the key, so report something to the CC
    // if we haven't already.
    if (!mChildTracer.mTracedAny &&
        aKey && JS::GCThingIsMarkedGray(aKey) && kdelegate) {
      mCb.NoteWeakMapping(aMap, aKey, kdelegate, nullptr);
    }
  }
}

// Report whether the key or value of a weak mapping entry are gray but need to
// be marked black.
static void
ShouldWeakMappingEntryBeBlack(JSObject* aMap, JS::GCCellPtr aKey, JS::GCCellPtr aValue,
                              bool* aKeyShouldBeBlack, bool* aValueShouldBeBlack)
{
  *aKeyShouldBeBlack = false;
  *aValueShouldBeBlack = false;

  // If nothing that could be held alive by this entry is marked gray, return.
  bool keyMightNeedMarking = aKey && JS::GCThingIsMarkedGray(aKey);
  bool valueMightNeedMarking = aValue && JS::GCThingIsMarkedGray(aValue) &&
    aValue.kind() != JS::TraceKind::String;
  if (!keyMightNeedMarking && !valueMightNeedMarking) {
    return;
  }

  if (!AddToCCKind(aKey.kind())) {
    aKey = nullptr;
  }

  if (keyMightNeedMarking && aKey.is<JSObject>()) {
    JSObject* kdelegate = js::GetWeakmapKeyDelegate(&aKey.as<JSObject>());
    if (kdelegate && !JS::ObjectIsMarkedGray(kdelegate) &&
        (!aMap || !JS::ObjectIsMarkedGray(aMap)))
    {
      *aKeyShouldBeBlack = true;
    }
  }

  if (aValue && JS::GCThingIsMarkedGray(aValue) &&
      (!aKey || !JS::GCThingIsMarkedGray(aKey)) &&
      (!aMap || !JS::ObjectIsMarkedGray(aMap)) &&
      aValue.kind() != JS::TraceKind::Shape) {
    *aValueShouldBeBlack = true;
  }
}

struct FixWeakMappingGrayBitsTracer : public js::WeakMapTracer
{
  explicit FixWeakMappingGrayBitsTracer(JSRuntime* aRt)
    : js::WeakMapTracer(aRt)
  {
  }

  void
  FixAll()
  {
    do {
      mAnyMarked = false;
      js::TraceWeakMaps(this);
    } while (mAnyMarked);
  }

  void trace(JSObject* aMap, JS::GCCellPtr aKey, JS::GCCellPtr aValue) override
  {
    bool keyShouldBeBlack;
    bool valueShouldBeBlack;
    ShouldWeakMappingEntryBeBlack(aMap, aKey, aValue,
                                  &keyShouldBeBlack, &valueShouldBeBlack);
    if (keyShouldBeBlack && JS::UnmarkGrayGCThingRecursively(aKey)) {
      mAnyMarked = true;
    }

    if (valueShouldBeBlack && JS::UnmarkGrayGCThingRecursively(aValue)) {
      mAnyMarked = true;
    }
  }

  MOZ_INIT_OUTSIDE_CTOR bool mAnyMarked;
};

#ifdef DEBUG
// Check whether weak maps are marked correctly according to the logic above.
struct CheckWeakMappingGrayBitsTracer : public js::WeakMapTracer
{
  explicit CheckWeakMappingGrayBitsTracer(JSRuntime* aRt)
    : js::WeakMapTracer(aRt), mFailed(false)
  {
  }

  static bool
  Check(JSRuntime* aRt)
  {
    CheckWeakMappingGrayBitsTracer tracer(aRt);
    js::TraceWeakMaps(&tracer);
    return !tracer.mFailed;
  }

  void trace(JSObject* aMap, JS::GCCellPtr aKey, JS::GCCellPtr aValue) override
  {
    bool keyShouldBeBlack;
    bool valueShouldBeBlack;
    ShouldWeakMappingEntryBeBlack(aMap, aKey, aValue,
                                  &keyShouldBeBlack, &valueShouldBeBlack);

    if (keyShouldBeBlack) {
      fprintf(stderr, "Weak mapping key %p of map %p should be black\n",
              aKey.asCell(), aMap);
      mFailed = true;
    }

    if (valueShouldBeBlack) {
      fprintf(stderr, "Weak mapping value %p of map %p should be black\n",
              aValue.asCell(), aMap);
      mFailed = true;
    }
  }

  bool mFailed;
};
#endif // DEBUG

static void
CheckParticipatesInCycleCollection(JS::GCCellPtr aThing, const char* aName,
                                   void* aClosure)
{
  bool* cycleCollectionEnabled = static_cast<bool*>(aClosure);

  if (*cycleCollectionEnabled) {
    return;
  }

  if (AddToCCKind(aThing.kind()) && JS::GCThingIsMarkedGray(aThing)) {
    *cycleCollectionEnabled = true;
  }
}

NS_IMETHODIMP
JSGCThingParticipant::TraverseNative(void* aPtr,
                                     nsCycleCollectionTraversalCallback& aCb)
{
  auto runtime = reinterpret_cast<CycleCollectedJSRuntime*>(
    reinterpret_cast<char*>(this) - offsetof(CycleCollectedJSRuntime,
                                             mGCThingCycleCollectorGlobal));

  JS::GCCellPtr cellPtr(aPtr, JS::GCThingTraceKind(aPtr));
  runtime->TraverseGCThing(CycleCollectedJSRuntime::TRAVERSE_FULL, cellPtr, aCb);
  return NS_OK;
}

// NB: This is only used to initialize the participant in
// CycleCollectedJSRuntime. It should never be used directly.
static JSGCThingParticipant sGCThingCycleCollectorGlobal;

NS_IMETHODIMP
JSZoneParticipant::TraverseNative(void* aPtr,
                                  nsCycleCollectionTraversalCallback& aCb)
{
  auto runtime = reinterpret_cast<CycleCollectedJSRuntime*>(
    reinterpret_cast<char*>(this) - offsetof(CycleCollectedJSRuntime,
                                             mJSZoneCycleCollectorGlobal));

  MOZ_ASSERT(!aCb.WantAllTraces());
  JS::Zone* zone = static_cast<JS::Zone*>(aPtr);

  runtime->TraverseZone(zone, aCb);
  return NS_OK;
}

struct TraversalTracer : public JS::CallbackTracer
{
  TraversalTracer(JSRuntime* aRt, nsCycleCollectionTraversalCallback& aCb)
    : JS::CallbackTracer(aRt, DoNotTraceWeakMaps), mCb(aCb)
  {
    setCanSkipJsids(true);
  }
  void onChild(const JS::GCCellPtr& aThing) override;
  nsCycleCollectionTraversalCallback& mCb;
};

void
TraversalTracer::onChild(const JS::GCCellPtr& aThing)
{
  // Checking strings and symbols for being gray is rather slow, and we don't
  // need either of them for the cycle collector.
  if (aThing.is<JSString>() || aThing.is<JS::Symbol>()) {
    return;
  }

  // Don't traverse non-gray objects, unless we want all traces.
  if (!JS::GCThingIsMarkedGray(aThing) && !mCb.WantAllTraces()) {
    return;
  }

  /*
   * This function needs to be careful to avoid stack overflow. Normally, when
   * AddToCCKind is true, the recursion terminates immediately as we just add
   * |thing| to the CC graph. So overflow is only possible when there are long
   * or cyclic chains of non-AddToCCKind GC things. Places where this can occur
   * use special APIs to handle such chains iteratively.
   */
  if (AddToCCKind(aThing.kind())) {
    if (MOZ_UNLIKELY(mCb.WantDebugInfo())) {
      char buffer[200];
      getTracingEdgeName(buffer, sizeof(buffer));
      mCb.NoteNextEdgeName(buffer);
    }
    mCb.NoteJSChild(aThing);
  } else if (aThing.is<js::Shape>()) {
    // The maximum depth of traversal when tracing a Shape is unbounded, due to
    // the parent pointers on the shape.
    JS_TraceShapeCycleCollectorChildren(this, aThing);
  } else if (aThing.is<js::ObjectGroup>()) {
    // The maximum depth of traversal when tracing an ObjectGroup is unbounded,
    // due to information attached to the groups which can lead other groups to
    // be traced.
    JS_TraceObjectGroupCycleCollectorChildren(this, aThing);
  } else {
    JS::TraceChildren(this, aThing);
  }
}

static void
NoteJSChildGrayWrapperShim(void* aData, JS::GCCellPtr aThing)
{
  TraversalTracer* trc = static_cast<TraversalTracer*>(aData);
  trc->onChild(aThing);
}

/*
 * The cycle collection participant for a Zone is intended to produce the same
 * results as if all of the gray GCthings in a zone were merged into a single node,
 * except for self-edges. This avoids the overhead of representing all of the GCthings in
 * the zone in the cycle collector graph, which should be much faster if many of
 * the GCthings in the zone are gray.
 *
 * Zone merging should not always be used, because it is a conservative
 * approximation of the true cycle collector graph that can incorrectly identify some
 * garbage objects as being live. For instance, consider two cycles that pass through a
 * zone, where one is garbage and the other is live. If we merge the entire
 * zone, the cycle collector will think that both are alive.
 *
 * We don't have to worry about losing track of a garbage cycle, because any such garbage
 * cycle incorrectly identified as live must contain at least one C++ to JS edge, and
 * XPConnect will always add the C++ object to the CC graph. (This is in contrast to pure
 * C++ garbage cycles, which must always be properly identified, because we clear the
 * purple buffer during every CC, which may contain the last reference to a garbage
 * cycle.)
 */

// NB: This is only used to initialize the participant in
// CycleCollectedJSRuntime. It should never be used directly.
static const JSZoneParticipant sJSZoneCycleCollectorGlobal;

static
void JSObjectsTenuredCb(JSContext* aContext, void* aData)
{
  static_cast<CycleCollectedJSRuntime*>(aData)->JSObjectsTenured();
}

bool
mozilla::GetBuildId(JS::BuildIdCharVector* aBuildID)
{
  nsCOMPtr<nsIPlatformInfo> info = do_GetService("@mozilla.org/xre/app-info;1");
  if (!info) {
    return false;
  }

  nsCString buildID;
  nsresult rv = info->GetPlatformBuildID(buildID);
  NS_ENSURE_SUCCESS(rv, false);

  if (!aBuildID->resize(buildID.Length())) {
    return false;
  }

  for (size_t i = 0; i < buildID.Length(); i++) {
    (*aBuildID)[i] = buildID[i];
  }

  return true;
}

static void
MozCrashWarningReporter(JSContext*, JSErrorReport*)
{
  MOZ_CRASH("Why is someone touching JSAPI without an AutoJSAPI?");
}

CycleCollectedJSRuntime::CycleCollectedJSRuntime(JSContext* aCx)
  : mGCThingCycleCollectorGlobal(sGCThingCycleCollectorGlobal)
  , mJSZoneCycleCollectorGlobal(sJSZoneCycleCollectorGlobal)
  , mJSRuntime(JS_GetRuntime(aCx))
  , mHasPendingIdleGCTask(false)
  , mPrevGCSliceCallback(nullptr)
  , mPrevGCNurseryCollectionCallback(nullptr)
  , mJSHolderMap(256)
  , mOutOfMemoryState(OOMState::OK)
  , mLargeAllocationFailureState(OOMState::OK)
#ifdef DEBUG
  , mShutdownCalled(false)
#endif
{
  MOZ_COUNT_CTOR(CycleCollectedJSRuntime);
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(mJSRuntime);

  if (!JS_AddExtraGCRootsTracer(aCx, TraceBlackJS, this)) {
    MOZ_CRASH("JS_AddExtraGCRootsTracer failed");
  }
  JS_SetGrayGCRootsTracer(aCx, TraceGrayJS, this);
  JS_SetGCCallback(aCx, GCCallback, this);
  mPrevGCSliceCallback = JS::SetGCSliceCallback(aCx, GCSliceCallback);

  if (NS_IsMainThread()) {
    // We would like to support all threads here, but the way timeline consumers
    // are set up currently, you can either add a marker for one specific
    // docshell, or for every consumer globally. We would like to add a marker
    // for every consumer observing anything on this thread, but that is not
    // currently possible. For now, add global markers only when we are on the
    // main thread, since the UI for this tracing data only displays data
    // relevant to the main-thread.
    mPrevGCNurseryCollectionCallback = JS::SetGCNurseryCollectionCallback(
      aCx, GCNurseryCollectionCallback);
  }

  JS_SetObjectsTenuredCallback(aCx, JSObjectsTenuredCb, this);
  JS::SetOutOfMemoryCallback(aCx, OutOfMemoryCallback, this);
  JS_SetExternalStringSizeofCallback(aCx, SizeofExternalStringCallback);
  JS::SetBuildIdOp(aCx, GetBuildId);
  JS::SetWarningReporter(aCx, MozCrashWarningReporter);

  js::AutoEnterOOMUnsafeRegion::setAnnotateOOMAllocationSizeCallback(
    CrashReporter::AnnotateOOMAllocationSize);

  static js::DOMCallbacks DOMcallbacks = {
    InstanceClassHasProtoAtDepth
  };
  SetDOMCallbacks(aCx, &DOMcallbacks);
  js::SetScriptEnvironmentPreparer(aCx, &mEnvironmentPreparer);

  JS::dbg::SetDebuggerMallocSizeOf(aCx, moz_malloc_size_of);

#ifdef MOZ_JS_DEV_ERROR_INTERCEPTOR
  JS_SetErrorInterceptorCallback(mJSRuntime, &mErrorInterceptor);
#endif // MOZ_JS_DEV_ERROR_INTERCEPTOR

  if (recordreplay::IsRecordingOrReplaying()) {
    // When recording or replaying, the set of deferred things needing
    // finalization  will be consistent between executions, see
    // DeferredFinalize.h. This callback is used with the record/replay trigger
    // mechanism to make sure that finalization of those things also happens at
    // a consistent point.
    recordreplay::RegisterTrigger(this,
                                  [=]() {
                                    FinalizeDeferredThings(CycleCollectedJSContext::FinalizeNow);
                                  });
  }
}

void
CycleCollectedJSRuntime::Shutdown(JSContext* cx)
{
#ifdef MOZ_JS_DEV_ERROR_INTERCEPTOR
  mErrorInterceptor.Shutdown(mJSRuntime);
#endif // MOZ_JS_DEV_ERROR_INTERCEPTOR
  JS_RemoveExtraGCRootsTracer(cx, TraceBlackJS, this);
  JS_RemoveExtraGCRootsTracer(cx, TraceGrayJS, this);
#ifdef DEBUG
  mShutdownCalled = true;
#endif
}

CycleCollectedJSRuntime::~CycleCollectedJSRuntime()
{
  MOZ_COUNT_DTOR(CycleCollectedJSRuntime);
  MOZ_ASSERT(!mDeferredFinalizerTable.Count());
  MOZ_ASSERT(mShutdownCalled);

  if (recordreplay::IsRecordingOrReplaying()) {
    recordreplay::UnregisterTrigger(this);
  }
}

void
CycleCollectedJSRuntime::AddContext(CycleCollectedJSContext* aContext)
{
  mContexts.insertBack(aContext);
}

void
CycleCollectedJSRuntime::RemoveContext(CycleCollectedJSContext* aContext)
{
  aContext->removeFrom(mContexts);
}

size_t
CycleCollectedJSRuntime::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;

  // We're deliberately not measuring anything hanging off the entries in
  // mJSHolders.
  n += mJSHolders.SizeOfExcludingThis(aMallocSizeOf);
  n += mJSHolderMap.ShallowSizeOfExcludingThis(aMallocSizeOf);

  return n;
}

void
CycleCollectedJSRuntime::UnmarkSkippableJSHolders()
{
  for (auto iter = mJSHolders.Iter(); !iter.Done(); iter.Next()) {
    void* holder = iter.Get().mHolder;
    nsScriptObjectTracer* tracer = iter.Get().mTracer;
    tracer->CanSkip(holder, true);
  }
}

void
CycleCollectedJSRuntime::DescribeGCThing(bool aIsMarked, JS::GCCellPtr aThing,
                                         nsCycleCollectionTraversalCallback& aCb) const
{
  if (!aCb.WantDebugInfo()) {
    aCb.DescribeGCedNode(aIsMarked, "JS Object");
    return;
  }

  char name[72];
  uint64_t compartmentAddress = 0;
  if (aThing.is<JSObject>()) {
    JSObject* obj = &aThing.as<JSObject>();
    compartmentAddress = (uint64_t)js::GetObjectCompartment(obj);
    const js::Class* clasp = js::GetObjectClass(obj);

    // Give the subclass a chance to do something
    if (DescribeCustomObjects(obj, clasp, name)) {
      // Nothing else to do!
    } else if (js::IsFunctionObject(obj)) {
      JSFunction* fun = JS_GetObjectFunction(obj);
      JSString* str = JS_GetFunctionDisplayId(fun);
      if (str) {
        JSFlatString* flat = JS_ASSERT_STRING_IS_FLAT(str);
        nsAutoString chars;
        AssignJSFlatString(chars, flat);
        NS_ConvertUTF16toUTF8 fname(chars);
        SprintfLiteral(name, "JS Object (Function - %s)", fname.get());
      } else {
        SprintfLiteral(name, "JS Object (Function)");
      }
    } else {
      SprintfLiteral(name, "JS Object (%s)", clasp->name);
    }
  } else {
    SprintfLiteral(name, "JS %s", JS::GCTraceKindToAscii(aThing.kind()));
  }

  // Disable printing global for objects while we figure out ObjShrink fallout.
  aCb.DescribeGCedNode(aIsMarked, name, compartmentAddress);
}

void
CycleCollectedJSRuntime::NoteGCThingJSChildren(JS::GCCellPtr aThing,
                                               nsCycleCollectionTraversalCallback& aCb) const
{
  TraversalTracer trc(mJSRuntime, aCb);
  JS::TraceChildren(&trc, aThing);
}

void
CycleCollectedJSRuntime::NoteGCThingXPCOMChildren(const js::Class* aClasp,
                                                  JSObject* aObj,
                                                  nsCycleCollectionTraversalCallback& aCb) const
{
  MOZ_ASSERT(aClasp);
  MOZ_ASSERT(aClasp == js::GetObjectClass(aObj));

  if (NoteCustomGCThingXPCOMChildren(aClasp, aObj, aCb)) {
    // Nothing else to do!
    return;
  }
  // XXX This test does seem fragile, we should probably whitelist classes
  //     that do hold a strong reference, but that might not be possible.
  else if (aClasp->flags & JSCLASS_HAS_PRIVATE &&
           aClasp->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "js::GetObjectPrivate(obj)");
    aCb.NoteXPCOMChild(static_cast<nsISupports*>(js::GetObjectPrivate(aObj)));
  } else {
    const DOMJSClass* domClass = GetDOMClass(aObj);
    if (domClass) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCb, "UnwrapDOMObject(obj)");
      // It's possible that our object is an unforgeable holder object, in
      // which case it doesn't actually have a C++ DOM object associated with
      // it.  Use UnwrapPossiblyNotInitializedDOMObject, which produces null in
      // that case, since NoteXPCOMChild/NoteNativeChild are null-safe.
      if (domClass->mDOMObjectIsISupports) {
        aCb.NoteXPCOMChild(UnwrapPossiblyNotInitializedDOMObject<nsISupports>(aObj));
      } else if (domClass->mParticipant) {
        aCb.NoteNativeChild(UnwrapPossiblyNotInitializedDOMObject<void>(aObj),
                            domClass->mParticipant);
      }
    }
  }
}

void
CycleCollectedJSRuntime::TraverseGCThing(TraverseSelect aTs, JS::GCCellPtr aThing,
                                         nsCycleCollectionTraversalCallback& aCb)
{
  bool isMarkedGray = JS::GCThingIsMarkedGray(aThing);

  if (aTs == TRAVERSE_FULL) {
    DescribeGCThing(!isMarkedGray, aThing, aCb);
  }

  // If this object is alive, then all of its children are alive. For JS objects,
  // the black-gray invariant ensures the children are also marked black. For C++
  // objects, the ref count from this object will keep them alive. Thus we don't
  // need to trace our children, unless we are debugging using WantAllTraces.
  if (!isMarkedGray && !aCb.WantAllTraces()) {
    return;
  }

  if (aTs == TRAVERSE_FULL) {
    NoteGCThingJSChildren(aThing, aCb);
  }

  if (aThing.is<JSObject>()) {
    JSObject* obj = &aThing.as<JSObject>();
    NoteGCThingXPCOMChildren(js::GetObjectClass(obj), obj, aCb);
  }
}

struct TraverseObjectShimClosure
{
  nsCycleCollectionTraversalCallback& cb;
  CycleCollectedJSRuntime* self;
};

void
CycleCollectedJSRuntime::TraverseZone(JS::Zone* aZone,
                                      nsCycleCollectionTraversalCallback& aCb)
{
  /*
   * We treat the zone as being gray. We handle non-gray GCthings in the
   * zone by not reporting their children to the CC. The black-gray invariant
   * ensures that any JS children will also be non-gray, and thus don't need to be
   * added to the graph. For C++ children, not representing the edge from the
   * non-gray JS GCthings to the C++ object will keep the child alive.
   *
   * We don't allow zone merging in a WantAllTraces CC, because then these
   * assumptions don't hold.
   */
  aCb.DescribeGCedNode(false, "JS Zone");

  /*
   * Every JS child of everything in the zone is either in the zone
   * or is a cross-compartment wrapper. In the former case, we don't need to
   * represent these edges in the CC graph because JS objects are not ref counted.
   * In the latter case, the JS engine keeps a map of these wrappers, which we
   * iterate over. Edges between compartments in the same zone will add
   * unnecessary loop edges to the graph (bug 842137).
   */
  TraversalTracer trc(mJSRuntime, aCb);
  js::VisitGrayWrapperTargets(aZone, NoteJSChildGrayWrapperShim, &trc);

  /*
   * To find C++ children of things in the zone, we scan every JS Object in
   * the zone. Only JS Objects can have C++ children.
   */
  TraverseObjectShimClosure closure = { aCb, this };
  js::IterateGrayObjects(aZone, TraverseObjectShim, &closure);
}

/* static */ void
CycleCollectedJSRuntime::TraverseObjectShim(void* aData, JS::GCCellPtr aThing)
{
  TraverseObjectShimClosure* closure =
    static_cast<TraverseObjectShimClosure*>(aData);

  MOZ_ASSERT(aThing.is<JSObject>());
  closure->self->TraverseGCThing(CycleCollectedJSRuntime::TRAVERSE_CPP,
                                 aThing, closure->cb);
}

void
CycleCollectedJSRuntime::TraverseNativeRoots(nsCycleCollectionNoteRootCallback& aCb)
{
  // NB: This is here just to preserve the existing XPConnect order. I doubt it
  // would hurt to do this after the JS holders.
  TraverseAdditionalNativeRoots(aCb);

  for (auto iter = mJSHolders.Iter(); !iter.Done(); iter.Next()) {
    void* holder = iter.Get().mHolder;
    nsScriptObjectTracer* tracer = iter.Get().mTracer;

    bool noteRoot = false;
    if (MOZ_UNLIKELY(aCb.WantAllTraces())) {
      noteRoot = true;
    } else {
      tracer->Trace(holder,
                    TraceCallbackFunc(CheckParticipatesInCycleCollection),
                    &noteRoot);
    }

    if (noteRoot) {
      aCb.NoteNativeRoot(holder, tracer);
    }
  }
}

/* static */ void
CycleCollectedJSRuntime::TraceBlackJS(JSTracer* aTracer, void* aData)
{
  CycleCollectedJSRuntime* self = static_cast<CycleCollectedJSRuntime*>(aData);

  self->TraceNativeBlackRoots(aTracer);
}

/* static */ void
CycleCollectedJSRuntime::TraceGrayJS(JSTracer* aTracer, void* aData)
{
  CycleCollectedJSRuntime* self = static_cast<CycleCollectedJSRuntime*>(aData);

  // Mark these roots as gray so the CC can walk them later.
  self->TraceNativeGrayRoots(aTracer);
}

/* static */ void
CycleCollectedJSRuntime::GCCallback(JSContext* aContext,
                                    JSGCStatus aStatus,
                                    void* aData)
{
  CycleCollectedJSRuntime* self = static_cast<CycleCollectedJSRuntime*>(aData);

  MOZ_ASSERT(CycleCollectedJSContext::Get()->Context() == aContext);
  MOZ_ASSERT(CycleCollectedJSContext::Get()->Runtime() == self);

  self->OnGC(aContext, aStatus);
}

/* static */ void
CycleCollectedJSRuntime::GCSliceCallback(JSContext* aContext,
                                         JS::GCProgress aProgress,
                                         const JS::GCDescription& aDesc)
{
  CycleCollectedJSRuntime* self = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(CycleCollectedJSContext::Get()->Context() == aContext);

#ifdef MOZ_GECKO_PROFILER
  if (profiler_is_active()) {
    if (aProgress == JS::GC_CYCLE_END) {
      profiler_add_marker(
        "GCMajor",
        MakeUnique<GCMajorMarkerPayload>(aDesc.startTime(aContext),
                                         aDesc.endTime(aContext),
                                         aDesc.summaryToJSON(aContext)));
    } else if (aProgress == JS::GC_SLICE_END) {
      profiler_add_marker(
        "GCSlice",
        MakeUnique<GCSliceMarkerPayload>(aDesc.lastSliceStart(aContext),
                                         aDesc.lastSliceEnd(aContext),
                                         aDesc.sliceToJSON(aContext)));
    }
  }
#endif

  if (aProgress == JS::GC_CYCLE_END &&
      JS::dbg::FireOnGarbageCollectionHookRequired(aContext)) {
    JS::gcreason::Reason reason = aDesc.reason_;
    Unused <<
      NS_WARN_IF(NS_FAILED(DebuggerOnGCRunnable::Enqueue(aContext, aDesc)) &&
                 reason != JS::gcreason::SHUTDOWN_CC &&
                 reason != JS::gcreason::DESTROY_RUNTIME &&
                 reason != JS::gcreason::XPCONNECT_SHUTDOWN);
  }

  if (self->mPrevGCSliceCallback) {
    self->mPrevGCSliceCallback(aContext, aProgress, aDesc);
  }
}

class MinorGCMarker : public TimelineMarker
{
private:
  JS::gcreason::Reason mReason;

public:
  MinorGCMarker(MarkerTracingType aTracingType,
                JS::gcreason::Reason aReason)
    : TimelineMarker("MinorGC",
                     aTracingType,
                     MarkerStackRequest::NO_STACK)
    , mReason(aReason)
    {
      MOZ_ASSERT(aTracingType == MarkerTracingType::START ||
                 aTracingType == MarkerTracingType::END);
    }

  MinorGCMarker(JS::GCNurseryProgress aProgress,
                JS::gcreason::Reason aReason)
    : TimelineMarker("MinorGC",
                     aProgress == JS::GCNurseryProgress::GC_NURSERY_COLLECTION_START
                       ? MarkerTracingType::START
                       : MarkerTracingType::END,
                     MarkerStackRequest::NO_STACK)
    , mReason(aReason)
  { }

  virtual void
  AddDetails(JSContext* aCx,
             dom::ProfileTimelineMarker& aMarker) override
  {
    TimelineMarker::AddDetails(aCx, aMarker);

    if (GetTracingType() == MarkerTracingType::START) {
      auto reason = JS::gcreason::ExplainReason(mReason);
      aMarker.mCauseName.Construct(NS_ConvertUTF8toUTF16(reason));
    }
  }

  virtual UniquePtr<AbstractTimelineMarker>
  Clone() override
  {
    auto clone = MakeUnique<MinorGCMarker>(GetTracingType(), mReason);
    clone->SetCustomTime(GetTime());
    return UniquePtr<AbstractTimelineMarker>(std::move(clone));
  }
};

/* static */ void
CycleCollectedJSRuntime::GCNurseryCollectionCallback(JSContext* aContext,
                                                     JS::GCNurseryProgress aProgress,
                                                     JS::gcreason::Reason aReason)
{
  CycleCollectedJSRuntime* self = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(CycleCollectedJSContext::Get()->Context() == aContext);
  MOZ_ASSERT(NS_IsMainThread());

  if (!recordreplay::IsRecordingOrReplaying()) {
    RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
    if (timelines && !timelines->IsEmpty()) {
      UniquePtr<AbstractTimelineMarker> abstractMarker(
        MakeUnique<MinorGCMarker>(aProgress, aReason));
      timelines->AddMarkerForAllObservedDocShells(abstractMarker);
    }
  }

  if (aProgress == JS::GCNurseryProgress::GC_NURSERY_COLLECTION_START) {
    recordreplay::AutoPassThroughThreadEvents pt;
    self->mLatestNurseryCollectionStart = TimeStamp::Now();
  }
#ifdef MOZ_GECKO_PROFILER
  else if (aProgress == JS::GCNurseryProgress::GC_NURSERY_COLLECTION_END &&
           profiler_is_active())
  {
    profiler_add_marker(
      "GCMinor",
      MakeUnique<GCMinorMarkerPayload>(self->mLatestNurseryCollectionStart,
                                       TimeStamp::Now(),
                                       JS::MinorGcToJSON(aContext)));
  }
#endif

  if (self->mPrevGCNurseryCollectionCallback) {
    self->mPrevGCNurseryCollectionCallback(aContext, aProgress, aReason);
  }
}


/* static */ void
CycleCollectedJSRuntime::OutOfMemoryCallback(JSContext* aContext,
                                             void* aData)
{
  CycleCollectedJSRuntime* self = static_cast<CycleCollectedJSRuntime*>(aData);

  MOZ_ASSERT(CycleCollectedJSContext::Get()->Context() == aContext);
  MOZ_ASSERT(CycleCollectedJSContext::Get()->Runtime() == self);

  self->OnOutOfMemory();
}

/* static */ size_t
CycleCollectedJSRuntime::SizeofExternalStringCallback(JSString* aStr,
                                                      MallocSizeOf aMallocSizeOf)
{
  // We promised the JS engine we would not GC.  Enforce that:
  JS::AutoCheckCannotGC autoCannotGC;

  if (!XPCStringConvert::IsDOMString(aStr)) {
    // Might be a literal or something we don't understand.  Just claim 0.
    return 0;
  }

  const char16_t* chars = JS_GetTwoByteExternalStringChars(aStr);
  const nsStringBuffer* buf = nsStringBuffer::FromData((void*)chars);
  // We want sizeof including this, because the entire string buffer is owned by
  // the external string.  But only report here if we're unshared; if we're
  // shared then we don't know who really owns this data.
  return buf->SizeOfIncludingThisIfUnshared(aMallocSizeOf);
}

struct JsGcTracer : public TraceCallbacks
{
  virtual void Trace(JS::Heap<JS::Value>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS::TraceEdge(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<jsid>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS::TraceEdge(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSObject*>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS::TraceEdge(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JSObject** aPtr, const char* aName,
                     void* aClosure) const override
  {
    js::UnsafeTraceManuallyBarrieredEdge(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::TenuredHeap<JSObject*>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS::TraceEdge(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSString*>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS::TraceEdge(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSScript*>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS::TraceEdge(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSFunction*>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS::TraceEdge(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
};

void
mozilla::TraceScriptHolder(nsISupports* aHolder, JSTracer* aTracer)
{
  nsXPCOMCycleCollectionParticipant* participant = nullptr;
  CallQueryInterface(aHolder, &participant);
  participant->Trace(aHolder, JsGcTracer(), aTracer);
}

void
CycleCollectedJSRuntime::TraceNativeGrayRoots(JSTracer* aTracer)
{
  // NB: This is here just to preserve the existing XPConnect order. I doubt it
  // would hurt to do this after the JS holders.
  TraceAdditionalNativeGrayRoots(aTracer);

  for (auto iter = mJSHolders.Iter(); !iter.Done(); iter.Next()) {
    void* holder = iter.Get().mHolder;
    nsScriptObjectTracer* tracer = iter.Get().mTracer;
    tracer->Trace(holder, JsGcTracer(), aTracer);
  }
}

void
CycleCollectedJSRuntime::AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer)
{
  auto entry = mJSHolderMap.LookupForAdd(aHolder);
  if (entry) {
    JSHolderInfo* info = entry.Data();
    MOZ_ASSERT(info->mHolder == aHolder);
    info->mTracer = aTracer;
    return;
  }

  mJSHolders.InfallibleAppend(JSHolderInfo {aHolder, aTracer});
  entry.OrInsert([&] {return &mJSHolders.GetLast();});
}

struct ClearJSHolder : public TraceCallbacks
{
  virtual void Trace(JS::Heap<JS::Value>* aPtr, const char*, void*) const override
  {
    aPtr->setUndefined();
  }

  virtual void Trace(JS::Heap<jsid>* aPtr, const char*, void*) const override
  {
    *aPtr = JSID_VOID;
  }

  virtual void Trace(JS::Heap<JSObject*>* aPtr, const char*, void*) const override
  {
    *aPtr = nullptr;
  }

  virtual void Trace(JSObject** aPtr, const char* aName,
                     void* aClosure) const override
  {
    *aPtr = nullptr;
  }

  virtual void Trace(JS::TenuredHeap<JSObject*>* aPtr, const char*, void*) const override
  {
    *aPtr = nullptr;
  }

  virtual void Trace(JS::Heap<JSString*>* aPtr, const char*, void*) const override
  {
    *aPtr = nullptr;
  }

  virtual void Trace(JS::Heap<JSScript*>* aPtr, const char*, void*) const override
  {
    *aPtr = nullptr;
  }

  virtual void Trace(JS::Heap<JSFunction*>* aPtr, const char*, void*) const override
  {
    *aPtr = nullptr;
  }
};

void
CycleCollectedJSRuntime::RemoveJSHolder(void* aHolder)
{
  auto entry = mJSHolderMap.Lookup(aHolder);
  if (entry) {
    JSHolderInfo* info = entry.Data();
    MOZ_ASSERT(info->mHolder == aHolder);
    info->mTracer->Trace(aHolder, ClearJSHolder(), nullptr);

    JSHolderInfo* lastInfo = &mJSHolders.GetLast();
    bool updateLast = (info != lastInfo);
    if (updateLast) {
      *info = *lastInfo;
    }

    mJSHolders.PopLast();
    entry.Remove();

    if (updateLast) {
      // We have to do this after removing the entry above to ensure that we
      // don't trip over the hashtable generation number assertion.
      mJSHolderMap.Put(info->mHolder, info);
    }
  }
}

#ifdef DEBUG
bool
CycleCollectedJSRuntime::IsJSHolder(void* aHolder)
{
  return mJSHolderMap.Get(aHolder, nullptr);
}

static void
AssertNoGcThing(JS::GCCellPtr aGCThing, const char* aName, void* aClosure)
{
  MOZ_ASSERT(!aGCThing);
}

void
CycleCollectedJSRuntime::AssertNoObjectsToTrace(void* aPossibleJSHolder)
{
  JSHolderInfo* info = nullptr;
  if (!mJSHolderMap.Get(aPossibleJSHolder, &info)) {
    return;
  }

  MOZ_ASSERT(info->mHolder == aPossibleJSHolder);
  info->mTracer->Trace(aPossibleJSHolder, TraceCallbackFunc(AssertNoGcThing), nullptr);
}
#endif

nsCycleCollectionParticipant*
CycleCollectedJSRuntime::GCThingParticipant()
{
  return &mGCThingCycleCollectorGlobal;
}

nsCycleCollectionParticipant*
CycleCollectedJSRuntime::ZoneParticipant()
{
  return &mJSZoneCycleCollectorGlobal;
}

nsresult
CycleCollectedJSRuntime::TraverseRoots(nsCycleCollectionNoteRootCallback& aCb)
{
  TraverseNativeRoots(aCb);

  NoteWeakMapsTracer trc(mJSRuntime, aCb);
  js::TraceWeakMaps(&trc);

  return NS_OK;
}

bool
CycleCollectedJSRuntime::UsefulToMergeZones() const
{
  return false;
}

void
CycleCollectedJSRuntime::FixWeakMappingGrayBits() const
{
  MOZ_ASSERT(!JS::IsIncrementalGCInProgress(mJSRuntime),
             "Don't call FixWeakMappingGrayBits during a GC.");
  FixWeakMappingGrayBitsTracer fixer(mJSRuntime);
  fixer.FixAll();
}

void
CycleCollectedJSRuntime::CheckGrayBits() const
{
  MOZ_ASSERT(!JS::IsIncrementalGCInProgress(mJSRuntime),
             "Don't call CheckGrayBits during a GC.");

#ifndef ANDROID
  // Bug 1346874 - The gray state check is expensive. Android tests are already
  // slow enough that this check can easily push them over the threshold to a
  // timeout.

  MOZ_ASSERT(js::CheckGrayMarkingState(mJSRuntime));
  MOZ_ASSERT(CheckWeakMappingGrayBitsTracer::Check(mJSRuntime));
#endif
}

bool
CycleCollectedJSRuntime::AreGCGrayBitsValid() const
{
  return js::AreGCGrayBitsValid(mJSRuntime);
}

void
CycleCollectedJSRuntime::GarbageCollect(uint32_t aReason) const
{
  MOZ_ASSERT(aReason < JS::gcreason::NUM_REASONS);
  JS::gcreason::Reason gcreason = static_cast<JS::gcreason::Reason>(aReason);

  JSContext* cx = CycleCollectedJSContext::Get()->Context();
  JS::PrepareForFullGC(cx);
  JS::NonIncrementalGC(cx, GC_NORMAL, gcreason);
}

void
CycleCollectedJSRuntime::JSObjectsTenured()
{
  for (auto iter = mNurseryObjects.Iter(); !iter.Done(); iter.Next()) {
    nsWrapperCache* cache = iter.Get();
    JSObject* wrapper = cache->GetWrapperMaybeDead();
    MOZ_DIAGNOSTIC_ASSERT(wrapper || recordreplay::IsReplaying());
    if (!JS::ObjectIsTenured(wrapper)) {
      MOZ_ASSERT(!cache->PreservingWrapper());
      const JSClass* jsClass = js::GetObjectJSClass(wrapper);
      jsClass->doFinalize(nullptr, wrapper);
    }
  }

#ifdef DEBUG
for (auto iter = mPreservedNurseryObjects.Iter(); !iter.Done(); iter.Next()) {
  MOZ_ASSERT(JS::ObjectIsTenured(iter.Get().get()));
}
#endif

  mNurseryObjects.Clear();
  mPreservedNurseryObjects.Clear();
}

void
CycleCollectedJSRuntime::NurseryWrapperAdded(nsWrapperCache* aCache)
{
  MOZ_ASSERT(aCache);
  MOZ_ASSERT(aCache->GetWrapperMaybeDead());
  MOZ_ASSERT(!JS::ObjectIsTenured(aCache->GetWrapperMaybeDead()));
  mNurseryObjects.InfallibleAppend(aCache);
}

void
CycleCollectedJSRuntime::NurseryWrapperPreserved(JSObject* aWrapper)
{
  mPreservedNurseryObjects.InfallibleAppend(
    JS::PersistentRooted<JSObject*>(mJSRuntime, aWrapper));
}

void
CycleCollectedJSRuntime::DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                                          DeferredFinalizeFunction aFunc,
                                          void* aThing)
{
  if (auto entry = mDeferredFinalizerTable.LookupForAdd(aFunc)) {
    aAppendFunc(entry.Data(), aThing);
  } else {
    entry.OrInsert(
      [aAppendFunc, aThing] () { return aAppendFunc(nullptr, aThing); });
  }
}

void
CycleCollectedJSRuntime::DeferredFinalize(nsISupports* aSupports)
{
  typedef DeferredFinalizerImpl<nsISupports> Impl;
  DeferredFinalize(Impl::AppendDeferredFinalizePointer, Impl::DeferredFinalize,
                   aSupports);
}

void
CycleCollectedJSRuntime::DumpJSHeap(FILE* aFile)
{
  JSContext* cx = CycleCollectedJSContext::Get()->Context();
  js::DumpHeap(cx, aFile, js::CollectNurseryBeforeDump);
}

IncrementalFinalizeRunnable::IncrementalFinalizeRunnable(CycleCollectedJSRuntime* aRt,
                                                         DeferredFinalizerTable& aFinalizers)
  : CancelableRunnable("IncrementalFinalizeRunnable")
  , mRuntime(aRt)
  , mFinalizeFunctionToRun(0)
  , mReleasing(false)
{
  for (auto iter = aFinalizers.Iter(); !iter.Done(); iter.Next()) {
    DeferredFinalizeFunction& function = iter.Key();
    void*& data = iter.Data();

    DeferredFinalizeFunctionHolder* holder =
      mDeferredFinalizeFunctions.AppendElement();
    holder->run = function;
    holder->data = data;

    iter.Remove();
  }
}

IncrementalFinalizeRunnable::~IncrementalFinalizeRunnable()
{
  MOZ_ASSERT(this != mRuntime->mFinalizeRunnable);
}

void
IncrementalFinalizeRunnable::ReleaseNow(bool aLimited)
{
  if (mReleasing) {
    NS_WARNING("Re-entering ReleaseNow");
    return;
  }
  {
    mozilla::AutoRestore<bool> ar(mReleasing);
    mReleasing = true;
    MOZ_ASSERT(mDeferredFinalizeFunctions.Length() != 0,
               "We should have at least ReleaseSliceNow to run");
    MOZ_ASSERT(mFinalizeFunctionToRun < mDeferredFinalizeFunctions.Length(),
               "No more finalizers to run?");
    if (recordreplay::IsRecordingOrReplaying()) {
      aLimited = false;
    }

    TimeDuration sliceTime = TimeDuration::FromMilliseconds(SliceMillis);
    TimeStamp started = aLimited ? TimeStamp::Now() : TimeStamp();
    bool timeout = false;
    do {
      const DeferredFinalizeFunctionHolder& function =
        mDeferredFinalizeFunctions[mFinalizeFunctionToRun];
      if (aLimited) {
        bool done = false;
        while (!timeout && !done) {
          /*
           * We don't want to read the clock too often, so we try to
           * release slices of 100 items.
           */
          done = function.run(100, function.data);
          timeout = TimeStamp::Now() - started >= sliceTime;
        }
        if (done) {
          ++mFinalizeFunctionToRun;
        }
        if (timeout) {
          break;
        }
      } else {
        while (!function.run(UINT32_MAX, function.data));
        ++mFinalizeFunctionToRun;
      }
    } while (mFinalizeFunctionToRun < mDeferredFinalizeFunctions.Length());
  }

  if (mFinalizeFunctionToRun == mDeferredFinalizeFunctions.Length()) {
    MOZ_ASSERT(mRuntime->mFinalizeRunnable == this);
    mDeferredFinalizeFunctions.Clear();
    // NB: This may delete this!
    mRuntime->mFinalizeRunnable = nullptr;
  }
}

NS_IMETHODIMP
IncrementalFinalizeRunnable::Run()
{
  AUTO_PROFILER_LABEL("IncrementalFinalizeRunnable::Run", GCCC);

  if (mRuntime->mFinalizeRunnable != this) {
    /* These items were already processed synchronously in JSGC_END. */
    MOZ_ASSERT(!mDeferredFinalizeFunctions.Length());
    return NS_OK;
  }

  TimeStamp start = TimeStamp::Now();
  ReleaseNow(true);

  if (mDeferredFinalizeFunctions.Length()) {
    nsresult rv = NS_DispatchToCurrentThread(this);
    if (NS_FAILED(rv)) {
      ReleaseNow(false);
    }
  }

  uint32_t duration = (uint32_t)((TimeStamp::Now() - start).ToMilliseconds());
  Telemetry::Accumulate(Telemetry::DEFERRED_FINALIZE_ASYNC, duration);

  return NS_OK;
}

void
CycleCollectedJSRuntime::FinalizeDeferredThings(CycleCollectedJSContext::DeferredFinalizeType aType)
{
  /*
   * If the previous GC created a runnable to finalize objects
   * incrementally, and if it hasn't finished yet, finish it now. We
   * don't want these to build up. We also don't want to allow any
   * existing incremental finalize runnables to run after a
   * non-incremental GC, since they are often used to detect leaks.
   */
  if (mFinalizeRunnable) {
    mFinalizeRunnable->ReleaseNow(false);
    if (mFinalizeRunnable) {
      // If we re-entered ReleaseNow, we couldn't delete mFinalizeRunnable and
      // we need to just continue processing it.
      return;
    }
  }

  if (mDeferredFinalizerTable.Count() == 0) {
    return;
  }

  mFinalizeRunnable = new IncrementalFinalizeRunnable(this,
                                                      mDeferredFinalizerTable);

  // Everything should be gone now.
  MOZ_ASSERT(mDeferredFinalizerTable.Count() == 0);

  if (aType == CycleCollectedJSContext::FinalizeIncrementally) {
    NS_IdleDispatchToCurrentThread(do_AddRef(mFinalizeRunnable), 2500);
  } else {
    mFinalizeRunnable->ReleaseNow(false);
    MOZ_ASSERT(!mFinalizeRunnable);
  }
}

void
CycleCollectedJSRuntime::AnnotateAndSetOutOfMemory(OOMState* aStatePtr,
                                                   OOMState aNewState)
{
  *aStatePtr = aNewState;
  CrashReporter::AnnotateCrashReport(aStatePtr == &mOutOfMemoryState
                                     ? NS_LITERAL_CSTRING("JSOutOfMemory")
                                     : NS_LITERAL_CSTRING("JSLargeAllocationFailure"),
                                     aNewState == OOMState::Reporting
                                     ? NS_LITERAL_CSTRING("Reporting")
                                     : aNewState == OOMState::Reported
                                     ? NS_LITERAL_CSTRING("Reported")
                                     : NS_LITERAL_CSTRING("Recovered"));
}

void
CycleCollectedJSRuntime::OnGC(JSContext* aContext,
                              JSGCStatus aStatus)
{
  switch (aStatus) {
    case JSGC_BEGIN:
      nsCycleCollector_prepareForGarbageCollection();
      mZonesWaitingForGC.Clear();
      break;
    case JSGC_END: {
      if (mOutOfMemoryState == OOMState::Reported) {
        AnnotateAndSetOutOfMemory(&mOutOfMemoryState, OOMState::Recovered);
      }
      if (mLargeAllocationFailureState == OOMState::Reported) {
        AnnotateAndSetOutOfMemory(&mLargeAllocationFailureState, OOMState::Recovered);
      }

      // Do any deferred finalization of native objects. Normally we do this
      // incrementally for an incremental GC, and immediately for a
      // non-incremental GC, on the basis that the type of GC reflects how
      // urgently resources should be destroyed. However under some circumstances
      // (such as in js::InternalCallOrConstruct) we can end up running a
      // non-incremental GC when there is a pending exception, and the finalizers
      // are not set up to handle that. In that case, just run them later, after
      // we've returned to the event loop.
      if (recordreplay::IsRecordingOrReplaying()) {
        // Cause deferred things to be finalized soon, at a consistent point in
        // the recording and replay, per the earlier registered trigger.
        recordreplay::ActivateTrigger(this);
      } else {
        bool finalizeIncrementally = JS::WasIncrementalGC(mJSRuntime) || JS_IsExceptionPending(aContext);
        FinalizeDeferredThings(finalizeIncrementally
                               ? CycleCollectedJSContext::FinalizeIncrementally
                               : CycleCollectedJSContext::FinalizeNow);
      }

      break;
    }
    default:
      MOZ_CRASH();
  }

  CustomGCCallback(aStatus);
}

void
CycleCollectedJSRuntime::OnOutOfMemory()
{
  AnnotateAndSetOutOfMemory(&mOutOfMemoryState, OOMState::Reporting);
  CustomOutOfMemoryCallback();
  AnnotateAndSetOutOfMemory(&mOutOfMemoryState, OOMState::Reported);
}

void
CycleCollectedJSRuntime::SetLargeAllocationFailure(OOMState aNewState)
{
  AnnotateAndSetOutOfMemory(&mLargeAllocationFailureState, aNewState);
}

void
CycleCollectedJSRuntime::PrepareWaitingZonesForGC()
{
  JSContext* cx = CycleCollectedJSContext::Get()->Context();
  if (mZonesWaitingForGC.Count() == 0) {
    JS::PrepareForFullGC(cx);
  } else {
    for (auto iter = mZonesWaitingForGC.Iter(); !iter.Done(); iter.Next()) {
      JS::PrepareZoneForGC(iter.Get()->GetKey());
    }
    mZonesWaitingForGC.Clear();
  }
}

void
CycleCollectedJSRuntime::EnvironmentPreparer::invoke(JS::HandleObject global,
                                                     js::ScriptEnvironmentPreparer::Closure& closure)
{
  MOZ_ASSERT(JS_IsGlobalObject(global));
  nsIGlobalObject* nativeGlobal = xpc::NativeGlobal(global);

  // Not much we can do if we simply don't have a usable global here...
  NS_ENSURE_TRUE_VOID(nativeGlobal && nativeGlobal->GetGlobalJSObject());

  AutoEntryScript aes(nativeGlobal, "JS-engine-initiated execution");

  MOZ_ASSERT(!JS_IsExceptionPending(aes.cx()));

  DebugOnly<bool> ok = closure(aes.cx());

  MOZ_ASSERT_IF(ok, !JS_IsExceptionPending(aes.cx()));

  // The AutoEntryScript will check for JS_IsExceptionPending on the
  // JSContext and report it as needed as it comes off the stack.
}

/* static */ CycleCollectedJSRuntime*
CycleCollectedJSRuntime::Get()
{
  auto context = CycleCollectedJSContext::Get();
  if (context) {
    return context->Runtime();
  }
  return nullptr;
}

#ifdef MOZ_JS_DEV_ERROR_INTERCEPTOR

namespace js {
extern void DumpValue(const JS::Value& val);
}

void
CycleCollectedJSRuntime::ErrorInterceptor::Shutdown(JSRuntime* rt)
{
  JS_SetErrorInterceptorCallback(rt, nullptr);
  mThrownError.reset();
}

/* virtual */ void
CycleCollectedJSRuntime::ErrorInterceptor::interceptError(JSContext* cx, const JS::Value& exn)
{
  if (mThrownError) {
    // We already have an error, we don't need anything more.
    return;
  }

  if (!nsContentUtils::ThreadsafeIsSystemCaller(cx)) {
    // We are only interested in chrome code.
    return;
  }

  const auto type = JS_GetErrorType(exn);
  if (!type) {
    // This is not one of the primitive error types.
    return;
  }

  switch (*type) {
    case JSExnType::JSEXN_REFERENCEERR:
    case JSExnType::JSEXN_SYNTAXERR:
    case JSExnType::JSEXN_TYPEERR:
      break;
    default:
      // Not one of the errors we are interested in.
      return;
  }

  // Now copy the details of the exception locally.
  // While copying the details of an exception could be expensive, in most runs,
  // this will be done at most once during the execution of the process, so the
  // total cost should be reasonable.
  JS::RootedValue value(cx, exn);

  ErrorDetails details;
  details.mType = *type;
  // If `exn` isn't an exception object, `ExtractErrorValues` could end up calling
  // `toString()`, which could in turn end up throwing an error. While this should
  // work, we want to avoid that complex use case.
  // Fortunately, we have already checked above that `exn` is an exception object,
  // so nothing such should happen.
  nsContentUtils::ExtractErrorValues(cx, value, details.mFilename, &details.mLine, &details.mColumn, details.mMessage);

  JS::UniqueChars buf = JS::FormatStackDump(cx, nullptr, /* showArgs = */ false, /* showLocals = */ false, /* showThisProps = */ false);
  CopyUTF8toUTF16(buf.get(), details.mStack);

  mThrownError.emplace(std::move(details));
}

void
CycleCollectedJSRuntime::ClearRecentDevError()
{
  mErrorInterceptor.mThrownError.reset();
}

bool
CycleCollectedJSRuntime::GetRecentDevError(JSContext*cx, JS::MutableHandle<JS::Value> error)
{
  if (!mErrorInterceptor.mThrownError) {
    return true;
  }

  // Create a copy of the exception.
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return false;
  }

  JS::RootedValue message(cx);
  JS::RootedValue filename(cx);
  JS::RootedValue stack(cx);
  if (!ToJSValue(cx, mErrorInterceptor.mThrownError->mMessage, &message) ||
      !ToJSValue(cx, mErrorInterceptor.mThrownError->mFilename, &filename) ||
      !ToJSValue(cx, mErrorInterceptor.mThrownError->mStack, &stack)) {
    return false;
  }

  // Build the object.
  const auto FLAGS = JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
  if (!JS_DefineProperty(cx, obj, "message", message, FLAGS) ||
      !JS_DefineProperty(cx, obj, "fileName", filename, FLAGS) ||
      !JS_DefineProperty(cx, obj, "lineNumber", mErrorInterceptor.mThrownError->mLine, FLAGS) ||
      !JS_DefineProperty(cx, obj, "stack", stack, FLAGS)) {
    return false;
  }

  // Pass the result.
  error.setObject(*obj);
  return true;
}
#endif // MOZ_JS_DEV_ERROR_INTERCEPTOR

#undef MOZ_JS_DEV_ERROR_INTERCEPTOR
