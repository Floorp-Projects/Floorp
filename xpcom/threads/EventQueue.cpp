/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventQueue.h"

#include "GeckoProfiler.h"
#include "nsIRunnable.h"

using namespace mozilla;
using namespace mozilla::detail;

template <size_t ItemsPerPage>
EventQueueInternal<ItemsPerPage>::EventQueueInternal(
    EventQueuePriority aPriority) {}

template <size_t ItemsPerPage>
void EventQueueInternal<ItemsPerPage>::PutEvent(
    already_AddRefed<nsIRunnable>&& aEvent, EventQueuePriority aPriority,
    const MutexAutoLock& aProofOfLock, mozilla::TimeDuration* aDelay) {
#ifdef MOZ_GECKO_PROFILER
  // Sigh, this doesn't check if this thread is being profiled
  if (profiler_is_active()) {
    // check to see if the profiler has been enabled since the last PutEvent
    while (mDispatchTimes.Count() < mQueue.Count()) {
      mDispatchTimes.Push(TimeStamp());
    }
    mDispatchTimes.Push(aDelay ? TimeStamp::Now() - *aDelay : TimeStamp::Now());
  }
#endif

  nsCOMPtr<nsIRunnable> event(aEvent);
  mQueue.Push(std::move(event));
}

template <size_t ItemsPerPage>
already_AddRefed<nsIRunnable> EventQueueInternal<ItemsPerPage>::GetEvent(
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
  // We always want to clear the dispatch times, even if the profiler is turned
  // off, because we want to empty the (previously-collected) dispatch times, if
  // any, from when the profiler was turned on.  We only want to do something
  // interesting with the dispatch times if the profiler is turned on, though.
  if (!mDispatchTimes.IsEmpty()) {
    TimeStamp dispatch_time = mDispatchTimes.Pop();
    if (profiler_is_active()) {
      if (!dispatch_time.IsNull()) {
        if (aLastEventDelay) {
          *aLastEventDelay = TimeStamp::Now() - dispatch_time;
        }
      }
    }
  } else if (profiler_is_active()) {
    if (aLastEventDelay) {
      // if we just turned on the profiler, we don't have dispatch
      // times for events already in the queue.
      *aLastEventDelay = TimeDuration();
    }
  }
#endif

  nsCOMPtr<nsIRunnable> result = mQueue.Pop();
  return result.forget();
}

template <size_t ItemsPerPage>
bool EventQueueInternal<ItemsPerPage>::IsEmpty(
    const MutexAutoLock& aProofOfLock) {
  return mQueue.IsEmpty();
}

template <size_t ItemsPerPage>
bool EventQueueInternal<ItemsPerPage>::HasReadyEvent(
    const MutexAutoLock& aProofOfLock) {
  return !IsEmpty(aProofOfLock);
}

template <size_t ItemsPerPage>
size_t EventQueueInternal<ItemsPerPage>::Count(
    const MutexAutoLock& aProofOfLock) const {
  return mQueue.Count();
}
