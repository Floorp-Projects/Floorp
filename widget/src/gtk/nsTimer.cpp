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
#include <gtk/gtk.h>

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

extern "C" gint nsTimerExpired(gpointer aCallData);

/*
 * Implementation of timers using Gtk timer facility 
 */
class TimerImpl : public nsITimer {
public:

public:
  TimerImpl();
  virtual ~TimerImpl();

  virtual nsresult Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
//              PRBool aRepeat, 
                PRUint32 aDelay);

  virtual nsresult Init(nsITimerCallback *aCallback,
//              PRBool aRepeat, 
                PRUint32 aDelay);

  NS_DECL_ISUPPORTS

  virtual void Cancel();
  virtual PRUint32 GetDelay() { return mDelay; }
  virtual void SetDelay(PRUint32 aDelay) { mDelay=aDelay; };
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
  guint mTimerId;
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
//  mTimerId = gtk_timeout_add(aDelay, nsTimerExpired, this);
}


TimerImpl::TimerImpl()
{
  //  printf("TimerImple::TimerImpl called for %p\n", this);
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
  //printf("TimerImpl::~TimerImpl called for %p\n", this);
  Cancel();
  NS_IF_RELEASE(mCallback);
}

nsresult 
TimerImpl::Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
//              PRBool aRepeat, 
                PRUint32 aDelay)
{
  //printf("TimerImpl::Init called with func + closure for %p\n", this);
    mFunc = aFunc;
    mClosure = aClosure;
    // mRepeat = aRepeat;

  if ((aDelay > 10000) || (aDelay < 0)) {
    printf("Timer::Init() called with bogus value \"%d\"!  Not enabling timer.\n",
           aDelay);
    return Init(aDelay);
  }

    mTimerId = gtk_timeout_add(aDelay, nsTimerExpired, this);

    return Init(aDelay);
}

nsresult 
TimerImpl::Init(nsITimerCallback *aCallback,
//              PRBool aRepeat, 
                PRUint32 aDelay)
{
  //printf("TimerImpl::Init called with callback only for %p\n", this);
    mCallback = aCallback;
    NS_ADDREF(mCallback);
    // mRepeat = aRepeat;
  if ((aDelay > 10000) || (aDelay < 0)) {
    printf("Timer::Init() called with bogus value \"%d\"!  Not enabling timer.\n",
           aDelay);
    return Init(aDelay);
  }

    mTimerId = gtk_timeout_add(aDelay, nsTimerExpired, this);

    return Init(aDelay);
}

nsresult
TimerImpl::Init(PRUint32 aDelay)
{
  //printf("TimerImpl::Init called with delay %d only for %p\n", aDelay, this);

    mDelay = aDelay;
    //    NS_ADDREF(this);

    return NS_OK;
}

NS_IMPL_ISUPPORTS(TimerImpl, kITimerIID)


void
TimerImpl::Cancel()
{
  //printf("TimerImpl::Cancel called for %p\n", this);
  TimerImpl *me = this;
  if (mTimerId)
    gtk_timeout_remove(mTimerId);
}

NS_BASE nsresult NS_NewTimer(nsITimer** aInstancePtrResult)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if (nsnull == aInstancePtrResult) {
      return NS_ERROR_NULL_POINTER;
    }  

    TimerImpl *timer = new TimerImpl();
    if (nsnull == timer) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}

gint nsTimerExpired(gpointer aCallData)
{
  //printf("nsTimerExpired for %p\n", aCallData);
  TimerImpl* timer = (TimerImpl *)aCallData;
  timer->FireTimeout();
  return 0;
}
