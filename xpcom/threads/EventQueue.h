/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventQueue_h
#define mozilla_EventQueue_h

#include "mozilla/AbstractEventQueue.h"
#include "mozilla/Queue.h"
#include "nsCOMPtr.h"

class nsIRunnable;

namespace mozilla {

class IdlePeriodState;

namespace detail {

template <size_t ItemsPerPage>
class EventQueueInternal : public AbstractEventQueue {
 public:
  static const bool SupportsPrioritization = false;

  EventQueueInternal() {}
  explicit EventQueueInternal(EventQueuePriority aPriority);

  void PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventQueuePriority aPriority, const MutexAutoLock& aProofOfLock,
                mozilla::TimeDuration* aDelay = nullptr) final;
  already_AddRefed<nsIRunnable> GetEvent(
      EventQueuePriority* aPriority, const MutexAutoLock& aProofOfLock,
      mozilla::TimeDuration* aLastEventDelay = nullptr) final;
  already_AddRefed<nsIRunnable> GetEvent(EventQueuePriority* aPriority,
                                         const MutexAutoLock& aProofOfLock,
                                         TimeDuration* aLastEventDelay,
                                         bool* aIsIdleEvent) {
    *aIsIdleEvent = false;
    return GetEvent(aPriority, aProofOfLock, aLastEventDelay);
  }

  void DidRunEvent(const MutexAutoLock& aProofOfLock) {}

  bool IsEmpty(const MutexAutoLock& aProofOfLock) final;
  bool HasReadyEvent(const MutexAutoLock& aProofOfLock) final;
  bool HasPendingHighPriorityEvents(const MutexAutoLock& aProofOfLock) final {
    // EventQueueInternal doesn't support any prioritization.
    return false;
  }

  size_t Count(const MutexAutoLock& aProofOfLock) const final;
  // For some reason, if we put this in the .cpp file the linker can't find it
  already_AddRefed<nsIRunnable> PeekEvent(const MutexAutoLock& aProofOfLock) {
    if (mQueue.IsEmpty()) {
      return nullptr;
    }

    nsCOMPtr<nsIRunnable> result = mQueue.FirstElement();
    return result.forget();
  }

  void EnableInputEventPrioritization(const MutexAutoLock& aProofOfLock) final {
  }
  void FlushInputEventPrioritization(const MutexAutoLock& aProofOfLock) final {}
  void SuspendInputEventPrioritization(
      const MutexAutoLock& aProofOfLock) final {}
  void ResumeInputEventPrioritization(const MutexAutoLock& aProofOfLock) final {
  }

  IdlePeriodState* GetIdlePeriodState() const { return nullptr; }

  bool HasIdleRunnables(const MutexAutoLock& aProofOfLock) const {
    return false;
  }

  size_t SizeOfExcludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const override {
    size_t size = mQueue.ShallowSizeOfExcludingThis(aMallocSizeOf);
#ifdef MOZ_GECKO_PROFILER
    size += mDispatchTimes.ShallowSizeOfExcludingThis(aMallocSizeOf);
#endif
    return size;
  }

 private:
  mozilla::Queue<nsCOMPtr<nsIRunnable>, ItemsPerPage> mQueue;
#ifdef MOZ_GECKO_PROFILER
  // This queue is only populated when the profiler is turned on.
  mozilla::Queue<mozilla::TimeStamp, ItemsPerPage> mDispatchTimes;
  TimeDuration mLastEventDelay;
#endif
};

}  // namespace detail

class EventQueue final : public mozilla::detail::EventQueueInternal<16> {
 public:
  EventQueue() : mozilla::detail::EventQueueInternal<16>() {}
  explicit EventQueue(EventQueuePriority aPriority)
      : mozilla::detail::EventQueueInternal<16>(aPriority){};
};

template <size_t ItemsPerPage = 16>
class EventQueueSized final
    : public mozilla::detail::EventQueueInternal<ItemsPerPage> {
 public:
  EventQueueSized() : mozilla::detail::EventQueueInternal<ItemsPerPage>() {}
  explicit EventQueueSized(EventQueuePriority aPriority)
      : mozilla::detail::EventQueueInternal<ItemsPerPage>(aPriority){};
};

}  // namespace mozilla

#endif  // mozilla_EventQueue_h
