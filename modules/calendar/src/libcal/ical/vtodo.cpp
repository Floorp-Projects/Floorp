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
 * vtodo.cpp
 * John Sun
 * 4/22/98 3:41:09 PM
 */

#include "stdafx.h"
#include "jdefines.h"

#include "jutility.h"
#include "vtodo.h"
#include "icalredr.h"
#include "prprtyfy.h"
#include "unistrto.h"
#include "jlog.h"
#include "vtimezne.h"
#include "keyword.h"
#include "period.h"
#include "datetime.h"

//---------------------------------------------------------------------
void VTodo::setDefaultFmt(UnicodeString s)
{
    JulianFormatString::Instance()->ms_VTodoStrDefaultFmt = s;
}
//---------------------------------------------------------------------
#if 0
VTodo::VTodo()
{
    PR_ASSERT(FALSE);
}
#endif
//---------------------------------------------------------------------

VTodo::VTodo(JLog * initLog)
: m_Due(0),
  m_Completed(0),
  m_TempDuration(0),
  m_GEO(0), 
  m_Location(0),
  m_PercentComplete(0),
  m_Priority(0),
  m_ResourcesVctr(0),
  TimeBasedEvent(initLog)
{
}

//---------------------------------------------------------------------

VTodo::VTodo(VTodo & that)
: m_Due(0),
  m_Completed(0),
  m_TempDuration(0),
  m_GEO(0),
  m_Location(0),
  m_PercentComplete(0),
  m_Priority(0),
  m_ResourcesVctr(0),
  TimeBasedEvent(that)
{
    m_origDue = that.m_origDue;
    m_origMyDue = that.m_origMyDue;
    
    if (that.m_Completed != 0) { m_Completed = that.m_Completed->clone(m_Log); }
    if (that.m_Due != 0) { m_Due = that.m_Due->clone(m_Log); }
    //if (that.m_Duration != 0) { m_Duration = that.m_Duration->clone(m_Log); }
    if (that.m_GEO != 0) { m_GEO = that.m_GEO->clone(m_Log); }
    if (that.m_Location != 0) { m_Location = that.m_Location->clone(m_Log); }
    if (that.m_PercentComplete != 0) { m_PercentComplete = that.m_PercentComplete->clone(m_Log); }
    if (that.m_Priority != 0) { m_Priority = that.m_Priority->clone(m_Log); }
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
VTodo::clone(JLog * initLog)
{
    m_Log = initLog;
    return new VTodo(*this);
}

//---------------------------------------------------------------------

VTodo::~VTodo()
{
    // properties
    if (m_Completed != 0) { delete m_Completed; m_Completed = 0; }
    if (m_Due != 0) { delete m_Due; m_Due = 0; }
    //if (m_Duration != 0) { delete m_Duration; m_Duration = 0; }
    if (m_GEO != 0) { delete m_GEO; m_GEO = 0; }
    if (m_Location!= 0) { delete m_Location; m_Location = 0; }
    if (m_PercentComplete != 0) { delete m_PercentComplete; m_PercentComplete = 0; }
    if (m_Priority!= 0) { delete m_Priority; m_Priority = 0; }
    
    // vector of strings
    if (m_ResourcesVctr != 0) { 
        ICalProperty::deleteICalPropertyVector(m_ResourcesVctr);
        delete m_ResourcesVctr; m_ResourcesVctr = 0; 
    }
    //delete (TimeBasedEvent *) this;
}

//---------------------------------------------------------------------

UnicodeString &
VTodo::parse(ICalReader * brFile, UnicodeString & sMethod, 
             UnicodeString & parseStatus, JulianPtrArray * vTimeZones,
             t_bool bIgnoreBeginError, JulianUtility::MimeEncoding encoding) 
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVTODO;
    return parseType(u, brFile, sMethod, parseStatus, vTimeZones, bIgnoreBeginError, encoding);
}

//---------------------------------------------------------------------

Date VTodo::difference()
{
    if (getDue().getTime() >= 0 && getDTStart().getTime() >= 0)
    {
        return (getDue().getTime() - getDTStart().getTime());
    }
    else return 0;
}

//---------------------------------------------------------------------

void VTodo::selfCheck()
{
    TimeBasedEvent::selfCheck();
    
    // if no due, and there is a dtstart and duration, calculate due
    if (!getDue().isValid())
    {
        // calculate Due from duration and dtstart
        // TODO: remove memory leaks in m_TempDuration
        if (getDTStart().isValid() && m_TempDuration != 0 && m_TempDuration->isValid())
        {
            DateTime due;
            due = getDTStart();
            due.add(*m_TempDuration);
            setDue(due);
            delete m_TempDuration; m_TempDuration = 0;
        }
        else if (!getDTStart().isValid())
        {
            if (m_TempDuration != 0)
                delete m_TempDuration; m_TempDuration = 0;
        }
    }
}

//---------------------------------------------------------------------

t_bool 
VTodo::storeData(UnicodeString & strLine, UnicodeString & propName, 
                 UnicodeString & propVal, JulianPtrArray * parameters, 
                 JulianPtrArray * vTimeZones)
{
    if (TimeBasedEvent::storeData(strLine, propName, propVal, 
                                  parameters, vTimeZones))
        return TRUE;
    else
    {
        //if (propName.compareIgnoreCase(ms_sDUE) == 0)
        t_int32 hashCode = propName.hashCode();

        t_bool bParamValid;

        if (JulianKeyword::Instance()->ms_ATOM_DUE == hashCode)
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
                    JulianKeyword::Instance()->ms_sVTODO, strLine, 100);
            }

            if (getDueProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVTODO, propName, 100);
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
                        JulianKeyword::Instance()->ms_sVTODO, strLine, 100);
                }   
            }
            else
            {
                // if there is a VALUE=X parameter, make sure X is DATETIME
                if (out.size() != 0 && (JulianKeyword::Instance()->ms_ATOM_DATETIME != out.hashCode()))
                {
                    if (m_Log) m_Log->logString(
                        JulianLogErrorMessage::Instance()->ms_sPropertyValueTypeMismatch,
                        JulianKeyword::Instance()->ms_sVTODO, strLine, 100);
                }
            }

            DateTime d;
            d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);
            
            setDue(d, parameters);
            return TRUE;
        }    
        else if (JulianKeyword::Instance()->ms_ATOM_COMPLETED == hashCode)
        {
            // no parameters (MUST BE IN UTC)
            if (parameters->GetSize() > 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                    JulianKeyword::Instance()->ms_sVTODO, strLine, 100);
            }
            if (getCompletedProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVTODO, propName, 100);
            }
            DateTime d;
            d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);
    
            setCompleted(d, parameters);   
            return TRUE;
        }    
        else if (JulianKeyword::Instance()->ms_ATOM_DURATION == hashCode)
        {
            // no parameters
            if (parameters->GetSize() > 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                    JulianKeyword::Instance()->ms_sVTODO, strLine, 100);
            }

            if (m_TempDuration != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVTODO, propName, 100);
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
                    JulianKeyword::Instance()->ms_sVTODO, strLine, 100);
            }

            if (getLocationProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVTODO, propName, 100);
            }
            setLocation(propVal, parameters);
            return TRUE;
        }
        else if (JulianKeyword::Instance()->ms_ATOM_PERCENTCOMPLETE == hashCode)
        {
            t_bool bParseError = FALSE;
            t_int32 i;

            // no parameters
            if (parameters->GetSize() > 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                    JulianKeyword::Instance()->ms_sVTODO, strLine, 100);
            }

            i = JulianUtility::atot_int32(propVal.toCString(""), bParseError, propVal.size());
            if (getPercentCompleteProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVTODO, propName, 100);
            }
            if (!bParseError)
            {
                if (i > 100)
                {
                    if (m_Log) m_Log->logString(
                        JulianLogErrorMessage::Instance()->ms_sRoundedPercentCompleteTo100, 
                        JulianKeyword::Instance()->ms_sVTODO, propName, 100);
                    i = 100;    
                }
                setPercentComplete(i, parameters);
            }
            else
            {
                if (m_Log) m_Log->logString(JulianLogErrorMessage::Instance()->ms_sInvalidNumberFormat, 
                    JulianKeyword::Instance()->ms_sVTODO, propName, propVal, 200);
            }
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
                    JulianKeyword::Instance()->ms_sVTODO, strLine, 100);
            }

            i = JulianUtility::atot_int32(propVal.toCString(""), bParseError, propVal.size());
            if (getPriorityProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVTODO, propName, 100);
            }
            if (!bParseError)
            {
                setPriority(i, parameters);
            }
            else
            {
                if (m_Log) m_Log->logString(JulianLogErrorMessage::Instance()->ms_sInvalidNumberFormat, 
                    JulianKeyword::Instance()->ms_sVTODO, propName, propVal, 200);
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
                    JulianKeyword::Instance()->ms_sVTODO, strLine, 100);
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
                    JulianKeyword::Instance()->ms_sVTODO, strLine, 100);
            }

            if (getGEOProperty() != 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVTODO, propName, 100);
            }
            setGEO(propVal, parameters);
            return TRUE;
        } 
        else
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidPropertyName, 
                JulianKeyword::Instance()->ms_sVTODO, propName, 200);
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

void VTodo::populateDatesHelper(DateTime start, Date ldiff, 
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

    DateTime dDue;
    dDue.setTime(start.getTime() + diff);
    setDue(dDue);
    setMyOrigDue(dDue);

    DateTime origDue;
    origDue.setTime(getOrigStart().getTime() + ldiff);
    setOrigDue(origDue);
}

//---------------------------------------------------------------------

UnicodeString VTodo::formatHelper(UnicodeString & strFmt, 
                                  UnicodeString sFilterAttendee, 
                                  t_bool delegateRequest) 
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVTODO;
    return ICalComponent::format(u, strFmt, sFilterAttendee, delegateRequest);
}

//---------------------------------------------------------------------

UnicodeString VTodo::toString()
{
    return ICalComponent::toStringFmt(
        JulianFormatString::Instance()->ms_VTodoStrDefaultFmt);
}

//---------------------------------------------------------------------

UnicodeString VTodo::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
    UnicodeString sResult;
    switch ( c )
    {
    case ms_cCompleted:
        return ICalProperty::propertyToString(getCompletedProperty(), dateFmt, sResult);
    case ms_cDue:
        return ICalProperty::propertyToString(getDueProperty(), dateFmt, sResult);
    case ms_cDuration:
        return getDuration().toString();
        //return ICalProperty::propertyToString(getDurationProperty(), dateFmt, sResult);
    case ms_cGEO:
        return ICalProperty::propertyToString(getGEOProperty(), dateFmt, sResult);
    case ms_cLocation:
        return ICalProperty::propertyToString(getLocationProperty(), dateFmt, sResult);
    case ms_cPercentComplete:
        return ICalProperty::propertyToString(getPercentCompleteProperty(), dateFmt, sResult);
    case ms_cPriority:
        return ICalProperty::propertyToString(getPriorityProperty(), dateFmt, sResult);
    case ms_cResources:
        return ICalProperty::propertyVectorToString(getResources(), dateFmt, sResult);
    default:
        return TimeBasedEvent::toStringChar(c, dateFmt);
    }
}

//---------------------------------------------------------------------

UnicodeString VTodo::formatChar(t_int32 c, UnicodeString sFilterAttendee, 
                                 t_bool delegateRequest) 
{
    UnicodeString s, sResult;
    switch (c) {
    case ms_cCompleted:
        s = JulianKeyword::Instance()->ms_sCOMPLETED;
        return ICalProperty::propertyToICALString(s, getCompletedProperty(), sResult);
    case ms_cDue:
        s = JulianKeyword::Instance()->ms_sDUE;
        return ICalProperty::propertyToICALString(s, getDueProperty(), sResult);
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
    case ms_cPercentComplete:
        s = JulianKeyword::Instance()->ms_sPERCENTCOMPLETE;
        return ICalProperty::propertyToICALString(s, getPercentCompleteProperty(), sResult);
    case ms_cPriority: 
        s = JulianKeyword::Instance()->ms_sPRIORITY;
        return ICalProperty::propertyToICALString(s, getPriorityProperty(), sResult);    
    case ms_cResources:
        s = JulianKeyword::Instance()->ms_sRESOURCES;
        return ICalProperty::propertyVectorToICALString(s, getResources(), sResult);    
    case ms_cGEO: 
        s = JulianKeyword::Instance()->ms_sGEO;
        return ICalProperty::propertyToICALString(s, getGEOProperty(), sResult);     
    default:
        {
            return TimeBasedEvent::formatChar(c, sFilterAttendee, delegateRequest);
        }
    }
}

//---------------------------------------------------------------------

t_bool VTodo::isValid()
{
    if ((getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sPUBLISH) == 0) ||
        (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREQUEST) == 0) ||
        (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sADD) == 0))
    {
        // must have dtstart
        if ((!getDTStart().isValid()))
            return FALSE;

        // If due exists, make sure it is not before dtstart
        //if (getDTEnd().isValid() && getDTEnd() < getDTStart())
        //    return FALSE;

        // must have dtstamp, summary, uid
        if ((!getDTStamp().isValid()) || (getSummary().size() == 0) || 
            (getUID().size() == 0))
            return FALSE;

        // must have organizer
        if (getOrganizer().size() <= 0)
            return FALSE;
    
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

UnicodeString VTodo::cancelMessage() 
{
    UnicodeString s = JulianKeyword::Instance()->ms_sCANCELLED;
    setStatus(s);
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoCancelMessage);
}

//---------------------------------------------------------------------

UnicodeString VTodo::requestMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoRequestMessage);
}

//---------------------------------------------------------------------

UnicodeString VTodo::requestRecurMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoRecurRequestMessage);
}

//---------------------------------------------------------------------
 
UnicodeString VTodo::counterMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoCounterMessage);
}

//---------------------------------------------------------------------
  
UnicodeString VTodo::declineCounterMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoDeclineCounterMessage);
}

//---------------------------------------------------------------------

UnicodeString VTodo::addMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoAddMessage);
}

//---------------------------------------------------------------------
  
UnicodeString VTodo::refreshMessage(UnicodeString sAttendeeFilter) 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoRefreshMessage, sAttendeeFilter);
}

//---------------------------------------------------------------------
 
UnicodeString VTodo::allMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoAllPropertiesMessage);
}

//---------------------------------------------------------------------
 
UnicodeString VTodo::replyMessage(UnicodeString sAttendeeFilter) 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoReplyMessage, sAttendeeFilter);
}

//---------------------------------------------------------------------
 
UnicodeString VTodo::publishMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoPublishMessage);
}

//---------------------------------------------------------------------
 
UnicodeString VTodo::publishRecurMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVTodoRecurPublishMessage);
}

//---------------------------------------------------------------------

//---------------------------------------------------------------------
///Completed
void VTodo::setCompleted(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_Completed == 0)
        m_Completed = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_Completed->setValue((void *) &s);
        m_Completed->setParameters(parameters);
    }
    //validateCompleted();
}
DateTime VTodo::getCompleted() const 
{
    DateTime d(-1);
    if (m_Completed == 0)
        return d; //return 0;
    else
    {
        d = *((DateTime *) m_Completed->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
///Due
void VTodo::setDue(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_Due == 0)
        m_Due = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_Due->setValue((void *) &s);
        m_Due->setParameters(parameters);
    }
    //validateDue();
}
DateTime VTodo::getDue() const 
{
    DateTime d(-1);
    if (m_Due == 0)
        return d; //return 0;
    else
    {
        d = *((DateTime *) m_Due->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
///Duration
void VTodo::setDuration(Duration s, JulianPtrArray * parameters)
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

    
    DateTime due;
    due = getDTStart();
    due.add(s);
    setDue(due);
}
Duration VTodo::getDuration() const 
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
    DateTime start, due;
    start = getDTStart();
    due = getDue();
    DateTime::getDuration(start,due,duration);
    return duration;
}
//---------------------------------------------------------------------
//GEO
void VTodo::setGEO(UnicodeString s, JulianPtrArray * parameters)
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
UnicodeString VTodo::getGEO() const 
{
    if (m_GEO == 0)
        return "";
    else
        return *((UnicodeString *) m_GEO->getValue());
}

//---------------------------------------------------------------------
//Location
void VTodo::setLocation(UnicodeString s, JulianPtrArray * parameters)
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
UnicodeString VTodo::getLocation() const  
{
    if (m_Location == 0)
        return "";
    else
        return *((UnicodeString *) m_Location->getValue());
}
//---------------------------------------------------------------------
//PercentComplete
void VTodo::setPercentComplete(t_int32 i, JulianPtrArray * parameters)
{
    if (m_PercentComplete == 0)
        m_PercentComplete = ICalPropertyFactory::Make(ICalProperty::INTEGER, 
                                            (void *) &i, parameters);
    else
    {
        m_PercentComplete->setValue((void *) &i);
        m_PercentComplete->setParameters(parameters);
    }
}
t_int32 VTodo::getPercentComplete() const 
{
    if (m_PercentComplete == 0)
        return -1;
    else
        return *((t_int32 *) m_PercentComplete->getValue());
}
//---------------------------------------------------------------------
//Priority
void VTodo::setPriority(t_int32 i, JulianPtrArray * parameters)
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
t_int32 VTodo::getPriority() const 
{
    if (m_Priority == 0)
        return -1;
    else
        return *((t_int32 *) m_Priority->getValue());
}
//---------------------------------------------------------------------
// RESOURCES
void VTodo::addResources(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *) &s, parameters);
    addResourcesProperty(prop);
}

//---------------------------------------------------------------------

void VTodo::addResourcesProperty(ICalProperty * prop)
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
VTodo::addResourcesPropertyVector(UnicodeString & propVal,
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
    UnicodeString sDelim = JulianKeyword::Instance()->ms_sCOMMA_SYMBOL;

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

