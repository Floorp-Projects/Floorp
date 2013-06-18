/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CycleCollectedJSRuntime.h"
#include "jsfriendapi.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "nsLayoutStatics.h"
#include "xpcpublic.h"

using namespace mozilla;

inline bool
AddToCCKind(JSGCTraceKind kind)
{
  return kind == JSTRACE_OBJECT || kind == JSTRACE_SCRIPT;
}

struct NoteWeakMapChildrenTracer : public JSTracer
{
  NoteWeakMapChildrenTracer(nsCycleCollectionNoteRootCallback& cb)
  : mCb(cb)
  {
  }
  nsCycleCollectionNoteRootCallback& mCb;
  bool mTracedAny;
  JSObject* mMap;
  void* mKey;
  void* mKeyDelegate;
};

static void
TraceWeakMappingChild(JSTracer* trc, void** thingp, JSGCTraceKind kind)
{
  MOZ_ASSERT(trc->callback == TraceWeakMappingChild);
  void* thing = *thingp;
  NoteWeakMapChildrenTracer* tracer =
    static_cast<NoteWeakMapChildrenTracer*>(trc);

  if (kind == JSTRACE_STRING) {
    return;
  }

  if (!xpc_IsGrayGCThing(thing) && !tracer->mCb.WantAllTraces()) {
    return;
  }

  if (AddToCCKind(kind)) {
    tracer->mCb.NoteWeakMapping(tracer->mMap, tracer->mKey, tracer->mKeyDelegate, thing);
    tracer->mTracedAny = true;
  } else {
    JS_TraceChildren(trc, thing, kind);
  }
}

struct NoteWeakMapsTracer : public js::WeakMapTracer
{
  NoteWeakMapsTracer(JSRuntime* rt, js::WeakMapTraceCallback cb,
                     nsCycleCollectionNoteRootCallback& cccb)
  : js::WeakMapTracer(rt, cb), mCb(cccb), mChildTracer(cccb)
  {
    JS_TracerInit(&mChildTracer, rt, TraceWeakMappingChild);
  }
  nsCycleCollectionNoteRootCallback& mCb;
  NoteWeakMapChildrenTracer mChildTracer;
};

static void
TraceWeakMapping(js::WeakMapTracer* trc, JSObject* m,
               void* k, JSGCTraceKind kkind,
               void* v, JSGCTraceKind vkind)
{
  MOZ_ASSERT(trc->callback == TraceWeakMapping);
  NoteWeakMapsTracer* tracer = static_cast<NoteWeakMapsTracer* >(trc);

  // If nothing that could be held alive by this entry is marked gray, return.
  if ((!k || !xpc_IsGrayGCThing(k)) && MOZ_LIKELY(!tracer->mCb.WantAllTraces())) {
    if (!v || !xpc_IsGrayGCThing(v) || vkind == JSTRACE_STRING) {
      return;
    }
  }

  // The cycle collector can only properly reason about weak maps if it can
  // reason about the liveness of their keys, which in turn requires that
  // the key can be represented in the cycle collector graph.  All existing
  // uses of weak maps use either objects or scripts as keys, which are okay.
  MOZ_ASSERT(AddToCCKind(kkind));

  // As an emergency fallback for non-debug builds, if the key is not
  // representable in the cycle collector graph, we treat it as marked.  This
  // can cause leaks, but is preferable to ignoring the binding, which could
  // cause the cycle collector to free live objects.
  if (!AddToCCKind(kkind)) {
    k = nullptr;
  }

  JSObject* kdelegate = nullptr;
  if (k && kkind == JSTRACE_OBJECT) {
    kdelegate = js::GetWeakmapKeyDelegate((JSObject*)k);
  }

  if (AddToCCKind(vkind)) {
    tracer->mCb.NoteWeakMapping(m, k, kdelegate, v);
  } else {
    tracer->mChildTracer.mTracedAny = false;
    tracer->mChildTracer.mMap = m;
    tracer->mChildTracer.mKey = k;
    tracer->mChildTracer.mKeyDelegate = kdelegate;

    if (v && vkind != JSTRACE_STRING) {
      JS_TraceChildren(&tracer->mChildTracer, v, vkind);
    }

    // The delegate could hold alive the key, so report something to the CC
    // if we haven't already.
    if (!tracer->mChildTracer.mTracedAny && k && xpc_IsGrayGCThing(k) && kdelegate) {
      tracer->mCb.NoteWeakMapping(m, k, kdelegate, nullptr);
    }
  }
}

// This is based on the logic in TraceWeakMapping.
struct FixWeakMappingGrayBitsTracer : public js::WeakMapTracer
{
  FixWeakMappingGrayBitsTracer(JSRuntime* rt)
    : js::WeakMapTracer(rt, FixWeakMappingGrayBits)
  {}

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
  FixWeakMappingGrayBits(js::WeakMapTracer* trc, JSObject* m,
                         void* k, JSGCTraceKind kkind,
                         void* v, JSGCTraceKind vkind)
  {
    MOZ_ASSERT(!JS::IsIncrementalGCInProgress(trc->runtime),
               "Don't call FixWeakMappingGrayBits during a GC.");

    FixWeakMappingGrayBitsTracer* tracer = static_cast<FixWeakMappingGrayBitsTracer*>(trc);

    // If nothing that could be held alive by this entry is marked gray, return.
    bool delegateMightNeedMarking = k && xpc_IsGrayGCThing(k);
    bool valueMightNeedMarking = v && xpc_IsGrayGCThing(v) && vkind != JSTRACE_STRING;
    if (!delegateMightNeedMarking && !valueMightNeedMarking) {
      return;
    }

    if (!AddToCCKind(kkind)) {
      k = nullptr;
    }

    if (delegateMightNeedMarking && kkind == JSTRACE_OBJECT) {
      JSObject* kdelegate = js::GetWeakmapKeyDelegate((JSObject*)k);
      if (kdelegate && !xpc_IsGrayGCThing(kdelegate)) {
        JS::UnmarkGrayGCThingRecursively(k, JSTRACE_OBJECT);
        tracer->mAnyMarked = true;
      }
    }

    if (v && xpc_IsGrayGCThing(v) &&
        (!k || !xpc_IsGrayGCThing(k)) &&
        (!m || !xpc_IsGrayGCThing(m)) &&
        vkind != JSTRACE_SHAPE) {
      JS::UnmarkGrayGCThingRecursively(v, vkind);
      tracer->mAnyMarked = true;
    }
  }

  bool mAnyMarked;
};

class JSContextParticipant : public nsCycleCollectionParticipant
{
public:
  static NS_METHOD RootImpl(void *n)
  {
    return NS_OK;
  }
  static NS_METHOD UnlinkImpl(void *n)
  {
    return NS_OK;
  }
  static NS_METHOD UnrootImpl(void *n)
  {
    return NS_OK;
  }
  static NS_METHOD_(void) UnmarkIfPurpleImpl(void *n)
  {
  }
  static NS_METHOD TraverseImpl(JSContextParticipant *that, void *n,
                                nsCycleCollectionTraversalCallback &cb)
  {
    JSContext *cx = static_cast<JSContext*>(n);

    // JSContexts do not have an internal refcount and always have a single
    // owner (e.g., nsJSContext). Thus, the default refcount is 1. However,
    // in the (abnormal) case of synchronous cycle-collection, the context
    // may be actively executing code in which case we want to treat it as
    // rooted by adding an extra refcount.
    unsigned refCount = js::ContextHasOutstandingRequests(cx) ? 2 : 1;

    cb.DescribeRefCountedNode(refCount, "JSContext");
    if (JSObject *global = js::GetDefaultGlobalForContext(cx)) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "[global object]");
      cb.NoteJSChild(global);
    }

    return NS_OK;
  }
};

static const CCParticipantVTable<JSContextParticipant>::Type
JSContext_cycleCollectorGlobal =
{
  NS_IMPL_CYCLE_COLLECTION_NATIVE_VTABLE(JSContextParticipant)
};

struct Closure
{
  bool cycleCollectionEnabled;
  nsCycleCollectionNoteRootCallback *cb;
};

static void
CheckParticipatesInCycleCollection(void *aThing, const char *name, void *aClosure)
{
  Closure *closure = static_cast<Closure*>(aClosure);

  if (closure->cycleCollectionEnabled) {
    return;
  }

  if (AddToCCKind(js::GCThingTraceKind(aThing)) &&
      xpc_IsGrayGCThing(aThing))
  {
    closure->cycleCollectionEnabled = true;
  }
}

static PLDHashOperator
NoteJSHolder(void *holder, nsScriptObjectTracer *&tracer, void *arg)
{
  Closure *closure = static_cast<Closure*>(arg);

  closure->cycleCollectionEnabled = false;
  tracer->Trace(holder, TraceCallbackFunc(CheckParticipatesInCycleCollection), closure);
  if (closure->cycleCollectionEnabled) {
    closure->cb->NoteNativeRoot(holder, tracer);
  }

  return PL_DHASH_NEXT;
}

CycleCollectedJSRuntime::CycleCollectedJSRuntime(uint32_t aMaxbytes,
                                                 JSUseHelperThreads aUseHelperThreads,
                                                 bool aExpectUnrootedGlobals)
  : mJSRuntime(nullptr)
#ifdef DEBUG
  , mObjectToUnlink(nullptr)
  , mExpectUnrootedGlobals(aExpectUnrootedGlobals)
#endif
{
  mJSRuntime = JS_NewRuntime(aMaxbytes, aUseHelperThreads);
  if (!mJSRuntime) {
    MOZ_CRASH();
  }

  mJSHolders.Init(512);
}

CycleCollectedJSRuntime::~CycleCollectedJSRuntime()
{
  JS_DestroyRuntime(mJSRuntime);
  mJSRuntime = nullptr;
}

void
CycleCollectedJSRuntime::MaybeTraceGlobals(JSTracer* aTracer) const
{
  JSContext* iter = nullptr;
  while (JSContext* acx = JS_ContextIterator(Runtime(), &iter)) {
    MOZ_ASSERT(js::HasUnrootedGlobal(acx) == mExpectUnrootedGlobals);
    if (!js::HasUnrootedGlobal(acx)) {
      continue;
    }

    if (JSObject* global = js::GetDefaultGlobalForContext(acx)) {
      JS_CallObjectTracer(aTracer, &global, "Global Object");
    }
  }
}

// For all JS objects that are held by native objects but aren't held
// through rooting or locking, we need to add all the native objects that
// hold them so that the JS objects are colored correctly in the cycle
// collector. This includes JSContexts that don't have outstanding requests,
// because their global object wasn't marked by the JS GC. All other JS
// roots were marked by the JS GC and will be colored correctly in the cycle
// collector.
void
CycleCollectedJSRuntime::MaybeTraverseGlobals(nsCycleCollectionNoteRootCallback& aCb) const
{
  JSContext *iter = nullptr, *acx;
  while ((acx = JS_ContextIterator(Runtime(), &iter))) {
    // Add the context to the CC graph only if traversing it would
    // end up doing something.
    JSObject* global = js::GetDefaultGlobalForContext(acx);
    if (global && xpc_IsGrayGCThing(global)) {
      aCb.NoteNativeRoot(acx, JSContextParticipant());
    }
  }
}

void
CycleCollectedJSRuntime::TraverseNativeRoots(nsCycleCollectionNoteRootCallback& aCb)
{
  MaybeTraverseGlobals(aCb);

  // NB: This is here just to preserve the existing XPConnect order. I doubt it
  // would hurt to do this after the JS holders.
  TraverseAdditionalNativeRoots(aCb);

  Closure closure = { true, &aCb };
  mJSHolders.Enumerate(NoteJSHolder, &closure);
}

void
CycleCollectedJSRuntime::AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer)
{
  MOZ_ASSERT(aTracer->Trace, "AddJSHolder needs a non-null Trace function");
  bool wasEmpty = mJSHolders.Count() == 0;
  mJSHolders.Put(aHolder, aTracer);
  if (wasEmpty && mJSHolders.Count() == 1) {
    nsLayoutStatics::AddRef();
  }
}

void
CycleCollectedJSRuntime::RemoveJSHolder(void* aHolder)
{
#ifdef DEBUG
  // Assert that the holder doesn't try to keep any GC things alive.
  // In case of unlinking cycle collector calls AssertNoObjectsToTrace
  // manually because we don't want to check the holder before we are
  // finished unlinking it
  if (aHolder != mObjectToUnlink) {
    AssertNoObjectsToTrace(aHolder);
  }
#endif
  bool hadOne = mJSHolders.Count() == 1;
  mJSHolders.Remove(aHolder);
  if (hadOne && mJSHolders.Count() == 0) {
    nsLayoutStatics::Release();
  }
}

#ifdef DEBUG
bool
CycleCollectedJSRuntime::TestJSHolder(void* aHolder)
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
  if (tracer && tracer->Trace) {
    tracer->Trace(aPossibleJSHolder, TraceCallbackFunc(AssertNoGcThing), nullptr);
  }
}
#endif

// static
nsCycleCollectionParticipant*
CycleCollectedJSRuntime::JSContextParticipant()
{
  return JSContext_cycleCollectorGlobal.GetParticipant();
}

bool
CycleCollectedJSRuntime::NotifyLeaveMainThread() const
{
  MOZ_ASSERT(NS_IsMainThread());
  if (JS_IsInRequest(mJSRuntime)) {
    return false;
  }
  JS_ClearRuntimeThread(mJSRuntime);
  return true;
}

void
CycleCollectedJSRuntime::NotifyEnterCycleCollectionThread() const
{
  MOZ_ASSERT(!NS_IsMainThread());
  JS_SetRuntimeThread(mJSRuntime);
}

void
CycleCollectedJSRuntime::NotifyLeaveCycleCollectionThread() const
{
  MOZ_ASSERT(!NS_IsMainThread());
  JS_ClearRuntimeThread(mJSRuntime);
}

void
CycleCollectedJSRuntime::NotifyEnterMainThread() const
{
  MOZ_ASSERT(NS_IsMainThread());
  JS_SetRuntimeThread(mJSRuntime);
}

nsresult
CycleCollectedJSRuntime::BeginCycleCollection(nsCycleCollectionNoteRootCallback &aCb)
{
  static bool gcHasRun = false;
  if (!gcHasRun) {
    uint32_t gcNumber = JS_GetGCParameter(mJSRuntime, JSGC_NUMBER);
    if (!gcNumber) {
      // Cannot cycle collect if GC has not run first!
      MOZ_CRASH();
    }
    gcHasRun = true;
  }

  TraverseNativeRoots(aCb);

  NoteWeakMapsTracer trc(mJSRuntime, TraceWeakMapping, aCb);
  js::TraceWeakMaps(&trc);

  return NS_OK;
}

void
CycleCollectedJSRuntime::FixWeakMappingGrayBits() const
{
  FixWeakMappingGrayBitsTracer fixer(mJSRuntime);
  fixer.FixAll();
}

bool
CycleCollectedJSRuntime::NeedCollect() const
{
  return !js::AreGCGrayBitsValid(mJSRuntime);
}
