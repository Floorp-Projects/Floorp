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

#ifndef TimerThread_h___
#define TimerThread_h___

#include "nsWeakReference.h"

#include "nsIEventQueueService.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsIThread.h"

#include "nsTimerImpl.h"

#include "nsVoidArray.h"

#include "prcvar.h"
#include "prinrval.h"
#include "prlock.h"

class TimerThread : public nsSupportsWeakReference,
                    public nsIRunnable,
                    public nsIObserver
{
public:
  TimerThread();
  NS_HIDDEN_(nsresult) InitLocks();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIOBSERVER
  
  NS_HIDDEN_(nsresult) Init();
  NS_HIDDEN_(nsresult) Shutdown();

  nsresult AddTimer(nsTimerImpl *aTimer);
  nsresult TimerDelayChanged(nsTimerImpl *aTimer);
  nsresult RemoveTimer(nsTimerImpl *aTimer);

#define FILTER_DURATION         1e3     /* one second */
#define FILTER_FEEDBACK_MAX     100     /* 1/10th of a second */

  void UpdateFilter(PRUint32 aDelay, PRIntervalTime aTimeout,
                    PRIntervalTime aNow);

  // For use by nsTimerImpl::Fire()
  nsCOMPtr<nsIEventQueueService> mEventQueueService;

  void DoBeforeSleep();
  void DoAfterSleep();

private:
  ~TimerThread();

  PRInt32 mInitInProgress;
  PRBool  mInitialized;

  // These two internal helper methods must be called while mLock is held.
  // AddTimerInternal returns the position where the timer was added in the
  // list, or -1 if it failed.
  PRInt32 AddTimerInternal(nsTimerImpl *aTimer);
  PRBool  RemoveTimerInternal(nsTimerImpl *aTimer);

  nsCOMPtr<nsIThread> mThread;
  PRLock *mLock;
  PRCondVar *mCondVar;

  PRPackedBool mShutdown;
  PRPackedBool mWaiting;
  PRPackedBool mSleeping;
  
  nsVoidArray mTimers;

#define DELAY_LINE_LENGTH_LOG2  5
#define DELAY_LINE_LENGTH_MASK  PR_BITMASK(DELAY_LINE_LENGTH_LOG2)
#define DELAY_LINE_LENGTH       PR_BIT(DELAY_LINE_LENGTH_LOG2)

  PRInt32  mDelayLine[DELAY_LINE_LENGTH];
  PRUint32 mDelayLineCounter;
  PRUint32 mMinTimerPeriod;     // milliseconds
  PRInt32  mTimeoutAdjustment;
};

#endif /* TimerThread_h___ */
