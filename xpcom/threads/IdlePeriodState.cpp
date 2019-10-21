/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/IdlePeriodState.h"
#include "mozilla/StaticPrefs_idle_period.h"
#include "mozilla/ipc/IdleSchedulerChild.h"
#include "nsIIdlePeriod.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"

static uint64_t sIdleRequestCounter = 0;

namespace mozilla {

IdlePeriodState::IdlePeriodState(already_AddRefed<nsIIdlePeriod>&& aIdlePeriod)
    : mIdlePeriod(aIdlePeriod) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");
}

IdlePeriodState::~IdlePeriodState() {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");
  if (mIdleScheduler) {
    mIdleScheduler->Disconnect();
  }
}

size_t IdlePeriodState::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  if (mIdlePeriod) {
    n += aMallocSizeOf(mIdlePeriod);
  }

  return n;
}

void IdlePeriodState::FlagNotIdle(Mutex& aMutexToUnlock) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");

  EnsureIsActive();
  if (mIdleToken && mIdleToken < TimeStamp::Now()) {
    ClearIdleToken(aMutexToUnlock);
  }
}

void IdlePeriodState::RanOutOfTasks(Mutex& aMutexToUnlock) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");
  MOZ_ASSERT(!mHasPendingEventsPromisedIdleEvent);
  EnsureIsPaused(aMutexToUnlock);
  ClearIdleToken(aMutexToUnlock);
}

TimeStamp IdlePeriodState::GetIdleDeadlineInternal(bool aIsPeek,
                                                   Mutex& aMutexToUnlock) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");

  bool shuttingDown;
  TimeStamp localIdleDeadline =
      GetLocalIdleDeadline(shuttingDown, aMutexToUnlock);
  if (!localIdleDeadline) {
    if (!aIsPeek) {
      EnsureIsPaused(aMutexToUnlock);
      ClearIdleToken(aMutexToUnlock);
    }
    return TimeStamp();
  }

  TimeStamp idleDeadline =
      mHasPendingEventsPromisedIdleEvent || shuttingDown
          ? localIdleDeadline
          : GetIdleToken(localIdleDeadline, aMutexToUnlock);
  if (!idleDeadline) {
    if (!aIsPeek) {
      EnsureIsPaused(aMutexToUnlock);

      // Don't call ClearIdleToken() here, since we may have a pending
      // request already.
      // RequestIdleToken can do all sorts of IPC stuff that might take mutexes.
      MutexAutoUnlock unlock(aMutexToUnlock);
      RequestIdleToken(localIdleDeadline);
    }
    return TimeStamp();
  }

  if (!aIsPeek) {
    EnsureIsActive();
  }
  return idleDeadline;
}

TimeStamp IdlePeriodState::GetLocalIdleDeadline(bool& aShuttingDown,
                                                Mutex& aMutexToUnlock) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");
  // If we are shutting down, we won't honor the idle period, and we will
  // always process idle runnables.  This will ensure that the idle queue
  // gets exhausted at shutdown time to prevent intermittently leaking
  // some runnables inside that queue and even worse potentially leaving
  // some important cleanup work unfinished.
  if (gXPCOMThreadsShutDown ||
      nsThreadManager::get().GetCurrentThread()->ShuttingDown()) {
    aShuttingDown = true;
    return TimeStamp::Now();
  }

  aShuttingDown = false;
  TimeStamp idleDeadline;
  {
    // Releasing the lock temporarily since getting the idle period
    // might need to lock the timer thread. Unlocking here might make
    // us receive an event on the main queue, but we've committed to
    // run an idle event anyhow.
    MutexAutoUnlock unlock(aMutexToUnlock);
    mIdlePeriod->GetIdlePeriodHint(&idleDeadline);
  }

  // If HasPendingEvents() has been called and it has returned true because of
  // pending idle events, there is a risk that we may decide here that we aren't
  // idle and return null, in which case HasPendingEvents() has effectively
  // lied.  Since we can't go back and fix the past, we have to adjust what we
  // do here and forcefully pick the idle queue task here.  Note that this means
  // that we are choosing to run a task from the idle queue when we would
  // normally decide that we aren't in an idle period, but this can only happen
  // if we fall out of the idle period in between the call to HasPendingEvents()
  // and here, which should hopefully be quite rare.  We are effectively
  // choosing to prioritize the sanity of our API semantics over the optimal
  // scheduling.
  if (!mHasPendingEventsPromisedIdleEvent &&
      (!idleDeadline || idleDeadline < TimeStamp::Now())) {
    return TimeStamp();
  }
  if (mHasPendingEventsPromisedIdleEvent && !idleDeadline) {
    // If HasPendingEvents() has been called and it has returned true, but we're
    // no longer in the idle period, we must return a valid timestamp to pretend
    // that we are still in the idle period.
    return TimeStamp::Now();
  }
  return idleDeadline;
}

TimeStamp IdlePeriodState::GetIdleToken(TimeStamp aLocalIdlePeriodHint,
                                        Mutex& aMutexToUnlock) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");

  if (XRE_IsParentProcess()) {
    return aLocalIdlePeriodHint;
  }
  if (mIdleToken) {
    TimeStamp now = TimeStamp::Now();
    if (mIdleToken < now) {
      ClearIdleToken(aMutexToUnlock);
      return mIdleToken;
    }
    return mIdleToken < aLocalIdlePeriodHint ? mIdleToken
                                             : aLocalIdlePeriodHint;
  }
  return TimeStamp();
}

void IdlePeriodState::RequestIdleToken(TimeStamp aLocalIdlePeriodHint) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");
  MOZ_ASSERT(!mActive);

  if (!mIdleSchedulerInitialized) {
    mIdleSchedulerInitialized = true;
    if (StaticPrefs::idle_period_cross_process_scheduling() &&
        XRE_IsContentProcess() &&
        // Disable when recording/replaying, as IdleSchedulerChild uses mutable
        // shared memory which needs special handling.
        !recordreplay::IsRecordingOrReplaying()) {
      // For now cross-process idle scheduler is supported only on the main
      // threads of the child processes.
      mIdleScheduler = ipc::IdleSchedulerChild::GetMainThreadIdleScheduler();
      if (mIdleScheduler) {
        mIdleScheduler->Init(this);
      }
    }
  }

  if (mIdleScheduler && !mIdleRequestId) {
    TimeStamp now = TimeStamp::Now();
    if (aLocalIdlePeriodHint <= now) {
      return;
    }

    mIdleRequestId = ++sIdleRequestCounter;
    mIdleScheduler->SendRequestIdleTime(mIdleRequestId,
                                        aLocalIdlePeriodHint - now);
  }
}

void IdlePeriodState::SetIdleToken(uint64_t aId, TimeDuration aDuration) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");
  if (mIdleRequestId == aId) {
    mIdleToken = TimeStamp::Now() + aDuration;
  }
}

void IdlePeriodState::SetActive() {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");
  MOZ_ASSERT(!mActive);
  if (mIdleScheduler) {
    mIdleScheduler->SetActive();
  }
  mActive = true;
}

void IdlePeriodState::SetPaused(Mutex& aMutexToUnlock) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");
  MOZ_ASSERT(mActive);
  if (mIdleScheduler && mIdleScheduler->SetPaused()) {
    MutexAutoUnlock unlock(aMutexToUnlock);
    // We may have gotten a free cpu core for running idle tasks.
    // We don't try to catch the case when there are prioritized processes
    // running.

    // This SendSchedule call is why we need to MutexAutoUnlock here, because
    // IPC can do weird things with mutexes.
    mIdleScheduler->SendSchedule();
  }
  mActive = false;
}

void IdlePeriodState::ClearIdleToken(Mutex& aMutexToUnlock) {
  MOZ_ASSERT(NS_IsMainThread(),
             "Why are we touching idle state off the main thread?");

  if (mIdleRequestId) {
    if (mIdleScheduler) {
      // This SendIdleTimeUsed call is why we need to MutexAutoUnlock here,
      // because IPC can do weird things with mutexes.
      MutexAutoUnlock unlock(aMutexToUnlock);
      mIdleScheduler->SendIdleTimeUsed(mIdleRequestId);
    }
    mIdleRequestId = 0;
    mIdleToken = TimeStamp();
  }
}

}  // namespace mozilla
