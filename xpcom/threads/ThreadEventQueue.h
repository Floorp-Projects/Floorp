/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ThreadEventQueue_h
#define mozilla_ThreadEventQueue_h

#include "mozilla/EventQueue.h"
#include "mozilla/CondVar.h"
#include "mozilla/SynchronizedEventQueue.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

class nsIEventTarget;
class nsISerialEventTarget;
class nsIThreadObserver;

namespace mozilla {

class EventQueue;
class ThreadEventTarget;

// A ThreadEventQueue implements normal monitor-style synchronization over the
// EventQueue. It also implements PushEventQueue and PopEventQueue for workers
// (see the documentation below for an explanation of those). All threads use a
// ThreadEventQueue as their event queue. Although for the main thread this
// simply forwards events to the TaskController.
class ThreadEventQueue final : public SynchronizedEventQueue {
 public:
  explicit ThreadEventQueue(UniquePtr<EventQueue> aQueue,
                            bool aIsMainThread = false);

  bool PutEvent(already_AddRefed<nsIRunnable>&& aEvent,
                EventQueuePriority aPriority) final;

  already_AddRefed<nsIRunnable> GetEvent(
      bool aMayWait, mozilla::TimeDuration* aLastEventDelay = nullptr) final;
  bool HasPendingEvent() final;

  bool ShutdownIfNoPendingEvents() final;

  void Disconnect(const MutexAutoLock& aProofOfLock) final {}

  nsresult RegisterShutdownTask(nsITargetShutdownTask* aTask) final;
  nsresult UnregisterShutdownTask(nsITargetShutdownTask* aTask) final;
  void RunShutdownTasks() final;

  already_AddRefed<nsISerialEventTarget> PushEventQueue() final;
  void PopEventQueue(nsIEventTarget* aTarget) final;

  already_AddRefed<nsIThreadObserver> GetObserver() final;
  already_AddRefed<nsIThreadObserver> GetObserverOnThread() final;
  void SetObserver(nsIThreadObserver* aObserver) final;

  Mutex& MutexRef() { return mLock; }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) override;

 private:
  class NestedSink;

  virtual ~ThreadEventQueue();

  bool PutEventInternal(already_AddRefed<nsIRunnable>&& aEvent,
                        EventQueuePriority aPriority, NestedSink* aQueue);

  const UniquePtr<EventQueue> mBaseQueue;

  struct NestedQueueItem {
    UniquePtr<EventQueue> mQueue;
    RefPtr<ThreadEventTarget> mEventTarget;

    NestedQueueItem(UniquePtr<EventQueue> aQueue,
                    ThreadEventTarget* aEventTarget);
  };

  nsTArray<NestedQueueItem> mNestedQueues MOZ_GUARDED_BY(mLock);

  Mutex mLock;
  CondVar mEventsAvailable MOZ_GUARDED_BY(mLock);

  bool mEventsAreDoomed MOZ_GUARDED_BY(mLock) = false;
  nsCOMPtr<nsIThreadObserver> mObserver MOZ_GUARDED_BY(mLock);
  nsTArray<nsCOMPtr<nsITargetShutdownTask>> mShutdownTasks
      MOZ_GUARDED_BY(mLock);
  bool mShutdownTasksRun MOZ_GUARDED_BY(mLock) = false;

  const bool mIsMainThread;
};

}  // namespace mozilla

#endif  // mozilla_ThreadEventQueue_h
