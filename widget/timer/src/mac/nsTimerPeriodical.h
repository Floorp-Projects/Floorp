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

#include "nsRepeater.h"
#include "nsTimerMac.h"

#include <Events.h>

//========================================================================================
class nsTimerPeriodical : public Repeater
// nsTimerPeriodical is a singleton Repeater subclass that fires
// off nsTimerImpl. The firing is done on idle.
//========================================================================================
{      
public:

                      nsTimerPeriodical();
    virtual           ~nsTimerPeriodical();
    
    // Repeater interface
    virtual void      RepeatAction(const EventRecord &aMacEvent);
    virtual void      IdleAction(const EventRecord &aMacEvent);
    
    // utility methods for adding/removing timers
    nsresult          AddTimer(nsTimerImpl* aTimer);
    nsresult          RemoveTimer(nsTimerImpl* aTimer);
    
public:

    // Returns the singleton instance
    static nsTimerPeriodical * GetPeriodical();
    
protected:

    void              ProcessTimers(UInt32 currentTicks);
    void              ProcessExpiredTimer(nsTimerImpl* aTimer);

    void              FireNextReadyTimer();

    void              AddToReadyQueue(nsTimerImpl* aTimer);

    void              FireAndReprimeTimer(nsTimerImpl* aTimer);

    static nsresult   AddTimerToList(nsTimerImpl* aTimer, nsTimerImpl*& timerList);
    static nsresult   RemoveTimerFromList(nsTimerImpl* aTimer, nsTimerImpl*& timerList);
    

    nsTimerImpl*    mTimers;			   // linked list of pending timers
    
    nsTimerImpl*    mReadyTimers;    // timers that have fired and are ready to be called

    nsTimerImpl*    mFiringTimer;    // points to the timer being fired now. Used to avoid
                                     // problems calling Cancel() from the callback.
    
    static nsTimerPeriodical * gPeriodical;

};

