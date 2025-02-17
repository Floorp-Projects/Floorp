/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=2 ts=8 et tw=80 :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChannelEventQueue.h"

#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"
#include "nsIChannel.h"
#include "mozilla/dom/Document.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

ChannelEvent* ChannelEventQueue::TakeEvent() {
  mMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mFlushing);

  if (mSuspended || mEventQueue.IsEmpty()) {
    return nullptr;
  }

  UniquePtr<ChannelEvent> event(std::move(mEventQueue[0]));
  mEventQueue.RemoveElementAt(0);

  return event.release();
}

void ChannelEventQueue::FlushQueue() {
  // Events flushed could include destruction of channel (and our own
  // destructor) unless we make sure its refcount doesn't drop to 0 while this
  // method is running.
  nsCOMPtr<nsISupports> kungFuDeathGrip;
  {
    MutexAutoLock lock(mMutex);
    kungFuDeathGrip = mOwner;
  }
  mozilla::Unused << kungFuDeathGrip;  // Not used in this function

#ifdef DEBUG
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mFlushing);
  }
#endif  // DEBUG

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
      PrependEvent(std::move(event));

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
  }  // end of while(true)

  // The flush procedure is aborted because next event cannot be run on current
  // thread. We need to resume the event processing right after flush procedure
  // is finished.
  // Note: we cannot call Resume() while "mFlushing == true" because
  // CompleteResume will not trigger FlushQueue while there is an ongoing flush.
  if (needResumeOnOtherThread) {
    Resume();
  }
}

void ChannelEventQueue::Suspend() {
  MutexAutoLock lock(mMutex);
  SuspendInternal();
}

void ChannelEventQueue::SuspendInternal() {
  mMutex.AssertCurrentThreadOwns();

  mSuspended = true;
  mSuspendCount++;
}

void ChannelEventQueue::Resume() {
  MutexAutoLock lock(mMutex);
  ResumeInternal();
}

void ChannelEventQueue::ResumeInternal() {
  mMutex.AssertCurrentThreadOwns();

  // Resuming w/o suspend: error in debug mode, ignore in build
  MOZ_ASSERT(mSuspendCount > 0);
  if (mSuspendCount <= 0) {
    return;
  }

  if (!--mSuspendCount) {
    if (mEventQueue.IsEmpty() || !!mForcedCount) {
      // Nothing in queue to flush or waiting for AutoEventEnqueuer to
      // finish the force enqueue period, simply clear the flag.
      mSuspended = false;
      return;
    }

    // Hold a strong reference of mOwner to avoid the channel release
    // before CompleteResume was executed.
    class CompleteResumeRunnable : public Runnable {
     public:
      explicit CompleteResumeRunnable(ChannelEventQueue* aQueue,
                                      nsISupports* aOwner)
          : Runnable("CompleteResumeRunnable"),
            mQueue(aQueue),
            mOwner(aOwner) {}

      NS_IMETHOD Run() override {
        mQueue->CompleteResume();
        return NS_OK;
      }

     private:
      virtual ~CompleteResumeRunnable() = default;

      RefPtr<ChannelEventQueue> mQueue;
      nsCOMPtr<nsISupports> mOwner;
    };

    if (!mOwner) {
      return;
    }

    // Worker thread requires a CancelableRunnable.
    RefPtr<Runnable> event = new CompleteResumeRunnable(this, mOwner);

    nsCOMPtr<nsIEventTarget> target;
    target = mEventQueue[0]->GetEventTarget();
    MOZ_ASSERT(target);

    Unused << NS_WARN_IF(
        NS_FAILED(target->Dispatch(event.forget(), NS_DISPATCH_NORMAL)));
  }
}

bool ChannelEventQueue::MaybeSuspendIfEventsAreSuppressed() {
  // We only ever need to suppress events on the main thread, since this is
  // where content scripts can run.
  if (!NS_IsMainThread()) {
    return false;
  }

  // Only suppress events for queues associated with XHRs, as these can cause
  // content scripts to run.
  if (mHasCheckedForAsyncXMLHttpRequest && !mForAsyncXMLHttpRequest) {
    return false;
  }

  mMutex.AssertCurrentThreadOwns();
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(mOwner));
  if (!channel) {
    return false;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
  // Figure out if this is for an Async XHR, if we haven't done so already.
  // We don't want to suspend Sync XHRs, as they'll always suspend event
  // handling on the document, but we still need to process events for them.
  if (!mHasCheckedForAsyncXMLHttpRequest) {
    nsContentPolicyType contentType = loadInfo->InternalContentPolicyType();
    mForAsyncXMLHttpRequest =
        contentType == nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST_ASYNC;
    mHasCheckedForAsyncXMLHttpRequest = true;

    if (!mForAsyncXMLHttpRequest) {
      return false;
    }
  }

  // Suspend the queue if the associated document has suppressed event handling.
  RefPtr<dom::Document> document;
  loadInfo->GetLoadingDocument(getter_AddRefs(document));
  if (document && document->EventHandlingSuppressed()) {
    document->AddSuspendedChannelEventQueue(this);
    SuspendInternal();
    return true;
  }

  return false;
}

}  // namespace net
}  // namespace mozilla
