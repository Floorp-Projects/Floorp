/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEventQueue.h"
#include "nsAutoPtr.h"
#include "mozilla/Logging.h"
#include "nsThreadUtils.h"
#include "prthread.h"
#include "mozilla/ChaosMode.h"

using namespace mozilla;

static LazyLogModule sEventQueueLog("nsEventQueue");
#ifdef LOG
#undef LOG
#endif
#define LOG(args) MOZ_LOG(sEventQueueLog, mozilla::LogLevel::Debug, args)

nsEventQueue::nsEventQueue(Mutex& aLock)
  : mHead(nullptr)
  , mTail(nullptr)
  , mOffsetHead(0)
  , mOffsetTail(0)
  , mEventsAvailable(aLock, "[nsEventQueue.mEventsAvailable]")
{
}

nsEventQueue::~nsEventQueue()
{
  // It'd be nice to be able to assert that no one else is holding the lock,
  // but NSPR doesn't really expose APIs for it.
  NS_ASSERTION(IsEmpty(),
               "Non-empty event queue being destroyed; events being leaked.");

  if (mHead) {
    FreePage(mHead);
  }
}

bool
nsEventQueue::GetEvent(bool aMayWait, nsIRunnable** aResult,
                       MutexAutoLock& aProofOfLock)
{
  while (IsEmpty()) {
    if (!aMayWait) {
      if (aResult) {
        *aResult = nullptr;
      }
      return false;
    }
    LOG(("EVENTQ(%p): wait begin\n", this));
    mEventsAvailable.Wait();
    LOG(("EVENTQ(%p): wait end\n", this));
  }

  if (aResult) {
    MOZ_ASSERT(mOffsetHead < EVENTS_PER_PAGE);
    MOZ_ASSERT_IF(mHead == mTail, mOffsetHead <= mOffsetTail);
    *aResult = mHead->mEvents[mOffsetHead++];

    MOZ_ASSERT(*aResult);
    MOZ_ASSERT(mOffsetHead <= EVENTS_PER_PAGE);

    // Check if mHead points to empty Page
    if (mOffsetHead == EVENTS_PER_PAGE) {
      Page* dead = mHead;
      mHead = mHead->mNext;
      FreePage(dead);
      mOffsetHead = 0;
    }
  }

  return true;
}

void
nsEventQueue::PutEvent(already_AddRefed<nsIRunnable>&& aRunnable,
                       MutexAutoLock& aProofOfLock)
{
  if (!mHead) {
    mHead = NewPage();
    MOZ_ASSERT(mHead);

    mTail = mHead;
    mOffsetHead = 0;
    mOffsetTail = 0;
  } else if (mOffsetTail == EVENTS_PER_PAGE) {
    Page* page = NewPage();
    MOZ_ASSERT(page);

    mTail->mNext = page;
    mTail = page;
    mOffsetTail = 0;
  }

  nsIRunnable*& queueLocation = mTail->mEvents[mOffsetTail];
  MOZ_ASSERT(!queueLocation);
  queueLocation = aRunnable.take();
  ++mOffsetTail;
  LOG(("EVENTQ(%p): notify\n", this));
  mEventsAvailable.Notify();
}

void
nsEventQueue::PutEvent(nsIRunnable* aRunnable, MutexAutoLock& aProofOfLock)
{
  nsCOMPtr<nsIRunnable> event(aRunnable);
  PutEvent(event.forget(), aProofOfLock);
}

size_t
nsEventQueue::Count(MutexAutoLock& aProofOfLock) const
{
  // It is obvious count is 0 when the queue is empty.
  if (!mHead) {
    return 0;
  }

  /* How we count the number of events in the queue:
   * 1. Let pageCount(x, y) denote the number of pages excluding the tail page
   *    where x is the index of head page and y is the index of the tail page.
   * 2. Then we have pageCount(x, y) = y - x.
   *
   * Ex: pageCount(0, 0) = 0 where both head and tail pages point to page 0.
   *     pageCount(0, 1) = 1 where head points to page 0 and tail points page 1.
   *
   * 3. number of events = (EVENTS_PER_PAGE * pageCount(x, y))
   *      - (empty slots in head page) + (non-empty slots in tail page)
   *      = (EVENTS_PER_PAGE * pageCount(x, y)) - mOffsetHead + mOffsetTail
   */

  int count = -mOffsetHead;

  // Compute (EVENTS_PER_PAGE * pageCount(x, y))
  for (Page* page = mHead; page != mTail; page = page->mNext) {
    count += EVENTS_PER_PAGE;
  }

  count += mOffsetTail;
  MOZ_ASSERT(count >= 0);

  return count;
}
