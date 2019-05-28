/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CycleCollectedJSContext_h
#define mozilla_CycleCollectedJSContext_h

#include <queue>

#include "mozilla/Attributes.h"
#include "mozilla/DeferredFinalize.h"
#include "mozilla/LinkedList.h"
#include "mozilla/mozalloc.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/AtomList.h"
#include "mozilla/dom/Promise.h"
#include "jsapi.h"
#include "js/Promise.h"

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
}  // namespace dom

// Contains various stats about the cycle collection.
struct CycleCollectorResults {
  CycleCollectorResults() {
    // Initialize here so when we increment mNumSlices the first time we're
    // not using uninitialized memory.
    Init();
  }

  void Init() {
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
  // mAnyManual is true if any slice was manually triggered, and at shutdown.
  bool mAnyManual;
  uint32_t mVisitedRefCounted;
  uint32_t mVisitedGCed;
  uint32_t mFreedRefCounted;
  uint32_t mFreedGCed;
  uint32_t mFreedJSZones;
  uint32_t mNumSlices;
};

class MicroTaskRunnable {
 public:
  MicroTaskRunnable() = default;
  NS_INLINE_DECL_REFCOUNTING(MicroTaskRunnable)
  MOZ_CAN_RUN_SCRIPT virtual void Run(AutoSlowOperation& aAso) = 0;
  virtual bool Suppressed() { return false; }

 protected:
  virtual ~MicroTaskRunnable() = default;
};

class CycleCollectedJSContext
    : dom::PerThreadAtomCache,
      public LinkedListElement<CycleCollectedJSContext>,
      private JS::JobQueue {
  friend class CycleCollectedJSRuntime;

 protected:
  CycleCollectedJSContext();
  virtual ~CycleCollectedJSContext();

  MOZ_IS_CLASS_INIT
  nsresult Initialize(JSRuntime* aParentRuntime, uint32_t aMaxBytes,
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
                                        JS::HandleObject aPromise,
                                        JS::HandleObject aJob,
                                        JS::HandleObject aAllocationSite,
                                        JS::HandleObject aIncumbentGlobal,
                                        void* aData);
  static void PromiseRejectionTrackerCallback(
      JSContext* aCx, JS::HandleObject aPromise,
      JS::PromiseRejectionHandlingState state, void* aData);

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

  CycleCollectedJSRuntime* Runtime() const {
    MOZ_ASSERT(mRuntime);
    return mRuntime;
  }

  already_AddRefed<dom::Exception> GetPendingException() const;
  void SetPendingException(dom::Exception* aException);

  std::queue<RefPtr<MicroTaskRunnable>>& GetMicroTaskQueue();
  std::queue<RefPtr<MicroTaskRunnable>>& GetDebuggerMicroTaskQueue();

  JSContext* Context() const {
    MOZ_ASSERT(mJSContext);
    return mJSContext;
  }

  JS::RootingContext* RootingCx() const {
    MOZ_ASSERT(mJSContext);
    return JS::RootingContext::get(mJSContext);
  }

  void SetTargetedMicroTaskRecursionDepth(uint32_t aDepth) {
    mTargetedMicroTaskRecursionDepth = aDepth;
  }

 protected:
  JSContext* MaybeContext() const { return mJSContext; }

 public:
  // nsThread entrypoints
  //
  // MOZ_CAN_RUN_SCRIPT_BOUNDARY so we don't need to annotate
  // nsThread::ProcessNextEvent and all its callers MOZ_CAN_RUN_SCRIPT for now.
  // But we really should!
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual void BeforeProcessTask(bool aMightBlock);
  // MOZ_CAN_RUN_SCRIPT_BOUNDARY so we don't need to annotate
  // nsThread::ProcessNextEvent and all its callers MOZ_CAN_RUN_SCRIPT for now.
  // But we really should!
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual void AfterProcessTask(uint32_t aRecursionDepth);

  // Check whether we need an idle GC task.
  void IsIdleGCTaskNeeded() const;

  uint32_t RecursionDepth() const;

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
  virtual void DispatchToMicroTask(
      already_AddRefed<MicroTaskRunnable> aRunnable);

  // Call EnterMicroTask when you're entering JS execution.
  // Usually the best way to do this is to use nsAutoMicroTask.
  void EnterMicroTask() { ++mMicroTaskLevel; }

  MOZ_CAN_RUN_SCRIPT
  void LeaveMicroTask() {
    if (--mMicroTaskLevel == 0) {
      PerformMicroTaskCheckPoint();
    }
  }

  bool IsInMicroTask() const { return mMicroTaskLevel != 0; }

  uint32_t MicroTaskLevel() const { return mMicroTaskLevel; }

  void SetMicroTaskLevel(uint32_t aLevel) { mMicroTaskLevel = aLevel; }

  MOZ_CAN_RUN_SCRIPT
  bool PerformMicroTaskCheckPoint(bool aForce = false);

  MOZ_CAN_RUN_SCRIPT
  void PerformDebuggerMicroTaskCheckpoint();

  bool IsInStableOrMetaStableState() const { return mDoingStableStates; }

  // Storage for watching rejected promises waiting for some client to
  // consume their rejection.
  // Promises in this list have been rejected in the last turn of the
  // event loop without the rejection being handled.
  // Note that this can contain nullptrs in place of promises removed because
  // they're consumed before it'd be reported.
  JS::PersistentRooted<JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>>
      mUncaughtRejections;

  // Promises in this list have previously been reported as rejected
  // (because they were in the above list), but the rejection was handled
  // in the last turn of the event loop.
  JS::PersistentRooted<JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>>
      mConsumedRejections;
  nsTArray<nsCOMPtr<nsISupports /* UncaughtRejectionObserver */>>
      mUncaughtRejectionObservers;

  virtual bool IsSystemCaller() const = 0;

 private:
  // JS::JobQueue implementation: see js/public/Promise.h.
  // SpiderMonkey uses some of these methods to enqueue promise resolution jobs.
  // Others protect the debuggee microtask queue from the debugger's
  // interruptions; see the comments on JS::AutoDebuggerJobQueueInterruption for
  // details.
  JSObject* getIncumbentGlobal(JSContext* cx) override;
  bool enqueuePromiseJob(JSContext* cx, JS::HandleObject promise,
                         JS::HandleObject job, JS::HandleObject allocationSite,
                         JS::HandleObject incumbentGlobal) override;
  // MOZ_CAN_RUN_SCRIPT_BOUNDARY for now so we don't have to change SpiderMonkey
  // headers.  The caller presumably knows this can run script (like everything
  // in SpiderMonkey!) and will deal.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void runJobs(JSContext* cx) override;
  bool empty() const override;
  class SavedMicroTaskQueue;
  js::UniquePtr<SavedJobQueue> saveJobQueue(JSContext*) override;

 private:
  // A primary context owns the mRuntime. Non-main-thread contexts should always
  // be primary. On the main thread, the primary context should be the first one
  // created and the last one destroyed. Non-primary contexts are used for
  // cooperatively scheduled threads.
  bool mIsPrimaryContext;

  CycleCollectedJSRuntime* mRuntime;

  JSContext* mJSContext;

  nsCOMPtr<dom::Exception> mPendingException;
  nsThread* mOwningThread;  // Manual refcounting to avoid include hell.

  struct PendingIDBTransactionData {
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

  // How many times the debugger has interrupted execution, possibly creating
  // microtask checkpoints in places that they would not normally occur.
  uint32_t mDebuggerRecursionDepth;

  uint32_t mMicroTaskRecursionDepth;

  // This implements about-to-be-notified rejected promises list in the spec.
  // https://html.spec.whatwg.org/multipage/webappapis.html#about-to-be-notified-rejected-promises-list
  typedef nsTArray<RefPtr<dom::Promise>> PromiseArray;
  PromiseArray mAboutToBeNotifiedRejectedPromises;

  // This is for the "outstanding rejected promises weak set" in the spec,
  // https://html.spec.whatwg.org/multipage/webappapis.html#outstanding-rejected-promises-weak-set
  // We use different data structure and opposite logic here to achieve the same
  // effect. Basically this is used for tracking the rejected promise that does
  // NOT need firing a rejectionhandled event. We will check the table to see if
  // firing rejectionhandled event is required when a rejected promise is being
  // handled.
  //
  // The rejected promise will be stored in the table if
  // - it is unhandled, and
  // - The unhandledrejection is not yet fired.
  //
  // And be removed when
  // - it is handled, or
  // - A unhandledrejection is fired and it isn't being handled in event
  // handler.
  typedef nsRefPtrHashtable<nsUint64HashKey, dom::Promise> PromiseHashtable;
  PromiseHashtable mPendingUnhandledRejections;

  class NotifyUnhandledRejections final : public CancelableRunnable {
   public:
    NotifyUnhandledRejections(CycleCollectedJSContext* aCx,
                              PromiseArray&& aPromises)
        : CancelableRunnable("NotifyUnhandledRejections"),
          mCx(aCx),
          mUnhandledRejections(std::move(aPromises)) {}

    NS_IMETHOD Run() final;

    nsresult Cancel() final;

   private:
    CycleCollectedJSContext* mCx;
    PromiseArray mUnhandledRejections;
  };
};

class MOZ_STACK_CLASS nsAutoMicroTask {
 public:
  nsAutoMicroTask() {
    CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
    if (ccjs) {
      ccjs->EnterMicroTask();
    }
  }
  MOZ_CAN_RUN_SCRIPT ~nsAutoMicroTask() {
    CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
    if (ccjs) {
      ccjs->LeaveMicroTask();
    }
  }
};

}  // namespace mozilla

#endif  // mozilla_CycleCollectedJSContext_h
