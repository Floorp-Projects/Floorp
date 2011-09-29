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

#include "nsEventQueue.h"
#include "nsAutoPtr.h"
#include "prlog.h"
#include "nsThreadUtils.h"

using namespace mozilla;

#ifdef PR_LOGGING
static PRLogModuleInfo *sLog = PR_NewLogModule("nsEventQueue");
#endif
#define LOG(args) PR_LOG(sLog, PR_LOG_DEBUG, args)

nsEventQueue::nsEventQueue()
  : mReentrantMonitor("nsEventQueue.mReentrantMonitor")
  , mHead(nsnull)
  , mTail(nsnull)
  , mOffsetHead(0)
  , mOffsetTail(0)
{
}

nsEventQueue::~nsEventQueue()
{
  // It'd be nice to be able to assert that no one else is holding the monitor,
  // but NSPR doesn't really expose APIs for it.
  NS_ASSERTION(IsEmpty(), "Non-empty event queue being destroyed; events being leaked.");

  if (mHead)
    FreePage(mHead);
}

bool
nsEventQueue::GetEvent(bool mayWait, nsIRunnable **result)
{
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);
    
    while (IsEmpty()) {
      if (!mayWait) {
        if (result)
          *result = nsnull;
        return PR_FALSE;
      }
      LOG(("EVENTQ(%p): wait begin\n", this)); 
      mon.Wait();
      LOG(("EVENTQ(%p): wait end\n", this)); 
    }
    
    if (result) {
      *result = mHead->mEvents[mOffsetHead++];
      
      // Check if mHead points to empty Page
      if (mOffsetHead == EVENTS_PER_PAGE) {
        Page *dead = mHead;
        mHead = mHead->mNext;
        FreePage(dead);
        mOffsetHead = 0;
      }
    }
  }

  return PR_TRUE;
}

bool
nsEventQueue::PutEvent(nsIRunnable *runnable)
{
  // Avoid calling AddRef+Release while holding our monitor.
  nsRefPtr<nsIRunnable> event(runnable);
  bool rv = true;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    if (!mHead) {
      mHead = NewPage();
      if (!mHead) {
        rv = PR_FALSE;
      } else {
        mTail = mHead;
        mOffsetHead = 0;
        mOffsetTail = 0;
      }
    } else if (mOffsetTail == EVENTS_PER_PAGE) {
      Page *page = NewPage();
      if (!page) {
        rv = PR_FALSE;
      } else {
        mTail->mNext = page;
        mTail = page;
        mOffsetTail = 0;
      }
    }
    if (rv) {
      event.swap(mTail->mEvents[mOffsetTail]);
      ++mOffsetTail;
      LOG(("EVENTQ(%p): notify\n", this)); 
      mon.NotifyAll();
    }
  }
  return rv;
}
