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

// datetime.cpp
// John Sun
// 2:39 PM Janaury 21 1997

#include "stdafx.h"
#include "jdefines.h"

#include <calendar.h>
#include <gregocal.h>
#include <datefmt.h>
#include <time.h>
#include <smpdtfmt.h>
#include <simpletz.h>
#include "datetime.h"
#include "jutility.h"
#include "duration.h"
#include "keyword.h"
//---------------------------------------------------------------------
//---------------------------
// STATIC INITIALIZATION
//----------------------------
//---------------------------------------------------------------------

/*
 *  This is a hack so I could get it compiled on unix.  We need
 *  to remove Windows specific code from this file.
 *  -sman
 */
//#define TRACE printf

t_bool DateTime::m_Init = TRUE;

// static data used for getMonthLength method.
static t_int32 s_shMonthLength[] = 
{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static t_int32 s_shMonthLengthLeapYear[] = 
{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30 , 31};

/*
UnicodeString DateTime::ms_asPatterns[] = 
{
    UnicodeString("yyyy MM dd HH m s z"),
    UnicodeString("yyyy MM dd HH m s"),
    UnicodeString("EEE MMM dd hh:mm:ss z yyyy"),
    UnicodeString("MMM dd, yyyy h:m a"),
    UnicodeString("MMM dd, yyyy h:m z"),
    UnicodeString("MMM dd, yyyy h:m:s a"),
    UnicodeString("MMM dd, yyyy h:m:s z"),
    UnicodeString("MMMM dd, yyyy h:m a"),
    UnicodeString("MMMM dd, yyyy h:m:s a"),
    UnicodeString("dd MMM yyyy h:m:s z"),
    UnicodeString("dd MMM yyyy h:m:s z"),
    UnicodeString("MM/dd/yy hh:mm:s a"),
    UnicodeString("MM/dd/yy hh:mm:s z"),
    UnicodeString("MM/dd/yy hh:mm z"),
    UnicodeString("MM/dd/yy hh:mm"),
    UnicodeString("hh:mm MM/d/y z")
};
*/
const t_int32 kPATTERNSIZE = 16;
/*
UnicodeString DateTime::ms_sISO8601Pattern("yyyyMMdd'T'HHmmss'Z'");
UnicodeString DateTime::ms_sISO8601LocalPattern("yyyyMMdd'T'HHmmss");
UnicodeString DateTime::ms_sISO8601TimeOnlyPattern("HHmmss");
UnicodeString DateTime::ms_sISO8601DateOnlyPattern("yyyyMMdd");
UnicodeString DateTime::ms_sDefaultPattern("MMM dd, yyyy h:mm a z");
*/

ErrorCode DateTime::staticStatus = ZERO_ERROR;
ErrorCode DateTime::staticStatus2= ZERO_ERROR;

#if 0
    /* Not doing this now: Cache simpledateformats of certain locales */
Hashtable DateTime::sdfHashtable;

SimpleDateFormat DateTime::kDateFormats[] =
{
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::US, 
        DateTime::staticStatus),
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::UK, 
        DateTime::staticStatus),
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::FRANCE, 
        DateTime::staticStatus),
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::GERMANY, 
        DateTime::staticStatus),
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::ITALY, 
        DateTime::staticStatus),
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::JAPAN, 
        DateTime::staticStatus),
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::KOREA, 
        DateTime::staticStatus),
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::CHINA, 
        DateTime::staticStatus),
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::TAIWAN, 
        DateTime::staticStatus),
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::CANADA, 
        DateTime::staticStatus),
    SimpleDateFormat(DateTime::ms_sDefaultPattern, Locale::CANADA_FRENCH, 
        DateTime::staticStatus)
};
char* DateTime::kDateFormatIDs[] = 
{
    "en_US", "en_GB", "fr_FR", "de_DE", "it_IT", "ja_JP", "ko_KR",
        "zh_CN", "zh_TW", "en_CA", "fr_CA"
};

const t_int32 DateTime::kNumDateFormats = 
    sizeof(kDateFormats) / sizeof(kDateFormats[0]);

void DateTime::initHashtable()
{
    UnicodeString temp;
    SimpleDateFormat * previous = NULL;
    t_int32 i;

    for (i = 0; i < kNumDateFormats; i++)
    {
        previous = (SimpleDateFormat *) sdfHashtable.put(
            new UnicodeStringKey(kDateFormatIDs[i]), &kDateFormats[i]);
            // XXX: new UnicodeStringKey(...) would cause memory leaks her
            if (previous != 0) delete previous;
    }
}
#endif

//const TimeZone * DateTime::m_kGMTTimeZone = TimeZone::createTimeZone("GMT");
TimeZone * DateTime::m_kGMTTimeZone = 0;
//const TimeZone * DateTime::m_kDefaultTimeZone = TimeZone::createDefault();
TimeZone * DateTime::m_kDefaultTimeZone = 0;

// when timezone was static
TimeZone * DateTime::ms_TimeZone = 0; 
//TimeZone * DateTime::ms_TimeZone = TimeZone::createDefault(); 
//Locale DateTime::ms_Locale = Locale::getDefault();
//Locale DateTime::ms_Locale = 0;

//GregorianCalendar DateTime::ms_Calendar((TimeZone *) m_kDefaultTimeZone, 
//                                        ms_Locale, staticStatus);
GregorianCalendar * DateTime::ms_Calendar = 0;

//SimpleDateFormat DateTime::ms_DateFormat(staticStatus2);

//SimpleDateFormat * DateTime::ms_DateFormat = 
//  new SimpleDateFormat(staticStatus2);
//SimpleDateFormat DateTime::ms_DateFormat(staticStatus2);
SimpleDateFormat * DateTime::ms_DateFormat = 0;

//SimpleDateFormat DateTime::ms_GMTISO8601DateFormat(ms_sISO8601Pattern, 
//    staticStatus2);

//---------------------------------------------------------------------

/**
 *  A comparison procedure callable from C
 *  @param v1 pointer to a pointer to the first date
 *  @param v2 pointer to a pointer to the second date
 *  @return   -1 if D1 < D2, 0 if D1 == D2,  1 if D1 > D2
 */
int DateTime_SortAscending( const void* v1, const void* v2 )
{
    DateTime *pD1 = *(DateTime**)v1;
    DateTime *pD2 = *(DateTime**)v2;

    if (*pD1 == *pD2)
        return 0;
    else if (*pD1 > *pD2)
        return 1;
    else
        return -1;
}

/**
 *  A comparison procedure callable from C
 *  @param v1 pointer to a pointer to the first date
 *  @param v2 pointer to a pointer to the second date
 *  @return   -1 if D1 > D2, 0 if D1 == D2,  1 if D1 < D2
 */
int DateTime_SortDescending( const void* v1, const void* v2 )
{
    DateTime *pD1 = *(DateTime**)v1;
    DateTime *pD2 = *(DateTime**)v2;
    if (*pD1 == *pD2)
        return 0;
    else if (*pD1 < *pD2)
        return 1;
    else
        return -1;
}
//----------------------------
// PRIVATE METHODS
//----------------------------
//---------------------------------------------------------------------

t_int8 DateTime::compare(Date d)
{
    if (m_Date < d)
        return -1;
    else if (m_Date > d)
        return 1;
    else return 0;
}

//---------------------------------------------------------------------
// initializes core data members

void DateTime::CtorCore()
{
    m_kGMTTimeZone = TimeZone::createTimeZone("GMT");
    m_kDefaultTimeZone = TimeZone::createDefault();
    ms_TimeZone = TimeZone::createDefault();
    //ms_Locale = &(Locale::getDefault());
    ms_Calendar = new GregorianCalendar(m_kDefaultTimeZone, Locale::getDefault(), staticStatus);
    ms_DateFormat = new SimpleDateFormat(staticStatus2);

    if (ms_Calendar == 0 || ms_DateFormat == 0 || 
        FAILURE(staticStatus) || FAILURE(staticStatus2))
    {
        
        // FAILED to create GregorianCalendar successfully
        //if (FALSE) TRACE("FAILURE in DateTime::CtorCore %d, %d\r\n", 
            //staticStatus, staticStatus2);
		CalendarSetTimeHelper(Calendar::getNow());
    }
    m_Init = FALSE;
    //m_TimeZone = TimeZone::createDefault();
    //PR_ASSERT(m_TimeZone == 0);
    //m_TimeZone = initTimeZone;
    //PR_ASSERT(m_TimeZone != 0);
    //setTimeZone(m_TimeZone);
}

//---------------------------------------------------------------------

t_int32 DateTime::CalendarGetFieldHelper(Calendar::EDateFields aField,
                                         TimeZone * timezone)
{
    ErrorCode status = ZERO_ERROR;
    t_int32 retVal;

    ms_Calendar->setTimeZone(*timezone);
    retVal = ms_Calendar->get(aField, status);
    ms_Calendar->setTimeZone(*ms_TimeZone);

    //retVal = ms_Calendar->get(aField);
    //PR_ASSERT(!FAILURE(status));
    if (FAILURE(status)) 
    {
        //if (FALSE) TRACE("FAILURE in DateTime::CalendarGetFieldHelper %d\r\n", status); 
    }
    return retVal;
}

//---------------------------------------------------------------------

Date DateTime::CalendarGetTimeHelper()
{
    ErrorCode status = ZERO_ERROR;
    Date retVal;

    retVal = ms_Calendar->getTime(status);
    //PR_ASSERT(!FAILURE(status));
    if (FAILURE(status)) 
    { 
        //TRACE("FAILURE in DateTime::CalendarGetTimeHelper %d\r\n", status);
    }
    return retVal;
    
}

//---------------------------------------------------------------------

void DateTime::CalendarSetTimeHelper(Date d)
{
    ErrorCode status = ZERO_ERROR;

    ms_Calendar->setTime(d, status);
    if (FAILURE(status)) 
    {
        //if (FALSE) TRACE("FAILURE in DateTime::CalendarSetTimeHelper %d\r\n", status);
    }
    //ms_Calendar->setTime(d);
    //PR_ASSERT(!FAILURE(status));
}

//---------------------------------------------------------------------

Date DateTime::parse(UnicodeString & s, Locale & locale)
{
    Date d = -1;
    t_int32 i;
    if ((d = parseISO8601(s)) < 0)
    {
        for (i = 0; i < kPATTERNSIZE; i++)
        {
            if ((d = parseTime(JulianFormatString::Instance()->ms_asDateTimePatterns[i], s, locale)) > 0)
            {
                break;
            }
        }
    }
    return d;
}

//---------------------------------------------------------------------

Date DateTime::parseTime(UnicodeString & pattern, UnicodeString & time, 
                         Locale & locale, TimeZone * timezone)
{
    ErrorCode status = ZERO_ERROR;
    Date date = -1;

    ParsePosition pos(0);
    SimpleDateFormat dateFormat(pattern, locale, status);
    //ms_DateFormat->applyLocalizedPattern(pattern, status);

    if (FAILURE(status))
        return -1;
    
    dateFormat.setLenient(FALSE);
    dateFormat.setTimeZone(*timezone);
    date = dateFormat.parse(time, pos);
    //ms_DateFormat->setLenient(FALSE);
    //date = ((DateFormat *) ms_DateFormat)->parse(time, status);

    if (FAILURE(status))
        return -1;

    if (pos == 0)
        return -1;
    
    return date;
}

//---------------------------------------------------------------------

Date DateTime::parseTime(UnicodeString & pattern, UnicodeString & time,
                         TimeZone * timezone)
{
    ErrorCode status = ZERO_ERROR;
    ParsePosition pos(0);
    Date date = -1;

    //ms_DateFormat->applyLocalizedPattern(pattern, status);

    ms_DateFormat->applyLocalizedPattern(pattern, status);
    //ms_DateFormat.applyPattern(pattern);

    if (FAILURE(status)) return -1;

    //ms_DateFormat->setLenient(FALSE);
    
    ms_DateFormat->setLenient(FALSE);
    ms_DateFormat->setTimeZone(*timezone);
    //date = ((DateFormat *) ms_DateFormat)->parse(time, status);
    
    //date = ms_DateFormat->parse(time, pos);
    date = ms_DateFormat->parse(time, pos);
    //ms_DateFormat.setTimeZone(*ms_TimeZone);
    if (pos == 0)
        return -1;
    //if (FAILURE(status)) return -1;

    return date;
}

//---------------------------------------------------------------------

t_int32 DateTime::getMonthLength(t_int32 iMonthNum, t_int32 iYearNum)
{
    // NOTE: iMonthNum assumed to be 1-based.
    // Algorithm assumes GregorianCalendar after 1582.

    if ((iYearNum % 4) != 0)
    {
        return s_shMonthLength[iMonthNum - 1];  // normal years
    }
    else if (((iYearNum  % 4) == 0) && 
        (((iYearNum % 100) != 0) || ((iYearNum % 400) == 0)))
    {
        // normal leap years + 1600, 2000, 2400
        return s_shMonthLengthLeapYear[iMonthNum - 1];  
    }
    else 
    {
        return s_shMonthLength[iMonthNum - 1];  // 1700, 1800, 1900, 2100, 2200, 2300
    }
}

//---------------------------------------------------------------------

Date DateTime::parseISO8601(UnicodeString & us, TimeZone * timezone)
{
    Date t = 0;
    

    // Removed redundant code.  Instead, all parsing using the IsParseable method.

    t_int32 iYear = 0, iMonth = 0, iDay = 0, iHour = 0, iMin = 0, iSec = 0;
    if (!DateTime::IsParseable(us, iYear, iMonth, iDay, iHour, iMin, iSec))
    {
        return -1;
    }
    else
    {
        char sBuf[32];
        t_bool bUTC = FALSE;
        t_int32 strLength = us.size();

        if (iYear < 1970)
        {
            // Dates with year value less than 1970 will be set to 1970
            // So 19671225112233 will become 19701225T112233
            iYear = 1970; 
        }

        // if length is 16 then GMT time
        if (strLength == 16)
        {
            sprintf(sBuf, "%d %d %d %d %d %d GMT", iYear, iMonth, iDay, iHour, iMin, iSec);
            bUTC = TRUE;
        }
        else 
            sprintf(sBuf, "%d %d %d %d %d %d", iYear, iMonth, iDay, iHour, iMin, iSec);

        UnicodeString temp = sBuf;

        //if (FALSE) TRACE("temp = %s\r\n", temp.toCString("")); 
        
        if (bUTC)
        {
         
            if ((t = parseTime(JulianFormatString::Instance()->ms_asDateTimePatterns[0], temp, (TimeZone *) m_kGMTTimeZone)) < 0)
            {
                t = parseTime(JulianFormatString::Instance()->ms_asDateTimePatterns[1], temp, (TimeZone *) m_kGMTTimeZone);
            }
        }
        else 
        {        
            if ((t = parseTime(JulianFormatString::Instance()->ms_asDateTimePatterns[0], temp, timezone)) < 0)
            {
                t = parseTime(JulianFormatString::Instance()->ms_asDateTimePatterns[1], temp, timezone);
            }
        }

        //if (FALSE) TRACE("t = %f\r\n", t);

        return t;
    }
}

//---------------------------------------------------------------------

/////////////////////////
//  PUBLIC METHODS
/////////////////////////

//---------------------------------------------------------------------

//----------------------------
// CONSTRUCTORS
//----------------------------

//---------------------------------------------------------------------

DateTime::DateTime() : m_Date(Calendar::getNow())
{
    if (m_Init)
        CtorCore();
}

//---------------------------------------------------------------------

DateTime::DateTime(t_int32 year, t_int32 month, t_int32 date)
{
    if (m_Init)
        CtorCore();
    
    ms_Calendar->set(year, month, date);
    this->setTime(CalendarGetTimeHelper());
}

//---------------------------------------------------------------------

DateTime::DateTime(t_int32 year, t_int32 month, t_int32 date, 
                   t_int32 hrs, t_int32 min)
{
    if (m_Init)
        CtorCore();
    
    ms_Calendar->set(year, month, date, hrs, min);
    this->setTime(CalendarGetTimeHelper());
}

//---------------------------------------------------------------------

DateTime::DateTime(t_int32 year, t_int32 month, t_int32 date, 
                   t_int32 hrs, t_int32 min, t_int32 sec)
{
    if (m_Init)
        CtorCore();
    
    ms_Calendar->set(year, month, date, hrs, min, sec);
    this->setTime(CalendarGetTimeHelper());

}

//---------------------------------------------------------------------

DateTime::DateTime(Date d)
{
    if (m_Init)
        CtorCore();

    //CalendarSetTimeHelper(d);
    //this->setTime(CalendarGetTimeHelper());
    if (d < 0)
        d = -1;
    this->setTime(d);
}

//---------------------------------------------------------------------

DateTime::DateTime(DateTime & aDateTime)
{
    if (m_Init)
        CtorCore();
    //m_TimeZone = 0;
    //if (m_TimeZone != 0)
    //    delete m_TimeZone;
    //m_TimeZone = aDateTime.m_TimeZone;

    /*
    ms_Calendar->set(aDateTime.getYear(), aDateTime.getMonth(),
        aDateTime.getDate(), aDateTime.getHour(), aDateTime.getMinute(),
        aDateTime.getSecond());
        */
    //this->setTime(CalendarGetTimeHelper());
    this->setTime(aDateTime.m_Date);
}

//---------------------------------------------------------------------

DateTime::DateTime(UnicodeString & us, TimeZone * timezone)
{
    if (m_Init)
    {
        CtorCore();
        if (timezone == 0)
            timezone = ms_TimeZone;
    }
    // to accept non-ISO8601 strings
    ////CalendarSetTimeHelper(DateTime::parse(us, ms_Locale)); 
    
    // only accept ISO8601
    //CalendarSetTimeHelper(DateTime::parseISO8601(us)); 
    //this->setTime(CalendarGetTimeHelper());   

    this->setTime(DateTime::parseISO8601(us, timezone));
}
//---------------------------------------------------------------------

//----------------------------
// DESTRUCTORS
//----------------------------

//---------------------------------------------------------------------

DateTime::~DateTime() 
{ 
    //if (m_TimeZone != 0) 
    //    delete m_TimeZone; m_TimeZone = 0;
}

//---------------------------------------------------------------------

//----------------------------
// GETTERS and SETTERS and ADDERS
//----------------------------

//---------------------------------------------------------------------

Date DateTime::getTime() const
{
    return m_Date;
}

//---------------------------------------------------------------------

void DateTime::setTime(Date d)
{
    m_Date = d;
}

//---------------------------------------------------------------------

void DateTime::setTimeString(UnicodeString & us, TimeZone * timezone)
{
    //setTime(parse(us, ms_Locale));
    setTime(parseISO8601(us, timezone));
}

//---------------------------------------------------------------------
/*
void DateTime::setTimeZone(TimeZone * t)
{
    //if (m_TimeZone != 0)
    //    delete m_TimeZone;
    //m_TimeZone = 0;
    ms_TimeZone = t;
    PR_ASSERT(ms_TimeZone != 0);
    //ms_DateFormat->setTimeZone(*ms_TimeZone);
    ms_DateFormat.setTimeZone(*ms_TimeZone);
    ms_Calendar->setTimeZone(*ms_TimeZone);
}
*/
//---------------------------------------------------------------------

t_int32 DateTime::get(Calendar::EDateFields aField, 
                      TimeZone * timezone)
{
    t_int32 i = -1;

    PR_ASSERT(timezone != 0);
    if (timezone != 0)
    {
        ms_Calendar->setTimeZone(*timezone);
        CalendarSetTimeHelper(this->getTime());
 
        i = CalendarGetFieldHelper(aField, timezone);

        ms_Calendar->setTimeZone(*ms_TimeZone);
    }
    return i;
}

//---------------------------------------------------------------------

t_int32 DateTime::getYear(TimeZone * timezone)
{
    return get(Calendar::YEAR, timezone);
}

//---------------------------------------------------------------------

t_int32 DateTime::getMonth(TimeZone * timezone)
{
    return get(Calendar::MONTH, timezone);
}

//---------------------------------------------------------------------

t_int32 DateTime::getDate(TimeZone * timezone)
{
    return get(Calendar::DATE, timezone);
}

//---------------------------------------------------------------------

t_int32 DateTime::getHour(TimeZone * timezone)
{
    return get(Calendar::HOUR_OF_DAY, timezone);
}

//---------------------------------------------------------------------

t_int32 DateTime::getMinute(TimeZone * timezone)
{
    return get(Calendar::MINUTE, timezone);
}

//---------------------------------------------------------------------

t_int32 DateTime::getSecond(TimeZone * timezone)
{
    return get(Calendar::SECOND, timezone);
}

//---------------------------------------------------------------------

t_int32 DateTime::getDayOfWeekInMonth(TimeZone * timezone)
{
    return get(Calendar::DAY_OF_WEEK_IN_MONTH, timezone);
}

//---------------------------------------------------------------------

void DateTime::setDMY(t_int32 iDay, t_int32 iMonth, t_int32 iYear)
{

    //ErrorCode status = ZERO_ERROR;
   
    CalendarSetTimeHelper(this->getTime());
    ms_Calendar->set(iYear, iMonth, iDay);
    Date d = CalendarGetTimeHelper();
    this->setTime(d);
}

//---------------------------------------------------------------------

void DateTime::setDayOfMonth(t_int32 iDay)
{
    //ErrorCode status = ZERO_ERROR;

    CalendarSetTimeHelper(this->getTime());
    ms_Calendar->set(this->get(Calendar::YEAR), 
        this->get(Calendar::MONTH), iDay);
    Date d = CalendarGetTimeHelper();
    this->setTime(d);
}

//---------------------------------------------------------------------

void DateTime::set(Calendar::EDateFields aField, t_int32 amount)
{
    CalendarSetTimeHelper(this->getTime());
    
    ms_Calendar->set(aField, amount);
    
    this->setTime(CalendarGetTimeHelper());
}

//---------------------------------------------------------------------

void DateTime::add(Calendar::EDateFields aField, t_int32 amount)
{
    ErrorCode status = ZERO_ERROR;

    CalendarSetTimeHelper(this->getTime());
    
    ms_Calendar->add(aField, amount, status);
    //PR_ASSERT(!FAILURE(status));
    if (FAILURE(status)) 
    {
        //if (FALSE) TRACE("FAILURE in DateTime::add %d\r\n", status);
    }
    this->setTime(CalendarGetTimeHelper());
}

//---------------------------------------------------------------------

void DateTime::add(Duration d)
{
    add(Calendar::YEAR, d.getYear());
    add(Calendar::MONTH, d.getMonth()); 
    add(Calendar::DATE, d.getDay()); 
    add(Calendar::HOUR_OF_DAY, d.getHour()); 
    add(Calendar::MINUTE, d.getMinute());
    add(Calendar::SECOND, d.getSecond());
    add(Calendar::WEEK_OF_YEAR, d.getWeek());
}

//---------------------------------------------------------------------

void DateTime::subtract(Duration d)
{
    add(Calendar::YEAR, - d.getYear());
    add(Calendar::MONTH, - d.getMonth()); 
    add(Calendar::DATE, - d.getDay()); 
    add(Calendar::HOUR_OF_DAY, - d.getHour()); 
    add(Calendar::MINUTE, - d.getMinute());
    add(Calendar::SECOND, - d.getSecond());
    add(Calendar::WEEK_OF_YEAR, - d.getWeek());
}

//---------------------------------------------------------------------
Duration & DateTime::getDuration(DateTime start, DateTime end, 
                                 Duration & out)
{
     t_int32 y = 0, mo = 0, d = 0, h = 0, m = 0, s = 0;
     t_int32 toNextMonth = 0;

    if (end < start)
    {
        out.set(-1, -1, -1, -1, -1, -1);
        out.setWeek(-1);
        return out;
    }
    else
    {
        y = end.get(Calendar::YEAR) - start.get(Calendar::YEAR);
        mo = end.get(Calendar::MONTH) - start.get(Calendar::MONTH);
        if (mo < 0)
        {
            y--;
            mo += 12;
        }
        d = end.get(Calendar::DATE) - start.get(Calendar::DATE);
        if (d < 0)
        {
            mo--;
            toNextMonth = start.get(Calendar::DATE);
            
            // count number of days to get to next month
            start.findLastDayOfMonth();
            toNextMonth = start.get(Calendar::DATE) - toNextMonth;
            start.nextDay();
            toNextMonth++;
            
            // now start day <= end day
            d = end.get(Calendar::DATE) - start.get(Calendar::DATE);
            d += toNextMonth; // add number of days to get to next month
        }
        
        h = end.get(Calendar::HOUR_OF_DAY) - start.get(Calendar::HOUR_OF_DAY);
        if (h < 0)
        {
            d--;
            h += 24;
        }
        m = end.get(Calendar::MINUTE) - start.get(Calendar::MINUTE);
        if (m < 0)
        {
            h--;
            m += 60;
        }
        s = end.get(Calendar::SECOND) - start.get(Calendar::SECOND);
        if (s < 0)
        {
            m--;
            s += 60;
        }
        out.set(y, mo, d, h, m, s);
        out.setWeek(0);
        return out;
    }
}
//---------------------------------------------------------------------

void DateTime::clear()
{
    CalendarSetTimeHelper(this->getTime());

    ms_Calendar->clear();
    
    this->setTime(CalendarGetTimeHelper());
}
//---------------------------------------------------------------------

//----------------------------
// UTILITIES
//----------------------------

//---------------------------------------------------------------------

t_bool DateTime::isValid()
{
    if (getTime() < 0)
        return FALSE;
    return TRUE;
}

//---------------------------------------------------------------------

void DateTime::prevDay()
{
    add(Calendar::DAY_OF_YEAR, -1);
}

//---------------------------------------------------------------------

void DateTime::prevDay(t_int32 i)
{
    add(Calendar::DAY_OF_YEAR, -i);
}

//---------------------------------------------------------------------

void DateTime::nextDay()
{
    add(Calendar::DAY_OF_YEAR, 1);
}

//---------------------------------------------------------------------

void DateTime::nextDay(t_int32 i)
{
    add(Calendar::DAY_OF_YEAR, i);
}

//---------------------------------------------------------------------

void DateTime::prevWeek()
{
    add(Calendar::WEEK_OF_YEAR, -1);
}

//---------------------------------------------------------------------

void DateTime::nextWeek()
{
    add(Calendar::WEEK_OF_YEAR, 1);
}

//---------------------------------------------------------------------

void DateTime::moveTo(t_int32 i, t_int32 direction, t_bool force)
{
    if (force)
        add(Calendar::DAY_OF_WEEK, direction);

    while (get(Calendar::DAY_OF_WEEK) != i)
    {
        add(Calendar::DAY_OF_WEEK, direction);
    }
}

//---------------------------------------------------------------------

void DateTime::setByISOWeek(t_int32 week)
{
    PR_ASSERT(week != 0);
    if (week > 0)
    {
        // find the first day of the year
        add(Calendar::DAY_OF_YEAR, - get(Calendar::DAY_OF_YEAR) + 1);

        // jump to the first day
        moveTo(Calendar::THURSDAY, 1, FALSE);

        // back track to the Monday before the Thursday
        moveTo(Calendar::MONDAY, -1, FALSE);

        // we're now on the 1st day of the 1st week 
        add(Calendar::DAY_OF_YEAR, 7 * (week - 1));
    }
    else 
    {
        // find the last day of the year
        findLastDayOfYear();
        moveTo(Calendar::THURSDAY, -1, FALSE);
        moveTo(Calendar::MONDAY, -1, FALSE);
        add(Calendar::DAY_OF_YEAR, 7 * (week + 1));
    }
}
//---------------------------------------------------------------------

void DateTime::prevMonth()
{
    add(Calendar::MONTH, -1);
}

//---------------------------------------------------------------------

void DateTime::prevMonth(t_int32 iLim)
{
    t_int32 i;
    for (i = 0; i < iLim; i++)
        prevMonth();
}

//---------------------------------------------------------------------

void DateTime::nextMonth()
{
    t_int32 iMonth = get(Calendar::MONTH);
    if (iMonth == Calendar::DECEMBER)
        iMonth = Calendar::JANUARY;
    else
        iMonth++;

    add(Calendar::MONTH, 1);

    while ( get(Calendar::MONTH) != iMonth)
        add(Calendar::DATE, -1);

}

//---------------------------------------------------------------------

void DateTime::nextMonth(t_int32 iLim)
{
    t_int32 i;
    for (i = 0; i < iLim; i++)
		nextMonth();
}

//---------------------------------------------------------------------

t_int32 DateTime::prevDOW(t_int32 n)
{
    t_int32 iCount = 0;

    CalendarSetTimeHelper(getTime());

    if (n < Calendar::SUNDAY || n > Calendar::SATURDAY)
      return 0;

    while(get(Calendar::DAY_OF_WEEK) != n)
    {
      prevDay();
      ++iCount;
    }

    return iCount;
}

//---------------------------------------------------------------------

t_int32 DateTime::compareTo(DateTime &dt)
{
    Date d = dt.getTime();
    return compare(d);
}

//---------------------------------------------------------------------

t_bool DateTime::beforeDate(Date d)
{
    return m_Date < d;
}

//---------------------------------------------------------------------

t_bool DateTime::beforeDateTime(DateTime dt)
{
    return beforeDate(dt.getTime());
}

//---------------------------------------------------------------------

t_bool DateTime::before(DateTime * dt)
{
    PR_ASSERT(dt != 0);
    return (compareTo(*dt) == -1);
}

//---------------------------------------------------------------------

t_bool DateTime::afterDate(Date d)
{
    return m_Date > d;
}

//---------------------------------------------------------------------

t_bool DateTime::afterDateTime(DateTime dt)
{
    return afterDate(dt.getTime());
}

//---------------------------------------------------------------------

t_bool DateTime::after(DateTime *dt)
{
    PR_ASSERT(dt != 0);
    return (compareTo(*dt) == 1);
}

//---------------------------------------------------------------------

t_bool DateTime::equalsDate(Date d)
{
    return m_Date == d;
}

//---------------------------------------------------------------------

t_bool DateTime::equalsDateTime(DateTime dt)
{
    return equalsDate(dt.getTime());
}

//---------------------------------------------------------------------

t_bool DateTime::equals(DateTime *dt)
{
    PR_ASSERT(dt != 0);
    return (compareTo(*dt) == 0);
}

//---------------------------------------------------------------------

t_bool DateTime::sameDMY(DateTime * d, TimeZone * timezone)
{
    t_bool retVal;

    CalendarSetTimeHelper(this->getTime());
    t_int32 myYear = this->get(Calendar::YEAR, timezone);
    t_int32 myMonth = this->get(Calendar::MONTH, timezone);
    t_int32 myDate = this->get(Calendar::DATE, timezone);

    CalendarSetTimeHelper(d->getTime());
    
    retVal = 
        ((myDate == CalendarGetFieldHelper(Calendar::DATE, timezone)) &&
        (myMonth == CalendarGetFieldHelper(Calendar::MONTH, timezone)) &&
        (myYear == CalendarGetFieldHelper(Calendar::YEAR, timezone))); 

    CalendarSetTimeHelper(this->getTime()); // set time back
    return retVal;
}

//---------------------------------------------------------------------

UnicodeString DateTime::toString(TimeZone * timezone)
{
    //return strftimeNoLocale(ms_sDefaultPattern, m_TimeZone);
    return strftimeNoLocale(JulianFormatString::Instance()->ms_sDateTimeDefaultPattern, timezone);
}

char * DateTime::DEBUG_toString() { return toString().toCString(""); }
//---------------------------------------------------------------------

UnicodeString DateTime::toISO8601Local(TimeZone * timezone)
{
    //return strftimeNoLocale(ms_sISO8601LocalPattern, m_TimeZone);
    return strftimeNoLocale(JulianFormatString::Instance()->ms_sDateTimeISO8601LocalPattern, timezone);
}

char * DateTime::DEBUG_toISO8601Local() { return toISO8601().toCString(""); }
//---------------------------------------------------------------------

UnicodeString DateTime::toISO8601()
{
    return strftimeNoLocale(JulianFormatString::Instance()->ms_sDateTimeISO8601Pattern, 
        (TimeZone *) m_kGMTTimeZone);
}

char * DateTime::DEBUG_toISO8601() { return toISO8601().toCString(""); }
//---------------------------------------------------------------------

UnicodeString DateTime::toISO8601LocalTimeOnly(TimeZone * timezone)
{
    //return strftimeNoLocale(ms_sISO8601TimeOnlyPattern, m_TimeZone);
    return strftimeNoLocale(JulianFormatString::Instance()->ms_sDateTimeISO8601TimeOnlyPattern, timezone);
}

//---------------------------------------------------------------------

UnicodeString DateTime::toISO8601TimeOnly()
{
    return strftimeNoLocale(JulianFormatString::Instance()->ms_sDateTimeISO8601TimeOnlyPattern, 
        (TimeZone *) m_kGMTTimeZone);
}

//---------------------------------------------------------------------

UnicodeString DateTime::toISO8601LocalDateOnly(TimeZone * timezone)
{
    //return strftimeNoLocale(ms_sISO8601DateOnlyPattern, m_TimeZone);
    return strftimeNoLocale(JulianFormatString::Instance()->ms_sDateTimeISO8601DateOnlyPattern, timezone);
}

//---------------------------------------------------------------------

UnicodeString DateTime::toISO8601DateOnly()
{
    return strftimeNoLocale(JulianFormatString::Instance()->ms_sDateTimeISO8601DateOnlyPattern, 
        (TimeZone *) m_kGMTTimeZone);
}

//---------------------------------------------------------------------

UnicodeString DateTime::strftime(UnicodeString &pattern, TimeZone * timezone)
{
    //return strftimeNoLocale(pattern, m_TimeZone);
    return strftimeNoLocale(pattern, timezone);
}

//---------------------------------------------------------------------

UnicodeString DateTime::strftimeNoLocale(UnicodeString &pattern, 
                                         TimeZone * aTimeZone)
{
    UnicodeString result;

    //time_t tt, tt2;
    //time (&tt);

    FieldPosition pos(0);
    ErrorCode status = ZERO_ERROR;
    Date d = getTime();

    //ms_DateFormat->applyLocalizedPattern(pattern, status);
    ms_DateFormat->applyLocalizedPattern(pattern, status);
    
    if (FAILURE(status)) 
    {
        //if (FALSE) TRACE("FAILURE in DateTime::strftimeNoLocale %d\r\n", status);
    }
    if (aTimeZone != 0) {
        //ms_DateFormat->setTimeZone(*aTimeZone); 
        ms_DateFormat->setTimeZone(*aTimeZone); 
    }
    else {
        //ms_DateFormat->setTimeZone(*ms_TimeZone);  // assumption: ms_TimeZone is never NULL
        ms_DateFormat->setTimeZone(*ms_TimeZone);  // assumption: ms_TimeZone is never NULL
    }
    //return ((DateFormat *) ms_DateFormat)->format(getTime(), result);
    //return ms_DateFormat->format(d, result, pos);

    //ms_DateFormat->format(d, result, pos);
    ms_DateFormat->format(d, result, pos);

    //time (&tt2);
    //if (FALSE) TRACE("%d milleseconds for strftime(1)\r\n", tt2 -tt);

    return result;
}

//---------------------------------------------------------------------

UnicodeString DateTime::strftime(UnicodeString &pattern, Locale & locale, 
                                 TimeZone * timezone)
{
    UnicodeString result;

    //time_t tt, tt2;
    //time (&tt);

    ErrorCode status = ZERO_ERROR;
    FieldPosition pos(0);
    Date d = getTime();
    UnicodeString name;
    
    ///////// PTR ///////////
    //SimpleDateFormat * dateFormat;

    //if (sdfHashtable.isEmpty()) initHashtable();

    UnicodeString ID = locale.getName(name);
    //dateFormat = (SimpleDateFormat *) sdfHashtable.get(UnicodeStringKey(ID));

    // Must split up to avoid LNK4103
    SimpleDateFormat sdf(pattern, locale, status);
    if (FAILURE(status)) 
    {
        //if (FALSE) TRACE("FAILURE in DateTime::strftime(3, 1) %d\r\n", status);
    }
    if (timezone != 0) sdf.setTimeZone(*timezone);
    sdf.format(d, result, pos);
    return result;
        
    //}
    //else
    //{
        // TODO: CACHE SIMPLEDATEFORMATS
        /*
        ////if (FALSE) TRACE("pattern = %s, locale = %s\r\n", pattern.toCString(""), locale.getName(name).toCString(""));
        ////dateFormat->applyLocalizedPattern(pattern, status);
        //dateFormat->applyPattern(pattern);

        //if (FAILURE(status)) 
        //    if (FALSE) TRACE("FAILURE in DateTime::strftime(3, 2) :%d errorcode, %s pattern, %s locale\r\n", 
        //        status, pattern.toCString(""), locale.getName(name).toCString(""));
        //if (timezone != NULL) dateFormat->setTimeZone(*timezone); // ptr
        //dateFormat->format(d, result, pos);  //ptr
        ////dateFormat->format(d, result);  // dateformat ptr
        ////delete dateFormat;
        */
    //}
   
    //if (FALSE) TRACE("result = %s", result.toCString(""));
    //time (&tt2);
    //if (FALSE) TRACE("%d milleseconds for strftime(3)\r\n", tt2 -tt);
    //return result;
}

//---------------------------------------------------------------------
/*
void DateTime::findLastDayOfMonth(DateTime *s)
{
    set(Calendar::MONTH, (s->get(Calendar::MONTH) + 1) % 12);
    add(Calendar::MONTH, 1);
    set(Calendar::DATE, 1);
    add(Calendar::DATE, -1);
}
*/
void DateTime::findLastDayOfMonth()
{
    add(Calendar::MONTH, 1);
    //set(Calendar::DATE, 1);
    add(Calendar::DATE, -get(Calendar::DATE));
}

//---------------------------------------------------------------------
/*
void DateTime::findLastDayOfYear(DateTime *s)
{
    set(Calendar::YEAR, s->get(Calendar::YEAR));
    s->add(Calendar::DAY_OF_YEAR, - (get(Calendar::DAY_OF_YEAR)));
}
*/
void DateTime::findLastDayOfYear()
{
    add(Calendar::YEAR, 1);
    add(Calendar::DAY_OF_YEAR, - (get(Calendar::DAY_OF_YEAR)));
}

//---------------------------------------------------------------------

//----------------------------
// STATIC METHODS
//----------------------------

//---------------------------------------------------------------------

t_int32 DateTime::getPrevDOW(t_int32 n)
{
    if (n == Calendar::SATURDAY)
        return Calendar::SUNDAY;
    else
        return n - 1;
}

//---------------------------------------------------------------------

t_int32 DateTime::getNextDOW(t_int32 n)
{
    if (n == Calendar::SATURDAY)
        return Calendar::SUNDAY;
    else
        return n + 1;
}

//---------------------------------------------------------------------

UnicodeString DateTime::DateToString(DateTime & dt)
{
    return dt.toString();
}

//---------------------------------------------------------------------
 
t_bool DateTime::IsValidDateTime(DateTime & dt)
{
    if (dt.getTime() < 0) 
        return FALSE;
    else
        return TRUE;
}
//---------------------------------------------------------------------

t_bool DateTime::IsParseable(UnicodeString & s)
{
    t_int32 year, month, day, hour, min, sec;
    return IsParseable(s, year, month, day, hour, min, sec);
}
//---------------------------------------------------------------------
t_bool 
DateTime::IsParseable(UnicodeString & s, t_int32 & iYear, t_int32 & iMonth,
                      t_int32 & iDay, t_int32 & iHour, t_int32 & iMinute,
                      t_int32 & iSecond)
{
    t_bool bParseError = FALSE;
    //UnicodeString temp;

    if (s.size() < 8)
        return FALSE;
    else if (s.size() > 8 && s.size() < 15)
        return FALSE;
    else if (s.size() > 16)
        return FALSE;
    else if (s.size() == 16 && s[(TextOffset) 15] != 'Z')
        return FALSE;
    else if (s.size() > 8 && s.indexOf('T', 8, 1) < 0)
        return FALSE;
    else
    {
        char * c = s.toCString("");

         // Looks like it could be formatted OK, try to parse it
        //s.trim();

        /*
        // old way, works but slow
        iYear  = JulianUtility::atot_int32(s.extract(0, 4, temp).toCString(""), bParseError, 4);
        iMonth = JulianUtility::atot_int32(s.extract(4, 2, temp).toCString(""), bParseError, 2);
        iDay   = JulianUtility::atot_int32(s.extract(6, 2, temp).toCString(""), bParseError, 2);
        */

         // new way is faster, but uses ptr arithmetic
        iYear  = JulianUtility::atot_int32(c, bParseError, 4);
        iMonth = JulianUtility::atot_int32(c + 4, bParseError, 2);
        iDay   = JulianUtility::atot_int32(c + 6, bParseError, 2);
       
        if (iMonth > 12 || iDay > 31) 
            return FALSE;

        // more advanced check - check for day mishaps, including leap years
        if (iDay > getMonthLength(iMonth, iYear))
            return FALSE;

        if (s.size() > 8)
        {
            /*
            // old way, works but slow
            iHour  = JulianUtility::atot_int32(s.extract(9, 2, temp).toCString(""), bParseError, 2);
            iMinute= JulianUtility::atot_int32(s.extract(11, 2, temp).toCString(""), bParseError, 2);
            iSecond= JulianUtility::atot_int32(s.extract(13, 2, temp).toCString(""), bParseError, 2);
            */

            // new way is faster, but uses ptr arithmetic
            iHour  = JulianUtility::atot_int32(c + 9, bParseError, 2);
            iMinute   = JulianUtility::atot_int32(c + 11, bParseError, 2);
            iSecond   = JulianUtility::atot_int32(c + 13, bParseError, 2);

            if (iHour > 23 || iMinute > 59 || iSecond > 59)
                return FALSE;
        }
        if (bParseError)
            return FALSE;
     
        return TRUE;
    }
}
//---------------------------------------------------------------------

t_bool DateTime::IsParseableDateTime(UnicodeString & s)
{
    return (s.size() >= 15 && DateTime::IsParseable(s));
}
//---------------------------------------------------------------------

t_bool DateTime::IsParseableDate(UnicodeString & s)
{
    return (s.size() == 8 && DateTime::IsParseable(s));
}
//---------------------------------------------------------------------
t_bool DateTime::IsParseableUTCOffset(UnicodeString & s)
{
    t_int32 hour, minute;
    return DateTime::IsParseableUTCOffset(s, hour, minute);
}
//---------------------------------------------------------------------
t_bool DateTime::IsParseableUTCOffset(UnicodeString & s, 
                                      t_int32 & iHour, t_int32 & iMin)
{
    t_bool bParseError = ZERO_ERROR;
    char c;

    if (s.size() != 5)
        return FALSE;

    UnicodeString u;
    //if (FALSE) TRACE("%s, %s\r\n", s.toCString(""), u.toCString(""));
    
    c = (char) s[(TextOffset) 0];
    if ((c != '+') && (c != '-'))
        return FALSE;
    
    iHour = JulianUtility::atot_int32(s.extract(1, 2, u).toCString(""), bParseError, 2);
    iMin = JulianUtility::atot_int32(s.extract(3, 2, u).toCString(""), bParseError, 2);

    if (bParseError)
        return FALSE;
    if ((iHour > 23) || (iMin > 59))
        return FALSE;
    if (c == '-')
    {
        iHour = - iHour;
        iMin = - iMin;
    }
    return TRUE;
}

//---------------------------------------------------------------------

int DateTime::CompareDateTimes(const void * a, const void * b)
{
    PR_ASSERT(a != 0 && b != 0);

    DateTime * dtA = *(DateTime **) a;
    DateTime * dtB = *(DateTime **) b;
    return (int) (*dtA).compareTo(*dtB);
}

//---------------------------------------------------------------------

//-----------------------------
// OVERLOADED OPERATORS    
//-----------------------------

//---------------------------------------------------------------------

const DateTime & DateTime::operator=(const DateTime & that)
{

    // if(m_Init)
    //CtorCore(that.m_TimeZone);
    //Date d = that.getTime();
    //if (m_TimeZone != 0)
    //    delete m_TimeZone;
    //m_TimeZone = that.m_TimeZone;
    
    //CalendarSetTimeHelper(d);
    //this->setTime(CalendarGetTimeHelper());
    //this->setTimeZone(that.ms_TimeZone);
    this->setTime(that.getTime());
    return *this;
}

//---------------------------------------------------------------------

t_bool DateTime::operator==(const DateTime & that)
{
    return compare(that.getTime()) == 0;
}

//---------------------------------------------------------------------

t_bool DateTime::operator!=(const DateTime & that)
{
    return compare(that.getTime()) != 0;
}

//---------------------------------------------------------------------

t_bool DateTime::operator>(const DateTime & that)
{
    return compare(that.getTime()) == 1;
}

//---------------------------------------------------------------------

t_bool DateTime::operator<(const DateTime & that)
{
    return compare(that.getTime()) == -1;
}

//---------------------------------------------------------------------
