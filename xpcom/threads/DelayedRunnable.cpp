/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DelayedRunnable.h"

namespace mozilla {

DelayedRunnable::DelayedRunnable(already_AddRefed<nsIEventTarget> aTarget,
                                 already_AddRefed<nsIRunnable> aRunnable,
                                 uint32_t aDelay)
    : mozilla::Runnable("DelayedRunnable"),
      mTarget(aTarget),
      mObserver(do_QueryInterface(mTarget)),
      mWrappedRunnable(aRunnable),
      mDelayedFrom(TimeStamp::NowLoRes()),
      mDelay(aDelay) {
  MOZ_DIAGNOSTIC_ASSERT(mObserver,
                        "Target must implement nsIDelayedRunnableObserver");
}

nsresult DelayedRunnable::Init() {
  mObserver->OnDelayedRunnableCreated(this);
  return NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, mDelay,
                                 nsITimer::TYPE_ONE_SHOT, mTarget);
}

void DelayedRunnable::CancelTimer() {
  MOZ_ASSERT(mTarget->IsOnCurrentThread());
  mTimer->Cancel();
}

NS_IMETHODIMP DelayedRunnable::Run() {
  MOZ_ASSERT(mTimer, "DelayedRunnable without Init?");

  // Already ran?
  if (!mWrappedRunnable) {
    return NS_OK;
  }

  // Are we too early?
  if ((mozilla::TimeStamp::NowLoRes() - mDelayedFrom).ToMilliseconds() <
      mDelay) {
    if (mObserver) {
      mObserver->OnDelayedRunnableScheduled(this);
    }
    return NS_OK;  // Let the nsITimer run us.
  }

  mTimer->Cancel();
  return DoRun();
}

NS_IMETHODIMP DelayedRunnable::Notify(nsITimer* aTimer) {
  // If we already ran, the timer should have been canceled.
  MOZ_ASSERT(mWrappedRunnable);

  if (mObserver) {
    mObserver->OnDelayedRunnableRan(this);
  }
  return DoRun();
}

nsresult DelayedRunnable::DoRun() {
  nsCOMPtr<nsIRunnable> r = std::move(mWrappedRunnable);
  return r->Run();
}

NS_IMPL_ISUPPORTS_INHERITED(DelayedRunnable, Runnable, nsITimerCallback)

}  // namespace mozilla
