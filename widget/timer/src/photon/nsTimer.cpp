/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsCRT.h"
#include "prlog.h"
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#include <Pt.h>

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

/* Use the Widget Debug log */
extern PRLogModuleInfo *PhWidLog;

/*
 * Implementation of timers QNX/Neutrino timers.
 */

class TimerImpl : public nsITimer {
public:

public:
  TimerImpl();
  virtual ~TimerImpl();

  NS_DECL_ISUPPORTS

  virtual nsresult  Init( nsTimerCallbackFunc aFunc, void *aClosure, PRUint32 aDelay );
  virtual nsresult  Init( nsITimerCallback *aCallback, PRUint32 aDelay );
  virtual void      Cancel();
  virtual PRUint32  GetDelay() { return mDelay; }
  virtual void      SetDelay( PRUint32 aDelay ) { mDelay=aDelay; };
  virtual void*     GetClosure() { return mClosure; }

  static int TimerEventHandler( void *aData, pid_t aRcvId, void *aMsg, size_t aMsgLen );

private:

  nsresult SetupTimer( PRUint32 aDelay );
  
  PRUint32              mDelay;
  nsTimerCallbackFunc   mFunc;
  void                 *mClosure;
  nsITimerCallback     *mCallback;
  timer_t               mTimerId;
  pid_t                 mPulsePid;
  PtPulseMsg_t          mPulseMsg;
  PtPulseMsgId_t       *mPulseMsgId;
  PtInputId_t          *mInputId;
};


TimerImpl::TimerImpl()
{
  NS_INIT_REFCNT();
  mFunc = NULL;
  mClosure = NULL;
  mCallback = NULL;
  mDelay = 0;
  mTimerId = -1;
  mPulsePid = 0;
  mPulseMsgId = NULL;
  mInputId = NULL;
}


TimerImpl::~TimerImpl()
{
  Cancel();
  NS_IF_RELEASE(mCallback);
}


NS_IMPL_ISUPPORTS(TimerImpl, kITimerIID)

 
NS_METHOD TimerImpl::SetupTimer( PRUint32 aDelay )
{
  struct itimerspec  tv;
  int err;

  if ((aDelay > 10000) || (aDelay < 0))
  {
    NS_WARNING("TimerImpl::SetupTimer called with bogus value\n");
    return NS_ERROR_FAILURE;
  }

  mDelay = aDelay;
  if( mPulsePid )
  {
    NS_ASSERTION(0,"TimerImpl::SetupTimer - reuse of timer not allowed!");
    return NS_ERROR_FAILURE;
  }
  if(( mPulsePid = PtAppCreatePulse( NULL, -1 )) > -2 )
  {
    NS_ASSERTION(0,"TimerImpl::SetupTimer - failed to create pulse");
    return NS_ERROR_FAILURE;
  }
  if(( mPulseMsgId = PtPulseArmPid( NULL, mPulsePid, getpid(), &mPulseMsg )) == NULL )
  {
    NS_ASSERTION(0,"TimerImpl::SetupTimer - failed to arm pulse!");
    return NS_ERROR_FAILURE;
  }
  if(( mInputId = PtAppAddInput( NULL, mPulsePid, TimerEventHandler, this )) == NULL )
  {
    NS_ASSERTION(0,"TimerImpl::SetupTimer - failed to add input handler!");
    return NS_ERROR_FAILURE;
  }

  mTimerId = -1;
  err = timer_create( CLOCK_SOFTTIME, &mPulseMsg, &mTimerId );
  if( err != 0 )
  { 
    NS_ASSERTION(0,"TimerImpl::SetupTimer - timer_create error");
    return NS_ERROR_FAILURE;
  }

  tv.it_interval.tv_sec  = 0;
  tv.it_interval.tv_nsec = 0;
  tv.it_value.tv_sec     = ( aDelay / 1000 );
  tv.it_value.tv_nsec    = ( aDelay % 1000 ) * 1000000L;

  /* If delay is set to 0 seconds then change it to 1 nsec. */
  if ( (tv.it_value.tv_sec == 0) && (tv.it_value.tv_nsec == 0))
  {
    tv.it_value.tv_nsec = 1;
  }
  
  err = timer_settime( mTimerId, 0, &tv, 0 );
  if( err != 0 )
  {
    NS_ASSERTION(0,"TimerImpl::SetupTimer timer_settime");
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}


NS_METHOD TimerImpl::Init( nsTimerCallbackFunc aFunc, void *aClosure, PRUint32 aDelay )
{
  nsresult err;

  mFunc = aFunc;
  mClosure = aClosure;
  err = SetupTimer( aDelay );
  return err;
}


NS_METHOD TimerImpl::Init( nsITimerCallback *aCallback, PRUint32 aDelay )
{
  nsresult err;

  mCallback = aCallback;
  err = SetupTimer(aDelay);
  NS_ADDREF(mCallback);
  return err;
}


void TimerImpl::Cancel()
{
  int err;
  
  if( mTimerId >= 0)
  {
    err = timer_delete( mTimerId );
    if (err < 0)
    {
      char buf[256];
	  sprintf(buf, "TimerImpl::Cancel Failed in timer_delete mTimerId=<%d> err=<%d> errno=<%d>", mTimerId, err, errno);
      //NS_ASSERTION(0,"TimerImpl::Cancel Failed in timer_delete");
      NS_ASSERTION(0,buf);
      return;
    }
	
    mTimerId = -1;   // HACK for Debug 
  }

  if( mInputId )
  {
    PtAppRemoveInput( NULL, mInputId );
    mInputId = NULL;
  }

  if( mPulseMsgId )
  {
    PtPulseDisarm( mPulseMsgId );
    mPulseMsgId = NULL;
  }

  if( mPulsePid )
  {
    PtAppDeletePulse( NULL, mPulsePid );
    mPulsePid = 0;
  }
}


// This is the timer handler that gets called by the Photon
// input proc

int TimerImpl::TimerEventHandler( void *aData, pid_t aRcvId, void *aMsg, size_t aMsgLen )
{
  int localTimerId;

  TimerImpl* timer = (TimerImpl *)aData;
  if( timer )
  {
    localTimerId = timer->mTimerId;
	
    if( timer->mFunc != NULL )
    {
      (*timer->mFunc)( timer, timer->mClosure );
    }
    else if ( timer->mCallback != NULL )
    {
      timer->mCallback->Notify( timer );
    }

/* These stupid people destroy this object inside the callback */
/* so don't do anything with it from here on */
  }

  return Pt_CONTINUE;
}


nsresult NS_NewTimer(nsITimer** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "NS_NewTimer - null ptr");
  if (nsnull == aInstancePtrResult)
  {
    return NS_ERROR_NULL_POINTER;
  }  

  TimerImpl *timer = new TimerImpl();
  if (nsnull == timer)
  {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}
