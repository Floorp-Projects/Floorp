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
 * deftgntr.h
 * John Sun
 * 2/3/98 3:46:49 PM
 */

#ifndef __DEFAULTGENERATOR_H_
#define __DEFAULTGENERATOR_H_

#include "dategntr.h"

/**
 * The DefaultGenerator class generates dates for rules w/o BYxxx tags.
 *
 * @see              DateGenerator
 */
class DefaultGenerator : public DateGenerator
{
public:
    
    /**
     * Default Constructor
     */
    DefaultGenerator();
    
    /**
     * Destructor
     */
    ~DefaultGenerator() {}

    /*---------------------------------------------------------------------*/
    /* --- DateGenerator interface --- */

    /**
     * Returns the interval for which the generator represents.
     * @return          the interval (MONTHLY)
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
    virtual t_bool generate(DateTime * start, JulianPtrArray & dateVector, DateTime * until);
    
    /**
     * Sets the week start parameter.
     * @param           param   the week start value
     */
    virtual void setParams(t_int32 param) { m_iSpan = m_aiParams[2] = param; }
    
    /**
     * Sets the span, count, interval parameters. TODO: move to the .cpp file.
     * @param           params[]    the list of parameters to set to
     * @param           length      the length of params[]
     */
    virtual void setParamsArray(t_int32 params[], t_int32 length) 
    { 
        isActive = TRUE;
        t_int32 i;
        for (i = 0; i < length; i++)
            m_aiParams[i] = params[i];
    }
    /**
     * Determines if a generator has been set active.
     *
     * @return          TRUE if isActive in On, FALSE otherwise
     */
    virtual t_bool active() { return isActive; }

    /* --- End of DateGenerator interface --- */
    /*---------------------------------------------------------------------*/

private:
	
    /** whether generator's parameters have been set by client */
    t_bool isActive;

    /** contains span, interval, count as parameters */
    t_int32 m_aiParams[3];
};

#endif /* __DEFAULTGENERATOR_H_ */

