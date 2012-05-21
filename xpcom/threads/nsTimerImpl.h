/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
