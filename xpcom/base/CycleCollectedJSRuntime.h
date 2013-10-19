/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedJSRuntime_h__
#define mozilla_CycleCollectedJSRuntime_h__

#include "mozilla/MemoryReporting.h"
#include "jsapi.h"

#include "nsCycleCollector.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"

class nsCycleCollectionNoteRootCallback;
class nsIException;

namespace js {
class Class;
}

namespace mozilla {

class JSGCThingParticipant: public nsCycleCollectionParticipant
{
public:
  NS_IMETHOD_(void) Root(void *n)
  {
  }

  NS_IMETHOD_(void) Unlink(void *n)
  {
  }

  NS_IMETHOD_(void) Unroot(void *n)
  {
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

  NS_IMETHOD_(void) Root(void *p)
  {
  }

  NS_IMETHOD_(void) Unlink(void *p)
  {
  }

  NS_IMETHOD_(void) Unroot(void *p)
  {
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
                          JSUseHelperThreads aUseHelperThreads);
  virtual ~CycleCollectedJSRuntime();

  // Idempotent. Subclasses may destroy their runtimes earlier in execution if
  // they so desire.
  void DestroyRuntime();

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  void UnmarkSkippableJSHolders();

  virtual void TraverseAdditionalNativeRoots(nsCycleCollectionNoteRootCallback& aCb) {}
  virtual void TraceAdditionalNativeGrayRoots(JSTracer* aTracer) {}

  virtual void CustomGCCallback(JSGCStatus aStatus) {}
  virtual bool CustomContextCallback(JSContext* aCx, unsigned aOperation)
  {
    return true; // Don't block context creation.
  }

private:

  void
  DescribeGCThing(bool aIsMarked, void* aThing, JSGCTraceKind aTraceKind,
                  nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool
  DescribeCustomObjects(JSObject* aObject, const js::Class* aClasp,
                        char (&aName)[72]) const
  {
    return false; // We did nothing.
  }

  void
  NoteGCThingJSChildren(void* aThing, JSGCTraceKind aTraceKind,
                        nsCycleCollectionTraversalCallback& aCb) const;

  void
  NoteGCThingXPCOMChildren(const js::Class* aClasp, JSObject* aObj,
                           nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool
  NoteCustomGCThingXPCOMChildren(const js::Class* aClasp, JSObject* aObj,
                                 nsCycleCollectionTraversalCallback& aCb) const
  {
    return false; // We did nothing.
  }

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

  void TraverseNativeRoots(nsCycleCollectionNoteRootCallback& aCb);

  static void TraceBlackJS(JSTracer* aTracer, void* aData);
  static void TraceGrayJS(JSTracer* aTracer, void* aData);
  static void GCCallback(JSRuntime* aRuntime, JSGCStatus aStatus, void* aData);
  static bool ContextCallback(JSContext* aCx, unsigned aOperation,
                              void* aData);

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
  bool IsJSHolder(void* aHolder);
  void AssertNoObjectsToTrace(void* aPossibleJSHolder);
#endif

  already_AddRefed<nsIException> GetPendingException() const;
  void SetPendingException(nsIException* aException);

  nsCycleCollectionParticipant* GCThingParticipant();
  nsCycleCollectionParticipant* ZoneParticipant();

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

  void DumpJSHeap(FILE* aFile);
  
  virtual void DispatchDeferredDeletion(bool aContinuation) = 0;

  JSRuntime* Runtime() const
  {
    MOZ_ASSERT(mJSRuntime);
    return mJSRuntime;
  }

  // Get the current thread's CycleCollectedJSRuntime.  Returns null if there
  // isn't one.
  static CycleCollectedJSRuntime* Get();

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

  nsCOMPtr<nsIException> mPendingException;
};

} // namespace mozilla

#endif // mozilla_CycleCollectedJSRuntime_h__
