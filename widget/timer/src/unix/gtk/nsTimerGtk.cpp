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

#include "nsTimerGtk.h"

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

// extern "C" int  NS_TimeToNextTimeout(struct timeval *aTimer);
// extern "C" void NS_ProcessTimeouts(void);

extern "C" gint nsTimerExpired(gpointer aCallData);

void nsTimerGtk::FireTimeout()
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


nsTimerGtk::nsTimerGtk()
{
  //  printf("nsTimerGtke::nsTimerGtk called for %p\n", this);
  NS_INIT_REFCNT();
  mFunc = NULL;
  mCallback = NULL;
  mNext = NULL;
  mTimerId = 0;
  mDelay = 0;
  mClosure = NULL;
}

nsTimerGtk::~nsTimerGtk()
{

//  printf("nsTimerGtk::~nsTimerGtk called for %p\n", this);

  Cancel();
  NS_IF_RELEASE(mCallback);
}

nsresult 
nsTimerGtk::Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
//              PRBool aRepeat, 
                PRUint32 aDelay)
{
  //printf("nsTimerGtk::Init called with func + closure for %p\n", this);
    mFunc = aFunc;
    mClosure = aClosure;
    // mRepeat = aRepeat;

// This is ancient debug code that is making it impossible to have timeouts
// greater than 10 seconds. -re

//   if ((aDelay > 10000) || (aDelay < 0)) {
//     printf("Timer::Init() called with bogus value \"%d\"!  Not enabling timer.\n",
//            aDelay);
//     return Init(aDelay);
//   }

    mTimerId = gtk_timeout_add(aDelay, nsTimerExpired, this);

    return Init(aDelay);
}

nsresult 
nsTimerGtk::Init(nsITimerCallback *aCallback,
//              PRBool aRepeat, 
                PRUint32 aDelay)
{
  //printf("nsTimerGtk::Init called with callback only for %p\n", this);
    mCallback = aCallback;
    NS_ADDREF(mCallback);
    // mRepeat = aRepeat;

// This is ancient debug code that is making it impossible to have timeouts
// greater than 10 seconds. -re

//   if ((aDelay > 10000) || (aDelay < 0)) {
//     printf("Timer::Init() called with bogus value \"%d\"!  Not enabling timer.\n",
//            aDelay);
//     return Init(aDelay);
//   }

    mTimerId = gtk_timeout_add(aDelay, nsTimerExpired, this);

    return Init(aDelay);
}

nsresult
nsTimerGtk::Init(PRUint32 aDelay)
{
  //printf("nsTimerGtk::Init called with delay %d only for %p\n", aDelay, this);

    mDelay = aDelay;
    //    NS_ADDREF(this);

    return NS_OK;
}

NS_IMPL_ISUPPORTS(nsTimerGtk, kITimerIID)


void
nsTimerGtk::Cancel()
{
  //printf("nsTimerGtk::Cancel called for %p\n", this);

  //nsTimerGtk *me = this;

  if (mTimerId)
    gtk_timeout_remove(mTimerId);

  //
  // This is reported as a leak by purify, but it also causes a 
  // crash on startup if uncommented.  Need to dig deeper.
  //
  //NS_RELEASE(me);
  //
}

gint nsTimerExpired(gpointer aCallData)
{
  //printf("nsTimerExpired for %p\n", aCallData);
  nsTimerGtk* timer = (nsTimerGtk *)aCallData;
  timer->FireTimeout();
  return 0;
}

#ifdef MOZ_MONOLITHIC_TOOLKIT
nsresult NS_NewTimer(nsITimer** aInstancePtrResult)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if (nsnull == aInstancePtrResult) {
      return NS_ERROR_NULL_POINTER;
    }  

    nsTimerGtk *timer = new nsTimerGtk();
    if (nsnull == timer) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}

int NS_TimeToNextTimeout(struct timeval *aTimer) 
{
  return 0;
}

void NS_ProcessTimeouts(void) 
{
}
#endif /* MOZ_MONOLITHIC_TOOLKIT */
