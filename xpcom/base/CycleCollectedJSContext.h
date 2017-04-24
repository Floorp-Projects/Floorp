/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedJSContext_h
#define mozilla_CycleCollectedJSContext_h

#include <queue>

#include "mozilla/DeferredFinalize.h"
#include "mozilla/mozalloc.h"
#include "mozilla/MemoryReporting.h"
#include "jsapi.h"
#include "jsfriendapi.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"

class nsCycleCollectionNoteRootCallback;
class nsIException;
class nsIRunnable;
class nsThread;
class nsWrapperCache;

namespace mozilla {

class CycleCollectedJSRuntime;

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
  friend class CycleCollectedJSRuntime;

protected:
  CycleCollectedJSContext();
  virtual ~CycleCollectedJSContext();

  MOZ_IS_CLASS_INIT
  nsresult Initialize(JSRuntime* aParentRuntime,
                      uint32_t aMaxBytes,
                      uint32_t aMaxNurseryBytes);

  // See explanation in mIsPrimaryContext.
  MOZ_IS_CLASS_INIT
  nsresult InitializeNonPrimary(CycleCollectedJSContext* aPrimaryContext);

  virtual CycleCollectedJSRuntime* CreateRuntime(JSContext* aCx) = 0;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  std::queue<nsCOMPtr<nsIRunnable>> mPromiseMicroTaskQueue;
  std::queue<nsCOMPtr<nsIRunnable>> mDebuggerPromiseMicroTaskQueue;

private:
  MOZ_IS_CLASS_INIT
  void InitializeCommon();

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

public:
  CycleCollectedJSRuntime* Runtime() const
  {
    MOZ_ASSERT(mRuntime);
    return mRuntime;
  }

  already_AddRefed<nsIException> GetPendingException() const;
  void SetPendingException(nsIException* aException);

  std::queue<nsCOMPtr<nsIRunnable>>& GetPromiseMicroTaskQueue();
  std::queue<nsCOMPtr<nsIRunnable>>& GetDebuggerPromiseMicroTaskQueue();

  void PrepareForForgetSkippable();
  void BeginCycleCollectionCallback();
  void EndCycleCollectionCallback(CycleCollectorResults& aResults);
  void DispatchDeferredDeletion(bool aContinuation, bool aPurge = false);

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

  void AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer);
  void RemoveJSHolder(void* aHolder);
#ifdef DEBUG
  bool IsJSHolder(void* aHolder);
  void AssertNoObjectsToTrace(void* aPossibleJSHolder);
#endif

  nsCycleCollectionParticipant* GCThingParticipant();
  nsCycleCollectionParticipant* ZoneParticipant();

  nsresult TraverseRoots(nsCycleCollectionNoteRootCallback& aCb);
  virtual bool UsefulToMergeZones() const;
  void FixWeakMappingGrayBits() const;
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

  // Add aZone to the set of zones waiting for a GC.
  void AddZoneWaitingForGC(JS::Zone* aZone);

  // Prepare any zones for GC that have been passed to AddZoneWaitingForGC()
  // since the last GC or since the last call to PrepareWaitingZonesForGC(),
  // whichever was most recent. If there were no such zones, prepare for a
  // full GC.
  void PrepareWaitingZonesForGC();

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
  // A primary context owns the mRuntime. Non-main-thread contexts should always
  // be primary. On the main thread, the primary context should be the first one
  // created and the last one destroyed. Non-primary contexts are used for
  // cooperatively scheduled threads.
  bool mIsPrimaryContext;

  CycleCollectedJSRuntime* mRuntime;

  JSContext* mJSContext;

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
};

} // namespace mozilla

#endif // mozilla_CycleCollectedJSContext_h
