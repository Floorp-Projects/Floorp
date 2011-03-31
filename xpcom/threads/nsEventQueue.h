/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsEventQueue_h__
#define nsEventQueue_h__

#include <stdlib.h>
#include "prmon.h"
#include "nsIRunnable.h"

// A threadsafe FIFO event queue...
class NS_COM nsEventQueue
{
public:
  nsEventQueue();
  ~nsEventQueue();

  // Returns "true" if the event queue has been completely constructed.
  PRBool IsInitialized() { return mMonitor != nsnull; }

  // This method adds a new event to the pending event queue.  The event object
  // is AddRef'd if this method succeeds.  This method returns PR_TRUE if the
  // event was stored in the event queue, and it returns PR_FALSE if it could
  // not allocate sufficient memory.
  PRBool PutEvent(nsIRunnable *event);

  // This method gets an event from the event queue.  If mayWait is true, then
  // the method will block the calling thread until an event is available.  If
  // the event is null, then the method returns immediately indicating whether
  // or not an event is pending.  When the resulting event is non-null, the
  // caller is responsible for releasing the event object.  This method does
  // not alter the reference count of the resulting event.
  PRBool GetEvent(PRBool mayWait, nsIRunnable **event);

  // This method returns true if there is a pending event.
  PRBool HasPendingEvent() {
    return GetEvent(PR_FALSE, nsnull);
  }

  // This method returns the next pending event or null.
  PRBool GetPendingEvent(nsIRunnable **runnable) {
    return GetEvent(PR_FALSE, runnable);
  }

  // This method waits for and returns the next pending event.
  PRBool WaitPendingEvent(nsIRunnable **runnable) {
    return GetEvent(PR_TRUE, runnable);
  }

  // Expose the event queue's monitor for "power users"
  PRMonitor *Monitor() {
    return mMonitor;
  }

private:

  PRBool IsEmpty() {
    return !mHead || (mHead == mTail && mOffsetHead == mOffsetTail);
  }

  enum { EVENTS_PER_PAGE = 250 };

  // Page objects are linked together to form a simple deque.

  struct Page; friend struct Page; // VC6!
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

  PRMonitor *mMonitor;

  Page *mHead;
  Page *mTail;

  PRUint16 mOffsetHead;  // offset into mHead where next item is removed
  PRUint16 mOffsetTail;  // offset into mTail where next item is added
};

#endif  // nsEventQueue_h__
