/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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

/* 
 * bydmgntr.h
 * John Sun
 * 2/3/98 3:06:20 PM
 */

#ifndef __BYDAYMONTHLYGENERATOR_H_
#define __BYDAYMONTHLYGENERATOR_H_

#include "dategntr.h"
#include "rrday.h"

/**
 * The ByDayMonthlyGenerator class generates dates for the 
 * BYDAY tag w/ a MONTHLY frequency.
 *
 * @see              DateGenerator
 */
class ByDayMonthlyGenerator : public DateGenerator
{
public:

    /**
     * Default Constructor
     */
    ByDayMonthlyGenerator();

    /**
     * Destructor
     */
    ~ByDayMonthlyGenerator() {}
    
    /*---------------------------------------------------------------------*/
    /* --- DateGenerator interface --- */

    /**
     * Returns the interval for which the generator represents.
     * @return          the interval (DAILY)
     */
    virtual t_int32 getInterval() const; 

    /**
     * Fills in dateVector with DateTime objects that correspond to the
     * parameters and start value and the until value.
     * DateTime generated should be after start's value and before until's
     * value.  If until is invalid, ignore it.
     *
     * @param           start       the date from which to begin generating recurrences
     * @param           dateVector  storage for the generated dates
     * @param           until       the end date for the recurrence
     *
     * @return          TRUE if until date boundary was reached, FALSE otherwise
     */
    virtual t_bool generate(DateTime * start, 
        JulianPtrArray & dateVector, DateTime * until);

    /**
     * Sets the week start parameter.
     * @param           param   the week start value
     */
    virtual void setParams(t_int32 param) { m_iWkSt = param; }
    
    /**
     * Sets the RRDay parameters.  
     * @param           params[]    the list of RRDay to set to
     * @param           length      the length of params[]
     * @see             RRDay
     */
    virtual void setRDay(RRDay params[], t_int32 length) 
    { 
        m_aiParams = params; 
        m_iParamsLen = length;
    }
 
    /**
     * Determines if a generator has been set active.
     * Usually tests if parameters have been set or not.
     *
     * @return          TRUE if parameters set, FALSE otherwise.
     */
    virtual t_bool active() { return (m_aiParams != 0); }

    /* --- End of DateGenerator interface --- */
    /*---------------------------------------------------------------------*/

private:

    /** 
     * ptr to array of RRDay objects 
     * @see RRDay 
     */
    RRDay * m_aiParams;
    
    /** 
     * week start value 
     * @see Calendar 
     */
    t_int32 m_iWkSt;

};

#endif /* __BYDAYMONTHLYGENERATOR_H_ */




