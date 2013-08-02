/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedJSRuntime_h__
#define mozilla_CycleCollectedJSRuntime_h__

#include "mozilla/MemoryReporting.h"
#include "jsprvtd.h"
#include "jsapi.h"

#include "nsCycleCollector.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"

class nsCycleCollectionNoteRootCallback;
class nsScriptObjectTracer;

namespace mozilla {

class JSGCThingParticipant: public nsCycleCollectionParticipant
{
public:
  NS_IMETHOD Root(void *n)
  {
    return NS_OK;
  }

  NS_IMETHOD Unlink(void *n)
  {
    return NS_OK;
  }

  NS_IMETHOD Unroot(void *n)
  {
    return NS_OK;
  }

  NS_IMETHOD_(void) DeleteCycleCollectable(void *n)
  {
  }

  NS_IMETHOD Traverse(void *n, nsCycleCollectionTraversalCallback &cb);
};

class JSZoneParticipant : public nsCycleCollectionParticipant
{
public:
  MOZ_CONSTEXPR JSZoneParticipant(): nsCycleCollectionParticipant() {}

  NS_IMETHOD Root(void *p)
  {
    return NS_OK;
  }

  NS_IMETHOD Unlink(void *p)
  {
    return NS_OK;
  }

  NS_IMETHOD Unroot(void *p)
  {
    return NS_OK;
  }

  NS_IMETHOD_(void) DeleteCycleCollectable(void *n)
  {
  }

  NS_IMETHOD Traverse(void *p, nsCycleCollectionTraversalCallback &cb);
};

class IncrementalFinalizeRunnable;

class CycleCollectedJSRuntime
{
  friend class JSGCThingParticipant;
  friend class JSZoneParticipant;
  friend class IncrementalFinalizeRunnable;
protected:
  CycleCollectedJSRuntime(uint32_t aMaxbytes,
                          JSUseHelperThreads aUseHelperThreads,
                          bool aExpectRootedGlobals);
  virtual ~CycleCollectedJSRuntime();

  JSRuntime* Runtime() const
  {
    MOZ_ASSERT(mJSRuntime);
    return mJSRuntime;
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  void UnmarkSkippableJSHolders();

  virtual void TraverseAdditionalNativeRoots(nsCycleCollectionNoteRootCallback& aCb) = 0;
  virtual void TraceAdditionalNativeGrayRoots(JSTracer* aTracer) = 0;

  virtual void CustomGCCallback(JSGCStatus aStatus) {}

private:

  void
  DescribeGCThing(bool aIsMarked, void* aThing, JSGCTraceKind aTraceKind,
                  nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool
  DescribeCustomObjects(JSObject* aObject, js::Class* aClasp,
                        char (&aName)[72]) const = 0;

  void
  NoteGCThingJSChildren(void* aThing, JSGCTraceKind aTraceKind,
                        nsCycleCollectionTraversalCallback& aCb) const;

  void
  NoteGCThingXPCOMChildren(js::Class* aClasp, JSObject* aObj,
                           nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool
  NoteCustomGCThingXPCOMChildren(js::Class* aClasp, JSObject* aObj,
                                 nsCycleCollectionTraversalCallback& aCb) const = 0;


  enum TraverseSelect {
      TRAVERSE_CPP,
      TRAVERSE_FULL
  };

  void
  TraverseGCThing(TraverseSelect aTs, void* aThing,
                  JSGCTraceKind aTraceKind,
                  nsCycleCollectionTraversalCallback& aCb);

  void
  TraverseZone(JS::Zone* aZone, nsCycleCollectionTraversalCallback& aCb);

  static void
  TraverseObjectShim(void* aData, void* aThing);

  void MaybeTraverseGlobals(nsCycleCollectionNoteRootCallback& aCb) const;

  void TraverseNativeRoots(nsCycleCollectionNoteRootCallback& aCb);

  void MaybeTraceGlobals(JSTracer* aTracer) const;


  static void TraceBlackJS(JSTracer* aTracer, void* aData);
  static void TraceGrayJS(JSTracer* aTracer, void* aData);
  static void GCCallback(JSRuntime* aRuntime, JSGCStatus aStatus, void* aData);

  virtual void TraceNativeBlackRoots(JSTracer* aTracer) { };
  void TraceNativeGrayRoots(JSTracer* aTracer);

  enum DeferredFinalizeType {
    FinalizeIncrementally,
    FinalizeNow,
  };

  void FinalizeDeferredThings(DeferredFinalizeType aType);

  void OnGC(JSGCStatus aStatus);

public:
  void AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer);
  void RemoveJSHolder(void* aHolder);
#ifdef DEBUG
  bool TestJSHolder(void* aHolder);
  void SetObjectToUnlink(void* aObject) { mObjectToUnlink = aObject; }
  void AssertNoObjectsToTrace(void* aPossibleJSHolder);
#endif

  // This returns the singleton nsCycleCollectionParticipant for JSContexts.
  static nsCycleCollectionParticipant* JSContextParticipant();

  nsCycleCollectionParticipant* GCThingParticipant();
  nsCycleCollectionParticipant* ZoneParticipant();

  bool NotifyLeaveMainThread() const;
  void NotifyEnterCycleCollectionThread() const;
  void NotifyLeaveCycleCollectionThread() const;
  void NotifyEnterMainThread() const;
  nsresult BeginCycleCollection(nsCycleCollectionNoteRootCallback &aCb);
  bool UsefulToMergeZones() const;
  void FixWeakMappingGrayBits() const;
  bool NeedCollect() const;
  void Collect(uint32_t reason) const;

  virtual void PrepareForForgetSkippable() {}
  virtual void PrepareForCollection() {}

  void DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                        DeferredFinalizeFunction aFunc,
                        void* aThing);
  void DeferredFinalize(nsISupports* aSupports);

private:
  JSGCThingParticipant mGCThingCycleCollectorGlobal;

  JSZoneParticipant mJSZoneCycleCollectorGlobal;

  JSRuntime* mJSRuntime;

  nsDataHashtable<nsPtrHashKey<void>, nsScriptObjectTracer*> mJSHolders;

  nsTArray<nsISupports*> mDeferredSupports;
  typedef nsDataHashtable<nsFuncPtrHashKey<DeferredFinalizeFunction>, void*>
    DeferredFinalizerTable;
  DeferredFinalizerTable mDeferredFinalizerTable;

  nsRefPtr<IncrementalFinalizeRunnable> mFinalizeRunnable;

#ifdef DEBUG
  void* mObjectToUnlink;
  bool mExpectUnrootedGlobals;
#endif
};

} // namespace mozilla

#endif // mozilla_CycleCollectedJSRuntime_h__
