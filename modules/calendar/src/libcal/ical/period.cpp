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

// period.cpp
// John Sun
// 11:37 AM January 29 1998

#include "stdafx.h"
#include "jdefines.h"

#include <assert.h>
#include <ptypes.h>
#include <unistring.h>
#include "datetime.h"
#include "duration.h"

#include "period.h"

//static UnicodeString s_sP = "P";
//static UnicodeString s_sDash = " - ";
//static UnicodeString s_sForwardSlash = "/";
//static UnicodeString s_sInvalidPeriod = "Invalid Period";
//static UnicodeString s_sICALInvalidPeriod = "19691231T235959Z/PT-1H";
//---------------------------------------------------------------------
//////////////////////////////
// PRIVATE METHODS
//////////////////////////////
//---------------------------------------------------------------------

void Period::parse(UnicodeString & us)
{
    //UnicodeString s = us;
    UnicodeString sStart, sEnd;
    t_int32 indexOfSlash = us.indexOf('/');
    //DateTime start, end;
    //Duration d;

    if (indexOfSlash < 0)
    {
        // LOG and create invalid period
        createInvalidPeriod();
    }
    else
    {
        sStart = us.extractBetween(0, indexOfSlash, sStart);
        sEnd = us.extractBetween(indexOfSlash + 1, us.size(), sEnd);
     
        m_DTStart.setTimeString(sStart); //PR_ASSERT(m_DTStart != 0);
        if (sEnd[(TextOffset) 0] == 'P') //sEnd.startsWith(s_sP))
        {
            m_Duration.setDurationString(sEnd); //PR_ASSERT(m_Duration != 0);
        }
        else
        {
            m_DTEnd.setTimeString(sEnd); //PR_ASSERT(m_DTEnd != 0);
        }
    }
}
//---------------------------------------------------------------------

void Period::selfCheck()
{
    if (!isValid())
        createInvalidPeriod();
}

//---------------------------------------------------------------------

void Period::createInvalidPeriod()
{
    m_DTStart.setTime(-1);
    m_DTEnd.setTime(-1);
    m_Duration.set(-1, -1, -1, -1, -1, -1);
    m_Duration.setWeek(-1);
}

//---------------------------------------------------------------------

DateTime & Period::getEndingTime(DateTime & d)
{
    PR_ASSERT(isValid());
    if (isValid())
    {
        if (getEnd().isValid())
        {
            d = m_DTEnd;
            return d;
        }
        else
        {
            d = m_DTStart;
            d.add(m_Duration);
            return d;
        }
    }
    else
    {
        // NOT valid period returns -1
        d.setTime(-1);
        return d;
    }
}

//---------------------------------------------------------------------
//////////////////////////////
// CONSTRUCTORS
//////////////////////////////
//---------------------------------------------------------------------

Period::Period()
:
    m_DTStart(-1),
    m_DTEnd(-1),
    m_Duration(-1, -1, -1, -1, -1, -1, -1) 
    //m_DTStart(0),
    //m_DTEnd(0),
    //m_Duration(0)
{}

//---------------------------------------------------------------------

Period::Period(Period & that)
{

    m_DTStart.setTime(that.getStart().getTime());
    m_DTEnd.setTime(that.getEnd().getTime());

    m_Duration.set(that.getDuration().getYear(),that.getDuration().getMonth(), 
        that.getDuration().getDay(), that.getDuration().getHour(), 
        that.getDuration().getMinute(), that.getDuration().getSecond());

    m_Duration.setWeek(that.getDuration().getWeek());
}
    
//---------------------------------------------------------------------

Period::Period(UnicodeString & us)
:
    m_DTStart(-1),
    m_DTEnd(-1),
    m_Duration(-1, -1, -1, -1, -1, -1, -1)
{
    parse(us);
    selfCheck();
}

//---------------------------------------------------------------------

//////////////////////////////
// DESTRUCTORS
//////////////////////////////
//---------------------------------------------------------------------

Period::~Period()
{
    //if (m_DTStart != 0) { delete m_DTStart; m_DTStart = 0; }
    //if (m_DTEnd != 0) { delete m_DTEnd; m_DTEnd = 0; }
    //if (m_Duration != 0) { delete m_Duration; m_Duration = 0; }
}

//---------------------------------------------------------------------

//////////////////////////////
// GETTERS and SETTERS
//////////////////////////////
//---------------------------------------------------------------------

void Period::setStart(DateTime d) 
{ 
    m_DTStart = d; 
}

//---------------------------------------------------------------------

void Period::setEnd(DateTime d) 
{ 
    m_DTEnd = d; 
}

//---------------------------------------------------------------------

void Period::setDuration(Duration d) 
{ 
    m_Duration = d; 
}

//---------------------------------------------------------------------

void Period::setPeriodString(UnicodeString & us)
{
    parse(us);
}

//---------------------------------------------------------------------

//////////////////////////////
// UTILITIES
//////////////////////////////

//---------------------------------------------------------------------

t_bool Period::isValid()
{
    //Date d;

    if (!getStart().isValid())
    {
        return FALSE;
    }
    else if (getEnd().isValid())
    {
        if (getEnd() < getStart())
            return FALSE;
        // assert duration is invalid
        PR_ASSERT(!getDuration().isValid());
        return TRUE;
    }
    else {
        if (getDuration().isValid())
            return TRUE;
        return FALSE;
    }
}
//---------------------------------------------------------------------

t_bool Period::isIntersecting(DateTime & start, DateTime & end)
{
    DateTime pEnd;

    PR_ASSERT(!start.afterDateTime(end));
    PR_ASSERT(isValid());
    // Bad input always returns FALSE
    if (start.afterDateTime(end) || !isValid())
    {
        return FALSE;
    }


    if (!getStart().beforeDateTime(end))
        return FALSE;
    
    pEnd = getEndingTime(pEnd);

    if (!pEnd.afterDateTime(start))
        return FALSE;

    return TRUE;
}

//---------------------------------------------------------------------

t_bool Period::isIntersecting(Period & period)
{
    DateTime dS, dE;
    dE = period.getEndingTime(dE);
    dS = period.getStart();
    return isIntersecting(dS, dE);
}

//---------------------------------------------------------------------

t_bool Period::isInside(DateTime & start, DateTime & end)
{
    DateTime pEnd;

    PR_ASSERT(!start.afterDateTime(end));
    PR_ASSERT(isValid());
    // handle bad input (ALWAYS returns FALSE)
    if (start.afterDateTime(end) || !isValid())
    {
        return FALSE;
    }
    pEnd = getEndingTime(pEnd);
    if ((!getStart().beforeDateTime(start)) &&
        (!pEnd.afterDateTime(end)))
        return TRUE;
    return FALSE;
}

//---------------------------------------------------------------------

t_bool Period::isInside(Period & period)
{
    DateTime dS, dE;
    dE = period.getEndingTime(dE);
    dS = period.getStart();
    return isInside(dS, dE);
}

//---------------------------------------------------------------------

UnicodeString Period::toString()
{
    if (!isValid())
    {
        return "Invalid Period";
    }
    else
    {
        UnicodeString u;
        u = getStart().toString();
        u += "-";
        if (getEnd().isValid())
        {
            u += getEnd().toString();
        }
        else
        {
            u += getDuration().toString();
        }
        return u;
    }
}

//---------------------------------------------------------------------

UnicodeString Period::toICALString()
{
    if (!isValid())
    {
        return "19691231T235959Z/PT-1H";
    }
    else 
    {
        UnicodeString sOut = getStart().toISO8601();
        sOut += '/';
        if (getEnd().isValid())
            sOut += getEnd().toISO8601();
        else
            sOut += getDuration().toICALString();
        return sOut;
    }

}

//---------------------------------------------------------------------

//////////////////////////////
// STATIC METHODS
//////////////////////////////
//---------------------------------------------------------------------

t_bool Period::IsConnectedPeriods(Period & p1, Period & p2)
{
    DateTime p1end;
    DateTime p2end;

    if (p1.isIntersecting(p2))
        return TRUE;

    // check for back-to-back periods
    p1end = p1.getEndingTime(p1end);
    p2end = p2.getEndingTime(p2end);
    
    if (p1.getStart().equalsDateTime(p2end) ||
        p2.getStart().equalsDateTime(p1end))
        return TRUE;

    return FALSE;
}

//---------------------------------------------------------------------

void Period::Union(Period & inPer1, Period & inPer2, Period & outPer)
{
    DateTime s1, s2;
    DateTime p1end;
    DateTime p2end;

    PR_ASSERT(inPer1.isValid() && inPer2.isValid());
    // handle bad input (return invalid period)
    if (!inPer1.isValid() || !inPer2.isValid())
    {
        DateTime d;
        d.setTime(-1);
        outPer.setStart(d);
        outPer.setEnd(d);
        return;
    }

    if (!IsConnectedPeriods(inPer1, inPer2))
    {
        // return an invalid period
        outPer.createInvalidPeriod();
    }
    else
    {
        s1 = inPer1.getStart();
        s2 = inPer2.getStart();
        if (s1.beforeDateTime(s2))
            outPer.setStart(s1);
        else
            outPer.setStart(s2);
            
        p1end = inPer1.getEndingTime(p1end);
        p2end = inPer2.getEndingTime(p2end);

        if (p1end.afterDateTime(p2end))
            outPer.setEnd(p1end);
        else
            outPer.setEnd(p2end);
    }
}

//---------------------------------------------------------------------

void Period::Intersection(Period & inPer, DateTime & start, 
                          DateTime & end, Period & outPer)
{
    DateTime pend;
    DateTime pstart;

    PR_ASSERT(inPer.isValid());
    PR_ASSERT(!start.afterDateTime(end));
    // handle bad input (return invalid period)
    if (!inPer.isValid() || start.afterDateTime(end))
    {
        pstart.setTime(-1);
        outPer.setStart(pstart);
        outPer.setEnd(pstart);
    }


    if (!inPer.isIntersecting(start, end))
    {
        outPer.createInvalidPeriod();
    }
    else 
    {
        pstart = inPer.getStart();

        if (start.afterDateTime(pstart))
            outPer.setStart(start);
        else
            outPer.setStart(pstart);

        pend = inPer.getEndingTime(pend);
    
        if (end.beforeDateTime(pend))
            outPer.setEnd(end);
        else
            outPer.setEnd(pend);
    }
}
//---------------------------------------------------------------------

void Period::SubtractedPeriod(Period & inPer, DateTime & start, 
                              DateTime & end, JulianPtrArray * out)
{
    Period * pNew;
    DateTime pend, pstart;

    PR_ASSERT(!start.afterDateTime(end));
    // handle bad input (add nothing to vector)
    if (start.afterDateTime(end))
    {
        return;
    }

    pstart = inPer.getStart();
    if (pstart.beforeDateTime(start))
    {
        pNew = new Period(); PR_ASSERT(pNew != 0);
        pNew->setStart(pstart);
        pNew->setEnd(start);
        out->Add(pNew);
    }
    pend = inPer.getEndingTime(pend);

    if (pend.afterDateTime(end))
    {
        pNew = new Period(); PR_ASSERT(pNew != 0);
        pNew->setStart(end);
        pNew->setEnd(pend);
        out->Add(pNew);
    }
}

//---------------------------------------------------------------------

//---------------------------------------------------------------------

t_bool Period::IsParseable(UnicodeString & s)
{
    UnicodeString sStart, sEnd;
    t_int32 indexOfSlash = s.indexOf('/', 0);

    if (indexOfSlash < 0)
        return FALSE;
    else
    {
        sStart = s.extractBetween(0, indexOfSlash, sStart);
        sEnd = s.extractBetween(indexOfSlash + 1, s.size(), sEnd);
        
        if (!DateTime::IsParseable(sStart))
            return FALSE;
        if (!DateTime::IsParseable(sEnd))
        {
            Duration d(sEnd); //PR_ASSERT(d != 0);
            if (!d.isValid())
                return FALSE;
            //delete d; d = 0; // nullify after delete
        }
    }   
    return TRUE;
}

//---------------------------------------------------------------------

UnicodeString & 
Period::getTimeString(UnicodeString & sPeriod, t_bool bStart,
                      UnicodeString & out)
{
    PR_ASSERT(IsParseable(sPeriod));
    if (!IsParseable(sPeriod))
    {
        out = "19691231T235959Z/PT-1H";
        return out;
    }
    t_int32 index = sPeriod.indexOf('/');
    PR_ASSERT(index != -1);
    if (index == -1)
    {
        out = "19691231T235959Z/PT-1H";
        return out;
    }

    if (bStart)
    {
        return sPeriod.extractBetween(0, index, out);
    }
    else
        return sPeriod.extractBetween(index + 1, sPeriod.size(), out);
}
//---------------------------------------------------------------------

UnicodeString &
Period::getStartTimeString(UnicodeString & sPeriod, UnicodeString & out)
{
    return getTimeString(sPeriod, TRUE, out);
}

//---------------------------------------------------------------------

UnicodeString &
Period::getEndTimeString(UnicodeString & sPeriod, UnicodeString & out)
{
    return getTimeString(sPeriod, FALSE, out);
}

//---------------------------------------------------------------------

int Period::ComparePeriods(const void * a, const void * b)
{
    DateTime startA, startB;
    PR_ASSERT(a != 0 && b != 0);

    Period * dtA = *(Period **) a;
    Period * dtB = *(Period **) b;
    
    startA = dtA->getStart();
    startB = dtB->getStart();
    return (int) startA.compareTo(startB);
    //return (int) (DateTime::CompareDateTimes(&startA, &startB));
}

//---------------------------------------------------------------------

int Period::ComparePeriodsByEndTime(const void * a, const void * b)
{
    DateTime endA, endB;
    PR_ASSERT(a != 0 && b != 0);

    Period * dtA = *(Period **) a;
    Period * dtB = *(Period **) b;
    
    endA = dtA->getEndingTime(endA);
    endB = dtB->getEndingTime(endB);
    return (int) endA.compareTo(endB);
    //return (int) (DateTime::CompareDateTimes(&startA, &startB));
}

//---------------------------------------------------------------------

