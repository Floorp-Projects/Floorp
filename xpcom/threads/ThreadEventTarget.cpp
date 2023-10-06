/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadEventTarget.h"
#include "mozilla/ThreadEventQueue.h"

#include "LeakRefPtr.h"
#include "mozilla/DelayedRunnable.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/TimeStamp.h"
#include "nsComponentManagerUtils.h"
#include "nsITimer.h"
#include "nsThreadManager.h"
#include "nsThreadSyncDispatch.h"
#include "nsThreadUtils.h"
#include "ThreadDelay.h"

using namespace mozilla;

#ifdef DEBUG
// This flag will be set right after XPCOMShutdownThreads finished but before
// we continue with other processing. It is exclusively meant to prime the
// assertion of ThreadEventTarget::Dispatch as early as possible.
// Please use AppShutdown::IsInOrBeyond(ShutdownPhase::???)
// elsewhere to check for shutdown phases.
static mozilla::Atomic<bool, mozilla::SequentiallyConsistent>
    gXPCOMThreadsShutDownNotified(false);
#endif

ThreadEventTarget::ThreadEventTarget(ThreadTargetSink* aSink,
                                     bool aIsMainThread, bool aBlockDispatch)
    : mSink(aSink),
#ifdef DEBUG
      mIsMainThread(aIsMainThread),
#endif
      mBlockDispatch(aBlockDispatch) {
  mThread = PR_GetCurrentThread();
}

ThreadEventTarget::~ThreadEventTarget() = default;

void ThreadEventTarget::SetCurrentThread(PRThread* aThread) {
  mThread = aThread;
}

void ThreadEventTarget::ClearCurrentThread() { mThread = nullptr; }

NS_IMPL_ISUPPORTS(ThreadEventTarget, nsIEventTarget, nsISerialEventTarget)

NS_IMETHODIMP
ThreadEventTarget::DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags) {
  return Dispatch(do_AddRef(aRunnable), aFlags);
}

#ifdef DEBUG
// static
void ThreadEventTarget::XPCOMShutdownThreadsNotificationFinished() {
  gXPCOMThreadsShutDownNotified = true;
}
#endif

NS_IMETHODIMP
ThreadEventTarget::Dispatch(already_AddRefed<nsIRunnable> aEvent,
                            uint32_t aFlags) {
  // We want to leak the reference when we fail to dispatch it, so that
  // we won't release the event in a wrong thread.
  LeakRefPtr<nsIRunnable> event(std::move(aEvent));
  if (NS_WARN_IF(!event)) {
    return NS_ERROR_INVALID_ARG;
  }

  NS_ASSERTION(!gXPCOMThreadsShutDownNotified || mIsMainThread ||
                   PR_GetCurrentThread() == mThread ||
                   (aFlags & NS_DISPATCH_IGNORE_BLOCK_DISPATCH),
               "Dispatch to non-main thread after xpcom-shutdown-threads");

  if (mBlockDispatch && !(aFlags & NS_DISPATCH_IGNORE_BLOCK_DISPATCH)) {
    MOZ_DIAGNOSTIC_ASSERT(
        false,
        "Attempt to dispatch to thread which does not usually process "
        "dispatched runnables until shutdown");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  LogRunnable::LogDispatch(event.get());

  NS_ASSERTION((aFlags & (NS_DISPATCH_AT_END |
                          NS_DISPATCH_IGNORE_BLOCK_DISPATCH)) == aFlags,
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
ThreadEventTarget::RegisterShutdownTask(nsITargetShutdownTask* aTask) {
  return mSink->RegisterShutdownTask(aTask);
}

NS_IMETHODIMP
ThreadEventTarget::UnregisterShutdownTask(nsITargetShutdownTask* aTask) {
  return mSink->UnregisterShutdownTask(aTask);
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
