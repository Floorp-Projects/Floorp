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
 *   Tony Tsui <tony@igleaus.com.au>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#include "nsTimerXlib.h"

#include "nsVoidArray.h"
#include <unistd.h>
#include <stdio.h>

#include <xlibrgb.h>

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

nsVoidArray *nsTimerXlib::gHighestList = (nsVoidArray *)nsnull;
nsVoidArray *nsTimerXlib::gHighList    = (nsVoidArray *)nsnull;
nsVoidArray *nsTimerXlib::gNormalList  = (nsVoidArray *)nsnull;
nsVoidArray *nsTimerXlib::gLowList     = (nsVoidArray *)nsnull;
nsVoidArray *nsTimerXlib::gLowestList  = (nsVoidArray *)nsnull;
PRPackedBool nsTimerXlib::gTimeoutAdded = PR_FALSE;
Display *nsTimerXlib::mDisplay = nsnull;

/* Interval how controlls how often we visit the timer queues ...*/
#define CALLPROCESSTIMEOUTSVAL (10)

/* local prototypes */
PR_BEGIN_EXTERN_C
static void CallProcessTimeoutsXtProc( XtPointer dummy1, XtIntervalId *dummy2 );
PR_END_EXTERN_C

nsTimerXlib::nsTimerXlib() :
  mFunc(nsnull),
  mCallback(nsnull),
  mDelay(0),
  mClosure(nsnull),
  mPriority(0),
  mType(NS_TYPE_ONE_SHOT),
  mQueued(PR_FALSE)
{
  NS_INIT_REFCNT();
}

nsTimerXlib::~nsTimerXlib()
{ 
  Cancel();
  NS_IF_RELEASE(mCallback);
}

void nsTimerXlib::Shutdown()
{
  if (gHighestList) delete gHighestList; gHighestList = nsnull;
  if (gHighList)    delete gHighList;    gHighList    = nsnull;
  if (gNormalList)  delete gNormalList;  gNormalList  = nsnull;
  if (gLowList)     delete gLowList;     gLowList     = nsnull;
  if (gLowestList)  delete gLowestList;  gLowestList  = nsnull;

  gTimeoutAdded = PR_FALSE;
}

NS_IMPL_ISUPPORTS1(nsTimerXlib, nsITimer)

nsresult
nsTimerXlib::Init(nsTimerCallbackFunc aFunc,
                  void *aClosure,
                  PRUint32 aDelay,
                  PRUint32 aPriority,
                  PRUint32 aType )
{
  mFunc = aFunc;
  mClosure = aClosure;
  mType = aType;
  return Init(aDelay, aPriority);
}

nsresult 
nsTimerXlib::Init(nsITimerCallback *aCallback,
                  PRUint32 aDelay,
                  PRUint32 aPriority,
                  PRUint32 aType)
{
  mType = aType;
  mCallback = aCallback;
  NS_ADDREF(mCallback);
  return Init(aDelay, aPriority);
}

nsresult
nsTimerXlib::Init(PRUint32 aDelay, PRUint32 aPriority)
{
  struct timeval Now;
#ifdef TIMER_DEBUG
  fprintf(stderr, "nsTimerXlib::Init (%p) called with delay %d\n", this, aDelay);
#endif
  
  mDelay = aDelay;
  mPriority = aPriority;
  // get the cuurent time
  
  gettimeofday(&Now, nsnull);
  mFireTime.tv_sec = Now.tv_sec + (aDelay / 1000);
  mFireTime.tv_usec = Now.tv_usec + ((aDelay%1000) * 1000);

  if (mFireTime.tv_usec >= 1000000) 
  {
    mFireTime.tv_sec++;
    mFireTime.tv_usec -= 1000000;
  }

#ifdef TIMER_DEBUG
  fprintf(stderr, "fire set to %ld / %ld\n", mFireTime.tv_sec, mFireTime.tv_usec);
#endif

  if (!gTimeoutAdded)
  {
    mDisplay = xxlib_rgb_get_display(xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE));
    NS_ASSERTION(mDisplay!=nsnull, "xxlib_rgb_get_display() returned nsnull display. BAD.");
    XtAppContext app_context = XtDisplayToApplicationContext(mDisplay);
    NS_ASSERTION(app_context!=nsnull, "XtDisplayToApplicationContext() returned nsnull Xt app context. BAD.");
    if (!app_context)
      return NS_ERROR_OUT_OF_MEMORY;

    gHighestList = new nsVoidArray(64);
    gHighList    = new nsVoidArray(64);
    gNormalList  = new nsVoidArray(64);
    gLowList     = new nsVoidArray(64);
    gLowestList  = new nsVoidArray(64);

    if (!(gHighestList && gHighList && gNormalList && gLowList && gLowestList)) {
      NS_WARNING("nsTimerXlib initalisation failed. DIE!");
      return NS_ERROR_OUT_OF_MEMORY;
    }  
    
    // start timers
    XtAppAddTimeOut(app_context, 
                    CALLPROCESSTIMEOUTSVAL,
                    CallProcessTimeoutsXtProc, 
                    app_context);

    gTimeoutAdded = PR_TRUE;
  }

  switch (aPriority)
  {
    case NS_PRIORITY_HIGHEST:
      mQueued = gHighestList->AppendElement(this);
      break;
    case NS_PRIORITY_HIGH:
      mQueued = gHighList->AppendElement(this);
      break;
    case NS_PRIORITY_NORMAL:
      mQueued = gNormalList->AppendElement(this);
      break;
    case NS_PRIORITY_LOW:
      mQueued = gLowList->AppendElement(this);
      break;
    default:  
    case NS_PRIORITY_LOWEST:
      mQueued = gLowestList->AppendElement(this);
      break;  
  }
  
  NS_ASSERTION(mQueued, "can't add timer to queue list!");
  
  return (mQueued)?(NS_OK):(NS_ERROR_OUT_OF_MEMORY);
}

PRBool
nsTimerXlib::Fire()
{
  nsCOMPtr<nsITimer> kungFuDeathGrip = this;

  if (mFunc != nsnull) {
    (*mFunc)(this, mClosure);
  }
  else if (mCallback != nsnull) {
    mCallback->Notify(this);
  }

  return ((mType == NS_TYPE_REPEATING_SLACK) || (mType == NS_TYPE_REPEATING_PRECISE));
}

void
nsTimerXlib::Cancel()
{
  if (!mQueued)
    return;
    
  switch(mPriority)
  {
    case NS_PRIORITY_HIGHEST:
      gHighestList->RemoveElement(this);
      break;
    case NS_PRIORITY_HIGH:
      gHighList->RemoveElement(this);
      break;
    case NS_PRIORITY_NORMAL:
      gNormalList->RemoveElement(this);
      break;
    case NS_PRIORITY_LOW:
      gLowList->RemoveElement(this);
      break;
    case NS_PRIORITY_LOWEST:
      gLowestList->RemoveElement(this);
      break;
  }
}

void
nsTimerXlib::ProcessTimeouts(nsVoidArray *array)
{
  PRInt32 count = array->Count();

  if (count == 0)
    return;
   
  struct timeval aNow;
  struct timeval ntv;
  int res;

  gettimeofday(&aNow, nsnull);

#ifdef TIMER_DEBUG  
  fprintf(stderr, "nsTimerXlib::ProcessTimeouts called at %ld / %ld\n",
         aNow.tv_sec, aNow.tv_usec);
#endif
  
  nsCOMPtr<nsTimerXlib> timer;
  
  for (int i = count; i >=0; i--)
  {
    /*  Make sure that the timer cannot be deleted during the
     *  Fire(...) call which may release *all* other references
     *  to p... 
     */
    timer = NS_STATIC_CAST(nsTimerXlib *, array->ElementAt(i));

    if (timer)
    {
      if ((timer->mFireTime.tv_sec < aNow.tv_sec) ||
          ((timer->mFireTime.tv_sec == aNow.tv_sec) &&
           (timer->mFireTime.tv_usec <= aNow.tv_usec))) 
      {
#ifdef TIMER_DEBUG
        fprintf(stderr, "Firing timeout for (%p)\n", timer);
#endif     
        res = timer->Fire();      
      
        if (res == 0) 
        {
          array->RemoveElement(timer);
        }
        else 
        {
          gettimeofday(&ntv, nsnull);
          timer->mFireTime.tv_sec = ntv.tv_sec + (timer->mDelay / 1000);
          timer->mFireTime.tv_usec = ntv.tv_usec + ((timer->mDelay%1000) * 1000);
        }     
      }
    }

    /* force destruction of timers (via nsCOMPtr magic) which do not
     * have other references anymore !
     */
    timer = nsnull;   
  }
}

void nsTimerXlib::SetDelay(PRUint32 aDelay) 
{
  mDelay = aDelay;
}

void nsTimerXlib::SetPriority(PRUint32 aPriority) 
{
  mPriority = aPriority;
}
              
void nsTimerXlib::SetType(PRUint32 aType) 
{
  mType = aType;
}

void nsTimerXlib::CallProcessTimeouts(XtAppContext app_context)
{
  ProcessTimeouts(gHighestList);
  ProcessTimeouts(gHighList);
  ProcessTimeouts(gNormalList);
 
  /* no X events anymore ? then crawl the low priority timers ...
   * Smallish hack: |if ((XtAppPending(app_context) & XtIMXEvent) == 0)|
   *  would be the correct way in libXt apps. - but I want to avoid any
   * possible call to XFlush() ...
   */
  if (XEventsQueued(mDisplay, QueuedAlready) == 0)
  {
    ProcessTimeouts(gLowList);
    ProcessTimeouts(gLowestList);
  }
}

/* must be a |XtInputCallbackProc| !! */
PR_BEGIN_EXTERN_C
static
void CallProcessTimeoutsXtProc( XtPointer dummy1, XtIntervalId *dummy2 )
{
  XtAppContext app_context = (XtAppContext) dummy1;

  nsTimerXlib::CallProcessTimeouts(app_context);
  
  /* reset timer */
  XtAppAddTimeOut(app_context, 
                  CALLPROCESSTIMEOUTSVAL,
                  CallProcessTimeoutsXtProc, 
                  app_context);
}
PR_END_EXTERN_C



