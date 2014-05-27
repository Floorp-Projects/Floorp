/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEventQueue.h"
#include "nsAutoPtr.h"
#include "prlog.h"
#include "nsThreadUtils.h"
#include "prthread.h"
#include "mozilla/ChaosMode.h"

using namespace mozilla;

#ifdef PR_LOGGING
static PRLogModuleInfo*
GetLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("nsEventQueue");
  }
  return sLog;
}
#endif
#ifdef LOG
#undef LOG
#endif
#define LOG(args) PR_LOG(GetLog(), PR_LOG_DEBUG, args)

nsEventQueue::nsEventQueue()
  : mReentrantMonitor("nsEventQueue.mReentrantMonitor")
  , mHead(nullptr)
  , mTail(nullptr)
  , mOffsetHead(0)
  , mOffsetTail(0)
{
}

nsEventQueue::~nsEventQueue()
{
  // It'd be nice to be able to assert that no one else is holding the monitor,
  // but NSPR doesn't really expose APIs for it.
  NS_ASSERTION(IsEmpty(),
               "Non-empty event queue being destroyed; events being leaked.");

  if (mHead) {
    FreePage(mHead);
  }
}

bool
nsEventQueue::GetEvent(bool aMayWait, nsIRunnable** aResult)
{
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    while (IsEmpty()) {
      if (!aMayWait) {
        if (aResult) {
          *aResult = nullptr;
        }
        return false;
      }
      LOG(("EVENTQ(%p): wait begin\n", this));
      mon.Wait();
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
  }

  return true;
}

void
nsEventQueue::PutEvent(nsIRunnable *runnable)
{
  // Avoid calling AddRef+Release while holding our monitor.
  nsRefPtr<nsIRunnable> event(runnable);

  if (ChaosMode::isActive()) {
    // With probability 0.5, yield so other threads have a chance to
    // dispatch events to this queue first.
    if (ChaosMode::randomUint32LessThan(2)) {
      PR_Sleep(PR_INTERVAL_NO_WAIT);
    }
  }

  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (!mHead) {
    mHead = NewPage();
    MOZ_ASSERT(mHead);

    mTail = mHead;
    mOffsetHead = 0;
    mOffsetTail = 0;
  } else if (mOffsetTail == EVENTS_PER_PAGE) {
    Page *page = NewPage();
    MOZ_ASSERT(page);

    mTail->mNext = page;
    mTail = page;
    mOffsetTail = 0;
  }

  event.swap(mTail->mEvents[mOffsetTail]);
  ++mOffsetTail;
  LOG(("EVENTQ(%p): notify\n", this)); 
  mon.NotifyAll();
}
