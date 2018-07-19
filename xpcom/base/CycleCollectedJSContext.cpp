/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CycleCollectedJSContext.h"
#include <algorithm>
#include "mozilla/ArrayUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/CycleCollectedJSRuntime.h"
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
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/ScriptSettings.h"
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
#include "nsWrapperCache.h"
#include "nsStringBuffer.h"

#include "nsIPlatformInfo.h"
#include "nsThread.h"
#include "nsThreadUtils.h"
#include "xpcpublic.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

CycleCollectedJSContext::CycleCollectedJSContext()
  : mIsPrimaryContext(true)
  , mRuntime(nullptr)
  , mJSContext(nullptr)
  , mDoingStableStates(false)
  , mTargetedMicroTaskRecursionDepth(0)
  , mMicroTaskLevel(0)
  , mMicroTaskRecursionDepth(0)
{
  MOZ_COUNT_CTOR(CycleCollectedJSContext);

  // Reinitialize PerThreadAtomCache because dom/bindings/Codegen.py compares
  // against zero rather than JSID_VOID to detect uninitialized jsid members.
  memset(static_cast<PerThreadAtomCache*>(this), 0, sizeof(PerThreadAtomCache));

  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  mOwningThread = thread.forget().downcast<nsThread>().take();
  MOZ_RELEASE_ASSERT(mOwningThread);
}

CycleCollectedJSContext::~CycleCollectedJSContext()
{
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

void
CycleCollectedJSContext::InitializeCommon()
{
  mRuntime->AddContext(this);

  mOwningThread->SetScriptObserver(this);
  // The main thread has a base recursion depth of 0, workers of 1.
  mBaseRecursionDepth = RecursionDepth();

  NS_GetCurrentThread()->SetCanInvokeJS(true);

  JS::SetGetIncumbentGlobalCallback(mJSContext, GetIncumbentGlobalCallback);

  JS::SetEnqueuePromiseJobCallback(mJSContext, EnqueuePromiseJobCallback, this);
  JS::SetPromiseRejectionTrackerCallback(mJSContext, PromiseRejectionTrackerCallback, this);
  mUncaughtRejections.init(mJSContext, JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>(js::SystemAllocPolicy()));
  mConsumedRejections.init(mJSContext, JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>(js::SystemAllocPolicy()));

  // Cast to PerThreadAtomCache for dom::GetAtomCache(JSContext*).
  JS_SetContextPrivate(mJSContext, static_cast<PerThreadAtomCache*>(this));
}

nsresult
CycleCollectedJSContext::Initialize(JSRuntime* aParentRuntime,
                                    uint32_t aMaxBytes,
                                    uint32_t aMaxNurseryBytes)
{
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

nsresult
CycleCollectedJSContext::InitializeNonPrimary(CycleCollectedJSContext* aPrimaryContext)
{
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

/* static */ CycleCollectedJSContext*
CycleCollectedJSContext::GetFor(JSContext* aCx)
{
  // Cast from void* matching JS_SetContextPrivate.
  auto atomCache = static_cast<PerThreadAtomCache*>(JS_GetContextPrivate(aCx));
  // Down cast.
  return static_cast<CycleCollectedJSContext*>(atomCache);
}

size_t
CycleCollectedJSContext::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return 0;
}

class PromiseJobRunnable final : public MicroTaskRunnable
{
public:
  PromiseJobRunnable(JS::HandleObject aCallback,
                     JS::HandleObject aAllocationSite,
                     nsIGlobalObject* aIncumbentGlobal)
    :mCallback(
       new PromiseJobCallback(aCallback, aAllocationSite, aIncumbentGlobal))
  {
  }

  virtual ~PromiseJobRunnable()
  {
  }

protected:
  virtual void Run(AutoSlowOperation& aAso) override
  {
    JSObject* callback = mCallback->CallbackPreserveColor();
    nsIGlobalObject* global = callback ? xpc::NativeGlobal(callback) : nullptr;
    if (global && !global->IsDying()) {
      mCallback->Call("promise callback");
      aAso.CheckForInterrupt();
    }
  }

  virtual bool Suppressed() override
  {
    nsIGlobalObject* global =
      xpc::NativeGlobal(mCallback->CallbackPreserveColor());
    return global && global->IsInSyncOperation();
  }

private:
  RefPtr<PromiseJobCallback> mCallback;
};

/* static */
JSObject*
CycleCollectedJSContext::GetIncumbentGlobalCallback(JSContext* aCx)
{
  nsIGlobalObject* global = mozilla::dom::GetIncumbentGlobal();
  if (global) {
    return global->GetGlobalJSObject();
  }
  return nullptr;
}

/* static */
bool
CycleCollectedJSContext::EnqueuePromiseJobCallback(JSContext* aCx,
                                                   JS::HandleObject aJob,
                                                   JS::HandleObject aAllocationSite,
                                                   JS::HandleObject aIncumbentGlobal,
                                                   void* aData)
{
  CycleCollectedJSContext* self = static_cast<CycleCollectedJSContext*>(aData);
  MOZ_ASSERT(aCx == self->Context());
  MOZ_ASSERT(Get() == self);

  nsIGlobalObject* global = nullptr;
  if (aIncumbentGlobal) {
    global = xpc::NativeGlobal(aIncumbentGlobal);
  }
  RefPtr<MicroTaskRunnable> runnable = new PromiseJobRunnable(aJob, aAllocationSite, global);
  self->DispatchToMicroTask(runnable.forget());
  return true;
}

/* static */
void
CycleCollectedJSContext::PromiseRejectionTrackerCallback(JSContext* aCx,
                                                         JS::HandleObject aPromise,
                                                         JS::PromiseRejectionHandlingState state,
                                                         void* aData)
{
#ifdef DEBUG
  CycleCollectedJSContext* self = static_cast<CycleCollectedJSContext*>(aData);
#endif // DEBUG
  MOZ_ASSERT(aCx == self->Context());
  MOZ_ASSERT(Get() == self);

  if (state == JS::PromiseRejectionHandlingState::Unhandled) {
    PromiseDebugging::AddUncaughtRejection(aPromise);
  } else {
    PromiseDebugging::AddConsumedRejection(aPromise);
  }
}

already_AddRefed<Exception>
CycleCollectedJSContext::GetPendingException() const
{
  MOZ_ASSERT(mJSContext);

  nsCOMPtr<Exception> out = mPendingException;
  return out.forget();
}

void
CycleCollectedJSContext::SetPendingException(Exception* aException)
{
  MOZ_ASSERT(mJSContext);
  mPendingException = aException;
}

std::queue<RefPtr<MicroTaskRunnable>>&
CycleCollectedJSContext::GetMicroTaskQueue()
{
  MOZ_ASSERT(mJSContext);
  return mPendingMicroTaskRunnables;
}

std::queue<RefPtr<MicroTaskRunnable>>&
CycleCollectedJSContext::GetDebuggerMicroTaskQueue()
{
  MOZ_ASSERT(mJSContext);
  return mDebuggerMicroTaskQueue;
}

void
CycleCollectedJSContext::ProcessStableStateQueue()
{
  MOZ_ASSERT(mJSContext);
  MOZ_RELEASE_ASSERT(!mDoingStableStates);
  mDoingStableStates = true;

  for (uint32_t i = 0; i < mStableStateEvents.Length(); ++i) {
    nsCOMPtr<nsIRunnable> event = mStableStateEvents[i].forget();
    event->Run();
  }

  mStableStateEvents.Clear();
  mDoingStableStates = false;
}

void
CycleCollectedJSContext::CleanupIDBTransactions(uint32_t aRecursionDepth)
{
  MOZ_ASSERT(mJSContext);
  MOZ_RELEASE_ASSERT(!mDoingStableStates);
  mDoingStableStates = true;

  nsTArray<PendingIDBTransactionData> localQueue = std::move(mPendingIDBTransactions);

  for (uint32_t i = 0; i < localQueue.Length(); ++i)
  {
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

  // If the queue has events in it now, they were added from something we called,
  // so they belong at the end of the queue.
  localQueue.AppendElements(mPendingIDBTransactions);
  localQueue.SwapElements(mPendingIDBTransactions);
  mDoingStableStates = false;
}

void
CycleCollectedJSContext::BeforeProcessTask(bool aMightBlock)
{
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

void
CycleCollectedJSContext::AfterProcessTask(uint32_t aRecursionDepth)
{
  MOZ_ASSERT(mJSContext);

  // See HTML 6.1.4.2 Processing model

  // Step 4.1: Execute microtasks.
  PerformMicroTaskCheckPoint();

  // Step 4.2 Execute any events that were waiting for a stable state.
  ProcessStableStateQueue();

  // This should be a fast test so that it won't affect the next task processing.
  IsIdleGCTaskNeeded();
}

void
CycleCollectedJSContext::AfterProcessMicrotasks()
{
  MOZ_ASSERT(mJSContext);
  // Cleanup Indexed Database transactions:
  // https://html.spec.whatwg.org/multipage/webappapis.html#perform-a-microtask-checkpoint
  CleanupIDBTransactions(RecursionDepth());
}

void CycleCollectedJSContext::IsIdleGCTaskNeeded()
{
  class IdleTimeGCTaskRunnable : public mozilla::IdleRunnable
  {
  public:
    using mozilla::IdleRunnable::IdleRunnable;

  public:
    NS_IMETHOD Run() override
    {
      CycleCollectedJSRuntime* ccrt = CycleCollectedJSRuntime::Get();
      if (ccrt) {
        ccrt->RunIdleTimeGCTask();
      }
      return NS_OK;
    }

    nsresult Cancel() override
    {
      return NS_OK;
    }
  };

  if (Runtime()->IsIdleGCTaskNeeded()) {
    nsCOMPtr<nsIRunnable> gc_task = new IdleTimeGCTaskRunnable();
    NS_IdleDispatchToCurrentThread(gc_task.forget());
    Runtime()->SetPendingIdleGCTask();
  }
}

uint32_t
CycleCollectedJSContext::RecursionDepth()
{
  return mOwningThread->RecursionDepth();
}

void
CycleCollectedJSContext::RunInStableState(already_AddRefed<nsIRunnable>&& aRunnable)
{
  MOZ_ASSERT(mJSContext);
  mStableStateEvents.AppendElement(std::move(aRunnable));
}

void
CycleCollectedJSContext::AddPendingIDBTransaction(already_AddRefed<nsIRunnable>&& aTransaction)
{
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

void
CycleCollectedJSContext::DispatchToMicroTask(
    already_AddRefed<MicroTaskRunnable> aRunnable)
{
  RefPtr<MicroTaskRunnable> runnable(aRunnable);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(runnable);

  mPendingMicroTaskRunnables.push(runnable.forget());
}

class AsyncMutationHandler final : public mozilla::Runnable
{
public:
  AsyncMutationHandler() : mozilla::Runnable("AsyncMutationHandler") {}

  NS_IMETHOD Run() override
  {
    CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
    if (ccjs) {
      ccjs->PerformMicroTaskCheckPoint();
    }
    return NS_OK;
  }
};

bool
CycleCollectedJSContext::PerformMicroTaskCheckPoint(bool aForce)
{
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
      suppressed.push(runnable);
    } else {
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

void
CycleCollectedJSContext::PerformDebuggerMicroTaskCheckpoint()
 {
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
    runnable->Run(aso);
  }

  AfterProcessMicrotasks();
}
} // namespace mozilla
