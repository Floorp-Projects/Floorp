/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PrioritizedEventQueue_h
#define mozilla_PrioritizedEventQueue_h

#include "mozilla/AbstractEventQueue.h"
#include "mozilla/EventQueue.h"
#include "mozilla/IdlePeriodState.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"

class nsIIdlePeriod;
class nsIRunnable;

namespace mozilla {
namespace ipc {
class IdleSchedulerChild;
}

// This AbstractEventQueue implementation has one queue for each
// EventQueuePriority. The type of queue used for each priority is determined by
// the template parameter.
//
// When an event is pushed, its priority is determined by QIing the runnable to
// nsIRunnablePriority, or by falling back to the aPriority parameter if the QI
// fails.
//
// When an event is popped, a queue is selected based on heuristics that
// optimize for performance. Roughly, events are selected from the highest
// priority queue that is non-empty. However, there are a few exceptions:
// - We try to avoid processing too many high-priority events in a row so
//   that the normal priority queue is not starved. When there are high-
//   and normal-priority events available, we interleave popping from the
//   normal and high queues.
// - We do not select events from the idle queue if the current idle period
//   is almost over.
class PrioritizedEventQueue final : public AbstractEventQueue {
 public:
  static const bool SupportsPrioritization = true;

  explicit PrioritizedEventQueue(already_AddRefed<nsIIdlePeriod>&& aIdlePeriod);

  virtual ~PrioritizedEventQueue();

  void PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventQueuePriority aPriority, const MutexAutoLock& aProofOfLock,
                mozilla::TimeDuration* aDelay = nullptr) final;
  // See PrioritizedEventQueue.cpp for explanation of
  // aHypotheticalInputEventDelay
  already_AddRefed<nsIRunnable> GetEvent(
      EventQueuePriority* aPriority, const MutexAutoLock& aProofOfLock,
      TimeDuration* aHypotheticalInputEventDelay = nullptr) final;
  // *aIsIdleEvent will be set to true when we are returning a non-null runnable
  // which came from one of our idle queues, and will be false otherwise.
  already_AddRefed<nsIRunnable> GetEvent(
      EventQueuePriority* aPriority, const MutexAutoLock& aProofOfLock,
      TimeDuration* aHypotheticalInputEventDelay, bool* aIsIdleEvent);
  void DidRunEvent(const MutexAutoLock& aProofOfLock);

  bool IsEmpty(const MutexAutoLock& aProofOfLock) final;
  size_t Count(const MutexAutoLock& aProofOfLock) const final;
  bool HasReadyEvent(const MutexAutoLock& aProofOfLock) final;
  bool HasPendingHighPriorityEvents(const MutexAutoLock& aProofOfLock) final;

  // When checking the idle deadline, we need to drop whatever mutex protects
  // this queue. This method allows that mutex to be stored so that we can drop
  // it and reacquire it when checking the idle deadline. The mutex must live at
  // least as long as the queue.
  void SetMutexRef(Mutex& aMutex) { mMutex = &aMutex; }

  void EnableInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;
  void FlushInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;
  void SuspendInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;
  void ResumeInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;

  IdlePeriodState* GetIdlePeriodState() { return &mIdlePeriodState; }

  bool HasIdleRunnables(const MutexAutoLock& aProofOfLock) const;

  size_t SizeOfExcludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const override {
    size_t n = 0;

    n += mHighQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mInputQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mMediumHighQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mNormalQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mDeferredTimersQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mIdleQueue->SizeOfIncludingThis(aMallocSizeOf);

    n += mIdlePeriodState.SizeOfExcludingThis(aMallocSizeOf);

    return n;
  }

 private:
  EventQueuePriority SelectQueue(bool aUpdateState,
                                 const MutexAutoLock& aProofOfLock);

  void IndirectlyQueueRunnable(already_AddRefed<nsIRunnable>&& aEvent,
                               EventQueuePriority aPriority,
                               const MutexAutoLock& aProofOfLock,
                               mozilla::TimeDuration* aDelay);

  UniquePtr<EventQueue> mHighQueue;
  UniquePtr<EventQueueSized<32>> mInputQueue;
  UniquePtr<EventQueue> mMediumHighQueue;
  UniquePtr<EventQueueSized<64>> mNormalQueue;
  UniquePtr<EventQueue> mDeferredTimersQueue;
  UniquePtr<EventQueue> mIdleQueue;

  // We need to drop the queue mutex when checking the idle deadline, so we keep
  // a pointer to it here.
  Mutex* mMutex = nullptr;

  TimeDuration mLastEventDelay;
  TimeStamp mLastEventStart;

  TimeStamp mInputHandlingStartTime;

  enum InputEventQueueState {
    STATE_DISABLED,
    STATE_FLUSHING,
    STATE_SUSPEND,
    STATE_ENABLED
  };
  InputEventQueueState mInputQueueState = STATE_DISABLED;

  // Tracking of our idle state of various sorts.
  IdlePeriodState mIdlePeriodState;
};

}  // namespace mozilla

#endif  // mozilla_PrioritizedEventQueue_h
