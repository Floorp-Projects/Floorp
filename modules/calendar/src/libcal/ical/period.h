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
 * period.h
 * John Sun
 * 1/29/98 11:16:33 AM
 */

#ifndef __PERIOD_H_
#define __PERIOD_H_

#include "datetime.h"
#include "duration.h"
#include "ptrarray.h"

class Period
{
private:
    
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    
    /** the starting time of the period */
    DateTime m_DTStart;

    /** the ending time of the period */
    DateTime m_DTEnd;
    
    /** the duration of the period */
    Duration m_Duration;

    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    
    /**
     * Parses an iCal Period string and
     * populates date members.
     * @param           us  iCal Period string
     */
    void parse(UnicodeString & us);
    
    /** checks period's data members */
    void selfCheck();
    
    /** create invalid period */
    void createInvalidPeriod();


    /**
     * Helper method to get a part of period string.
     * If bStart is TRUE, get the start string part,
     * else get end string part.
     * @param           sPeriod     period string
     * @param           bStart      start or end
     * @param           out         output start or end string
     *
     * @return          output start or end string (out)
     */
    static UnicodeString & getTimeString(UnicodeString & sPeriod, 
        t_bool bStart, UnicodeString & out);
public:

    /**
     * Retuns ending time regardless if dtend is invalid.
     * If DTEnd is invalid, will then add duration to start to
     * get ending time.
     * @param           d   output ending datetime
     *
     * @return          ouptu ending datetime (d)
     */
    DateTime & getEndingTime(DateTime & d);

    /**
     * Constructor.  Creates invalid period.
     */
    Period();

    /**
     * Copy constructor.
     * @param           that    period to copy
     */
    Period(Period & that);

    /**
     * Constructor.  Creates period by parsing string. 
     * @param           us      string to parse
     */
    Period(UnicodeString & us);

    /**
     * Destructor.
     */
    ~Period();

    /**
     * Return start time.
     *
     * @return          DateTime 
     */
    DateTime getStart() { return m_DTStart; }

    /**
     * Return dtend (may be invalid).
     *
     * @return          DateTime 
     */
    DateTime getEnd() { return m_DTEnd; }

    /**
     * Return duration (may be invalid).
     *
     * @return          Duration 
     */
    Duration getDuration() { return m_Duration; }

    /**
     * Set start time
     * @param           d   new start value
     */
    void setStart(DateTime d); 
    
    /**
     * Set end time
     * @param           d   new end value
     */
    void setEnd(DateTime d); 
    
    /**
     * Set duration
     * @param           d   new duration value
     */
    void setDuration(Duration d); 
    
    /**
     * Set period by passing string.
     * @param           us   string to parse.
     */
    void setPeriodString(UnicodeString & us);

    /* UTILITIES */

    /**
     * Return if this period is valid.  A valid period must have a 
     * valid start datetime and a valid end datetime or a valid duration time.
     * and the end time must be after the start time.
     *
     * @return      TRUE if valid, FALSE otherwise
     */
    t_bool isValid();

    /**
     * Return if this period intersects start, end range
     * @param           start   start of target range
     * @param           end     end of target range
     *
     * @return          TRUE if in range, FALSE otherwise
     */
    t_bool isIntersecting(DateTime & start, DateTime & end);

    /**
     * Return if this period intersects period range
     * @param           p  target range
     *
     * @return          TRUE if in range, FALSE otherwise
     */
    t_bool isIntersecting(Period & p);

    /**
     * Return if this period is completely inside start, end range
     * @param           start   start of target range
     * @param           end     end of target range
     *
     * @return          TRUE if completely inside range, FALSE otherwise
     */
    t_bool isInside(DateTime & start, DateTime & end);
    
    /**
     * Return if this period is completely inside period range
     * @param           p  target range
     *
     * @return          TRUE if in range, FALSE otherwise
     */
    t_bool isInside(Period & p);

    /**
     * Returns period in human readable format of "start - end".
     *
     * @return          output string
     */
    UnicodeString toString(); 

    /**
     * Returns period in iCal period format of "start/end", where
     * start is UTC ISO8601 datetime format and end is either
     * a UTC IS08601 datetime format or an iCal Duration string.
     *
     * @return          output string
     */
    UnicodeString toICALString(); 

    /* STATIC METHODS */

    /**
     * Checks whether periods are connected.  Connected means that the 
     * two periods combined form one block of time.  Returns TRUE if 
     * p1 and p2 are connected, FALSE otherwise.
     * @param           p1      first period 
     * @param           p2      second period
     *
     * @return          TRUE if p1,p2 are connected, FALSE otherwise
     */
    static t_bool IsConnectedPeriods(Period & p1, Period & p2);

    /**
     * Returns the union of two periods in outPer, if possible.  
     * If the union of the two-periods is not possible in one period 
     * (i.e. inPer1, inPer2 are disjoint) then return an invalid period.
     * @param           inPer1      first period
     * @param           inPer2      second period
     * @param           outPer      return union of inPer1, inPer2. 
     *                              Invalid if inPer1,inPer are disjoint
     */
    static void Union(Period & inPer1, Period & inPer2, Period & outPer);

    /**
     * Returns the intersection of this period with the range 
     * (start,end) into outPer if possible.  If the 
     * intersection is null (i.e. inPer1, [start, end) are disjoint) 
     * then return an invalid period in outPer.
     * @param           iPer1   a period
     * @param           start   start of range
     * @param           end     end of range
     * @param           outPer  output intersection of iPer1 with (start,end) range
     *
     */
    static void Intersection(Period & iPer1, DateTime & start, DateTime & end, Period & outPer);

    /**
     * Fill in vector out with periods inside inPer that are outside of 
     * (start, end) range.  This simulates logical subtraction of periods
     * @param           inPer   a period
     * @param           start   start of range
     * @param           end     end of range
     * @param           out     output vector of periods inside inPer but outside (start,end)
     *
     */
    static void SubtractedPeriod(Period & inPer, DateTime & start, DateTime & end, JulianPtrArray * out);

    /**
     * Checks to see if u is a parseable iCal Period string.  Return TRUE
     * if parseable, FALSE otherwise.
     * @param           u   target period string
     *
     * @return          TRUE if parseable iCal Period, FALSE otherwise.
     */
    static t_bool IsParseable(UnicodeString & u); 


    /**
     * Gets starting time string from sPeriod.
     * @param           sPeriod     input period string
     * @param           out         output starting time string
     *
     * @return          output starting time string (out)  
     */
    static UnicodeString & getStartTimeString(UnicodeString & sPeriod, UnicodeString & out);
    
    /**
     * Gets ending time string from sPeriod.  Ending time maybe a 
     * duration string or a datetime string
     * @param           sPeriod     input period string
     * @param           out         output ending time string
     *
     * @return          output ending time string (out)  
     */
    static UnicodeString & getEndTimeString(UnicodeString & sPeriod, UnicodeString & out);
    
    /**
     * Compares periods by their starting time.  Used
     * to pass into JulianPtrArray::QuickSort method.  
     * @param           a   ptr to first period 
     * @param           b   ptr to second period
     *
     * @return          compareTo(a.start, b.start)
     */
    static int ComparePeriods(const void * a, const void * b);
    
    /**
     * Compares periods by their ending time.  Used
     * to pass into JulianPtrArray::QuickSort method.
     * @param           a   ptr to first period
     * @param           b   ptr to second period
     *
     * @return          compareTo(a.getEndingTime(), b.getEndingTime())
     */
    static int ComparePeriodsByEndTime(const void * a, const void * b);
};

#endif /* __PERIOD_H_ */


