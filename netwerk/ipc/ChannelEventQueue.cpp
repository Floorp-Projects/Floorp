/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set sw=2 ts=8 et tw=80 :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

ChannelEvent*
ChannelEventQueue::TakeEvent()
{
  MutexAutoLock lock(mMutex);
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

  // Prevent flushed events from flushing the queue recursively
  {
    MutexAutoLock lock(mMutex);
    mFlushing = true;
  }

  while (true) {
    UniquePtr<ChannelEvent> event(TakeEvent());
    if (!event) {
      break;
    }

    event->Run();
  }

  MutexAutoLock lock(mMutex);
  mFlushing = false;
}

void
ChannelEventQueue::Resume()
{
  MutexAutoLock lock(mMutex);

  // Resuming w/o suspend: error in debug mode, ignore in build
  MOZ_ASSERT(mSuspendCount > 0);
  if (mSuspendCount <= 0) {
    return;
  }

  if (!--mSuspendCount) {
    RefPtr<Runnable> event =
      NewRunnableMethod(this, &ChannelEventQueue::CompleteResume);
    if (mTargetThread) {
      mTargetThread->Dispatch(event.forget(), NS_DISPATCH_NORMAL);
    } else {
      MOZ_RELEASE_ASSERT(NS_IsMainThread());
      NS_WARN_IF(NS_FAILED(NS_DispatchToCurrentThread(event.forget())));
    }
  }
}

nsresult
ChannelEventQueue::RetargetDeliveryTo(nsIEventTarget* aTargetThread)
{
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(!mTargetThread);
  MOZ_RELEASE_ASSERT(aTargetThread);

  mTargetThread = do_QueryInterface(aTargetThread);
  MOZ_RELEASE_ASSERT(mTargetThread);

  return NS_OK;
}

} // namespace net
} // namespace mozilla
