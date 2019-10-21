/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_IdlePeriodState_h
#define mozilla_IdlePeriodState_h

/**
 * A class for tracking the state of our idle period.  This includes keeping
 * track of both the state of our process-local idle period estimate and, for
 * content processes, managing communication with the parent process for
 * cross-pprocess idle detection.
 */

#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"

#include <stdint.h>

class nsIIdlePeriod;

namespace mozilla {
namespace ipc {
class IdleSchedulerChild;
}  // namespace ipc

class IdlePeriodState {
 public:
  explicit IdlePeriodState(already_AddRefed<nsIIdlePeriod>&& aIdlePeriod);

  ~IdlePeriodState();

  // Integration with memory reporting.
  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;

  // Notification that whoever we are tracking idle state for has found a
  // non-idle task to process.
  //
  // aMutexToUnlock is the mutex to unlock if we do anything that might lock
  // other mutexes.
  void FlagNotIdle(Mutex& aMutexToUnlock);

  // Notification that whoever we are tracking idle state for has no more
  // tasks (idle or not) to process.
  //
  // aMutexToUnlock is the mutex to unlock if we do anything that might lock
  // other mutexes.
  void RanOutOfTasks(Mutex& aMutexToUnlock);

  // Notification that whoever we are tracking idle state has idle tasks that
  // they are considering ready to run and that we should keep claiming they are
  // ready to run until they call ForgetPendingTaskGuarantee().
  void EnforcePendingTaskGuarantee() {
    mHasPendingEventsPromisedIdleEvent = true;
  }

  // Notification that whoever we are tracking idle state for is done with our
  // "we have an idle event ready to run" guarantee.  When this happens, we can
  // reset mHasPendingEventsPromisedIdleEvent to false, because we have
  // fulfilled our contract.
  void ForgetPendingTaskGuarantee() {
    mHasPendingEventsPromisedIdleEvent = false;
  }

  // Get our current idle deadline so we can run an idle task with that
  // deadline.  This can return a null timestamp (which means we are not idle
  // right now), and it can also queue up queries to our parent process, if
  // we're a content process, to find out whether we're idle.  This should only
  // be called when there is an actual idle task that might run.
  //
  // aMutexToUnlock is the mutex to unlock if we do anything that might lock
  // other mutexes.
  TimeStamp GetDeadlineForIdleTask(Mutex& aMutexToUnlock) {
    return GetIdleDeadlineInternal(false, aMutexToUnlock);
  }

  // Peek our current idle deadline.  This can return a null timestamp (which
  // means we are not idle right now).  This method does not have any
  // side-effects on our state, apart from guaranteeing that if it returns
  // non-null then GetDeadlineForIdleTask will return non-null until
  // ForgetPendingTaskGuarantee() is called.
  //
  // aMutexToUnlock is the mutex to unlock if we do anything that might lock
  // other mutexes.
  TimeStamp PeekIdleDeadline(Mutex& aMutexToUnlock) {
    return GetIdleDeadlineInternal(true, aMutexToUnlock);
  }

  void SetIdleToken(uint64_t aId, TimeDuration aDuration);

  bool IsActive() { return mActive; }

 protected:
  void EnsureIsActive() {
    if (!mActive) {
      SetActive();
    }
  }

  void EnsureIsPaused(Mutex& aMutexToUnlock) {
    if (mActive) {
      SetPaused(aMutexToUnlock);
    }
  }

  // Returns a null TimeStamp if we're not in the idle period.
  TimeStamp GetLocalIdleDeadline(bool& aShuttingDown, Mutex& aMutexToUnlock);

  // Gets the idle token, which is the end time of the idle period.
  //
  // aMutexToUnlock is the mutex to unlock if we do anything that might lock
  // other mutexes.
  TimeStamp GetIdleToken(TimeStamp aLocalIdlePeriodHint, Mutex& aMutexToUnlock);

  // In case of child processes, requests idle time from the cross-process
  // idle scheduler.
  void RequestIdleToken(TimeStamp aLocalIdlePeriodHint);

  // Mark that we don't have idle time to use, nor are expecting to get an idle
  // token from the idle scheduler.
  void ClearIdleToken(Mutex& aMutexToUnlock);

  // SetActive should be called when the event queue is running any type of
  // tasks.
  void SetActive();
  // SetPaused should be called once the event queue doesn't have more
  // tasks to process, or is waiting for the idle token.
  //
  // aMutexToUnlock is the mutex to unlock if we do anything that might lock
  // other mutexes.
  void SetPaused(Mutex& aMutexToUnlock);

  // Get or peek our idle deadline.  When peeking, we generally don't change any
  // of our internal state.  When getting, we may request an idle token as
  // needed.
  //
  // aMutexToUnlock is the mutex to unlock if we do anything that might lock
  // other mutexes.
  TimeStamp GetIdleDeadlineInternal(bool aIsPeek, Mutex& aMutexToUnlock);

  // Set to true if we have claimed we have a ready-to-run idle task when asked.
  // In that case, we will ensure that we allow at least one task to run when
  // someone tries to run a task, even if we have run out of idle period at that
  // point.  This ensures that we never fail to produce a task to run if we
  // claim we have a task ready to run.
  bool mHasPendingEventsPromisedIdleEvent = false;

  // mIdlePeriod keeps track of the current idle period. Calling
  // mIdlePeriod->GetIdlePeriodHint() will give an estimate of when
  // the current idle period will end.
  nsCOMPtr<nsIIdlePeriod> mIdlePeriod;

  // If non-null, this timestamp represents the end time of the idle period.  An
  // idle period starts when we get the idle token from the parent process and
  // ends when either there are no more things we want to run at idle priority
  // or mIdleToken < TimeStamp::Now(), so we have reached our idle deadline.
  TimeStamp mIdleToken;

  // The id of the last idle request to the cross-process idle scheduler.
  uint64_t mIdleRequestId = 0;

  // If we're in a content process, we use mIdleScheduler to communicate with
  // the parent process for purposes of cross-process idle tracking.
  RefPtr<ipc::IdleSchedulerChild> mIdleScheduler;

  // mIdleSchedulerInitialized is true if our mIdleScheduler has been
  // initialized.  It may be null even after initialiazation, in various
  // situations.
  bool mIdleSchedulerInitialized = false;

  // mActive is true when the PrioritizedEventQueue or TaskController we are
  // associated with is running tasks.
  bool mActive = true;
};

}  // namespace mozilla

#endif  // mozilla_IdlePeriodState_h
