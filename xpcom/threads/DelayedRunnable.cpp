/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DelayedRunnable.h"

#include "mozilla/ProfilerRunnable.h"

namespace mozilla {

DelayedRunnable::DelayedRunnable(already_AddRefed<nsISerialEventTarget> aTarget,
                                 already_AddRefed<nsIRunnable> aRunnable,
                                 uint32_t aDelay)
    : mozilla::Runnable("DelayedRunnable"),
      mTarget(aTarget),
      mDelayedFrom(TimeStamp::NowLoRes()),
      mDelay(aDelay),
      mWrappedRunnable(aRunnable) {}

nsresult DelayedRunnable::Init() {
  MutexAutoLock lock(mMutex);
  if (!mWrappedRunnable) {
    MOZ_ASSERT_UNREACHABLE();
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = mTarget->RegisterShutdownTask(this);
  if (NS_FAILED(rv)) {
    MOZ_DIAGNOSTIC_ASSERT(
        rv == NS_ERROR_UNEXPECTED,
        "DelayedRunnable target must support RegisterShutdownTask");
    NS_WARNING("DelayedRunnable init after target is shutdown");
    return rv;
  }

  rv = NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, mDelay,
                               nsITimer::TYPE_ONE_SHOT, mTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mTarget->UnregisterShutdownTask(this);
  }
  return rv;
}

NS_IMETHODIMP DelayedRunnable::Run() {
  MOZ_ASSERT(mTarget->IsOnCurrentThread());

  nsCOMPtr<nsIRunnable> runnable;
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mTimer, "Init() must have been called");

    // Already ran?
    if (!mWrappedRunnable) {
      return NS_OK;
    }

    // Are we too early?
    if ((mozilla::TimeStamp::NowLoRes() - mDelayedFrom).ToMilliseconds() <
        mDelay) {
      return NS_OK;  // Let the nsITimer run us.
    }

    mTimer->Cancel();
    mTarget->UnregisterShutdownTask(this);
    runnable = mWrappedRunnable.forget();
  }

  AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable);
  return runnable->Run();
}

NS_IMETHODIMP DelayedRunnable::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(mTarget->IsOnCurrentThread());

  nsCOMPtr<nsIRunnable> runnable;
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mTimer, "Init() must have been called");

    // We may have already run due to races
    if (!mWrappedRunnable) {
      return NS_OK;
    }

    mTarget->UnregisterShutdownTask(this);
    runnable = mWrappedRunnable.forget();
  }

  AUTO_PROFILE_FOLLOWING_RUNNABLE(runnable);
  return runnable->Run();
}

void DelayedRunnable::TargetShutdown() {
  MOZ_ASSERT(mTarget->IsOnCurrentThread());

  // Called at shutdown
  MutexAutoLock lock(mMutex);
  if (!mWrappedRunnable) {
    return;
  }
  mWrappedRunnable = nullptr;

  if (mTimer) {
    mTimer->Cancel();
  }
}

NS_IMPL_ISUPPORTS_INHERITED(DelayedRunnable, Runnable, nsITimerCallback,
                            nsITargetShutdownTask)

}  // namespace mozilla
