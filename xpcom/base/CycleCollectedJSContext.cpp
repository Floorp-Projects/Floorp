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
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseBinding.h"
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsprf.h"
#include "js/Debug.h"
#include "js/GCAPI.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"
#include "nsDOMJSUtils.h"
#include "nsJSUtils.h"
#include "nsWrapperCache.h"
#include "nsStringBuffer.h"

#ifdef MOZ_CRASHREPORTER
#include "nsExceptionHandler.h"
#endif

#include "nsIException.h"
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
  , mDisableMicroTaskCheckpoint(false)
{
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  mOwningThread = thread.forget().downcast<nsThread>().take();
  MOZ_RELEASE_ASSERT(mOwningThread);
}

CycleCollectedJSContext::~CycleCollectedJSContext()
{
  // If the allocation failed, here we are.
  if (!mJSContext) {
    return;
  }

  mRuntime->RemoveContext(this);

  if (mIsPrimaryContext) {
    mRuntime->Shutdown(mJSContext);
  }

  // Last chance to process any events.
  ProcessMetastableStateQueue(mBaseRecursionDepth);
  MOZ_ASSERT(mMetastableStateEvents.IsEmpty());

  ProcessStableStateQueue();
  MOZ_ASSERT(mStableStateEvents.IsEmpty());

  // Clear mPendingException first, since it might be cycle collected.
  mPendingException = nullptr;

  MOZ_ASSERT(mDebuggerPromiseMicroTaskQueue.empty());
  MOZ_ASSERT(mPromiseMicroTaskQueue.empty());

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

size_t
CycleCollectedJSContext::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  return 0;
}

class PromiseJobRunnable final : public Runnable
{
public:
  PromiseJobRunnable(JS::HandleObject aCallback, JS::HandleObject aAllocationSite,
                     nsIGlobalObject* aIncumbentGlobal)
    : mCallback(new PromiseJobCallback(aCallback, aAllocationSite, aIncumbentGlobal))
  {
  }

  virtual ~PromiseJobRunnable()
  {
  }

protected:
  NS_IMETHOD
  Run() override
  {
    JSObject* callback = mCallback->CallbackPreserveColor();
    nsIGlobalObject* global = callback ? xpc::NativeGlobal(callback) : nullptr;
    if (global && !global->IsDying()) {
      mCallback->Call("promise callback");
    }
    return NS_OK;
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
  nsCOMPtr<nsIRunnable> runnable = new PromiseJobRunnable(aJob, aAllocationSite, global);
  self->DispatchToMicroTask(runnable.forget());
  return true;
}

/* static */
void
CycleCollectedJSContext::PromiseRejectionTrackerCallback(JSContext* aCx,
                                                         JS::HandleObject aPromise,
                                                         PromiseRejectionHandlingState state,
                                                         void* aData)
{
#ifdef DEBUG
  CycleCollectedJSContext* self = static_cast<CycleCollectedJSContext*>(aData);
#endif // DEBUG
  MOZ_ASSERT(aCx == self->Context());
  MOZ_ASSERT(Get() == self);

  if (state == PromiseRejectionHandlingState::Unhandled) {
    PromiseDebugging::AddUncaughtRejection(aPromise);
  } else {
    PromiseDebugging::AddConsumedRejection(aPromise);
  }
}

already_AddRefed<nsIException>
CycleCollectedJSContext::GetPendingException() const
{
  MOZ_ASSERT(mJSContext);

  nsCOMPtr<nsIException> out = mPendingException;
  return out.forget();
}

void
CycleCollectedJSContext::SetPendingException(nsIException* aException)
{
  MOZ_ASSERT(mJSContext);
  mPendingException = aException;
}

std::queue<nsCOMPtr<nsIRunnable>>&
CycleCollectedJSContext::GetPromiseMicroTaskQueue()
{
  MOZ_ASSERT(mJSContext);
  return mPromiseMicroTaskQueue;
}

std::queue<nsCOMPtr<nsIRunnable>>&
CycleCollectedJSContext::GetDebuggerPromiseMicroTaskQueue()
{
  MOZ_ASSERT(mJSContext);
  return mDebuggerPromiseMicroTaskQueue;
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
CycleCollectedJSContext::ProcessMetastableStateQueue(uint32_t aRecursionDepth)
{
  MOZ_ASSERT(mJSContext);
  MOZ_RELEASE_ASSERT(!mDoingStableStates);
  mDoingStableStates = true;

  nsTArray<RunInMetastableStateData> localQueue = Move(mMetastableStateEvents);

  for (uint32_t i = 0; i < localQueue.Length(); ++i)
  {
    RunInMetastableStateData& data = localQueue[i];
    if (data.mRecursionDepth != aRecursionDepth) {
      continue;
    }

    {
      nsCOMPtr<nsIRunnable> runnable = data.mRunnable.forget();
      runnable->Run();
    }

    localQueue.RemoveElementAt(i--);
  }

  // If the queue has events in it now, they were added from something we called,
  // so they belong at the end of the queue.
  localQueue.AppendElements(mMetastableStateEvents);
  localQueue.SwapElements(mMetastableStateEvents);
  mDoingStableStates = false;
}

void
CycleCollectedJSContext::AfterProcessTask(uint32_t aRecursionDepth)
{
  MOZ_ASSERT(mJSContext);

  // See HTML 6.1.4.2 Processing model

  // Execute any events that were waiting for a microtask to complete.
  // This is not (yet) in the spec.
  ProcessMetastableStateQueue(aRecursionDepth);

  // Step 4.1: Execute microtasks.
  if (!mDisableMicroTaskCheckpoint) {
    if (NS_IsMainThread()) {
      nsContentUtils::PerformMainThreadMicroTaskCheckpoint();
      Promise::PerformMicroTaskCheckpoint();
    } else {
      Promise::PerformWorkerMicroTaskCheckpoint();
    }
  }

  // Step 4.2 Execute any events that were waiting for a stable state.
  ProcessStableStateQueue();
}

void
CycleCollectedJSContext::AfterProcessMicrotask()
{
  MOZ_ASSERT(mJSContext);
  AfterProcessMicrotask(RecursionDepth());
}

void
CycleCollectedJSContext::AfterProcessMicrotask(uint32_t aRecursionDepth)
{
  MOZ_ASSERT(mJSContext);

  // Between microtasks, execute any events that were waiting for a microtask
  // to complete.
  ProcessMetastableStateQueue(aRecursionDepth);
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
  mStableStateEvents.AppendElement(Move(aRunnable));
}

void
CycleCollectedJSContext::RunInMetastableState(already_AddRefed<nsIRunnable>&& aRunnable)
{
  MOZ_ASSERT(mJSContext);

  RunInMetastableStateData data;
  data.mRunnable = aRunnable;

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

  mMetastableStateEvents.AppendElement(Move(data));
}

void
CycleCollectedJSContext::DispatchToMicroTask(already_AddRefed<nsIRunnable> aRunnable)
{
  RefPtr<nsIRunnable> runnable(aRunnable);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(runnable);

  mPromiseMicroTaskQueue.push(runnable.forget());
}

// All these functions just delegate to the runtime.

void
CycleCollectedJSContext::FinalizeDeferredThings(DeferredFinalizeType aType)
{
  Runtime()->FinalizeDeferredThings(aType);
}

void
CycleCollectedJSContext::AddJSHolder(void* aHolder, nsScriptObjectTracer* aTracer)
{
  Runtime()->AddJSHolder(aHolder, aTracer);
}

void
CycleCollectedJSContext::RemoveJSHolder(void* aHolder)
{
  Runtime()->RemoveJSHolder(aHolder);
}

#ifdef DEBUG
bool
CycleCollectedJSContext::IsJSHolder(void* aHolder)
{
  return Runtime()->IsJSHolder(aHolder);
}

void
CycleCollectedJSContext::AssertNoObjectsToTrace(void* aPossibleJSHolder)
{
  Runtime()->AssertNoObjectsToTrace(aPossibleJSHolder);
}
#endif

nsCycleCollectionParticipant*
CycleCollectedJSContext::GCThingParticipant()
{
  return Runtime()->GCThingParticipant();
}

nsCycleCollectionParticipant*
CycleCollectedJSContext::ZoneParticipant()
{
  return Runtime()->ZoneParticipant();
}

nsresult
CycleCollectedJSContext::TraverseRoots(nsCycleCollectionNoteRootCallback& aCb)
{
  return Runtime()->TraverseRoots(aCb);
}

bool
CycleCollectedJSContext::UsefulToMergeZones() const
{
  return Runtime()->UsefulToMergeZones();
}

void
CycleCollectedJSContext::FixWeakMappingGrayBits() const
{
  Runtime()->FixWeakMappingGrayBits();
}

bool
CycleCollectedJSContext::AreGCGrayBitsValid() const
{
  return Runtime()->AreGCGrayBitsValid();
}

void
CycleCollectedJSContext::GarbageCollect(uint32_t aReason) const
{
  Runtime()->GarbageCollect(aReason);
}

void
CycleCollectedJSContext::NurseryWrapperAdded(nsWrapperCache* aCache)
{
  Runtime()->NurseryWrapperAdded(aCache);
}

void
CycleCollectedJSContext::NurseryWrapperPreserved(JSObject* aWrapper)
{
  Runtime()->NurseryWrapperPreserved(aWrapper);
}

void
CycleCollectedJSContext::JSObjectsTenured()
{
  Runtime()->JSObjectsTenured();
}

void
CycleCollectedJSContext::DeferredFinalize(DeferredFinalizeAppendFunction aAppendFunc,
                                          DeferredFinalizeFunction aFunc,
                                          void* aThing)
{
  Runtime()->DeferredFinalize(aAppendFunc, aFunc, aThing);
}

void
CycleCollectedJSContext::DeferredFinalize(nsISupports* aSupports)
{
  Runtime()->DeferredFinalize(aSupports);
}

void
CycleCollectedJSContext::DumpJSHeap(FILE* aFile)
{
  Runtime()->DumpJSHeap(aFile);
}

void
CycleCollectedJSContext::AddZoneWaitingForGC(JS::Zone* aZone)
{
  Runtime()->AddZoneWaitingForGC(aZone);
}

void
CycleCollectedJSContext::PrepareWaitingZonesForGC()
{
  Runtime()->PrepareWaitingZonesForGC();
}

void
CycleCollectedJSContext::PrepareForForgetSkippable()
{
  Runtime()->PrepareForForgetSkippable();
}

void
CycleCollectedJSContext::BeginCycleCollectionCallback()
{
  Runtime()->BeginCycleCollectionCallback();
}

void
CycleCollectedJSContext::EndCycleCollectionCallback(CycleCollectorResults& aResults)
{
  Runtime()->EndCycleCollectionCallback(aResults);
}

void
CycleCollectedJSContext::DispatchDeferredDeletion(bool aContinuation, bool aPurge)
{
  Runtime()->DispatchDeferredDeletion(aContinuation, aPurge);
}

} // namespace mozilla
