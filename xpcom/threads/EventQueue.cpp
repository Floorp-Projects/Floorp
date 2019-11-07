/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventQueue.h"
#include "nsIRunnable.h"

using namespace mozilla;

EventQueue::EventQueue(EventQueuePriority aPriority) {}

void EventQueue::PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                          EventQueuePriority aPriority,
                          const MutexAutoLock& aProofOfLock,
                          mozilla::TimeDuration* aDelay) {
#ifdef MOZ_GECKO_PROFILER
  // Sigh, this doesn't check if this thread is being profiled
  if (profiler_is_active()) {
    // check to see if the profiler has been enabled since the last PutEvent
    while (mDispatchTimes.Count() < mQueue.Count()) {
      mDispatchTimes.Push(TimeStamp());
    }
    mDispatchTimes.Push(aDelay ? TimeStamp::Now() - *aDelay : TimeStamp::Now());
  } else {
    // XXX clear queues when profiler is turned off without adding (much)
    // overhead instead of doing this
    mDispatchTimes.Push(TimeStamp());
  }
#endif

  nsCOMPtr<nsIRunnable> event(aEvent);
  mQueue.Push(std::move(event));
}

already_AddRefed<nsIRunnable> EventQueue::GetEvent(
    EventQueuePriority* aPriority, const MutexAutoLock& aProofOfLock,
    mozilla::TimeDuration* aLastEventDelay) {
  if (mQueue.IsEmpty()) {
    if (aLastEventDelay) {
      *aLastEventDelay = TimeDuration();
    }
    return nullptr;
  }

  if (aPriority) {
    *aPriority = EventQueuePriority::Normal;
  }

#ifdef MOZ_GECKO_PROFILER
  if (profiler_is_active()) {
    // check to see if the profiler has been enabled since the last PutEvent
    while (mDispatchTimes.Count() < mQueue.Count()) {
      mDispatchTimes.Push(TimeStamp());
    }
    TimeStamp dispatch_time = mDispatchTimes.Pop();
    if (!dispatch_time.IsNull()) {
      if (aLastEventDelay) {
        *aLastEventDelay = TimeStamp::Now() - dispatch_time;
      }
    }
  } else {
    (void)mDispatchTimes.Pop();
  }
  // XXX clear queues when profiler is turned off without adding (much) overhead
#endif

  nsCOMPtr<nsIRunnable> result = mQueue.Pop();
  return result.forget();
}

bool EventQueue::IsEmpty(const MutexAutoLock& aProofOfLock) {
  return mQueue.IsEmpty();
}

bool EventQueue::HasReadyEvent(const MutexAutoLock& aProofOfLock) {
  return !IsEmpty(aProofOfLock);
}

already_AddRefed<nsIRunnable> EventQueue::PeekEvent(
    const MutexAutoLock& aProofOfLock) {
  if (mQueue.IsEmpty()) {
    return nullptr;
  }

  nsCOMPtr<nsIRunnable> result = mQueue.FirstElement();
  return result.forget();
}

size_t EventQueue::Count(const MutexAutoLock& aProofOfLock) const {
  return mQueue.Count();
}
