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
 
//
// Mac implementation of the nsITimer interface
//


#include "nsTimerMac.h"
#include "nsTimerPeriodical.h"

#include "prlog.h"


//========================================================================================
// nsTimerImpl implementation
//========================================================================================

NS_IMPL_ISUPPORTS1(nsTimerImpl, nsITimer)

//----------------------------------------------------------------------------------------
nsTimerImpl::nsTimerImpl()
//----------------------------------------------------------------------------------------
:  mCallbackFunc(nsnull)
,  mCallbackObject(nsnull)
,  mClosure(nsnull)
,  mDelay(0)
,  mPriority(NS_PRIORITY_NORMAL)
,  mType(NS_TYPE_ONE_SHOT)
,  mFireTime(0)
,  mPrev(nsnull)
,  mNext(nsnull)
,  mTimerSpent(PR_FALSE)
,  mCanceledInCallback(PR_FALSE)
#if DEBUG
,  mSignature(eGoodTimerSignature)
#endif
{
  NS_INIT_REFCNT();
}

//----------------------------------------------------------------------------------------
nsTimerImpl::~nsTimerImpl()
//----------------------------------------------------------------------------------------
{
  // hack hack. Cancel() calls Remove... which Releases us again. Need to bump
  // up refCnt to avoid recursive delete.
  mRefCnt = 99;  
  
  Cancel();
  NS_IF_RELEASE(mCallbackObject);
#if DEBUG
  mSignature = eDeletedTimerSignature;
#endif
}

//----------------------------------------------------------------------------------------
nsresult nsTimerImpl::Init(nsTimerCallbackFunc aFunc,
                       void *aClosure,
                       PRUint32 aDelay,
                       PRUint32 aPriority,
                       PRUint32 aType)
//----------------------------------------------------------------------------------------
{
  mCallbackFunc = aFunc;
  mClosure = aClosure;
  return Setup(aDelay, aPriority, aType);
}

//----------------------------------------------------------------------------------------
nsresult nsTimerImpl::Init(nsITimerCallback *aCallback,
                       PRUint32 aDelay,
                       PRUint32 aPriority,
                       PRUint32 aType)
//----------------------------------------------------------------------------------------
{
  NS_ADDREF(aCallback);
  mCallbackObject = aCallback;
  return Setup(aDelay, aPriority, aType);
}


//----------------------------------------------------------------------------------------
nsresult nsTimerImpl::Setup(PRUint32 aDelay, PRUint32 aPriority, PRUint32 aType)
//----------------------------------------------------------------------------------------
{
  SetPriority(aPriority);
  SetType(aType);
  SetDelaySelf(aDelay);
  return nsTimerPeriodical::GetPeriodical()->AddTimer(this);
}


//----------------------------------------------------------------------------------------
void nsTimerImpl::Cancel()
//----------------------------------------------------------------------------------------
{
  nsTimerPeriodical::GetPeriodical()->RemoveTimer(this);
}

//----------------------------------------------------------------------------------------
PRUint32 nsTimerImpl::GetDelay()
//----------------------------------------------------------------------------------------
{
  return mDelay;
}

//----------------------------------------------------------------------------------------
void nsTimerImpl::SetDelay(PRUint32 aDelay)
//----------------------------------------------------------------------------------------
{
  SetDelaySelf(aDelay);
}

//----------------------------------------------------------------------------------------
void* nsTimerImpl::GetClosure()
//----------------------------------------------------------------------------------------
{
  return mClosure;
}

//----------------------------------------------------------------------------------------
void nsTimerImpl::Fire()
//----------------------------------------------------------------------------------------
{
  NS_PRECONDITION(mRefCnt > 0, "Firing a disposed Timer!");

  if (mCallbackFunc != NULL)
  {
    (*mCallbackFunc)(this, mClosure);
  }
  else if (mCallbackObject != NULL)
  {
    mCallbackObject->Notify(this);
  }
}

//----------------------------------------------------------------------------------------
void nsTimerImpl::SetDelaySelf( PRUint32 aDelay )
//----------------------------------------------------------------------------------------
{

  mDelay = aDelay;
  mFireTime = ::TickCount() + (mDelay * 3) / 50;  // We need mFireTime in ticks (1/60th)
                          //  but aDelay is in 1000th (60/1000 = 3/50)
                          
  // we need to re-order in the periodical here, I think
}


//----------------------------------------------------------------------------------------
PRUint32 nsTimerImpl::GetPriority()
//----------------------------------------------------------------------------------------
{
  return mPriority;
}

//----------------------------------------------------------------------------------------
void nsTimerImpl::SetPriority(PRUint32 aPriority)
//----------------------------------------------------------------------------------------
{
  PR_ASSERT(aPriority >= NS_PRIORITY_LOWEST && 
            aPriority <= NS_PRIORITY_HIGHEST); 
  mPriority = aPriority;
}

//----------------------------------------------------------------------------------------
PRUint32 nsTimerImpl::GetType()
//----------------------------------------------------------------------------------------
{
  return mType;
}


//----------------------------------------------------------------------------------------
void nsTimerImpl::SetType(PRUint32 aType)
//----------------------------------------------------------------------------------------
{
  mType = aType;
}


#pragma mark -

#if 0
//----------------------------------------------------------------------------------------
PR_PUBLIC_API(nsresult) NS_NewTimer(nsITimer** aInstancePtrResult)
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if (nsnull == aInstancePtrResult) {
      return NS_ERROR_NULL_POINTER;
    }  
    nsTimerImpl *timer = new nsTimerImpl();
    if (nsnull == timer) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return timer->QueryInterface(nsITimer::GetIID(), (void **) aInstancePtrResult);
}
#endif
