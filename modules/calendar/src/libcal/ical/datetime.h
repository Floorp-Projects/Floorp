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
 * datetime.h
 * John Sun
 * 1/22/98 9:24:15 AM
 */

#ifndef __DATETIME_H_
#define __DATETIME_H_

#include <ptypes.h>
#include <locid.h>
#include <hashtab.h>
#include <timezone.h>
#include <smpdtfmt.h>
#include <calendar.h>
#include <gregocal.h>
#include <datefmt.h>

#include "duration.h"

/*
 * Helper functions for use with JulianPtrArray
 */
int DateTime_SortAscending( const void* v1, const void* v2 );
int DateTime_SortDescending( const void* v1, const void* v2 );

class JULIAN_PUBLIC DateTime
{
private:
    
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/

#if 0
    /* date formatting strings.  Moved to JulianFormatString class */
    static UnicodeString ms_asPatterns[];
    static UnicodeString ms_sISO8601Pattern;
    static UnicodeString ms_sISO8601LocalPattern;
    static UnicodeString ms_sISO8601TimeOnlyPattern;
    static UnicodeString ms_sISO8601DateOnlyPattern;
    static UnicodeString ms_sDefaultPattern;

    /* hashtable of simpleDateFormats vars and methods */
    /* TODO: use for caching simpledateformats in other locales */
    static SimpleDateFormat kDateFormats[];
    static char* kDateFormatIDs[];
    static const t_int32 kNumDateFormats;
    static Hashtable sdfHashtable; /* hashtable of date formats */
    void initHashtable();

    /* old data structures, used when static */
     /*TimeZone * m_TimeZone;*/     
    
    static Locale * ms_Locale;
    static Locale ms_Locale;
    static ResourceBundle * ms_ResourceBundle;
    static GregorianCalendar ms_Calendar;
    static DateTime * invalidDate;    /* singleton instance of an invalid Date */
    static DateFormat ms_GMTISO8601DateFormat;
    static SimpleDateFormat * ms_DateFormat; 
#endif

    static ErrorCode staticStatus;
    static ErrorCode staticStatus2;
    static TimeZone * m_kGMTTimeZone;  /* was const */
    static TimeZone * m_kDefaultTimeZone; /* was const */
    static TimeZone * ms_TimeZone;

    static GregorianCalendar * ms_Calendar;   
    static SimpleDateFormat * ms_DateFormat;

    static t_bool m_Init;
    Date m_Date; /* like the long in java.util.Date */

    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/

    t_int8 compare(Date d);
    
    /**
     * Checks to see if static member data was initialized correctly.  May remove later.
     */
    static void CtorCore();
    
    /**
     * helper method that encapsulates errorcode checking
     * while calling a get-field method
     *
     * @param    aField     a calendar field to get
     * @param    timezone   timezone returned field should be relevant to 
     * @return the value of that field
     */
    static t_int32 CalendarGetFieldHelper(Calendar::EDateFields aField,
        TimeZone * timezone = ms_TimeZone); 
   
    /**
     * helper method that encapsulates errorcode checking
     * while calling a getTime()
     * (like calling ms_Calendar->getTime(ErrorCode);)
     *
     * @return the Date value of ms_Calendar
     */
    static Date CalendarGetTimeHelper(); 
    /**
     * helper method that encapsulates errorcode checking
     * while calling a setTime()
     * (like calling ms_Calendar->setTime(Date, ErrorCode);)
     *
     * @param d the Date value to set to ms_Calendar
     */
    static void CalendarSetTimeHelper(Date d); 

                                               
    /**
     * parse the date string to get the Date value of the string
     *
     * @param s         the UnicodeString to parse
     * @param locale    the Locale to parse with
     * @return          the Date value of the string
     */
    static Date parse(UnicodeString & us, Locale & locale);
    
    /**
     * parse the date string to get the Date value of the string relative to a
     * a date-format pattern.
     * NOTE: Slow since creates new SimpleDateFormat
     *
     * @param pattern         the date-format pattern to parse with
     * @param s               the UnicodeString to parse
     * @param locale          the Locale to parse with
     * @param timezone        the TimeZone of the time string, default to ms_TimeZone
     * @return                the Date value of the string
     */
    static Date parseTime(UnicodeString & pattern, UnicodeString & time, 
        Locale & locale, TimeZone * timezone = ms_TimeZone);
   
    /**
     * parse the date string to get the Date value of the string relative to a
     * a date-format pattern.
     *
     * @param pattern         the date-format pattern to parse with
     * @param s               the UnicodeString to parse
     * @param timezone        the TimeZone of the time string, default to ms_TimeZone
     * @return                the Date value of the string
     */ 
    static Date parseTime(UnicodeString & pattern, UnicodeString & time,
        TimeZone * timezone = ms_TimeZone); /* no locale */
    /*static UnicodeString stringCheck(UnicodeString uc);*/

public:
    /**
     * correctly returns the number of days in a month, given
     * the month and a year.  The rule for this method is the following:
     * Assume rule is Gregorian (pretend all dates after 1582).  Use leap-year
     * rule every 4 years, except every 100 years, except every 400 years.
     * Thus 1700, 1800, 1900, 2100, 2200, 2300 are not leap years.
     * But 2000, 1600, 2400 are leap years.  Of course, year divisible by 4,
     * but not divisible by 100 are leap years.
     *
     * @param iMonthNum         the 1-based month value
     * @param iYearNum          the year value (i.e. 1998, 1989, 2001)
     * @return                  the number of days in that month
     */
    static t_int32 getMonthLength(t_int32 iMonthNum, t_int32 iYearNum);

private:

    /**
     * Given an ISO8601 Date string, parse it to return the Date value.
     * Allows for passing in timezones.  Thus if string is 19971225T112233
     * and TimeZone is PST, then returned date value will be equal to
     * the number of milleseconds since the epoch (Jan 1, 1970 12:00 AM GMT)
     * on Dec 25, 1997 11:22:33 AM PST.
     * If the string is a UTC string (ends with a Z), the timezone is ignored
     * and the string is parsed relevant to the GMT timezone.
     *
     * Since the Calendar cannot recognize dates, before the epoch, 
     * parseable DateTime strings with year value before 1970 will be set to 1970
     * Thus a string 19671225T112233 will be changed to 19701225T112233.
     * This allows for parsing timezones with a start time before 1970.
     * Return -1 if the Date string is bogus. (i.e. bad parse, impossible values)
     * Also parses Dates, i.e. 19971110.  Date values will have hours, minutes,
     * and seconds set to 0 in with respect to the timezone passed in.  
     * 
     *
     * @param us                the UnicodeString to parse
     * @param timezone          the TimeZone of the datetime is related to
     * @return                  the Date value of the string
     * @see IsParseable
     */
    static Date parseISO8601(UnicodeString & us,
        TimeZone * timezone = ms_TimeZone);


    /**
     * The format workhorse method.  Calls the SimpleDateformat 
     * object to format pattern with respect to timezone
     * @param           pattern     pattern to format datetime to
     * @param           aTimeZone   timezone of datetime to format to
     *
     * @return          UnicodeString 
     */
    UnicodeString strftimeNoLocale(UnicodeString& pattern, 
        TimeZone * aTimeZone = ms_TimeZone);
public:

    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    
    /**
     * default constructor - set the time to current time
     */
    DateTime();
    
    /**
     * 3 element args constructor - given year, month, and date,
     * construct a datetime
     *
     * @param year          the initial year of the datetime
     * @param month         the initial month of the datetime
     * @param date          the initial date of the datetime
     */
    DateTime(t_int32 year, t_int32 month, t_int32 date);
  
    /**
     * 5 element args constructor - given year, month, date,
     * hrs, and mins, construct a datetime
     *
     * @param year          the initial year of the datetime
     * @param month         the initial month of the datetime
     * @param date          the initial date of the datetime
     * @param hrs           the initial hours of the datetime
     * @param min           the initial minutes of the datetime
     */
    DateTime(t_int32 year, t_int32 month, t_int32 date, t_int32 hrs, t_int32 min);
  
    /**
     * 6 element args constructor - given year, month, date,
     * hrs, and mins, construct a datetime
     *
     * @param year          the initial year of the datetime
     * @param month         the initial month of the datetime
     * @param date          the initial date of the datetime
     * @param hrs           the initial hours of the datetime
     * @param min           the initial minutes of the datetime
     * @param sec           the initial seconds of the datetime
     */
    DateTime(t_int32 year, t_int32 month, t_int32 date, t_int32 hrs, t_int32 min, t_int32 sec);

    /**
     * constructor that sets date value to Date arg
     * 
     * @param d             the initial Date value of the datetime
     */
    DateTime(Date d);
    
    /**
     * copy constructor
     * @param aDateTime     the DateTime to copy
     */
    DateTime(DateTime & aDateTime);
 
    /**
     * a constructor that takes a string and converts to datetime.
     * ASSUMPTION: us is a ISO8601 Date-Time Format
     * 
     * @param us            the UnicodeString to parse, setting Date value
     * @param timezone      the TimeZone to parse in
     */  
    DateTime(UnicodeString & us, TimeZone * timezone = ms_TimeZone);

	~DateTime();

    /*-----------------------------
    ** GETTERS and SETTERS and ADDERS
    **---------------------------*/

    /**
     * Returns the value of the Date 
     *
     * @return          the current Date value of this DateTime object
     */
    Date getTime() const;
    
    /**
     * Sets the value of the Date 
     *
     * @param   d       the Date arg to set to 
     */
    void setTime(Date d);

    /**
     * Given a string, sets time to string's value if parseable
     * NOTE: Currently only accepts ISO8601 strings.
     * @param    us         date-time string to set to
     * @param    timezone   timezone the date-time should be parsed to 
     * @see parseISO8601
     */
    void setTimeString(UnicodeString & us, 
        TimeZone * timezone = ms_TimeZone);
    
    /**
     * Set the value of the timezone
     *
     * @param   t       the TimeZone arg to set to
     */
    /*void setTimeZone(TimeZone * t);*/

    /**
     * Returns a field of the calendar
     *
     * @param   aField      the field to return
     * @param    timezone   timezone returned field should be relevant to 
     * @return              the value of the field
     *
     */
    t_int32 get(Calendar::EDateFields aField, 
        TimeZone * timezone = ms_TimeZone);
    
    /**
     * @param    timezone   timezone returned field should be relevant to 
     * @return  the year field value
     */
    t_int32 getYear(TimeZone * timezone = ms_TimeZone);
    
    /**
     * @param    timezone   timezone returned field should be relevant to 
     * @return  the month field value
     */
    t_int32 getMonth(TimeZone * timezone = ms_TimeZone);
    
    /**
     * @param    timezone   timezone returned field should be relevant to 
     * @return  the day-of-month field value
     */
    t_int32 getDate(TimeZone * timezone = ms_TimeZone);
    
    /**
     * @param    timezone   timezone returned field should be relevant to 
     * @return  the hour-of-day(24hrs) field value
     */
    t_int32 getHour(TimeZone * timezone = ms_TimeZone);
    
    /**
     * @param    timezone   timezone returned field should be relevant to 
     * @return  the minute field value
     */
    t_int32 getMinute(TimeZone * timezone = ms_TimeZone);
    
    /**
     * @param    timezone   timezone returned field should be relevant to 
     * @return  the second field value
     */
    t_int32 getSecond(TimeZone * timezone = ms_TimeZone);
    
    /**
     * @param    timezone   timezone returned field should be relevant to 
     * @return  the day-of-week-in-month field value
     */
    t_int32 getDayOfWeekInMonth(TimeZone * timezone = ms_TimeZone);

    /**
     * Set the day month and year of d2 to the supplied values
     *
     *  @param iDay   the desired day of the month
     *  @param iMonth the desired month of the year
     *  @param iYear  the desired year
     *  @return       the updated <code>DateTime</code>
     */
    void setDMY(t_int32 iDay, t_int32 iMonth, t_int32 iYear);
    void setDayOfMonth(t_int32 iDay);

    /**
     * Sets the value of a calendar field to an amount
     *
     * @param   aField      the field to set 
     * @param   amount      the amount of the field
     */
    void set(Calendar::EDateFields, t_int32 value);
    /**
     * Adds the an amount to the value of a calendar field
     *
     * @param   aField      the field to add 
     * @param   amount      the amount of the field
     */
    void add(Calendar::EDateFields, t_int32 amount);

    /**
     * adds a duration amount to this datetime
     * @param    d       the duration to add
     */
    void add(Duration d);

    /**
     * subtracts a duration amount from this datetime
     * @param    d       the duration to subtract
     */
    void subtract(Duration d);

    /**
     * given two datetimes, return the duration length between them.
     * if end before start, return an invalid duration.
     */
    static Duration & getDuration(DateTime start, DateTime end, Duration & out);

    /**
     * Clears all fields
     */
    void clear();
    
    /*-----------------------------
    ** UTILITIES
    **---------------------------*/

    /**
     * A invalid DateTime has a Date value less than 0.
     *
     * @return TRUE if valid, FALSE otherwise.
     */
    t_bool isValid();

    /**
     * Sets this DateTime to the previous day
     */
    void prevDay();
    
    /**
     * Sets this DateTime to a previous number of days
     * @param   i   number of days to go back
     */
    void prevDay(t_int32 i);
    
    /**
     * Sets this DateTime to the next day
     */
    void nextDay();
    
    /**
     * Sets this DateTime ahead a number of days
     * @param   i   number of days to go ahead
     */
    void nextDay(t_int32 i);
   
    /**
     * Sets this DateTime to the prev week
     */
    void prevWeek();
    
    /**
     * Sets this DateTime to the next week
     */
    void nextWeek();

    /* moves by week */
    void moveTo(t_int32 i, t_int32 direction, t_bool force);

    void setByISOWeek(t_int32 week);
    /**
     * Sets this DateTime to the previous month
     */
    void prevMonth();
    
    /**
     * Sets this DateTime to a previous number of months
     * @param   iLim    number of months to go back
     */
    void prevMonth(t_int32 iLim);
    
    /**
     * Sets this DateTime to the next month
     */
    void nextMonth();
    
    /**
     * Sets this DateTime to advance a number of months
     * @param   iLim    number of months to advance
     */
    void nextMonth(t_int32 iLim);
    /**---------------------------------------------------------------------
    **  prevDOW
    **
    **  This method is useful in determining the beginning of a calendar
    **  week.  The idea is: you pass in the day of the week the user
    **  wants as the first day of the week and this routine finds it.
    **  if the date passed in is already the desired day, the same date
    **  is returned.  Otherwise, it goes backwards until it finds the
    **  desired day of the week.
    ***
    *** @param n  back the this date off to the supplied day-of-the-week 
    ***           This parameter must be one of Calendar::SUNDAY through
    ***           Calendar::SATURDAY.
    ***
    *** @return   the number of days backed off
    ***
    *** sman@netscape.com
    ***--------------------------------------------------------------------*/
    t_int32 prevDOW(t_int32 n);

    /* Boolean check methods */
    
    /**
     * @param  dt    DateTime to compare with
     * @return -1 if this DateTime is before, 0 if equal, 1 if afte
     */
    t_int32 compareTo(DateTime &dt);
    
    /**
     * @param  d    Date to compare with
     * @return TRUE if this DateTime is before a Date, FALSE otherwise
     * @deprecated
     */
    t_bool beforeDate(Date d);
    t_bool beforeDateTime(DateTime dt);
    t_bool before(DateTime * dt);
        
    /**
     * @param  d    Date to compare with
     * @return TRUE if this DateTime is after a Date, FALSE otherwise
     * @deprecated
     */
    t_bool afterDate(Date d);
    t_bool afterDateTime(DateTime dt);
    t_bool after(DateTime * dt);

    /**
     * @param  d    Date to compare with
     * @return TRUE if this DateTime is equals a Date, FALSE otherwise
     * @deprecated
     */
    t_bool equalsDate(Date d);
    t_bool equalsDateTime(DateTime dt);
    t_bool equals(DateTime * dt);

    /**
     * This method returns TRUE if the supplied date is on the same
     * day, month, and year as this date.
     *
     * @param           d           datetime to compare against
     * @param           timezone    timezone that d is in
     * @return          TRUE if day, month and year fields in d and this DateTime match, FALSE otherwise
     */
    t_bool sameDMY(DateTime * d, TimeZone * timezone = ms_TimeZone);

    /* PRINT methods */
    
    /**
     * Returns this DateTime to a localized string
     *
     * @param           timezone    timezone to localize to   
     * @return  the localized string of the date
     */ 
    UnicodeString toString(TimeZone * timezone = ms_TimeZone);
    char * DEBUG_toString();
    
    /**
     * Returns this DateTime to a the GMT ISO8601 format
     *
     * @param           timezone    timezone to localize to   
     * @return  the GMT ISO8601 format of the date
     */ 
    UnicodeString toISO8601Local(TimeZone * timezone = ms_TimeZone);
    char * DEBUG_toISO8601Local();
    
    /**
     * Returns this DateTime to a the localized ISO8601 format
     *
     * @return  the localized ISO8601 format of the date
     */ 
    UnicodeString toISO8601();
    char * DEBUG_toISO8601();

    /**
     * Return the Time string in ISO8601 format w/respect to GMT 
     * For example a DateTime equal to 19981225T013000Z would return 
     * the string "013000".
     *
     * @return          output time string
     */
    UnicodeString toISO8601TimeOnly();

    /**
     * Return the Date string in ISO8601 format w/respect to GMT 
     * For example a DateTime equal to 19981225T013000Z 
     * would return the string "19981225".
     *
     * @return          output date string
     */
    UnicodeString toISO8601DateOnly();

    /**
     * Return the Time string in ISO8601 format w/respect to timezone argument.
     * For example a DateTime equal to 19981225T013000Z, and a timezone arg of EST
     * would return the string "193000" since it is 7:30 PM in Eastern Standard Time.
     * @param           timezone            timezone to apply to.
     */
    UnicodeString toISO8601LocalTimeOnly(TimeZone * timezone = ms_TimeZone);

    /**
     * Return the Time string in ISO8601 format w/respect to timezone argument.
     * For example a DateTime equal to 19981225T013000Z, and a timezone arg of EST
     * would return the string "19981224" since it is Dec 24th in Eastern Standard Time.
     * @param           timezone            timezone to apply to.
     */
    UnicodeString toISO8601LocalDateOnly(TimeZone * timezone = ms_TimeZone);
                                                                             
   /**
     * Returns a string representing the formatted string of the datetime
     * given a formatting string.  Applies result to the timezone passed in.
     * For example, if the datetime was 19981225T013000Z and if
     * pattern was "MM/dd/yy hh:mm:ss a z" and timezone was
     * EST, the returned string would be "12/24/98 07:30:00 PM EST".
     *
     * @param           pattern         the date-format pattern
     * @param           timezone        timezone to apply to
     * @return          the formatted string of this DateTime 
     */
    UnicodeString strftime(UnicodeString& pattern, 
        TimeZone * timezone = ms_TimeZone);
    
    /**
     * Returns a string representing the formatted string of the datetime
     * given a formatting string.  Must create new DateFormat object to 
     * pass in locale
     * NOTE: Slow since creates new SimpleDateFormat
     *
     * @param   pattern     the date-format pattern
     * @param   locale      the locale to print in
     * @param   timezone    the timezone to format in (default to 0)
     * @return              the formatted string of this DateTime 
     */
    UnicodeString strftime(UnicodeString& pattern, Locale & locale, TimeZone * timezone = 0);

    /**
     * Sets this DateTime to the last day of the month of s.
     * Thus if s has a month value of March, then this
     * DateTime is set to March 31.
     *
     * @param   s       a DateTime
     */
    void findLastDayOfMonth(); /* go to last day of current month */

    /**
     * Sets this DateTime to the last day of the year of s.
     * Thus if s has a year value of 1998, then this
     * DateTime is set to Dec 31, 1998.
     *
     * @param   s       a DateTime
     */
    void findLastDayOfYear();  /* go to last day of current year */
 
    /*-----------------------------
    ** STATIC METHODS
    **---------------------------*/

    /**
     *  Given the day of the week indicator, 
     *  return the next day of the week indicator
     *
     *  @param n   the day of the week, for example: Calendar::MONDAY
     *  @return    the next day of the week
     */
    static t_int32 getPrevDOW(t_int32 n);
    
    /**
     *  Given the day of the week indicator, 
     *  return the next day of the week indicator
     *
     *  @param n   the day of the week, for example: Calendar.MONDAY
     *  @return    the next day of the week
     */
    static t_int32 getNextDOW(t_int32 n);

    /**
     * Returns string of dt.
     * @param           DateTime & dt
     *
     * @return          static UnicodeString 
     */
    static UnicodeString DateToString(DateTime & dt);
    
    /**
     * Checks to see is the DateTime is valid
     * (i.e. date not before epoch)
     *
     * @param   d   DateTime to check
     * @return      TRUE if valid DateTime type, FALSE otherwise
     */
    static t_bool IsValidDateTime(DateTime & dt);

    /**
     * Given a string, checks to see whether the string is valid 
     * ISO8601 DateTime or Date string.  
     *
     * A valid ISO8601 DateTime has the following grammar.  
     * [year][month][day]'T'[hour][minute][second]
     * 
     * In addition, the DateTime may have an optional 'Z'
     * character at the end to signify UTC time.
     *
     * A valid ISO8601 Date has the following grammar.
     * [year][month][day]
     * 
     * The year part contains four numbers.
     * The month part contains two numbers comprising a valid month value [01-12].
     * The day part contains two numbers comprising a valid day value [01-31].
     * The day part must also match the month and year values (i.e.
     * cannot have month = 02 and day = 31.) 
     * The hour part contains two numbers comprising a valid hour value [00-23].
     * The minute part contains two numbers comprising a valid minute value [00-59].
     * The second part contains two numbers comprising a valid second value [00-59].
     *
     * If the string is a valid ISO8601 DateTime or Date string, then this method 
     * will correctly fill in the passed in integer variables with their 
     * appropriate value.  For example, if the string was "19971225T123045Z", 
     * the returned values would be iYear = 1997, iMonth = 12, iDay = 25, iHour = 12,
     * iMinute = 30, iSecond = 45.
     * @param           s           string to check if valid
     * @param           iYear       returned year value
     * @param           iMonth      returned month value
     * @param           iDay        returned day value
     * @param           iHour       returned hour value
     * @param           iMinute     returned minute value
     * @param           iSecond     returned second value
     *
     * @return          TRUE if string is a valid ISO8601 DateTime or Date string, FALSE otherwise
     */
    static t_bool IsParseable(UnicodeString & s, t_int32 & iYear, t_int32 & iMonth,
        t_int32 & iDay, t_int32 & iHour, t_int32 & iMinute, t_int32 & iSecond);

    /**
     * Given a string, checks to see whether the string is valid 
     * ISO8601 DateTime or Date string.  
     * @param           s           string to check if valid
     *
     * @return          TRUE if string is a valid ISO8601 DateTime or Date string, FALSE otherwise
     */
    static t_bool IsParseable(UnicodeString & s);
    
    /**
     * Given a string, checks to see whether the string is valid 
     * ISO8601 DateTime string.  
     * @param           s           string to check if valid
     *
     * @return          TRUE if string is a valid ISO8601 DateTime, FALSE otherwise
     */
    static t_bool IsParseableDateTime(UnicodeString & s);
    
    /**
     * Given a string, checks to see whether the string is valid 
     * ISO8601 Date string.  
     * @param           s           string to check if valid
     *
     * @return          TRUE if string is a valid ISO8601 Date, FALSE otherwise
     */
    static t_bool IsParseableDate(UnicodeString & s);
    
    /**
     * Given a string, checks to see whether the string is valid 
     * ISO8601 UTCOffset string.  
     * @param           s           string to check if valid
     *
     * @return          TRUE if string is a valid UTCOffset, FALSE otherwise
     */
    static t_bool IsParseableUTCOffset(UnicodeString & s);


    /**
     * Given a string, checks to see whether the string is valid 
     * ISO8601 UTCOffset string.  
     * 
     * A valid UTCOffset has the following grammar:
     * [+/-][hour][minute]
     * 
     * The first character must be a plus or minus character.
     * The hour part contains two numbers comprising a hour offset [0-12]
     * The minute part contains two number comprising a minute offset [0-59]
     *
     * If the string is a valid UTCOffset, then the methods
     * will correctly fill in the argument integers with their
     * appropriate value.  For
     * @param           s           string to check if valid
     * @param           t_int32 & iHour
     * @param           t_int32 & iMinute
     *
     * @return          static t_bool 
     */
    static t_bool IsParseableUTCOffset(UnicodeString & s, t_int32 & iHour, t_int32 & iMinute);

    /* compare method to pass into JulianPtrArray::QuickSort */
    static int CompareDateTimes(const void * a, const void * b);
 
    /*-----------------------------
    ** OVERLOADED OPERATORS
    **---------------------------*/
    
    /**
     * assignment operator
     *
     * @param   that    DateTime to copy
     * @return          a copy of that DateTime
     */
    const DateTime &operator=(const DateTime & that); 
    
    /**
     * equality operator
     *
     * @param   that    DateTime to copy
     * @return          TRUE if equal, FALSE otherwise
     */
    t_bool operator==(const DateTime & that);
    
    /**
     * inequality operator
     *
     * @param   that    DateTime to copy
     * @return          TRUE if NOT equal, FALSE otherwise
     */
    t_bool operator!=(const DateTime & that);

    /**
     * greater than operator
     *
     * @param   that    DateTime to copy
     * @return          TRUE if after, FALSE otherwise
     */
    t_bool operator>(const DateTime & that);
                                             
    /**
     * less than operator
     *
     * @param   that    DateTime to copy
     * @return          TRUE if befor, FALSE otherwise
     */
    t_bool operator<(const DateTime & that);
};

#endif /* __DATETIME_H_ */

