/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedJSContext_h__
#define mozilla_CycleCollectedJSContext_h__

#include <queue>

#include "mozilla/DeferredFinalize.h"
#include "mozilla/mozalloc.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/SegmentedVector.h"
#include "jsapi.h"
#include "jsfriendapi.h"

#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsTHashtable.h"

class nsCycleCollectionNoteRootCallback;
class nsIException;
class nsIRunnable;
class nsThread;
class nsWrapperCache;

namespace js {
struct Class;
} // namespace js

namespace mozilla {

class JSGCThingParticipant: public nsCycleCollectionParticipant
{
public:
  constexpr JSGCThingParticipant()
    : nsCycleCollectionParticipant(false) {}

  NS_IMETHOD_(void) Root(void*) override
  {
    MOZ_ASSERT(false, "Don't call Root on GC things");
  }

  NS_IMETHOD_(void) Unlink(void*) override
  {
    MOZ_ASSERT(false, "Don't call Unlink on GC things, as they may be dead");
  }

  NS_IMETHOD_(void) Unroot(void*) override
  {
    MOZ_ASSERT(false, "Don't call Unroot on GC things, as they may be dead");
  }

  NS_IMETHOD_(void) DeleteCycleCollectable(void* aPtr) override
  {
    MOZ_ASSERT(false, "Can't directly delete a cycle collectable GC thing");
  }

  NS_IMETHOD TraverseNative(void* aPtr, nsCycleCollectionTraversalCallback& aCb)
    override;

  NS_DECL_CYCLE_COLLECTION_CLASS_NAME_METHOD(JSGCThingParticipant)
};

class JSZoneParticipant : public nsCycleCollectionParticipant
{
public:
  constexpr JSZoneParticipant(): nsCycleCollectionParticipant(false)
  {
  }

  NS_IMETHOD_(void) Root(void*) override
  {
    MOZ_ASSERT(false, "Don't call Root on GC things");
  }

  NS_IMETHOD_(void) Unlink(void*) override
  {
    MOZ_ASSERT(false, "Don't call Unlink on GC things, as they may be dead");
  }

  NS_IMETHOD_(void) Unroot(void*) override
  {
    MOZ_ASSERT(false, "Don't call Unroot on GC things, as they may be dead");
  }

  NS_IMETHOD_(void) DeleteCycleCollectable(void*) override
  {
    MOZ_ASSERT(false, "Can't directly delete a cycle collectable GC thing");
  }

  NS_IMETHOD TraverseNative(void* aPtr, nsCycleCollectionTraversalCallback& aCb)
    override;

  NS_DECL_CYCLE_COLLECTION_CLASS_NAME_METHOD(JSZoneParticipant)
};

class IncrementalFinalizeRunnable;

// Contains various stats about the cycle collection.
struct CycleCollectorResults
{
  CycleCollectorResults()
  {
    // Initialize here so when we increment mNumSlices the first time we're
    // not using uninitialized memory.
    Init();
  }

  void Init()
  {
    mForcedGC = false;
    mMergedZones = false;
    mAnyManual = false;
    mVisitedRefCounted = 0;
    mVisitedGCed = 0;
    mFreedRefCounted = 0;
    mFreedGCed = 0;
    mFreedJSZones = 0;
    mNumSlices = 1;
    // mNumSlices is initialized to one, because we call Init() after the
    // per-slice increment of mNumSlices has already occurred.
  }

  bool mForcedGC;
  bool mMergedZones;
  bool mAnyManual; // true if any slice of the CC was manually triggered, or at shutdown.
  uint32_t mVisitedRefCounted;
  uint32_t mVisitedGCed;
  uint32_t mFreedRefCounted;
  uint32_t mFreedGCed;
  uint32_t mFreedJSZones;
  uint32_t mNumSlices;
};

class CycleCollectedJSContext
{
  friend class JSGCThingParticipant;
  friend class JSZoneParticipant;
  friend class IncrementalFinalizeRunnable;
protected:
  CycleCollectedJSContext();
  virtual ~CycleCollectedJSContext();

  MOZ_IS_CLASS_INIT
  nsresult Initialize(JSRuntime* aParentRuntime,
                      uint32_t aMaxBytes,
                      uint32_t aMaxNurseryBytes);

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
  void UnmarkSkippableJSHolders();

  virtual void
  TraverseAdditionalNativeRoots(nsCycleCollectionNoteRootCallback& aCb) {}
  virtual void TraceAdditionalNativeGrayRoots(JSTracer* aTracer) {}

  virtual void CustomGCCallback(JSGCStatus aStatus) {}
  virtual void CustomOutOfMemoryCallback() {}

  std::queue<nsCOMPtr<nsIRunnable>> mPromiseMicroTaskQueue;
  std::queue<nsCOMPtr<nsIRunnable>> mDebuggerPromiseMicroTaskQueue;

private:
  void
  DescribeGCThing(bool aIsMarked, JS::GCCellPtr aThing,
                  nsCycleCollectionTraversalCallback& aCb) const;

  virtual bool
  DescribeCustomObjects(JSObject* aObject, const js::Class* aClasp,
                        char (&aName)[72]) const
  {
    return false; // We did nothing.
  }

  void
  NoteGCThingJSChildren(JS::GCCellPtr aThing,
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
  TraverseGCThing(TraverseSelect aTs, JS::GCCellPtr aThing,
                  nsCycleCollectionTraversalCallback& aCb);

  void
  TraverseZone(JS::Zone* aZone, nsCycleCollectionTraversalCallback& aCb);

  static void
  TraverseObjectShim(void* aData, JS::GCCellPtr aThing);

  void TraverseNativeRoots(nsCycleCollectionNoteRootCallback& aCb);

  static void TraceBlackJS(JSTracer* aTracer, void* aData);
  static void TraceGrayJS(JSTracer* aTracer, void* aData);
  static void GCCallback(JSContext* aContext, JSGCStatus aStatus, void* aData);
  static void GCSliceCallback(JSContext* aContext, JS::GCProgress aProgress,
                              const JS::GCDescription& aDesc);
  static void GCNurseryCollectionCallback(JSContext* aContext,
                                          JS::GCNurseryProgress aProgress,
                                          JS::gcreason::Reason aReason);
  static void OutOfMemoryCallback(JSContext* aContext, void* aData);
  /**
   * Callback for reporting external string memory.
   */
  static size_t SizeofExternalStringCallback(JSString* aStr,
                                             mozilla::MallocSizeOf aMallocSizeOf);

  static bool ContextCallback(JSContext* aCx, unsigned aOperation,
                              void* aData);
  static JSObject* GetIncumbentGlobalCallback(JSContext* aCx);
  static bool EnqueuePromiseJobCallback(JSContext* aCx,
                                        JS::HandleObject aJob,
                                        JS::HandleObject aAllocationSite,
                                        JS::HandleObject aIncumbentGlobal,
                                        void* aData);
  static void PromiseRejectionTrackerCallback(JSContext* aCx,
                                              JS::HandleObject aPromise,
                                              PromiseRejectionHandlingState state,
                                              void* aData);

  virtual void TraceNativeBlackRoots(JSTracer* aTracer) { };
  void TraceNativeGrayRoots(JSTracer* aTracer);

  void AfterProcessMicrotask(uint32_t aRecursionDepth);
public:
  void ProcessStableStateQueue();
private:
  void ProcessMetastableStateQueue(uint32_t aRecursionDepth);

public:
  enum DeferredFinalizeType {
    FinalizeIncrementally,
    FinalizeNow,
  };

  void FinalizeDeferredThings(DeferredFinalizeType aType);

  // Two conditions, JSOutOfMemory and JSLargeAllocationFailure, are noted in
  // crash reports. Here are the values that can appear in the reports:
  enum class OOMState : uint32_t {
    // The condition has never happened. No entry appears in the crash report.
    OK,

    // We are currently reporting the given condition.
    //
    // Suppose a crash report contains "JSLargeAllocationFailure:
    // Reporting". This means we crashed while executing memory-pressure
    // observers, trying to shake loose some memory. The large allocation in
    // question did not return null: it is still on the stack. Had we not
    // crashed, it would have been retried.
    Reporting,

    // The condition has been reported since the last GC.
    //
    // If a crash report contains "JSOutOfMemory: Reported", that means a small
    // allocation failed, and then we crashed, probably due to buggy
    // error-handling code that ran after allocation returned null.
    //
    // This contrasts with "Reporting" which means that no error-handling code
    // had executed yet.
    Reported,

    // The condition has happened, but a GC cycle ended since then.
    //
    // GC is taken as a proxy for "we've been banging on the heap a good bit
    // now and haven't crashed; the OOM was probably handled correctly".
    Recovered
  };

  void SetLargeAllocationFailure(OOMState aNewState);

private:
  void AnnotateAndSetOutOfMemory(OOMState* aStatePtr, OOMState aNewState);
  void OnGC(JSGCStatus aStatus);
  void OnOutOfMemory();

public:
  void AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer);
  void RemoveJSHolder(void* aHolder);
#ifdef DEBUG
  bool IsJSHolder(void* aHolder);
  void AssertNoObjectsToTrace(void* aPossibleJSHolder);
#endif

  already_AddRefed<nsIException> GetPendingException() const;
  void SetPendingException(nsIException* aException);

  std::queue<nsCOMPtr<nsIRunnable>>& GetPromiseMicroTaskQueue();
  std::queue<nsCOMPtr<nsIRunnable>>& GetDebuggerPromiseMicroTaskQueue();

  nsCycleCollectionParticipant* GCThingParticipant();
  nsCycleCollectionParticipant* ZoneParticipant();

  nsresult TraverseRoots(nsCycleCollectionNoteRootCallback& aCb);
  virtual bool UsefulToMergeZones() const;
  void FixWeakMappingGrayBits() const;
  void CheckGrayBits() const;
  bool AreGCGrayBitsValid() const;
  void GarbageCollect(uint32_t aReason) const;

  void NurseryWrapperAdded(nsWrapperCache* aCache);
  void NurseryWrapperPreserved(JSObject* aWrapper);
  void JSObjectsTenured();

  void DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                        DeferredFinalizeFunction aFunc,
                        void* aThing);
  void DeferredFinalize(nsISupports* aSupports);

  void DumpJSHeap(FILE* aFile);

  virtual void PrepareForForgetSkippable() = 0;
  virtual void BeginCycleCollectionCallback() = 0;
  virtual void EndCycleCollectionCallback(CycleCollectorResults& aResults) = 0;
  virtual void DispatchDeferredDeletion(bool aContinuation, bool aPurge = false) = 0;

  JSContext* Context() const
  {
    MOZ_ASSERT(mJSContext);
    return mJSContext;
  }

  JS::RootingContext* RootingCx() const
  {
    MOZ_ASSERT(mJSContext);
    return JS::RootingContext::get(mJSContext);
  }

  bool MicroTaskCheckpointDisabled() const
  {
    return mDisableMicroTaskCheckpoint;
  }

  void DisableMicroTaskCheckpoint(bool aDisable)
  {
    mDisableMicroTaskCheckpoint = aDisable;
  }

  class MOZ_RAII AutoDisableMicroTaskCheckpoint
  {
    public:
    AutoDisableMicroTaskCheckpoint()
    : mCCJSCX(CycleCollectedJSContext::Get())
    {
      mOldValue = mCCJSCX->MicroTaskCheckpointDisabled();
      mCCJSCX->DisableMicroTaskCheckpoint(true);
    }

    ~AutoDisableMicroTaskCheckpoint()
    {
      mCCJSCX->DisableMicroTaskCheckpoint(mOldValue);
    }

    CycleCollectedJSContext* mCCJSCX;
    bool mOldValue;
  };

protected:
  JSContext* MaybeContext() const { return mJSContext; }

public:
  // nsThread entrypoints
  virtual void BeforeProcessTask(bool aMightBlock) { };
  virtual void AfterProcessTask(uint32_t aRecursionDepth);

  // microtask processor entry point
  void AfterProcessMicrotask();

  uint32_t RecursionDepth();

  // Run in stable state (call through nsContentUtils)
  void RunInStableState(already_AddRefed<nsIRunnable>&& aRunnable);
  // This isn't in the spec at all yet, but this gets the behavior we want for IDB.
  // Runs after the current microtask completes.
  void RunInMetastableState(already_AddRefed<nsIRunnable>&& aRunnable);

  // Get the current thread's CycleCollectedJSContext.  Returns null if there
  // isn't one.
  static CycleCollectedJSContext* Get();

  // Add aZone to the set of zones waiting for a GC.
  void AddZoneWaitingForGC(JS::Zone* aZone)
  {
    mZonesWaitingForGC.PutEntry(aZone);
  }

  // Prepare any zones for GC that have been passed to AddZoneWaitingForGC()
  // since the last GC or since the last call to PrepareWaitingZonesForGC(),
  // whichever was most recent. If there were no such zones, prepare for a
  // full GC.
  void PrepareWaitingZonesForGC();

  // Queue an async microtask to the current main or worker thread.
  virtual void DispatchToMicroTask(already_AddRefed<nsIRunnable> aRunnable);

  // Storage for watching rejected promises waiting for some client to
  // consume their rejection.
  // Promises in this list have been rejected in the last turn of the
  // event loop without the rejection being handled.
  // Note that this can contain nullptrs in place of promises removed because
  // they're consumed before it'd be reported.
  JS::PersistentRooted<JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>> mUncaughtRejections;

  // Promises in this list have previously been reported as rejected
  // (because they were in the above list), but the rejection was handled
  // in the last turn of the event loop.
  JS::PersistentRooted<JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>> mConsumedRejections;
  nsTArray<nsCOMPtr<nsISupports /* UncaughtRejectionObserver */ >> mUncaughtRejectionObservers;

private:
  JSGCThingParticipant mGCThingCycleCollectorGlobal;

  JSZoneParticipant mJSZoneCycleCollectorGlobal;

  JSContext* mJSContext;

  JS::GCSliceCallback mPrevGCSliceCallback;
  JS::GCNurseryCollectionCallback mPrevGCNurseryCollectionCallback;

  nsDataHashtable<nsPtrHashKey<void>, nsScriptObjectTracer*> mJSHolders;

  typedef nsDataHashtable<nsFuncPtrHashKey<DeferredFinalizeFunction>, void*>
    DeferredFinalizerTable;
  DeferredFinalizerTable mDeferredFinalizerTable;

  RefPtr<IncrementalFinalizeRunnable> mFinalizeRunnable;

  nsCOMPtr<nsIException> mPendingException;
  nsThread* mOwningThread; // Manual refcounting to avoid include hell.

  struct RunInMetastableStateData
  {
    nsCOMPtr<nsIRunnable> mRunnable;
    uint32_t mRecursionDepth;
  };

  nsTArray<nsCOMPtr<nsIRunnable>> mStableStateEvents;
  nsTArray<RunInMetastableStateData> mMetastableStateEvents;
  uint32_t mBaseRecursionDepth;
  bool mDoingStableStates;

  bool mDisableMicroTaskCheckpoint;

  OOMState mOutOfMemoryState;
  OOMState mLargeAllocationFailureState;

  static const size_t kSegmentSize = 512;
  SegmentedVector<nsWrapperCache*, kSegmentSize, InfallibleAllocPolicy>
    mNurseryObjects;
  SegmentedVector<JS::PersistentRooted<JSObject*>, kSegmentSize,
                  InfallibleAllocPolicy>
    mPreservedNurseryObjects;

  nsTHashtable<nsPtrHashKey<JS::Zone>> mZonesWaitingForGC;

  struct EnvironmentPreparer : public js::ScriptEnvironmentPreparer {
    void invoke(JS::HandleObject scope, Closure& closure) override;
  };
  EnvironmentPreparer mEnvironmentPreparer;
};

void TraceScriptHolder(nsISupports* aHolder, JSTracer* aTracer);

// Returns true if the JS::TraceKind is one the cycle collector cares about.
inline bool AddToCCKind(JS::TraceKind aKind)
{
  return aKind == JS::TraceKind::Object ||
         aKind == JS::TraceKind::Script ||
         aKind == JS::TraceKind::Scope ||
         aKind == JS::TraceKind::RegExpShared;
}

bool
GetBuildId(JS::BuildIdCharVector* aBuildID);

} // namespace mozilla

#endif // mozilla_CycleCollectedJSContext_h__
