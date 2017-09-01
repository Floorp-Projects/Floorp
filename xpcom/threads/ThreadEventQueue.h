/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ThreadEventQueue_h
#define mozilla_ThreadEventQueue_h

#include "mozilla/AbstractEventQueue.h"
#include "mozilla/CondVar.h"
#include "mozilla/SynchronizedEventQueue.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

class nsIEventTarget;
class nsISerialEventTarget;
class nsIThreadObserver;

namespace mozilla {

class EventQueue;

template<typename InnerQueueT>
class PrioritizedEventQueue;

class LabeledEventQueue;

class ThreadEventTarget;

// A ThreadEventQueue implements normal monitor-style synchronization over the
// InnerQueueT AbstractEventQueue. It also implements PushEventQueue and
// PopEventQueue for workers (see the documentation below for an explanation of
// those). All threads use a ThreadEventQueue as their event queue. InnerQueueT
// is a template parameter to avoid virtual dispatch overhead.
template<class InnerQueueT>
class ThreadEventQueue final : public SynchronizedEventQueue
{
public:
  explicit ThreadEventQueue(UniquePtr<InnerQueueT> aQueue);

  bool PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventPriority aPriority) final;

  already_AddRefed<nsIRunnable> GetEvent(bool aMayWait,
                                         EventPriority* aPriority) final;
  bool HasPendingEvent() final;

  bool ShutdownIfNoPendingEvents() final;

  void Disconnect(const MutexAutoLock& aProofOfLock) final {}

  void EnableInputEventPrioritization() final;
  void FlushInputEventPrioritization() final;
  void SuspendInputEventPrioritization() final;
  void ResumeInputEventPrioritization() final;

  /**
   * This method causes any events currently enqueued on the thread to be
   * suppressed until PopEventQueue is called, and any event dispatched to this
   * thread's nsIEventTarget will queue as well. Calls to PushEventQueue may be
   * nested and must each be paired with a call to PopEventQueue in order to
   * restore the original state of the thread. The returned nsIEventTarget may
   * be used to push events onto the nested queue. Dispatching will be disabled
   * once the event queue is popped. The thread will only ever process pending
   * events for the innermost event queue. Must only be called on the target
   * thread.
   */
  already_AddRefed<nsISerialEventTarget> PushEventQueue();

  /**
   * Revert a call to PushEventQueue. When an event queue is popped, any events
   * remaining in the queue are appended to the elder queue. This also causes
   * the nsIEventTarget returned from PushEventQueue to stop dispatching events.
   * Must only be called on the target thread, and with the innermost event
   * queue.
   */
  void PopEventQueue(nsIEventTarget* aTarget);

  already_AddRefed<nsIThreadObserver> GetObserver() final;
  already_AddRefed<nsIThreadObserver> GetObserverOnThread() final;
  void SetObserver(nsIThreadObserver* aObserver) final;

  Mutex& MutexRef() { return mLock; }

private:
  class NestedSink;

  virtual ~ThreadEventQueue();

  bool PutEventInternal(already_AddRefed<nsIRunnable>&& aEvent,
                        EventPriority aPriority,
                        NestedSink* aQueue);

  UniquePtr<InnerQueueT> mBaseQueue;

  struct NestedQueueItem
  {
    UniquePtr<EventQueue> mQueue;
    RefPtr<ThreadEventTarget> mEventTarget;

    NestedQueueItem(UniquePtr<EventQueue> aQueue,
                    ThreadEventTarget* aEventTarget)
      : mQueue(Move(aQueue))
      , mEventTarget(aEventTarget)
    {}
  };

  nsTArray<NestedQueueItem> mNestedQueues;

  Mutex mLock;
  CondVar mEventsAvailable;

  bool mEventsAreDoomed = false;
  nsCOMPtr<nsIThreadObserver> mObserver;
};

extern template class ThreadEventQueue<EventQueue>;
extern template class ThreadEventQueue<PrioritizedEventQueue<EventQueue>>;
extern template class ThreadEventQueue<PrioritizedEventQueue<LabeledEventQueue>>;

}; // namespace mozilla

#endif // mozilla_ThreadEventQueue_h
