/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*- */
/* 
 * dategntr.h
 * John Sun
 * 2/3/98 2:57:44 PM
 */

#ifndef __DATEGENERATOR_H_
#define __DATEGENERATOR_H_

#include <assert.h>
#include "nspr.h"
#include "ptypes.h"
#include "datetime.h"
#include "ptrarray.h"
#include "rrday.h"

/**
 * The DateGenerator class defines an interface that provides a basic
 * call structure for a BYxxx tag.
 * DateGenerators are combined in the context of a Recurrence object 
 * to create more specialized and versatile date recurrences.
 *
 * @see              DateGenerator
 */
class DateGenerator
{
public:
    

    /**
     * Virtual Destructor.
     */
    virtual ~DateGenerator() {}
   
    /**
     * Returns the interval for which the generator spans. 
     * (i.e. a year, a month, a week, etc) 
     *
     * @return          the generator's interval
     */
    t_int32 getSpan() const { return m_iSpan; }

    /**
     * Sets the generation interval.
     * (i.e. minutely, hourly, daily, weekly, monthly, yearly)
     * @param           span  span value to set to. 
     */
    void setSpan(t_int32 span) { m_iSpan = span; }
    
    /*---------------------------------------------------------------------*/
    /* --- DateGenerator interface --- */

    /**
     * Pure virtual.  Returns the interval for which the generator represents.
     * @return          the interval (DAILY)
     */
    virtual t_int32 getInterval() const { PR_ASSERT(FALSE); return -1;}
    
    /**
     * Pure virtual.  Fills in dateVector with DateTime objects that correspond to the
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
    virtual t_bool generate(DateTime * startDate,
        JulianPtrArray & dateVector, DateTime * until) 
    { 
        /* PR_ASSERT(FALSE); */
        if (startDate) {}
        if (until) {}
        if (dateVector.GetSize() == 0) {}
        return FALSE; 
    }
     
    /**
     * Pure virtual. A general method to set a single parameter.
     * Usually sets the week start parameter.
     * @param           param   the week start value
     */
    virtual void setParams(t_int32 param) 
    { 
        if (param == 0);
    }
     
    /**
     * Pure virtual.  A general method to set a list of parameters.
     * @param           params[]    a list of parameters
     * @param           length      the length of params[]
     *
     */
    virtual void setParamsArray(t_int32 params[], t_int32 length) 
    { 
        if (params != 0) {}
        if (length == 0) {}
    }
     
    /**
     * A general method to set a list of lists of parameters for a <code>DateGenerator</code>.
     */
    virtual void setRDay(RRDay params[], t_int32 length)
    {
        /* PR_ASSERT(FALSE); */
        if (params != 0) {}
        if (length == 0) {}
    }

    /**
     * Pure virtual.  Determines if a generator has been set active.
     * Usually tests if parameters have been set or not.
     *
     * @return          TRUE if generator is active, FALSE otherwise.
     */
    virtual t_bool active() { PR_ASSERT(FALSE); return FALSE; }

    /* --- End of DateGenerator interface --- */
    /*---------------------------------------------------------------------*/

protected:

    /**
     * Constructor sets span, parameters length. 
     * @param           span        initial span value
     * @param           paramsLen   initial parameters length value
     */
    DateGenerator(t_int32 span, t_int32 paramsLen = 0);
    
    /** span value */
    t_int32 m_iSpan;

    /** parameters length */
    t_int32 m_iParamsLen;

private:

    /** hide default constructor */
    DateGenerator();

};

#endif  /* __DATEGENERATOR_H_ */


