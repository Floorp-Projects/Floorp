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
 

#ifndef nsTimerMac_h_
#define nsTimerMac_h_


#include "nsITimer.h"
#include "nsITimerCallback.h"

#include <MacTypes.h>


 //========================================================================================
class nsTimerImpl : public nsITimer 
// nsTimerImpl implements nsITimer API
//========================================================================================
{
    friend class nsTimerPeriodical;
    
  public:

  // constructors

              nsTimerImpl();
    virtual   ~nsTimerImpl();
      
    NS_DECL_ISUPPORTS

    PRUint32  GetFireTime() const { return mFireTime; }

    void      Fire();

  // nsITimer overrides

    NS_IMETHOD Init(nsTimerCallbackFunc aFunc,
                          void *aClosure,
                          PRUint32 aDelay,
                          PRUint32 aPriority = NS_PRIORITY_NORMAL,
                          PRUint32 aType = NS_TYPE_ONE_SHOT);

    NS_IMETHOD Init(nsITimerCallback *aCallback,
                          PRUint32 aDelay,
                          PRUint32 aPriority = NS_PRIORITY_NORMAL,
                          PRUint32 aType = NS_TYPE_ONE_SHOT);

    virtual void     Cancel();

    virtual PRUint32 GetDelay();
    virtual void     SetDelay(PRUint32 aDelay);

    virtual PRUint32 GetPriority();
    virtual void     SetPriority(PRUint32 aPriority);

    virtual PRUint32 GetType();
    virtual void     SetType(PRUint32 aType);

    virtual void*    GetClosure();
  
#if DEBUG
	enum {
		eGoodTimerSignature     = 'Barf',
		eDeletedTimerSignature  = 'oops'
	};
	
  	Boolean   IsGoodTimer() const { return (mSignature == eGoodTimerSignature); }
#endif
  	
  protected:
  
    // Calculates mFireTime too
    void      SetDelaySelf(PRUint32 aDelay);
    nsresult  Setup(PRUint32 aDelay, PRUint32 aPriority, PRUint32 aType);
    

  private:
    nsTimerCallbackFunc mCallbackFunc;
    nsITimerCallback *  mCallbackObject;
    void *              mClosure;
    PRUint32            mDelay;
    PRUint32            mPriority;
    PRUint32            mType;
    PRUint32            mFireTime;  // Timer should fire when TickCount >= this number
    nsTimerImpl*        mPrev; 
    nsTimerImpl*        mNext;  

    PRPackedBool        mTimerSpent;    // the timer is done, and was released
    PRPackedBool        mCanceledInCallback;       // whether the timer callback cancelled it
#if DEBUG
    UInt32		        mSignature;
#endif

};

#endif // nsTimerMac_h_
