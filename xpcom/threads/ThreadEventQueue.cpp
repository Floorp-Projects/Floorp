/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ThreadEventQueue.h"
#include "mozilla/EventQueue.h"

#include "LeakRefPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIThreadInternal.h"
#include "nsThreadUtils.h"
#include "PrioritizedEventQueue.h"
#include "ThreadEventTarget.h"

using namespace mozilla;

template <class InnerQueueT>
class ThreadEventQueue<InnerQueueT>::NestedSink : public ThreadTargetSink {
 public:
  NestedSink(EventQueue* aQueue, ThreadEventQueue* aOwner)
      : mQueue(aQueue), mOwner(aOwner) {}

  bool PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventQueuePriority aPriority) final {
    return mOwner->PutEventInternal(std::move(aEvent), aPriority, this);
  }

  void Disconnect(const MutexAutoLock& aProofOfLock) final { mQueue = nullptr; }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    if (mQueue) {
      return mQueue->SizeOfIncludingThis(aMallocSizeOf);
    }
    return 0;
  }

 private:
  friend class ThreadEventQueue;

  // This is a non-owning reference. It must live at least until Disconnect is
  // called to clear it out.
  EventQueue* mQueue;
  RefPtr<ThreadEventQueue> mOwner;
};

template <class InnerQueueT>
ThreadEventQueue<InnerQueueT>::ThreadEventQueue(UniquePtr<InnerQueueT> aQueue)
    : mBaseQueue(std::move(aQueue)),
      mLock("ThreadEventQueue"),
      mEventsAvailable(mLock, "EventsAvail") {
  static_assert(std::is_base_of<AbstractEventQueue, InnerQueueT>::value,
                "InnerQueueT must be an AbstractEventQueue subclass");
}

template <class InnerQueueT>
ThreadEventQueue<InnerQueueT>::~ThreadEventQueue() {
  MOZ_ASSERT(mNestedQueues.IsEmpty());
}

template <class InnerQueueT>
bool ThreadEventQueue<InnerQueueT>::PutEvent(
    already_AddRefed<nsIRunnable>&& aEvent, EventQueuePriority aPriority) {
  return PutEventInternal(std::move(aEvent), aPriority, nullptr);
}

template <class InnerQueueT>
bool ThreadEventQueue<InnerQueueT>::PutEventInternal(
    already_AddRefed<nsIRunnable>&& aEvent, EventQueuePriority aPriority,
    NestedSink* aSink) {
  // We want to leak the reference when we fail to dispatch it, so that
  // we won't release the event in a wrong thread.
  LeakRefPtr<nsIRunnable> event(std::move(aEvent));
  nsCOMPtr<nsIThreadObserver> obs;

  {
    // Check if the runnable wants to override the passed-in priority.
    // Do this outside the lock, so runnables implemented in JS can QI
    // (and possibly GC) outside of the lock.
    if (InnerQueueT::SupportsPrioritization) {
      auto* e = event.get();  // can't do_QueryInterface on LeakRefPtr.
      if (nsCOMPtr<nsIRunnablePriority> runnablePrio = do_QueryInterface(e)) {
        uint32_t prio = nsIRunnablePriority::PRIORITY_NORMAL;
        runnablePrio->GetPriority(&prio);
        if (prio == nsIRunnablePriority::PRIORITY_HIGH) {
          aPriority = EventQueuePriority::High;
        } else if (prio == nsIRunnablePriority::PRIORITY_INPUT_HIGH) {
          aPriority = EventQueuePriority::Input;
        } else if (prio == nsIRunnablePriority::PRIORITY_MEDIUMHIGH) {
          aPriority = EventQueuePriority::MediumHigh;
        } else if (prio == nsIRunnablePriority::PRIORITY_DEFERRED_TIMERS) {
          aPriority = EventQueuePriority::DeferredTimers;
        } else if (prio == nsIRunnablePriority::PRIORITY_IDLE) {
          aPriority = EventQueuePriority::Idle;
        }
      }
    }

    MutexAutoLock lock(mLock);

    if (mEventsAreDoomed) {
      return false;
    }

    if (aSink) {
      if (!aSink->mQueue) {
        return false;
      }

      aSink->mQueue->PutEvent(event.take(), aPriority, lock);
    } else {
      mBaseQueue->PutEvent(event.take(), aPriority, lock);
    }

    mEventsAvailable.Notify();

    // Make sure to grab the observer before dropping the lock, otherwise the
    // event that we just placed into the queue could run and eventually delete
    // this nsThread before the calling thread is scheduled again. We would then
    // crash while trying to access a dead nsThread.
    obs = mObserver;
  }

  if (obs) {
    obs->OnDispatchedEvent();
  }

  return true;
}

template <class InnerQueueT>
already_AddRefed<nsIRunnable> ThreadEventQueue<InnerQueueT>::GetEvent(
    bool aMayWait, EventQueuePriority* aPriority,
    mozilla::TimeDuration* aLastEventDelay) {
  nsCOMPtr<nsIRunnable> event;
  bool eventIsIdleRunnable = false;
  // This will be the IdlePeriodState for the queue the event, if any,
  // came from.  May be null all along.
  IdlePeriodState* idleState = nullptr;

  {
    // Scope for lock.  When we are about to return, we will exit this
    // scope so we can do some work after releasing the lock but
    // before returning.
    MutexAutoLock lock(mLock);

    for (;;) {
      const bool noNestedQueue = mNestedQueues.IsEmpty();
      if (noNestedQueue) {
        idleState = mBaseQueue->GetIdlePeriodState();
        event = mBaseQueue->GetEvent(aPriority, lock, aLastEventDelay,
                                     &eventIsIdleRunnable);
      } else {
        // We always get events from the topmost queue when there are nested
        // queues.
        MOZ_ASSERT(!mNestedQueues.LastElement().mQueue->GetIdlePeriodState());
        event = mNestedQueues.LastElement().mQueue->GetEvent(
            aPriority, lock, aLastEventDelay, &eventIsIdleRunnable);
        MOZ_ASSERT(!eventIsIdleRunnable);
      }

      if (event) {
        break;
      }

      if (idleState) {
        MOZ_ASSERT(noNestedQueue);
        if (mBaseQueue->HasIdleRunnables(lock)) {
          // We have idle runnables that we may not have gotten above because
          // our idle state is not up to date.  We need to update the idle state
          // and try again.  We need to temporarily release the lock while we do
          // that.
          MutexAutoUnlock unlock(mLock);
          idleState->UpdateCachedIdleDeadline(unlock);
        } else {
          // We need to notify our idle state that we're out of tasks to run.
          // This needs to be done while not holding the lock.
          MutexAutoUnlock unlock(mLock);
          idleState->RanOutOfTasks(unlock);
        }

        // When we unlocked, someone may have queued a new runnable on us.  So
        // we _must_ try to get a runnable again before we start sleeping, since
        // that might be the runnable we were waiting for.
        MOZ_ASSERT(
            noNestedQueue == mNestedQueues.IsEmpty(),
            "Who is pushing nested queues on us from some other thread?");
        event = mBaseQueue->GetEvent(aPriority, lock, aLastEventDelay,
                                     &eventIsIdleRunnable);
        // Now clear the cached idle deadline, because it was specific to this
        // GetEvent() call.
        idleState->ClearCachedIdleDeadline();

        if (event) {
          break;
        }
      }

      // No runnable available.  Sleep waiting for one if if we're supposed to.
      // Otherwise just go ahead and return null.
      if (!aMayWait) {
        break;
      }

      AUTO_PROFILER_LABEL("ThreadEventQueue::GetEvent::Wait", IDLE);
      mEventsAvailable.Wait();
    }
  }

  if (idleState) {
    // The pending task guarantee is not needed anymore, since we just tried
    // doing GetEvent().
    idleState->ForgetPendingTaskGuarantee();
    if (event && !eventIsIdleRunnable) {
      // We don't have a MutexAutoUnlock to pass to the callee here.  We _could_
      // have one if we wanted to, simply by moving this into the same scope as
      // our MutexAutoLock and adding a MutexAutoUnlock, but then we'd be doing
      // an extra lock/unlock pair on mLock, which seems uncalled-for.
      idleState->FlagNotIdle();
    }
  }

  return event.forget();
}

template <class InnerQueueT>
void ThreadEventQueue<InnerQueueT>::DidRunEvent() {
  MutexAutoLock lock(mLock);
  if (mNestedQueues.IsEmpty()) {
    mBaseQueue->DidRunEvent(lock);
    // Don't do anything else here, because that call might have
    // temporarily unlocked the lock.
  } else {
    mNestedQueues.LastElement().mQueue->DidRunEvent(lock);
  }
}

template <class InnerQueueT>
bool ThreadEventQueue<InnerQueueT>::HasPendingEvent() {
  MutexAutoLock lock(mLock);

  // We always get events from the topmost queue when there are nested queues.
  if (mNestedQueues.IsEmpty()) {
    return mBaseQueue->HasReadyEvent(lock);
  } else {
    return mNestedQueues.LastElement().mQueue->HasReadyEvent(lock);
  }
}

template <class InnerQueueT>
bool ThreadEventQueue<InnerQueueT>::HasPendingHighPriorityEvents() {
  MutexAutoLock lock(mLock);

  // We always get events from the topmost queue when there are nested queues.
  if (mNestedQueues.IsEmpty()) {
    return mBaseQueue->HasPendingHighPriorityEvents(lock);
  } else {
    return mNestedQueues.LastElement().mQueue->HasPendingHighPriorityEvents(
        lock);
  }
}

template <class InnerQueueT>
bool ThreadEventQueue<InnerQueueT>::ShutdownIfNoPendingEvents() {
  MutexAutoLock lock(mLock);
  if (mNestedQueues.IsEmpty() && mBaseQueue->IsEmpty(lock)) {
    mEventsAreDoomed = true;
    return true;
  }
  return false;
}

template <class InnerQueueT>
void ThreadEventQueue<InnerQueueT>::EnableInputEventPrioritization() {
  MutexAutoLock lock(mLock);
  mBaseQueue->EnableInputEventPrioritization(lock);
}

template <class InnerQueueT>
void ThreadEventQueue<InnerQueueT>::FlushInputEventPrioritization() {
  MutexAutoLock lock(mLock);
  mBaseQueue->FlushInputEventPrioritization(lock);
}

template <class InnerQueueT>
void ThreadEventQueue<InnerQueueT>::SuspendInputEventPrioritization() {
  MutexAutoLock lock(mLock);
  mBaseQueue->SuspendInputEventPrioritization(lock);
}

template <class InnerQueueT>
void ThreadEventQueue<InnerQueueT>::ResumeInputEventPrioritization() {
  MutexAutoLock lock(mLock);
  mBaseQueue->ResumeInputEventPrioritization(lock);
}

template <class InnerQueueT>
already_AddRefed<nsISerialEventTarget>
ThreadEventQueue<InnerQueueT>::PushEventQueue() {
  auto queue = MakeUnique<EventQueue>();
  RefPtr<NestedSink> sink = new NestedSink(queue.get(), this);
  RefPtr<ThreadEventTarget> eventTarget =
      new ThreadEventTarget(sink, NS_IsMainThread());

  MutexAutoLock lock(mLock);

  mNestedQueues.AppendElement(NestedQueueItem(std::move(queue), eventTarget));
  return eventTarget.forget();
}

template <class InnerQueueT>
void ThreadEventQueue<InnerQueueT>::PopEventQueue(nsIEventTarget* aTarget) {
  MutexAutoLock lock(mLock);

  MOZ_ASSERT(!mNestedQueues.IsEmpty());

  NestedQueueItem& item = mNestedQueues.LastElement();

  MOZ_ASSERT(aTarget == item.mEventTarget);

  // Disconnect the event target that will be popped.
  item.mEventTarget->Disconnect(lock);

  AbstractEventQueue* prevQueue =
      mNestedQueues.Length() == 1
          ? static_cast<AbstractEventQueue*>(mBaseQueue.get())
          : static_cast<AbstractEventQueue*>(
                mNestedQueues[mNestedQueues.Length() - 2].mQueue.get());

  // Move events from the old queue to the new one.
  nsCOMPtr<nsIRunnable> event;
  EventQueuePriority prio;
  TimeDuration delay;
  while ((event = item.mQueue->GetEvent(&prio, lock, &delay))) {
    // preserve the event delay so far
    prevQueue->PutEvent(event.forget(), prio, lock, &delay);
  }

  mNestedQueues.RemoveLastElement();
}

template <class InnerQueueT>
size_t ThreadEventQueue<InnerQueueT>::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;

  n += mBaseQueue->SizeOfIncludingThis(aMallocSizeOf);

  n += mNestedQueues.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto& queue : mNestedQueues) {
    n += queue.mEventTarget->SizeOfIncludingThis(aMallocSizeOf);
  }

  return SynchronizedEventQueue::SizeOfExcludingThis(aMallocSizeOf) + n;
}

template <class InnerQueueT>
already_AddRefed<nsIThreadObserver>
ThreadEventQueue<InnerQueueT>::GetObserver() {
  MutexAutoLock lock(mLock);
  return do_AddRef(mObserver);
}

template <class InnerQueueT>
already_AddRefed<nsIThreadObserver>
ThreadEventQueue<InnerQueueT>::GetObserverOnThread() {
  return do_AddRef(mObserver);
}

template <class InnerQueueT>
void ThreadEventQueue<InnerQueueT>::SetObserver(nsIThreadObserver* aObserver) {
  MutexAutoLock lock(mLock);
  mObserver = aObserver;
}

namespace mozilla {
template class ThreadEventQueue<EventQueue>;
template class ThreadEventQueue<PrioritizedEventQueue>;
}  // namespace mozilla
