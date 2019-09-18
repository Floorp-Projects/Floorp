/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PrioritizedEventQueue_h
#define mozilla_PrioritizedEventQueue_h

#include "mozilla/AbstractEventQueue.h"
#include "mozilla/EventQueue.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIIdlePeriod.h"

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

  explicit PrioritizedEventQueue(already_AddRefed<nsIIdlePeriod> aIdlePeriod);

  virtual ~PrioritizedEventQueue();

  void PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventQueuePriority aPriority,
                const MutexAutoLock& aProofOfLock) final;
  already_AddRefed<nsIRunnable> GetEvent(
      EventQueuePriority* aPriority, const MutexAutoLock& aProofOfLock) final;
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

#ifndef RELEASE_OR_BETA
  // nsThread.cpp sends telemetry containing the most recently computed idle
  // deadline. We store a reference to a field in nsThread where this deadline
  // will be stored so that it can be fetched quickly for telemetry.
  void SetNextIdleDeadlineRef(TimeStamp& aDeadline) {
    mNextIdleDeadline = &aDeadline;
  }
#endif

  void EnableInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;
  void FlushInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;
  void SuspendInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;
  void ResumeInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;

  size_t SizeOfExcludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const override {
    size_t n = 0;

    n += mHighQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mInputQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mMediumHighQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mNormalQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mDeferredTimersQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mIdleQueue->SizeOfIncludingThis(aMallocSizeOf);

    if (mIdlePeriod) {
      n += aMallocSizeOf(mIdlePeriod);
    }

    return n;
  }

  void SetIdleToken(uint64_t aId, TimeDuration aDuration);

  bool IsActive() { return mActive; }

  void EnsureIsActive() {
    if (!mActive) {
      SetActive();
    }
  }

  void EnsureIsPaused() {
    if (mActive) {
      SetPaused();
    }
  }

 private:
  EventQueuePriority SelectQueue(bool aUpdateState,
                                 const MutexAutoLock& aProofOfLock);

  // Returns a null TimeStamp if we're not in the idle period.
  mozilla::TimeStamp GetLocalIdleDeadline(bool& aShuttingDown);

  // SetActive should be called when the event queue is running any type of
  // tasks.
  void SetActive();
  // SetPaused should be called once the event queue doesn't have more
  // tasks to process, or is waiting for the idle token.
  void SetPaused();

  // Gets the idle token, which is the end time of the idle period.
  TimeStamp GetIdleToken(TimeStamp aLocalIdlePeriodHint);

  // In case of child processes, requests idle time from the cross-process
  // idle scheduler.
  void RequestIdleToken(TimeStamp aLocalIdlePeriodHint);

  // Returns true if the event queue either is waiting for an idle token
  // from the idle scheduler or has one.
  bool HasIdleRequest() { return mIdleRequestId != 0; }

  // Mark that the event queue doesn't have idle time to use, nor is expecting
  // to get idle token from the idle scheduler.
  void ClearIdleToken();

  UniquePtr<EventQueue> mHighQueue;
  UniquePtr<EventQueue> mInputQueue;
  UniquePtr<EventQueue> mMediumHighQueue;
  UniquePtr<EventQueue> mNormalQueue;
  UniquePtr<EventQueue> mDeferredTimersQueue;
  UniquePtr<EventQueue> mIdleQueue;

  // We need to drop the queue mutex when checking the idle deadline, so we keep
  // a pointer to it here.
  Mutex* mMutex = nullptr;

#ifndef RELEASE_OR_BETA
  // Pointer to a place where the most recently computed idle deadline is
  // stored.
  TimeStamp* mNextIdleDeadline = nullptr;
#endif

  // Try to process one high priority runnable after each normal
  // priority runnable. This gives the processing model HTML spec has for
  // 'Update the rendering' in the case only vsync messages are in the
  // secondary queue and prevents starving the normal queue.
  bool mProcessHighPriorityQueue = false;

  // mIdlePeriod keeps track of the current idle period. If at any
  // time the main event queue is empty, calling
  // mIdlePeriod->GetIdlePeriodHint() will give an estimate of when
  // the current idle period will end.
  nsCOMPtr<nsIIdlePeriod> mIdlePeriod;

  // Set to true if HasPendingEvents() has been called and returned true because
  // of a pending idle event.  This is used to remember to return that idle
  // event from GetIdleEvent() to ensure that HasPendingEvents() never lies.
  bool mHasPendingEventsPromisedIdleEvent = false;

  TimeStamp mInputHandlingStartTime;

  enum InputEventQueueState {
    STATE_DISABLED,
    STATE_FLUSHING,
    STATE_SUSPEND,
    STATE_ENABLED
  };
  InputEventQueueState mInputQueueState = STATE_DISABLED;

  // If non-null, tells the end time of the idle period.
  // Idle period starts when we get idle token from the parent process and
  // ends when either there are no runnables in the event queues or
  // mIdleToken < TimeStamp::Now()
  TimeStamp mIdleToken;

  // The id of the last idle request to the cross-process idle scheduler.
  uint64_t mIdleRequestId = 0;

  RefPtr<ipc::IdleSchedulerChild> mIdleScheduler;
  bool mIdleSchedulerInitialized = false;

  // mActive tells whether the event queue is running non-idle tasks.
  bool mActive = true;
};

}  // namespace mozilla

#endif  // mozilla_PrioritizedEventQueue_h
