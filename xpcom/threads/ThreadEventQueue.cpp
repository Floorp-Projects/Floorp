/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ThreadEventQueue.h"
#include "mozilla/EventQueue.h"
#include "LabeledEventQueue.h"

#include "LeakRefPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIThreadInternal.h"
#include "nsThreadUtils.h"
#include "PrioritizedEventQueue.h"
#include "ThreadEventTarget.h"

using namespace mozilla;

template<class InnerQueueT>
class ThreadEventQueue<InnerQueueT>::NestedSink : public ThreadTargetSink
{
public:
  NestedSink(EventQueue* aQueue, ThreadEventQueue* aOwner)
    : mQueue(aQueue)
    , mOwner(aOwner)
  {
  }

  bool PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventPriority aPriority) final
  {
    return mOwner->PutEventInternal(std::move(aEvent), aPriority, this);
  }

  void Disconnect(const MutexAutoLock& aProofOfLock) final
  {
    mQueue = nullptr;
  }

private:
  friend class ThreadEventQueue;

  // This is a non-owning reference. It must live at least until Disconnect is
  // called to clear it out.
  EventQueue* mQueue;
  RefPtr<ThreadEventQueue> mOwner;
};

template<class InnerQueueT>
ThreadEventQueue<InnerQueueT>::ThreadEventQueue(UniquePtr<InnerQueueT> aQueue)
  : mBaseQueue(std::move(aQueue))
  , mLock("ThreadEventQueue")
  , mEventsAvailable(mLock, "EventsAvail")
{
  static_assert(IsBaseOf<AbstractEventQueue, InnerQueueT>::value,
                "InnerQueueT must be an AbstractEventQueue subclass");
}

template<class InnerQueueT>
ThreadEventQueue<InnerQueueT>::~ThreadEventQueue()
{
  MOZ_ASSERT(mNestedQueues.IsEmpty());
}

template<class InnerQueueT>
bool
ThreadEventQueue<InnerQueueT>::PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                                        EventPriority aPriority)
{
  return PutEventInternal(std::move(aEvent), aPriority, nullptr);
}

template<class InnerQueueT>
bool
ThreadEventQueue<InnerQueueT>::PutEventInternal(already_AddRefed<nsIRunnable>&& aEvent,
                                                EventPriority aPriority,
                                                NestedSink* aSink)
{
  // We want to leak the reference when we fail to dispatch it, so that
  // we won't release the event in a wrong thread.
  LeakRefPtr<nsIRunnable> event(std::move(aEvent));
  nsCOMPtr<nsIThreadObserver> obs;

  {
    // Check if the runnable wants to override the passed-in priority.
    // Do this outside the lock, so runnables implemented in JS can QI
    // (and possibly GC) outside of the lock.
    if (InnerQueueT::SupportsPrioritization) {
      auto* e = event.get();    // can't do_QueryInterface on LeakRefPtr.
      if (nsCOMPtr<nsIRunnablePriority> runnablePrio = do_QueryInterface(e)) {
        uint32_t prio = nsIRunnablePriority::PRIORITY_NORMAL;
        runnablePrio->GetPriority(&prio);
        if (prio == nsIRunnablePriority::PRIORITY_HIGH) {
          aPriority = EventPriority::High;
        } else if (prio == nsIRunnablePriority::PRIORITY_INPUT) {
          aPriority = EventPriority::Input;
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

template<class InnerQueueT>
already_AddRefed<nsIRunnable>
ThreadEventQueue<InnerQueueT>::GetEvent(bool aMayWait,
                                        EventPriority* aPriority)
{
  MutexAutoLock lock(mLock);

  nsCOMPtr<nsIRunnable> event;
  for (;;) {
    if (mNestedQueues.IsEmpty()) {
      event = mBaseQueue->GetEvent(aPriority, lock);
    } else {
      // We always get events from the topmost queue when there are nested
      // queues.
      event = mNestedQueues.LastElement().mQueue->GetEvent(aPriority, lock);
    }

    if (event || !aMayWait) {
      break;
    }

    mEventsAvailable.Wait();
  }

  return event.forget();
}

template<class InnerQueueT>
bool
ThreadEventQueue<InnerQueueT>::HasPendingEvent()
{
  MutexAutoLock lock(mLock);

  // We always get events from the topmost queue when there are nested queues.
  if (mNestedQueues.IsEmpty()) {
    return mBaseQueue->HasReadyEvent(lock);
  } else {
    return mNestedQueues.LastElement().mQueue->HasReadyEvent(lock);
  }
}

template<class InnerQueueT>
bool
ThreadEventQueue<InnerQueueT>::ShutdownIfNoPendingEvents()
{
  MutexAutoLock lock(mLock);
  if (mNestedQueues.IsEmpty() && mBaseQueue->IsEmpty(lock)) {
    mEventsAreDoomed = true;
    return true;
  }
  return false;
}

template<class InnerQueueT>
void
ThreadEventQueue<InnerQueueT>::EnableInputEventPrioritization()
{
  MutexAutoLock lock(mLock);
  mBaseQueue->EnableInputEventPrioritization(lock);
}

template<class InnerQueueT>
void
ThreadEventQueue<InnerQueueT>::FlushInputEventPrioritization()
{
  MutexAutoLock lock(mLock);
  mBaseQueue->FlushInputEventPrioritization(lock);
}

template<class InnerQueueT>
void
ThreadEventQueue<InnerQueueT>::SuspendInputEventPrioritization()
{
  MutexAutoLock lock(mLock);
  mBaseQueue->SuspendInputEventPrioritization(lock);
}

template<class InnerQueueT>
void
ThreadEventQueue<InnerQueueT>::ResumeInputEventPrioritization()
{
  MutexAutoLock lock(mLock);
  mBaseQueue->ResumeInputEventPrioritization(lock);
}

template<class InnerQueueT>
already_AddRefed<nsISerialEventTarget>
ThreadEventQueue<InnerQueueT>::PushEventQueue()
{
  auto queue = MakeUnique<EventQueue>();
  RefPtr<NestedSink> sink = new NestedSink(queue.get(), this);
  RefPtr<ThreadEventTarget> eventTarget = new ThreadEventTarget(sink, NS_IsMainThread());

  MutexAutoLock lock(mLock);

  mNestedQueues.AppendElement(NestedQueueItem(std::move(queue), eventTarget));
  return eventTarget.forget();
}

template<class InnerQueueT>
void
ThreadEventQueue<InnerQueueT>::PopEventQueue(nsIEventTarget* aTarget)
{
  MutexAutoLock lock(mLock);

  MOZ_ASSERT(!mNestedQueues.IsEmpty());

  NestedQueueItem& item = mNestedQueues.LastElement();

  MOZ_ASSERT(aTarget == item.mEventTarget);

  // Disconnect the event target that will be popped.
  item.mEventTarget->Disconnect(lock);

  AbstractEventQueue* prevQueue =
    mNestedQueues.Length() == 1
    ? static_cast<AbstractEventQueue*>(mBaseQueue.get())
    : static_cast<AbstractEventQueue*>(mNestedQueues[mNestedQueues.Length() - 2].mQueue.get());

  // Move events from the old queue to the new one.
  nsCOMPtr<nsIRunnable> event;
  EventPriority prio;
  while ((event = item.mQueue->GetEvent(&prio, lock))) {
    prevQueue->PutEvent(event.forget(), prio, lock);
  }

  mNestedQueues.RemoveLastElement();
}

template<class InnerQueueT>
already_AddRefed<nsIThreadObserver>
ThreadEventQueue<InnerQueueT>::GetObserver()
{
  MutexAutoLock lock(mLock);
  return do_AddRef(mObserver);
}

template<class InnerQueueT>
already_AddRefed<nsIThreadObserver>
ThreadEventQueue<InnerQueueT>::GetObserverOnThread()
{
  return do_AddRef(mObserver);
}

template<class InnerQueueT>
void
ThreadEventQueue<InnerQueueT>::SetObserver(nsIThreadObserver* aObserver)
{
  MutexAutoLock lock(mLock);
  mObserver = aObserver;
}

namespace mozilla {
template class ThreadEventQueue<EventQueue>;
template class ThreadEventQueue<PrioritizedEventQueue<EventQueue>>;
template class ThreadEventQueue<PrioritizedEventQueue<LabeledEventQueue>>;
}
