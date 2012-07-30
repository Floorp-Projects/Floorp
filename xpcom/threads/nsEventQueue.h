/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEventQueue_h__
#define nsEventQueue_h__

#include <stdlib.h>
#include "mozilla/ReentrantMonitor.h"
#include "nsIRunnable.h"

// A threadsafe FIFO event queue...
class nsEventQueue
{
  typedef mozilla::ReentrantMonitor ReentrantMonitor;

public:
  nsEventQueue();
  ~nsEventQueue();

  // This method adds a new event to the pending event queue.  The event object
  // is AddRef'd if this method succeeds.  This method returns true if the
  // event was stored in the event queue, and it returns false if it could
  // not allocate sufficient memory.
  bool PutEvent(nsIRunnable *event);

  // This method gets an event from the event queue.  If mayWait is true, then
  // the method will block the calling thread until an event is available.  If
  // the event is null, then the method returns immediately indicating whether
  // or not an event is pending.  When the resulting event is non-null, the
  // caller is responsible for releasing the event object.  This method does
  // not alter the reference count of the resulting event.
  bool GetEvent(bool mayWait, nsIRunnable **event);

  // This method returns true if there is a pending event.
  bool HasPendingEvent() {
    return GetEvent(false, nullptr);
  }

  // This method returns the next pending event or null.
  bool GetPendingEvent(nsIRunnable **runnable) {
    return GetEvent(false, runnable);
  }

  // This method waits for and returns the next pending event.
  bool WaitPendingEvent(nsIRunnable **runnable) {
    return GetEvent(true, runnable);
  }

  // Expose the event queue's monitor for "power users"
  ReentrantMonitor& GetReentrantMonitor() {
    return mReentrantMonitor;
  }

private:

  bool IsEmpty() {
    return !mHead || (mHead == mTail && mOffsetHead == mOffsetTail);
  }

  enum { EVENTS_PER_PAGE = 250 };

  // Page objects are linked together to form a simple deque.

  struct Page {
    struct Page *mNext;
    nsIRunnable *mEvents[EVENTS_PER_PAGE];
  };

  static Page *NewPage() {
    return static_cast<Page *>(calloc(1, sizeof(Page)));
  }

  static void FreePage(Page *p) {
    free(p);
  }

  ReentrantMonitor mReentrantMonitor;

  Page *mHead;
  Page *mTail;

  PRUint16 mOffsetHead;  // offset into mHead where next item is removed
  PRUint16 mOffsetTail;  // offset into mTail where next item is added
};

#endif  // nsEventQueue_h__
