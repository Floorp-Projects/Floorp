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

class EventQueue final : public AbstractEventQueue {
 public:
  static const bool SupportsPrioritization = false;

  EventQueue() {}
  explicit EventQueue(EventQueuePriority aPriority);

  void PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventQueuePriority aPriority, const MutexAutoLock& aProofOfLock,
                mozilla::TimeDuration* aDelay = nullptr) final;
  already_AddRefed<nsIRunnable> GetEvent(
      EventQueuePriority* aPriority, const MutexAutoLock& aProofOfLock,
      mozilla::TimeDuration* aLastEventDelay = nullptr) final;
  void DidRunEvent(const MutexAutoLock& aProofOfLock) {}

  bool IsEmpty(const MutexAutoLock& aProofOfLock) final;
  bool HasReadyEvent(const MutexAutoLock& aProofOfLock) final;
  bool HasPendingHighPriorityEvents(const MutexAutoLock& aProofOfLock) final {
    // EventQueue doesn't support any prioritization.
    return false;
  }

  size_t Count(const MutexAutoLock& aProofOfLock) const final;
  already_AddRefed<nsIRunnable> PeekEvent(const MutexAutoLock& aProofOfLock);

  void EnableInputEventPrioritization(const MutexAutoLock& aProofOfLock) final {
  }
  void FlushInputEventPrioritization(const MutexAutoLock& aProofOfLock) final {}
  void SuspendInputEventPrioritization(
      const MutexAutoLock& aProofOfLock) final {}
  void ResumeInputEventPrioritization(const MutexAutoLock& aProofOfLock) final {
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
  mozilla::Queue<nsCOMPtr<nsIRunnable>, 16> mQueue;
#ifdef MOZ_GECKO_PROFILER
  // This queue is only populated when the profiler is turned on.
  mozilla::Queue<mozilla::TimeStamp, 16> mDispatchTimes;
  TimeDuration mLastEventDelay;
#endif
};

}  // namespace mozilla

#endif  // mozilla_EventQueue_h
