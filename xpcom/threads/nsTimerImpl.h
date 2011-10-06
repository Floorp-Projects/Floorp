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
#include "nsIEventTarget.h"
#include "nsIObserver.h"

#include "nsCOMPtr.h"

#include "prlog.h"
#include "mozilla/TimeStamp.h"

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

class nsTimerImpl : public nsITimer
{
public:
  typedef mozilla::TimeStamp TimeStamp;

  nsTimerImpl();

  static NS_HIDDEN_(nsresult) Startup();
  static NS_HIDDEN_(void) Shutdown();

  friend class TimerThread;

  void Fire();
  nsresult PostTimerEvent();
  void SetDelayInternal(PRUint32 aDelay);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMER

  PRInt32 GetGeneration() { return mGeneration; }

private:
  ~nsTimerImpl();
  nsresult InitCommon(PRUint32 aType, PRUint32 aDelay);

  void ReleaseCallback()
  {
    // if we're the last owner of the callback object, make
    // sure that we don't recurse into ReleaseCallback in case
    // the callback's destructor calls Cancel() or similar.
    PRUint8 cbType = mCallbackType;
    mCallbackType = CALLBACK_TYPE_UNKNOWN; 

    if (cbType == CALLBACK_TYPE_INTERFACE)
      NS_RELEASE(mCallback.i);
    else if (cbType == CALLBACK_TYPE_OBSERVER)
      NS_RELEASE(mCallback.o);
  }

  bool IsRepeating() const {
    PR_STATIC_ASSERT(TYPE_ONE_SHOT < TYPE_REPEATING_SLACK);
    PR_STATIC_ASSERT(TYPE_REPEATING_SLACK < TYPE_REPEATING_PRECISE);
    PR_STATIC_ASSERT(TYPE_REPEATING_PRECISE < TYPE_REPEATING_PRECISE_CAN_SKIP);
    return mType >= TYPE_REPEATING_SLACK;
  }

  bool IsRepeatingPrecisely() const {
    return mType >= TYPE_REPEATING_PRECISE;
  }

  nsCOMPtr<nsIEventTarget> mEventTarget;

  void *                mClosure;

  union CallbackUnion {
    nsTimerCallbackFunc c;
    nsITimerCallback *  i;
    nsIObserver *       o;
  } mCallback;

  // Some callers expect to be able to access the callback while the
  // timer is firing.
  nsCOMPtr<nsITimerCallback> mTimerCallbackWhileFiring;

  // These members are set by Init (called from NS_NewTimer) and never reset.
  PRUint8               mCallbackType;

  // These members are set by the initiating thread, when the timer's type is
  // changed and during the period where it fires on that thread.
  PRUint8               mType;
  bool                  mFiring;


  // Use a bool (int) here to isolate loads and stores of these two members
  // done on various threads under the protection of TimerThread::mLock, from
  // loads and stores done on the initiating/type-changing/timer-firing thread
  // to the above PRUint8/bool members.
  bool                  mArmed;
  bool                  mCanceled;

  // The generation number of this timer, re-generated each time the timer is
  // initialized so one-shot timers can be canceled and re-initialized by the
  // arming thread without any bad race conditions.
  PRInt32               mGeneration;

  PRUint32              mDelay;
  TimeStamp             mTimeout;

#ifdef DEBUG_TIMERS
  TimeStamp             mStart, mStart2;
  static double         sDeltaSum;
  static double         sDeltaSumSquared;
  static double         sDeltaNum;
#endif

};

#endif /* nsTimerImpl_h___ */
