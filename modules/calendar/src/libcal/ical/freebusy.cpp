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

// freebusy.cpp
// John Sun
// 6:35 PM Febuary 18 1998

#include "stdafx.h"
#include "jdefines.h"
#include "freebusy.h"
#include "icalcomp.h"
#include "unistrto.h"
#include "jlog.h"
#include "jutility.h"
#include "keyword.h"
//---------------------------------------------------------------------
const t_int32 Freebusy::ms_cFreebusyType = 'T';
const t_int32 Freebusy::ms_cFreebusyPeriod = 'P';
const t_int32 Freebusy::ms_iDEFAULT_TYPE= FB_TYPE_BUSY;

#if 0
const t_int32 Freebusy::ms_cFreebusyStatus = 'S';
const t_int32 Freebusy::ms_iDEFAULT_STATUS= FB_STATUS_BUSY;
#endif

//UnicodeString Freebusy::m_strDefaultFmt = "\tTYPE= ^T STATUS = ^S ^P\r\n";
//const UnicodeString Freebusy::JulianKeyword::Instance()->ms_sDEFAULT_TYPE   = JulianKeyword::Instance()->ms_sBUSY; 
//const UnicodeString Freebusy::JulianKeyword::Instance()->ms_sDEFAULT_STATUS = JulianKeyword::Instance()->ms_sBUSY; 
//static UnicodeString s_sComma = JulianKeyword::Instance()->ms_sCOMMA_SYMBOL;

//---------------------------------------------------------------------

Freebusy::FB_TYPE
Freebusy::stringToType(UnicodeString & sType)
{
    t_int32 hashCode = sType.hashCode();

    if (JulianKeyword::Instance()->ms_ATOM_BUSY == hashCode) return FB_TYPE_BUSY;
    else if (JulianKeyword::Instance()->ms_ATOM_FREE == hashCode) return FB_TYPE_FREE;
    // added 4-28-98
    else if (JulianKeyword::Instance()->ms_ATOM_BUSY_UNAVAILABLE == hashCode) 
        return FB_TYPE_BUSY_UNAVAILABLE;
    else if (JulianKeyword::Instance()->ms_ATOM_BUSY_TENTATIVE == hashCode) 
        return FB_TYPE_BUSY_TENTATIVE;
    else return FB_TYPE_INVALID;
}

//---------------------------------------------------------------------

UnicodeString & 
Freebusy::typeToString(Freebusy::FB_TYPE type, UnicodeString & out)
{
    switch(type)
    {
    case FB_TYPE_BUSY: out = JulianKeyword::Instance()->ms_sBUSY; break;
    case FB_TYPE_FREE: out = JulianKeyword::Instance()->ms_sFREE; break;
    // added 4-28-98
    case FB_TYPE_BUSY_UNAVAILABLE: out = JulianKeyword::Instance()->ms_sBUSY_UNAVAILABLE; break;
    case FB_TYPE_BUSY_TENTATIVE: out = JulianKeyword::Instance()->ms_sBUSY_TENTATIVE; break;
    default:
        // return default BUSY
        out = JulianKeyword::Instance()->ms_sBUSY;
        break;
    }
    return out;
}

//---------------------------------------------------------------------
#if 0
Freebusy::FB_STATUS
Freebusy::stringToStatus(UnicodeString & sStatus)
{
    t_int32 hashCode = sStatus.hashCode();

    if (JulianKeyword::Instance()->ms_ATOM_BUSY == hashCode) return FB_STATUS_BUSY;
    else if (JulianKeyword::Instance()->ms_ATOM_UNAVAILABLE == hashCode) return FB_STATUS_UNAVAILABLE;
    else if (JulianKeyword::Instance()->ms_ATOM_TENTATIVE == hashCode) return FB_STATUS_TENTATIVE;
    else return FB_STATUS_INVALID;
}

//---------------------------------------------------------------------

UnicodeString & 
Freebusy::statusToString(Freebusy::FB_STATUS status, UnicodeString & out)
{
    switch(status)
    {
    case FB_STATUS_BUSY: out = JulianKeyword::Instance()->ms_sBUSY; break;
    case FB_STATUS_UNAVAILABLE: out = JulianKeyword::Instance()->ms_sUNAVAILABLE; break;
    case FB_STATUS_TENTATIVE: out = JulianKeyword::Instance()->ms_sTENTATIVE; break;
    default:
        // return default BUSY
        out = JulianKeyword::Instance()->ms_sBUSY;
        break;
    }
    return out;
}
#endif
//---------------------------------------------------------------------
// TODO: Finish
void Freebusy::parsePeriod(UnicodeString & s, JulianPtrArray * vTimeZones)
{
    UnicodeStringTokenizer * stMult;
    //UnicodeString delim = ",";
    UnicodeString sPeriod;
    //UnicodeString sPeriodStart;
    //UnicodeString sPeriodEnd;
    Period * p;
    ErrorCode status = ZERO_ERROR;

    if (vTimeZones)
    {
    }
    stMult = new UnicodeStringTokenizer(s, 
        JulianKeyword::Instance()->ms_sCOMMA_SYMBOL);
    PR_ASSERT(stMult != 0);
    if (stMult != 0)
    {
        while (stMult->hasMoreTokens())
        {
            sPeriod = stMult->nextToken(sPeriod, status);
            sPeriod.trim();
            //if (FALSE) TRACE("sPeriod = %s\r\n", sPeriod.toCString(""));
            // TODO: check that period start/end always uses UTC

            p = new Period(sPeriod);
            PR_ASSERT(p != 0);
            if (p != 0)
            {
                if (!p->isValid())
                {
                    if (m_Log) m_Log->logString(JulianLogErrorMessage::Instance()->ms_sFreebusyPeriodInvalid, 200);
                }
                else
                {
                    //sPeriodStart = Period::getStartTimeString(sPeriod, sPeriodStart);
                    //sPeriodEnd = Period::getEndTimeString(sPeriod, sPeriodEnd);
                    addPeriod(p);
                }
                delete p; p = 0;
            }
        }
        delete stMult; stMult = 0;
    }
}
//---------------------------------------------------------------------
void Freebusy::setParam(UnicodeString & paramName, UnicodeString & paramVal)
{
    t_int32 i;
    if (paramName.size() == 0)
    {
        if (m_Log) m_Log->logString(JulianLogErrorMessage::Instance()->ms_sInvalidParameterName, 
            JulianKeyword::Instance()->ms_sFREEBUSY, paramName, 200);
    }
    else
    {
        t_int32 hashCode = paramName.hashCode();

        if (JulianKeyword::Instance()->ms_ATOM_FBTYPE == hashCode)
        {
            i = Freebusy::stringToType(paramVal);
            if (i < 0)
            {
                if (m_Log) m_Log->logString(JulianLogErrorMessage::Instance()->ms_sInvalidParameterValue, 
                    JulianKeyword::Instance()->ms_sFREEBUSY, paramName, paramVal, 200);
            }
            else 
            {
                if (getType() >= 0)
                {
                    if (m_Log) m_Log->logString(JulianLogErrorMessage::Instance()->ms_sDuplicatedParameter, 
                        JulianKeyword::Instance()->ms_sFREEBUSY, paramName, 100);
                }
                setType((Freebusy::FB_TYPE) i);
            }
        
        } 
#if 0
        else if (JulianKeyword::Instance()->ms_ATOM_BSTAT == hashCode)   
        {
            i = Freebusy::stringToStatus(paramVal);
            if (i < 0)
            {
                 if (m_Log) m_Log->logString(JulianLogErrorMessage::Instance()->ms_sInvalidParameterValue, 
                    JulianKeyword::Instance()->ms_sFREEBUSY, paramName, paramVal, 200);
            }
            else 
            {
                if (getStatus() >= 0)
                {
                    if (m_Log) m_Log->logString(JulianLogErrorMessage::Instance()->ms_sDuplicatedParameter, 
                        JulianKeyword::Instance()->ms_sFREEBUSY, paramName, 100);
                }
                setStatus((Freebusy::FB_STATUS) i);
            }
        }
#endif
        else 
        {
            // NOTE: what about optional parameters?? THERE ARE NONE.
            if (m_Log) m_Log->logString(JulianLogErrorMessage::Instance()->ms_sInvalidParameterName,
                        JulianKeyword::Instance()->ms_sFREEBUSY, paramName, 200);
        }
    }
}
//---------------------------------------------------------------------
void Freebusy::selfCheck()
{
    if (getType() < 0 || getType() > FB_TYPE_LENGTH)
        setDefaultProps(FB_PROPS_TYPE);
#if 0
    else if (getStatus() < 0 || getStatus() > FB_STATUS_LENGTH)
        setDefaultProps(FB_PROPS_STATUS);
#endif
    // NOTE: selfcheck each period???, no need since addPeriod auto checks??
}
//---------------------------------------------------------------------
void Freebusy::setDefaultProps(Freebusy::FB_PROPS prop)
{
    switch (prop)
    {
    case FB_PROPS_TYPE: setType((Freebusy::FB_TYPE) ms_iDEFAULT_TYPE); return;
#if 0
    case FB_PROPS_STATUS: setStatus((Freebusy::FB_STATUS) ms_iDEFAULT_STATUS); return;
#endif
    default: return;
    }
}
//---------------------------------------------------------------------
void Freebusy::sortPeriods()
{
    if (m_vPeriod != 0)
        m_vPeriod->QuickSort(Period::ComparePeriods);
}
//---------------------------------------------------------------------
void Freebusy::sortPeriodsByEndTime()
{
    if (m_vPeriod != 0)
        m_vPeriod->QuickSort(Period::ComparePeriodsByEndTime);
}
//---------------------------------------------------------------------

// private never use
#if 0
Freebusy::Freebusy()
{
    PR_ASSERT(FALSE);
}
#endif

//---------------------------------------------------------------------

Freebusy::Freebusy(JLog * initLog)
: m_Type(ms_iDEFAULT_TYPE), 
#if 0
    m_Status(ms_iDEFAULT_STATUS),
#endif
 m_vPeriod(0), m_Log(initLog)
{
    //PR_ASSERT(initLog != 0);
}

//---------------------------------------------------------------------
Freebusy::Freebusy(Freebusy & that)
: m_vPeriod(0)
{
    t_int32 i;

    m_Type = that.m_Type;
#if 0
    m_Status = that.m_Status;
#endif
    if (that.m_vPeriod != 0)
    {
        m_vPeriod = new JulianPtrArray();
        PR_ASSERT(m_vPeriod != 0);
        if (m_vPeriod != 0)
        {
            for (i = 0; i < that.m_vPeriod->GetSize(); i++)
            {
                Period * p = (Period *) (that.m_vPeriod->GetAt(i));
                PR_ASSERT(p != 0);
                if (p != 0)
                {
                    addPeriod(p);
                }
            }
        }
    }
}
//---------------------------------------------------------------------
Freebusy::~Freebusy()
{
    if (m_vPeriod != 0)
    {
        ICalProperty::deletePeriodVector(m_vPeriod);
        delete m_vPeriod; m_vPeriod = 0;
    }
}

//---------------------------------------------------------------------
ICalProperty * 
Freebusy::clone(JLog * initLog)
{
    m_Log = initLog; 
    //PR_ASSERT(m_Log != 0);
    return new Freebusy(*this);
}
//---------------------------------------------------------------------

void Freebusy::addPeriod(Period * p) 
{ 
    PR_ASSERT(p != 0);
    if (m_vPeriod == 0)
        m_vPeriod = new JulianPtrArray();
    PR_ASSERT(m_vPeriod != 0);
    if (m_vPeriod != 0 && p != 0)
    {
        m_vPeriod->Add(new Period(*p));
    }
}

//---------------------------------------------------------------------
DateTime Freebusy::startOfFreebusyPeriod()
{
    t_int32 i;
    DateTime d(-1);
    if (m_vPeriod != 0)
    {
        i = m_vPeriod->GetSize();

        if (i > 0)
        {
            sortPeriods();
            Period * p = (Period *) m_vPeriod->GetAt(0);
            d = p->getStart();
            return d;
        }
        else
        {
            // NOTE: No periods in vector, so returning invalid datetime
            return d; 
        }
    }
    else 
    {
        // NOTE: No periods in vector, so returning invalid datetime
        return d;
    }
}
//---------------------------------------------------------------------
DateTime Freebusy::endOfFreebusyPeriod()
{
    t_int32 i;
    DateTime d(-1);
    Duration du(-1, -1, -1, -1, -1, -1, -1);
    if (m_vPeriod != 0)
    {
        i = m_vPeriod->GetSize();
        if (i > 0)
        {
            sortPeriodsByEndTime();
            Period * p = (Period *) m_vPeriod->GetAt(i - 1);
            if (p->getEnd().isValid())
            {
                d = p->getEnd();
                return d;
            }
            else
            {   
                d = p->getStart();
                du = p->getDuration();
                d.add(du);
                return d;
            }
            //return p->getEndingTime(d);
        }
        else 
        {
           // NOTE: No periods in vector, so returning invalid datetime
            return d;
        }
    }
    else 
    {
        // NOTE: No periods in vector, so returning invalid datetime
        return d;
    }
}
//---------------------------------------------------------------------
void Freebusy::parse(UnicodeString & in)
{
    UnicodeString propName;
    UnicodeString propVal;
    JulianPtrArray * parameters = new JulianPtrArray();
    PR_ASSERT(parameters != 0);
    if (parameters != 0)
    {
        ICalProperty::parsePropertyLine(in, propName, propVal, parameters);
    
        parse(propVal, parameters);

        ICalProperty::deleteICalParameterVector(parameters);
        delete parameters; parameters = 0;
    }
}
//---------------------------------------------------------------------
void Freebusy::parse(UnicodeString & sPeriods, JulianPtrArray * parameters,
                     JulianPtrArray * vTimeZones)
{
    t_int32 i;
    ICalParameter * ip;
    UnicodeString pName;
    UnicodeString pVal;
    if (sPeriods.size() == 0)
    {
    }
    else 
    {
        parsePeriod(sPeriods, vTimeZones);
        if (parameters != 0)
        {
            for (i = 0; i < parameters->GetSize(); i++)
            {
                ip = (ICalParameter *) parameters->GetAt(i);
                setParam(ip->getParameterName(pName), ip->getParameterValue(pVal));
            }
        }
    }

    sortPeriods();
    selfCheck();
}
//---------------------------------------------------------------------
void Freebusy::normalizePeriods()
{
    sortPeriodsByEndTime();
    t_int32 size = 0;
    t_int32 i;
    Period * p1;
    Period * p2;
    Period * pUnion;

    if (m_vPeriod != 0)
    {
        size = m_vPeriod->GetSize();
        for (i = size -1; i >= 0; i--)
        {
            p1 = (Period *) m_vPeriod->GetAt(i); PR_ASSERT(p1 != 0);
            if (i - 1 >= 0)
            {
                p2 = (Period *) m_vPeriod->GetAt(i - 1); PR_ASSERT(p2 != 0);
                if (Period::IsConnectedPeriods(*p1, *p2))
                {
                    pUnion = new Period(); PR_ASSERT(pUnion != 0);
                    if (pUnion != 0)
                    {
                        Period::Union(*p1, *p2, *pUnion);
                        delete p1; p1 = 0;
                        m_vPeriod->RemoveAt(i); // remove p1
                        delete p2; p2 = 0;
                        m_vPeriod->RemoveAt(i - 1); // remove p2
                        //removePeriod(p1); p1 = 0;
                        //removePeriod(p2); p2 = 0;
                        m_vPeriod->InsertAt(i -1, pUnion);
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------------------

t_bool Freebusy::isValid()
{
    if (m_vPeriod == 0 || m_vPeriod->GetSize() == 0)
        return FALSE;
    return TRUE;
}

//---------------------------------------------------------------------

UnicodeString & Freebusy::toString(UnicodeString & out)
{
    out = toString(JulianFormatString::Instance()->ms_FreebusyStrDefaultFmt, out);
    return out;
}

//---------------------------------------------------------------------

UnicodeString & 
Freebusy::toString(UnicodeString & strFmt, UnicodeString & out) 
{
    if (strFmt.size() == 0 && JulianFormatString::Instance()->ms_FreebusyStrDefaultFmt.size() > 0)
    {
        // if empty string, use default
        return toString(out);
    }

    UnicodeString into;
    t_int32 i,j;    
    //char * c = strFmt.toCString("");
    out = "";
    for ( i = 0; i < strFmt.size(); )
    {
	
        // NOTE: changed from % to ^ for freebusy
        ///
	    ///  If there's a special formatting character,
	    ///  handle it.  Otherwise, just emit the supplied
	    ///  character.
	    ///
	    into = "";
        j = strFmt.indexOf('^', i);
        if ( -1 != j)
        {
	        if (j > i)
            {
	            out += strFmt.extractBetween(i,j,into);
            }
            i = j + 1;
            if (strFmt.size() > i)
            {
                out += toStringChar(strFmt[(TextOffset) i], into); 
                i++;
            }
	    }
	    else
        {
            out += strFmt.extractBetween(i, strFmt.size(),into);
            break;
        }
    }
    return out;
}

//---------------------------------------------------------------------

UnicodeString &
Freebusy::toStringChar(t_int32 c, UnicodeString & out)
{
    switch ( c )
    {
      case ms_cFreebusyType:
          return typeToString(getType(), out);
#if 0
      case ms_cFreebusyStatus:
          return statusToString(getStatus(), out);
#endif
      case ms_cFreebusyPeriod:
          if (m_vPeriod != 0)
          {
              Period * p;
              t_int32 i;
              for (i = 0; i < m_vPeriod->GetSize(); i++)
              {
                  p = (Period *) m_vPeriod->GetAt(i);
                  if (i < m_vPeriod->GetSize() -1)
                  {
                      out += p->toString();
                      out += ',';
                  }
                  else
                      out += p->toString();
              }
          }
          return out;
      default:
	    return out;
    }
}// end of 
//---------------------------------------------------------------------

UnicodeString &
Freebusy::toICALString(UnicodeString & sProp, UnicodeString & out)
{
    // NOTE: Remove later, to avoid warning
    if (sProp.size() > 0) {}
    return toICALString(out);
}

//---------------------------------------------------------------------
UnicodeString & Freebusy::toICALString(UnicodeString & out)
{
    UnicodeString s, u;
    t_int32 i;

    out = JulianKeyword::Instance()->ms_sFREEBUSY;
    s = typeToString(getType(), s);
    out += ICalProperty::parameterToCalString(JulianKeyword::Instance()->ms_sFBTYPE, s, u);
#if 0
    s = statusToString(getStatus(), s);
    out += ICalProperty::parameterToCalString(JulianKeyword::Instance()->ms_sBSTAT, s, u);
#endif
    out += ":";
    if (m_vPeriod != 0)
    {
        for (i = 0; i < m_vPeriod->GetSize(); i++)
        {
            Period * p = (Period *) m_vPeriod->GetAt(i);

            if (i < m_vPeriod->GetSize() -1)
            {
                out += p->toICALString();
                out += JulianKeyword::Instance()->ms_sCOMMA_SYMBOL;
            }
            else
                out += p->toICALString();
        }
    }
    out += JulianKeyword::Instance()->ms_sLINEBREAK;
    out = ICalProperty::multiLineFormat(out);
    return out;
}
//---------------------------------------------------------------------
void Freebusy::setParameters(JulianPtrArray * parameters)
{
    t_int32 i;
    ICalParameter * ip;
    UnicodeString pName;
    UnicodeString pVal;
    if (parameters != 0)
    {
        for (i = 0; i < parameters->GetSize(); i++)
        {
            ip = (ICalParameter *) parameters->GetAt(i);
            setParam(ip->getParameterName(pName), ip->getParameterValue(pVal));
        }
    }
}
//---------------------------------------------------------------------
// NOTE: assumes normalized periods
// TODO: verify it works
/*
t_bool Freebusy::IsInside(DateTime start, DateTime end)
{
    PR_ASSERT(m_vPeriod != 0);
    t_int32 i;
    Period * p;
    if (m_vPeriod != 0)
    {
        for (i = 0; i < m_vPeriod->GetSize(); i++)
        {
            p = (Period *) m_vPeriod->GetAt(i);
            if (!p->isInside(start, end))
                return FALSE;
        }
    }
    return TRUE;
}
*/
//---------------------------------------------------------------------
// TODO: update method
//void update()
//---------------------------------------------------------------------
// TODO: verify it works
/*
void Freebusy::clipOutOfBounds(DateTime start, DateTime end)
{
    PR_ASSERT(!start.afterDateTime(end));
    Period * p;
    Period * pShrunk;
    t_int32 i;

    if (m_vPeriod != 0)
    {
        for (i = m_vPeriod->GetSize() - 1; i >= 0; i--)
        {
            p = (Period *) m_vPeriod->GetAt(i);
            if (!p->isInside(start, end))
            {
                if (p->isIntersecting(start, end))
                {
                    pShrunk = new Period(); PR_ASSERT(pShrunk != 0);
                    Period::Intersection(*p, start, end, *pShrunk);
                    addPeriod(pShrunk);
                }
                removePeriod(p); p = 0;
            }
        }
    }
}
*/
//---------------------------------------------------------------------
int Freebusy::CompareFreebusy(const void * a, const void * b)
{
    Freebusy * fa = *(Freebusy **) a;
    Freebusy * fb = *(Freebusy **) b;

    PR_ASSERT(fa->isValid() && fb->isValid());
    Period * pA = (Period *) fa->getPeriod()->GetAt(0);
    Period * pB = (Period *) fb->getPeriod()->GetAt(0);
    return Period::ComparePeriods(&pA, &pB);
}
//---------------------------------------------------------------------
void Freebusy::deleteFreebusyVector(JulianPtrArray * freebusy)
{
    Freebusy * ip;
    t_int32 i;
    if (freebusy != 0)
    {
        for (i = freebusy->GetSize() - 1; i >= 0; i--)
        {
            ip = (Freebusy *) freebusy->GetAt(i);
            delete ip; ip = 0;
        }
    }
}

//---------------------------------------------------------------------

t_bool
Freebusy::HasSameParams(Freebusy * f1, Freebusy * f2)
{
    PR_ASSERT(f1 != 0 && f2 != 0);
    if (f1 == 0 || f2 == 0)
    {
        // BOTH null returns FALSE
        return FALSE;
    }
#if 0
    return ((f1->getType() == f2->getType()) && 
        (f1->getStatus() == f2->getStatus()));
#else
    return (f1->getType() == f2->getType());
#endif
}

//---------------------------------------------------------------------

void
Freebusy::addAllPeriods(JulianPtrArray * periods)
{
    Period * p;
    t_int32 i;
    if (periods != 0)
    {
        for (i = 0; i < periods->GetSize(); i++)
        {
            p = (Period *) periods->GetAt(i);
            addPeriod(p);
        }
    }
}
//---------------------------------------------------------------------


