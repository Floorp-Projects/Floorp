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
// rcrrence.cpp
// John Sun
// 10:45 AM Febuary 2 1997

// TODO: Known bugs in recurrence handling:
// 1) Given a rule WITH a BYMONTH tag and no BYDAY, BYMONTHDAY, BYYEARDAY tag
// I should use dtstart as BYMONTHDAY
// i.e. RRULE:FREQ=YEARLY;COUNT=10;BYMONTH=6,7 -> 6/7/97,7/7/97,6/7/98,7/7/98,6/7/99,7/7/99, etc.
// NOT  6/1-31/97,7/1-31/97,6/1-31/98,7/1-31/98,6/1-31/99,7/1-31/99, etc. as it does now.
// 2) UNTIL BUG
// generator aborts if date gets past until.  However some dates may not be produced
// that are actually before certain produced dates.
// 3) BYSETPOS
// start dates affect bySETPOS' first value.  This is bad.
// (i.e FREQ=MONTHLY;BYDAY=TU,WE,TH;BYSETPOS=3 with start of 19970904T090000)
// should not produce 9/10, which current code does
// 4) I DON'T HANDLE BYSECOND. or FREQ=SECONDLY
// 5) DON'T HANDLE TZID yet!
// 7) FREQ tag that is lower than span value of BYxxx TAG fail
//  (i.e FREQ=MINUTELY;INTERVAL=20;BYHOUR=9,10,11,12,13,14,15,16 FAILS)

#include "stdafx.h"
#include "jdefines.h"

#include <unistring.h>
#include <hashtab.h>
#include "datetime.h"
#include "duration.h"
#include "rrday.h"
#include "ptrarray.h"
#include "jutility.h"
#include "bydmgntr.h"
#include "bydwgntr.h"
#include "bydygntr.h"
#include "byhgntr.h"
#include "bymgntr.h"
#include "bymdgntr.h"
#include "bymogntr.h"
#include "bywngntr.h"
#include "byydgntr.h"
#include "deftgntr.h"
#include "dategntr.h"
#include "keyword.h"
#include "period.h"
#include "datetime.h"
#include "rcrrence.h"
#include "unistrto.h"
#include "keyword.h"
#include "icalprm.h"
#include "jlog.h"

//#define DEBUG 1
//---------------------------------------------------------------------
//static UnicodeString s_sSemiColonSymbol = ";";
//static UnicodeString JulianKeyword::Instance()->ms_sCOMMA_SYMBOL = ",";

/*
 *  This is a hack so I could get it compiled on unix.  We need
 *  to remove Windows specific code from this file.
 *  -terry (copying sman's hack)
 */
#define TRACE printf

void Recurrence::TRACE_DATETIMEVECTOR(JulianPtrArray * x, char * name)
{
    //if (FALSE) TRACE("Name: %s,", name);
    if (x != 0)
    {
        //if (FALSE) TRACE("Size: %d", x->GetSize());
        t_int32 ii;
        for (ii = 0; ii < x->GetSize(); ii++)
        {
            //if (FALSE) TRACE("\r\n %s[%d] = %s", name, ii, ((DateTime *)x->GetAt(ii))->toISO8601().toCString(""));
        }   
        //if (FALSE) TRACE("\r\n");
    }
    else
    {
        //if (FALSE) TRACE(" is NULL\r\n");
    }
};
// prints a vector of vector of datetimes
void Recurrence::TRACE_DATETIMEVECTORVECTOR(JulianPtrArray * x, char * name)
{
    //if (FALSE) TRACE("Name: %s,", name);
    JulianPtrArray * xi;
    if (x != 0)
    {
        //if (FALSE) TRACE("Size: %d", x->GetSize());
        t_int32 ii;
        t_int32 jj;
        for (ii = 0; ii < x->GetSize(); ii++)
        {
            xi = (JulianPtrArray *) x->GetAt(ii);
            for (jj = 0; jj < xi->GetSize(); jj++)
            {
                //if (FALSE) TRACE("\r\n %s[%d][%d] = %s", name, ii, jj,((DateTime *)xi->GetAt(jj))->toISO8601().toCString(""));
            }
        }   
        //if (FALSE) RACE("\r\n");
    }
    else
    {
        //if (FALSE) TRACE(" is NULL\r\n");
    }
}

const t_int32 Recurrence::ms_iUNSET = -10000; // should be set to Integer.MIN

const t_int32 Recurrence::ms_iByMinuteGntrIndex = 0;
const t_int32 Recurrence::ms_iByHourGntrIndex = 1;
const t_int32 Recurrence::ms_iByWeekNoGntrIndex = 2;
const t_int32 Recurrence::ms_iByYearDayGntrIndex = 3;
const t_int32 Recurrence::ms_iByMonthGntrIndex = 4;
const t_int32 Recurrence::ms_iByMonthDayGntrIndex = 5;
const t_int32 Recurrence::ms_iByDayYearlyGntrIndex = 6;
const t_int32 Recurrence::ms_iByDayMonthlyGntrIndex = 7;
const t_int32 Recurrence::ms_iByDayWeeklyGntrIndex = 8;
const t_int32 Recurrence::ms_iDefaultGntrIndex = 9;
const t_int32 Recurrence::ms_iGntrSize = 10;

const t_int32 Recurrence::ms_aiGenOrderZero[] =  
{
    Recurrence::ms_iByWeekNoGntrIndex, 
    Recurrence::ms_iByYearDayGntrIndex, 
    Recurrence::ms_iByMonthGntrIndex, 
    Recurrence::ms_iByDayYearlyGntrIndex

    /*
    Recurrence::ms_iByMonthGntrIndex, 
    Recurrence::ms_iByWeekNoGntrIndex, 
    Recurrence::ms_iByYearDayGntrIndex, 
    Recurrence::ms_iByDayYearlyGntrIndex*/
};
const t_int32 Recurrence::ms_GenOrderZeroLen = 4;
const t_int32 Recurrence::ms_aiGenOrderOne[] =  
{
    Recurrence::ms_iByMonthDayGntrIndex, 
    Recurrence::ms_iByDayMonthlyGntrIndex
};
const t_int32 Recurrence::ms_GenOrderOneLen = 2;

const t_int32 Recurrence::ms_aiGenOrderTwo[] ={Recurrence::ms_iByDayWeeklyGntrIndex};
const t_int32 Recurrence::ms_GenOrderTwoLen = 1;
const t_int32 Recurrence::ms_aiGenOrderThree[] = {Recurrence::ms_iByHourGntrIndex};
const t_int32 Recurrence::ms_GenOrderThreeLen = 1;
const t_int32 Recurrence::ms_aiGenOrderFour[] = {Recurrence::ms_iByMinuteGntrIndex};
const t_int32 Recurrence::ms_GenOrderFourLen = 1;
const t_int32 Recurrence::ms_aiGenOrderFive[] = {Recurrence::ms_iDefaultGntrIndex};  
const t_int32 Recurrence::ms_GenOrderFiveLen = 1;

const t_int32 * Recurrence::ms_aaiGenOrder[] = 
{
    Recurrence::ms_aiGenOrderZero,
    Recurrence::ms_aiGenOrderOne,
    Recurrence::ms_aiGenOrderTwo,
    Recurrence::ms_aiGenOrderThree,
    Recurrence::ms_aiGenOrderFour,
    Recurrence::ms_aiGenOrderFive,
};
const t_int32 Recurrence::ms_GenOrderLen = 6;

//---------------------------------------------------------------------

void Recurrence::init()
{
    m_iType = JulianUtility::RT_NONE;
    m_Interval = 0;
    m_iCount = ms_iUNSET;
    m_iWkSt = Calendar::MONDAY;
    m_Until.setTime(-1);
    m_iBySetPosLen = 0;

    m_aaiByDay = 0;
    m_aiByMinute = 0;
    m_aiByHour = 0;
    m_aiByMonthDay = 0;
    m_aiByYearDay = 0;
    m_aiBySetPos = 0;
    m_aiByWeekNo = 0;
    m_aiByMonth = 0;

    m_GntrVctr = new JulianPtrArray(); PR_ASSERT(m_GntrVctr != 0);
    
    ms_ByMinuteGntr = new ByMinuteGenerator(); PR_ASSERT(ms_ByMinuteGntr != 0); 
    ms_ByHourGntr = new ByHourGenerator(); PR_ASSERT(ms_ByHourGntr != 0);
    ms_ByWeekNoGntr = new ByWeekNoGenerator(); PR_ASSERT(ms_ByWeekNoGntr != 0);
    ms_ByYearDayGntr = new ByYearDayGenerator(); PR_ASSERT(ms_ByYearDayGntr != 0);
    ms_ByMonthGntr = new ByMonthGenerator(); PR_ASSERT(ms_ByMonthGntr != 0);
    ms_ByMonthDayGntr = new ByMonthDayGenerator(); PR_ASSERT(ms_ByMonthDayGntr != 0);
    ms_ByDayYearlyGntr = new ByDayYearlyGenerator(); PR_ASSERT(ms_ByDayYearlyGntr != 0);
    ms_ByDayMonthlyGntr = new ByDayMonthlyGenerator(); PR_ASSERT(ms_ByDayMonthlyGntr != 0);
    ms_ByDayWeeklyGntr = new ByDayWeeklyGenerator(); PR_ASSERT(ms_ByDayWeeklyGntr != 0);
    ms_DefaultGntr = new DefaultGenerator(); PR_ASSERT(ms_DefaultGntr != 0);


    // MUST BE IN THIS ORDER!!
    m_GntrVctr->Add(ms_ByMinuteGntr);
    m_GntrVctr->Add(ms_ByHourGntr);
    m_GntrVctr->Add(ms_ByWeekNoGntr);
    m_GntrVctr->Add(ms_ByYearDayGntr);
    m_GntrVctr->Add(ms_ByMonthGntr);
    m_GntrVctr->Add(ms_ByMonthDayGntr);
    m_GntrVctr->Add(ms_ByDayYearlyGntr);
    m_GntrVctr->Add(ms_ByDayMonthlyGntr);
    m_GntrVctr->Add(ms_ByDayWeeklyGntr);
    m_GntrVctr->Add(ms_DefaultGntr);

    m_iActiveGenerators = 0;
    
}
//---------------------------------------------------------------------

Recurrence::Recurrence()
{
    init();
}

//---------------------------------------------------------------------

Recurrence::Recurrence(DateTime startDate, DateTime stopDate, 
                       Duration * duration, UnicodeString & ruleString)
{
    init();
    m_StartDate = startDate;
    m_StopDate = stopDate;
    m_Duration = duration;
    
    m_bParseValid = parse(ruleString);

}
//---------------------------------------------------------------------

Recurrence::Recurrence(DateTime startDate, Duration * duration, 
                       UnicodeString & ruleString)
{
    init();
    m_StartDate = startDate;
    m_Duration = duration;

    m_StopDate = startDate;
    m_StopDate.add(*duration);
    
    m_bParseValid = parse(ruleString);
}

//---------------------------------------------------------------------

Recurrence::Recurrence(DateTime startDate, UnicodeString & ruleString)
{
    init();
    m_StartDate = startDate;
    m_StopDate.setTime(-1);
    m_Duration = 0;

    m_bParseValid = parse(ruleString);
}
//---------------------------------------------------------------------

Recurrence::~Recurrence()
{
    // delete, generators, then delete parameters.

    t_int32 i;
    for (i = m_GntrVctr->GetSize() - 1; i >= 0; i--)
    {
        delete ((DateGenerator *)m_GntrVctr->GetAt(i));
    }
    m_GntrVctr->RemoveAll();

    if (m_aaiByDay != 0) { delete [] m_aaiByDay; m_aaiByDay = 0; }
    if (m_aiByMinute != 0) { delete [] m_aiByMinute; m_aiByMinute = 0; }
    if (m_aiByHour != 0) { delete [] m_aiByHour ; m_aiByHour = 0; }
    if (m_aiByMonthDay != 0) { delete [] m_aiByMonthDay; m_aiByMonthDay = 0; }
    if (m_aiByYearDay != 0) { delete [] m_aiByYearDay ; m_aiByYearDay = 0; }
    if (m_aiBySetPos != 0) { delete [] m_aiBySetPos ; m_aiBySetPos = 0; }
    if (m_aiByWeekNo != 0) { delete [] m_aiByWeekNo ; m_aiByWeekNo = 0; }
    if (m_aiByMonth != 0) { delete [] m_aiByMonth ; m_aiByMonth = 0; }

    if (m_GntrVctr != 0) { delete m_GntrVctr; m_GntrVctr = 0; }
    if (m_Interval != 0) { delete m_Interval; m_Interval = 0; }
    if (m_Duration != 0) { delete m_Duration; m_Duration = 0; }
}

//---------------------------------------------------------------------

t_bool Recurrence::isValid() const
{
    return m_bParseValid;
}

//---------------------------------------------------------------------

void 
Recurrence::parsePropertyLine(UnicodeString & strLine,
                              JulianPtrArray * parameters)
                              //Hashtable * properties)
{
    t_int32 iColon = strLine.indexOf(':');
    if (iColon < 0)
    {
        return;
    }
    else 
    {   
        ErrorCode status = ZERO_ERROR;
        UnicodeString u;

        u = strLine.extractBetween(iColon + 1, strLine.size(), u);

        UnicodeStringTokenizer * st = 
            new UnicodeStringTokenizer(u, JulianKeyword::Instance()->ms_sSEMICOLON_SYMBOL);
        
        PR_ASSERT(st != 0);

        UnicodeString paramName;
        UnicodeString paramVal;
        t_int32 iEq = -1;
            
        while (st->hasMoreTokens())
        {
            u = st->nextToken(u, status);
            u.trim();        

#if DEBUG
            //if (FALSE) TRACE("u = %s\r\n", u.toCString(""));
#endif
            iEq = u.indexOf('=');
            if (iEq < 0)
            {
                paramName = u.extractBetween(0, u.size(), paramName).toUpper();
                paramVal = "";
            }
            else
            {
                paramName = u.extractBetween(0, iEq, paramName).toUpper();
                paramVal = u.extractBetween(iEq + 1, u.size(), paramVal);
            }
#if DEBUG
            //if (FALSE) TRACE("[paramName | paramVal] = [%s | %s]\r\n", paramName.toCString(""), paramVal.toCString(""));
#endif
            parameters->Add(new ICalParameter(paramName, paramVal));
        }
        delete st; st = 0;
    }
}
//---------------------------------------------------------------------
// TODO: crash proof
t_bool
Recurrence::parse(UnicodeString & s)

{
    t_bool bParseError = FALSE;

    DateGenerator * dg;
    JulianPtrArray * v;

    UnicodeString sErrorMsg = "InvalidByMinuteValue";

    UnicodeString t, u;
    t_int32 tempInterval = ms_iUNSET;
    t_int32 iYearSpanConflict = 0;
    t_bool bYearDay = FALSE;
    t_bool bWeekNo = FALSE;
    t_bool bByMonth = FALSE;
    t_int32 i, j;
    t_int32 retSize;
    t_int32 byMonthDaySize = 0;
    t_int32 byMonthSize = 0;
    t_int32 temp = 0;

    // PARSES THE LINE AND FILLS IN PARAMETERS Vector
    JulianPtrArray * parameters = new JulianPtrArray(); PR_ASSERT(parameters != 0);
    if (parameters == 0)
    {
        // Ran out of memory, return FALSE.
        return FALSE;
    }
    Recurrence::parsePropertyLine(s, parameters);

#if DEBUG
    //if (FALSE) TRACE("\r\n----------------------------------------------------\r\n");
#endif    
    
    // First get FREQ value
    ICalParameter * ip;
    t_bool hasFreq = FALSE;
    for (i = 0; i < parameters->GetSize(); i++)
    {
        ip = (ICalParameter *) parameters->GetAt(i);
        if (ip->getParameterName(u).compareIgnoreCase(JulianKeyword::Instance()->ms_sFREQ) == 0)
        {
            t = ip->getParameterValue(t);
#if DEBUG
            //if (FALSE) TRACE("FREQ = %s\r\n", t.toCString(""));
#endif
            m_iType = stringToType(t, bParseError);
            hasFreq = TRUE;
        }
    }

    if (bParseError || !hasFreq)
    {
        // Parse Error: Invalid or Missing Frequency Parameter
        // CLEANUP 
        deleteICalParameterVector(parameters);
        delete parameters;
        return FALSE;
    }


    v = new JulianPtrArray(); PR_ASSERT(v != 0);

    for (i = 0; i < parameters->GetSize(); i++)
    {
        ip = (ICalParameter *) parameters->GetAt(i);
        u = ip->getParameterName(u);
        t = ip->getParameterValue(t);
        
        if (t.size() == 0)
        {

        }
        else
        {
#if DEBUG
            //if (FALSE) TRACE("(key | value) = (%s | %s)\r\n", u.toCString(""), t.toCString(""));
#endif
            //const char * tCharStar = t.toCString("");
            t_int32 tSize = t.size();
            JAtom atomParam(u);
            
            if (JulianKeyword::Instance()->ms_ATOM_UNTIL == atomParam)
            {
                // TODO: ONLY accepts UTC time for now
                m_Until.setTimeString(t);
                // TODO: Log if m_Until is invalid or before start time
            }
            else if (JulianKeyword::Instance()->ms_ATOM_COUNT == atomParam)
            {
                m_iCount = JulianUtility::atot_int32(t.toCString(""), bParseError, tSize);
            }
            else if (JulianKeyword::Instance()->ms_ATOM_INTERVAL == atomParam)
            {
                // TODO: if duration allowed, parse duration, then it to interval
                tempInterval = JulianUtility::atot_int32(t.toCString(""), bParseError, tSize);
            }
            else if (JulianKeyword::Instance()->ms_ATOM_WKST == atomParam)
            {
                m_iWkSt = stringToDay(t, bParseError);
            }
            else if (JulianKeyword::Instance()->ms_ATOM_BYSETPOS == atomParam)
            {
                parseDelimited(t, JulianKeyword::Instance()->ms_sCOMMA_SYMBOL, v);
                m_aiBySetPos = verifyIntList(v, -366, 366, sErrorMsg, retSize, bParseError, TRUE);
                m_iBySetPosLen = retSize;
                m_iActiveGenerators++;
            }
            else if (JulianKeyword::Instance()->ms_ATOM_BYMINUTE == atomParam)
            {
                dg = (DateGenerator *) m_GntrVctr->GetAt(ms_iByMinuteGntrIndex);
                if (dg->getSpan() < m_iType)
                {
                    // error FIMR (FreqIntervalMismatchRecurrence)
                    bParseError = TRUE;
                }
                else
                {
                    parseDelimited(t, JulianKeyword::Instance()->ms_sCOMMA_SYMBOL, v);
                    m_aiByMinute = verifyIntList(v, 0, 59, sErrorMsg, retSize, bParseError);
                    dg->setParamsArray(m_aiByMinute, retSize);
                    m_iActiveGenerators++;
                }
            }
            else if (JulianKeyword::Instance()->ms_ATOM_BYHOUR == atomParam)
            {
                dg = (DateGenerator *) m_GntrVctr->GetAt(ms_iByHourGntrIndex);
                if (dg->getSpan() < m_iType)
                {
                    // error FIMR (FreqIntervalMismatchRecurrence)
                    bParseError = TRUE;
                }
                else
                {
                    parseDelimited(t, JulianKeyword::Instance()->ms_sCOMMA_SYMBOL, v);
                    m_aiByHour = verifyIntList(v, 0, 23, sErrorMsg, retSize, bParseError);
                    dg->setParamsArray(m_aiByHour, retSize);
                    m_iActiveGenerators++;
                }
            }
            else if (JulianKeyword::Instance()->ms_ATOM_BYMONTHDAY == atomParam)
            {
                dg = (DateGenerator *) m_GntrVctr->GetAt(ms_iByMonthDayGntrIndex);
                if (dg->getSpan() < m_iType)
                {
                    // error FIMR (FreqIntervalMismatchRecurrence)
                    bParseError = TRUE;
                }
                else
                {
                    parseDelimited(t, JulianKeyword::Instance()->ms_sCOMMA_SYMBOL, v);
                    m_aiByMonthDay = verifyIntList(v, -31, 31, sErrorMsg, retSize, bParseError, TRUE);
                    dg->setParamsArray(m_aiByMonthDay, retSize);
                    m_iActiveGenerators++;
                    byMonthDaySize = retSize;
                }
            }
            else if (JulianKeyword::Instance()->ms_ATOM_BYYEARDAY == atomParam)
            {
                dg = (DateGenerator *) m_GntrVctr->GetAt(ms_iByYearDayGntrIndex);
                if (dg->getSpan() < m_iType)
                {
                    // error FIMR (FreqIntervalMismatchRecurrence)
                    bParseError = TRUE;
                }
                else
                {
                    parseDelimited(t, JulianKeyword::Instance()->ms_sCOMMA_SYMBOL, v);
                    m_aiByYearDay = verifyIntList(v, -366, 366, sErrorMsg, retSize, bParseError, TRUE);
                    dg->setParamsArray(m_aiByYearDay, retSize);
                    m_iActiveGenerators++;
                    iYearSpanConflict++;
                    bYearDay = TRUE;
                }
            }
            else if (JulianKeyword::Instance()->ms_ATOM_BYWEEKNO == atomParam)
            {
                dg = (DateGenerator *) m_GntrVctr->GetAt(ms_iByWeekNoGntrIndex);
                if (dg->getSpan() < m_iType)
                {
                    // error FIMR (FreqIntervalMismatchRecurrence)
                    bParseError = TRUE;
                }
                else
                {
                    parseDelimited(t, JulianKeyword::Instance()->ms_sCOMMA_SYMBOL, v);
                    m_aiByWeekNo = verifyIntList(v, -53, 53, sErrorMsg, retSize, bParseError, TRUE);
                    dg->setParamsArray(m_aiByWeekNo, retSize);
                    m_iActiveGenerators++;
                    iYearSpanConflict++;
                    bWeekNo = TRUE;
                }
            }
            else if (JulianKeyword::Instance()->ms_ATOM_BYMONTH == atomParam)
            {
                dg = (DateGenerator *) m_GntrVctr->GetAt(ms_iByMonthGntrIndex);
                if (dg->getSpan() < m_iType)
                {
                    // error FIMR (FreqIntervalMismatchRecurrence)
                    bParseError = TRUE;
                }
                else
                {
                    parseDelimited(t, JulianKeyword::Instance()->ms_sCOMMA_SYMBOL, v);
                    m_aiByMonth = verifyIntList(v, 1, 12, sErrorMsg, retSize, bParseError, TRUE);
                    dg->setParamsArray(m_aiByMonth, retSize);
                    m_iActiveGenerators++;
                    iYearSpanConflict++;
                    bByMonth = TRUE;
                    byMonthSize = retSize;
                }
            }
            else if (JulianKeyword::Instance()->ms_ATOM_BYDAY == atomParam)
            {
                switch(m_iType)
                {
                case JulianUtility::RT_WEEKLY:
                    dg = (DateGenerator *) m_GntrVctr->GetAt(ms_iByDayWeeklyGntrIndex);
                    break;
                case JulianUtility::RT_MONTHLY:
                    dg = (DateGenerator *) m_GntrVctr->GetAt(ms_iByDayMonthlyGntrIndex);
                    break;
                case JulianUtility::RT_YEARLY:
                    dg = (DateGenerator *) m_GntrVctr->GetAt(ms_iByDayYearlyGntrIndex);
                    break;
                default:
                    bParseError = TRUE;
                    //return FALSE;
                }
                PR_ASSERT(dg != 0);
                if (dg->getSpan() < m_iType)
                {
                    bParseError = TRUE;
                }
                else
                {
                    parseDelimited(t, JulianKeyword::Instance()->ms_sCOMMA_SYMBOL, v);
                    m_aaiByDay = createByDayList(v, m_iType, retSize, bParseError);
                    dg->setRDay(m_aaiByDay, retSize);
                    m_iActiveGenerators++;
                }
            }
            
        }
        deleteUnicodeStringVector(v);
        v->RemoveAll();
    }
    //delete propEnum; propEnum = 0;

    // Check BYMONTH, BYMONTHDAY mismatch (i.e. BYMONTH=11,BYMONTHDAY=-31 or 31 is error)
    // NOTE: TODO: Doesn't account for leap years!!!!
    // THUS: (asking for BYMONTH=2,BYMONTHDAY=29 will succeed, even for bad years!!!)
    if (m_aiByMonth != 0 && m_aiByMonthDay != 0 && !bParseError)
    {
        for (i = 0; i < byMonthSize; i++)
        {
            for (j = 0; j < byMonthDaySize; j++)
            {
                // NOTE: to fix HPUX compile problems with abs
                temp = m_aiByMonthDay[j];
                if (temp < 0)
                    temp = 0 - temp;

                if (DateTime::getMonthLength(m_aiByMonth[i], 2000) < 
                    //abs(m_aiByMonthDay[j]) // this line is replaced by below
                    temp)
                {
                    bParseError = TRUE;
                    break;
                }
            }
        }
    }

    if (tempInterval == ms_iUNSET)
    {
        internalSetInterval(1);
    }
    else
    {
        internalSetInterval(tempInterval);
    }
    // Default Generator
    // If there are no active generators, we have no BYxxx tags, so we must install a
    // default generator to do the work.
    if (m_iActiveGenerators == 0)
    {
        // NOTE: to make it work on HPUX.
        t_int32 params[3];
        params[0] = m_iType;
        if (tempInterval == ms_iUNSET)
            params[1] = 1;
        else
            params[1] = tempInterval;
        params[2] = m_iCount;
        
        // commented out line below so HPUX won't die.
        //t_int32 params[] = {m_iType, (tempInterval == ms_iUNSET) ? 1 : tempInterval, m_iCount};

        ((DateGenerator *) m_GntrVctr->GetAt(ms_iDefaultGntrIndex))->setParamsArray(params, 3);
        m_iActiveGenerators++;
    }

    // TODO: IF there is more than one BY tag with a yearly span,
    // make sure they don't conflict.
    // If they do, then only the start date should be returned
   
    
    /*
    if (iYearSpanConflict > 1) 
    {
    
        if (bByMonth && bYearDay) 
        {
        }

        if (bByMonth && bWeekNo) 
        {
        }

        if (bYearDay && bWeekNo) 
        {
        }
    }
    */

    if((dg = (DateGenerator *)m_GntrVctr->GetAt(ms_iByDayWeeklyGntrIndex))->active() ||
       (dg = (DateGenerator *)m_GntrVctr->GetAt(ms_iByDayMonthlyGntrIndex))->active() ||
       (dg = (DateGenerator *)m_GntrVctr->GetAt(ms_iByDayYearlyGntrIndex))->active())
    {
        dg->setParams(m_iWkSt);
    }

    // CLEANUP 
    deleteUnicodeStringVector(v);
    v->RemoveAll();
    delete v; v = 0;
    
    deleteICalParameterVector(parameters);
    delete parameters; parameters = 0;

    if (bParseError)
    {
        // PARSE ERROR ENCOUNTERED
#if DEBUG
        //if (FALSE) TRACE("ERROR: Parse error somewhere\r\n");
#endif
        return FALSE;
    }
    
    return TRUE;

}

//---------------------------------------------------------------------

void Recurrence::internalSetInterval(t_int32 i)
{
    m_Interval = new Duration();

    switch(m_iType)
    {
    case JulianUtility::RT_MINUTELY:
        m_Interval->setMinute(i);
        break;
    case JulianUtility::RT_HOURLY:
        m_Interval->setHour(i);
        break;
    case JulianUtility::RT_DAILY:
        m_Interval->setDay(i);
        break;
    case JulianUtility::RT_WEEKLY:
        m_Interval->setWeek(i);
        break;
    case JulianUtility::RT_MONTHLY:
        m_Interval->setMonth(i);
        break;
    case JulianUtility::RT_YEARLY:
        m_Interval->setYear(i);
        break;
    default:
        break;
    }
}
//---------------------------------------------------------------------
// TODO: crash proof
void
Recurrence::stringEnforce(DateTime startDate, JulianPtrArray * srr, 
                          JulianPtrArray * ser, JulianPtrArray * srd, 
                          JulianPtrArray * sed, t_int32 bound, JulianPtrArray * out,
                          JLog * log)
{
    // rr and er are vector of Recurrence objects
    // rd and ed are vector of DateTime objects

    // Vector of Recurrence objects
    JulianPtrArray * rr = new JulianPtrArray(); PR_ASSERT(rr != 0);
    JulianPtrArray * er = new JulianPtrArray(); PR_ASSERT(er != 0);
    
    // Vector of DateTimes
    JulianPtrArray * rd = new JulianPtrArray(); PR_ASSERT(rd != 0);
    JulianPtrArray * ed = new JulianPtrArray(); PR_ASSERT(ed != 0);
    
    t_int32 i = 0, size = 0;
    UnicodeString u, t;
    DateTime * dt;

    size = ((srr != 0) ? srr->GetSize() : 0);
    if (size > 0)
    {
        for (i = 0; i < size; i++)
        {
            u = *((UnicodeString *) srr->GetAt(i));
#if DEBUG
            //if (FALSE) TRACE("u = -%s-\r\n", u.toCString(""));
#endif
            rr->Add(new Recurrence(startDate, u));
        }
    }

    size = ((ser != 0) ? ser->GetSize() : 0);
    if (size > 0)
    {
        for (i = 0; i < size; i++)
        {
            u = *((UnicodeString *) ser->GetAt(i));
#if DEBUG
            //if (FALSE) TRACE("u = -%s-\r\n", u.toCString(""));
#endif
            er->Add(new Recurrence(startDate, u));
        }
    }

    size = ((srd != 0) ? srd->GetSize() : 0);
    if (size > 0)
    {
        for (i = 0; i < size; i++)
        {
            u = * ((UnicodeString *) srd->GetAt(i));
#if DEBUG
            //if (FALSE) TRACE("u = -%s-\r\n", u.toCString(""));
#endif   
            t = u;
            if (Period::IsParseable(u))
            {
                Period * p = new Period(u); PR_ASSERT(p != 0);
                if (p != 0)
                {
                    PR_ASSERT(p->isValid());
                    DateTime d;
                    d = p->getStart();
                    // create new period
                    // add the start time of the period to vector only if after start time
                    if (!(d.beforeDateTime(startDate)))
                        rd->Add(new DateTime(d));
                    delete p; p = 0;
                }
            }
            else if (DateTime::IsParseableDate(u))
            {
                t += 'T';
                t += startDate.toISO8601LocalTimeOnly();
#if DEBUG
                //if (FALSE) TRACE("t = -%s-\r\n", t.toCString(""));
#endif       
                dt = new DateTime(t); PR_ASSERT(dt != 0);
                if (!(dt->beforeDateTime(startDate)))
                    rd->Add(dt);
            }
            else if (DateTime::IsParseableDateTime(u))
            {
                dt = new DateTime(t); PR_ASSERT(dt != 0);
                if (!(dt->beforeDateTime(startDate)))
                    rd->Add(dt);
            }
            else
            {
                // log an invalid rdate
                if (log != 0) log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidRDate, 200);
            }
        }
    }
    size = ((sed != 0) ? sed->GetSize() : 0);
    if (size > 0)
    {
        for (i = 0; i < size; i++)
        {
            u = * ((UnicodeString *) sed->GetAt(i));
#if DEBUG
            //if (FALSE) TRACE("u = -%s-\r\n", u.toCString(""));
#endif   
            ed->Add(new DateTime(u));
        }
    }
    // NOTE: rd, ed ownership taken in enforce
    enforce(rr, er, rd, ed, bound, out, log);

    // CLEANUP: NOTE: rd, ed are deleted in enforce already
    deleteRecurrenceVector(rr);
    delete rr; rr = 0;
    deleteRecurrenceVector(er);
    delete er; er = 0;
}

//---------------------------------------------------------------------
// TODO: crash proof
void
Recurrence::enforce(JulianPtrArray * rr, JulianPtrArray * er, 
                    JulianPtrArray * rd, JulianPtrArray * ed,
                    t_int32 bound, JulianPtrArray * out, JLog * log)
{
    JulianPtrArray * dr = new JulianPtrArray(); // dr is a vector of vectors
    PR_ASSERT(dr != 0);

    JulianPtrArray * de = new JulianPtrArray(); // de is a vector of vectors
    PR_ASSERT(de != 0);

    JulianPtrArray * dates;

    Recurrence * recur;
    t_int32 i;

    PR_ASSERT(rr != 0 && er != 0 && rd != 0 && ed != 0);

    // First, create all dates with instances 
    for (i = 0; i < rr->GetSize(); i++) 
    {
        dates = new JulianPtrArray(); PR_ASSERT(dates != 0);
        recur = (Recurrence *) rr->GetAt(i);
        recur->unzip(bound, dates, log);
        dr->Add(dates);
#if DEBUG
        TRACE_DATETIMEVECTOR(dates, "dates");
#endif
    }
    dr->Add(rd);

    // SECOND, create all exception dates
    for (i = 0; i < er->GetSize(); i++) 
    {
        dates = new JulianPtrArray(); PR_ASSERT(dates != 0);
        recur = (Recurrence *) er->GetAt(i);
        recur->unzip(bound, dates, log);
        de->Add(dates);
#if DEBUG
        TRACE_DATETIMEVECTOR(dates, "dates");
#endif
    }
    de->Add(ed);

    // Now, flatten out dates, exdates
    JulianPtrArray * sZero = new JulianPtrArray();
    JulianPtrArray * sOne = new JulianPtrArray();

    concat(dr, sZero);
    concat(de, sOne);
    
    // eliminate duplicates from dates, exdates
    eliminateDuplicates(sZero);
    eliminateDuplicates(sOne);

    // intersect dates with exdates, add them to out vector
    JulianPtrArray * s = new JulianPtrArray();
    s->Add(sZero);
    s->Add(sOne);
    intersectDateList(s, TRUE, out);

    // cleanup
    deleteDateTimeVectorVector(dr); // deletes rd 
    delete dr; dr = 0;
    deleteDateTimeVectorVector(de); // deleted ed 
    delete de; de = 0;

    deleteDateTimeVectorVector(s); // should delete sZero, and sOne
    delete s; s = 0;
}

//---------------------------------------------------------------------

t_int32 Recurrence::getGenOrderIndexLength(t_int32 genOrderIndex)
{
     switch (genOrderIndex)
     {
         case 0: return ms_GenOrderZeroLen; 
         case 1: return ms_GenOrderOneLen; 
         case 2: return ms_GenOrderTwoLen; 
         case 3: return ms_GenOrderThreeLen; 
         case 4: return ms_GenOrderFourLen; 
         case 5: return ms_GenOrderFiveLen; 
         default: return 0;
     }
}

//---------------------------------------------------------------------
// TODO: make crash proof
void
Recurrence::unzip(t_int32 bound, JulianPtrArray * out, JLog * log)
{
    //PR_ASSERT(out != 0);
    //PR_ASSERT(isValid());
    // return if only startDate if !isValid
    if (!isValid())
    {
        if (out != 0)
        {
            out->Add(new DateTime(m_StartDate));
        }
        if (log != 0)
        {
            log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidRecurrenceError, 200);
        }
        return;
    }

    // u = returnDates, z = genDates,

    t_int32 i, j, k, temp;
    t_int32 lastInterval = m_iType;
    DateGenerator * dg;
    JulianPtrArray * v; // vector of vector of dates for this span type, deleted
    JulianPtrArray * genDates; // vector of newly generated dates, deleted with v
    t_bool done = FALSE;
    DateTime t = m_StartDate;
    DateTime * dt;
    t_bool bSubGenerate = FALSE;

    // TODO: Known bug in handling UNTIL, if generating dates with UNTIL,
    // it's possible that the generator exists the generate() method before
    // generating all dates before until.

    t_int32 genOrderIndexLength = 0;

    JulianPtrArray * x = 0; // vector of dates for this interval, deleted
    JulianPtrArray * returnDateVector = 0; // vector of dates to return, deleted

    if (m_iCount != ms_iUNSET && (!m_Until.isValid()))
        bound = m_iCount;

    // TODO: prevent generator from running if previous generator ran with
    // no dates generated
    t_bool bPrevGenRan = FALSE;

    // If the default generator is active, twiddle it with the new count value. 

    if((dg = (DateGenerator *)m_GntrVctr->GetAt(ms_iDefaultGntrIndex))->active())
    {
        dg->setParams(bound);
    }

    do
    {
        x = new JulianPtrArray(); PR_ASSERT(x != 0);
        //bPrevGenRan = FALSE;
        for (i = 0; i < ms_GenOrderLen; i++)
        {
            v = new JulianPtrArray();

            genOrderIndexLength = getGenOrderIndexLength(i);

            for (j = 0; j < genOrderIndexLength; j++)
            {
                temp = ms_aaiGenOrder[i][j]; dg = (DateGenerator *) m_GntrVctr->GetAt(temp);

                // Make sure we have an active generator, and perform a strange test      
                // to see if a previous generator ran, but didn't find any dates.         
                // (If a previous generator ran and went dateless, we don't want to run.) 

                if (dg->active() && (!(bPrevGenRan != FALSE && (x->GetSize() == 0))))
                {
                    genDates = new JulianPtrArray(); PR_ASSERT(genDates != 0);

                    dg->setSpan(lastInterval);
                    // If previous generator has been run and produced dates, then run this test
                    if (x != 0 && x->GetSize() > 0)
                    {
                        for (k = 0; k < x->GetSize(); k++)
                        {
                            done = done || dg->generate((DateTime *) x->GetAt(k), *genDates, &m_Until);
#if DEBUG
                            //if (FALSE) TRACE("1) i = %d, j = %d, done = %d, genDates = ", i, j, done);
                            //if (FALSE) TRACE_DATETIMEVECTOR(genDates, "genDates");
#endif
                        }
                    }
                    else
                    {
                        done = done || dg->generate(&t, *genDates, &m_Until);
#if DEBUG
                        //if (FALSE) TRACE("2) i = %d, j = %d, done = %d, genDates = ", i, j, done);
                        //if (FALSE) TRACE_DATETIMEVECTOR(genDates, "genDates");
#endif
                    }

                    lastInterval = dg->getInterval();

                    v->Add(genDates);
                    //bPrevGenRan = TRUE;
                }
            }
            bSubGenerate = (i > 2);

            if (v->GetSize() > 0)
            {
                //bPrevGenRan = TRUE;       
#if DEBUG
                TRACE_DATETIMEVECTORVECTOR(v, "v");
                TRACE_DATETIMEVECTOR(x, "x");
#endif
                JulianPtrArray * vFlat = new JulianPtrArray();

                if (v->GetSize() > 1)
                {
                    if (x->GetSize() == 0)
                    {
                        intersectDateList(v, FALSE, x);
                    }
                    else 
                    {
                        intersectDateList(v, FALSE, vFlat);
                        intersectOutList(x, vFlat);
                    }
                }
                else 
                {
                    // If no previous dates generated, just cat.
                    if (x->GetSize() == 0)
                    {
                        concat(v, x);
                    }
                    else
                    {
                        // Hours, Minutes, Seconds can be applied to a any date.
                        // That is why call replace.
                        // Day Of Week, Day Of Month, Day Of Year, Week Number
                        // cannot be applied to any date.  Thus must intersect.
                        if (bSubGenerate)
                        {
                            // WORKS FOR BYHOUR, BYMINUTE, ELECTION DAYS FAIL
                            // replace on byhour, byminute, bysecond
                            //concat(v, x);                        
                            replace(v, x);
                        }
                        else
                        {
                            // FAILS FOR BYHOUR, BYMINUTE, ELECTION DAYS WORK
                            // intersect on bymonth, byday, bymonthday, byyearday, byweekno
                            // Have x.
                            concat(v, vFlat);
                            intersectOutList(x, vFlat); // ?? FAILS for BYHOUR, BYMINUTE
                        }
                    }
                }
#if DEBUG
                //if (FALSE) TRACE_DATETIMEVECTOR(vFlat, "vFlat");
                //if (FALSE) TRACE("After concat: x = ");
                //if (FALSE) TRACE_DATETIMEVECTOR(x, "x");
#endif
                deleteDateTimeVector(vFlat); 
                delete vFlat; vFlat = 0;
            }
            deleteDateTimeVectorVector(v); // genDates deleted here as well
            delete v;  v = 0;
        }
        
        // MUST sort before handling SETPOS
        x->QuickSort(DateTime::CompareDateTimes);

        handleSetPos(x);

        // ADD newest created dates to overall dateVector

        if (x != 0 && x->GetSize() > 0)
        {
            t = *((DateTime*)x->GetAt(x->GetSize() - 1));

            if (returnDateVector == 0)
            {
                returnDateVector = new JulianPtrArray(); PR_ASSERT(returnDateVector != 0);
            }
            for (i = 0; i < x->GetSize(); i++)
            {
                dt = (DateTime *) x->GetAt(i);
                returnDateVector->Add(new DateTime(*dt));
            }
        }
        
        if (m_iType == JulianUtility::RT_MONTHLY)
        {
            t_int32 interval = m_Interval->getMonth();
            while (interval > 0)
            {
                t.nextMonth();
                interval--;
            }
        }
        else
        {
            PR_ASSERT(m_Interval != 0);
            t.add(*m_Interval);
        }

        reset(t, m_iType, m_iWkSt);
        
        deleteDateTimeVector(x);
        delete x; x = 0; // delete x;

        lastInterval = m_iType;
        
    }
    while (!done && (returnDateVector == 0 || returnDateVector->GetSize() < bound));
    cleanup(returnDateVector, bound, out);
    if (returnDateVector != 0)
    {
        deleteDateTimeVector(returnDateVector);
        delete returnDateVector; returnDateVector = 0;
    }
    //return;
}

//---------------------------------------------------------------------

void
Recurrence::cleanup(JulianPtrArray * vDatesIn, t_int32 length, 
                    JulianPtrArray * out)
{
    t_bool pushStart = FALSE;
    t_int32 i;
    t_int32 newLength = 0;
    DateTime * dt;
    t_int32 size = vDatesIn->GetSize();
    if (vDatesIn == 0 || size == 0)
    {
        out->Add(new DateTime(m_StartDate));
        return;
    }

    dt = (DateTime *) vDatesIn->GetAt(0);
    if (dt->afterDateTime(m_StartDate) || dt->beforeDateTime(m_StartDate))
        pushStart = TRUE;
    
    if (size < length)
    {
        newLength = size + (pushStart ? 1 : 0);
    }
    else
        newLength = length;

    if ((size != length) || pushStart)
    {
        t_int32 startIndex = 0;
        if (pushStart)
        {
            out->Add(new DateTime(m_StartDate));
            newLength = newLength - 1;
        }
        for (i = 0; i < newLength; i++)
        {
            dt = (DateTime *) vDatesIn->GetAt(i);
            //if ((!m_Until.isValid()) || (!(dt->afterDateTime(m_Until))))
            //{
                out->Add(new DateTime(*dt));
            //}
        }
    }
    else
    {
        for (i = 0; i < newLength; i++)
        {
            dt = (DateTime *) vDatesIn->GetAt(i);
            //if ((!m_Until.isValid()) || (!(dt->afterDateTime(m_Until))))
            //{
                out->Add(new DateTime(*dt));
            //}
        }
    }
}

//---------------------------------------------------------------------

void
Recurrence::reset(DateTime & t, t_int32 type, t_int32 wkSt)
{
    t_int32 temp = 0;
    switch(type)
    {
    case JulianUtility::RT_YEARLY:
        t.add(Calendar::DAY_OF_YEAR, -t.get(Calendar::DAY_OF_YEAR) + 1);
        break;
    case JulianUtility::RT_MONTHLY:
        t.add(Calendar::DAY_OF_MONTH, -t.get(Calendar::DAY_OF_MONTH) + 1);
        break;
    case JulianUtility::RT_WEEKLY:
        t.moveTo(wkSt, -1, FALSE);
        break;
    case JulianUtility::RT_DAILY:
    
        // this case is purposely empty. a daily rule can only contain a byhour,
        // or a byminute tag. or it can have no tags. if it has a tag, then the
        // assumed line: t.set(calendar.hour_of_day, 0); will be executed in the
        // appropriate code. if it has no tags, then the reset() method will
        // never be reached, since a defaultgenerator will be installed and will
        //handle all of the work in one run. 
       break;
    case JulianUtility::RT_HOURLY:
        t.set(Calendar::MINUTE, 0);
        break;
    case JulianUtility::RT_MINUTELY:
        t.set(Calendar::SECOND, 0);
        break;
    }
     // a given rule either has or doesn't have a BYHOUR and/or BYMINUTE tag.
     // if it doesn't, then the specific time in each recurrence is based entirely
     // on the time provided in the startDate. if it does then either the hour
     // or the minute is based on the parameter to the BYHOUR or BYMINUTE tag.
     // thus, if the tags exist, we need to reset those values. in the case of
     // the BYMINUTE tag, we must also reset the hourly value, but only if we
     // are not in an hourly rule. confusing..
    
    temp = ms_iByHourGntrIndex;
    if (((DateGenerator *) (m_GntrVctr->GetAt(temp)))->active())
    {
        t.set(Calendar::HOUR_OF_DAY, 0);
    }      
    temp = ms_iByMinuteGntrIndex;
    if (((DateGenerator *) (m_GntrVctr->GetAt(temp)))->active())
    {
        if (type != JulianUtility::RT_HOURLY)
            t.set(Calendar::HOUR_OF_DAY, 0);
        t.set(Calendar::MINUTE, 0);
    }
}

//---------------------------------------------------------------------

void
Recurrence::handleSetPos(JulianPtrArray * vDates)
{
    // If no set pos, just return, leave vDates alone.
    if (m_iBySetPosLen == 0)
        return;

    t_int32 i;
    DateTime * dt;
    JulianPtrArray * vBySetPosDates;
    t_int32 oldSize = vDates->GetSize();
    t_int32 temp = 0;

    if (vDates != 0 && oldSize > 0)
    {
        vBySetPosDates = new JulianPtrArray();
    
        // First get the dates that satisfy the BYSETPOS tag.
        for (i = 0; i < m_iBySetPosLen; i++)
        {
            if(m_aiBySetPos[i] == 0)
            {
                // An parse error should already have been thrown. 
                PR_ASSERT(FALSE);
                break;
            }
            // NOTE: to handle HPUX compile problems with abs
            temp = m_aiBySetPos[i];
            if (temp < 0)
                temp = 0 - temp;

            // replacing this line with next line to avoid HPUX compile problems
            //if(abs(m_aiBySetPos[i]) > oldSize)
            if (temp > oldSize)
            {
                // No such element to add.
                continue;
            }
            if (m_aiBySetPos[i] > 0)
            {
                temp = m_aiBySetPos[i] - 1;
                dt = new DateTime(
                    *( (DateTime*)vDates->GetAt(temp)));
                vBySetPosDates->Add(dt);
            }
            else
            {
                dt = new DateTime(
                    *( (DateTime *)vDates->GetAt(m_aiBySetPos[i] + oldSize)));
                vBySetPosDates->Add(dt);
            }
        }
        // delete elements from vDates
        deleteDateTimeVector(vDates);
        vDates->RemoveAll();

        // copy elements from vBySetPosDates to vDates.
        for (i = 0; i < vBySetPosDates->GetSize(); i++)
        {
            dt = (DateTime *) vBySetPosDates->GetAt(i);
            vDates->Add(dt);
        }
        delete vBySetPosDates; vBySetPosDates = 0;
    }
}
 
//---------------------------------------------------------------------

t_int32 Recurrence::stringToDay(UnicodeString & s, t_bool & error)
{
    t_int32 i = (t_int32) Calendar::SUNDAY;

    if (s.size() == 0)
    {
        error = TRUE;
    }
    else
    {
        if (s == JulianKeyword::Instance()->ms_sSU) i = Calendar::SUNDAY;
        else if (s == JulianKeyword::Instance()->ms_sMO) i = Calendar::MONDAY;
        else if (s == JulianKeyword::Instance()->ms_sTU) i = Calendar::TUESDAY;
        else if (s == JulianKeyword::Instance()->ms_sWE) i = Calendar::WEDNESDAY;
        else if (s == JulianKeyword::Instance()->ms_sTH) i = Calendar::THURSDAY;
        else if (s == JulianKeyword::Instance()->ms_sFR) i = Calendar::FRIDAY;
        else if (s == JulianKeyword::Instance()->ms_sSA) i = Calendar::SATURDAY;
        else
        {
            error = TRUE;
        }
    }
    return i;
}

//---------------------------------------------------------------------

void 
Recurrence::parseDelimited(UnicodeString & us, UnicodeString & strDelim,
                           JulianPtrArray * vectorToFillIn)
{
    UnicodeStringTokenizer * st = new UnicodeStringTokenizer(us, strDelim);
    PR_ASSERT(st != 0);
    if (st != 0)
    {
        UnicodeString u;
        ErrorCode status = ZERO_ERROR;
        while (st->hasMoreTokens())
        {
            u = st->nextToken(u, status);
#if DEBUG
            //if (FALSE) TRACE("u = %s", u.toCString(""));
#endif
            vectorToFillIn->Add(new UnicodeString(u));
        }
        delete st; st = 0;
    }
}
//---------------------------------------------------------------------

// replaces contents of vOut with contents of vvIn
void 
Recurrence::replace(JulianPtrArray * vvIn, JulianPtrArray * vOut)
{
    deleteDateTimeVector(vOut);
    vOut->RemoveAll();
    concat(vvIn, vOut);
}

//---------------------------------------------------------------------
/* concatenates a multidimensional array of dates into a single list */
void 
Recurrence::concat(JulianPtrArray * vvIn, JulianPtrArray * vOut)
{
    int i, j;

    JulianPtrArray * vvInIndex;
    DateTime * dt;
    if (vvIn != 0) 
    {
        PR_ASSERT(vOut != 0);
        if (vOut != 0)
        {
            for(i = 0; i < vvIn->GetSize(); i++)
            {
                vvInIndex = (JulianPtrArray *) vvIn->GetAt(i);
                for (j = 0; j < vvInIndex->GetSize(); j++)
                {
                    dt = (DateTime *) vvInIndex->GetAt(j);
                    vOut->Add(new DateTime(*dt));
                }
            }
        }
    }  
}
//---------------------------------------------------------------------
void
Recurrence::intersectOutList(JulianPtrArray * vDatesOut, JulianPtrArray * vDatesIn)
{
    t_int32 i;
    PR_ASSERT(vDatesOut != 0 && vDatesIn != 0);
    DateTime * dt;
    t_int32 oldSize = vDatesOut->GetSize();

    for (i = oldSize - 1; i >= 0; i--)
    {
        dt = (DateTime *) vDatesOut->GetAt(i);
        if (!contains(vDatesIn, dt))
        {
            vDatesOut->RemoveAt(i);
            delete dt; dt = 0;
        }
    }
}
//---------------------------------------------------------------------
// todo: crash proof
 /* subtracts or intersects datelists:
  *
  * subtract mode: subtracts lists 1..n from list 0
  * intersect mode: intersects lists 0..n
  */
void
Recurrence::intersectDateList(JulianPtrArray * vvDates, t_bool subtract,
                              JulianPtrArray * out)
{
    t_int32 i, j;
    DateTime * bi;
    PR_ASSERT(vvDates != 0 && out != 0);
    PR_ASSERT(vvDates->GetSize() > 0);

    // remove all elements already in out vector
    if (out->GetSize() != 0)
    {
        deleteDateTimeVector(out);
        out->RemoveAll();
    }
  
    PR_ASSERT(out->GetSize() == 0);
    JulianPtrArray * vvDatesIndex = ((JulianPtrArray *) vvDates->GetAt(0));

    for (i = 0; i < vvDatesIndex->GetSize(); i++)
    {
        for (j = 1; j < vvDates->GetSize(); j++)
        {
            bi = (DateTime *)vvDatesIndex->GetAt(i);
            if (contains((JulianPtrArray *)vvDates->GetAt(j), bi))
            {
                if (!subtract)
                    out->Add(new DateTime(*bi));
            }
            else if (subtract)
            {
                out->Add(new DateTime(*bi));
            }
        }
    }
}

//---------------------------------------------------------------------
// checks to see if date b is in date array a, currently masks out milliseconds when it compares

t_bool 
Recurrence::contains(JulianPtrArray * datetimes, DateTime * date)
{
    t_int32 i;
    PR_ASSERT(datetimes != 0 && date != 0);
    if (datetimes != 0 && date != 0)
    {
        DateTime * ai;
        for (i = 0; i < datetimes->GetSize(); i++)
        {
            ai = (DateTime *) datetimes->GetAt(i);
            
            //if (ai != 0 && ((ai->getTime() - (ai->getTime() % 1000)) ==
            //   date->getTime() - (date->getTime() % 1000)))
            if (ai != 0 &&
                ((ai->getTime() / 1000.0) == (date->getTime() / 1000.0)))    
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

//---------------------------------------------------------------------


t_int32 Recurrence::stringToType(UnicodeString & s, t_bool & bParseError)
{
    t_int32 iType;

    if (s.size() > 0)
    {
        if (s == JulianKeyword::Instance()->ms_sMINUTELY) iType = JulianUtility::RT_MINUTELY;
        else if (s == JulianKeyword::Instance()->ms_sHOURLY) iType = JulianUtility::RT_HOURLY;
        else if (s == JulianKeyword::Instance()->ms_sDAILY) iType = JulianUtility::RT_DAILY;
        else if (s == JulianKeyword::Instance()->ms_sWEEKLY) iType = JulianUtility::RT_WEEKLY;
        else if (s == JulianKeyword::Instance()->ms_sMONTHLY) iType = JulianUtility::RT_MONTHLY;
        else if (s == JulianKeyword::Instance()->ms_sYEARLY) iType = JulianUtility::RT_YEARLY;
        else bParseError = TRUE;
    }
    else
    {
        bParseError = TRUE;
    }
    return iType;
}

//---------------------------------------------------------------------
// TODO: crash proof
// bZero, allow zero to pass or not?
t_int32 * 
Recurrence::verifyIntList(JulianPtrArray * v, t_int32 lowerBound, 
                          t_int32 upperBound, UnicodeString & error, 
                          t_int32 & retSize, t_bool & bParseError, 
                          t_bool bZero)
{
    UnicodeString s, into;
    t_int32 * i = new t_int32[v->GetSize()];
    t_int32 j;
    t_int32 k;
    retSize = 0;
    t_int8 bMinusOrPlus = 0;
    t_int32 startIndex = 0;

    // NOTE: to remove compiler warning
    if (error.size() > 0) {}

    PR_ASSERT(v != 0);
    for (j = 0; j < v->GetSize(); j++)
    {
        s = *((UnicodeString *) v->GetAt(j));
        startIndex = 0;
        bMinusOrPlus = 0;
        // If minus or plus in front, remove before extracting number
        if (s[(TextOffset)0] == '-')
        {
            bMinusOrPlus = -1;
            startIndex = 1;
        }
        else if (s[(TextOffset)0] == '+')
        {
            bMinusOrPlus = 1;
            startIndex = 1;
        }
        k = JulianUtility::atot_int32(s.extractBetween(startIndex, 
            s.size(), into).toCString(""), 
            bParseError, (s.size() - startIndex));
            
        // if Minus sign, set to inverse
        if (bMinusOrPlus < 0)
        {
            k = 0 - k;
        }    
        //k = JulianUtility::atot_int32(s.toCString(""), bParseError, s.size());

        if (bZero && k == 0)
            bParseError = TRUE;
        if ((k <= upperBound) && (k >= lowerBound))
        {
            i[j] = k;
            retSize++;
        }
        else
        {
            // TODO: log and error
            bParseError = TRUE;
        } 
    }

    // TODO: remove duplicates and sort int list HERE

    return i;
}

//---------------------------------------------------------------------
// TODO: crash proof 
RRDay *
Recurrence::createByDayList(JulianPtrArray * v, t_int32 type,
                            t_int32 & retSize,
                            t_bool & bParseError)
{
    t_int32 i;
    t_int32 day, modifier;
    PR_ASSERT(v != 0);

    RRDay * l = new RRDay[v->GetSize()];
    UnicodeString * sPtr;
    retSize = v->GetSize();

    for (i = 0; i < v->GetSize(); i++)
    {
        sPtr = (UnicodeString *) v->GetAt(i);

        // get the day and modifier from the string
        createByDayListHelper(*sPtr, day, modifier, bParseError);
    
        // if modifier out of bounds, set bParseError to TRUE
        if ((type == JulianUtility::RT_YEARLY) && 
            (modifier < -53 || modifier > 53))
        {
            bParseError = TRUE;
        }
        else if ((type == JulianUtility::RT_MONTHLY) &&
            (modifier < -5 || modifier > 5))
        {
            bParseError = TRUE;
        }
        l[i].fields[0] = day;
        l[i].fields[1] = modifier;
    }
    return l;
}
//---------------------------------------------------------------------
void Recurrence::createByDayListHelper(UnicodeString & in,
                                       t_int32 & day,
                                       t_int32 & modifier,
                                       t_bool & bParseError)
{
    if (in.size() < 2)
        bParseError = TRUE;
    else
    {
        t_int8 bMinusOrPlus = 0, startIndex = 0;
        UnicodeString into;
        into = in.extractBetween(in.size() - 2, in.size(), into);
        day = stringToDay(into, bParseError);
        if (in.size() > 2)
        {
            // If minus or plus in front, remove before extracting number
            if (in[(TextOffset)0] == '-')
            {
                bMinusOrPlus = -1;
                startIndex = 1;
            }
            else if (in[(TextOffset)0] == '+')
            {
                bMinusOrPlus = 1;
                startIndex = 1;
            }
            modifier = 
                JulianUtility::atot_int32(in.extractBetween(startIndex, 
                in.size() - 2, into).toCString(""), 
                bParseError, (in.size() - 2 - startIndex));
            
            // if Minus sign, set to inverse
            if (bMinusOrPlus < 0)
            {
                modifier = 0 - modifier;
            }    
        }
        else 
        {
            modifier = 0;
        }
    }
}
//---------------------------------------------------------------------

void Recurrence::eliminateDuplicates(JulianPtrArray * datetimes)
{
    t_int32 i;
    if (datetimes != 0)
    {
        // NOTE: Order NlogN + N, (sort, then compare consective elements)
        datetimes->QuickSort(DateTime::CompareDateTimes);
        t_int32 size = datetimes->GetSize();        
        DateTime * dt, * dt2;

        for (i = size - 1; i >= 1; i--)
        {
            dt = (DateTime *) datetimes->GetAt(i);
            dt2 = (DateTime *) datetimes->GetAt(i - 1);                
            if (dt->equalsDateTime(*dt2))
            {
                delete dt; dt = 0;
                datetimes->RemoveAt(i);
            }
        }
    }
}

//---------------------------------------------------------------------
void Recurrence::deleteUnicodeStringVector(JulianPtrArray * strings)
{
    t_int32 i;
    if (strings != 0)
    {
        for (i = strings->GetSize() - 1; i >= 0; i--) 
        {
            delete ((UnicodeString *)strings->GetAt(i));
        }
    }
}
//---------------------------------------------------------------------
void Recurrence::deleteDateTimeVector(JulianPtrArray * datetimes)
{
    t_int32 i;
    if (datetimes != 0)
    {
        for (i = datetimes->GetSize() - 1; i >= 0; i--) 
        {
            delete ((DateTime *)datetimes->GetAt(i));
        }
    }
}
//---------------------------------------------------------------------
void Recurrence::deleteRecurrenceVector(JulianPtrArray * recurrences)
{
    t_int32 i;
    if (recurrences != 0)
    {
        for (i = recurrences->GetSize() - 1; i >= 0; i--) 
        {
            delete ((Recurrence *)recurrences->GetAt(i));
        }
    }
}
//---------------------------------------------------------------------
void 
Recurrence::deleteICalParameterVector(JulianPtrArray * parameters)
{
    ICalParameter * ip;
    t_int32 i;
    if (parameters != 0)
    {
        for (i = parameters->GetSize() - 1; i >= 0; i--)
        {
            ip = (ICalParameter *) parameters->GetAt(i);
            delete ip; ip = 0;
        }
    }
}
//---------------------------------------------------------------------
void Recurrence::deleteDateTimeVectorVector(JulianPtrArray * vDateTimes)
{
    t_int32 i;
    JulianPtrArray * xp;
    if (vDateTimes != 0)
    {
        for (i = vDateTimes->GetSize() - 1; i >= 0; i--) 
        {
            xp = (JulianPtrArray *) vDateTimes->GetAt(i);
            deleteDateTimeVector(xp);
            xp->RemoveAll();
            delete xp; xp = 0;
        }
    }
}
//---------------------------------------------------------------------
#if 0
void 
Recurrence::copyDateTimesInto(JulianPtrArray * from,
                              DateTime * to)
{
    t_int32 i;
    PR_ASSERT(to != 0 && from != 0);
    DateTime d;
    for (i = 0; i < from->GetSize(); i++)
    {
        d = *((DateTime *) from->GetAt(i));
        to[i] = d;
    }
}
#endif
//---------------------------------------------------------------------

