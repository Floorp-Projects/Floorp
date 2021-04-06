/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventQueue_h
#define mozilla_EventQueue_h

#include "mozilla/Mutex.h"
#include "mozilla/Queue.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"

class nsIRunnable;

namespace mozilla {

enum class EventQueuePriority {
  Idle,
  DeferredTimers,
  InputLow,
  Normal,
  MediumHigh,
  InputHigh,
  Vsync,
  InputHighest,

  Count
};

class IdlePeriodState;

namespace detail {

// EventQueue is our unsynchronized event queue implementation. It is a queue
// of runnables used for non-main thread, as well as optionally providing
// forwarding to TaskController.
//
// Since EventQueue is unsynchronized, it should be wrapped in an outer
// SynchronizedEventQueue implementation (like ThreadEventQueue).
template <size_t ItemsPerPage>
class EventQueueInternal {
 public:
  explicit EventQueueInternal(bool aForwardToTC) : mForwardToTC(aForwardToTC) {}

  // Add an event to the end of the queue. Implementors are free to use
  // aPriority however they wish.  If the runnable supports
  // nsIRunnablePriority and the implementing class supports
  // prioritization, aPriority represents the result of calling
  // nsIRunnablePriority::GetPriority().  *aDelay is time the event has
  // already been delayed (used when moving an event from one queue to
  // another)
  void PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventQueuePriority aPriority, const MutexAutoLock& aProofOfLock,
                mozilla::TimeDuration* aDelay = nullptr);

  // Get an event from the front of the queue. This should return null if the
  // queue is non-empty but the event in front is not ready to run.
  // *aLastEventDelay is the time the event spent in queues before being
  // retrieved.
  already_AddRefed<nsIRunnable> GetEvent(
      const MutexAutoLock& aProofOfLock,
      mozilla::TimeDuration* aLastEventDelay = nullptr);

  // Returns true if the queue is empty. Implies !HasReadyEvent().
  bool IsEmpty(const MutexAutoLock& aProofOfLock);

  // Returns true if the queue is non-empty and if the event in front is ready
  // to run. Implies !IsEmpty(). This should return true iff GetEvent returns a
  // non-null value.
  bool HasReadyEvent(const MutexAutoLock& aProofOfLock);

  // Returns the number of events in the queue.
  size_t Count(const MutexAutoLock& aProofOfLock) const;
  // For some reason, if we put this in the .cpp file the linker can't find it
  already_AddRefed<nsIRunnable> PeekEvent(const MutexAutoLock& aProofOfLock) {
    if (mQueue.IsEmpty()) {
      return nullptr;
    }

    nsCOMPtr<nsIRunnable> result = mQueue.FirstElement();
    return result.forget();
  }

  void EnableInputEventPrioritization(const MutexAutoLock& aProofOfLock) {}
  void FlushInputEventPrioritization(const MutexAutoLock& aProofOfLock) {}
  void SuspendInputEventPrioritization(const MutexAutoLock& aProofOfLock) {}
  void ResumeInputEventPrioritization(const MutexAutoLock& aProofOfLock) {}

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    size_t size = mQueue.ShallowSizeOfExcludingThis(aMallocSizeOf);
    size += mDispatchTimes.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return size;
  }

 private:
  mozilla::Queue<nsCOMPtr<nsIRunnable>, ItemsPerPage> mQueue;
  // This queue is only populated when the profiler is turned on.
  mozilla::Queue<mozilla::TimeStamp, ItemsPerPage> mDispatchTimes;
  TimeDuration mLastEventDelay;
  // This indicates PutEvent forwards runnables to the TaskController. This
  // should be true for the top level event queue on the main thread.
  bool mForwardToTC;
};

}  // namespace detail

class EventQueue final : public mozilla::detail::EventQueueInternal<16> {
 public:
  explicit EventQueue(bool aForwardToTC = false)
      : mozilla::detail::EventQueueInternal<16>(aForwardToTC) {}
};

template <size_t ItemsPerPage = 16>
class EventQueueSized final
    : public mozilla::detail::EventQueueInternal<ItemsPerPage> {
 public:
  explicit EventQueueSized(bool aForwardToTC = false)
      : mozilla::detail::EventQueueInternal<ItemsPerPage>(aForwardToTC) {}
};

}  // namespace mozilla

#endif  // mozilla_EventQueue_h
