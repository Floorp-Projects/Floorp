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

// vevent.cpp
// John Sun
// 10:50 AM February 9 1998

#include "stdafx.h"
#include "jdefines.h"

#include "jutility.h"
#include "vevent.h"
#include "icalredr.h"
#include "prprtyfy.h"
#include "unistrto.h"
#include "jlog.h"
#include "vtimezne.h"
#include "keyword.h"

#include "period.h"
#include "datetime.h"
/*
//---------------------------------------------------------------------
// checked
UnicodeString VEvent::ms_sAllPropertiesMessage = 
"%v%a%k%c%K%H%t%i%D%B%e%C%X%E%O%M%L%J%p%x%y%R%o%T%r%s%g%S%h%U%u%w%Z";

//---------------------------------------------------------------------
// checked 
UnicodeString VEvent::ms_sCancelMessage =
"%K%R%U%s%J%C%g%Z"; 

//---------------------------------------------------------------------
// request, checked both
// everything but duration, req-status
UnicodeString VEvent::ms_sRequestMessage =
"%v%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%J%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";

UnicodeString VEvent::ms_sRecurRequestMessage =
"%v%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%J%p%x%y%o%r%s%g%S%h%U%u%w%Z";
//---------------------------------------------------------------------
// checked, counter, no organizer, duration, req-status    
UnicodeString VEvent::ms_sCounterMessage =
"%v%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";

//---------------------------------------------------------------------
// checked
UnicodeString VEvent::ms_sDeclineCounterMessage =
"%K%R%U%s%J%T%C%Z";

//---------------------------------------------------------------------
UnicodeString VEvent::ms_sAddMessage =
"%v%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%J%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";

//---------------------------------------------------------------------
// checked
UnicodeString VEvent::ms_sRefreshMessage =
"%v%R%U%C%K%H%Z";
    
//---------------------------------------------------------------------
// checked
UnicodeString VEvent::ms_sReplyMessage =
"%v%K%R%U%s%J%T%C%H%X%E";
    
//---------------------------------------------------------------------
// checked, publish
// everything but duration, req-status

UnicodeString VEvent::ms_sPublishMessage =
"%v%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%J%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";
    
UnicodeString VEvent::ms_sRecurPublishMessage =
"%v%a%k%c%K%H%t%i%B%e%C%X%E%O%M%L%J%p%x%y%o%r%s%g%S%h%U%u%w%Z";

//---------------------------------------------------------------------
    
UnicodeString VEvent::ms_sDelegateRequestMessage =
"%v%K%B%e%i%R%U%s%C";

UnicodeString VEvent::ms_sRecurDelegateRequestMessage =
"%v%K%B%e%i%U%s%C";

*/

// public
//const UnicodeString VEvent::ms_aTRANSP[]    =
//  { JulianKeyword::Instance()->ms_sOPAQUE, JulianKeyword::Instance()->ms_sTRANSPARENT };

//private
/*
UnicodeString VEvent::m_strDefaultFmt =
// "%(EEE MM/dd/yy hh:mm a)B - %(EEE MM/dd/yy hh:mm a)e\r\n\t UID: %U, SEQ: %s, SUM: %S, LastM: %(EEE MM/dd/yy hh:mm a)M %w";
 "[%(EEE MM/dd/yy hh:mm:ss z)B - %(EEE MM/dd/yy hh:mm:ss z)e]  UID: %U, SEQ: %s, SUM: %S, LastM: %(EEE MM/dd/yy hh:mm:ss z)M %(\r\n\t^N ^R,^S)v %w";
*/
//---------------------------------------------------------------------
void VEvent::setDefaultFmt(UnicodeString s)
{
    JulianFormatString::Instance()->ms_VEventStrDefaultFmt = s;
}
//---------------------------------------------------------------------

// never use
#if 0
VEvent::VEvent()
{
    PR_ASSERT(FALSE);
}
#endif
//---------------------------------------------------------------------

VEvent::VEvent(JLog * initLog)
:   m_DTEnd(0), 
    m_TempDuration(0),
    m_GEO(0), m_Location(0), m_Priority(0), m_ResourcesVctr(0), 
    m_Transp(0),
    TimeBasedEvent(initLog)
{
}
//---------------------------------------------------------------------
VEvent::VEvent(VEvent & that)
: m_DTEnd(0), 
  m_TempDuration(0),
  m_GEO(0), m_Location(0), m_Priority(0), m_ResourcesVctr(0),
  m_Transp(0),
  TimeBasedEvent(that)
{
    m_origDTEnd = that.m_origDTEnd;
    m_origMyDTEnd = that.m_origMyDTEnd;

    if (that.m_DTEnd != 0) { m_DTEnd = that.m_DTEnd->clone(m_Log); }
    //if (that.m_Duration != 0) { m_Duration = that.m_Duration->clone(m_Log); }
    if (that.m_GEO != 0) { m_GEO = that.m_GEO->clone(m_Log); }
    if (that.m_Location != 0) { m_Location = that.m_Location->clone(m_Log); }
    if (that.m_Priority != 0) { m_Priority = that.m_Priority->clone(m_Log); }
    if (that.m_Transp != 0) { m_Transp = that.m_Transp->clone(m_Log); }
    if (that.m_ResourcesVctr != 0)
    {
        m_ResourcesVctr = new JulianPtrArray(); PR_ASSERT(m_ResourcesVctr != 0);
        if (m_ResourcesVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_ResourcesVctr, m_ResourcesVctr, m_Log);
        }
    }
}
//---------------------------------------------------------------------
ICalComponent * 
VEvent::clone(JLog * initLog)
{
    m_Log = initLog; 
    //PR_ASSERT(m_Log != 0);
    return new VEvent(*this);
}
//---------------------------------------------------------------------

VEvent::~VEvent()
{
    // properties
    if (m_DTEnd != 0) { delete m_DTEnd; m_DTEnd = 0; }
    //if (m_Duration != 0) { delete m_Duration; m_Duration = 0; }
    if (m_GEO != 0) { delete m_GEO; m_GEO = 0; }
    if (m_Location!= 0) { delete m_Location; m_Location = 0; }
    if (m_Priority!= 0) { delete m_Priority; m_Priority = 0; }
    if (m_Transp != 0) { delete m_Transp; m_Transp = 0; }
    
    // vector of strings
    if (m_ResourcesVctr != 0) { 
        ICalProperty::deleteICalPropertyVector(m_ResourcesVctr);
        delete m_ResourcesVctr; m_ResourcesVctr = 0; 
    }
    //delete (TimeBasedEvent *) this;
}

//---------------------------------------------------------------------

UnicodeString &
VEvent::parse(ICalReader * brFile, UnicodeString & sMethod, 
              UnicodeString & parseStatus, JulianPtrArray * vTimeZones,
              t_bool bIgnoreBeginError, JulianUtility::MimeEncoding encoding) 
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVEVENT;
    return parseType(u, brFile, sMethod, parseStatus, vTimeZones, bIgnoreBeginError, encoding);
}

//---------------------------------------------------------------------
#if 0
t_int8 VEvent::compareTo(VEvent * ve)
{
    PR_ASSERT(ve != 0);
    // TODO;
    return -1;
}
#endif /* #if 0 */
//---------------------------------------------------------------------

Date VEvent::difference()
{
    if (getDTEnd().getTime() >= 0 && getDTStart().getTime() >= 0)
    {
        return (getDTEnd().getTime() - getDTStart().getTime());
    }
    else return 0;
}

//---------------------------------------------------------------------

void VEvent::selfCheck()
{

    TimeBasedEvent::selfCheck();

    // if no dtend, and there is a dtstart and duration, calculate dtend
    if (!getDTEnd().isValid())
    {
        // calculate DTEnd from duration and dtstart
        // TODO: remove memory leaks in m_TempDuration
        if (getDTStart().isValid() && m_TempDuration != 0 && m_TempDuration->isValid())
        {
            DateTime end;
            end = getDTStart();
            end.add(*m_TempDuration);
            setDTEnd(end);
            delete m_TempDuration; m_TempDuration = 0; // lifetime of variable is only
            // after load.
        }
        else if (!getDTStart().isValid())
        {
            if (m_TempDuration != 0)
                delete m_TempDuration; m_TempDuration = 0;
        }
    }

    // if is anniversary event, set transp to TRANSPARENT
    if (isAllDayEvent())
    {
        setTransp(JulianKeyword::Instance()->ms_sTRANSP);
        
        // if no dtend, set dtend to end of day of dtstart.
        if (!getDTEnd().isValid())
        {
            DateTime d;
            d = getDTStart();
            d.add(Calendar::DATE, 1);
            d.set(Calendar::HOUR_OF_DAY, 0);
            d.set(Calendar::MINUTE, 0);
            d.set(Calendar::SECOND, 0);
            setDTEnd(d);
        }

        // If DTEND is before DTSTART, set dtend = dtstart
        if (getDTEnd().getTime() < getDTStart().getTime())
        {
            DateTime d;
            d = getDTStart();
            setDTEnd(d);

            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDTEndBeforeDTStart, 100);
        }
    }
    else 
    {
        if (!getDTEnd().isValid())
        {
            // dtend is same as dtstart
            DateTime d;
            d = getDTStart();
            if (d.isValid())
                setDTEnd(d);
        }

        // If DTEND is before DTSTART, set dtend = dtstart
        if (getDTEnd().getTime() < getDTStart().getTime())
        {
            DateTime d;
            d = getDTStart();
            setDTEnd(d);

            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDTEndBeforeDTStart, 100);
        }
    }
}

//---------------------------------------------------------------------
/**
* return TRUE if the event falls on the given date
* @param aDate	a perticular date
*/
/*
t_bool VEvent::eventFallsOnTime(DateTime * aDate)
{
    DateTime dtS = getDTStart();
    DateTime dtE = getDTEnd();
    if (dtS != 0 && dtE != 0)
    {
        // DebugMsg.Instance().println(0,"DTStart: " + m_DTStart.toISO8601());
        // DebugMsg.Instance().println(0,"DTEnd: " + m_DTEnd.toISO8601());
        if ((dtS->before(aDate) && dtE->after(aDate))
            || dtS->equals(aDate) || dtE->equals(aDate))
    	  return TRUE;
        else 
          return FALSE;    
    }
    else
    {	
	// DebugMsg.Instance().println(0,"Warning: Either starting date or ending date missing.\n");
      return FALSE;
    }
}
*/
//---------------------------------------------------------------------

/**
* return TRUE if the event falls on the given time period
* @param StartTime	a starting time
* @param EndTime	an ending time
*/
/*
t_bool VEvent::eventMatchesTimePeriod(DateTime * SD, DateTime * ED)
{
    
    DateTime * dtS = getDTStart();
    DateTime * dtE = getDTEnd();
    
    if (dtS != 0 && dtE != 0)
    {
      if (dtE->before(SD)|| dtS->after(ED)
          || dtE->equals(SD)|| dtS->equals(ED))
        return FALSE;
      else 
        return TRUE;
    }
    else
    {	
        // DebugMsg.Instance().println(0,"Warning: Either starting date or ending date missing.\n");
    	return FALSE;
    }
}
*/
//---------------------------------------------------------------------

t_bool 
VEvent::storeData(UnicodeString & strLine, UnicodeString & propName, 
                  UnicodeString & propVal, JulianPtrArray * parameters, 
                  JulianPtrArray * vTimeZones)
{
    if (TimeBasedEvent::storeData(strLine, propName, propVal, 
                                  parameters, vTimeZones))
        return TRUE;
    else
    {
        //if (propName.compareIgnoreCase(ms_sDTEND) == 0)
        t_int32 hashCode = propName.hashCode();

        t_bool bParamValid;

        if (JulianKeyword::Instance()->ms_ATOM_DTEND == hashCode)
        {
            // check parameters
            bParamValid = ICalProperty::CheckParams(parameters, 
                JulianAtomRange::Instance()->ms_asTZIDValueParamRange,
                JulianAtomRange::Instance()->ms_asTZIDValueParamRangeSize,
                JulianAtomRange::Instance()->ms_asDateDateTimeValueRange,
                JulianAtomRange::Instance()->ms_asDateDateTimeValueRangeSize);
            if (!bParamValid)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                    JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
            }

            if (getDTEndProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVEVENT, propName, 100);
            }  
            UnicodeString u, out;
        
            u = JulianKeyword::Instance()->ms_sVALUE;
            out = ICalParameter::GetParameterFromVector(u, out, parameters);

            t_bool bIsDate = DateTime::IsParseableDate(propVal);

            if (bIsDate)
            {
                // if there is a VALUE=X parameter, make sure X is DATE
                if (out.size() != 0 && (JulianKeyword::Instance()->ms_ATOM_DATE != out.hashCode()))
                {
                    if (m_Log) m_Log->logString(
                        JulianLogErrorMessage::Instance()->ms_sPropertyValueTypeMismatch,
                        JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
                }   
            }
            else
            {
                // if there is a VALUE=X parameter, make sure X is DATETIME
                if (out.size() != 0 && (JulianKeyword::Instance()->ms_ATOM_DATETIME != out.hashCode()))
                {
                    if (m_Log) m_Log->logString(
                        JulianLogErrorMessage::Instance()->ms_sPropertyValueTypeMismatch,
                        JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
                }
            }

            DateTime d;
            d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);
            
            setDTEnd(d, parameters);
            return TRUE;
        }
        else if (JulianKeyword::Instance()->ms_ATOM_DURATION == hashCode)
        {
            // no parameters
            if (parameters->GetSize() > 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                    JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
            }

            if (m_TempDuration != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVEVENT, propName, 100);
            }
            //Duration d(propVal);
            if (m_TempDuration == 0)
            {
                m_TempDuration = new Duration(propVal);
                PR_ASSERT(m_TempDuration != 0);
            }
            else
                m_TempDuration->setDurationString(propVal);
            return TRUE;
        } 
        else if (JulianKeyword::Instance()->ms_ATOM_LOCATION == hashCode)
        {
            bParamValid = ICalProperty::CheckParams(parameters, 
                JulianAtomRange::Instance()->ms_asAltrepLanguageParamRange,
                JulianAtomRange::Instance()->ms_asAltrepLanguageParamRangeSize);
        
            if (!bParamValid)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                    JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
            }

            if (getLocationProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVEVENT, propName, 100);
            }
            setLocation(propVal, parameters);
            return TRUE;
        }
        else if (JulianKeyword::Instance()->ms_ATOM_PRIORITY == hashCode)
        {
            t_bool bParseError = FALSE;
            t_int32 i;

            // no parameters
            if (parameters->GetSize() > 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                    JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
            }

            i = JulianUtility::atot_int32(propVal.toCString(""), bParseError, propVal.size());
            if (getPriorityProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVEVENT, propName, 100);
            }
            if (!bParseError)
            {
                setPriority(i, parameters);
            }
            else
            {
                if (m_Log) m_Log->logString(JulianLogErrorMessage::Instance()->ms_sInvalidNumberFormat, 
                    JulianKeyword::Instance()->ms_sVEVENT, propName, propVal, 200);
            }
            return TRUE;
        } 
        else if (JulianKeyword::Instance()->ms_ATOM_RESOURCES == hashCode)
        {
            // check parameters 
            bParamValid = ICalProperty::CheckParams(parameters, 
                JulianAtomRange::Instance()->ms_asLanguageParamRange,
                JulianAtomRange::Instance()->ms_asLanguageParamRangeSize);
            if (!bParamValid)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                    JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
            }

            addResourcesPropertyVector(propVal, parameters);
            return TRUE;
        }
        else if (JulianKeyword::Instance()->ms_ATOM_GEO == hashCode)
        {
            // no parameters
            if (parameters->GetSize() > 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                    JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
            }

            if (getGEOProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVEVENT, propName, 100);
            }
            setGEO(propVal, parameters);
            return TRUE;
        } 
        else if (JulianKeyword::Instance()->ms_ATOM_TRANSP == hashCode)
        {
            // no parameters
            if (parameters->GetSize() > 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                    JulianKeyword::Instance()->ms_sVEVENT, strLine, 100);
            }

            if (getTranspProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVEVENT, propName, 100);
            }
            setTransp(propVal, parameters);
            return TRUE;
        }  
        else
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidPropertyName, 
                JulianKeyword::Instance()->ms_sVEVENT, propName, 200);
            UnicodeString u;
            u = JulianLogErrorMessage::Instance()->ms_sRS202;
            u += '.'; u += ' ';
            u += strLine;
            //setRequestStatus(JulianLogErrorMessage::Instance()->ms_sRS202); 
            setRequestStatus(u);
            return FALSE;
        }
    }
}

//---------------------------------------------------------------------
/// note: todo: crash proof, bulletproofed
void VEvent::populateDatesHelper(DateTime start, Date ldiff, 
                                 JulianPtrArray * vPeriods)
{
    Date diff = ldiff;
    Date diff2 = -1;

    if (vPeriods != 0)
    {
        // Check if period matches this date, if-so, set
        // difference to be length of period
        // TODO: if multiple periods match this date, choose period
        // with longest length

        t_int32 i;
        UnicodeString u;
        Period p;
        DateTime ps, pe;
        Date pl;
        for (i = 0; i < vPeriods->GetSize(); i++)
        {   
            u = *((UnicodeString *) vPeriods->GetAt(i));
            
            //if (FALSE) TRACE("populateDatesHelper(): u = %s\r\n", u.toCString(""));

            p.setPeriodString(u);
            PR_ASSERT(p.isValid());
            if (p.isValid())
            {
                ps = p.getStart();
                
                if (ps.equalsDateTime(start))
                {
                    pe = p.getEndingTime(pe);
                    pl = (pe.getTime() - ps.getTime());
                    //if (FALSE) TRACE("populateDatesHelper(): pl = %d\r\n", pl);

                    diff2 = ((diff2 > pl) ? diff2 : pl);
                }
            }
        }
    }
    if (diff2 != -1)
        diff = diff2;

    DateTime dEnd;
    dEnd.setTime(start.getTime() + diff);
    setDTEnd(dEnd);
    setMyOrigEnd(dEnd);

    DateTime origEnd;
    origEnd.setTime(getOrigStart().getTime() + ldiff);
    setOrigEnd(origEnd);
}

//---------------------------------------------------------------------

UnicodeString VEvent::formatHelper(UnicodeString & strFmt, 
                                   UnicodeString sFilterAttendee, 
                                   t_bool delegateRequest) 
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVEVENT;
    return ICalComponent::format(u, strFmt, sFilterAttendee, delegateRequest);
}

//---------------------------------------------------------------------

UnicodeString VEvent::toString()
{
    return ICalComponent::toStringFmt(
        JulianFormatString::Instance()->ms_VEventStrDefaultFmt);
}

//---------------------------------------------------------------------

UnicodeString VEvent::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
    UnicodeString sResult;
    switch ( c )
    {
    case ms_cDuration:
        return getDuration().toString();
        //return ICalProperty::propertyToString(getDurationProperty(), dateFmt, sResult);
    case ms_cDTEnd:
        return ICalProperty::propertyToString(getDTEndProperty(), dateFmt, sResult);
    case ms_cGEO:
        return ICalProperty::propertyToString(getGEOProperty(), dateFmt, sResult);
    case ms_cLocation:
        return ICalProperty::propertyToString(getLocationProperty(), dateFmt, sResult);
    case ms_cTransp:
        return ICalProperty::propertyToString(getTranspProperty(), dateFmt, sResult);
    case ms_cPriority:
        return ICalProperty::propertyToString(getPriorityProperty(), dateFmt, sResult);
    case ms_cResources:
        return ICalProperty::propertyVectorToString(getResources(), dateFmt, sResult);
    default:
        return TimeBasedEvent::toStringChar(c, dateFmt);
    }
}

//---------------------------------------------------------------------

UnicodeString VEvent::formatChar(t_int32 c, UnicodeString sFilterAttendee, 
                                 t_bool delegateRequest) 
{
    UnicodeString s, sResult;
    switch (c) {
    case ms_cDTEnd:
        s = JulianKeyword::Instance()->ms_sDTEND;
        return ICalProperty::propertyToICALString(s, getDTEndProperty(), sResult);
    case ms_cDuration:
        s = JulianKeyword::Instance()->ms_sDURATION;
        s += ':';
        s += getDuration().toICALString();
        s += JulianKeyword::Instance()->ms_sLINEBREAK;
        return s;
        //return ICalProperty::propertyToICALString(s, getDurationProperty(), sResult);
    case ms_cLocation: 
        s = JulianKeyword::Instance()->ms_sLOCATION;
        return ICalProperty::propertyToICALString(s, getLocationProperty(), sResult);
    case ms_cPriority: 
        s = JulianKeyword::Instance()->ms_sPRIORITY;
        return ICalProperty::propertyToICALString(s, getPriorityProperty(), sResult);    
    case ms_cResources:
        s = JulianKeyword::Instance()->ms_sRESOURCES;
        return ICalProperty::propertyVectorToICALString(s, getResources(), sResult);    
    case ms_cGEO: 
        s = JulianKeyword::Instance()->ms_sGEO;
        return ICalProperty::propertyToICALString(s, getGEOProperty(), sResult);     
    case ms_cTransp:
        s = JulianKeyword::Instance()->ms_sTRANSP;
        return ICalProperty::propertyToICALString(s, getTranspProperty(), sResult);
    default:
        {
            return TimeBasedEvent::formatChar(c, sFilterAttendee, delegateRequest);
        }
    }
}

//---------------------------------------------------------------------

t_bool VEvent::isValid()
{
    if ((getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sPUBLISH) == 0) ||
        (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREQUEST) == 0) ||
        (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sADD) == 0))
    {
        // must have dtstart
        if ((!getDTStart().isValid()))
            return FALSE;

        // If dtend exists, make sure it is not before dtstart
        if (getDTEnd().isValid() && getDTEnd() < getDTStart())
            return FALSE;

        // must have dtstamp, summary, uid
        if ((!getDTStamp().isValid()) || (getSummary().size() == 0) || 
            (getUID().size() == 0))
            return FALSE;

        // TODO: must have organizer, comment out for now since CS&T doesn't have ORGANIZER
        /*
        if (getOrganizer().size() <= 0)
            return FALSE;
        */

        // must have sequence >= 0
        if (getSequence() < 0)
            return FALSE;

        if (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREQUEST) == 0)
        {
            if (getAttendees() == 0 || getAttendees()->GetSize() == 0)
                return FALSE;
        }
    }
    else if ((getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREPLY) == 0) ||
             (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sCANCEL) == 0) ||
             (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sDECLINECOUNTER) == 0))
    {
         // must have dtstamp, uid
        if ((!getDTStamp().isValid()) || (getUID().size() == 0))
            return FALSE;

        // must have organizer
        if (getOrganizer().size() <= 0)
            return FALSE;
    
        // must have sequence >= 0
        if (getSequence() < 0)
            return FALSE;
        
        if (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREPLY) == 0)
        {
            // TODO: attendees are required, commenting out for now
            if (getAttendees() == 0 || getAttendees()->GetSize() == 0)
                return FALSE;
        }
    }
    else if ((getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREFRESH) == 0))
    {
        // must have dtstamp, uid
        if ((!getDTStamp().isValid()) || (getUID().size() == 0))
            return FALSE;
        // TODO: attendees required?
    }
    else if ((getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sCOUNTER) == 0))
    {
        // must have dtstamp, uid
        if ((!getDTStamp().isValid()) || (getUID().size() == 0))
            return FALSE;

        // must have sequence >= 0
        if (getSequence() < 0)
            return FALSE;
    }
            

    // check super class isValid method
    return TimeBasedEvent::isValid();
}

//---------------------------------------------------------------------
#if 0
t_bool VEvent::isCriticalInfoSame()
{
    // TODO finish
    return FALSE;
}
#endif /* #if 0 */
//---------------------------------------------------------------------

UnicodeString VEvent::cancelMessage() 
{
    UnicodeString s = JulianKeyword::Instance()->ms_sCANCELLED;
    setStatus(s);
    return formatHelper(JulianFormatString::Instance()->ms_sVEventCancelMessage);
}

//---------------------------------------------------------------------

UnicodeString VEvent::requestMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventRequestMessage);
}

//---------------------------------------------------------------------

UnicodeString VEvent::requestRecurMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventRecurRequestMessage);
}

//---------------------------------------------------------------------
 
UnicodeString VEvent::counterMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventCounterMessage);
}

//---------------------------------------------------------------------
  
UnicodeString VEvent::declineCounterMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventDeclineCounterMessage);
}

//---------------------------------------------------------------------

UnicodeString VEvent::addMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventAddMessage);
}

//---------------------------------------------------------------------
  
UnicodeString VEvent::refreshMessage(UnicodeString sAttendeeFilter) 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventRefreshMessage, sAttendeeFilter);
}

//---------------------------------------------------------------------
 
UnicodeString VEvent::allMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventAllPropertiesMessage);
}

//---------------------------------------------------------------------
 
UnicodeString VEvent::replyMessage(UnicodeString sAttendeeFilter) 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventReplyMessage, sAttendeeFilter);
}

//---------------------------------------------------------------------
 
UnicodeString VEvent::publishMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventPublishMessage);
}

//---------------------------------------------------------------------
 
UnicodeString VEvent::publishRecurMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVEventRecurPublishMessage);
}

//---------------------------------------------------------------------
///DTEnd
void VEvent::setDTEnd(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_DTEnd == 0)
        m_DTEnd = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_DTEnd->setValue((void *) &s);
        m_DTEnd->setParameters(parameters);
    }
    //validateDTEnd();
}
DateTime VEvent::getDTEnd() const 
{
    DateTime d(-1);
    if (m_DTEnd == 0)
        return d; //return 0;
    else
    {
        d = *((DateTime *) m_DTEnd->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
///Duration
void VEvent::setDuration(Duration s, JulianPtrArray * parameters)
{ 
    /*
    if (m_Duration == 0)
        m_Duration = ICalPropertyFactory::Make(ICalProperty::DURATION, 
                                            (void *) &s, parameters);
    else
    {
        m_Duration->setValue((void *) &s);
        m_Duration->setParameters(parameters);
    }
    //validateDuration();
    */
    // NOTE: remove later, to avoid compiler warnings
    if (parameters) 
    {
    }

    
    DateTime end;
    end = getDTStart();
    end.add(s);
    setDTEnd(end);
}
Duration VEvent::getDuration() const 
{
    /*
    Duration d; d.setMonth(-1);
    if (m_Duration == 0)
        return d; // return 0;
    else
    {
        d = *((Duration *) m_Duration->getValue());
        return d;
    }
    */
    Duration duration;
    DateTime start, end;
    start = getDTStart();
    end = getDTEnd();
    DateTime::getDuration(start,end,duration);
    return duration;
}
//---------------------------------------------------------------------
//GEO
void VEvent::setGEO(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_GEO == 0)
        m_GEO = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_GEO->setValue((void *) &s);
        m_GEO->setParameters(parameters);
    }
}
UnicodeString VEvent::getGEO() const 
{
    if (m_GEO == 0)
        return "";
    else
        return *((UnicodeString *) m_GEO->getValue());
}

//---------------------------------------------------------------------
//Location
void VEvent::setLocation(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Location == 0)
    {
        m_Location = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                           (void *) &s, parameters);
    }
    else
    {
        m_Location->setValue((void *) &s);
        m_Location->setParameters(parameters);
    }
}
UnicodeString VEvent::getLocation() const  
{
    if (m_Location == 0)
        return "";
    else
        return *((UnicodeString *) m_Location->getValue());
}
//---------------------------------------------------------------------
//Priority
void VEvent::setPriority(t_int32 i, JulianPtrArray * parameters)
{
    if (m_Priority == 0)
        m_Priority = ICalPropertyFactory::Make(ICalProperty::INTEGER, 
                                            (void *) &i, parameters);
    else
    {
        m_Priority->setValue((void *) &i);
        m_Priority->setParameters(parameters);
    }
}
t_int32 VEvent::getPriority() const 
{
    if (m_Priority == 0)
        return -1;
    else
        return *((t_int32 *) m_Priority->getValue());
}
//---------------------------------------------------------------------
//Transp
void VEvent::setTransp(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Transp == 0)
        m_Transp = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Transp->setValue((void *) &s);
        m_Transp->setParameters(parameters);
    }
}
UnicodeString VEvent::getTransp() const  
{
    if (m_Transp == 0)
        return "";
    else
        return *((UnicodeString *) m_Transp->getValue());
}
//---------------------------------------------------------------------
// RESOURCES
void VEvent::addResources(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *) &s, parameters);
    addResourcesProperty(prop);
}

//---------------------------------------------------------------------

void VEvent::addResourcesProperty(ICalProperty * prop)
{
    if (m_ResourcesVctr == 0)
        m_ResourcesVctr = new JulianPtrArray(); 
    PR_ASSERT(m_ResourcesVctr != 0);
    if (m_ResourcesVctr != 0)
    {
        m_ResourcesVctr->Add(prop);
    }
}

//---------------------------------------------------------------------

void
VEvent::addResourcesPropertyVector(UnicodeString & propVal,
                                   JulianPtrArray * parameters)
{
    //ICalProperty * ip;

    //ip = ICalPropertyFactory::Make(ICalProperty::TEXT, new UnicodeString(propVal),
    //        parameters);
    addResources(propVal, parameters);
    //addResourcesProperty(ip);
  

#if 0
    // TODO: see TimeBasedEvent::addCategoriesPropertyVector()

    ErrorCode status = ZERO_ERROR;
    UnicodeStringTokenizer * st;
    UnicodeString us; 
    UnicodeString sDelim = ",";

    st = new UnicodeStringTokenizer(propVal, sDelim);
    //ICalProperty * ip;
    while (st->hasMoreTokens())
    {
        us = st->nextToken(us, status);
        us.trim();
        //if (FALSE) TRACE("us = %s, status = %d", us.toCString(""), status);
        // TODO: if us is in Resources range then add, else, log and error
        addResourcesProperty(ICalPropertyFactory::Make(ICalProperty::TEXT,
            new UnicodeString(us), parameters));
    }
    delete st; st = 0;
#endif

}
//---------------------------------------------------------------------

