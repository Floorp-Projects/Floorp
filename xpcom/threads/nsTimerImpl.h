/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsTimerImpl_h___
#define nsTimerImpl_h___

//#define FORCE_PR_LOG /* Allow logging in the release build */

#include "nsITimer.h"
#include "nsVoidArray.h"
#include "nsIThread.h"
#include "nsITimerInternal.h"
#include "nsIObserver.h"

#include "nsCOMPtr.h"

#include "prlog.h"

#if defined(PR_LOGGING)
static PRLogModuleInfo *gTimerLog = PR_NewLogModule("nsTimerImpl");
#define DEBUG_TIMERS 1
#else
#undef DEBUG_TIMERS
#endif

#define NS_TIMER_CLASSNAME "Timer"
#define NS_TIMER_CID \
{ /* 5ff24248-1dd2-11b2-8427-fbab44f29bc8 */         \
     0x5ff24248,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x84, 0x27, 0xfb, 0xab, 0x44, 0xf2, 0x9b, 0xc8} \
}

enum {
  CALLBACK_TYPE_UNKNOWN   = 0,
  CALLBACK_TYPE_INTERFACE = 1,
  CALLBACK_TYPE_FUNC      = 2,
  CALLBACK_TYPE_OBSERVER  = 3
};

// Two timer deadlines must differ by less than half the PRIntervalTime domain.
#define DELAY_INTERVAL_LIMIT    PR_BIT(8 * sizeof(PRIntervalTime) - 1)

// Maximum possible delay (XXX rework to use ms rather than interval ticks).
#define DELAY_INTERVAL_MAX      (DELAY_INTERVAL_LIMIT - 1)

// Is interval-time t less than u, even if t has wrapped PRIntervalTime?
#define TIMER_LESS_THAN(t, u)   ((t) - (u) > DELAY_INTERVAL_LIMIT)

class nsTimerImpl : public nsITimer, public nsITimerInternal
{
public:

  nsTimerImpl();

  static NS_HIDDEN_(nsresult) Startup();
  static NS_HIDDEN_(void) Shutdown();

  friend class TimerThread;

  void Fire();
  void PostTimerEvent();
  void SetDelayInternal(PRUint32 aDelay);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMER
  NS_DECL_NSITIMERINTERNAL

  PRInt32 GetGeneration() { return mGeneration; }

private:
  ~nsTimerImpl();

  nsresult InitCommon(PRUint32 aType, PRUint32 aDelay);

  void ReleaseCallback()
  {
    if (mCallbackType == CALLBACK_TYPE_INTERFACE)
      NS_RELEASE(mCallback.i);
    else if (mCallbackType == CALLBACK_TYPE_OBSERVER)
      NS_RELEASE(mCallback.o);
  }

  nsCOMPtr<nsIThread>   mCallingThread;

  void *                mClosure;

  union {
    nsTimerCallbackFunc c;
    nsITimerCallback *  i;
    nsIObserver *       o;
  } mCallback;

  // These members are set by Init (called from NS_NewTimer) and never reset.
  PRUint8               mCallbackType;
  PRPackedBool          mIdle;

  // These members are set by the initiating thread, when the timer's type is
  // changed and during the period where it fires on that thread.
  PRUint8               mType;
  PRPackedBool          mFiring;


  // Use a PRBool (int) here to isolate loads and stores of these two members
  // done on various threads under the protection of TimerThread::mLock, from
  // loads and stores done on the initiating/type-changing/timer-firing thread
  // to the above PRUint8/PRPackedBool members.
  PRBool                mArmed;
  PRBool                mCanceled;

  // The generation number of this timer, re-generated each time the timer is
  // initialized so one-shot timers can be canceled and re-initialized by the
  // arming thread without any bad race conditions.
  PRInt32               mGeneration;

  PRUint32              mDelay;
  PRIntervalTime        mTimeout;

#ifdef DEBUG_TIMERS
  PRIntervalTime        mStart, mStart2;
  static double         sDeltaSum;
  static double         sDeltaSumSquared;
  static double         sDeltaNum;
#endif

};

#define NS_TIMERMANAGER_CONTRACTID "@mozilla.org/timer/manager;1"
#define NS_TIMERMANAGER_CLASSNAME "Timer Manager"
#define NS_TIMERMANAGER_CID \
{ /* 4fe206fa-1dd2-11b2-8a0a-88bacbecc7d2 */         \
     0x4fe206fa,                                     \
     0x1dd2,                                         \
     0x11b2,                                         \
    {0x8a, 0x0a, 0x88, 0xba, 0xcb, 0xec, 0xc7, 0xd2} \
}

#include "nsITimerManager.h"

class nsTimerManager : nsITimerManager
{
public:
  nsTimerManager();

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERMANAGER

  nsresult AddIdleTimer(nsITimer* timer);
private:
  ~nsTimerManager();

  PRLock *mLock;
  nsVoidArray mIdleTimers;
};


#endif /* nsTimerImpl_h___ */
