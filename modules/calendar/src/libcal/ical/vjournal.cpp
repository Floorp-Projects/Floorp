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
 * vjournal.cpp
 * John Sun
 * 4/23/98 10:33:36 AM
 */

#include "stdafx.h"
#include "jdefines.h"

#include "jutility.h"
#include "vjournal.h"
#include "icalredr.h"
#include "prprtyfy.h"
#include "unistrto.h"
#include "jlog.h"
#include "vtimezne.h"
#include "keyword.h"
#include "period.h"
#include "datetime.h"

//---------------------------------------------------------------------
void VJournal::setDefaultFmt(UnicodeString s)
{
    JulianFormatString::Instance()->ms_VJournalStrDefaultFmt = s;
}
//---------------------------------------------------------------------
#if 0
VJournal::VJournal()
{
    PR_ASSERT(FALSE);
}
#endif
//---------------------------------------------------------------------

VJournal::VJournal(JLog * initLog)
: TimeBasedEvent(initLog)
{
}

//---------------------------------------------------------------------

VJournal::VJournal(VJournal & that)
: TimeBasedEvent(that)
{
}

//---------------------------------------------------------------------

ICalComponent *
VJournal::clone(JLog * initLog)
{
    m_Log = initLog;
    return new VJournal(*this);
}

//---------------------------------------------------------------------

VJournal::~VJournal()
{
    // should call TimeBasedEvent destructor
}

//---------------------------------------------------------------------

UnicodeString &
VJournal::parse(ICalReader * brFile, UnicodeString & sMethod, 
                UnicodeString & parseStatus, JulianPtrArray * vTimeZones,
                t_bool bIgnoreBeginError, JulianUtility::MimeEncoding encoding) 
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVJOURNAL;
    return parseType(u, brFile, sMethod, parseStatus, vTimeZones, bIgnoreBeginError, encoding);
}

//---------------------------------------------------------------------

Date VJournal::difference()
{
    return 0;
}

//---------------------------------------------------------------------

void VJournal::selfCheck()
{
    TimeBasedEvent::selfCheck();   
}

//---------------------------------------------------------------------

t_bool 
VJournal::storeData(UnicodeString & strLine, UnicodeString & propName, 
                    UnicodeString & propVal, JulianPtrArray * parameters, 
                    JulianPtrArray * vTimeZones)
{
    if (TimeBasedEvent::storeData(strLine, propName, propVal, 
                                  parameters, vTimeZones))
        return TRUE;
    else
    {
        if (m_Log) m_Log->logString(
            JulianLogErrorMessage::Instance()->ms_sInvalidPropertyName, 
            JulianKeyword::Instance()->ms_sVJOURNAL, propName, 200);
        UnicodeString u;
        u = JulianLogErrorMessage::Instance()->ms_sRS202;
        u += '.'; u += ' ';
        u += strLine;
        //setRequestStatus(JulianLogErrorMessage::Instance()->ms_sRS202); 
        setRequestStatus(u);
        return FALSE;
    }
}

//---------------------------------------------------------------------

void VJournal::populateDatesHelper(DateTime start, Date ldiff, 
                                   JulianPtrArray * vPeriods)
{
}

//---------------------------------------------------------------------

UnicodeString VJournal::formatHelper(UnicodeString & strFmt, 
                                     UnicodeString sFilterAttendee, 
                                     t_bool delegateRequest) 
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVJOURNAL;
    return ICalComponent::format(u, strFmt, sFilterAttendee, delegateRequest);
}

//---------------------------------------------------------------------

UnicodeString VJournal::toString()
{
    return ICalComponent::toStringFmt(
        JulianFormatString::Instance()->ms_VJournalStrDefaultFmt);
}

//---------------------------------------------------------------------

UnicodeString VJournal::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
    return TimeBasedEvent::toStringChar(c, dateFmt);
}

//---------------------------------------------------------------------

UnicodeString VJournal::formatChar(t_int32 c, UnicodeString sFilterAttendee, 
                                 t_bool delegateRequest) 
{
    return TimeBasedEvent::formatChar(c, sFilterAttendee, delegateRequest);
}

//---------------------------------------------------------------------

t_bool VJournal::isValid()
{
    // TODO: finish
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

UnicodeString VJournal::cancelMessage() 
{
    UnicodeString s = JulianKeyword::Instance()->ms_sCANCELLED;
    setStatus(s);
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalCancelMessage);
}

//---------------------------------------------------------------------

UnicodeString VJournal::requestMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalRequestMessage);
}

//---------------------------------------------------------------------

UnicodeString VJournal::requestRecurMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalRecurRequestMessage);
}

//---------------------------------------------------------------------
 
UnicodeString VJournal::counterMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalCounterMessage);
}

//---------------------------------------------------------------------
  
UnicodeString VJournal::declineCounterMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalDeclineCounterMessage);
}

//---------------------------------------------------------------------

UnicodeString VJournal::addMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalAddMessage);
}

//---------------------------------------------------------------------
  
UnicodeString VJournal::refreshMessage(UnicodeString sAttendeeFilter) 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalRefreshMessage, sAttendeeFilter);
}

//---------------------------------------------------------------------
 
UnicodeString VJournal::allMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalAllPropertiesMessage);
}

//---------------------------------------------------------------------
 
UnicodeString VJournal::replyMessage(UnicodeString sAttendeeFilter) 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalReplyMessage, sAttendeeFilter);
}

//---------------------------------------------------------------------
 
UnicodeString VJournal::publishMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalPublishMessage);
}

//---------------------------------------------------------------------
 
UnicodeString VJournal::publishRecurMessage() 
{
    return formatHelper(JulianFormatString::Instance()->ms_sVJournalRecurPublishMessage);
}

//---------------------------------------------------------------------
