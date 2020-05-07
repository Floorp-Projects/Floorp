/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadEventTarget.h"
#include "mozilla/ThreadEventQueue.h"

#include "LeakRefPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "nsThreadManager.h"
#include "nsThreadSyncDispatch.h"
#include "nsThreadUtils.h"
#include "nsXPCOMPrivate.h"  // for gXPCOMThreadsShutDown
#include "ThreadDelay.h"

#ifdef MOZ_TASK_TRACER
#  include "GeckoTaskTracer.h"
#  include "TracedTaskCommon.h"
using namespace mozilla::tasktracer;
#endif

using namespace mozilla;

namespace {

class DelayedRunnable : public Runnable, public nsITimerCallback {
 public:
  DelayedRunnable(already_AddRefed<nsIEventTarget> aTarget,
                  already_AddRefed<nsIRunnable> aRunnable, uint32_t aDelay)
      : mozilla::Runnable("DelayedRunnable"),
        mTarget(aTarget),
        mWrappedRunnable(aRunnable),
        mDelayedFrom(TimeStamp::NowLoRes()),
        mDelay(aDelay) {}

  NS_DECL_ISUPPORTS_INHERITED

  nsresult Init() {
    return NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, mDelay,
                                   nsITimer::TYPE_ONE_SHOT, mTarget);
  }

  nsresult DoRun() {
    nsCOMPtr<nsIRunnable> r = std::move(mWrappedRunnable);
    return r->Run();
  }

  NS_IMETHOD Run() override {
    // Already ran?
    if (!mWrappedRunnable) {
      return NS_OK;
    }

    // Are we too early?
    if ((TimeStamp::NowLoRes() - mDelayedFrom).ToMilliseconds() < mDelay) {
      return NS_OK;  // Let the nsITimer run us.
    }

    mTimer->Cancel();
    return DoRun();
  }

  NS_IMETHOD Notify(nsITimer* aTimer) override {
    // If we already ran, the timer should have been canceled.
    MOZ_ASSERT(mWrappedRunnable);
    MOZ_ASSERT(aTimer == mTimer);

    return DoRun();
  }

 private:
  ~DelayedRunnable() = default;

  nsCOMPtr<nsIEventTarget> mTarget;
  nsCOMPtr<nsIRunnable> mWrappedRunnable;
  nsCOMPtr<nsITimer> mTimer;
  TimeStamp mDelayedFrom;
  uint32_t mDelay;
};

NS_IMPL_ISUPPORTS_INHERITED(DelayedRunnable, Runnable, nsITimerCallback)

}  // anonymous namespace

ThreadEventTarget::ThreadEventTarget(ThreadTargetSink* aSink,
                                     bool aIsMainThread)
    : mSink(aSink), mIsMainThread(aIsMainThread) {
  mThread = PR_GetCurrentThread();
}

void ThreadEventTarget::SetCurrentThread() { mThread = PR_GetCurrentThread(); }

void ThreadEventTarget::ClearCurrentThread() { mThread = nullptr; }

NS_IMPL_ISUPPORTS(ThreadEventTarget, nsIEventTarget, nsISerialEventTarget)

NS_IMETHODIMP
ThreadEventTarget::DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags) {
  return Dispatch(do_AddRef(aRunnable), aFlags);
}

NS_IMETHODIMP
ThreadEventTarget::Dispatch(already_AddRefed<nsIRunnable> aEvent,
                            uint32_t aFlags) {
  // We want to leak the reference when we fail to dispatch it, so that
  // we won't release the event in a wrong thread.
  LeakRefPtr<nsIRunnable> event(std::move(aEvent));
  if (NS_WARN_IF(!event)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (gXPCOMThreadsShutDown && !mIsMainThread) {
    NS_ASSERTION(false, "Failed Dispatch after xpcom-shutdown-threads");
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

#ifdef MOZ_TASK_TRACER
  nsCOMPtr<nsIRunnable> tracedRunnable = CreateTracedRunnable(event.take());
  (static_cast<TracedRunnable*>(tracedRunnable.get()))->DispatchTask();
  // XXX tracedRunnable will always leaked when we fail to disptch.
  event = tracedRunnable.forget();
#endif

  if (aFlags & DISPATCH_SYNC) {
    nsCOMPtr<nsIEventTarget> current = GetCurrentThreadEventTarget();
    if (NS_WARN_IF(!current)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    // XXX we should be able to do something better here... we should
    //     be able to monitor the slot occupied by this event and use
    //     that to tell us when the event has been processed.

    RefPtr<nsThreadSyncDispatch> wrapper =
        new nsThreadSyncDispatch(current.forget(), event.take());
    bool success = mSink->PutEvent(do_AddRef(wrapper),
                                   EventQueuePriority::Normal);  // hold a ref
    if (!success) {
      // PutEvent leaked the wrapper runnable object on failure, so we
      // explicitly release this object once for that. Note that this
      // object will be released again soon because it exits the scope.
      wrapper.get()->Release();
      return NS_ERROR_UNEXPECTED;
    }

    // Allows waiting; ensure no locks are held that would deadlock us!
    SpinEventLoopUntil(
        [&, wrapper]() -> bool { return !wrapper->IsPending(); });

    return NS_OK;
  }

  NS_ASSERTION(aFlags == NS_DISPATCH_NORMAL || aFlags == NS_DISPATCH_AT_END,
               "unexpected dispatch flags");
  if (!mSink->PutEvent(event.take(), EventQueuePriority::Normal)) {
    return NS_ERROR_UNEXPECTED;
  }
  // Delay to encourage the receiving task to run before we do work.
  DelayForChaosMode(ChaosFeature::TaskDispatching, 1000);
  return NS_OK;
}

NS_IMETHODIMP
ThreadEventTarget::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                                   uint32_t aDelayMs) {
  nsCOMPtr<nsIRunnable> event = aEvent;
  NS_ENSURE_TRUE(!!aDelayMs, NS_ERROR_UNEXPECTED);

  RefPtr<DelayedRunnable> r =
      new DelayedRunnable(do_AddRef(this), event.forget(), aDelayMs);
  nsresult rv = r->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  return Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
ThreadEventTarget::IsOnCurrentThread(bool* aIsOnCurrentThread) {
  *aIsOnCurrentThread = IsOnCurrentThread();
  return NS_OK;
}

NS_IMETHODIMP_(bool)
ThreadEventTarget::IsOnCurrentThreadInfallible() {
  // This method is only going to be called if `mThread` is null, which
  // only happens when the thread has exited the event loop.  Therefore, when
  // we are called, we can never be on this thread.
  return false;
}
