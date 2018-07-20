/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedJSContext_h
#define mozilla_CycleCollectedJSContext_h

#include <queue>

#include "mozilla/DeferredFinalize.h"
#include "mozilla/LinkedList.h"
#include "mozilla/mozalloc.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/AtomList.h"
#include "jsapi.h"
#include "jsfriendapi.h"

#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"

class nsCycleCollectionNoteRootCallback;
class nsIRunnable;
class nsThread;
class nsWrapperCache;

namespace mozilla {
class AutoSlowOperation;

class CycleCollectedJSRuntime;

namespace dom {
class Exception;
class WorkerJSContext;
class WorkletJSContext;
} // namespace dom

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

class MicroTaskRunnable
{
public:
  MicroTaskRunnable() {}
  NS_INLINE_DECL_REFCOUNTING(MicroTaskRunnable)
  virtual void Run(AutoSlowOperation& aAso) = 0;
  virtual bool Suppressed() { return false; }
protected:
  virtual ~MicroTaskRunnable() {}
};

class CycleCollectedJSContext
  : dom::PerThreadAtomCache
  , public LinkedListElement<CycleCollectedJSContext>
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
                                              JS::PromiseRejectionHandlingState state,
                                              void* aData);

  void AfterProcessMicrotasks();
public:
  void ProcessStableStateQueue();
private:
  void CleanupIDBTransactions(uint32_t aRecursionDepth);

public:
  enum DeferredFinalizeType {
    FinalizeIncrementally,
    FinalizeNow,
  };

  virtual dom::WorkerJSContext* GetAsWorkerJSContext() { return nullptr; }
  virtual dom::WorkletJSContext* GetAsWorkletJSContext() { return nullptr; }

  CycleCollectedJSRuntime* Runtime() const
  {
    MOZ_ASSERT(mRuntime);
    return mRuntime;
  }

  already_AddRefed<dom::Exception> GetPendingException() const;
  void SetPendingException(dom::Exception* aException);

  std::queue<RefPtr<MicroTaskRunnable>>& GetMicroTaskQueue();
  std::queue<RefPtr<MicroTaskRunnable>>& GetDebuggerMicroTaskQueue();

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

  void SetTargetedMicroTaskRecursionDepth(uint32_t aDepth)
  {
    mTargetedMicroTaskRecursionDepth = aDepth;
  }

protected:
  JSContext* MaybeContext() const { return mJSContext; }

public:
  // nsThread entrypoints
  virtual void BeforeProcessTask(bool aMightBlock);
  virtual void AfterProcessTask(uint32_t aRecursionDepth);

  // Check whether we need an idle GC task.
  void IsIdleGCTaskNeeded();

  uint32_t RecursionDepth();

  // Run in stable state (call through nsContentUtils)
  void RunInStableState(already_AddRefed<nsIRunnable>&& aRunnable);

  void AddPendingIDBTransaction(already_AddRefed<nsIRunnable>&& aTransaction);

  // Get the CycleCollectedJSContext for a JSContext.
  // Returns null only if Initialize() has not completed on or during
  // destruction of the CycleCollectedJSContext.
  static CycleCollectedJSContext* GetFor(JSContext* aCx);

  // Get the current thread's CycleCollectedJSContext.  Returns null if there
  // isn't one.
  static CycleCollectedJSContext* Get();

  // Queue an async microtask to the current main or worker thread.
  virtual void DispatchToMicroTask(already_AddRefed<MicroTaskRunnable> aRunnable);

  // Call EnterMicroTask when you're entering JS execution.
  // Usually the best way to do this is to use nsAutoMicroTask.
  void EnterMicroTask()
  {
    ++mMicroTaskLevel;
  }

  void LeaveMicroTask()
  {
    if (--mMicroTaskLevel == 0) {
      PerformMicroTaskCheckPoint();
    }
  }

  bool IsInMicroTask()
  {
    return mMicroTaskLevel != 0;
  }

  uint32_t MicroTaskLevel()
  {
    return mMicroTaskLevel;
  }

  void SetMicroTaskLevel(uint32_t aLevel)
  {
    mMicroTaskLevel = aLevel;
  }

  bool PerformMicroTaskCheckPoint(bool aForce = false);

  void PerformDebuggerMicroTaskCheckpoint();

  bool IsInStableOrMetaStableState()
  {
    return mDoingStableStates;
  }

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

  virtual bool IsSystemCaller() const = 0;

private:
  // A primary context owns the mRuntime. Non-main-thread contexts should always
  // be primary. On the main thread, the primary context should be the first one
  // created and the last one destroyed. Non-primary contexts are used for
  // cooperatively scheduled threads.
  bool mIsPrimaryContext;

  CycleCollectedJSRuntime* mRuntime;

  JSContext* mJSContext;

  nsCOMPtr<dom::Exception> mPendingException;
  nsThread* mOwningThread; // Manual refcounting to avoid include hell.

  struct PendingIDBTransactionData
  {
    nsCOMPtr<nsIRunnable> mTransaction;
    uint32_t mRecursionDepth;
  };

  nsTArray<nsCOMPtr<nsIRunnable>> mStableStateEvents;
  nsTArray<PendingIDBTransactionData> mPendingIDBTransactions;
  uint32_t mBaseRecursionDepth;
  bool mDoingStableStates;

  // If set to none 0, microtasks will be processed only when recursion depth
  // is the set value.
  uint32_t mTargetedMicroTaskRecursionDepth;

  uint32_t mMicroTaskLevel;

  std::queue<RefPtr<MicroTaskRunnable>> mPendingMicroTaskRunnables;
  std::queue<RefPtr<MicroTaskRunnable>> mDebuggerMicroTaskQueue;

  uint32_t mMicroTaskRecursionDepth;
};

class MOZ_STACK_CLASS nsAutoMicroTask
{
public:
  nsAutoMicroTask()
  {
    CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
    if (ccjs) {
      ccjs->EnterMicroTask();
    }
  }
  ~nsAutoMicroTask()
  {
    CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
    if (ccjs) {
      ccjs->LeaveMicroTask();
    }
  }
};

} // namespace mozilla

#endif // mozilla_CycleCollectedJSContext_h
