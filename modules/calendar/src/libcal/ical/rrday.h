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
 * rrday.h
 * John Sun
 * 2/4/98 7:54:41 PM
 */

#ifndef __RRDAY_H_
#define __RRDAY_H_

#include <assert.h>
#include <ptypes.h>

/**
 * RRDay encapsulates a day for recurrence handling
 * fields[0] is the value of the day (i.e Calendar::SUNDAY, ...
 * Calendar::SATURDAY).  fields[1] is the number of the day.
 * Thus if this is the 1st Sunday, then fields[1] = 1;
 * If this is the last Sunday, then fields[1] = -1;
 */
class RRDay
{
public:
    /** the fields */
    t_int32 fields[2];

    /** Default constructor */
    RRDay();

    /** Destructor */
    ~RRDay() {}


    /**
     * Constructor.  Sets fields to aDay, aModifier
     * @param           aDay        day of week value
     * @param           aModifier   modifier
     */
    RRDay(t_int32 aDay, t_int32 aModifier);


    /**
     * Return index into fields.
     * @param           index   index of field to get
     *
     * @return          value of fields[index]
     */
    t_int32 operator[](t_int32 index) const;


    /**
     * Assignment operator.
     * @param           that    RRDay to assign to
     */
    void operator=(RRDay & that);


    /**
     * Equality operator.
     * @param           that    RRDay to compare to
     *
     * @return          TRUE if equal, FALSE otherwise.
     */
    t_bool operator==(RRDay & that);

    /**
     * Inequality operator.
     * @param           that    RRDay to compare to
     *
     * @return          TRUE if NOT equal, FALSE otherwise.
     */
    t_bool operator!=(RRDay & that);

};

#endif /* __RRDAY_H_ */

