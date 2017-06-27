/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=2 ts=8 et tw=80 :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChannelEventQueue.h"

#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"
#include "nsISupports.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

ChannelEvent*
ChannelEventQueue::TakeEvent()
{
  mMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mFlushing);

  if (mSuspended || mEventQueue.IsEmpty()) {
    return nullptr;
  }

  UniquePtr<ChannelEvent> event(Move(mEventQueue[0]));
  mEventQueue.RemoveElementAt(0);

  return event.release();
}

void
ChannelEventQueue::FlushQueue()
{
  // Events flushed could include destruction of channel (and our own
  // destructor) unless we make sure its refcount doesn't drop to 0 while this
  // method is running.
  nsCOMPtr<nsISupports> kungFuDeathGrip(mOwner);
  mozilla::Unused << kungFuDeathGrip; // Not used in this function

  // Prevent flushed events from flushing the queue recursively
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(!mFlushing);
    mFlushing = true;
  }

  bool needResumeOnOtherThread = false;

  while (true) {
    UniquePtr<ChannelEvent> event;
    {
      MutexAutoLock lock(mMutex);
      event.reset(TakeEvent());
      if (!event) {
        MOZ_ASSERT(mFlushing);
        mFlushing = false;
        MOZ_ASSERT(mEventQueue.IsEmpty() || (mSuspended || !!mForcedCount));
        break;
      }
    }

    nsCOMPtr<nsIEventTarget> target = event->GetEventTarget();
    MOZ_ASSERT(target);

    bool isCurrentThread = false;
    nsresult rv = target->IsOnCurrentThread(&isCurrentThread);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // Simply run this event on current thread if we are not sure about it
      // in release channel, or assert in Aurora/Nightly channel.
      MOZ_DIAGNOSTIC_ASSERT(false);
      isCurrentThread = true;
    }

    if (!isCurrentThread) {
      // Next event needs to run on another thread. Put it back to
      // the front of the queue can try resume on that thread.
      Suspend();
      PrependEvent(event);

      needResumeOnOtherThread = true;
      {
        MutexAutoLock lock(mMutex);
        MOZ_ASSERT(mFlushing);
        mFlushing = false;
        MOZ_ASSERT(!mEventQueue.IsEmpty());
      }
      break;
    }

    event->Run();
  } // end of while(true)

  // The flush procedure is aborted because next event cannot be run on current
  // thread. We need to resume the event processing right after flush procedure
  // is finished.
  // Note: we cannot call Resume() while "mFlushing == true" because
  // CompleteResume will not trigger FlushQueue while there is an ongoing flush.
  if (needResumeOnOtherThread) {
    Resume();
  }
}

void
ChannelEventQueue::Suspend()
{
  MutexAutoLock lock(mMutex);
  SuspendInternal();
}

void
ChannelEventQueue::SuspendInternal()
{
  mMutex.AssertCurrentThreadOwns();

  mSuspended = true;
  mSuspendCount++;
}

void ChannelEventQueue::Resume()
{
  MutexAutoLock lock(mMutex);
  ResumeInternal();
}

void
ChannelEventQueue::ResumeInternal()
{
  mMutex.AssertCurrentThreadOwns();

  // Resuming w/o suspend: error in debug mode, ignore in build
  MOZ_ASSERT(mSuspendCount > 0);
  if (mSuspendCount <= 0) {
    return;
  }

  if (!--mSuspendCount) {
    if (mEventQueue.IsEmpty()) {
      // Nothing in queue to flush, simply clear the flag.
      mSuspended = false;
      return;
    }

    // Hold a strong reference of mOwner to avoid the channel release
    // before CompleteResume was executed.
    class CompleteResumeRunnable : public CancelableRunnable
    {
    public:
      explicit CompleteResumeRunnable(ChannelEventQueue* aQueue, nsISupports* aOwner)
        : CancelableRunnable("CompleteResumeRunnable")
        , mQueue(aQueue)
        , mOwner(aOwner)
      {
      }

      NS_IMETHOD Run() override
      {
        mQueue->CompleteResume();
        return NS_OK;
      }

    private:
      virtual ~CompleteResumeRunnable() {}

      RefPtr<ChannelEventQueue> mQueue;
      nsCOMPtr<nsISupports> mOwner;
    };

    // Worker thread requires a CancelableRunnable.
    RefPtr<Runnable> event = new CompleteResumeRunnable(this, mOwner);

    nsCOMPtr<nsIEventTarget> target;
      target = mEventQueue[0]->GetEventTarget();
    MOZ_ASSERT(target);

    Unused << NS_WARN_IF(NS_FAILED(target->Dispatch(event.forget(),
                                                    NS_DISPATCH_NORMAL)));
  }
}

} // namespace net
} // namespace mozilla
