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
 * duration.h
 * John Sun
 * 1/28/98 5:40:22 PM
 */

#ifndef __DURATION_H_
#define __DURATION_H_

#include <ptypes.h>
#include <unistring.h>

/**
 * The Duration class implements the "duration" data type.
 * The "duration" data type is used to identify properties that contain
 * a duration of time. The format is expressed as the [ISO 8601] basic
 * format for the duration of time. The format can represent durations
 * in terms of years, months, days, hours, minutes, and seconds. The
 * data type is defined by the following notation:
 *                                                              
 *   DIGIT      =<any ASCII decimal digit>      ;0-9
 *
 *   dur-second = 1*DIGIT "S"
 *   dur-minute = 1*DIGIT "M" [dur-second]
 *   dur-hour   = 1*DIGIT "H" [dur-minute]
 *   dur-time   = "T" (dur-hour / dur-minute / dur-second)
 *
 *   dur-week   = 1*DIGIT "W"
 *   dur-day    = 1*DIGIT "D"
 *   dur-month  = 1*DIGIT "M" [dur-day]
 *   dur-year   = 1*DIGIT "Y" [dur-month]
 *   dur-date   = (dur-day / dur-month / dur-year) [dur-time]
 *
 *   duration   = "P" (dur-date / dur-time / dur-week)</pre>
 * For example, a duration of 10 years, 3 months, 15 days, 5 hours, 30
 * minutes and 20 seconds would be:
 *                                                  
 *   P10Y3M15DT5H30M20S
 *                                                  
 * The Duration class models the duration datatype specified in the iCalendar document.
 * Assertion: if week value is not equal is zero, then all other values must be zero
 *
 */
class Duration 
{
private:

    /*-----------------------------
    ** MEMBERS
    **---------------------------*/

    /** duration year value */ 
    t_int32 m_iYear;
    /** duration month value */ 
    t_int32 m_iMonth;
    /** duration day value */ 
    t_int32 m_iDay;
    /** duration hour value */ 
    t_int32 m_iHour;
    /** duration minute value */ 
    t_int32 m_iMinute;
    /** duration second value */ 
    t_int32 m_iSecond;

    /** duration week value - NOTE
     * if week != 0 then ALL other members = 0 */ 
    t_int32 m_iWeek;

    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
   
    /**
     * initialize the member variables 
     */
    void init();    

    /**
     * sets values to an invalid duration.  This
     * internally sets all values to -1.
     */
    void setInvalidDuration();

    /**
     * parse an iCal Duration string and populate the data members
     *
     * @param   us  the iCal Duraton string
     */
    void parse(UnicodeString & us);

public:
      
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    
    /**
     * default constrctor, set duration to 5 min.
     */
    Duration();
    
    /**
     * Constructs a Duration object using the grammar
     * defined in the iCalendar spec.
     *
     * @param s String to parse
     */
    Duration(UnicodeString & us);
     
    /**
     * Constructs a copy of a Duration object.
     *
     * @param   d   Duration to copy
     */
    Duration(Duration & aDuration);
    
    /**
     * Constructs a Duration object with a single value set.
     *
     * @param type the field to set (uses <code>Recurrence</code> type constants)
     * @param value value to assign the field
     */
    Duration(t_int32 type, t_int32 value);
    
    /**
     * Constructs a Duration object with a single value set.
     *
     * @param   year    initial year value
     * @param   month   initial month value
     * @param   day     initial day value
     * @param   hour    initial hour value
     * @param   min     initial min value
     * @param   sec     intiial sec value
     * @param   week    intiial week value
     */
    Duration(t_int32 year, t_int32 month, t_int32 day, t_int32 hour, t_int32 min,
        t_int32 sec, t_int32 week);

    /** 
     * Destructor.
     */
    ~Duration();

    /*-----------------------------
    ** GETTERS and SETTERS
    **---------------------------*/
    
    /**
     * Set year value. 
     * @param           i   new year value
     */
    void setYear(t_int32 i) { m_iYear = i; }
    
    /**
     * Set year value. 
     * @param           i   new year value
     */
    void setMonth(t_int32 i) { m_iMonth = i; }
    
    /**
     * Set month value. 
     * @param           i   new month value
     */
    void setDay(t_int32 i) { m_iDay = i; }
    
    /**
     * Set day value. 
     * @param           i   new day value
     */
    void setHour(t_int32 i) { m_iHour = i; }
    
    /**
     * Set minute value. 
     * @param           i   new minute value
     */
    void setMinute(t_int32 i) { m_iMinute = i; }
    
    /**
     * Set second value. 
     * @param           i   new second value
     */
    void setSecond(t_int32 i) { m_iSecond = i; }
    
    /**
     * Set week value. 
     * @param           i   new week value
     */
    void setWeek(t_int32 i) { m_iWeek = i; }
    

    /**
     * Sets all duration member values except week.
     * @param           year    new year value
     * @param           month   new month value
     * @param           day     new day value
     * @param           hour    new hour value
     * @param           min     new minute value
     * @param           sec     new second value
     */
    void set(t_int32 year, t_int32 month, t_int32 day, t_int32 hour,
        t_int32 min, t_int32 sec);


    /**
     * Returns current year value
     *
     * @return          current year value 
     */
    t_int32 getYear() const   { return m_iYear; }
    
    /**
     * Returns current month value
     *
     * @return          current month value 
     */
    t_int32 getMonth() const  { return m_iMonth; }
    
    /**
     * Returns current day value
     *
     * @return          current day value 
     */
    t_int32 getDay() const    { return m_iDay; }
    
    /**
     * Returns current hour value
     *
     * @return          current hour value 
     */
    t_int32 getHour() const   { return m_iHour; }
    
    /**
     * Returns current minute value
     *
     * @return          current minute value 
     */
    t_int32 getMinute() const { return m_iMinute; }
    
    /**
     * Returns current second value
     *
     * @return          current second value 
     */
    t_int32 getSecond() const { return m_iSecond; }
    
    /**
     * Returns current week value
     *
     * @return          current week value 
     */
    t_int32 getWeek() const   { return m_iWeek; }

    /*-----------------------------
    ** UTILITIES
    **---------------------------*/

    /**
     * Sets duration members by parsing string input.
     * @param           s   string to parse
     */
    void setDurationString(UnicodeString & s);

    /**
     * Comparision method. 
     * @param           d   Duration to compare to.
     *
     * @return  -1 if this is shorter, 0 if equal, 1 if longer 
     *           length of duration
     */
    t_int32 compareTo(const Duration &d);

    /**
     * Normalizes the current duration.  This means rounding off
     * values.  For example, a duration with values 13 months,
     * is normalized to 1 year, 1 month.
     */
    void normalize();

    /**
     * Returns TRUE if duration lasts no length of time, otherwise
     * returns FALSE
     *
     * @return          TRUE if duration takes no time, FALSE otherwise
     */
    t_bool isZeroLength();

    /**
     * returns this Duration object to a UnicodeString
     * 
     * @return  a UnicodeString representing the human-readable format of this Duration
     */
    UnicodeString toString();

    /**
     * returns this Duration object to a UnicodeString
     * 
     * @return  a UnicodeString representing the iCal format of this Duration
     */
    UnicodeString toICALString();

    /**
     *  Check whether this duration is valid.  A invalid duration will
     *  have a value of -1 for at least one data member.
     *
     *  @return TRUE if duration is valid, FALSE otherwise
     */
    t_bool isValid();

    /*-----------------------------
    ** OVERLOADED OPERATORS
    **---------------------------*/

    /**
     * assignment operator
     *
     * @param   d       Duration to copy
     * @return          a copy of that Duration
     */
    const Duration &operator=(const Duration & d); 

    /**
     * (==) equality operator
     *
     * @param   d       Duration to copy
     * @return          TRUE if this is equal to d, otherwise FALSE
     */
    t_bool operator==(const Duration & that); 

    /**
     * (!=) in-equality operator
     *
     * @param   d       Duration to copy
     * @return          TRUE if this NOT equal to d, otherwise FALSE
     */
    t_bool operator!=(const Duration & that); 
     
    /*-- TODO - make normalize work so that >, < will work correctly */

    /**
     * (>) greater than operator
     *
     * @param   d       Duration to copy
     * @return          TRUE if this is longer duration than d, otherwise FALSE
     */
    t_bool operator>(const Duration & that); 

    /**
     * (<) less than operator
     *
     * @param   d       Duration to copy
     * @return          TRUE if this is shorter duration than d, otherwise FALSE
     */
    t_bool operator<(const Duration & that); 
};

#endif /* __DURATION_H_ */

