/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrioritizedEventQueue.h"
#include "mozilla/EventQueue.h"
#include "mozilla/ScopeExit.h"
#include "nsThreadManager.h"
#include "nsXPCOMPrivate.h" // for gXPCOMThreadsShutDown
#include "InputEventStatistics.h"

using namespace mozilla;

template<class InnerQueueT>
PrioritizedEventQueue<InnerQueueT>::PrioritizedEventQueue(UniquePtr<InnerQueueT> aHighQueue,
                                                          UniquePtr<InnerQueueT> aInputQueue,
                                                          UniquePtr<InnerQueueT> aNormalQueue,
                                                          UniquePtr<InnerQueueT> aIdleQueue,
                                                          already_AddRefed<nsIIdlePeriod> aIdlePeriod)
  : mHighQueue(Move(aHighQueue))
  , mInputQueue(Move(aInputQueue))
  , mNormalQueue(Move(aNormalQueue))
  , mIdleQueue(Move(aIdleQueue))
  , mIdlePeriod(aIdlePeriod)
{
  static_assert(IsBaseOf<AbstractEventQueue, InnerQueueT>::value,
                "InnerQueueT must be an AbstractEventQueue subclass");
}

template<class InnerQueueT>
void
PrioritizedEventQueue<InnerQueueT>::PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                                             EventPriority aPriority,
                                             const MutexAutoLock& aProofOfLock)
{
  // Double check the priority with a QI.
  RefPtr<nsIRunnable> event(aEvent);
  EventPriority priority = aPriority;
  if (nsCOMPtr<nsIRunnablePriority> runnablePrio = do_QueryInterface(event)) {
    uint32_t prio = nsIRunnablePriority::PRIORITY_NORMAL;
    runnablePrio->GetPriority(&prio);
    if (prio == nsIRunnablePriority::PRIORITY_HIGH) {
      priority = EventPriority::High;
    } else if (prio == nsIRunnablePriority::PRIORITY_INPUT) {
      priority = EventPriority::Input;
    }
  }

  if (priority == EventPriority::Input && mInputQueueState == STATE_DISABLED) {
    priority = EventPriority::Normal;
  }

  switch (priority) {
  case EventPriority::High:
    mHighQueue->PutEvent(event.forget(), priority, aProofOfLock);
    break;
  case EventPriority::Input:
    mInputQueue->PutEvent(event.forget(), priority, aProofOfLock);
    break;
  case EventPriority::Normal:
    mNormalQueue->PutEvent(event.forget(), priority, aProofOfLock);
    break;
  case EventPriority::Idle:
    mIdleQueue->PutEvent(event.forget(), priority, aProofOfLock);
    break;
  }
}

template<class InnerQueueT>
TimeStamp
PrioritizedEventQueue<InnerQueueT>::GetIdleDeadline()
{
  // If we are shutting down, we won't honor the idle period, and we will
  // always process idle runnables.  This will ensure that the idle queue
  // gets exhausted at shutdown time to prevent intermittently leaking
  // some runnables inside that queue and even worse potentially leaving
  // some important cleanup work unfinished.
  if (gXPCOMThreadsShutDown || nsThreadManager::get().GetCurrentThread()->ShuttingDown()) {
    return TimeStamp::Now();
  }

  TimeStamp idleDeadline;
  {
    // Releasing the lock temporarily since getting the idle period
    // might need to lock the timer thread. Unlocking here might make
    // us receive an event on the main queue, but we've committed to
    // run an idle event anyhow.
    MutexAutoUnlock unlock(*mMutex);
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
    // If HasPendingEvents() has been called and it has returned true, but we're no
    // longer in the idle period, we must return a valid timestamp to pretend that
    // we are still in the idle period.
    return TimeStamp::Now();
  }
  return idleDeadline;
}

template<class InnerQueueT>
EventPriority
PrioritizedEventQueue<InnerQueueT>::SelectQueue(bool aUpdateState,
                                                const MutexAutoLock& aProofOfLock)
{
  bool highPending = !mHighQueue->IsEmpty(aProofOfLock);
  bool normalPending = !mNormalQueue->IsEmpty(aProofOfLock);
  size_t inputCount = mInputQueue->Count(aProofOfLock);

  if (aUpdateState &&
      mInputQueueState == STATE_ENABLED &&
      mInputHandlingStartTime.IsNull() &&
      inputCount > 0) {
    mInputHandlingStartTime =
      InputEventStatistics::Get()
      .GetInputHandlingStartTime(inputCount);
  }

  // We check the different queues in the following order. The conditions we use
  // are meant to avoid starvation and to ensure that we don't process an event
  // at the wrong time.
  //
  // HIGH: if mProcessHighPriorityQueue
  // INPUT: if inputCount > 0 && TimeStamp::Now() > mInputHandlingStartTime
  // NORMAL: if normalPending
  //
  // If we still don't have an event, then we take events from the queues
  // in the following order:
  //
  // HIGH
  // INPUT
  // IDLE: if GetIdleDeadline()
  //
  // If we don't get an event in this pass, then we return null since no events
  // are ready.

  // This variable determines which queue we will take an event from.
  EventPriority queue;

  if (mProcessHighPriorityQueue) {
    queue = EventPriority::High;
  } else if (inputCount > 0 && (mInputQueueState == STATE_FLUSHING ||
                                (mInputQueueState == STATE_ENABLED &&
                                 !mInputHandlingStartTime.IsNull() &&
                                 TimeStamp::Now() > mInputHandlingStartTime))) {
    queue = EventPriority::Input;
  } else if (normalPending) {
    MOZ_ASSERT(mInputQueueState != STATE_FLUSHING,
               "Shouldn't consume normal event when flusing input events");
    queue = EventPriority::Normal;
  } else if (highPending) {
    queue = EventPriority::High;
  } else if (inputCount > 0 && mInputQueueState != STATE_SUSPEND) {
    MOZ_ASSERT(mInputQueueState != STATE_DISABLED,
               "Shouldn't consume input events when the input queue is disabled");
    queue = EventPriority::Input;
  } else {
    // We may not actually return an idle event in this case.
    queue = EventPriority::Idle;
  }

  MOZ_ASSERT_IF(queue == EventPriority::Input,
                mInputQueueState != STATE_DISABLED && mInputQueueState != STATE_SUSPEND);

  if (aUpdateState) {
    mProcessHighPriorityQueue = highPending;
  }

  return queue;
}

template<class InnerQueueT>
already_AddRefed<nsIRunnable>
PrioritizedEventQueue<InnerQueueT>::GetEvent(EventPriority* aPriority,
                                             const MutexAutoLock& aProofOfLock)
{
  MakeScopeExit([&] {
    mHasPendingEventsPromisedIdleEvent = false;
  });

#ifndef RELEASE_OR_BETA
  // Clear mNextIdleDeadline so that it is possible to determine that
  // we're running an idle runnable in ProcessNextEvent.
  *mNextIdleDeadline = TimeStamp();
#endif

  EventPriority queue = SelectQueue(true, aProofOfLock);

  if (aPriority) {
    *aPriority = queue;
  }

  if (queue == EventPriority::High) {
    nsCOMPtr<nsIRunnable> event = mHighQueue->GetEvent(aPriority, aProofOfLock);
    MOZ_ASSERT(event);
    mInputHandlingStartTime = TimeStamp();
    mProcessHighPriorityQueue = false;
    return event.forget();
  }

  if (queue == EventPriority::Input) {
    nsCOMPtr<nsIRunnable> event = mInputQueue->GetEvent(aPriority, aProofOfLock);
    MOZ_ASSERT(event);
    return event.forget();
  }

  if (queue == EventPriority::Normal) {
    nsCOMPtr<nsIRunnable> event = mNormalQueue->GetEvent(aPriority, aProofOfLock);
    return event.forget();
  }

  // If we get here, then all queues except idle are empty.
  MOZ_ASSERT(queue == EventPriority::Idle);

  if (mIdleQueue->IsEmpty(aProofOfLock)) {
    MOZ_ASSERT(!mHasPendingEventsPromisedIdleEvent);
    return nullptr;
  }

  TimeStamp idleDeadline = GetIdleDeadline();
  if (!idleDeadline) {
    return nullptr;
  }

  nsCOMPtr<nsIRunnable> event = mIdleQueue->GetEvent(aPriority, aProofOfLock);
  if (event) {
    nsCOMPtr<nsIIdleRunnable> idleEvent = do_QueryInterface(event);
    if (idleEvent) {
      idleEvent->SetDeadline(idleDeadline);
    }

#ifndef RELEASE_OR_BETA
    // Store the next idle deadline to be able to determine budget use
    // in ProcessNextEvent.
    *mNextIdleDeadline = idleDeadline;
#endif
  }

  return event.forget();
}

template<class InnerQueueT>
bool
PrioritizedEventQueue<InnerQueueT>::IsEmpty(const MutexAutoLock& aProofOfLock)
{
  // Just check IsEmpty() on the sub-queues. Don't bother checking the idle
  // deadline since that only determines whether an idle event is ready or not.
  return mHighQueue->IsEmpty(aProofOfLock)
      && mInputQueue->IsEmpty(aProofOfLock)
      && mNormalQueue->IsEmpty(aProofOfLock)
      && mIdleQueue->IsEmpty(aProofOfLock);
}

template<class InnerQueueT>
bool
PrioritizedEventQueue<InnerQueueT>::HasReadyEvent(const MutexAutoLock& aProofOfLock)
{
  mHasPendingEventsPromisedIdleEvent = false;

  EventPriority queue = SelectQueue(false, aProofOfLock);

  if (queue == EventPriority::High) {
    return mHighQueue->HasReadyEvent(aProofOfLock);
  } else if (queue == EventPriority::Input) {
    return mInputQueue->HasReadyEvent(aProofOfLock);
  } else if (queue == EventPriority::Normal) {
    return mNormalQueue->HasReadyEvent(aProofOfLock);
  }

  MOZ_ASSERT(queue == EventPriority::Idle);

  // If we get here, then both the high and normal queues are empty.

  if (mIdleQueue->IsEmpty(aProofOfLock)) {
    return false;
  }

  TimeStamp idleDeadline = GetIdleDeadline();
  if (idleDeadline && mIdleQueue->HasReadyEvent(aProofOfLock)) {
    mHasPendingEventsPromisedIdleEvent = true;
    return true;
  }

  return false;
}

template<class InnerQueueT>
size_t
PrioritizedEventQueue<InnerQueueT>::Count(const MutexAutoLock& aProofOfLock) const
{
  MOZ_CRASH("unimplemented");
}

template<class InnerQueueT>
void
PrioritizedEventQueue<InnerQueueT>::EnableInputEventPrioritization(const MutexAutoLock& aProofOfLock)
{
  MOZ_ASSERT(mInputQueueState == STATE_DISABLED);
  mInputQueueState = STATE_ENABLED;
  mInputHandlingStartTime = TimeStamp();
}

template<class InnerQueueT>
void
PrioritizedEventQueue<InnerQueueT>::
FlushInputEventPrioritization(const MutexAutoLock& aProofOfLock)
{
  MOZ_ASSERT(mInputQueueState == STATE_ENABLED || mInputQueueState == STATE_SUSPEND);
  mInputQueueState =
    mInputQueueState == STATE_ENABLED ? STATE_FLUSHING : STATE_SUSPEND;
}

template<class InnerQueueT>
void
PrioritizedEventQueue<InnerQueueT>::
SuspendInputEventPrioritization(const MutexAutoLock& aProofOfLock)
{
  MOZ_ASSERT(mInputQueueState == STATE_ENABLED || mInputQueueState == STATE_FLUSHING);
  mInputQueueState = STATE_SUSPEND;
}

template<class InnerQueueT>
void
PrioritizedEventQueue<InnerQueueT>::
ResumeInputEventPrioritization(const MutexAutoLock& aProofOfLock)
{
  MOZ_ASSERT(mInputQueueState == STATE_SUSPEND);
  mInputQueueState = STATE_ENABLED;
}

namespace mozilla {
template class PrioritizedEventQueue<EventQueue>;
template class PrioritizedEventQueue<LabeledEventQueue>;
}
