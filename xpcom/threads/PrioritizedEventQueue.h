/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PrioritizedEventQueue_h
#define mozilla_PrioritizedEventQueue_h

#include "mozilla/AbstractEventQueue.h"
#include "LabeledEventQueue.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIIdlePeriod.h"

class nsIRunnable;

namespace mozilla {

// This AbstractEventQueue implementation has one queue for each EventPriority.
// The type of queue used for each priority is determined by the template
// parameter.
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
template<class InnerQueueT>
class PrioritizedEventQueue final : public AbstractEventQueue
{
public:
  static const bool SupportsPrioritization = true;

  PrioritizedEventQueue(UniquePtr<InnerQueueT> aHighQueue,
                        UniquePtr<InnerQueueT> aInputQueue,
                        UniquePtr<InnerQueueT> aNormalQueue,
                        UniquePtr<InnerQueueT> aIdleQueue,
                        already_AddRefed<nsIIdlePeriod> aIdlePeriod);

  void PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventPriority aPriority,
                const MutexAutoLock& aProofOfLock) final;
  already_AddRefed<nsIRunnable> GetEvent(EventPriority* aPriority,
                                         const MutexAutoLock& aProofOfLock) final;

  bool IsEmpty(const MutexAutoLock& aProofOfLock) final;
  size_t Count(const MutexAutoLock& aProofOfLock) const final;
  bool HasReadyEvent(const MutexAutoLock& aProofOfLock) final;

  // When checking the idle deadline, we need to drop whatever mutex protects
  // this queue. This method allows that mutex to be stored so that we can drop
  // it and reacquire it when checking the idle deadline. The mutex must live at
  // least as long as the queue.
  void SetMutexRef(Mutex& aMutex) { mMutex = &aMutex; }

#ifndef RELEASE_OR_BETA
  // nsThread.cpp sends telemetry containing the most recently computed idle
  // deadline. We store a reference to a field in nsThread where this deadline
  // will be stored so that it can be fetched quickly for telemetry.
  void SetNextIdleDeadlineRef(TimeStamp& aDeadline) { mNextIdleDeadline = &aDeadline; }
#endif

  void EnableInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;
  void FlushInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;
  void SuspendInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;
  void ResumeInputEventPrioritization(const MutexAutoLock& aProofOfLock) final;

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override
  {
    size_t n = 0;

    n += mHighQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mInputQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mNormalQueue->SizeOfIncludingThis(aMallocSizeOf);
    n += mIdleQueue->SizeOfIncludingThis(aMallocSizeOf);

    if (mIdlePeriod) {
      n += aMallocSizeOf(mIdlePeriod);
    }

    return n;
  }

private:
  EventPriority SelectQueue(bool aUpdateState, const MutexAutoLock& aProofOfLock);

  // Returns a null TimeStamp if we're not in the idle period.
  mozilla::TimeStamp GetIdleDeadline();

  UniquePtr<InnerQueueT> mHighQueue;
  UniquePtr<InnerQueueT> mInputQueue;
  UniquePtr<InnerQueueT> mNormalQueue;
  UniquePtr<InnerQueueT> mIdleQueue;

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

  enum InputEventQueueState
  {
    STATE_DISABLED,
    STATE_FLUSHING,
    STATE_SUSPEND,
    STATE_ENABLED
  };
  InputEventQueueState mInputQueueState = STATE_DISABLED;
};

class EventQueue;
extern template class PrioritizedEventQueue<EventQueue>;
extern template class PrioritizedEventQueue<LabeledEventQueue>;

} // namespace mozilla

#endif // mozilla_PrioritizedEventQueue_h
