/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Alexander Larsson (alla@lysator.liu.se)
 */

#include "nsTimerGtk.h"
#include "nsCOMPtr.h"

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

// extern "C" int  NS_TimeToNextTimeout(struct timeval *aTimer);
// extern "C" void NS_ProcessTimeouts(void);

extern "C" gboolean nsTimerExpired(gpointer aCallData);

/* Just linear interpolation. Is this good? */
static gint calc_priority(PRUint32 aPriority)
{
  int pri = ((G_PRIORITY_HIGH-G_PRIORITY_LOW)*(int)aPriority)/NS_PRIORITY_HIGHEST + G_PRIORITY_LOW;
  return pri;
}

PRBool nsTimerGtk::FireTimeout()
{
  if (mType == NS_TYPE_REPEATING_PRECISE) {
    mTimerId = g_timeout_add_full(calc_priority(mPriority),
                                  mDelay, nsTimerExpired, this, NULL);
  }

  // because Notify can cause 'this' to get destroyed, we need to hold a ref
  nsCOMPtr<nsITimer> kungFuDeathGrip = this;
  
  if (mFunc != NULL) {
    (*mFunc)(this, mClosure);
  }
  else if (mCallback != NULL) {
    mCallback->Notify(this); // Fire the timer
  }

  return (mType == NS_TYPE_REPEATING_SLACK);
}

void nsTimerGtk::SetDelay(PRUint32 aDelay)
{
  if (aDelay!=mDelay) {
    Cancel();
    mDelay=aDelay;
    mTimerId = g_timeout_add_full(calc_priority(mPriority),
                                  mDelay, nsTimerExpired, this, NULL);
  }
};

void nsTimerGtk::SetPriority(PRUint32 aPriority)
{
  if (aPriority!=mPriority) {
    Cancel();
    mPriority = aPriority;
    mTimerId = g_timeout_add_full(calc_priority(mPriority),
                                  mDelay, nsTimerExpired, this, NULL);
  }
}

void nsTimerGtk::SetType(PRUint32 aType)
{
  if (aType!=mType) {
    Cancel();
    mType = aType;
    mTimerId = g_timeout_add_full(calc_priority(mPriority),
                                  mDelay, nsTimerExpired, this, NULL);
  }
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
  mPriority = 0;
  mType = NS_TYPE_ONE_SHOT;
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
                 PRUint32 aDelay,
                 PRUint32 aPriority,
                 PRUint32 aType
                )
{
  //printf("nsTimerGtk::Init called with func + closure for %p\n", this);
    mFunc = aFunc;
    mClosure = aClosure;
    mPriority = aPriority;
    mType = aType;
    mDelay = aDelay;

    mTimerId = g_timeout_add_full(calc_priority(mPriority),
                                  mDelay, nsTimerExpired, this, NULL);
 
    return NS_OK;
}

nsresult 
nsTimerGtk::Init(nsITimerCallback *aCallback,
                 PRUint32 aDelay,
                 PRUint32 aPriority,
                 PRUint32 aType
                 )
{
  //printf("nsTimerGtk::Init called with callback only for %p\n", this);
    mCallback = aCallback;
    NS_ADDREF(mCallback);
    mPriority = aPriority;
    mType = aType;
    mDelay = aDelay;

    mTimerId = g_timeout_add_full(calc_priority(mPriority),
                                  mDelay, nsTimerExpired, this, NULL);
 
    return NS_OK;
}

NS_IMPL_ISUPPORTS(nsTimerGtk, kITimerIID)

void
nsTimerGtk::Cancel()
{
  //printf("nsTimerGtk::Cancel called for %p\n", this);

  if (mTimerId)
    g_source_remove(mTimerId);
}

gboolean nsTimerExpired(gpointer aCallData)
{
  //printf("nsTimerExpired for %p\n", aCallData);
  nsTimerGtk* timer = (nsTimerGtk *)aCallData;
  return timer->FireTimeout();
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
