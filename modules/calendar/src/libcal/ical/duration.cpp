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

// duration.cpp
// John Sun
// 5:52 PM January 28 1998

#include "stdafx.h"
#include "jdefines.h"

#include <assert.h>
#include <ptypes.h>
#include <unistring.h>
//#include "datetime.h"
#include "duration.h"
#include "jutility.h"
//#include "jlog.h"
//////////////////////////////
// PRIVATE METHODS
//////////////////////////////

//---------------------------------------------------------------------

//static UnicodeString s_sP = "P";
//static UnicodeString s_sT = "T";
//static UnicodeString s_sY = "Y";
//static UnicodeString s_sM = "M";
//static UnicodeString s_sD = "D";
//static UnicodeString s_sH = "H";
//static UnicodeString s_sS = "S";
//static UnicodeString s_sW = "W";

//static UnicodeString s_sICALInvalidDuration = "PT-1H";
//---------------------------------------------------------------------

void Duration::init() {}

//---------------------------------------------------------------------

void Duration::setInvalidDuration()
{
    m_iYear = -1;
    m_iMonth = -1;
    m_iDay = -1;
    m_iHour = -1;
    m_iMinute = -1;
    m_iSecond = -1;
    m_iWeek = -1;
}

//---------------------------------------------------------------------

void Duration::parse(UnicodeString & us)
{
    // NOTE: use a better algorithm like a regexp scanf or something.

    //UnicodeString sYear, sMonth, sDay, sHour, sMinute, sSecond, sWeek;
    UnicodeString sVal;
    //UnicodeString sDate, sTime;
    UnicodeString sSection;
    //UnicodeString temp;
    t_bool bErrorInParse = FALSE;
    t_int32 startOfParse = 0, indexOfT = 0, endOfParse = 0;
    t_int32 year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, week = 0;
    t_int32 indexOfY = 0, indexOfMo = 0, indexOfD = 0;
    t_int32 indexOfH = 0, indexOfMi = 0, indexOfS = 0, indexOfW = 0;
    
    if (us[(TextOffset) 0] != 'P') //!us.startsWith(s_sP))    
    {
        bErrorInParse = TRUE;
    }
    else 
    {
        startOfParse = 1;
        indexOfT = us.indexOf('T');
        endOfParse = us.size();
        indexOfW = us.indexOf('W');
        if (indexOfW == -1)
        {
            // first parse the date section
            if (indexOfT > 0)
            {
                endOfParse = indexOfT;
            }
            sSection = us.extractBetween(startOfParse, endOfParse, sSection);
            startOfParse = 0;
            endOfParse = sSection.size();
            indexOfY = sSection.indexOf('Y');
            if (indexOfY >= 0)
            {
                sVal = sSection.extractBetween(startOfParse, indexOfY, sVal);
                year = JulianUtility::atot_int32(sVal.toCString(""), 
                    bErrorInParse, sVal.size());
                startOfParse = indexOfY + 1;
            }
            indexOfMo = sSection.indexOf('M');
            if (indexOfMo >= 0)
            {
                sVal = sSection.extractBetween(startOfParse, indexOfMo, sVal);
                month = JulianUtility::atot_int32(sVal.toCString(""), 
                    bErrorInParse, sVal.size());
                startOfParse = indexOfMo + 1;
            }
            indexOfD = sSection.indexOf('D');
            if (indexOfD >= 0) 
            {
                sVal = sSection.extractBetween(startOfParse, indexOfD, sVal);
                day = JulianUtility::atot_int32(sVal.toCString(""), 
                    bErrorInParse, sVal.size());
                startOfParse = indexOfD + 1;
            }
            if (sSection.size() >= 1 && indexOfY == -1 &&             
                indexOfMo == -1 && indexOfD == -1)
            {
                bErrorInParse = TRUE;
            }
            else if (indexOfY != -1 && indexOfMo != -1 && indexOfY > indexOfMo)
                bErrorInParse = TRUE;
            else if (indexOfY != -1 && indexOfD != -1 && indexOfY > indexOfD)
                bErrorInParse = TRUE;
            else if (indexOfMo != -1 && indexOfD != -1 && indexOfMo > indexOfD)
                bErrorInParse = TRUE;
            else if (indexOfD != -1 && indexOfD != endOfParse -1)
                bErrorInParse = TRUE;
            else if (indexOfMo != -1 && indexOfD == -1 && indexOfMo != endOfParse -1)
                bErrorInParse = TRUE;
            else if (indexOfY != -1 && indexOfD == -1 && indexOfMo == -1 && indexOfY != endOfParse -1)
                bErrorInParse = TRUE;
            // now parse time section
            if (indexOfT > 0)
            {
                startOfParse = indexOfT + 1;
                endOfParse = us.size();
                sSection = us.extractBetween(startOfParse, endOfParse, sSection);
                startOfParse = 0;
                endOfParse = sSection.size();
                indexOfH = sSection.indexOf('H');
                if (indexOfH >= 0) {
                    sVal = sSection.extractBetween(startOfParse, indexOfH, sVal);   
                    hour = JulianUtility::atot_int32(sVal.toCString(""),
                        bErrorInParse, sVal.size());
                    startOfParse = indexOfH + 1;
                }
                indexOfMi = sSection.indexOf('M');
                
                if (indexOfMi >= 0) {
                    sVal = sSection.extractBetween(startOfParse, indexOfMi, sVal);
                    minute = JulianUtility::atot_int32(sVal.toCString(""),
                        bErrorInParse, sVal.size());                    
                    startOfParse = indexOfMi + 1;
                }
                indexOfS = sSection.indexOf('S');
                if (indexOfS >= 0)
                {
                    sVal = sSection.extractBetween(startOfParse, indexOfS, sVal);
                    second = JulianUtility::atot_int32(sVal.toCString(""),
                        bErrorInParse, sVal.size());
                    startOfParse = indexOfS + 1;                
                }
                if (sSection.size() > 0 && indexOfH == -1 &&
                    indexOfMi == -1 && indexOfS == -1)
                {
                    bErrorInParse = TRUE;
                }
                else if (indexOfH != -1 && indexOfMi != -1 && indexOfH > indexOfMi)
                    bErrorInParse = TRUE;
                else if (indexOfH != -1 && indexOfS != -1 && indexOfH > indexOfS)
                    bErrorInParse = TRUE;
                else if (indexOfMi != -1 && indexOfS != -1 && indexOfMi > indexOfS)
                    bErrorInParse = TRUE;
                else if (indexOfS != -1 && indexOfS != endOfParse -1)
                    bErrorInParse = TRUE;
                else if (indexOfMi != -1 && indexOfS == -1 && indexOfMi != endOfParse -1)
                    bErrorInParse = TRUE;
                else if (indexOfH != -1 && indexOfS == -1 && indexOfMi == -1 && indexOfH != endOfParse -1)
                    bErrorInParse = TRUE;
            }
        }
        else
        {
            // week parse
            if (us.size() - 1 != indexOfW)
                bErrorInParse = TRUE;
          
            sVal = us.extractBetween(1, us.size() - 1, sVal);
            week = JulianUtility::atot_int32(sVal.toCString(""),
                bErrorInParse, sVal.size());
                       
        }
   } 
   //if (FALSE) TRACE("parse: %d W, %d Y %d M %d D T %d H %d M %d S\r\n", week, year, month, day, hour, minute, second);
   if (year < 0 || month < 0 || day < 0 ||
       hour < 0 || minute < 0 || second < 0 || week < 0)
       bErrorInParse = TRUE;
    
   if (bErrorInParse)
   {
        // LOG the error
        setInvalidDuration();
   }
   else
   {
        setYear(year);
        setMonth(month);
        setDay(day);
        setHour(hour);
        setMinute(minute);
        setSecond(second);
        setWeek(week);
        normalize();
   }
}

//---------------------------------------------------------------------

//////////////////////////////
// CONSTRUCTORS
//////////////////////////////

//---------------------------------------------------------------------

Duration::Duration()
:
    m_iYear(0),m_iMonth(0),m_iDay(0),m_iHour(0),
    m_iMinute(0),m_iSecond(0), m_iWeek(0)
{
    //init();
}

//---------------------------------------------------------------------

Duration::Duration(UnicodeString & us)
:
    m_iYear(0),m_iMonth(0),m_iDay(0),m_iHour(0),
    m_iMinute(0),m_iSecond(0), m_iWeek(0)
    
{
    //init();
    parse(us);
}

//---------------------------------------------------------------------

Duration::Duration(t_int32 type, t_int32 value)
:
    m_iYear(0),m_iMonth(0),m_iDay(0),m_iHour(0),
    m_iMinute(0),m_iSecond(0), m_iWeek(0)
{
    //init();
    switch (type)
    {
    case JulianUtility::RT_MINUTELY:
        m_iMinute = value;
        break;
    case JulianUtility::RT_HOURLY:
        m_iHour = value;
        break;
    case JulianUtility::RT_DAILY:
        m_iDay = value;
        break;
    case JulianUtility::RT_WEEKLY:
        m_iWeek = value;
        break;
    case JulianUtility::RT_MONTHLY:
        m_iMonth = value;
        break;
    case JulianUtility::RT_YEARLY:
        m_iYear = value;
        break;
    default:
        break;
    }
}

//---------------------------------------------------------------------

Duration::Duration(Duration & d)
:
    m_iYear(0),m_iMonth(0),m_iDay(0),m_iHour(0),
    m_iMinute(0),m_iSecond(0), m_iWeek(0)
{
    t_bool b;

    m_iYear = d.getYear();
    m_iMonth = d.getMonth();
    m_iDay = d.getDay();
    m_iHour = d.getHour();
    m_iMinute = d.getMinute();
    m_iSecond = d.getSecond();

    m_iWeek = d.getWeek();

    // check postcondition
    b = ((m_iWeek != 0) && (m_iYear != 0 || m_iMonth != 0 || 
        m_iDay != 0 || m_iHour !=0 || m_iMinute != 0 || 
        m_iSecond != 0));

    if (!b) {
        //PR_ASSERT(!b);
        //JLog::Instance()->logString(ms_sDurationAssertionFailed);
        //setInvalidDuration();
    }
}
//---------------------------------------------------------------------

Duration::Duration(t_int32 year, t_int32 month, t_int32 day, 
                   t_int32 hour, t_int32 min, t_int32 sec, 
                   t_int32 week)
:   m_iYear(year), m_iMonth(month), m_iDay(day), m_iHour(hour), 
    m_iMinute(min), m_iSecond(sec), m_iWeek(week)
{
    if (!isValid())
    {    
        //PR_ASSERT(isValid());
        //JLog::Instance()->logString(ms_sDurationAssertionFailed);
        setInvalidDuration();
    }
}

//---------------------------------------------------------------------

///////////////////////////////////
// DESTRUCTORS
///////////////////////////////////

//---------------------------------------------------------------------

Duration::~Duration() 
{
}

//---------------------------------------------------------------------

///////////////////////////////////
// GETTERS AND SETTERS
///////////////////////////////////
void Duration::set(t_int32 year, t_int32 month, t_int32 day, 
                    t_int32 hour, t_int32 min, t_int32 sec)
{
    m_iYear = year;
    m_iMonth = month;
    m_iDay = day;
    m_iHour = hour;
    m_iMinute = min;
    m_iSecond = sec;
}

//---------------------------------------------------------------------

///////////////////////////////////
// UTILITIES
///////////////////////////////////

//---------------------------------------------------------------------

void Duration::setDurationString(UnicodeString & s)
{
    parse(s);
}

//---------------------------------------------------------------------

// TODO - will work if only normalize works
t_int32 Duration::compareTo(const Duration & d)
{
    //normalize();

    if (getYear() != d.getYear())
        return ((getYear() > d.getYear()) ? 1 : -1);
    if (getMonth() != d.getMonth())
        return ((getMonth() > d.getMonth()) ? 1 : -1);
    if (getDay() != d.getDay())
        return ((getDay() > d.getDay()) ? 1 : -1);
    if (getHour() != d.getHour())
        return ((getHour() > d.getHour()) ? 1 : -1);
    if (getMinute() != d.getMinute())
        return ((getMinute() > d.getMinute()) ? 1 : -1);
    if (getSecond() != d.getSecond())
        return ((getSecond() > d.getSecond()) ? 1 : -1);
    if (getWeek() != d.getWeek())
        return ((getWeek() > d.getWeek()) ? 1 : -1);
    return 0;
}

//---------------------------------------------------------------------

void Duration::normalize()
{
    // TODO: RISKY, assumes 30 days in a month, 12 months in a year
    t_int32 temp1 = 0, temp2 = 0, temp3 = 0, temp4 = 0, temp5 = 0;
    if (m_iSecond >= 60)
    {
      temp1 = m_iSecond / 60;
      m_iSecond = m_iSecond % 60;
    }
    m_iMinute = m_iMinute + temp1;
    if (m_iMinute >= 60) {
      temp2 = m_iMinute / 60;
      m_iMinute = m_iMinute % 60;
    }
    m_iHour = m_iHour + temp2;
    if (m_iHour >= 24) {
      temp3 = m_iHour / 24;
      m_iHour = m_iHour % 24;
    }
    m_iDay = m_iDay + temp3;
    if (m_iDay >= 30) {
      temp4 = m_iDay / 30;
      m_iDay = m_iDay % 30;
    }
    m_iMonth = m_iMonth + temp4;
    if (m_iMonth >= 12) {
      temp5 = m_iMonth / 12;
      m_iMonth = m_iMonth % 12;
    }
    m_iYear = m_iYear + temp5;
}

//---------------------------------------------------------------------

t_bool
Duration::isZeroLength()
{
    if (m_iYear <= 0 && m_iMonth <= 0 && m_iDay <= 0 && 
        m_iHour <= 0 && m_iMinute <= 0 && m_iSecond <= 0 && 
        m_iWeek <= 0)
        return TRUE;
    else 
        return FALSE;
}

//---------------------------------------------------------------------

UnicodeString Duration::toString()
{
    char sBuf[100];
    char sBuf2[100];

	sBuf[0] = '\0';

    if (m_iWeek != 0)
        sprintf(sBuf, "%d weeks", m_iWeek);
    else
    {
        // Full form

		// eyork Still looking into Libnls to see if strings for year, month, etc are there
		if (m_iYear != 0)	{ sprintf(sBuf2, m_iYear > 1 ?	"%d years " :	"%d year ",	m_iYear);		strcat(sBuf, sBuf2); }
		if (m_iMonth != 0)	{ sprintf(sBuf2, m_iMonth > 1 ?	"%d months " :	"%d month ",	m_iMonth);	strcat(sBuf, sBuf2); }
		if (m_iDay != 0)	{ sprintf(sBuf2, m_iDay > 1 ?	"%d days "	:	"%d day ",	m_iDay);		strcat(sBuf, sBuf2); }
		if (m_iHour != 0)	{ sprintf(sBuf2, m_iHour > 1 ?	"%d hours " :	"%d hour ",	m_iHour);		strcat(sBuf, sBuf2); }
		if (m_iMinute != 0)	{ sprintf(sBuf2, m_iMinute > 1 ?"%d minutes " :	"%d minute ",	m_iMinute);	strcat(sBuf, sBuf2); }
		if (m_iSecond != 0)	{ sprintf(sBuf2, m_iSecond > 1 ?"%d seconds " :	"%d second ",	m_iSecond);	strcat(sBuf, sBuf2); }

		/*
        sprintf(sBuf, 
        "%d year, %d month , %d day, %d hour, %d mins, %d secs",
            m_iYear, m_iMonth, m_iDay, m_iHour, m_iMinute, m_iSecond);
		*/
    }
    return sBuf;
    // return toICALString();
}

//---------------------------------------------------------------------

UnicodeString Duration::toICALString()
{
    //if (FALSE) TRACE("toString: %d W, %d Y %d M %d D T %d H %d M %d S\r\n", m_iYear, m_iMonth, m_iDay, m_iHour, m_im_iMinute, m_iSecond);
    

    if (!isValid())
    {
        return "PT-1H";
        //sOut = "PT-1H";
        //return sOut;
    }
    else if (m_iWeek != 0)
    {
        char sNum[20];
        sprintf(sNum, "P%dW", m_iWeek); 
        return sNum;
    }
    else
    {    

#if 1
        // full form
        char sNum[100];
        sprintf(sNum, "P%dY%dM%dDT%dH%dM%dS", 
            m_iYear, m_iMonth, m_iDay, m_iHour, m_iMinute, m_iSecond);
        return sNum;
#else

        // condensed form
        UnicodeString sOut;
        UnicodeString sDate = "", sTime = "";
        t_int32 aiDate[] = { m_iYear, m_iMonth, m_iDay };
        char asDate[] = { 'Y', 'M', 'D' };
        t_int32 aiTime[] = { m_iHour, m_iMinute, m_iSecond };
        char asTime[] = { 'H', 'M', 'S' };
        t_int32 kSize = 3; 
        int i;

        for (i = 0; i < kSize ; i++) {
            if (aiDate[i] != 0) {
                char sNum[20];
                sprintf(sNum, "%d", aiDate[i]); 
                sDate += sNum;
                sDate += asDate[i];
            }   
        }
        for (i = 0; i < kSize ; i++) {
            if (aiTime[i] != 0) {
                char sNum[20];
                sprintf(sNum, "%d", aiTime[i]); 
                sTime += sNum;
                sTime += asTime[i];
            }   
        }
        if (sTime.size() > 0) {
            sTime.insert(0, "T");
        }
        sOut = sDate;
        sOut += sTime;
      
        if (sOut.size() > 0) {
            sOut.insert(0, "P");
        }
        return sOut;

#endif
    }
}

//---------------------------------------------------------------------

t_bool Duration::isValid()
{
    if (getYear() < 0 || getMonth() < 0 || getDay() < 0 ||
        getHour() < 0 || getMinute() < 0 || getSecond() < 0 || 
        getWeek() < 0)
      return FALSE;
    else return TRUE;
}
//---------------------------------------------------------------------

////////////////////////////////////
// OVERLOADED OPERATORS
////////////////////////////////////

//---------------------------------------------------------------------

const Duration & Duration::operator=(const Duration& d)
{
    //init();
    
    m_iYear = d.getYear();
    m_iMonth = d.getMonth();
    m_iDay = d.getDay();
    m_iHour = d.getHour();
    m_iMinute = d.getMinute();
    m_iSecond = d.getSecond();
    
    m_iWeek = d.getWeek();

    return *this;
}

//---------------------------------------------------------------------

t_bool Duration::operator==(const Duration & that) 
{
    return (compareTo(that) == 0);
}

//---------------------------------------------------------------------

t_bool Duration::operator!=(const Duration & that) 
{
    return (compareTo(that) != 0);
}

//---------------------------------------------------------------------

t_bool Duration::operator>(const Duration & that) 
{
    return (compareTo(that) > 0);
} 

//---------------------------------------------------------------------

t_bool Duration::operator<(const Duration & that) 
{
    return (compareTo(that) < 0);
} 

//---------------------------------------------------------------------

