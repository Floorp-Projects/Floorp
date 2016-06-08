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
#include "mozilla/Move.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Snprintf.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimelineConsumers.h"
#include "mozilla/TimelineMarker.h"
#include "mozilla/DebuggerOnGCRunnable.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsprf.h"
#include "js/Debug.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"
#include "nsDOMJSUtils.h"
#include "nsJSUtils.h"
#include "nsWrapperCache.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#include "nsIException.h"
#include "nsIPlatformInfo.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

struct DeferredFinalizeFunctionHolder
{
  DeferredFinalizeFunction run;
  void* data;
};

class IncrementalFinalizeRunnable : public Runnable
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

// This is based on the logic in FixWeakMappingGrayBitsTracer::trace.
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
    // If nothing that could be held alive by this entry is marked gray, return.
    bool delegateMightNeedMarking = aKey && JS::GCThingIsMarkedGray(aKey);
    bool valueMightNeedMarking = aValue && JS::GCThingIsMarkedGray(aValue) &&
                                 aValue.kind() != JS::TraceKind::String;
    if (!delegateMightNeedMarking && !valueMightNeedMarking) {
      return;
    }

    if (!AddToCCKind(aKey.kind())) {
      aKey = nullptr;
    }

    if (delegateMightNeedMarking && aKey.is<JSObject>()) {
      JSObject* kdelegate = js::GetWeakmapKeyDelegate(&aKey.as<JSObject>());
      if (kdelegate && !JS::ObjectIsMarkedGray(kdelegate)) {
        if (JS::UnmarkGrayGCThingRecursively(aKey)) {
          mAnyMarked = true;
        }
      }
    }

    if (aValue && JS::GCThingIsMarkedGray(aValue) &&
        (!aKey || !JS::GCThingIsMarkedGray(aKey)) &&
        (!aMap || !JS::ObjectIsMarkedGray(aMap)) &&
        aValue.kind() != JS::TraceKind::Shape) {
      if (JS::UnmarkGrayGCThingRecursively(aValue)) {
        mAnyMarked = true;
      }
    }
  }

  bool mAnyMarked;
};

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
JSGCThingParticipant::Traverse(void* aPtr,
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
JSZoneParticipant::Traverse(void* aPtr, nsCycleCollectionTraversalCallback& aCb)
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
  }
  void onChild(const JS::GCCellPtr& aThing) override;
  nsCycleCollectionTraversalCallback& mCb;
};

void
TraversalTracer::onChild(const JS::GCCellPtr& aThing)
{
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
    if (aThing.is<JSObject>()) {
      mCb.NoteJSObject(&aThing.as<JSObject>());
    } else {
      mCb.NoteJSScript(&aThing.as<JSScript>());
    }
  } else if (aThing.is<js::Shape>()) {
    // The maximum depth of traversal when tracing a Shape is unbounded, due to
    // the parent pointers on the shape.
    JS_TraceShapeCycleCollectorChildren(this, aThing);
  } else if (aThing.is<js::ObjectGroup>()) {
    // The maximum depth of traversal when tracing an ObjectGroup is unbounded,
    // due to information attached to the groups which can lead other groups to
    // be traced.
    JS_TraceObjectGroupCycleCollectorChildren(this, aThing);
  } else if (!aThing.is<JSString>()) {
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
void JSObjectsTenuredCb(JSRuntime* aRuntime, void* aData)
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

CycleCollectedJSRuntime::CycleCollectedJSRuntime()
  : mGCThingCycleCollectorGlobal(sGCThingCycleCollectorGlobal)
  , mJSZoneCycleCollectorGlobal(sJSZoneCycleCollectorGlobal)
  , mJSRuntime(nullptr)
  , mPrevGCSliceCallback(nullptr)
  , mPrevGCNurseryCollectionCallback(nullptr)
  , mJSHolders(256)
  , mDoingStableStates(false)
  , mOutOfMemoryState(OOMState::OK)
  , mLargeAllocationFailureState(OOMState::OK)
{
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  mOwningThread = thread.forget().downcast<nsThread>().take();
  MOZ_RELEASE_ASSERT(mOwningThread);
}

CycleCollectedJSRuntime::~CycleCollectedJSRuntime()
{
  // If the allocation failed, here we are.
  if (!mJSRuntime) {
    return;
  }

  MOZ_ASSERT(!mDeferredFinalizerTable.Count());

  // Last chance to process any events.
  ProcessMetastableStateQueue(mBaseRecursionDepth);
  MOZ_ASSERT(mMetastableStateEvents.IsEmpty());

  ProcessStableStateQueue();
  MOZ_ASSERT(mStableStateEvents.IsEmpty());

  // Clear mPendingException first, since it might be cycle collected.
  mPendingException = nullptr;

  MOZ_ASSERT(mDebuggerPromiseMicroTaskQueue.empty());
  MOZ_ASSERT(mPromiseMicroTaskQueue.empty());

  JS_DestroyRuntime(mJSRuntime);
  mJSRuntime = nullptr;
  nsCycleCollector_forgetJSRuntime();

  mozilla::dom::DestroyScriptSettings();

  mOwningThread->SetScriptObserver(nullptr);
  NS_RELEASE(mOwningThread);
}

static void
MozCrashWarningReporter(JSContext*, const char*, JSErrorReport*)
{
  MOZ_CRASH("Why is someone touching JSAPI without an AutoJSAPI?");
}

nsresult
CycleCollectedJSRuntime::Initialize(JSRuntime* aParentRuntime,
                                    uint32_t aMaxBytes,
                                    uint32_t aMaxNurseryBytes)
{
  MOZ_ASSERT(!mJSRuntime);

  mOwningThread->SetScriptObserver(this);
  // The main thread has a base recursion depth of 0, workers of 1.
  mBaseRecursionDepth = RecursionDepth();

  mozilla::dom::InitScriptSettings();
  mJSRuntime = JS_NewRuntime(aMaxBytes, aMaxNurseryBytes, aParentRuntime);
  if (!mJSRuntime) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!JS_AddExtraGCRootsTracer(mJSRuntime, TraceBlackJS, this)) {
    MOZ_CRASH();
  }
  JS_SetGrayGCRootsTracer(mJSRuntime, TraceGrayJS, this);
  JS_SetGCCallback(mJSRuntime, GCCallback, this);
  mPrevGCSliceCallback = JS::SetGCSliceCallback(mJSRuntime, GCSliceCallback);

  if (NS_IsMainThread()) {
    // We would like to support all threads here, but the way timeline consumers
    // are set up currently, you can either add a marker for one specific
    // docshell, or for every consumer globally. We would like to add a marker
    // for every consumer observing anything on this thread, but that is not
    // currently possible. For now, add global markers only when we are on the
    // main thread, since the UI for this tracing data only displays data
    // relevant to the main-thread.
    mPrevGCNurseryCollectionCallback = JS::SetGCNurseryCollectionCallback(
      mJSRuntime, GCNurseryCollectionCallback);
  }

  JS_SetObjectsTenuredCallback(mJSRuntime, JSObjectsTenuredCb, this);
  JS::SetOutOfMemoryCallback(mJSRuntime, OutOfMemoryCallback, this);
  JS::SetLargeAllocationFailureCallback(mJSRuntime,
                                        LargeAllocationFailureCallback, this);
  JS_SetContextCallback(mJSRuntime, ContextCallback, this);
  JS_SetDestroyZoneCallback(mJSRuntime, XPCStringConvert::FreeZoneCache);
  JS_SetSweepZoneCallback(mJSRuntime, XPCStringConvert::ClearZoneCache);
  JS::SetBuildIdOp(mJSRuntime, GetBuildId);
  JS::SetWarningReporter(mJSRuntime, MozCrashWarningReporter);

  static js::DOMCallbacks DOMcallbacks = {
    InstanceClassHasProtoAtDepth
  };
  SetDOMCallbacks(mJSRuntime, &DOMcallbacks);
  js::SetScriptEnvironmentPreparer(mJSRuntime, &mEnvironmentPreparer);

#ifdef SPIDERMONKEY_PROMISE
  JS::SetEnqueuePromiseJobCallback(mJSRuntime, EnqueuePromiseJobCallback, this);
#endif // SPIDERMONKEY_PROMISE

  JS::dbg::SetDebuggerMallocSizeOf(mJSRuntime, moz_malloc_size_of);

  nsCycleCollector_registerJSRuntime(this);

  return NS_OK;
}

size_t
CycleCollectedJSRuntime::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;

  // We're deliberately not measuring anything hanging off the entries in
  // mJSHolders.
  n += mJSHolders.ShallowSizeOfExcludingThis(aMallocSizeOf);

  return n;
}

void
CycleCollectedJSRuntime::UnmarkSkippableJSHolders()
{
  for (auto iter = mJSHolders.Iter(); !iter.Done(); iter.Next()) {
    void* holder = iter.Key();
    nsScriptObjectTracer*& tracer = iter.Data();
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
        snprintf_literal(name, "JS Object (Function - %s)", fname.get());
      } else {
        snprintf_literal(name, "JS Object (Function)");
      }
    } else {
      snprintf_literal(name, "JS Object (%s)", clasp->name);
    }
  } else {
    snprintf_literal(name, "JS %s", JS::GCTraceKindToAscii(aThing.kind()));
  }

  // Disable printing global for objects while we figure out ObjShrink fallout.
  aCb.DescribeGCedNode(aIsMarked, name, compartmentAddress);
}

void
CycleCollectedJSRuntime::NoteGCThingJSChildren(JS::GCCellPtr aThing,
                                               nsCycleCollectionTraversalCallback& aCb) const
{
  MOZ_ASSERT(mJSRuntime);
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
  MOZ_ASSERT(mJSRuntime);

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
    void* holder = iter.Key();
    nsScriptObjectTracer*& tracer = iter.Data();

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
CycleCollectedJSRuntime::GCCallback(JSRuntime* aRuntime,
                                    JSGCStatus aStatus,
                                    void* aData)
{
  CycleCollectedJSRuntime* self = static_cast<CycleCollectedJSRuntime*>(aData);

  MOZ_ASSERT(aRuntime == self->Runtime());

  self->OnGC(aStatus);
}

/* static */ void
CycleCollectedJSRuntime::GCSliceCallback(JSRuntime* aRuntime,
                                         JS::GCProgress aProgress,
                                         const JS::GCDescription& aDesc)
{
  CycleCollectedJSRuntime* self = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(self->Runtime() == aRuntime);

  if (aProgress == JS::GC_CYCLE_END) {
    JS::gcreason::Reason reason = aDesc.reason_;
    NS_WARN_IF(NS_FAILED(DebuggerOnGCRunnable::Enqueue(aRuntime, aDesc)) &&
               reason != JS::gcreason::SHUTDOWN_CC &&
               reason != JS::gcreason::DESTROY_RUNTIME &&
               reason != JS::gcreason::XPCONNECT_SHUTDOWN);
  }

  if (self->mPrevGCSliceCallback) {
    self->mPrevGCSliceCallback(aRuntime, aProgress, aDesc);
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
    return UniquePtr<AbstractTimelineMarker>(Move(clone));
  }
};

/* static */ void
CycleCollectedJSRuntime::GCNurseryCollectionCallback(JSRuntime* aRuntime,
                                                     JS::GCNurseryProgress aProgress,
                                                     JS::gcreason::Reason aReason)
{
  CycleCollectedJSRuntime* self = CycleCollectedJSRuntime::Get();
  MOZ_ASSERT(self->Runtime() == aRuntime);
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  if (timelines && !timelines->IsEmpty()) {
    UniquePtr<AbstractTimelineMarker> abstractMarker(
      MakeUnique<MinorGCMarker>(aProgress, aReason));
    timelines->AddMarkerForAllObservedDocShells(abstractMarker);
  }

  if (self->mPrevGCNurseryCollectionCallback) {
    self->mPrevGCNurseryCollectionCallback(aRuntime, aProgress, aReason);
  }
}


/* static */ void
CycleCollectedJSRuntime::OutOfMemoryCallback(JSContext* aContext,
                                             void* aData)
{
  CycleCollectedJSRuntime* self = static_cast<CycleCollectedJSRuntime*>(aData);

  MOZ_ASSERT(JS_GetRuntime(aContext) == self->Runtime());

  self->OnOutOfMemory();
}

/* static */ void
CycleCollectedJSRuntime::LargeAllocationFailureCallback(void* aData)
{
  CycleCollectedJSRuntime* self = static_cast<CycleCollectedJSRuntime*>(aData);

  self->OnLargeAllocationFailure();
}

/* static */ bool
CycleCollectedJSRuntime::ContextCallback(JSContext* aContext,
                                         unsigned aOperation,
                                         void* aData)
{
  CycleCollectedJSRuntime* self = static_cast<CycleCollectedJSRuntime*>(aData);

  MOZ_ASSERT(JS_GetRuntime(aContext) == self->Runtime());

  return self->CustomContextCallback(aContext, aOperation);
}

class PromiseJobRunnable final : public Runnable
{
public:
  PromiseJobRunnable(JS::HandleObject aCallback, JS::HandleObject aAllocationSite)
    : mCallback(new PromiseJobCallback(aCallback, aAllocationSite, nullptr))
  {
  }

  virtual ~PromiseJobRunnable()
  {
  }

protected:
  NS_IMETHOD
  Run() override
  {
    nsIGlobalObject* global = xpc::NativeGlobal(mCallback->CallbackPreserveColor());
    if (global && !global->IsDying()) {
      mCallback->Call("promise callback");
    }
    return NS_OK;
  }

private:
  RefPtr<PromiseJobCallback> mCallback;
};

/* static */
bool
CycleCollectedJSRuntime::EnqueuePromiseJobCallback(JSContext* aCx,
                                                   JS::HandleObject aJob,
                                                   JS::HandleObject aAllocationSite,
                                                   void* aData)
{
  CycleCollectedJSRuntime* self = static_cast<CycleCollectedJSRuntime*>(aData);
  MOZ_ASSERT(JS_GetRuntime(aCx) == self->Runtime());
  MOZ_ASSERT(Get() == self);

  nsCOMPtr<nsIRunnable> runnable = new PromiseJobRunnable(aJob, aAllocationSite);
  self->DispatchToMicroTask(runnable);
  return true;
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
  MOZ_ASSERT(mJSRuntime);

  // NB: This is here just to preserve the existing XPConnect order. I doubt it
  // would hurt to do this after the JS holders.
  TraceAdditionalNativeGrayRoots(aTracer);

  for (auto iter = mJSHolders.Iter(); !iter.Done(); iter.Next()) {
    void* holder = iter.Key();
    nsScriptObjectTracer*& tracer = iter.Data();
    tracer->Trace(holder, JsGcTracer(), aTracer);
  }
}

void
CycleCollectedJSRuntime::AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer)
{
  MOZ_ASSERT(mJSRuntime);
  mJSHolders.Put(aHolder, aTracer);
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
  MOZ_ASSERT(mJSRuntime);

  nsScriptObjectTracer* tracer = mJSHolders.Get(aHolder);
  if (!tracer) {
    return;
  }
  tracer->Trace(aHolder, ClearJSHolder(), nullptr);
  mJSHolders.Remove(aHolder);
}

#ifdef DEBUG
bool
CycleCollectedJSRuntime::IsJSHolder(void* aHolder)
{
  MOZ_ASSERT(mJSRuntime);
  return mJSHolders.Get(aHolder, nullptr);
}

static void
AssertNoGcThing(JS::GCCellPtr aGCThing, const char* aName, void* aClosure)
{
  MOZ_ASSERT(!aGCThing);
}

void
CycleCollectedJSRuntime::AssertNoObjectsToTrace(void* aPossibleJSHolder)
{
  MOZ_ASSERT(mJSRuntime);

  nsScriptObjectTracer* tracer = mJSHolders.Get(aPossibleJSHolder);
  if (tracer) {
    tracer->Trace(aPossibleJSHolder, TraceCallbackFunc(AssertNoGcThing), nullptr);
  }
}
#endif

already_AddRefed<nsIException>
CycleCollectedJSRuntime::GetPendingException() const
{
  MOZ_ASSERT(mJSRuntime);

  nsCOMPtr<nsIException> out = mPendingException;
  return out.forget();
}

void
CycleCollectedJSRuntime::SetPendingException(nsIException* aException)
{
  MOZ_ASSERT(mJSRuntime);
  mPendingException = aException;
}

std::queue<nsCOMPtr<nsIRunnable>>&
CycleCollectedJSRuntime::GetPromiseMicroTaskQueue()
{
  MOZ_ASSERT(mJSRuntime);
  return mPromiseMicroTaskQueue;
}

std::queue<nsCOMPtr<nsIRunnable>>&
CycleCollectedJSRuntime::GetDebuggerPromiseMicroTaskQueue()
{
  MOZ_ASSERT(mJSRuntime);
  return mDebuggerPromiseMicroTaskQueue;
}

nsCycleCollectionParticipant*
CycleCollectedJSRuntime::GCThingParticipant()
{
  MOZ_ASSERT(mJSRuntime);
  return &mGCThingCycleCollectorGlobal;
}

nsCycleCollectionParticipant*
CycleCollectedJSRuntime::ZoneParticipant()
{
  MOZ_ASSERT(mJSRuntime);
  return &mJSZoneCycleCollectorGlobal;
}

nsresult
CycleCollectedJSRuntime::TraverseRoots(nsCycleCollectionNoteRootCallback& aCb)
{
  MOZ_ASSERT(mJSRuntime);

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
  MOZ_ASSERT(mJSRuntime);
  MOZ_ASSERT(!JS::IsIncrementalGCInProgress(mJSRuntime),
             "Don't call FixWeakMappingGrayBits during a GC.");
  FixWeakMappingGrayBitsTracer fixer(mJSRuntime);
  fixer.FixAll();
}

bool
CycleCollectedJSRuntime::AreGCGrayBitsValid() const
{
  MOZ_ASSERT(mJSRuntime);
  return js::AreGCGrayBitsValid(mJSRuntime);
}

void
CycleCollectedJSRuntime::GarbageCollect(uint32_t aReason) const
{
  MOZ_ASSERT(mJSRuntime);

  MOZ_ASSERT(aReason < JS::gcreason::NUM_REASONS);
  JS::gcreason::Reason gcreason = static_cast<JS::gcreason::Reason>(aReason);

  JS::PrepareForFullGC(mJSRuntime);
  JS::GCForReason(mJSRuntime, GC_NORMAL, gcreason);
}

void
CycleCollectedJSRuntime::JSObjectsTenured()
{
  MOZ_ASSERT(mJSRuntime);

  for (auto iter = mNurseryObjects.Iter(); !iter.Done(); iter.Next()) {
    nsWrapperCache* cache = iter.Get();
    JSObject* wrapper = cache->GetWrapperPreserveColor();
    MOZ_ASSERT(wrapper);
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
  MOZ_ASSERT(mJSRuntime);
  MOZ_ASSERT(aCache);
  MOZ_ASSERT(aCache->GetWrapperPreserveColor());
  MOZ_ASSERT(!JS::ObjectIsTenured(aCache->GetWrapperPreserveColor()));
  mNurseryObjects.InfallibleAppend(aCache);
}

void
CycleCollectedJSRuntime::NurseryWrapperPreserved(JSObject* aWrapper)
{
  MOZ_ASSERT(mJSRuntime);

  mPreservedNurseryObjects.InfallibleAppend(
    JS::PersistentRooted<JSObject*>(mJSRuntime, aWrapper));
}

void
CycleCollectedJSRuntime::DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                                          DeferredFinalizeFunction aFunc,
                                          void* aThing)
{
  MOZ_ASSERT(mJSRuntime);

  void* thingArray = nullptr;
  bool hadThingArray = mDeferredFinalizerTable.Get(aFunc, &thingArray);

  thingArray = aAppendFunc(thingArray, aThing);
  if (!hadThingArray) {
    mDeferredFinalizerTable.Put(aFunc, thingArray);
  }
}

void
CycleCollectedJSRuntime::DeferredFinalize(nsISupports* aSupports)
{
  MOZ_ASSERT(mJSRuntime);

  typedef DeferredFinalizerImpl<nsISupports> Impl;
  DeferredFinalize(Impl::AppendDeferredFinalizePointer, Impl::DeferredFinalize,
                   aSupports);
}

void
CycleCollectedJSRuntime::DumpJSHeap(FILE* aFile)
{
  MOZ_ASSERT(mJSRuntime);
  js::DumpHeap(Runtime(), aFile, js::CollectNurseryBeforeDump);
}

void
CycleCollectedJSRuntime::ProcessStableStateQueue()
{
  MOZ_ASSERT(mJSRuntime);
  MOZ_RELEASE_ASSERT(!mDoingStableStates);
  mDoingStableStates = true;

  for (uint32_t i = 0; i < mStableStateEvents.Length(); ++i) {
    nsCOMPtr<nsIRunnable> event = mStableStateEvents[i].forget();
    event->Run();
  }

  mStableStateEvents.Clear();
  mDoingStableStates = false;
}

void
CycleCollectedJSRuntime::ProcessMetastableStateQueue(uint32_t aRecursionDepth)
{
  MOZ_ASSERT(mJSRuntime);
  MOZ_RELEASE_ASSERT(!mDoingStableStates);
  mDoingStableStates = true;

  nsTArray<RunInMetastableStateData> localQueue = Move(mMetastableStateEvents);

  for (uint32_t i = 0; i < localQueue.Length(); ++i)
  {
    RunInMetastableStateData& data = localQueue[i];
    if (data.mRecursionDepth != aRecursionDepth) {
      continue;
    }

    {
      nsCOMPtr<nsIRunnable> runnable = data.mRunnable.forget();
      runnable->Run();
    }

    localQueue.RemoveElementAt(i--);
  }

  // If the queue has events in it now, they were added from something we called,
  // so they belong at the end of the queue.
  localQueue.AppendElements(mMetastableStateEvents);
  localQueue.SwapElements(mMetastableStateEvents);
  mDoingStableStates = false;
}

void
CycleCollectedJSRuntime::AfterProcessTask(uint32_t aRecursionDepth)
{
  MOZ_ASSERT(mJSRuntime);

  // See HTML 6.1.4.2 Processing model

  // Execute any events that were waiting for a microtask to complete.
  // This is not (yet) in the spec.
  ProcessMetastableStateQueue(aRecursionDepth);

  // Step 4.1: Execute microtasks.
  if (NS_IsMainThread()) {
    nsContentUtils::PerformMainThreadMicroTaskCheckpoint();
    Promise::PerformMicroTaskCheckpoint();
  } else {
    Promise::PerformWorkerMicroTaskCheckpoint();
  }

  // Step 4.2 Execute any events that were waiting for a stable state.
  ProcessStableStateQueue();
}

void
CycleCollectedJSRuntime::AfterProcessMicrotask()
{
  MOZ_ASSERT(mJSRuntime);
  AfterProcessMicrotask(RecursionDepth());
}

void
CycleCollectedJSRuntime::AfterProcessMicrotask(uint32_t aRecursionDepth)
{
  MOZ_ASSERT(mJSRuntime);

  // Between microtasks, execute any events that were waiting for a microtask
  // to complete.
  ProcessMetastableStateQueue(aRecursionDepth);
}

uint32_t
CycleCollectedJSRuntime::RecursionDepth()
{
  return mOwningThread->RecursionDepth();
}

void
CycleCollectedJSRuntime::RunInStableState(already_AddRefed<nsIRunnable>&& aRunnable)
{
  MOZ_ASSERT(mJSRuntime);
  mStableStateEvents.AppendElement(Move(aRunnable));
}

void
CycleCollectedJSRuntime::RunInMetastableState(already_AddRefed<nsIRunnable>&& aRunnable)
{
  MOZ_ASSERT(mJSRuntime);

  RunInMetastableStateData data;
  data.mRunnable = aRunnable;

  MOZ_ASSERT(mOwningThread);
  data.mRecursionDepth = RecursionDepth();

  // There must be an event running to get here.
#ifndef MOZ_WIDGET_COCOA
  MOZ_ASSERT(data.mRecursionDepth > mBaseRecursionDepth);
#endif

  mMetastableStateEvents.AppendElement(Move(data));
}

IncrementalFinalizeRunnable::IncrementalFinalizeRunnable(CycleCollectedJSRuntime* aRt,
                                                         DeferredFinalizerTable& aFinalizers)
  : mRuntime(aRt)
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

    TimeDuration sliceTime = TimeDuration::FromMilliseconds(SliceMillis);
    TimeStamp started = TimeStamp::Now();
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
CycleCollectedJSRuntime::FinalizeDeferredThings(DeferredFinalizeType aType)
{
  MOZ_ASSERT(mJSRuntime);

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

  if (aType == FinalizeIncrementally) {
    NS_DispatchToCurrentThread(mFinalizeRunnable);
  } else {
    mFinalizeRunnable->ReleaseNow(false);
    MOZ_ASSERT(!mFinalizeRunnable);
  }
}

void
CycleCollectedJSRuntime::AnnotateAndSetOutOfMemory(OOMState* aStatePtr,
                                                   OOMState aNewState)
{
  MOZ_ASSERT(mJSRuntime);

  *aStatePtr = aNewState;
#ifdef MOZ_CRASHREPORTER
  CrashReporter::AnnotateCrashReport(aStatePtr == &mOutOfMemoryState
                                     ? NS_LITERAL_CSTRING("JSOutOfMemory")
                                     : NS_LITERAL_CSTRING("JSLargeAllocationFailure"),
                                     aNewState == OOMState::Reporting
                                     ? NS_LITERAL_CSTRING("Reporting")
                                     : aNewState == OOMState::Reported
                                     ? NS_LITERAL_CSTRING("Reported")
                                     : NS_LITERAL_CSTRING("Recovered"));
#endif
}

void
CycleCollectedJSRuntime::OnGC(JSGCStatus aStatus)
{
  MOZ_ASSERT(mJSRuntime);

  switch (aStatus) {
    case JSGC_BEGIN:
      nsCycleCollector_prepareForGarbageCollection();
      mZonesWaitingForGC.Clear();
      break;
    case JSGC_END: {
#ifdef MOZ_CRASHREPORTER
      if (mOutOfMemoryState == OOMState::Reported) {
        AnnotateAndSetOutOfMemory(&mOutOfMemoryState, OOMState::Recovered);
      }
      if (mLargeAllocationFailureState == OOMState::Reported) {
        AnnotateAndSetOutOfMemory(&mLargeAllocationFailureState, OOMState::Recovered);
      }
#endif

      // Do any deferred finalization of native objects.
      FinalizeDeferredThings(JS::WasIncrementalGC(mJSRuntime) ? FinalizeIncrementally :
                                                                FinalizeNow);
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
  MOZ_ASSERT(mJSRuntime);

  AnnotateAndSetOutOfMemory(&mOutOfMemoryState, OOMState::Reporting);
  CustomOutOfMemoryCallback();
  AnnotateAndSetOutOfMemory(&mOutOfMemoryState, OOMState::Reported);
}

void
CycleCollectedJSRuntime::OnLargeAllocationFailure()
{
  MOZ_ASSERT(mJSRuntime);

  AnnotateAndSetOutOfMemory(&mLargeAllocationFailureState, OOMState::Reporting);
  CustomLargeAllocationFailureCallback();
  AnnotateAndSetOutOfMemory(&mLargeAllocationFailureState, OOMState::Reported);
}

void
CycleCollectedJSRuntime::PrepareWaitingZonesForGC()
{
  if (mZonesWaitingForGC.Count() == 0) {
    JS::PrepareForFullGC(Runtime());
  } else {
    for (auto iter = mZonesWaitingForGC.Iter(); !iter.Done(); iter.Next()) {
      JS::PrepareZoneForGC(iter.Get()->GetKey());
    }
    mZonesWaitingForGC.Clear();
  }
}

void
CycleCollectedJSRuntime::DispatchToMicroTask(nsIRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  mPromiseMicroTaskQueue.push(aRunnable);
}

void
CycleCollectedJSRuntime::EnvironmentPreparer::invoke(JS::HandleObject scope,
                                                     js::ScriptEnvironmentPreparer::Closure& closure)
{
  nsIGlobalObject* global = xpc::NativeGlobal(scope);

  // Not much we can do if we simply don't have a usable global here...
  NS_ENSURE_TRUE_VOID(global && global->GetGlobalJSObject());

  bool mainThread = NS_IsMainThread();
  JSContext* cx =
    mainThread ? nullptr : nsContentUtils::GetDefaultJSContextForThread();
  AutoEntryScript aes(global, "JS-engine-initiated execution", mainThread, cx);

  MOZ_ASSERT(!JS_IsExceptionPending(aes.cx()));

  DebugOnly<bool> ok = closure(aes.cx());

  MOZ_ASSERT_IF(ok, !JS_IsExceptionPending(aes.cx()));

  // The AutoEntryScript will check for JS_IsExceptionPending on the
  // JSContext and report it as needed as it comes off the stack.
}
