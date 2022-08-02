/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ThreadEventQueue.h"
#include "mozilla/EventQueue.h"

#include "LeakRefPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsITargetShutdownTask.h"
#include "nsIThreadInternal.h"
#include "nsThreadUtils.h"
#include "nsThread.h"
#include "ThreadEventTarget.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/TaskController.h"
#include "mozilla/StaticPrefs_threads.h"

using namespace mozilla;

class ThreadEventQueue::NestedSink : public ThreadTargetSink {
 public:
  NestedSink(EventQueue* aQueue, ThreadEventQueue* aOwner)
      : mQueue(aQueue), mOwner(aOwner) {}

  bool PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventQueuePriority aPriority) final {
    return mOwner->PutEventInternal(std::move(aEvent), aPriority, this);
  }

  void Disconnect(const MutexAutoLock& aProofOfLock) final { mQueue = nullptr; }

  nsresult RegisterShutdownTask(nsITargetShutdownTask* aTask) final {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  nsresult UnregisterShutdownTask(nsITargetShutdownTask* aTask) final {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) {
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

ThreadEventQueue::ThreadEventQueue(UniquePtr<EventQueue> aQueue,
                                   bool aIsMainThread)
    : mBaseQueue(std::move(aQueue)),
      mLock("ThreadEventQueue"),
      mEventsAvailable(mLock, "EventsAvail"),
      mIsMainThread(aIsMainThread) {
  if (aIsMainThread) {
    TaskController::Get()->SetConditionVariable(&mEventsAvailable);
  }
}

ThreadEventQueue::~ThreadEventQueue() { MOZ_ASSERT(mNestedQueues.IsEmpty()); }

bool ThreadEventQueue::PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                                EventQueuePriority aPriority) {
  return PutEventInternal(std::move(aEvent), aPriority, nullptr);
}

bool ThreadEventQueue::PutEventInternal(already_AddRefed<nsIRunnable>&& aEvent,
                                        EventQueuePriority aPriority,
                                        NestedSink* aSink) {
  // We want to leak the reference when we fail to dispatch it, so that
  // we won't release the event in a wrong thread.
  LeakRefPtr<nsIRunnable> event(std::move(aEvent));
  nsCOMPtr<nsIThreadObserver> obs;

  {
    // Check if the runnable wants to override the passed-in priority.
    // Do this outside the lock, so runnables implemented in JS can QI
    // (and possibly GC) outside of the lock.
    if (mIsMainThread) {
      auto* e = event.get();  // can't do_QueryInterface on LeakRefPtr.
      if (nsCOMPtr<nsIRunnablePriority> runnablePrio = do_QueryInterface(e)) {
        uint32_t prio = nsIRunnablePriority::PRIORITY_NORMAL;
        runnablePrio->GetPriority(&prio);
        if (prio == nsIRunnablePriority::PRIORITY_CONTROL) {
          aPriority = EventQueuePriority::Control;
        } else if (prio == nsIRunnablePriority::PRIORITY_RENDER_BLOCKING) {
          aPriority = EventQueuePriority::RenderBlocking;
        } else if (prio == nsIRunnablePriority::PRIORITY_VSYNC) {
          aPriority = EventQueuePriority::Vsync;
        } else if (prio == nsIRunnablePriority::PRIORITY_INPUT_HIGH) {
          aPriority = EventQueuePriority::InputHigh;
        } else if (prio == nsIRunnablePriority::PRIORITY_MEDIUMHIGH) {
          aPriority = EventQueuePriority::MediumHigh;
        } else if (prio == nsIRunnablePriority::PRIORITY_DEFERRED_TIMERS) {
          aPriority = EventQueuePriority::DeferredTimers;
        } else if (prio == nsIRunnablePriority::PRIORITY_IDLE) {
          aPriority = EventQueuePriority::Idle;
        }
      }

      if (aPriority == EventQueuePriority::Control &&
          !StaticPrefs::threads_control_event_queue_enabled()) {
        aPriority = EventQueuePriority::MediumHigh;
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

already_AddRefed<nsIRunnable> ThreadEventQueue::GetEvent(
    bool aMayWait, mozilla::TimeDuration* aLastEventDelay) {
  nsCOMPtr<nsIRunnable> event;
  {
    // Scope for lock.  When we are about to return, we will exit this
    // scope so we can do some work after releasing the lock but
    // before returning.
    MutexAutoLock lock(mLock);

    for (;;) {
      const bool noNestedQueue = mNestedQueues.IsEmpty();
      if (noNestedQueue) {
        event = mBaseQueue->GetEvent(lock, aLastEventDelay);
      } else {
        // We always get events from the topmost queue when there are nested
        // queues.
        event =
            mNestedQueues.LastElement().mQueue->GetEvent(lock, aLastEventDelay);
      }

      if (event) {
        break;
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

  return event.forget();
}

bool ThreadEventQueue::HasPendingEvent() {
  MutexAutoLock lock(mLock);

  // We always get events from the topmost queue when there are nested queues.
  if (mNestedQueues.IsEmpty()) {
    return mBaseQueue->HasReadyEvent(lock);
  } else {
    return mNestedQueues.LastElement().mQueue->HasReadyEvent(lock);
  }
}

bool ThreadEventQueue::ShutdownIfNoPendingEvents() {
  MutexAutoLock lock(mLock);
  if (mNestedQueues.IsEmpty() && mBaseQueue->IsEmpty(lock)) {
    mEventsAreDoomed = true;
    return true;
  }
  return false;
}

already_AddRefed<nsISerialEventTarget> ThreadEventQueue::PushEventQueue() {
  auto queue = MakeUnique<EventQueue>();
  RefPtr<NestedSink> sink = new NestedSink(queue.get(), this);
  RefPtr<ThreadEventTarget> eventTarget =
      new ThreadEventTarget(sink, NS_IsMainThread());

  MutexAutoLock lock(mLock);

  mNestedQueues.AppendElement(NestedQueueItem(std::move(queue), eventTarget));
  return eventTarget.forget();
}

void ThreadEventQueue::PopEventQueue(nsIEventTarget* aTarget) {
  MutexAutoLock lock(mLock);

  MOZ_ASSERT(!mNestedQueues.IsEmpty());

  NestedQueueItem& item = mNestedQueues.LastElement();

  MOZ_ASSERT(aTarget == item.mEventTarget);

  // Disconnect the event target that will be popped.
  item.mEventTarget->Disconnect(lock);

  EventQueue* prevQueue =
      mNestedQueues.Length() == 1
          ? mBaseQueue.get()
          : mNestedQueues[mNestedQueues.Length() - 2].mQueue.get();

  // Move events from the old queue to the new one.
  nsCOMPtr<nsIRunnable> event;
  TimeDuration delay;
  while ((event = item.mQueue->GetEvent(lock, &delay))) {
    // preserve the event delay so far
    prevQueue->PutEvent(event.forget(), EventQueuePriority::Normal, lock,
                        &delay);
  }

  mNestedQueues.RemoveLastElement();
}

size_t ThreadEventQueue::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  size_t n = 0;

  n += mBaseQueue->SizeOfIncludingThis(aMallocSizeOf);

  {
    MutexAutoLock lock(mLock);
    n += mNestedQueues.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (auto& queue : mNestedQueues) {
      n += queue.mEventTarget->SizeOfIncludingThis(aMallocSizeOf);
    }
  }

  return SynchronizedEventQueue::SizeOfExcludingThis(aMallocSizeOf) + n;
}

already_AddRefed<nsIThreadObserver> ThreadEventQueue::GetObserver() {
  MutexAutoLock lock(mLock);
  return do_AddRef(mObserver);
}

already_AddRefed<nsIThreadObserver> ThreadEventQueue::GetObserverOnThread()
    MOZ_NO_THREAD_SAFETY_ANALYSIS {
  // only written on this thread
  return do_AddRef(mObserver);
}

void ThreadEventQueue::SetObserver(nsIThreadObserver* aObserver) {
  // Always called from the thread - single writer.
  nsCOMPtr<nsIThreadObserver> observer = aObserver;
  {
    MutexAutoLock lock(mLock);
    mObserver.swap(observer);
  }
  if (NS_IsMainThread()) {
    TaskController::Get()->SetThreadObserver(aObserver);
  }
}

nsresult ThreadEventQueue::RegisterShutdownTask(nsITargetShutdownTask* aTask) {
  NS_ENSURE_ARG(aTask);
  MutexAutoLock lock(mLock);
  if (mEventsAreDoomed || mShutdownTasksRun) {
    return NS_ERROR_UNEXPECTED;
  }
  MOZ_ASSERT(!mShutdownTasks.Contains(aTask));
  mShutdownTasks.AppendElement(aTask);
  return NS_OK;
}

nsresult ThreadEventQueue::UnregisterShutdownTask(
    nsITargetShutdownTask* aTask) {
  NS_ENSURE_ARG(aTask);
  MutexAutoLock lock(mLock);
  if (mEventsAreDoomed || mShutdownTasksRun) {
    return NS_ERROR_UNEXPECTED;
  }
  return mShutdownTasks.RemoveElement(aTask) ? NS_OK : NS_ERROR_UNEXPECTED;
}

void ThreadEventQueue::RunShutdownTasks() {
  nsTArray<nsCOMPtr<nsITargetShutdownTask>> shutdownTasks;
  {
    MutexAutoLock lock(mLock);
    shutdownTasks = std::move(mShutdownTasks);
    mShutdownTasks.Clear();
    mShutdownTasksRun = true;
  }
  for (auto& task : shutdownTasks) {
    task->TargetShutdown();
  }
}

ThreadEventQueue::NestedQueueItem::NestedQueueItem(
    UniquePtr<EventQueue> aQueue, ThreadEventTarget* aEventTarget)
    : mQueue(std::move(aQueue)), mEventTarget(aEventTarget) {}
