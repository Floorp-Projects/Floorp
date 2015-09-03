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

static PRLogModuleInfo*
GetLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("nsEventQueue");
  }
  return sLog;
}
#ifdef LOG
#undef LOG
#endif
#define LOG(args) MOZ_LOG(GetLog(), mozilla::LogLevel::Debug, args)

template<typename MonitorType>
nsEventQueueBase<MonitorType>::nsEventQueueBase()
  : mHead(nullptr)
  , mTail(nullptr)
  , mOffsetHead(0)
  , mOffsetTail(0)
{
}

template nsEventQueueBase<Monitor>::nsEventQueueBase();
template nsEventQueueBase<ReentrantMonitor>::nsEventQueueBase();

nsEventQueue::nsEventQueue()
  : mMonitor("[nsEventQueue.mMonitor]")
{
}

template<typename MonitorType>
nsEventQueueBase<MonitorType>::~nsEventQueueBase()
{
  // It'd be nice to be able to assert that no one else is holding the monitor,
  // but NSPR doesn't really expose APIs for it.
  NS_ASSERTION(IsEmpty(),
               "Non-empty event queue being destroyed; events being leaked.");

  if (mHead) {
    FreePage(mHead);
  }
}

template nsEventQueueBase<Monitor>::~nsEventQueueBase();
template nsEventQueueBase<ReentrantMonitor>::~nsEventQueueBase();

template<typename MonitorType>
bool
nsEventQueueBase<MonitorType>::GetEvent(bool aMayWait, nsIRunnable** aResult,
                                        MonitorAutoEnterType& aProofOfLock)
{
  while (IsEmpty()) {
    if (!aMayWait) {
      if (aResult) {
        *aResult = nullptr;
      }
      return false;
    }
    LOG(("EVENTQ(%p): wait begin\n", this));
    aProofOfLock.Wait();
    LOG(("EVENTQ(%p): wait end\n", this));
  }

  if (aResult) {
    *aResult = mHead->mEvents[mOffsetHead++];

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

template bool nsEventQueueBase<Monitor>::GetEvent(bool aMayWait, nsIRunnable** aResult,
                                                  MonitorAutoLock& aProofOfLock);
template bool nsEventQueueBase<ReentrantMonitor>::GetEvent(bool aMayWait, nsIRunnable** aResult,
                                                           ReentrantMonitorAutoEnter& aProofOfLock);

bool
nsEventQueue::GetEvent(bool aMayWait, nsIRunnable** aEvent)
{
  MonitorAutoEnterType mon(mMonitor);

  return Base::GetEvent(aMayWait, aEvent, mon);
}

template<typename MonitorType>
void
nsEventQueueBase<MonitorType>::PutEvent(
    already_AddRefed<nsIRunnable>&& aRunnable,
    MonitorAutoEnterType& aProofOfLock)
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
  aProofOfLock.Notify();
}

template void nsEventQueueBase<Monitor>::PutEvent(already_AddRefed<nsIRunnable>&& aRunnable,
                                                  MonitorAutoLock& aProofOfLock);
template void nsEventQueueBase<ReentrantMonitor>::PutEvent(already_AddRefed<nsIRunnable>&& aRunnable,
                                                           ReentrantMonitorAutoEnter& aProofOfLock);

void
nsEventQueue::PutEvent(nsIRunnable* aRunnable)
{
  nsCOMPtr<nsIRunnable> event(aRunnable);
  PutEvent(event.forget());
}

void
nsEventQueue::PutEvent(already_AddRefed<nsIRunnable>&& aRunnable)
{
  if (ChaosMode::isActive(ChaosFeature::ThreadScheduling)) {
    // With probability 0.5, yield so other threads have a chance to
    // dispatch events to this queue first.
    if (ChaosMode::randomUint32LessThan(2)) {
      PR_Sleep(PR_INTERVAL_NO_WAIT);
    }
  }

  MonitorAutoEnterType mon(mMonitor);

  Base::PutEvent(Move(aRunnable), mon);
}

template<typename MonitorType>
size_t
nsEventQueueBase<MonitorType>::Count(MonitorAutoEnterType& aProofOfLock)
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

template size_t nsEventQueueBase<Monitor>::Count(MonitorAutoLock& aProofOfLock);
template size_t nsEventQueueBase<ReentrantMonitor>::Count(ReentrantMonitorAutoEnter& aProofOfLock);

size_t
nsEventQueue::Count()
{
  MonitorAutoEnterType mon(mMonitor);

  return Base::Count(mon);
}
