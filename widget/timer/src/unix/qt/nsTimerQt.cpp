/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 */

#include "nsTimerQt.h"

#include <qtimer.h>

#include <stdio.h>
#include "nslog.h"

NS_IMPL_LOG(nsTimerQtLog)
#define PRINTF NS_LOG_PRINTF(nsTimerQtLog)
#define FLUSH  NS_LOG_FLUSH(nsTimerQtLog)

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

nsTimerQt::nsTimerQt()
{
    //debug("nsTimerQt::nsTimerQt called for %p", this);
    NS_INIT_REFCNT();
    mFunc         = nsnull;
    mCallback     = nsnull;
    mNext         = nsnull;
    mTimer        = nsnull;
    mDelay        = 0;
    mClosure      = nsnull;
    mEventHandler = nsnull;
}

nsTimerQt::~nsTimerQt()
{
    //debug("nsTimerQt::~nsTimerQt called for %p", this);
    Cancel();

    NS_IF_RELEASE(mCallback);

    if (mEventHandler) {
      delete mEventHandler;
      mEventHandler = nsnull;
    }

    if (mTimer) {
      delete mTimer;
      mTimer = nsnull;
    }
}

nsresult 
nsTimerQt::Init(nsTimerCallbackFunc aFunc, void *aClosure, PRUint32 aDelay,
                PRUint32 aPriority, PRUint32 aType)
{
    //debug("nsTimerQt::Init called with func + closure with %u delay", aDelay);
    mFunc = aFunc;
    mClosure = aClosure;

    return Init(aDelay);
}

nsresult 
nsTimerQt::Init(nsITimerCallback *aCallback, PRUint32 aDelay,
                PRUint32 aPriority, PRUint32 aType)
{
    //debug("nsTimerQt::Init called with callback only with %u delay", aDelay);
    mCallback = aCallback;
    NS_ADDREF(mCallback);
    return Init(aDelay);
}

nsresult
nsTimerQt::Init(PRUint32 aDelay)
{
    //debug("nsTimerQt::Init called with delay %d only for %p", aDelay, this);

    mEventHandler = new nsTimerEventHandler(this, mFunc, mClosure, mCallback);

    mTimer = new QTimer();
    
    if (!mTimer) {
        return NS_ERROR_NOT_INITIALIZED;
    }

    QObject::connect((QTimer *)mTimer, 
                     SIGNAL(timeout()), 
                     mEventHandler, 
                     SLOT(FireTimeout()));

    mTimer->start(aDelay);

    mDelay = aDelay;

    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsTimerQt, nsITimer)



void
nsTimerQt::Cancel()
{
    //debug("nsTimerQt::Cancel called for %p", this);
    if (mTimer) {
        mTimer->stop();
    }
}

#ifdef MOZ_MONOLITHIC_TOOLKIT
nsresult NS_NewTimer(nsITimer** aInstancePtrResult)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if (nsnull == aInstancePtrResult) {
        return NS_ERROR_NULL_POINTER;
    }  

    nsTimerQt *timer = new nsTimerQt();
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
