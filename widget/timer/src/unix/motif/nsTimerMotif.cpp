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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsTimerMotif.h"

// #include "nsITimerCallback.h"
// #include "nsCRT.h"
// #include "prlog.h"
// #include <stdio.h>
// #include <limits.h>
// #include <X11/Intrinsic.h>

#include "nsIComponentManager.h"
#include "nsIMotifAppContextService.h"
#include "nslog.h"

NS_IMPL_LOG(nsTimerMotifLog)
#define PRINTF NS_LOG_PRINTF(nsTimerMotifLog)
#define FLUSH  NS_LOG_FLUSH(nsTimerMotifLog)

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

extern void nsTimerExpired(XtPointer aCallData);

void nsTimerMotif::FireTimeout()
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

nsTimerMotif::nsTimerMotif()
{
  NS_INIT_REFCNT();
  mFunc = NULL;
  mCallback = NULL;
  mNext = NULL;
  mTimerId = 0;
  mDelay = 0;
  mClosure = NULL;
  mAppContext = nsnull;
}

nsTimerMotif::~nsTimerMotif()
{
}

static NS_DEFINE_CID(kCMotifAppContextServiceCID, NS_MOTIF_APP_CONTEXT_SERVICE_CID);

nsresult 
nsTimerMotif::Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
                PRUint32 aDelay,
                PRUint32 aPriority,
                PRUint32 aType
                )
{
    mFunc = aFunc;
    mClosure = aClosure;
    // mRepeat = aRepeat;

    EnsureAppContext();

    mTimerId = XtAppAddTimeOut(mAppContext, 
                               aDelay,
                               (XtTimerCallbackProc)nsTimerExpired, this);

    return Init(aDelay);
}

nsresult 
nsTimerMotif::Init(nsITimerCallback *aCallback,
                PRUint32 aDelay,
                PRUint32 aPriority,
                PRUint32 aType
                )
{
    mCallback = aCallback;
    // mRepeat = aRepeat;

    EnsureAppContext();

    mTimerId = XtAppAddTimeOut(mAppContext, 
                               aDelay, 
                               (XtTimerCallbackProc)nsTimerExpired, 
                               this);

    return Init(aDelay);
}

nsresult
nsTimerMotif::Init(PRUint32 aDelay)
{
  EnsureAppContext();
  
  mDelay = aDelay;
  NS_ADDREF(this);
  
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsTimerMotif, kITimerIID)


void
nsTimerMotif::Cancel()
{
  XtRemoveTimeOut(mTimerId);
}

void nsTimerExpired(XtPointer aCallData)
{
  nsTimerMotif* timer = (nsTimerMotif *)aCallData;
  timer->FireTimeout();
}

nsresult 
nsTimerMotif::EnsureAppContext()
{
  static XtAppContext gsAppContext = nsnull;

  mAppContext = nsnull;

  if (nsnull == gsAppContext)
  {
    nsresult   rv;
    nsIMotifAppContextService * ac_service = nsnull;
    
    rv = nsComponentManager::CreateInstance(kCMotifAppContextServiceCID,
                                            nsnull,
                                            NS_GET_IID(nsIMotifAppContextService),
                                            (void **)& ac_service);
    
    NS_ASSERTION(rv == NS_OK,"Cannot obtain app context service.");

    if (ac_service)
    {
      PRINTF("nsTimerMotif::EnsureAppContext() ac_service = %p\n",ac_service);

      nsresult rv2 = ac_service->GetAppContext(&gsAppContext);

      NS_ASSERTION(rv2 == NS_OK,"Cannot get the app context.");

      NS_ASSERTION(nsnull != gsAppContext,"Global app context is null.");

      NS_RELEASE(ac_service);

      PRINTF("nsTimerMotif::EnsureAppContext() gsAppContext = %p\n",gsAppContext);
    }
  }

  mAppContext = gsAppContext;
  
  return NS_OK;
}

#ifdef MOZ_MONOLITHIC_TOOLKIT
nsresult NS_NewTimer(nsITimer** aInstancePtrResult)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if (nsnull == aInstancePtrResult) {
      return NS_ERROR_NULL_POINTER;
    }  

    nsTimerMotif *timer = new nsTimerMotif();
    if (nsnull == timer) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}
#endif /* MOZ_MONOLITHIC_TOOLKIT */
