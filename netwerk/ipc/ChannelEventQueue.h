/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=2 ts=8 et tw=80 :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_ChannelEventQueue_h
#define mozilla_net_ChannelEventQueue_h

#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsIEventTarget.h"
#include "nsThreadUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Mutex.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

class nsISupports;

namespace mozilla {
namespace net {

class ChannelEvent
{
 public:
  ChannelEvent() { MOZ_COUNT_CTOR(ChannelEvent); }
  virtual ~ChannelEvent() { MOZ_COUNT_DTOR(ChannelEvent); }
  virtual void Run() = 0;
  virtual already_AddRefed<nsIEventTarget> GetEventTarget() = 0;
};

class MainThreadChannelEvent : public ChannelEvent
{
 public:
  MainThreadChannelEvent() { MOZ_COUNT_CTOR(MainThreadChannelEvent); }
  virtual ~MainThreadChannelEvent() { MOZ_COUNT_DTOR(MainThreadChannelEvent); }

  already_AddRefed<nsIEventTarget>
  GetEventTarget() override
  {
    return do_GetMainThread();
  }
};

// Workaround for Necko re-entrancy dangers. We buffer IPDL messages in a
// queue if still dispatching previous one(s) to listeners/observers.
// Otherwise synchronous XMLHttpRequests and/or other code that spins the
// event loop (ex: IPDL rpc) could cause listener->OnDataAvailable (for
// instance) to be dispatched and called before mListener->OnStartRequest has
// completed.

class ChannelEventQueue final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChannelEventQueue)

 public:
  explicit ChannelEventQueue(nsISupports *owner)
    : mSuspendCount(0)
    , mSuspended(false)
    , mForcedCount(0)
    , mFlushing(false)
    , mOwner(owner)
    , mMutex("ChannelEventQueue::mMutex")
    , mRunningMonitor("ChannelEventQueue::mRunningMonitor")
  {}

  // Puts IPDL-generated channel event into queue, to be run later
  // automatically when EndForcedQueueing and/or Resume is called.
  //
  // @param aCallback - the ChannelEvent
  // @param aAssertionWhenNotQueued - this optional param will be used in an
  //   assertion when the event is executed directly.
  inline void RunOrEnqueue(ChannelEvent* aCallback,
                           bool aAssertionWhenNotQueued = false);

  // Append ChannelEvent in front of the event queue.
  inline nsresult PrependEvent(UniquePtr<ChannelEvent>& aEvent);
  inline nsresult PrependEvents(nsTArray<UniquePtr<ChannelEvent>>& aEvents);

  // After StartForcedQueueing is called, RunOrEnqueue() will start enqueuing
  // events that will be run/flushed when EndForcedQueueing is called.
  // - Note: queueing may still be required after EndForcedQueueing() (if the
  //   queue is suspended, etc):  always call RunOrEnqueue() to avoid race
  //   conditions.
  inline void StartForcedQueueing();
  inline void EndForcedQueueing();

  // Suspend/resume event queue.  RunOrEnqueue() will start enqueuing
  // events and they will be run/flushed when resume is called.  These should be
  // called when the channel owning the event queue is suspended/resumed.
  void Suspend();
  // Resume flushes the queue asynchronously, i.e. items in queue will be
  // dispatched in a new event on the current thread.
  void Resume();

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~ChannelEventQueue()
  {
  }

  void SuspendInternal();
  void ResumeInternal();

  inline void MaybeFlushQueue();
  void FlushQueue();
  inline void CompleteResume();

  ChannelEvent* TakeEvent();

  nsTArray<UniquePtr<ChannelEvent>> mEventQueue;

  uint32_t mSuspendCount;
  bool mSuspended;
  uint32_t mForcedCount; // Support ForcedQueueing on multiple thread.
  bool mFlushing;

  // Keep ptr to avoid refcount cycle: only grab ref during flushing.
  nsISupports *mOwner;

  // For atomic mEventQueue operation and state update
  Mutex mMutex;

  // To guarantee event execution order among threads
  ReentrantMonitor mRunningMonitor;

  friend class AutoEventEnqueuer;
};

inline void
ChannelEventQueue::RunOrEnqueue(ChannelEvent* aCallback,
                                bool aAssertionWhenNotQueued)
{
  MOZ_ASSERT(aCallback);

  // Events execution could be a destruction of the channel (and our own
  // destructor) unless we make sure its refcount doesn't drop to 0 while this
  // method is running.
  nsCOMPtr<nsISupports> kungFuDeathGrip(mOwner);
  Unused << kungFuDeathGrip; // Not used in this function

  // To avoid leaks.
  UniquePtr<ChannelEvent> event(aCallback);

  // To guarantee that the running event and all the events generated within
  // it will be finished before events on other threads.
  ReentrantMonitorAutoEnter monitor(mRunningMonitor);

  {
    MutexAutoLock lock(mMutex);

    bool enqueue =  !!mForcedCount || mSuspended || mFlushing || !mEventQueue.IsEmpty();

    if (enqueue) {
      mEventQueue.AppendElement(Move(event));
      return;
    }

    nsCOMPtr<nsIEventTarget> target = event->GetEventTarget();
    MOZ_ASSERT(target);

    bool isCurrentThread = false;
    DebugOnly<nsresult> rv = target->IsOnCurrentThread(&isCurrentThread);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (!isCurrentThread) {
      // Leverage Suspend/Resume mechanism to trigger flush procedure without
      // creating a new one.
      SuspendInternal();
      mEventQueue.AppendElement(Move(event));
      ResumeInternal();
      return;
    }
  }

  MOZ_RELEASE_ASSERT(!aAssertionWhenNotQueued);
  event->Run();
}

inline void
ChannelEventQueue::StartForcedQueueing()
{
  MutexAutoLock lock(mMutex);
  ++mForcedCount;
}

inline void
ChannelEventQueue::EndForcedQueueing()
{
  bool tryFlush = false;
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mForcedCount > 0);
    if(!--mForcedCount) {
      tryFlush = true;
    }
  }

  if (tryFlush) {
    MaybeFlushQueue();
  }
}

inline nsresult
ChannelEventQueue::PrependEvent(UniquePtr<ChannelEvent>& aEvent)
{
  MutexAutoLock lock(mMutex);

  // Prepending event while no queue flush foreseen might cause the following
  // channel events not run. This assertion here guarantee there must be a
  // queue flush, either triggered by Resume or EndForcedQueueing, to execute
  // the added event.
  MOZ_ASSERT(mSuspended || !!mForcedCount);

  UniquePtr<ChannelEvent>* newEvent =
    mEventQueue.InsertElementAt(0, Move(aEvent));

  if (!newEvent) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

inline nsresult
ChannelEventQueue::PrependEvents(nsTArray<UniquePtr<ChannelEvent>>& aEvents)
{
  MutexAutoLock lock(mMutex);

  // Prepending event while no queue flush foreseen might cause the following
  // channel events not run. This assertion here guarantee there must be a
  // queue flush, either triggered by Resume or EndForcedQueueing, to execute
  // the added events.
  MOZ_ASSERT(mSuspended || !!mForcedCount);

  UniquePtr<ChannelEvent>* newEvents =
    mEventQueue.InsertElementsAt(0, aEvents.Length());
  if (!newEvents) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < aEvents.Length(); i++) {
    newEvents[i] = Move(aEvents[i]);
  }

  return NS_OK;
}

inline void
ChannelEventQueue::CompleteResume()
{
  bool tryFlush = false;
  {
    MutexAutoLock lock(mMutex);

    // channel may have been suspended again since Resume fired event to call
    // this.
    if (!mSuspendCount) {
      // we need to remain logically suspended (for purposes of queuing incoming
      // messages) until this point, else new incoming messages could run before
      // queued ones.
      mSuspended = false;
      tryFlush = true;
    }
  }

  if (tryFlush) {
    MaybeFlushQueue();
  }
}

inline void
ChannelEventQueue::MaybeFlushQueue()
{
  // Don't flush if forced queuing on, we're already being flushed, or
  // suspended, or there's nothing to flush
  bool flushQueue = false;

  {
    MutexAutoLock lock(mMutex);
    flushQueue = !mForcedCount && !mFlushing && !mSuspended &&
                 !mEventQueue.IsEmpty();
  }

  if (flushQueue) {
    FlushQueue();
  }
}

// Ensures that RunOrEnqueue() will be collecting events during its lifetime
// (letting caller know incoming IPDL msgs should be queued). Flushes the queue
// when it goes out of scope.
class MOZ_STACK_CLASS AutoEventEnqueuer
{
 public:
  explicit AutoEventEnqueuer(ChannelEventQueue *queue) : mEventQueue(queue) {
    mEventQueue->StartForcedQueueing();
  }
  ~AutoEventEnqueuer() {
    mEventQueue->EndForcedQueueing();
  }
 private:
  RefPtr<ChannelEventQueue> mEventQueue;
};

} // namespace net
} // namespace mozilla

#endif
