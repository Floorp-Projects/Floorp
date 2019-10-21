/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PrioritizedEventQueue.h"
#include "mozilla/EventQueue.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_threads.h"
#include "mozilla/ipc/IdleSchedulerChild.h"
#include "nsThreadManager.h"
#include "nsXPCOMPrivate.h"  // for gXPCOMThreadsShutDown
#include "InputEventStatistics.h"

using namespace mozilla;

PrioritizedEventQueue::PrioritizedEventQueue(
    already_AddRefed<nsIIdlePeriod>&& aIdlePeriod)
    : mHighQueue(MakeUnique<EventQueue>(EventQueuePriority::High)),
      mInputQueue(MakeUnique<EventQueue>(EventQueuePriority::Input)),
      mMediumHighQueue(MakeUnique<EventQueue>(EventQueuePriority::MediumHigh)),
      mNormalQueue(MakeUnique<EventQueue>(EventQueuePriority::Normal)),
      mDeferredTimersQueue(
          MakeUnique<EventQueue>(EventQueuePriority::DeferredTimers)),
      mIdleQueue(MakeUnique<EventQueue>(EventQueuePriority::Idle)),
      mIdlePeriodState(std::move(aIdlePeriod)) {}

PrioritizedEventQueue::~PrioritizedEventQueue() = default;

void PrioritizedEventQueue::PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                                     EventQueuePriority aPriority,
                                     const MutexAutoLock& aProofOfLock) {
  // Double check the priority with a QI.
  RefPtr<nsIRunnable> event(aEvent);
  EventQueuePriority priority = aPriority;

  if (priority == EventQueuePriority::Input &&
      mInputQueueState == STATE_DISABLED) {
    priority = EventQueuePriority::Normal;
  } else if (priority == EventQueuePriority::MediumHigh &&
             !StaticPrefs::threads_medium_high_event_queue_enabled()) {
    priority = EventQueuePriority::Normal;
  }

  switch (priority) {
    case EventQueuePriority::High:
      mHighQueue->PutEvent(event.forget(), priority, aProofOfLock);
      break;
    case EventQueuePriority::Input:
      mInputQueue->PutEvent(event.forget(), priority, aProofOfLock);
      break;
    case EventQueuePriority::MediumHigh:
      mMediumHighQueue->PutEvent(event.forget(), priority, aProofOfLock);
      break;
    case EventQueuePriority::Normal:
      mNormalQueue->PutEvent(event.forget(), priority, aProofOfLock);
      break;
    case EventQueuePriority::DeferredTimers:
      mDeferredTimersQueue->PutEvent(event.forget(), priority, aProofOfLock);
      break;
    case EventQueuePriority::Idle:
      mIdleQueue->PutEvent(event.forget(), priority, aProofOfLock);
      break;
    case EventQueuePriority::Count:
      MOZ_CRASH("EventQueuePriority::Count isn't a valid priority");
      break;
  }
}

EventQueuePriority PrioritizedEventQueue::SelectQueue(
    bool aUpdateState, const MutexAutoLock& aProofOfLock) {
  size_t inputCount = mInputQueue->Count(aProofOfLock);

  if (aUpdateState && mInputQueueState == STATE_ENABLED &&
      mInputHandlingStartTime.IsNull() && inputCount > 0) {
    mInputHandlingStartTime =
        InputEventStatistics::Get().GetInputHandlingStartTime(inputCount);
  }

  // We check the different queues in the following order. The conditions we use
  // are meant to avoid starvation and to ensure that we don't process an event
  // at the wrong time.
  //
  // HIGH: if mProcessHighPriorityQueue
  // INPUT: if inputCount > 0 && TimeStamp::Now() > mInputHandlingStartTime
  // MEDIUMHIGH: if medium high pending
  // NORMAL: if normal pending
  //
  // If we still don't have an event, then we take events from the queues
  // in the following order:
  //
  // HIGH
  // INPUT
  // DEFERREDTIMERS: if GetLocalIdleDeadline()
  // IDLE: if GetLocalIdleDeadline()
  //
  // If we don't get an event in this pass, then we return null since no events
  // are ready.

  // This variable determines which queue we will take an event from.
  EventQueuePriority queue;
  bool highPending = !mHighQueue->IsEmpty(aProofOfLock);

  if (mProcessHighPriorityQueue) {
    queue = EventQueuePriority::High;
  } else if (inputCount > 0 && (mInputQueueState == STATE_FLUSHING ||
                                (mInputQueueState == STATE_ENABLED &&
                                 !mInputHandlingStartTime.IsNull() &&
                                 TimeStamp::Now() > mInputHandlingStartTime))) {
    queue = EventQueuePriority::Input;
  } else if (!mMediumHighQueue->IsEmpty(aProofOfLock)) {
    MOZ_ASSERT(
        mInputQueueState != STATE_FLUSHING,
        "Shouldn't consume medium high event when flushing input events");
    queue = EventQueuePriority::MediumHigh;
  } else if (!mNormalQueue->IsEmpty(aProofOfLock)) {
    MOZ_ASSERT(mInputQueueState != STATE_FLUSHING,
               "Shouldn't consume normal event when flushing input events");
    queue = EventQueuePriority::Normal;
  } else if (highPending) {
    queue = EventQueuePriority::High;
  } else if (inputCount > 0 && mInputQueueState != STATE_SUSPEND) {
    MOZ_ASSERT(
        mInputQueueState != STATE_DISABLED,
        "Shouldn't consume input events when the input queue is disabled");
    queue = EventQueuePriority::Input;
  } else if (!mDeferredTimersQueue->IsEmpty(aProofOfLock)) {
    // We may not actually return an idle event in this case.
    queue = EventQueuePriority::DeferredTimers;
  } else {
    // We may not actually return an idle event in this case.
    queue = EventQueuePriority::Idle;
  }

  MOZ_ASSERT_IF(
      queue == EventQueuePriority::Input,
      mInputQueueState != STATE_DISABLED && mInputQueueState != STATE_SUSPEND);

  if (aUpdateState) {
    mProcessHighPriorityQueue = highPending;
  }

  return queue;
}

already_AddRefed<nsIRunnable> PrioritizedEventQueue::GetEvent(
    EventQueuePriority* aPriority, const MutexAutoLock& aProofOfLock) {
#ifndef RELEASE_OR_BETA
  // Clear mNextIdleDeadline so that it is possible to determine that
  // we're running an idle runnable in ProcessNextEvent.
  *mNextIdleDeadline = TimeStamp();
#endif

  EventQueuePriority queue = SelectQueue(true, aProofOfLock);
  auto guard = MakeScopeExit([&] {
    mIdlePeriodState.ForgetPendingTaskGuarantee();
    if (queue != EventQueuePriority::Idle &&
        queue != EventQueuePriority::DeferredTimers) {
      mIdlePeriodState.FlagNotIdle(*mMutex);
    }
  });

  if (aPriority) {
    *aPriority = queue;
  }

  if (queue == EventQueuePriority::High) {
    nsCOMPtr<nsIRunnable> event = mHighQueue->GetEvent(aPriority, aProofOfLock);
    MOZ_ASSERT(event);
    mInputHandlingStartTime = TimeStamp();
    mProcessHighPriorityQueue = false;
    return event.forget();
  }

  if (queue == EventQueuePriority::Input) {
    nsCOMPtr<nsIRunnable> event =
        mInputQueue->GetEvent(aPriority, aProofOfLock);
    MOZ_ASSERT(event);
    return event.forget();
  }

  if (queue == EventQueuePriority::MediumHigh) {
    nsCOMPtr<nsIRunnable> event =
        mMediumHighQueue->GetEvent(aPriority, aProofOfLock);
    return event.forget();
  }

  if (queue == EventQueuePriority::Normal) {
    nsCOMPtr<nsIRunnable> event =
        mNormalQueue->GetEvent(aPriority, aProofOfLock);
    return event.forget();
  }

  // If we get here, then all queues except deferredtimers and idle are empty.
  MOZ_ASSERT(queue == EventQueuePriority::Idle ||
             queue == EventQueuePriority::DeferredTimers);

  if (mIdleQueue->IsEmpty(aProofOfLock) &&
      mDeferredTimersQueue->IsEmpty(aProofOfLock)) {
    mIdlePeriodState.RanOutOfTasks(*mMutex);
    return nullptr;
  }

  TimeStamp idleDeadline = mIdlePeriodState.GetDeadlineForIdleTask(*mMutex);
  if (!idleDeadline) {
    return nullptr;
  }

  nsCOMPtr<nsIRunnable> event =
      mDeferredTimersQueue->GetEvent(aPriority, aProofOfLock);
  if (!event) {
    event = mIdleQueue->GetEvent(aPriority, aProofOfLock);
  }
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

void PrioritizedEventQueue::DidRunEvent(const MutexAutoLock& aProofOfLock) {
  if (IsEmpty(aProofOfLock)) {
    // Certainly no more idle tasks.
    mIdlePeriodState.RanOutOfTasks(*mMutex);
  }
}

bool PrioritizedEventQueue::IsEmpty(const MutexAutoLock& aProofOfLock) {
  // Just check IsEmpty() on the sub-queues. Don't bother checking the idle
  // deadline since that only determines whether an idle event is ready or not.
  return mHighQueue->IsEmpty(aProofOfLock) &&
         mInputQueue->IsEmpty(aProofOfLock) &&
         mMediumHighQueue->IsEmpty(aProofOfLock) &&
         mNormalQueue->IsEmpty(aProofOfLock) &&
         mDeferredTimersQueue->IsEmpty(aProofOfLock) &&
         mIdleQueue->IsEmpty(aProofOfLock);
}

bool PrioritizedEventQueue::HasReadyEvent(const MutexAutoLock& aProofOfLock) {
  mIdlePeriodState.ForgetPendingTaskGuarantee();

  EventQueuePriority queue = SelectQueue(false, aProofOfLock);

  if (queue == EventQueuePriority::High) {
    return mHighQueue->HasReadyEvent(aProofOfLock);
  } else if (queue == EventQueuePriority::Input) {
    return mInputQueue->HasReadyEvent(aProofOfLock);
  } else if (queue == EventQueuePriority::MediumHigh) {
    return mMediumHighQueue->HasReadyEvent(aProofOfLock);
  } else if (queue == EventQueuePriority::Normal) {
    return mNormalQueue->HasReadyEvent(aProofOfLock);
  }

  MOZ_ASSERT(queue == EventQueuePriority::Idle ||
             queue == EventQueuePriority::DeferredTimers);

  // If we get here, then both the high and normal queues are empty.

  if (mDeferredTimersQueue->IsEmpty(aProofOfLock) &&
      mIdleQueue->IsEmpty(aProofOfLock)) {
    return false;
  }

  TimeStamp idleDeadline = mIdlePeriodState.PeekIdleDeadline(*mMutex);
  if (idleDeadline && (mDeferredTimersQueue->HasReadyEvent(aProofOfLock) ||
                       mIdleQueue->HasReadyEvent(aProofOfLock))) {
    mIdlePeriodState.EnforcePendingTaskGuarantee();
    return true;
  }

  return false;
}

bool PrioritizedEventQueue::HasPendingHighPriorityEvents(
    const MutexAutoLock& aProofOfLock) {
  return !mHighQueue->IsEmpty(aProofOfLock);
}

size_t PrioritizedEventQueue::Count(const MutexAutoLock& aProofOfLock) const {
  MOZ_CRASH("unimplemented");
}

void PrioritizedEventQueue::EnableInputEventPrioritization(
    const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(mInputQueueState == STATE_DISABLED);
  mInputQueueState = STATE_ENABLED;
  mInputHandlingStartTime = TimeStamp();
}

void PrioritizedEventQueue::FlushInputEventPrioritization(
    const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(mInputQueueState == STATE_ENABLED ||
             mInputQueueState == STATE_SUSPEND);
  mInputQueueState =
      mInputQueueState == STATE_ENABLED ? STATE_FLUSHING : STATE_SUSPEND;
}

void PrioritizedEventQueue::SuspendInputEventPrioritization(
    const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(mInputQueueState == STATE_ENABLED ||
             mInputQueueState == STATE_FLUSHING);
  mInputQueueState = STATE_SUSPEND;
}

void PrioritizedEventQueue::ResumeInputEventPrioritization(
    const MutexAutoLock& aProofOfLock) {
  MOZ_ASSERT(mInputQueueState == STATE_SUSPEND);
  mInputQueueState = STATE_ENABLED;
}
