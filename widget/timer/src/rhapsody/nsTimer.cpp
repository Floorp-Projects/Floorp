/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsCRT.h"
#include "prlog.h"
#include <stdio.h>
#include <limits.h>

//
// Copied from the unix version, Rhapsody needs to 
// make this work.  Stubs to compile things for now.
//

#if 0
Michael Hanni <mhanni@sprintmail.com> suggests:

  I understand that nsTimer.cpp in base/rhapsody/ needs to be completed,
  yes? Wouldn't this code just use some NSTimers in the NSRunLoop?

  Timer = [NSTimer timerWithTimeInterval:0.02 //seconds
                target:self
                selector:@selector(doThis:)
                userInfo:nil
                repeats:YES];
  [[NSRunLoop currentRunLoop] addTimer:Timer
    forMode:NSDefaultRunLoopMode];

  I only looked at nsTimer.cpp briefly, but could something like this work
  if imbedded in all that c++? ;-)

#endif

extern void nsTimerExpired(void *aCallData);

class TimerImpl : public nsITimer {
public:

public:
  TimerImpl();
  virtual ~TimerImpl();

  virtual nsresult Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
                PRUint32 aDelay,
                PRUint32 aPriority = NS_PRIORITY_NORMAL,
                PRUint32 aType = NS_TYPE_ONE_SHOT
                );

  virtual nsresult Init(nsITimerCallback *aCallback,
                PRUint32 aDelay,
                PRUint32 aPriority = NS_PRIORITY_NORMAL,
                PRUint32 aType = NS_TYPE_ONE_SHOT
                );

  NS_DECL_ISUPPORTS

  virtual void Cancel();

  virtual PRUint32 GetDelay() { return mDelay; }
  virtual void SetDelay(PRUint32 aDelay) { mDelay=aDelay; };

  virtual PRUint32 GetPriority() {}
  virtual void SetPriority(PRUint32 aPriority) {}

  virtual PRUint32 GetType() {}
  virtual void SetType(PRUint32 aType) {}

  virtual void* GetClosure() { return mClosure; }

  void FireTimeout();

private:
  nsresult Init(PRUint32 aDelay);

  PRUint32 mDelay;
  nsTimerCallbackFunc mFunc;
  void *mClosure;
  nsITimerCallback *mCallback;
  // PRBool mRepeat;
  TimerImpl *mNext;
  int mTimerId; 
};

void TimerImpl::FireTimeout()
{
  if (mFunc != NULL) {
    (*mFunc)(this, mClosure);
  }
  else if (mCallback != NULL) {
    mCallback->Notify(this); // Fire the timer
  }

// Always repeating here

// if (mRepeat)
//  mTimerId = XtAppAddTimeOut(gAppContext, GetDelay(),(XtTimerCallbackProc)nsTimerExpired, this);
}


TimerImpl::TimerImpl()
{
  NS_INIT_REFCNT();
  mFunc = NULL;
  mCallback = NULL;
  mNext = NULL;
  mTimerId = 0;
  mDelay = 0;
  mClosure = NULL;
}

TimerImpl::~TimerImpl()
{
}

nsresult 
TimerImpl::Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
                PRUint32 aDelay,
                PRUint32 aPriority,
                PRUint32 aType
                )
{
    mFunc = aFunc;
    mClosure = aClosure;
    // mRepeat = aRepeat;

    printf("TimerImpl::Init() not implemented\n");

#ifdef RHAPSODY_NEEDS_TO_IMPLEMENT_THIS
    mTimerId = XtAppAddTimeOut(gAppContext, aDelay,(XtTimerCallbackProc)nsTimerExpired, this);
#endif

    return Init(aDelay);
}

nsresult 
TimerImpl::Init(nsITimerCallback *aCallback,
                PRUint32 aDelay,
                PRUint32 aPriority,
                PRUint32 aType
                )
{
    mCallback = aCallback;
    // mRepeat = aRepeat;

printf("TimerImpl::Init() not implmented.\n");

#ifdef RHAPSODY_NEEDS_TO_IMPLEMENT_THIS
    mTimerId = XtAppAddTimeOut(gAppContext, aDelay, (XtTimerCallbackProc)nsTimerExpired, this);
#endif

    return Init(aDelay);
}

nsresult
TimerImpl::Init(PRUint32 aDelay)
{
    mDelay = aDelay;
    NS_ADDREF(this);

    return NS_OK;
}

NS_IMPL_ISUPPORTS1(TimerImpl, nsITimer)


void
TimerImpl::Cancel()
{

  printf("TimerImpl::Cancel() not implemented.\n");

#ifdef RHAPSODY_NEEDS_TO_IMPLEMENT_THIS
  XtRemoveTimeOut(mTimerId);
#endif
}

nsresult NS_NewTimer(nsITimer** aInstancePtrResult)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if (nsnull == aInstancePtrResult) {
      return NS_ERROR_NULL_POINTER;
    }  

    TimerImpl *timer = new TimerImpl();
    if (nsnull == timer) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return CallQueryInterface(timer, aInstancePtrResult);
}


void nsTimerExpired(void *aCallData)
{
  TimerImpl* timer = (TimerImpl *)aCallData;
  timer->FireTimeout();
}
