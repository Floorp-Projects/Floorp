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
#include "mozilla/MemoryReporting.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/DebuggerOnGCRunnable.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsprf.h"
#include "js/Debug.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"
#include "nsDOMJSUtils.h"
#include "nsJSUtils.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#include "nsIException.h"
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

class IncrementalFinalizeRunnable : public nsRunnable
{
  typedef nsAutoTArray<DeferredFinalizeFunctionHolder, 16> DeferredFinalizeArray;
  typedef CycleCollectedJSRuntime::DeferredFinalizerTable DeferredFinalizerTable;

  CycleCollectedJSRuntime* mRuntime;
  DeferredFinalizeArray mDeferredFinalizeFunctions;
  uint32_t mFinalizeFunctionToRun;
  bool mReleasing;

  static const PRTime SliceMillis = 5; /* ms */

  static PLDHashOperator
  DeferredFinalizerEnumerator(DeferredFinalizeFunction& aFunction,
                              void*& aData,
                              void* aClosure);

public:
  IncrementalFinalizeRunnable(CycleCollectedJSRuntime* aRt,
                              DeferredFinalizerTable& aFinalizerTable);
  virtual ~IncrementalFinalizeRunnable();

  void ReleaseNow(bool aLimited);

  NS_DECL_NSIRUNNABLE
};

} // namespace mozilla

static void
TraceWeakMappingChild(JS::CallbackTracer* aTrc, void** aThingp,
                      JS::TraceKind aKind);

struct NoteWeakMapChildrenTracer : public JS::CallbackTracer
{
  NoteWeakMapChildrenTracer(JSRuntime* aRt,
                            nsCycleCollectionNoteRootCallback& aCb)
    : JS::CallbackTracer(aRt, TraceWeakMappingChild), mCb(aCb),
      mTracedAny(false), mMap(nullptr), mKey(nullptr),
      mKeyDelegate(nullptr)
  {
  }
  nsCycleCollectionNoteRootCallback& mCb;
  bool mTracedAny;
  JSObject* mMap;
  JS::GCCellPtr mKey;
  JSObject* mKeyDelegate;
};

static void
TraceWeakMappingChild(JS::CallbackTracer* aTrc, void** aThingp,
                      JS::TraceKind aKind)
{
  MOZ_ASSERT(aTrc->hasCallback(TraceWeakMappingChild));
  NoteWeakMapChildrenTracer* tracer =
    static_cast<NoteWeakMapChildrenTracer*>(aTrc);
  JS::GCCellPtr thing(*aThingp, aKind);

  if (thing.isString()) {
    return;
  }

  if (!JS::GCThingIsMarkedGray(thing) && !tracer->mCb.WantAllTraces()) {
    return;
  }

  if (AddToCCKind(thing.kind())) {
    tracer->mCb.NoteWeakMapping(tracer->mMap, tracer->mKey,
                                tracer->mKeyDelegate, thing);
    tracer->mTracedAny = true;
  } else {
    JS_TraceChildren(aTrc, thing.asCell(), thing.kind());
  }
}

struct NoteWeakMapsTracer : public js::WeakMapTracer
{
  NoteWeakMapsTracer(JSRuntime* aRt, js::WeakMapTraceCallback aCb,
                     nsCycleCollectionNoteRootCallback& aCccb)
    : js::WeakMapTracer(aRt, aCb), mCb(aCccb), mChildTracer(aRt, aCccb)
  {
  }
  nsCycleCollectionNoteRootCallback& mCb;
  NoteWeakMapChildrenTracer mChildTracer;
};

static void
TraceWeakMapping(js::WeakMapTracer* aTrc, JSObject* aMap,
                 JS::GCCellPtr aKey, JS::GCCellPtr aValue)
{
  MOZ_ASSERT(aTrc->callback == TraceWeakMapping);
  NoteWeakMapsTracer* tracer = static_cast<NoteWeakMapsTracer*>(aTrc);

  // If nothing that could be held alive by this entry is marked gray, return.
  if ((!aKey || !JS::GCThingIsMarkedGray(aKey)) &&
      MOZ_LIKELY(!tracer->mCb.WantAllTraces())) {
    if (!aValue || !JS::GCThingIsMarkedGray(aValue) || aValue.isString()) {
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
  if (aKey.isObject()) {
    kdelegate = js::GetWeakmapKeyDelegate(aKey.toObject());
  }

  if (AddToCCKind(aValue.kind())) {
    tracer->mCb.NoteWeakMapping(aMap, aKey, kdelegate, aValue);
  } else {
    tracer->mChildTracer.mTracedAny = false;
    tracer->mChildTracer.mMap = aMap;
    tracer->mChildTracer.mKey = aKey;
    tracer->mChildTracer.mKeyDelegate = kdelegate;

    if (aValue.isString()) {
      JS_TraceChildren(&tracer->mChildTracer, aValue.asCell(), aValue.kind());
    }

    // The delegate could hold alive the key, so report something to the CC
    // if we haven't already.
    if (!tracer->mChildTracer.mTracedAny &&
        aKey && JS::GCThingIsMarkedGray(aKey) && kdelegate) {
      tracer->mCb.NoteWeakMapping(aMap, aKey, kdelegate, nullptr);
    }
  }
}

// This is based on the logic in TraceWeakMapping.
struct FixWeakMappingGrayBitsTracer : public js::WeakMapTracer
{
  explicit FixWeakMappingGrayBitsTracer(JSRuntime* aRt)
    : js::WeakMapTracer(aRt, FixWeakMappingGrayBits)
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

private:

  static void
  FixWeakMappingGrayBits(js::WeakMapTracer* aTrc, JSObject* aMap,
                         JS::GCCellPtr aKey, JS::GCCellPtr aValue)
  {
    FixWeakMappingGrayBitsTracer* tracer =
      static_cast<FixWeakMappingGrayBitsTracer*>(aTrc);

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

    if (delegateMightNeedMarking && aKey.isObject()) {
      JSObject* kdelegate = js::GetWeakmapKeyDelegate(aKey.toObject());
      if (kdelegate && !JS::ObjectIsMarkedGray(kdelegate)) {
        if (JS::UnmarkGrayGCThingRecursively(aKey)) {
          tracer->mAnyMarked = true;
        }
      }
    }

    if (aValue && JS::GCThingIsMarkedGray(aValue) &&
        (!aKey || !JS::GCThingIsMarkedGray(aKey)) &&
        (!aMap || !JS::ObjectIsMarkedGray(aMap)) &&
        aValue.kind() != JS::TraceKind::Shape) {
      if (JS::UnmarkGrayGCThingRecursively(aValue)) {
        tracer->mAnyMarked = true;
      }
    }
  }

  bool mAnyMarked;
};

struct Closure
{
  explicit Closure(nsCycleCollectionNoteRootCallback* aCb)
    : mCycleCollectionEnabled(true), mCb(aCb)
  {
  }

  bool mCycleCollectionEnabled;
  nsCycleCollectionNoteRootCallback* mCb;
};

static void
CheckParticipatesInCycleCollection(JS::GCCellPtr aThing, const char* aName,
                                   void* aClosure)
{
  Closure* closure = static_cast<Closure*>(aClosure);

  if (closure->mCycleCollectionEnabled) {
    return;
  }

  if (AddToCCKind(aThing.kind()) && JS::GCThingIsMarkedGray(aThing)) {
    closure->mCycleCollectionEnabled = true;
  }
}

static PLDHashOperator
NoteJSHolder(void* aHolder, nsScriptObjectTracer*& aTracer, void* aArg)
{
  Closure* closure = static_cast<Closure*>(aArg);

  bool noteRoot;
  if (MOZ_UNLIKELY(closure->mCb->WantAllTraces())) {
    noteRoot = true;
  } else {
    closure->mCycleCollectionEnabled = false;
    aTracer->Trace(aHolder,
                   TraceCallbackFunc(CheckParticipatesInCycleCollection),
                   closure);
    noteRoot = closure->mCycleCollectionEnabled;
  }

  if (noteRoot) {
    closure->mCb->NoteNativeRoot(aHolder, aTracer);
  }

  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
JSGCThingParticipant::Traverse(void* aPtr,
                               nsCycleCollectionTraversalCallback& aCb)
{
  auto runtime = reinterpret_cast<CycleCollectedJSRuntime*>(
    reinterpret_cast<char*>(this) - offsetof(CycleCollectedJSRuntime,
                                             mGCThingCycleCollectorGlobal));

  JS::GCCellPtr cellPtr(aPtr, js::GCThingTraceKind(aPtr));
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

static void
NoteJSChildTracerShim(JS::CallbackTracer* aTrc, void** aThingp,
                      JS::TraceKind aTraceKind);

struct TraversalTracer : public JS::CallbackTracer
{
  TraversalTracer(JSRuntime* aRt, nsCycleCollectionTraversalCallback& aCb)
    : JS::CallbackTracer(aRt, NoteJSChildTracerShim, DoNotTraceWeakMaps),
      mCb(aCb)
  {
  }
  nsCycleCollectionTraversalCallback& mCb;
};

static void
NoteJSChild(JS::CallbackTracer* aTrc, JS::GCCellPtr aThing)
{
  TraversalTracer* tracer = static_cast<TraversalTracer*>(aTrc);

  // Don't traverse non-gray objects, unless we want all traces.
  if (!JS::GCThingIsMarkedGray(aThing) && !tracer->mCb.WantAllTraces()) {
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
    if (MOZ_UNLIKELY(tracer->mCb.WantDebugInfo())) {
      char buffer[200];
      tracer->getTracingEdgeName(buffer, sizeof(buffer));
      tracer->mCb.NoteNextEdgeName(buffer);
    }
    if (aThing.isObject()) {
      tracer->mCb.NoteJSObject(aThing.toObject());
    } else {
      tracer->mCb.NoteJSScript(aThing.toScript());
    }
  } else if (aThing.isShape()) {
    // The maximum depth of traversal when tracing a Shape is unbounded, due to
    // the parent pointers on the shape.
    JS_TraceShapeCycleCollectorChildren(aTrc, aThing);
  } else if (aThing.isObjectGroup()) {
    // The maximum depth of traversal when tracing an ObjectGroup is unbounded,
    // due to information attached to the groups which can lead other groups to
    // be traced.
    JS_TraceObjectGroupCycleCollectorChildren(aTrc, aThing);
  } else if (!aThing.isString()) {
    JS_TraceChildren(aTrc, aThing.asCell(), aThing.kind());
  }
}

static void
NoteJSChildTracerShim(JS::CallbackTracer* aTrc, void** aThingp,
                      JS::TraceKind aTraceKind)
{
  JS::GCCellPtr thing(*aThingp, aTraceKind);
  NoteJSChild(aTrc, thing);
}

static void
NoteJSChildGrayWrapperShim(void* aData, JS::GCCellPtr aThing)
{
  TraversalTracer* trc = static_cast<TraversalTracer*>(aData);
  NoteJSChild(trc, aThing);
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

CycleCollectedJSRuntime::CycleCollectedJSRuntime(JSRuntime* aParentRuntime,
                                                 uint32_t aMaxBytes,
                                                 uint32_t aMaxNurseryBytes)
  : mGCThingCycleCollectorGlobal(sGCThingCycleCollectorGlobal)
  , mJSZoneCycleCollectorGlobal(sJSZoneCycleCollectorGlobal)
  , mJSRuntime(nullptr)
  , mPrevGCSliceCallback(nullptr)
  , mJSHolders(256)
  , mOutOfMemoryState(OOMState::OK)
  , mLargeAllocationFailureState(OOMState::OK)
{
  mozilla::dom::InitScriptSettings();

  mJSRuntime = JS_NewRuntime(aMaxBytes, aMaxNurseryBytes, aParentRuntime);
  if (!mJSRuntime) {
    MOZ_CRASH();
  }

  if (!JS_AddExtraGCRootsTracer(mJSRuntime, TraceBlackJS, this)) {
    MOZ_CRASH();
  }
  JS_SetGrayGCRootsTracer(mJSRuntime, TraceGrayJS, this);
  JS_SetGCCallback(mJSRuntime, GCCallback, this);
  mPrevGCSliceCallback = JS::SetGCSliceCallback(mJSRuntime, GCSliceCallback);
  JS::SetOutOfMemoryCallback(mJSRuntime, OutOfMemoryCallback, this);
  JS::SetLargeAllocationFailureCallback(mJSRuntime,
                                        LargeAllocationFailureCallback, this);
  JS_SetContextCallback(mJSRuntime, ContextCallback, this);
  JS_SetDestroyZoneCallback(mJSRuntime, XPCStringConvert::FreeZoneCache);
  JS_SetSweepZoneCallback(mJSRuntime, XPCStringConvert::ClearZoneCache);

  static js::DOMCallbacks DOMcallbacks = {
    InstanceClassHasProtoAtDepth
  };
  SetDOMCallbacks(mJSRuntime, &DOMcallbacks);

  JS::dbg::SetDebuggerMallocSizeOf(mJSRuntime, moz_malloc_size_of);

  nsCycleCollector_registerJSRuntime(this);
}

CycleCollectedJSRuntime::~CycleCollectedJSRuntime()
{
  MOZ_ASSERT(mJSRuntime);
  MOZ_ASSERT(!mDeferredFinalizerTable.Count());

  // Clear mPendingException first, since it might be cycle collected.
  mPendingException = nullptr;

  JS_DestroyRuntime(mJSRuntime);
  mJSRuntime = nullptr;
  nsCycleCollector_forgetJSRuntime();

  mozilla::dom::DestroyScriptSettings();
}

size_t
CycleCollectedJSRuntime::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;

  // nullptr for the second arg;  we're not measuring anything hanging off the
  // entries in mJSHolders.
  n += mJSHolders.SizeOfExcludingThis(nullptr, aMallocSizeOf);

  return n;
}

static PLDHashOperator
UnmarkJSHolder(void* aHolder, nsScriptObjectTracer*& aTracer, void* aArg)
{
  aTracer->CanSkip(aHolder, true);
  return PL_DHASH_NEXT;
}

void
CycleCollectedJSRuntime::UnmarkSkippableJSHolders()
{
  mJSHolders.Enumerate(UnmarkJSHolder, nullptr);
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
  if (aThing.isObject()) {
    JSObject* obj = aThing.toObject();
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
        JS_snprintf(name, sizeof(name),
                    "JS Object (Function - %s)", fname.get());
      } else {
        JS_snprintf(name, sizeof(name), "JS Object (Function)");
      }
    } else {
      JS_snprintf(name, sizeof(name), "JS Object (%s)", clasp->name);
    }
  } else {
    JS_snprintf(name, sizeof(name), "JS %s", JS::GCTraceKindToAscii(aThing.kind()));
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
  JS_TraceChildren(&trc, aThing.asCell(), aThing.kind());
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

  if (aThing.isObject()) {
    JSObject* obj = aThing.toObject();
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

  MOZ_ASSERT(aThing.isObject());
  closure->self->TraverseGCThing(CycleCollectedJSRuntime::TRAVERSE_CPP,
                                 aThing, closure->cb);
}

void
CycleCollectedJSRuntime::TraverseNativeRoots(nsCycleCollectionNoteRootCallback& aCb)
{
  // NB: This is here just to preserve the existing XPConnect order. I doubt it
  // would hurt to do this after the JS holders.
  TraverseAdditionalNativeRoots(aCb);

  Closure closure(&aCb);
  mJSHolders.Enumerate(NoteJSHolder, &closure);
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
    NS_WARN_IF(NS_FAILED(DebuggerOnGCRunnable::Enqueue(aRuntime, aDesc)));
  }

  if (self->mPrevGCSliceCallback) {
    self->mPrevGCSliceCallback(aRuntime, aProgress, aDesc);
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

struct JsGcTracer : public TraceCallbacks
{
  virtual void Trace(JS::Heap<JS::Value>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS_CallValueTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<jsid>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS_CallIdTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSObject*>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS_CallObjectTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::TenuredHeap<JSObject*>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS_CallTenuredObjectTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSString*>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS_CallStringTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSScript*>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS_CallScriptTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSFunction*>* aPtr, const char* aName,
                     void* aClosure) const override
  {
    JS_CallFunctionTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
};

static PLDHashOperator
TraceJSHolder(void* aHolder, nsScriptObjectTracer*& aTracer, void* aArg)
{
  aTracer->Trace(aHolder, JsGcTracer(), aArg);

  return PL_DHASH_NEXT;
}

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

  mJSHolders.Enumerate(TraceJSHolder, aTracer);
}

void
CycleCollectedJSRuntime::AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer)
{
  mJSHolders.Put(aHolder, aTracer);
}

struct ClearJSHolder : TraceCallbacks
{
  virtual void Trace(JS::Heap<JS::Value>* aPtr, const char*, void*) const override
  {
    *aPtr = JSVAL_VOID;
  }

  virtual void Trace(JS::Heap<jsid>* aPtr, const char*, void*) const override
  {
    *aPtr = JSID_VOID;
  }

  virtual void Trace(JS::Heap<JSObject*>* aPtr, const char*, void*) const override
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
  nsScriptObjectTracer* tracer = mJSHolders.Get(aPossibleJSHolder);
  if (tracer) {
    tracer->Trace(aPossibleJSHolder, TraceCallbackFunc(AssertNoGcThing), nullptr);
  }
}
#endif

already_AddRefed<nsIException>
CycleCollectedJSRuntime::GetPendingException() const
{
  nsCOMPtr<nsIException> out = mPendingException;
  return out.forget();
}

void
CycleCollectedJSRuntime::SetPendingException(nsIException* aException)
{
  mPendingException = aException;
}

std::queue<nsCOMPtr<nsIRunnable>>&
CycleCollectedJSRuntime::GetPromiseMicroTaskQueue()
{
  return mPromiseMicroTaskQueue;
}

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

  NoteWeakMapsTracer trc(mJSRuntime, TraceWeakMapping, aCb);
  js::TraceWeakMaps(&trc);

  return NS_OK;
}

/*
 * Return true if there exists a JSContext with a default global whose current
 * inner is gray. The intent is to look for JS Object windows. We don't merge
 * system compartments, so we don't use them to trigger merging CCs.
 */
bool
CycleCollectedJSRuntime::UsefulToMergeZones() const
{
  if (!NS_IsMainThread()) {
    return false;
  }

  JSContext* iter = nullptr;
  JSContext* cx;
  JSAutoRequest ar(nsContentUtils::GetSafeJSContext());
  while ((cx = JS_ContextIterator(mJSRuntime, &iter))) {
    // Skip anything without an nsIScriptContext.
    nsIScriptContext* scx = GetScriptContextFromJSContext(cx);
    JS::RootedObject obj(cx, scx ? scx->GetWindowProxyPreserveColor() : nullptr);
    if (!obj) {
      continue;
    }
    MOZ_ASSERT(js::IsOuterObject(obj));
    // Grab the inner from the outer.
    obj = JS_ObjectToInnerObject(cx, obj);
    MOZ_ASSERT(JS_IsGlobalObject(obj));
    if (JS::ObjectIsMarkedGray(obj) &&
        !js::IsSystemCompartment(js::GetObjectCompartment(obj))) {
      return true;
    }
  }
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

  JS::PrepareForFullGC(mJSRuntime);
  JS::GCForReason(mJSRuntime, GC_NORMAL, gcreason);
}

void
CycleCollectedJSRuntime::DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                                          DeferredFinalizeFunction aFunc,
                                          void* aThing)
{
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
  typedef DeferredFinalizerImpl<nsISupports> Impl;
  DeferredFinalize(Impl::AppendDeferredFinalizePointer, Impl::DeferredFinalize,
                   aSupports);
}

void
CycleCollectedJSRuntime::DumpJSHeap(FILE* aFile)
{
  js::DumpHeap(Runtime(), aFile, js::CollectNurseryBeforeDump);
}


/* static */ PLDHashOperator
IncrementalFinalizeRunnable::DeferredFinalizerEnumerator(DeferredFinalizeFunction& aFunction,
                                                         void*& aData,
                                                         void* aClosure)
{
  DeferredFinalizeArray* array = static_cast<DeferredFinalizeArray*>(aClosure);

  DeferredFinalizeFunctionHolder* function = array->AppendElement();
  function->run = aFunction;
  function->data = aData;

  return PL_DHASH_REMOVE;
}

IncrementalFinalizeRunnable::IncrementalFinalizeRunnable(CycleCollectedJSRuntime* aRt,
                                                         DeferredFinalizerTable& aFinalizers)
  : mRuntime(aRt)
  , mFinalizeFunctionToRun(0)
  , mReleasing(false)
{
  aFinalizers.Enumerate(DeferredFinalizerEnumerator, &mDeferredFinalizeFunctions);
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
        function.run(UINT32_MAX, function.data);
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
  switch (aStatus) {
    case JSGC_BEGIN:
      nsCycleCollector_prepareForGarbageCollection();
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
  AnnotateAndSetOutOfMemory(&mOutOfMemoryState, OOMState::Reporting);
  CustomOutOfMemoryCallback();
  AnnotateAndSetOutOfMemory(&mOutOfMemoryState, OOMState::Reported);
}

void
CycleCollectedJSRuntime::OnLargeAllocationFailure()
{
  AnnotateAndSetOutOfMemory(&mLargeAllocationFailureState, OOMState::Reporting);
  CustomLargeAllocationFailureCallback();
  AnnotateAndSetOutOfMemory(&mLargeAllocationFailureState, OOMState::Reported);
}

