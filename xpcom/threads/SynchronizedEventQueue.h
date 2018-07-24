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
#include "nsTObserverArray.h"

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

class ThreadTargetSink
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ThreadTargetSink)

  virtual bool PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                        EventPriority aPriority) = 0;

  // After this method is called, no more events can be posted.
  virtual void Disconnect(const MutexAutoLock& aProofOfLock) = 0;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const = 0;

protected:
  virtual ~ThreadTargetSink() {}
};

class SynchronizedEventQueue : public ThreadTargetSink
{
public:
  virtual already_AddRefed<nsIRunnable> GetEvent(bool aMayWait,
                                                 EventPriority* aPriority) = 0;
  virtual bool HasPendingEvent() = 0;

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

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const override
  {
    return mEventObservers.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

protected:
  virtual ~SynchronizedEventQueue() {}

private:
  nsTObserverArray<nsCOMPtr<nsIThreadObserver>> mEventObservers;
};

} // namespace mozilla

#endif // mozilla_SynchronizedEventQueue_h
