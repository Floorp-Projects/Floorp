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
#include "mozilla/Mutex.h"
#include "mozilla/UniquePtr.h"

class nsISupports;
class nsIEventTarget;

namespace mozilla {
namespace net {

class ChannelEvent
{
 public:
  ChannelEvent() { MOZ_COUNT_CTOR(ChannelEvent); }
  virtual ~ChannelEvent() { MOZ_COUNT_DTOR(ChannelEvent); }
  virtual void Run() = 0;
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
    , mForced(false)
    , mFlushing(false)
    , mOwner(owner)
    , mMutex("ChannelEventQueue::mMutex")
  {}

  // Puts IPDL-generated channel event into queue, to be run later
  // automatically when EndForcedQueueing and/or Resume is called.
  //
  // @param aCallback - the ChannelEvent
  // @param aAssertionWhenNotQueued - this optional param will be used in an
  //   assertion when the event is executed directly.
  inline void RunOrEnqueue(ChannelEvent* aCallback,
                           bool aAssertionWhenNotQueued = false);
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
  inline void Suspend();
  // Resume flushes the queue asynchronously, i.e. items in queue will be
  // dispatched in a new event on the current thread.
  void Resume();

  // Retargets delivery of events to the target thread specified.
  nsresult RetargetDeliveryTo(nsIEventTarget* aTargetThread);

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~ChannelEventQueue()
  {
  }

  inline void MaybeFlushQueue();
  void FlushQueue();
  inline void CompleteResume();

  ChannelEvent* TakeEvent();

  nsTArray<UniquePtr<ChannelEvent>> mEventQueue;

  uint32_t mSuspendCount;
  bool     mSuspended;
  bool mForced;
  bool mFlushing;

  // Keep ptr to avoid refcount cycle: only grab ref during flushing.
  nsISupports *mOwner;

  Mutex mMutex;

  // EventTarget for delivery of events to the correct thread.
  nsCOMPtr<nsIEventTarget> mTargetThread;

  friend class AutoEventEnqueuer;
};

inline void
ChannelEventQueue::RunOrEnqueue(ChannelEvent* aCallback,
                                bool aAssertionWhenNotQueued)
{
  MOZ_ASSERT(aCallback);

  // To avoid leaks.
  UniquePtr<ChannelEvent> event(aCallback);

  {
    MutexAutoLock lock(mMutex);

    bool enqueue =  mForced || mSuspended || mFlushing;
    MOZ_ASSERT(enqueue == true || mEventQueue.IsEmpty(),
               "Should always enqueue if ChannelEventQueue not empty");

    if (enqueue) {
      mEventQueue.AppendElement(Move(event));
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
  mForced = true;
}

inline void
ChannelEventQueue::EndForcedQueueing()
{
  {
    MutexAutoLock lock(mMutex);
    mForced = false;
  }

  MaybeFlushQueue();
}

inline nsresult
ChannelEventQueue::PrependEvents(nsTArray<UniquePtr<ChannelEvent>>& aEvents)
{
  MutexAutoLock lock(mMutex);

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
ChannelEventQueue::Suspend()
{
  MutexAutoLock lock(mMutex);

  mSuspended = true;
  mSuspendCount++;
}

inline void
ChannelEventQueue::CompleteResume()
{
  {
    MutexAutoLock lock(mMutex);

    // channel may have been suspended again since Resume fired event to call
    // this.
    if (!mSuspendCount) {
      // we need to remain logically suspended (for purposes of queuing incoming
      // messages) until this point, else new incoming messages could run before
      // queued ones.
      mSuspended = false;
    }
  }

  MaybeFlushQueue();
}

inline void
ChannelEventQueue::MaybeFlushQueue()
{
  // Don't flush if forced queuing on, we're already being flushed, or
  // suspended, or there's nothing to flush
  bool flushQueue = false;

  {
    MutexAutoLock lock(mMutex);
    flushQueue = !mForced && !mFlushing && !mSuspended &&
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
