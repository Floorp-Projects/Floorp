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
 */

#ifndef __nsTimerPh_h
#define __nsTimerPh_h

#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <Pt.h>

/*
 * Implementation of timers using Photon timer facility 
 */
class nsTimerPh : public nsITimer 
{
public:

  nsTimerPh();
  virtual ~nsTimerPh();

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
  virtual void SetDelay(PRUint32 aDelay);

  virtual PRUint32 GetPriority() { return mPriority; }
  virtual void SetPriority(PRUint32 aPriority);

  virtual PRUint32 GetType()  { return mType; }
  virtual void SetType(PRUint32 aType);

  virtual void* GetClosure() { return mClosure; }

  PRBool FireTimeout();

  static int TimerEventHandler( void *aData, pid_t aRcvId, void *aMsg, size_t aMsgLen );

private:
  nsresult SetupTimer();

  PRUint32             mDelay;
  PRUint32             mPriority;
  PRUint32             mType;
  nsTimerCallbackFunc  mFunc;
  void *               mClosure;
  nsITimerCallback *   mCallback;
  nsTimerPh *          mNext;
  timer_t              mTimerId;

  /* Photon Specific Data Members */
  pid_t                mPulsePid;
  PtPulseMsg_t         mPulseMsg;
  PtPulseMsgId_t *     mPulseMsgId;
  PtInputId_t *        mInputId;
};

#endif // __nsTimerPh_h
