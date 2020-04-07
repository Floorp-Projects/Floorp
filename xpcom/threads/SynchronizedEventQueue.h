/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SynchronizedEventQueue_h
#define mozilla_SynchronizedEventQueue_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/AbstractEventQueue.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "nsIThreadInternal.h"
#include "nsTObserverArray.h"

class nsIEventTarget;
class nsISerialEventTarget;
class nsIThreadObserver;

namespace mozilla {

// A SynchronizedEventQueue is an abstract class for event queues that can be
// used across threads. A SynchronizedEventQueue implementation will typically
// use locks and condition variables to guarantee consistency. The methods of
// SynchronizedEventQueue are split between ThreadTargetSink (which contains
// methods for posting events) and SynchronizedEventQueue (which contains
// methods for getting events). This split allows event targets (specifically
// ThreadEventTarget) to use a narrow interface, since they only need to post
// events.
//
// ThreadEventQueue is the canonical implementation of
// SynchronizedEventQueue. When Quantum DOM is implemented, we will use a
// different synchronized queue on the main thread, SchedulerEventQueue, which
// will handle the cooperative threading model.

class ThreadTargetSink {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ThreadTargetSink)

  virtual bool PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                        EventQueuePriority aPriority) = 0;

  // After this method is called, no more events can be posted.
  virtual void Disconnect(const MutexAutoLock& aProofOfLock) = 0;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  virtual size_t SizeOfExcludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const = 0;

 protected:
  virtual ~ThreadTargetSink() = default;
};

class SynchronizedEventQueue : public ThreadTargetSink {
 public:
  virtual already_AddRefed<nsIRunnable> GetEvent(
      bool aMayWait, EventQueuePriority* aPriority,
      mozilla::TimeDuration* aLastEventDelay = nullptr) = 0;
  virtual void DidRunEvent() = 0;
  virtual bool HasPendingEvent() = 0;

  virtual bool HasPendingHighPriorityEvents() = 0;

  // This method atomically checks if there are pending events and, if there are
  // none, forbids future events from being posted. It returns true if there
  // were no pending events.
  virtual bool ShutdownIfNoPendingEvents() = 0;

  // These methods provide access to an nsIThreadObserver, whose methods are
  // called when posting and processing events. SetObserver should only be
  // called on the thread that processes events. GetObserver can be called from
  // any thread. GetObserverOnThread must be used from the thread that processes
  // events; it does not acquire a lock.
  virtual already_AddRefed<nsIThreadObserver> GetObserver() = 0;
  virtual already_AddRefed<nsIThreadObserver> GetObserverOnThread() = 0;
  virtual void SetObserver(nsIThreadObserver* aObserver) = 0;

  void AddObserver(nsIThreadObserver* aObserver);
  void RemoveObserver(nsIThreadObserver* aObserver);
  const nsTObserverArray<nsCOMPtr<nsIThreadObserver>>& EventObservers();

  virtual void EnableInputEventPrioritization() = 0;
  virtual void FlushInputEventPrioritization() = 0;
  virtual void SuspendInputEventPrioritization() = 0;
  virtual void ResumeInputEventPrioritization() = 0;

  size_t SizeOfExcludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) const override {
    return mEventObservers.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

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
  virtual already_AddRefed<nsISerialEventTarget> PushEventQueue() = 0;

  /**
   * Revert a call to PushEventQueue. When an event queue is popped, any events
   * remaining in the queue are appended to the elder queue. This also causes
   * the nsIEventTarget returned from PushEventQueue to stop dispatching events.
   * Must only be called on the target thread, and with the innermost event
   * queue.
   */
  virtual void PopEventQueue(nsIEventTarget* aTarget) = 0;

 protected:
  virtual ~SynchronizedEventQueue() = default;

 private:
  nsTObserverArray<nsCOMPtr<nsIThreadObserver>> mEventObservers;
};

}  // namespace mozilla

#endif  // mozilla_SynchronizedEventQueue_h
