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
 *		John C. Griggs <johng@corel.com>
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
#include "nsTimerEventHandler.h"

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRUint32 gTimerHandlerCount = 0;
PRUint32 gTimerHandlerID = 0;
#endif

nsTimerEventHandler::nsTimerEventHandler(nsITimer *aTimer,
                                         nsTimerCallbackFunc aFunc,
                                         void *aClosure,
                                         nsITimerCallback *aCallback)
{
#ifdef DBG_JCG
  gTimerHandlerCount++;
  mTimerHandlerID = gTimerHandlerID++;
  printf("JCG: nsTimerEventHandler CTOR (%p) ID: %d, Count: %d\n",
         this,mTimerHandlerID,gTimerHandlerCount);
#endif
  mTimer    = aTimer;
  mFunc     = aFunc;
  mClosure  = aClosure;
  mCallback = aCallback;
}
    
nsTimerEventHandler::~nsTimerEventHandler()
{
#ifdef DBG_JCG
  gTimerHandlerCount--;
  printf("JCG: nsTimerEventHandler DTOR (%p) ID: %d, Count: %d\n",
         this,mTimerHandlerID,gTimerHandlerCount);
#endif
}

void nsTimerEventHandler::FireTimeout()
{
#ifdef DBG_JCG
  printf("JCG: nsTimerEventHandler::FireTimeout (%p) ID: %d\n",
         this,mTimerHandlerID);
#endif
  //because Notify can cause 'this' to get destroyed,
  // we need to hold a ref
  nsCOMPtr<nsITimer> kungFuDeathGrip = mTimer;
  if (mFunc != NULL) {
#ifdef DBG_JCG
  printf("JCG: nsTimerEventHandler::FireTimeout: Calling Function\n");
#endif
    (*mFunc)(mTimer,mClosure);
  }
  else if (mCallback != NULL) {
#ifdef DBG_JCG
  printf("JCG: nsTimerEventHandler::FireTimeout: Calling Notify\n");
#endif
    mCallback->Notify(mTimer); // Fire the timer
  }
}
