/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *	John C. Griggs <johng@corel.com>
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
#include "nsTimerQt.h"
#include "nsTimerEventHandler.h"

#include <qtimer.h>

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRUint32 gTimerCount = 0;
PRUint32 gTimerID = 0;
#endif

static NS_DEFINE_IID(kITimerIID,NS_ITIMER_IID);

nsTimerQt::nsTimerQt()
{
#ifdef DBG_JCG
  gTimerCount++;
  mTimerID = gTimerID++;
  printf("JCG: nsTimerQT CTOR (%p) ID: %d, Count: %d\n",this,mTimerID,gTimerCount);
#endif
  NS_INIT_REFCNT();
  mFunc         = nsnull;
  mCallback     = nsnull;
  mNext         = nsnull;
  mTimer        = nsnull;
  mDelay        = 0;
  mClosure      = nsnull;
  mEventHandler = nsnull;
  mPriority = 0;
  mType = NS_TYPE_ONE_SHOT;
}

nsTimerQt::~nsTimerQt()
{
#ifdef DBG_JCG
  gTimerCount--;
  printf("JCG: nsTimerQT DTOR (%p) ID: %d, Count: %d\n",this,mTimerID,gTimerCount);
#endif
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
#ifdef DBG_JCG
printf("JCG: nsTimerQT::Init (%p) ID: %d WITH Function\n",this,mTimerID);
printf("JCG: Priority: %d, Delay: %d, OneShot: %s\n",aPriority,aDelay,
       (aType == NS_TYPE_ONE_SHOT)?"Y":"N");
#endif
  mFunc = aFunc;
  mClosure = aClosure;
  mPriority = aPriority;
  mType = aType;

  return Init(aDelay);
}

nsresult 
nsTimerQt::Init(nsITimerCallback *aCallback, PRUint32 aDelay,
                PRUint32 aPriority, PRUint32 aType)
{
#ifdef DBG_JCG
printf("JCG: nsTimerQT::Init (%p) ID: %d WITH Callback\n",this,mTimerID);
printf("JCG: Priority: %d, Delay: %d, OneShot: %s\n",aPriority,aDelay,
       (aType == NS_TYPE_ONE_SHOT)?"Y":"N");
#endif
  mCallback = aCallback;
  NS_ADDREF(mCallback);
  mPriority = aPriority;
  mType = aType;
  return Init(aDelay);
}

nsresult
nsTimerQt::Init(PRUint32 aDelay)
{
  mEventHandler = new nsTimerEventHandler(this,mFunc,mClosure,mCallback);

  mTimer = new QTimer();
  if (!mTimer) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  QObject::connect((QTimer*)mTimer,SIGNAL(timeout()),
                   mEventHandler,SLOT(FireTimeout()));

  mTimer->start(aDelay,mType == NS_TYPE_ONE_SHOT);
  mDelay = aDelay;
  return NS_OK;
}

void nsTimerQt::SetDelay(PRUint32 aDelay)
{
  mTimer->changeInterval(aDelay);
  mDelay = aDelay;
}

NS_IMPL_ISUPPORTS1(nsTimerQt, nsITimer)

void
nsTimerQt::Cancel()
{
#ifdef DBG_JCG
printf("JCG: nsTimerQT::Cancel (%p) ID: %d\n",this,mTimerID);
#endif
  if (mTimer) {
    mTimer->stop();
  }
}

