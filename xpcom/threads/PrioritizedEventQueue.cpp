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
#include "MainThreadUtils.h"

using namespace mozilla;

PrioritizedEventQueue::PrioritizedEventQueue(
    already_AddRefed<nsIIdlePeriod>&& aIdlePeriod)
    : mHighQueue(MakeUnique<EventQueue>(EventQueuePriority::High)),
      mInputQueue(
          MakeUnique<EventQueueSized<32>>(EventQueuePriority::InputHigh)),
      mMediumHighQueue(MakeUnique<EventQueue>(EventQueuePriority::MediumHigh)),
      mNormalQueue(MakeUnique<EventQueueSized<64>>(EventQueuePriority::Normal)),
      mDeferredTimersQueue(
          MakeUnique<EventQueue>(EventQueuePriority::DeferredTimers)),
      mIdleQueue(MakeUnique<EventQueue>(EventQueuePriority::Idle)) {
  mInputTaskManager = new InputTaskManager();
  mIdleTaskManager = new IdleTaskManager(std::move(aIdlePeriod));
  TaskController::Get()->SetIdleTaskManager(mIdleTaskManager);
}

PrioritizedEventQueue::~PrioritizedEventQueue() = default;

void PrioritizedEventQueue::PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                                     EventQueuePriority aPriority,
                                     const MutexAutoLock& aProofOfLock,
                                     mozilla::TimeDuration* aDelay) {
  RefPtr<nsIRunnable> event(aEvent);

  if (UseTaskController()) {
    TaskController* tc = TaskController::Get();

    TaskManager* manager = nullptr;
    if (aPriority == EventQueuePriority::InputHigh) {
      if (mInputTaskManager->State() == InputTaskManager::STATE_DISABLED) {
        aPriority = EventQueuePriority::Normal;
      } else {
        manager = mInputTaskManager;
      }
    } else if (aPriority == EventQueuePriority::DeferredTimers ||
               aPriority == EventQueuePriority::Idle) {
      manager = mIdleTaskManager;
    }

    tc->DispatchRunnable(event.forget(), static_cast<uint32_t>(aPriority),
                         manager);
    return;
  }

  // Double check the priority with a QI.
  EventQueuePriority priority = aPriority;

  if (priority == EventQueuePriority::InputHigh &&
      mInputTaskManager->State() == InputTaskManager::STATE_DISABLED) {
    priority = EventQueuePriority::Normal;
  } else if (priority == EventQueuePriority::MediumHigh &&
             !StaticPrefs::threads_medium_high_event_queue_enabled()) {
    priority = EventQueuePriority::Normal;
  }

  switch (priority) {
    case EventQueuePriority::High:
      mHighQueue->PutEvent(event.forget(), priority, aProofOfLock, aDelay);
      break;
    case EventQueuePriority::InputHigh:
      mInputQueue->PutEvent(event.forget(), priority, aProofOfLock, aDelay);
      break;
    case EventQueuePriority::MediumHigh:
      mMediumHighQueue->PutEvent(event.forget(), priority, aProofOfLock,
                                 aDelay);
      break;
    case EventQueuePriority::Normal:
      mNormalQueue->PutEvent(event.forget(), priority, aProofOfLock, aDelay);
      break;
    case EventQueuePriority::InputLow:
      MOZ_ASSERT(false, "InputLow is a TaskController's internal priority!");
      break;
    case EventQueuePriority::DeferredTimers: {
      if (NS_IsMainThread()) {
        mDeferredTimersQueue->PutEvent(event.forget(), priority, aProofOfLock,
                                       aDelay);
      } else {
        // We don't want to touch our idle queues from off the main
        // thread.  Queue it indirectly.
        IndirectlyQueueRunnable(event.forget(), priority, aProofOfLock, aDelay);
      }
      break;
    }
    case EventQueuePriority::Idle: {
      if (NS_IsMainThread()) {
        mIdleQueue->PutEvent(event.forget(), priority, aProofOfLock, aDelay);
      } else {
        // We don't want to touch our idle queues from off the main
        // thread.  Queue it indirectly.
        IndirectlyQueueRunnable(event.forget(), priority, aProofOfLock, aDelay);
      }
      break;
    }
    case EventQueuePriority::Count:
      MOZ_CRASH("EventQueuePriority::Count isn't a valid priority");
      break;
  }
}

EventQueuePriority PrioritizedEventQueue::SelectQueue(
    bool aUpdateState, const MutexAutoLock& aProofOfLock) {
  size_t inputCount = mInputQueue->Count(aProofOfLock);

  if (aUpdateState &&
      mInputTaskManager->State() == InputTaskManager::STATE_ENABLED &&
      mInputTaskManager->InputHandlingStartTime().IsNull() && inputCount > 0) {
    mInputTaskManager->SetInputHandlingStartTime(
        InputEventStatistics::Get().GetInputHandlingStartTime(inputCount));
  }

  // We check the different queues in the following order. The conditions we use
  // are meant to avoid starvation and to ensure that we don't process an event
  // at the wrong time.
  //
  // HIGH:
  // INPUT: if inputCount > 0 && TimeStamp::Now() >
  //        mInputTaskManager->InputHandlingStartTime(...);
  // MEDIUMHIGH: if medium high
  // pending NORMAL: if normal pending
  //
  // If we still don't have an event, then we take events from the queues
  // in the following order:
  // INPUT
  // DEFERREDTIMERS: if GetLocalIdleDeadline()
  // IDLE: if GetLocalIdleDeadline()
  //
  // If we don't get an event in this pass, then we return null since no events
  // are ready.

  // This variable determines which queue we will take an event from.
  EventQueuePriority queue;
  if (!mHighQueue->IsEmpty(aProofOfLock)) {
    queue = EventQueuePriority::High;
  } else if (inputCount > 0 &&
             (mInputTaskManager->State() == InputTaskManager::STATE_FLUSHING ||
              (mInputTaskManager->State() == InputTaskManager::STATE_ENABLED &&
               !mInputTaskManager->InputHandlingStartTime().IsNull() &&
               TimeStamp::Now() >
                   mInputTaskManager->InputHandlingStartTime()))) {
    queue = EventQueuePriority::InputHigh;
  } else if (!mMediumHighQueue->IsEmpty(aProofOfLock)) {
    MOZ_ASSERT(
        mInputTaskManager->State() != InputTaskManager::STATE_FLUSHING,
        "Shouldn't consume medium high event when flushing input events");
    queue = EventQueuePriority::MediumHigh;
  } else if (!mNormalQueue->IsEmpty(aProofOfLock)) {
    MOZ_ASSERT(mInputTaskManager->State() != InputTaskManager::STATE_FLUSHING,
               "Shouldn't consume normal event when flushing input events");
    queue = EventQueuePriority::Normal;
  } else if (inputCount > 0 &&
             mInputTaskManager->State() != InputTaskManager::STATE_SUSPEND) {
    MOZ_ASSERT(
        mInputTaskManager->State() != InputTaskManager::STATE_DISABLED,
        "Shouldn't consume input events when the input queue is disabled");
    queue = EventQueuePriority::InputHigh;
  } else if (!mDeferredTimersQueue->IsEmpty(aProofOfLock)) {
    // We may not actually return an idle event in this case.
    queue = EventQueuePriority::DeferredTimers;
  } else {
    // We may not actually return an idle event in this case.
    queue = EventQueuePriority::Idle;
  }

  MOZ_ASSERT_IF(
      queue == EventQueuePriority::InputHigh ||
          queue == EventQueuePriority::InputLow,
      mInputTaskManager->State() != InputTaskManager::STATE_DISABLED &&
          mInputTaskManager->State() != InputTaskManager::STATE_SUSPEND);

  return queue;
}

void PrioritizedEventQueue::IndirectlyQueueRunnable(
    already_AddRefed<nsIRunnable>&& aEvent, EventQueuePriority aPriority,
    const MutexAutoLock& aProofOfLock, mozilla::TimeDuration* aDelay) {
  nsCOMPtr<nsIRunnable> event(aEvent);

  nsCOMPtr<nsIRunnable> mainThreadRunnable = NS_NewRunnableFunction(
      "idle runnable shim",
      [event = std::move(event), priority = aPriority]() mutable {
        NS_DispatchToCurrentThreadQueue(event.forget(), priority);
      });
  mNormalQueue->PutEvent(mainThreadRunnable.forget(),
                         EventQueuePriority::Normal, aProofOfLock, aDelay);
}

// The delay returned is the queuing delay a hypothetical Input event would
// see due to the current running event if it had arrived while the current
// event was queued.  This means that any event running at  priority below
// Input doesn't cause queuing delay for Input events, and we return
// TimeDuration() for those cases.
already_AddRefed<nsIRunnable> PrioritizedEventQueue::GetEvent(
    EventQueuePriority* aPriority, const MutexAutoLock& aProofOfLock,
    TimeDuration* aHypotheticalInputEventDelay) {
  MOZ_ASSERT_UNREACHABLE("Who is managing to call this?");
  bool ignored;
  return GetEvent(aPriority, aProofOfLock, aHypotheticalInputEventDelay,
                  &ignored);
}

already_AddRefed<nsIRunnable> PrioritizedEventQueue::GetEvent(
    EventQueuePriority* aPriority, const MutexAutoLock& aProofOfLock,
    TimeDuration* aHypotheticalInputEventDelay, bool* aIsIdleEvent) {
  EventQueuePriority queue = SelectQueue(true, aProofOfLock);

  if (aPriority) {
    *aPriority = queue;
  }
  *aIsIdleEvent = false;

  // Since Input events will only be delayed behind Input or High events,
  // the amount of time a lower-priority event spent in the queue is
  // irrelevant in knowing how long an input event would be delayed.
  // Alternatively, we could export the delay and let the higher-level code
  // key off the returned priority level (though then it'd need to know
  // if the thread's queue was a PrioritizedEventQueue or normal/other
  // EventQueue).
  nsCOMPtr<nsIRunnable> event;
  switch (queue) {
    default:
      MOZ_CRASH();
      break;

    case EventQueuePriority::High:
      event = mHighQueue->GetEvent(nullptr, aProofOfLock,
                                   aHypotheticalInputEventDelay);
      MOZ_ASSERT(event);
      mInputTaskManager->SetInputHandlingStartTime(TimeStamp());
      break;

    case EventQueuePriority::InputHigh:
      event = mInputQueue->GetEvent(nullptr, aProofOfLock,
                                    aHypotheticalInputEventDelay);
      MOZ_ASSERT(event);
      break;

      // All queue priorities below Input don't add their queuing time to the
      // time an input event will be delayed, so report 0 for time-in-queue
      // if we're below Input; input events will only be delayed by the time
      // an event actually runs (if the event is below Input event's priority)
    case EventQueuePriority::MediumHigh:
      event = mMediumHighQueue->GetEvent(nullptr, aProofOfLock);
      *aHypotheticalInputEventDelay = TimeDuration();
      break;

    case EventQueuePriority::Normal:
      event = mNormalQueue->GetEvent(nullptr, aProofOfLock);
      *aHypotheticalInputEventDelay = TimeDuration();
      break;

    case EventQueuePriority::Idle:
    case EventQueuePriority::DeferredTimers:
      *aHypotheticalInputEventDelay = TimeDuration();
      // If we get here, then all queues except deferredtimers and idle are
      // empty.

      if (!HasIdleRunnables(aProofOfLock)) {
        return nullptr;
      }

      TimeStamp idleDeadline =
          mIdleTaskManager->State().GetCachedIdleDeadline();
      if (!idleDeadline) {
        return nullptr;
      }

      event = mDeferredTimersQueue->GetEvent(nullptr, aProofOfLock);
      if (!event) {
        event = mIdleQueue->GetEvent(nullptr, aProofOfLock);
      }
      if (event) {
        *aIsIdleEvent = true;
        nsCOMPtr<nsIIdleRunnable> idleEvent = do_QueryInterface(event);
        if (idleEvent) {
          idleEvent->SetDeadline(idleDeadline);
        }
      }
      break;
  }  // switch (queue)

  if (!event) {
    *aHypotheticalInputEventDelay = TimeDuration();
  }

  MOZ_ASSERT_IF(aPriority, *aPriority == queue);

  return event.forget();
}

void PrioritizedEventQueue::DidRunEvent(const MutexAutoLock& aProofOfLock) {
  if (IsEmpty(aProofOfLock)) {
    // Certainly no more idle tasks.  Unlocking here is OK, because
    // our caller does nothing after this except return, which unlocks
    // its lock anyway.
    MutexAutoUnlock unlock(*mMutex);
    mIdleTaskManager->State().RanOutOfTasks(unlock);
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
  mIdleTaskManager->State().ForgetPendingTaskGuarantee();

  EventQueuePriority queue = SelectQueue(false, aProofOfLock);

  if (queue == EventQueuePriority::High) {
    return mHighQueue->HasReadyEvent(aProofOfLock);
  } else if (queue == EventQueuePriority::InputHigh) {
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

  // Temporarily unlock so we can peek our idle deadline.
  {
    MutexAutoUnlock unlock(*mMutex);
    mIdleTaskManager->State().CachePeekedIdleDeadline(unlock);
  }
  TimeStamp idleDeadline = mIdleTaskManager->State().GetCachedIdleDeadline();
  mIdleTaskManager->State().ClearCachedIdleDeadline();

  // Re-check the emptiness of the queues, since we had the lock released for a
  // bit.
  if (idleDeadline && (mDeferredTimersQueue->HasReadyEvent(aProofOfLock) ||
                       mIdleQueue->HasReadyEvent(aProofOfLock))) {
    mIdleTaskManager->State().EnforcePendingTaskGuarantee();
    return true;
  }

  return false;
}

bool PrioritizedEventQueue::HasPendingHighPriorityEvents(
    const MutexAutoLock& aProofOfLock) {
  return !mHighQueue->IsEmpty(aProofOfLock);
}

bool PrioritizedEventQueue::HasIdleRunnables(
    const MutexAutoLock& aProofOfLock) const {
  return !mIdleQueue->IsEmpty(aProofOfLock) ||
         !mDeferredTimersQueue->IsEmpty(aProofOfLock);
}

size_t PrioritizedEventQueue::Count(const MutexAutoLock& aProofOfLock) const {
  MOZ_CRASH("unimplemented");
}

void InputTaskManager::EnableInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInputQueueState == STATE_DISABLED);
  mInputQueueState = STATE_ENABLED;
  mInputHandlingStartTime = TimeStamp();
}

void InputTaskManager::FlushInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInputQueueState == STATE_ENABLED ||
             mInputQueueState == STATE_SUSPEND);
  mInputQueueState =
      mInputQueueState == STATE_ENABLED ? STATE_FLUSHING : STATE_SUSPEND;
}

void InputTaskManager::SuspendInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInputQueueState == STATE_ENABLED ||
             mInputQueueState == STATE_FLUSHING);
  mInputQueueState = STATE_SUSPEND;
}

void InputTaskManager::ResumeInputEventPrioritization() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInputQueueState == STATE_SUSPEND);
  mInputQueueState = STATE_ENABLED;
}

int32_t InputTaskManager::GetPriorityModifierForEventLoopTurn(
    const MutexAutoLock& aProofOfLock) {
  size_t inputCount = PendingTaskCount();
  if (State() == STATE_ENABLED && InputHandlingStartTime().IsNull() &&
      inputCount > 0) {
    SetInputHandlingStartTime(
        InputEventStatistics::Get().GetInputHandlingStartTime(inputCount));
  }

  if (inputCount > 0 && (State() == InputTaskManager::STATE_FLUSHING ||
                         (State() == InputTaskManager::STATE_ENABLED &&
                          !InputHandlingStartTime().IsNull() &&
                          TimeStamp::Now() > InputHandlingStartTime()))) {
    return 0;
  }

  int32_t modifier = static_cast<int32_t>(EventQueuePriority::InputLow) -
                     static_cast<int32_t>(EventQueuePriority::MediumHigh);
  return modifier;
}

void InputTaskManager::WillRunTask() {
  TaskManager::WillRunTask();
  mStartTimes.AppendElement(TimeStamp::Now());
}

void InputTaskManager::DidRunTask() {
  TaskManager::DidRunTask();
  MOZ_ASSERT(!mStartTimes.IsEmpty());
  TimeStamp start = mStartTimes.PopLastElement();
  InputEventStatistics::Get().UpdateInputDuration(TimeStamp::Now() - start);
}
