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
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsprf.h"
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
  nsTArray<nsISupports*> mSupports;
  DeferredFinalizeArray mDeferredFinalizeFunctions;
  uint32_t mFinalizeFunctionToRun;

  static const PRTime SliceMillis = 10; /* ms */

  static PLDHashOperator
  DeferredFinalizerEnumerator(DeferredFinalizeFunction& aFunction,
                              void*& aData,
                              void* aClosure);

public:
  IncrementalFinalizeRunnable(CycleCollectedJSRuntime* aRt,
                              nsTArray<nsISupports*>& aMSupports,
                              DeferredFinalizerTable& aFinalizerTable);
  virtual ~IncrementalFinalizeRunnable();

  void ReleaseNow(bool aLimited);

  NS_DECL_NSIRUNNABLE
};

} // namespace mozilla

inline bool
AddToCCKind(JSGCTraceKind aKind)
{
  return aKind == JSTRACE_OBJECT || aKind == JSTRACE_SCRIPT;
}

static void
TraceWeakMappingChild(JSTracer* aTrc, void** aThingp, JSGCTraceKind aKind);

struct NoteWeakMapChildrenTracer : public JSTracer
{
  NoteWeakMapChildrenTracer(JSRuntime* aRt,
                            nsCycleCollectionNoteRootCallback& aCb)
    : JSTracer(aRt, TraceWeakMappingChild), mCb(aCb)
  {
  }
  nsCycleCollectionNoteRootCallback& mCb;
  bool mTracedAny;
  JSObject* mMap;
  void* mKey;
  void* mKeyDelegate;
};

static void
TraceWeakMappingChild(JSTracer* aTrc, void** aThingp, JSGCTraceKind aKind)
{
  MOZ_ASSERT(aTrc->callback == TraceWeakMappingChild);
  void* thing = *aThingp;
  NoteWeakMapChildrenTracer* tracer =
    static_cast<NoteWeakMapChildrenTracer*>(aTrc);

  if (aKind == JSTRACE_STRING) {
    return;
  }

  if (!xpc_IsGrayGCThing(thing) && !tracer->mCb.WantAllTraces()) {
    return;
  }

  if (AddToCCKind(aKind)) {
    tracer->mCb.NoteWeakMapping(tracer->mMap, tracer->mKey, tracer->mKeyDelegate, thing);
    tracer->mTracedAny = true;
  } else {
    JS_TraceChildren(aTrc, thing, aKind);
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
                 void* aKey, JSGCTraceKind aKeyKind,
                 void* aValue, JSGCTraceKind aValueKind)
{
  MOZ_ASSERT(aTrc->callback == TraceWeakMapping);
  NoteWeakMapsTracer* tracer = static_cast<NoteWeakMapsTracer*>(aTrc);

  // If nothing that could be held alive by this entry is marked gray, return.
  if ((!aKey || !xpc_IsGrayGCThing(aKey)) &&
      MOZ_LIKELY(!tracer->mCb.WantAllTraces())) {
    if (!aValue || !xpc_IsGrayGCThing(aValue) ||
        aValueKind == JSTRACE_STRING) {
      return;
    }
  }

  // The cycle collector can only properly reason about weak maps if it can
  // reason about the liveness of their keys, which in turn requires that
  // the key can be represented in the cycle collector graph.  All existing
  // uses of weak maps use either objects or scripts as keys, which are okay.
  MOZ_ASSERT(AddToCCKind(aKeyKind));

  // As an emergency fallback for non-debug builds, if the key is not
  // representable in the cycle collector graph, we treat it as marked.  This
  // can cause leaks, but is preferable to ignoring the binding, which could
  // cause the cycle collector to free live objects.
  if (!AddToCCKind(aKeyKind)) {
    aKey = nullptr;
  }

  JSObject* kdelegate = nullptr;
  if (aKey && aKeyKind == JSTRACE_OBJECT) {
    kdelegate = js::GetWeakmapKeyDelegate((JSObject*)aKey);
  }

  if (AddToCCKind(aValueKind)) {
    tracer->mCb.NoteWeakMapping(aMap, aKey, kdelegate, aValue);
  } else {
    tracer->mChildTracer.mTracedAny = false;
    tracer->mChildTracer.mMap = aMap;
    tracer->mChildTracer.mKey = aKey;
    tracer->mChildTracer.mKeyDelegate = kdelegate;

    if (aValue && aValueKind != JSTRACE_STRING) {
      JS_TraceChildren(&tracer->mChildTracer, aValue, aValueKind);
    }

    // The delegate could hold alive the key, so report something to the CC
    // if we haven't already.
    if (!tracer->mChildTracer.mTracedAny &&
        aKey && xpc_IsGrayGCThing(aKey) && kdelegate) {
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
                         void* aKey, JSGCTraceKind aKeyKind,
                         void* aValue, JSGCTraceKind aValueKind)
  {
    FixWeakMappingGrayBitsTracer* tracer =
      static_cast<FixWeakMappingGrayBitsTracer*>(aTrc);

    // If nothing that could be held alive by this entry is marked gray, return.
    bool delegateMightNeedMarking = aKey && xpc_IsGrayGCThing(aKey);
    bool valueMightNeedMarking = aValue && xpc_IsGrayGCThing(aValue) &&
                                 aValueKind != JSTRACE_STRING;
    if (!delegateMightNeedMarking && !valueMightNeedMarking) {
      return;
    }

    if (!AddToCCKind(aKeyKind)) {
      aKey = nullptr;
    }

    if (delegateMightNeedMarking && aKeyKind == JSTRACE_OBJECT) {
      JSObject* kdelegate = js::GetWeakmapKeyDelegate((JSObject*)aKey);
      if (kdelegate && !xpc_IsGrayGCThing(kdelegate)) {
        if (JS::UnmarkGrayGCThingRecursively(aKey, JSTRACE_OBJECT)) {
          tracer->mAnyMarked = true;
        }
      }
    }

    if (aValue && xpc_IsGrayGCThing(aValue) &&
        (!aKey || !xpc_IsGrayGCThing(aKey)) &&
        (!aMap || !xpc_IsGrayGCThing(aMap)) &&
        aValueKind != JSTRACE_SHAPE) {
      if (JS::UnmarkGrayGCThingRecursively(aValue, aValueKind)) {
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
CheckParticipatesInCycleCollection(void* aThing, const char* aName, void* aClosure)
{
  Closure* closure = static_cast<Closure*>(aClosure);

  if (closure->mCycleCollectionEnabled) {
    return;
  }

  if (AddToCCKind(js::GCThingTraceKind(aThing)) &&
      xpc_IsGrayGCThing(aThing)) {
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
  CycleCollectedJSRuntime* runtime = reinterpret_cast<CycleCollectedJSRuntime*>
    (reinterpret_cast<char*>(this) -
     offsetof(CycleCollectedJSRuntime, mGCThingCycleCollectorGlobal));

  runtime->TraverseGCThing(CycleCollectedJSRuntime::TRAVERSE_FULL,
                           aPtr, js::GCThingTraceKind(aPtr), aCb);
  return NS_OK;
}

// NB: This is only used to initialize the participant in
// CycleCollectedJSRuntime. It should never be used directly.
static JSGCThingParticipant sGCThingCycleCollectorGlobal;

NS_IMETHODIMP
JSZoneParticipant::Traverse(void* aPtr, nsCycleCollectionTraversalCallback& aCb)
{
  CycleCollectedJSRuntime* runtime = reinterpret_cast<CycleCollectedJSRuntime*>
    (reinterpret_cast<char*>(this) -
     offsetof(CycleCollectedJSRuntime, mJSZoneCycleCollectorGlobal));

  MOZ_ASSERT(!aCb.WantAllTraces());
  JS::Zone* zone = static_cast<JS::Zone*>(aPtr);

  runtime->TraverseZone(zone, aCb);
  return NS_OK;
}

static void
NoteJSChildTracerShim(JSTracer* aTrc, void** aThingp, JSGCTraceKind aTraceKind);

struct TraversalTracer : public JSTracer
{
  TraversalTracer(JSRuntime* aRt, nsCycleCollectionTraversalCallback& aCb)
    : JSTracer(aRt, NoteJSChildTracerShim, DoNotTraceWeakMaps), mCb(aCb)
  {
  }
  nsCycleCollectionTraversalCallback& mCb;
};

static void
NoteJSChild(JSTracer* aTrc, void* aThing, JSGCTraceKind aTraceKind)
{
  TraversalTracer* tracer = static_cast<TraversalTracer*>(aTrc);

  // Don't traverse non-gray objects, unless we want all traces.
  if (!xpc_IsGrayGCThing(aThing) && !tracer->mCb.WantAllTraces()) {
    return;
  }

  /*
   * This function needs to be careful to avoid stack overflow. Normally, when
   * AddToCCKind is true, the recursion terminates immediately as we just add
   * |thing| to the CC graph. So overflow is only possible when there are long
   * chains of non-AddToCCKind GC things. Currently, this only can happen via
   * shape parent pointers. The special JSTRACE_SHAPE case below handles
   * parent pointers iteratively, rather than recursively, to avoid overflow.
   */
  if (AddToCCKind(aTraceKind)) {
    if (MOZ_UNLIKELY(tracer->mCb.WantDebugInfo())) {
      // based on DumpNotify in jsapi.cpp
      if (tracer->debugPrinter()) {
        char buffer[200];
        tracer->debugPrinter()(aTrc, buffer, sizeof(buffer));
        tracer->mCb.NoteNextEdgeName(buffer);
      } else if (tracer->debugPrintIndex() != (size_t)-1) {
        char buffer[200];
        JS_snprintf(buffer, sizeof(buffer), "%s[%lu]",
                    static_cast<const char*>(tracer->debugPrintArg()),
                    tracer->debugPrintIndex());
        tracer->mCb.NoteNextEdgeName(buffer);
      } else {
        tracer->mCb.NoteNextEdgeName(static_cast<const char*>(tracer->debugPrintArg()));
      }
    }
    tracer->mCb.NoteJSChild(aThing);
  } else if (aTraceKind == JSTRACE_SHAPE) {
    JS_TraceShapeCycleCollectorChildren(aTrc, aThing);
  } else if (aTraceKind != JSTRACE_STRING) {
    JS_TraceChildren(aTrc, aThing, aTraceKind);
  }
}

static void
NoteJSChildTracerShim(JSTracer* aTrc, void** aThingp, JSGCTraceKind aTraceKind)
{
  NoteJSChild(aTrc, *aThingp, aTraceKind);
}

static void
NoteJSChildGrayWrapperShim(void* aData, void* aThing)
{
  TraversalTracer* trc = static_cast<TraversalTracer*>(aData);
  NoteJSChild(trc, aThing, js::GCThingTraceKind(aThing));
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
  , mJSHolders(512)
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
  JS::SetOutOfMemoryCallback(mJSRuntime, OutOfMemoryCallback, this);
  JS::SetLargeAllocationFailureCallback(mJSRuntime, LargeAllocationFailureCallback, this);
  JS_SetContextCallback(mJSRuntime, ContextCallback, this);
  JS_SetDestroyZoneCallback(mJSRuntime, XPCStringConvert::FreeZoneCache);
  JS_SetSweepZoneCallback(mJSRuntime, XPCStringConvert::ClearZoneCache);

  static js::DOMCallbacks DOMcallbacks = {
    InstanceClassHasProtoAtDepth
  };
  SetDOMCallbacks(mJSRuntime, &DOMcallbacks);

  nsCycleCollector_registerJSRuntime(this);
}

CycleCollectedJSRuntime::~CycleCollectedJSRuntime()
{
  MOZ_ASSERT(mJSRuntime);
  MOZ_ASSERT(!mDeferredFinalizerTable.Count());
  MOZ_ASSERT(!mDeferredSupports.Length());

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
CycleCollectedJSRuntime::DescribeGCThing(bool aIsMarked, void* aThing,
                                         JSGCTraceKind aTraceKind,
                                         nsCycleCollectionTraversalCallback& aCb) const
{
  if (!aCb.WantDebugInfo()) {
    aCb.DescribeGCedNode(aIsMarked, "JS Object");
    return;
  }

  char name[72];
  uint64_t compartmentAddress = 0;
  if (aTraceKind == JSTRACE_OBJECT) {
    JSObject* obj = static_cast<JSObject*>(aThing);
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
      JS_snprintf(name, sizeof(name), "JS Object (%s)",
                  clasp->name);
    }
  } else {
    static const char trace_types[][11] = {
      "Object",
      "String",
      "Symbol",
      "Script",
      "LazyScript",
      "IonCode",
      "Shape",
      "BaseShape",
      "TypeObject",
    };
    static_assert(MOZ_ARRAY_LENGTH(trace_types) == JSTRACE_LAST + 1,
                  "JSTRACE_LAST enum must match trace_types count.");
    JS_snprintf(name, sizeof(name), "JS %s", trace_types[aTraceKind]);
  }

  // Disable printing global for objects while we figure out ObjShrink fallout.
  aCb.DescribeGCedNode(aIsMarked, name, compartmentAddress);
}

void
CycleCollectedJSRuntime::NoteGCThingJSChildren(void* aThing,
                                               JSGCTraceKind aTraceKind,
                                               nsCycleCollectionTraversalCallback& aCb) const
{
  MOZ_ASSERT(mJSRuntime);
  TraversalTracer trc(mJSRuntime, aCb);
  JS_TraceChildren(&trc, aThing, aTraceKind);
}

void
CycleCollectedJSRuntime::NoteGCThingXPCOMChildren(const js::Class* aClasp, JSObject* aObj,
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
      if (domClass->mDOMObjectIsISupports) {
        aCb.NoteXPCOMChild(UnwrapDOMObject<nsISupports>(aObj));
      } else if (domClass->mParticipant) {
        aCb.NoteNativeChild(UnwrapDOMObject<void>(aObj),
                            domClass->mParticipant);
      }
    }
  }
}

void
CycleCollectedJSRuntime::TraverseGCThing(TraverseSelect aTs, void* aThing,
                                         JSGCTraceKind aTraceKind,
                                         nsCycleCollectionTraversalCallback& aCb)
{
  MOZ_ASSERT(aTraceKind == js::GCThingTraceKind(aThing));
  bool isMarkedGray = xpc_IsGrayGCThing(aThing);

  if (aTs == TRAVERSE_FULL) {
    DescribeGCThing(!isMarkedGray, aThing, aTraceKind, aCb);
  }

  // If this object is alive, then all of its children are alive. For JS objects,
  // the black-gray invariant ensures the children are also marked black. For C++
  // objects, the ref count from this object will keep them alive. Thus we don't
  // need to trace our children, unless we are debugging using WantAllTraces.
  if (!isMarkedGray && !aCb.WantAllTraces()) {
    return;
  }

  if (aTs == TRAVERSE_FULL) {
    NoteGCThingJSChildren(aThing, aTraceKind, aCb);
  }

  if (aTraceKind == JSTRACE_OBJECT) {
    JSObject* obj = static_cast<JSObject*>(aThing);
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
CycleCollectedJSRuntime::TraverseObjectShim(void* aData, void* aThing)
{
  TraverseObjectShimClosure* closure =
    static_cast<TraverseObjectShimClosure*>(aData);

  MOZ_ASSERT(js::GCThingTraceKind(aThing) == JSTRACE_OBJECT);
  closure->self->TraverseGCThing(CycleCollectedJSRuntime::TRAVERSE_CPP, aThing,
                                 JSTRACE_OBJECT, closure->cb);
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
CycleCollectedJSRuntime::OutOfMemoryCallback(JSContext *aContext,
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
                     void* aClosure) const MOZ_OVERRIDE
  {
    JS_CallHeapValueTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<jsid>* aPtr, const char* aName,
                     void* aClosure) const MOZ_OVERRIDE
  {
    JS_CallHeapIdTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSObject*>* aPtr, const char* aName,
                     void* aClosure) const MOZ_OVERRIDE
  {
    JS_CallHeapObjectTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::TenuredHeap<JSObject*>* aPtr, const char* aName,
                     void* aClosure) const MOZ_OVERRIDE
  {
    JS_CallTenuredObjectTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSString*>* aPtr, const char* aName,
                     void* aClosure) const MOZ_OVERRIDE
  {
    JS_CallHeapStringTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSScript*>* aPtr, const char* aName,
                     void* aClosure) const MOZ_OVERRIDE
  {
    JS_CallHeapScriptTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
  }
  virtual void Trace(JS::Heap<JSFunction*>* aPtr, const char* aName,
                     void* aClosure) const MOZ_OVERRIDE
  {
    JS_CallHeapFunctionTracer(static_cast<JSTracer*>(aClosure), aPtr, aName);
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
  virtual void Trace(JS::Heap<JS::Value>* aPtr, const char*, void*) const MOZ_OVERRIDE
  {
    *aPtr = JSVAL_VOID;
  }

  virtual void Trace(JS::Heap<jsid>* aPtr, const char*, void*) const MOZ_OVERRIDE
  {
    *aPtr = JSID_VOID;
  }

  virtual void Trace(JS::Heap<JSObject*>* aPtr, const char*, void*) const MOZ_OVERRIDE
  {
    *aPtr = nullptr;
  }

  virtual void Trace(JS::TenuredHeap<JSObject*>* aPtr, const char*, void*) const MOZ_OVERRIDE
  {
    *aPtr = nullptr;
  }

  virtual void Trace(JS::Heap<JSString*>* aPtr, const char*, void*) const MOZ_OVERRIDE
  {
    *aPtr = nullptr;
  }

  virtual void Trace(JS::Heap<JSScript*>* aPtr, const char*, void*) const MOZ_OVERRIDE
  {
    *aPtr = nullptr;
  }

  virtual void Trace(JS::Heap<JSFunction*>* aPtr, const char*, void*) const MOZ_OVERRIDE
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
AssertNoGcThing(void* aGCThing, const char* aName, void* aClosure)
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
    MOZ_ASSERT(!js::GetObjectParent(obj));
    if (JS::GCThingIsMarkedGray(obj) &&
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
  JS::GCForReason(mJSRuntime, gcreason);
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
  mDeferredSupports.AppendElement(aSupports);
}

void
CycleCollectedJSRuntime::DumpJSHeap(FILE* aFile)
{
  js::DumpHeapComplete(Runtime(), aFile, js::CollectNurseryBeforeDump);
}


bool
ReleaseSliceNow(uint32_t aSlice, void* aData)
{
  MOZ_ASSERT(aSlice > 0, "nonsensical/useless call with slice == 0");
  nsTArray<nsISupports*>* items = static_cast<nsTArray<nsISupports*>*>(aData);

  uint32_t length = items->Length();
  aSlice = std::min(aSlice, length);
  for (uint32_t i = length; i > length - aSlice; --i) {
    // Remove (and NS_RELEASE) the last entry in "items":
    uint32_t lastItemIdx = i - 1;

    nsISupports* wrapper = items->ElementAt(lastItemIdx);
    items->RemoveElementAt(lastItemIdx);
    NS_IF_RELEASE(wrapper);
  }

  return items->IsEmpty();
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
                                                         nsTArray<nsISupports*>& aSupports,
                                                         DeferredFinalizerTable& aFinalizers)
  : mRuntime(aRt)
  , mFinalizeFunctionToRun(0)
{
  this->mSupports.SwapElements(aSupports);
  DeferredFinalizeFunctionHolder* function = mDeferredFinalizeFunctions.AppendElement();
  function->run = ReleaseSliceNow;
  function->data = &this->mSupports;

  // Enumerate the hashtable into our array.
  aFinalizers.Enumerate(DeferredFinalizerEnumerator, &mDeferredFinalizeFunctions);
}

IncrementalFinalizeRunnable::~IncrementalFinalizeRunnable()
{
  MOZ_ASSERT(this != mRuntime->mFinalizeRunnable);
}

void
IncrementalFinalizeRunnable::ReleaseNow(bool aLimited)
{
  //MOZ_ASSERT(NS_IsMainThread());
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
    MOZ_ASSERT(!mSupports.Length());
    MOZ_ASSERT(!mDeferredFinalizeFunctions.Length());
    return NS_OK;
  }

  ReleaseNow(true);

  if (mDeferredFinalizeFunctions.Length()) {
    nsresult rv = NS_DispatchToCurrentThread(this);
    if (NS_FAILED(rv)) {
      ReleaseNow(false);
    }
  }

  return NS_OK;
}

void
CycleCollectedJSRuntime::FinalizeDeferredThings(DeferredFinalizeType aType)
{
  MOZ_ASSERT(!mFinalizeRunnable);
  mFinalizeRunnable = new IncrementalFinalizeRunnable(this,
                                                      mDeferredSupports,
                                                      mDeferredFinalizerTable);

  // Everything should be gone now.
  MOZ_ASSERT(!mDeferredSupports.Length());
  MOZ_ASSERT(!mDeferredFinalizerTable.Count());

  if (aType == FinalizeIncrementally) {
    NS_DispatchToCurrentThread(mFinalizeRunnable);
  } else {
    mFinalizeRunnable->ReleaseNow(false);
    MOZ_ASSERT(!mFinalizeRunnable);
  }
}

void 
CycleCollectedJSRuntime::AnnotateAndSetOutOfMemory(OOMState *aStatePtr, OOMState aNewState)
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

      /*
       * If the previous GC created a runnable to finalize objects
       * incrementally, and if it hasn't finished yet, finish it now. We
       * don't want these to build up. We also don't want to allow any
       * existing incremental finalize runnables to run after a
       * non-incremental GC, since they are often used to detect leaks.
       */
      if (mFinalizeRunnable) {
        mFinalizeRunnable->ReleaseNow(false);
      }

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
