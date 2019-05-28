/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CycleCollectedJSContext.h"
#include <algorithm>
#include "mozilla/ArrayUtils.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/Move.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimelineConsumers.h"
#include "mozilla/TimelineMarker.h"
#include "mozilla/Unused.h"
#include "mozilla/DebuggerOnGCRunnable.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/PromiseRejectionEvent.h"
#include "mozilla/dom/PromiseRejectionEventBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsapi.h"
#include "js/Debug.h"
#include "js/GCAPI.h"
#include "js/Utility.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"
#include "nsDOMJSUtils.h"
#include "nsDOMMutationObserver.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"
#include "nsStringBuffer.h"

#include "nsThread.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

CycleCollectedJSContext::CycleCollectedJSContext()
    : mIsPrimaryContext(true),
      mRuntime(nullptr),
      mJSContext(nullptr),
      mDoingStableStates(false),
      mTargetedMicroTaskRecursionDepth(0),
      mMicroTaskLevel(0),
      mDebuggerRecursionDepth(0),
      mMicroTaskRecursionDepth(0) {
  MOZ_COUNT_CTOR(CycleCollectedJSContext);

  // Reinitialize PerThreadAtomCache because dom/bindings/Codegen.py compares
  // against zero rather than JSID_VOID to detect uninitialized jsid members.
  memset(static_cast<PerThreadAtomCache*>(this), 0, sizeof(PerThreadAtomCache));

  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  mOwningThread = thread.forget().downcast<nsThread>().take();
  MOZ_RELEASE_ASSERT(mOwningThread);
}

CycleCollectedJSContext::~CycleCollectedJSContext() {
  MOZ_COUNT_DTOR(CycleCollectedJSContext);
  // If the allocation failed, here we are.
  if (!mJSContext) {
    return;
  }

  JS_SetContextPrivate(mJSContext, nullptr);

  mRuntime->RemoveContext(this);

  if (mIsPrimaryContext) {
    mRuntime->Shutdown(mJSContext);
  }

  // Last chance to process any events.
  CleanupIDBTransactions(mBaseRecursionDepth);
  MOZ_ASSERT(mPendingIDBTransactions.IsEmpty());

  ProcessStableStateQueue();
  MOZ_ASSERT(mStableStateEvents.IsEmpty());

  // Clear mPendingException first, since it might be cycle collected.
  mPendingException = nullptr;

  MOZ_ASSERT(mDebuggerMicroTaskQueue.empty());
  MOZ_ASSERT(mPendingMicroTaskRunnables.empty());

  mUncaughtRejections.reset();
  mConsumedRejections.reset();

  JS_DestroyContext(mJSContext);
  mJSContext = nullptr;

  if (mIsPrimaryContext) {
    nsCycleCollector_forgetJSContext();
  } else {
    nsCycleCollector_forgetNonPrimaryContext();
  }

  mozilla::dom::DestroyScriptSettings();

  mOwningThread->SetScriptObserver(nullptr);
  NS_RELEASE(mOwningThread);

  if (mIsPrimaryContext) {
    delete mRuntime;
  }
  mRuntime = nullptr;
}

void CycleCollectedJSContext::InitializeCommon() {
  mRuntime->AddContext(this);

  mOwningThread->SetScriptObserver(this);
  // The main thread has a base recursion depth of 0, workers of 1.
  mBaseRecursionDepth = RecursionDepth();

  NS_GetCurrentThread()->SetCanInvokeJS(true);

  JS::SetJobQueue(mJSContext, this);
  JS::SetPromiseRejectionTrackerCallback(mJSContext,
                                         PromiseRejectionTrackerCallback, this);
  mUncaughtRejections.init(mJSContext,
                           JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>(
                               js::SystemAllocPolicy()));
  mConsumedRejections.init(mJSContext,
                           JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>(
                               js::SystemAllocPolicy()));

  // Cast to PerThreadAtomCache for dom::GetAtomCache(JSContext*).
  JS_SetContextPrivate(mJSContext, static_cast<PerThreadAtomCache*>(this));
}

nsresult CycleCollectedJSContext::Initialize(JSRuntime* aParentRuntime,
                                             uint32_t aMaxBytes,
                                             uint32_t aMaxNurseryBytes) {
  MOZ_ASSERT(!mJSContext);

  mozilla::dom::InitScriptSettings();
  mJSContext = JS_NewContext(aMaxBytes, aMaxNurseryBytes, aParentRuntime);
  if (!mJSContext) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mRuntime = CreateRuntime(mJSContext);

  InitializeCommon();

  nsCycleCollector_registerJSContext(this);

  return NS_OK;
}

nsresult CycleCollectedJSContext::InitializeNonPrimary(
    CycleCollectedJSContext* aPrimaryContext) {
  MOZ_ASSERT(!mJSContext);

  mIsPrimaryContext = false;

  mozilla::dom::InitScriptSettings();
  mJSContext = JS_NewCooperativeContext(aPrimaryContext->mJSContext);
  if (!mJSContext) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mRuntime = aPrimaryContext->mRuntime;

  InitializeCommon();

  nsCycleCollector_registerNonPrimaryContext(this);

  return NS_OK;
}

/* static */
CycleCollectedJSContext* CycleCollectedJSContext::GetFor(JSContext* aCx) {
  // Cast from void* matching JS_SetContextPrivate.
  auto atomCache = static_cast<PerThreadAtomCache*>(JS_GetContextPrivate(aCx));
  // Down cast.
  return static_cast<CycleCollectedJSContext*>(atomCache);
}

size_t CycleCollectedJSContext::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return 0;
}

class PromiseJobRunnable final : public MicroTaskRunnable {
 public:
  PromiseJobRunnable(JS::HandleObject aPromise, JS::HandleObject aCallback,
                     JS::HandleObject aCallbackGlobal,
                     JS::HandleObject aAllocationSite,
                     nsIGlobalObject* aIncumbentGlobal)
      : mCallback(new PromiseJobCallback(aCallback, aCallbackGlobal,
                                         aAllocationSite, aIncumbentGlobal)),
        mPropagateUserInputEventHandling(false) {
    MOZ_ASSERT(js::IsFunctionObject(aCallback));

    if (aPromise) {
      JS::PromiseUserInputEventHandlingState state =
          JS::GetPromiseUserInputEventHandlingState(aPromise);
      mPropagateUserInputEventHandling =
          state ==
          JS::PromiseUserInputEventHandlingState::HadUserInteractionAtCreation;
    }
  }

  virtual ~PromiseJobRunnable() {}

 protected:
  MOZ_CAN_RUN_SCRIPT
  virtual void Run(AutoSlowOperation& aAso) override {
    JSObject* callback = mCallback->CallbackPreserveColor();
    nsIGlobalObject* global = callback ? xpc::NativeGlobal(callback) : nullptr;
    if (global && !global->IsDying()) {
      // Propagate the user input event handling bit if needed.
      nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(global);
      RefPtr<Document> doc;
      if (win) {
        doc = win->GetExtantDoc();
      }
      AutoHandlingUserInputStatePusher userInpStatePusher(
          mPropagateUserInputEventHandling, nullptr, doc);

      mCallback->Call("promise callback");
      aAso.CheckForInterrupt();
    }
    // Now that mCallback is no longer needed, clear any pointers it contains to
    // JS GC things. This removes any storebuffer entries associated with those
    // pointers, which can cause problems by taking up memory and by triggering
    // minor GCs. This otherwise would not happen until the next minor GC or
    // cycle collection.
    mCallback->Reset();
  }

  virtual bool Suppressed() override {
    nsIGlobalObject* global =
        xpc::NativeGlobal(mCallback->CallbackPreserveColor());
    return global && global->IsInSyncOperation();
  }

 private:
  const RefPtr<PromiseJobCallback> mCallback;
  bool mPropagateUserInputEventHandling;
};

JSObject* CycleCollectedJSContext::getIncumbentGlobal(JSContext* aCx) {
  nsIGlobalObject* global = mozilla::dom::GetIncumbentGlobal();
  if (global) {
    return global->GetGlobalJSObject();
  }
  return nullptr;
}

bool CycleCollectedJSContext::enqueuePromiseJob(
    JSContext* aCx, JS::HandleObject aPromise, JS::HandleObject aJob,
    JS::HandleObject aAllocationSite, JS::HandleObject aIncumbentGlobal) {
  MOZ_ASSERT(aCx == Context());
  MOZ_ASSERT(Get() == this);

  nsIGlobalObject* global = nullptr;
  if (aIncumbentGlobal) {
    global = xpc::NativeGlobal(aIncumbentGlobal);
  }
  JS::RootedObject jobGlobal(aCx, JS::CurrentGlobalOrNull(aCx));
  RefPtr<PromiseJobRunnable> runnable = new PromiseJobRunnable(
      aPromise, aJob, jobGlobal, aAllocationSite, global);
  DispatchToMicroTask(runnable.forget());
  return true;
}

// Used only by the SpiderMonkey Debugger API, and even then only via
// JS::AutoDebuggerJobQueueInterruption, to ensure that the debuggee's queue is
// not affected; see comments in js/public/Promise.h.
void CycleCollectedJSContext::runJobs(JSContext* aCx) {
  MOZ_ASSERT(aCx == Context());
  MOZ_ASSERT(Get() == this);
  PerformMicroTaskCheckPoint();
}

bool CycleCollectedJSContext::empty() const {
  // This is our override of JS::JobQueue::empty. Since that interface is only
  // concerned with the ordinary microtask queue, not the debugger microtask
  // queue, we only report on the former.
  return mPendingMicroTaskRunnables.empty();
}

// Preserve a debuggee's microtask queue while it is interrupted by the
// debugger. See the comments for JS::AutoDebuggerJobQueueInterruption.
class CycleCollectedJSContext::SavedMicroTaskQueue
    : public JS::JobQueue::SavedJobQueue {
 public:
  explicit SavedMicroTaskQueue(CycleCollectedJSContext* ccjs) : ccjs(ccjs) {
    ccjs->mDebuggerRecursionDepth++;
    ccjs->mPendingMicroTaskRunnables.swap(mQueue);
  }

  ~SavedMicroTaskQueue() {
    MOZ_RELEASE_ASSERT(ccjs->mPendingMicroTaskRunnables.empty());
    MOZ_RELEASE_ASSERT(ccjs->mDebuggerRecursionDepth);
    ccjs->mDebuggerRecursionDepth--;
    ccjs->mPendingMicroTaskRunnables.swap(mQueue);
  }

 private:
  CycleCollectedJSContext* ccjs;
  std::queue<RefPtr<MicroTaskRunnable>> mQueue;
};

js::UniquePtr<JS::JobQueue::SavedJobQueue>
CycleCollectedJSContext::saveJobQueue(JSContext* cx) {
  auto saved = js::MakeUnique<SavedMicroTaskQueue>(this);
  if (!saved) {
    // When MakeUnique's allocation fails, the SavedMicroTaskQueue constructor
    // is never called, so mPendingMicroTaskRunnables is still initialized.
    JS_ReportOutOfMemory(cx);
    return nullptr;
  }

  return saved;
}

/* static */
void CycleCollectedJSContext::PromiseRejectionTrackerCallback(
    JSContext* aCx, JS::HandleObject aPromise,
    JS::PromiseRejectionHandlingState state, void* aData) {
  CycleCollectedJSContext* self = static_cast<CycleCollectedJSContext*>(aData);

  MOZ_ASSERT(aCx == self->Context());
  MOZ_ASSERT(Get() == self);

  // TODO: Bug 1549351 - Promise rejection event should not be sent for
  // cross-origin scripts

  PromiseArray& aboutToBeNotified = self->mAboutToBeNotifiedRejectedPromises;
  PromiseHashtable& unhandled = self->mPendingUnhandledRejections;
  uint64_t promiseID = JS::GetPromiseID(aPromise);

  if (state == JS::PromiseRejectionHandlingState::Unhandled) {
    PromiseDebugging::AddUncaughtRejection(aPromise);
    if (mozilla::StaticPrefs::dom_promise_rejection_events_enabled()) {
      RefPtr<Promise> promise =
          Promise::CreateFromExisting(xpc::NativeGlobal(aPromise), aPromise);
      aboutToBeNotified.AppendElement(promise);
      unhandled.Put(promiseID, promise);
    }
  } else {
    PromiseDebugging::AddConsumedRejection(aPromise);
    if (mozilla::StaticPrefs::dom_promise_rejection_events_enabled()) {
      for (size_t i = 0; i < aboutToBeNotified.Length(); i++) {
        if (aboutToBeNotified[i] &&
            aboutToBeNotified[i]->PromiseObj() == aPromise) {
          // To avoid large amounts of memmoves, we don't shrink the vector
          // here. Instead, we filter out nullptrs when iterating over the
          // vector later.
          aboutToBeNotified[i] = nullptr;
          DebugOnly<bool> isFound = unhandled.Remove(promiseID);
          MOZ_ASSERT(isFound);
          return;
        }
      }
      RefPtr<Promise> promise;
      unhandled.Remove(promiseID, getter_AddRefs(promise));
      if (!promise) {
        nsIGlobalObject* global = xpc::NativeGlobal(aPromise);
        if (nsCOMPtr<EventTarget> owner = do_QueryInterface(global)) {
          PromiseRejectionEventInit init;
          init.mPromise = Promise::CreateFromExisting(global, aPromise);
          init.mReason = JS::GetPromiseResult(aPromise);

          RefPtr<PromiseRejectionEvent> event =
              PromiseRejectionEvent::Constructor(
                  owner, NS_LITERAL_STRING("rejectionhandled"), init);

          RefPtr<AsyncEventDispatcher> asyncDispatcher =
              new AsyncEventDispatcher(owner, event);
          asyncDispatcher->PostDOMEvent();
        }
      }
    }
  }
}

already_AddRefed<Exception> CycleCollectedJSContext::GetPendingException()
    const {
  MOZ_ASSERT(mJSContext);

  nsCOMPtr<Exception> out = mPendingException;
  return out.forget();
}

void CycleCollectedJSContext::SetPendingException(Exception* aException) {
  MOZ_ASSERT(mJSContext);
  mPendingException = aException;
}

std::queue<RefPtr<MicroTaskRunnable>>&
CycleCollectedJSContext::GetMicroTaskQueue() {
  MOZ_ASSERT(mJSContext);
  return mPendingMicroTaskRunnables;
}

std::queue<RefPtr<MicroTaskRunnable>>&
CycleCollectedJSContext::GetDebuggerMicroTaskQueue() {
  MOZ_ASSERT(mJSContext);
  return mDebuggerMicroTaskQueue;
}

void CycleCollectedJSContext::ProcessStableStateQueue() {
  MOZ_ASSERT(mJSContext);
  MOZ_RELEASE_ASSERT(!mDoingStableStates);
  mDoingStableStates = true;

  // When run, one event can add another event to the mStableStateEvents, as
  // such you can't use iterators here.
  for (uint32_t i = 0; i < mStableStateEvents.Length(); ++i) {
    nsCOMPtr<nsIRunnable> event = mStableStateEvents[i].forget();
    event->Run();
  }

  mStableStateEvents.Clear();
  mDoingStableStates = false;
}

void CycleCollectedJSContext::CleanupIDBTransactions(uint32_t aRecursionDepth) {
  MOZ_ASSERT(mJSContext);
  MOZ_RELEASE_ASSERT(!mDoingStableStates);
  mDoingStableStates = true;

  nsTArray<PendingIDBTransactionData> localQueue =
      std::move(mPendingIDBTransactions);

  for (uint32_t i = 0; i < localQueue.Length(); ++i) {
    PendingIDBTransactionData& data = localQueue[i];
    if (data.mRecursionDepth != aRecursionDepth) {
      continue;
    }

    {
      nsCOMPtr<nsIRunnable> transaction = data.mTransaction.forget();
      transaction->Run();
    }

    localQueue.RemoveElementAt(i--);
  }

  // If the queue has events in it now, they were added from something we
  // called, so they belong at the end of the queue.
  localQueue.AppendElements(mPendingIDBTransactions);
  localQueue.SwapElements(mPendingIDBTransactions);
  mDoingStableStates = false;
}

void CycleCollectedJSContext::BeforeProcessTask(bool aMightBlock) {
  // If ProcessNextEvent was called during a microtask callback, we
  // must process any pending microtasks before blocking in the event loop,
  // otherwise we may deadlock until an event enters the queue later.
  if (aMightBlock && PerformMicroTaskCheckPoint()) {
    // If any microtask was processed, we post a dummy event in order to
    // force the ProcessNextEvent call not to block.  This is required
    // to support nested event loops implemented using a pattern like
    // "while (condition) thread.processNextEvent(true)", in case the
    // condition is triggered here by a Promise "then" callback.
    NS_DispatchToMainThread(new Runnable("BeforeProcessTask"));
  }
}

void CycleCollectedJSContext::AfterProcessTask(uint32_t aRecursionDepth) {
  MOZ_ASSERT(mJSContext);

  // See HTML 6.1.4.2 Processing model

  // Step 4.1: Execute microtasks.
  PerformMicroTaskCheckPoint();

  // Step 4.2 Execute any events that were waiting for a stable state.
  ProcessStableStateQueue();

  // This should be a fast test so that it won't affect the next task
  // processing.
  IsIdleGCTaskNeeded();
}

void CycleCollectedJSContext::AfterProcessMicrotasks() {
  MOZ_ASSERT(mJSContext);
  // Notify unhandled promise rejections:
  // https://html.spec.whatwg.org/multipage/webappapis.html#notify-about-rejected-promises
  if (mAboutToBeNotifiedRejectedPromises.Length()) {
    RefPtr<NotifyUnhandledRejections> runnable = new NotifyUnhandledRejections(
        this, std::move(mAboutToBeNotifiedRejectedPromises));
    NS_DispatchToCurrentThread(runnable);
  }
  // Cleanup Indexed Database transactions:
  // https://html.spec.whatwg.org/multipage/webappapis.html#perform-a-microtask-checkpoint
  CleanupIDBTransactions(RecursionDepth());
}

void CycleCollectedJSContext::IsIdleGCTaskNeeded() const {
  class IdleTimeGCTaskRunnable : public mozilla::IdleRunnable {
   public:
    using mozilla::IdleRunnable::IdleRunnable;

   public:
    NS_IMETHOD Run() override {
      CycleCollectedJSRuntime* ccrt = CycleCollectedJSRuntime::Get();
      if (ccrt) {
        ccrt->RunIdleTimeGCTask();
      }
      return NS_OK;
    }

    nsresult Cancel() override { return NS_OK; }
  };

  if (Runtime()->IsIdleGCTaskNeeded()) {
    nsCOMPtr<nsIRunnable> gc_task = new IdleTimeGCTaskRunnable();
    NS_DispatchToCurrentThreadQueue(gc_task.forget(), EventQueuePriority::Idle);
    Runtime()->SetPendingIdleGCTask();
  }
}

uint32_t CycleCollectedJSContext::RecursionDepth() const {
  // Debugger interruptions are included in the recursion depth so that debugger
  // microtask checkpoints do not run IDB transactions which were initiated
  // before the interruption.
  return mOwningThread->RecursionDepth() + mDebuggerRecursionDepth;
}

void CycleCollectedJSContext::RunInStableState(
    already_AddRefed<nsIRunnable>&& aRunnable) {
  MOZ_ASSERT(mJSContext);
  mStableStateEvents.AppendElement(std::move(aRunnable));
}

void CycleCollectedJSContext::AddPendingIDBTransaction(
    already_AddRefed<nsIRunnable>&& aTransaction) {
  MOZ_ASSERT(mJSContext);

  PendingIDBTransactionData data;
  data.mTransaction = aTransaction;

  MOZ_ASSERT(mOwningThread);
  data.mRecursionDepth = RecursionDepth();

  // There must be an event running to get here.
#ifndef MOZ_WIDGET_COCOA
  MOZ_ASSERT(data.mRecursionDepth > mBaseRecursionDepth);
#else
  // XXX bug 1261143
  // Recursion depth should be greater than mBaseRecursionDepth,
  // or the runnable will stay in the queue forever.
  if (data.mRecursionDepth <= mBaseRecursionDepth) {
    data.mRecursionDepth = mBaseRecursionDepth + 1;
  }
#endif

  mPendingIDBTransactions.AppendElement(std::move(data));
}

void CycleCollectedJSContext::DispatchToMicroTask(
    already_AddRefed<MicroTaskRunnable> aRunnable) {
  RefPtr<MicroTaskRunnable> runnable(aRunnable);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(runnable);

  JS::JobQueueMayNotBeEmpty(Context());
  mPendingMicroTaskRunnables.push(runnable.forget());
}

class AsyncMutationHandler final : public mozilla::Runnable {
 public:
  AsyncMutationHandler() : mozilla::Runnable("AsyncMutationHandler") {}

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.  See
  // bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD Run() override {
    CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
    if (ccjs) {
      ccjs->PerformMicroTaskCheckPoint();
    }
    return NS_OK;
  }
};

bool CycleCollectedJSContext::PerformMicroTaskCheckPoint(bool aForce) {
  if (mPendingMicroTaskRunnables.empty() && mDebuggerMicroTaskQueue.empty()) {
    AfterProcessMicrotasks();
    // Nothing to do, return early.
    return false;
  }

  uint32_t currentDepth = RecursionDepth();
  if (mMicroTaskRecursionDepth >= currentDepth && !aForce) {
    // We are already executing microtasks for the current recursion depth.
    return false;
  }

  if (mTargetedMicroTaskRecursionDepth != 0 &&
      mTargetedMicroTaskRecursionDepth != currentDepth) {
    return false;
  }

  if (NS_IsMainThread() && !nsContentUtils::IsSafeToRunScript()) {
    // Special case for main thread where DOM mutations may happen when
    // it is not safe to run scripts.
    nsContentUtils::AddScriptRunner(new AsyncMutationHandler());
    return false;
  }

  mozilla::AutoRestore<uint32_t> restore(mMicroTaskRecursionDepth);
  MOZ_ASSERT(aForce ? currentDepth == 0 : currentDepth > 0);
  mMicroTaskRecursionDepth = currentDepth;

  bool didProcess = false;
  AutoSlowOperation aso;

  std::queue<RefPtr<MicroTaskRunnable>> suppressed;
  for (;;) {
    RefPtr<MicroTaskRunnable> runnable;
    if (!mDebuggerMicroTaskQueue.empty()) {
      runnable = mDebuggerMicroTaskQueue.front().forget();
      mDebuggerMicroTaskQueue.pop();
    } else if (!mPendingMicroTaskRunnables.empty()) {
      runnable = mPendingMicroTaskRunnables.front().forget();
      mPendingMicroTaskRunnables.pop();
    } else {
      break;
    }

    if (runnable->Suppressed()) {
      // Microtasks in worker shall never be suppressed.
      // Otherwise, mPendingMicroTaskRunnables will be replaced later with
      // all suppressed tasks in mDebuggerMicroTaskQueue unexpectedly.
      MOZ_ASSERT(NS_IsMainThread());
      JS::JobQueueMayNotBeEmpty(Context());
      suppressed.push(runnable);
    } else {
      if (mPendingMicroTaskRunnables.empty() &&
          mDebuggerMicroTaskQueue.empty() && suppressed.empty()) {
        JS::JobQueueIsEmpty(Context());
      }
      didProcess = true;
      runnable->Run(aso);
    }
  }

  // Put back the suppressed microtasks so that they will be run later.
  // Note, it is possible that we end up keeping these suppressed tasks around
  // for some time, but no longer than spinning the event loop nestedly
  // (sync XHR, alert, etc.)
  mPendingMicroTaskRunnables.swap(suppressed);

  AfterProcessMicrotasks();

  return didProcess;
}

void CycleCollectedJSContext::PerformDebuggerMicroTaskCheckpoint() {
  // Don't do normal microtask handling checks here, since whoever is calling
  // this method is supposed to know what they are doing.

  AutoSlowOperation aso;
  for (;;) {
    // For a debugger microtask checkpoint, we always use the debugger microtask
    // queue.
    std::queue<RefPtr<MicroTaskRunnable>>* microtaskQueue =
        &GetDebuggerMicroTaskQueue();

    if (microtaskQueue->empty()) {
      break;
    }

    RefPtr<MicroTaskRunnable> runnable = microtaskQueue->front().forget();
    MOZ_ASSERT(runnable);

    // This function can re-enter, so we remove the element before calling.
    microtaskQueue->pop();

    if (mPendingMicroTaskRunnables.empty() && mDebuggerMicroTaskQueue.empty()) {
      JS::JobQueueIsEmpty(Context());
    }
    runnable->Run(aso);
  }

  AfterProcessMicrotasks();
}

NS_IMETHODIMP CycleCollectedJSContext::NotifyUnhandledRejections::Run() {
  MOZ_ASSERT(mozilla::StaticPrefs::dom_promise_rejection_events_enabled());

  for (size_t i = 0; i < mUnhandledRejections.Length(); ++i) {
    RefPtr<Promise>& promise = mUnhandledRejections[i];
    if (!promise) {
      continue;
    }

    JS::RootedObject promiseObj(mCx->RootingCx(), promise->PromiseObj());
    MOZ_ASSERT(JS::IsPromiseObject(promiseObj));

    // Only fire unhandledrejection if the promise is still not handled;
    uint64_t promiseID = JS::GetPromiseID(promiseObj);
    if (!JS::GetPromiseIsHandled(promiseObj)) {
      if (nsCOMPtr<EventTarget> target =
              do_QueryInterface(promise->GetParentObject())) {
        PromiseRejectionEventInit init;
        init.mPromise = promise;
        init.mReason = JS::GetPromiseResult(promiseObj);
        init.mCancelable = true;

        RefPtr<PromiseRejectionEvent> event =
            PromiseRejectionEvent::Constructor(
                target, NS_LITERAL_STRING("unhandledrejection"), init);
        // We don't use the result of dispatching event here to check whether to
        // report the Promise to console.
        target->DispatchEvent(*event);
      }
    }

    if (!JS::GetPromiseIsHandled(promiseObj)) {
      DebugOnly<bool> isFound =
          mCx->mPendingUnhandledRejections.Remove(promiseID);
      MOZ_ASSERT(isFound);
    }

    // If a rejected promise is being handled in "unhandledrejection" event
    // handler, it should be removed from the table in
    // PromiseRejectionTrackerCallback.
    MOZ_ASSERT(!mCx->mPendingUnhandledRejections.Lookup(promiseID));
  }
  return NS_OK;
}

nsresult CycleCollectedJSContext::NotifyUnhandledRejections::Cancel() {
  MOZ_ASSERT(mozilla::StaticPrefs::dom_promise_rejection_events_enabled());

  for (size_t i = 0; i < mUnhandledRejections.Length(); ++i) {
    RefPtr<Promise>& promise = mUnhandledRejections[i];
    if (!promise) {
      continue;
    }

    JS::RootedObject promiseObj(mCx->RootingCx(), promise->PromiseObj());
    mCx->mPendingUnhandledRejections.Remove(JS::GetPromiseID(promiseObj));
  }
  return NS_OK;
}
}  // namespace mozilla
