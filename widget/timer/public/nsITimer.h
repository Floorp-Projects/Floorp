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
 *  Michael Lowe <michael.lowe@bigfoot.com>
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
#ifndef nsITimer_h___
#define nsITimer_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIComponentManager.h"

class nsITimer;
class nsITimerCallback;

// Implementations of nsITimer should be written such that there are no limitations
// on what can be called by the TimerCallbackFunc. On platforms like the Macintosh this
// means that callback functions must be called from the main event loop NOT from
// an interrupt.

/// Signature of timer callback function
typedef void
(*nsTimerCallbackFunc) (nsITimer *aTimer, void *aClosure);

/// Interface IID for nsITimer
#define NS_ITIMER_IID         \
{ 0x497eed20, 0xb740, 0x11d1,  \
{ 0x9b, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

// --- Timer priorities ---
#define NS_PRIORITY_HIGHEST 10
#define NS_PRIORITY_HIGH 8
#define NS_PRIORITY_NORMAL 5
#define NS_PRIORITY_LOW 2
#define NS_PRIORITY_LOWEST 0

// --- Timer types ---
#define NS_TYPE_ONE_SHOT 0           // Timer which fires once only

#define NS_TYPE_REPEATING_SLACK 1    // After firing, timer is stopped and not 
                                     // restarted until notifcation routine completes.   
                                     // Specified timer period will be at least time between 
                                     // when processing for last firing notifcation completes 
                                     // and when the next firing occurs.  This is the preferable
                                     // repeating type for most situations.

#define NS_TYPE_REPEATING_PRECISE 2  // Timer which aims to have constant time between firings.
                                     // The processing time for each timer notification should
                                     // not influence timer period.   However, if the processing
                                     // for the last timer firing could not be completed until  
                                     // just before the next firing occurs, then you could have 
                                     // two timer notification routines being executed in quick 
                                     // sucession.

// --- Indicate if timers on your platform support repeating timers ---
#if defined(XP_PC) || defined(XP_UNIX) || defined(XP_MAC) || defined(XP_BEOS)
#define REPEATING_TIMERS 1
#endif

/**
 * Timer class, used to invoke a function or method after a fixed
 * millisecond interval. <B>Note that this interface is subject to
 * change!</B>
 */
class nsITimer : public nsISupports {
public:  
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITIMER_IID)
  
  /**
   * Initialize a timer to fire after the given millisecond interval.
   * This version takes a function to call and a closure to pass to
   * that function.
   *
   * @param aFunc - The function to invoke
   * @param aClosure - an opaque pointer to pass to that function
   * @param aDelay - The millisecond interval
   * @param aPriority - The timer priority
   * @param aType - The timer type : one shot or repeating
   * @result - NS_OK if this operation was successful
   */
  NS_IMETHOD Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
                PRUint32 aDelay,
                PRUint32 aPriority = NS_PRIORITY_NORMAL,
                PRUint32 aType = NS_TYPE_ONE_SHOT
                )=0;

  /**
   * Initialize a timer to fire after the given millisecond interval.
   * This version takes an interface of type <code>nsITimerCallback</code>. 
   * The <code>Notify</code> method of this method is invoked.
   *
   * @param aCallback - The interface to notify
   * @param aDelay - The millisecond interval
   * @param aPriority - The timer priority
   * @param aType - The timer type : one shot or repeating
   * @result - NS_OK if this operation was successful
   */
  NS_IMETHOD Init(nsITimerCallback *aCallback,
                PRUint32 aDelay,
                PRUint32 aPriority = NS_PRIORITY_NORMAL,
                PRUint32 aType = NS_TYPE_ONE_SHOT
                )=0;

  /// Cancels the timeout
  NS_IMETHOD_(void) Cancel()=0;

  /// @return the millisecond delay of the timeout
  NS_IMETHOD_(PRUint32) GetDelay()=0;

  /// Change the millisecond interval for the timeout
  NS_IMETHOD_(void) SetDelay(PRUint32 aDelay)=0;

  NS_IMETHOD_(PRUint32) GetPriority()=0;
  NS_IMETHOD_(void) SetPriority(PRUint32 aPriority)=0;

  NS_IMETHOD_(PRUint32) GetType()=0;
  NS_IMETHOD_(void) SetType(PRUint32 aType)=0;

  /// @return the opaque pointer
  NS_IMETHOD_(void*) GetClosure()=0;
};

#endif // nsITimer_h___
