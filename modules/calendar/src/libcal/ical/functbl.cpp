/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the 'NPL'); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an 'AS IS' basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

/* 
 * functbl.cpp
 * John Sun
 * 7/27/98 1:59:53 PM
 */

#include "stdafx.h"
#include "jdefines.h"

#include "keyword.h"
#include "functbl.h"
#include "valarm.h"

//---------------------------------------------------------------------

JulianFunctionTable * JulianFunctionTable::m_Instance = 0;

//---------------------------------------------------------------------

JulianFunctionTable::JulianFunctionTable()
{

    alarmStoreTable[0].set(JulianKeyword::Instance()->ms_ATOM_ACTION.hashCode(), &VAlarm::storeAction);
    alarmStoreTable[1].set(JulianKeyword::Instance()->ms_ATOM_ACTION.hashCode(), &VAlarm::storeAction);
    alarmStoreTable[2].set(JulianKeyword::Instance()->ms_ATOM_ATTACH.hashCode(), &VAlarm::storeAttach);
    alarmStoreTable[3].set(JulianKeyword::Instance()->ms_ATOM_ATTENDEE.hashCode(), &VAlarm::storeAttendee);
    alarmStoreTable[4].set(JulianKeyword::Instance()->ms_ATOM_DESCRIPTION.hashCode(), &VAlarm::storeDescription);
    alarmStoreTable[5].set(JulianKeyword::Instance()->ms_ATOM_DURATION.hashCode(), &VAlarm::storeDuration);
    alarmStoreTable[6].set(JulianKeyword::Instance()->ms_ATOM_REPEAT.hashCode(), &VAlarm::storeRepeat);
    alarmStoreTable[7].set(JulianKeyword::Instance()->ms_ATOM_SUMMARY.hashCode(), &VAlarm::storeSummary);
    alarmStoreTable[8].set(JulianKeyword::Instance()->ms_ATOM_TRIGGER.hashCode(), &VAlarm::storeTrigger);
    alarmStoreTable[9].set(0,0);

    tbeStoreTable[0].set(JulianKeyword::Instance()->ms_ATOM_ATTACH.hashCode(), &TimeBasedEvent::storeAttach);
    tbeStoreTable[1].set(JulianKeyword::Instance()->ms_ATOM_ATTENDEE.hashCode(), &TimeBasedEvent::storeAttendees);
    tbeStoreTable[2].set(JulianKeyword::Instance()->ms_ATOM_CATEGORIES.hashCode(), &TimeBasedEvent::storeCategories);
    tbeStoreTable[3].set(JulianKeyword::Instance()->ms_ATOM_CLASS.hashCode(), &TimeBasedEvent::storeClass);
    tbeStoreTable[4].set(JulianKeyword::Instance()->ms_ATOM_COMMENT.hashCode(), &TimeBasedEvent::storeComment);
    tbeStoreTable[5].set(JulianKeyword::Instance()->ms_ATOM_CONTACT.hashCode(), &TimeBasedEvent::storeContact);
    tbeStoreTable[6].set(JulianKeyword::Instance()->ms_ATOM_CREATED.hashCode(), &TimeBasedEvent::storeCreated);
    tbeStoreTable[7].set(JulianKeyword::Instance()->ms_ATOM_DESCRIPTION.hashCode(), &TimeBasedEvent::storeDescription);
    tbeStoreTable[8].set(JulianKeyword::Instance()->ms_ATOM_DTSTART.hashCode(), &TimeBasedEvent::storeDTStart);
    tbeStoreTable[9].set(JulianKeyword::Instance()->ms_ATOM_DTSTAMP.hashCode(), &TimeBasedEvent::storeDTStamp);
    tbeStoreTable[10].set(JulianKeyword::Instance()->ms_ATOM_EXDATE.hashCode(), &TimeBasedEvent::storeExDate);
    tbeStoreTable[11].set(JulianKeyword::Instance()->ms_ATOM_EXRULE.hashCode(), &TimeBasedEvent::storeExRule);
    tbeStoreTable[12].set(JulianKeyword::Instance()->ms_ATOM_LASTMODIFIED.hashCode(), &TimeBasedEvent::storeLastModified);
    tbeStoreTable[13].set(JulianKeyword::Instance()->ms_ATOM_ORGANIZER.hashCode(), &TimeBasedEvent::storeOrganizer);
    tbeStoreTable[14].set(JulianKeyword::Instance()->ms_ATOM_RDATE.hashCode(), &TimeBasedEvent::storeRDate);
    tbeStoreTable[15].set(JulianKeyword::Instance()->ms_ATOM_RRULE.hashCode(), &TimeBasedEvent::storeRRule);
    tbeStoreTable[16].set(JulianKeyword::Instance()->ms_ATOM_RECURRENCEID.hashCode(), &TimeBasedEvent::storeRecurrenceID);
    tbeStoreTable[17].set(JulianKeyword::Instance()->ms_ATOM_RELATEDTO.hashCode(), &TimeBasedEvent::storeRelatedTo);
    tbeStoreTable[18].set(JulianKeyword::Instance()->ms_ATOM_REQUESTSTATUS.hashCode(), &TimeBasedEvent::storeRequestStatus);
    tbeStoreTable[19].set(JulianKeyword::Instance()->ms_ATOM_SEQUENCE.hashCode(), &TimeBasedEvent::storeSequence);
    tbeStoreTable[20].set(JulianKeyword::Instance()->ms_ATOM_STATUS.hashCode(), &TimeBasedEvent::storeStatus);
    tbeStoreTable[21].set(JulianKeyword::Instance()->ms_ATOM_SUMMARY.hashCode(), &TimeBasedEvent::storeSummary);
    tbeStoreTable[22].set(JulianKeyword::Instance()->ms_ATOM_UID.hashCode(), &TimeBasedEvent::storeUID);
    tbeStoreTable[23].set(JulianKeyword::Instance()->ms_ATOM_URL.hashCode(), &TimeBasedEvent::storeURL);
    tbeStoreTable[24].set(0,0);

    tzStoreTable[0].set(JulianKeyword::Instance()->ms_ATOM_COMMENT.hashCode(), &TZPart::storeComment);
    tzStoreTable[1].set(JulianKeyword::Instance()->ms_ATOM_DTSTART.hashCode(), &TZPart::storeDTStart);
    tzStoreTable[2].set(JulianKeyword::Instance()->ms_ATOM_RDATE.hashCode(), &TZPart::storeRDate);
    tzStoreTable[3].set(JulianKeyword::Instance()->ms_ATOM_RRULE.hashCode(), &TZPart::storeRRule);
    tzStoreTable[4].set(JulianKeyword::Instance()->ms_ATOM_TZNAME.hashCode(), &TZPart::storeTZName);
    tzStoreTable[5].set(JulianKeyword::Instance()->ms_ATOM_TZOFFSETTO.hashCode(), &TZPart::storeTZOffsetTo);
    tzStoreTable[6].set(JulianKeyword::Instance()->ms_ATOM_TZOFFSETFROM.hashCode(), &TZPart::storeTZOffsetFrom);
    tzStoreTable[7].set(0,0);

    veStoreTable[0].set(JulianKeyword::Instance()->ms_ATOM_DTEND.hashCode(), &VEvent::storeDTEnd);
    veStoreTable[1].set(JulianKeyword::Instance()->ms_ATOM_DURATION.hashCode(), &VEvent::storeDuration);
    veStoreTable[2].set(JulianKeyword::Instance()->ms_ATOM_GEO.hashCode(), &VEvent::storeGEO);
    veStoreTable[3].set(JulianKeyword::Instance()->ms_ATOM_LOCATION.hashCode(), &VEvent::storeLocation);
    veStoreTable[4].set(JulianKeyword::Instance()->ms_ATOM_PRIORITY.hashCode(), &VEvent::storePriority);
    veStoreTable[5].set(JulianKeyword::Instance()->ms_ATOM_RESOURCES.hashCode(), &VEvent::storeResources);
    veStoreTable[6].set(JulianKeyword::Instance()->ms_ATOM_TRANSP.hashCode(), &VEvent::storeTransp);
    veStoreTable[7].set(0,0);

    vfStoreTable[0].set(JulianKeyword::Instance()->ms_ATOM_ATTENDEE.hashCode(), &VFreebusy::storeAttendees);
    vfStoreTable[1].set(JulianKeyword::Instance()->ms_ATOM_COMMENT.hashCode(), &VFreebusy::storeComment);
    vfStoreTable[2].set(JulianKeyword::Instance()->ms_ATOM_CONTACT.hashCode(), &VFreebusy::storeContact);
    /*vfStoreTable[3].set(JulianKeyword::Instance()->ms_ATOM_CREATED.hashCode(), &VFreebusy::storeCreated);*/
    vfStoreTable[3].set(JulianKeyword::Instance()->ms_ATOM_DURATION.hashCode(), &VFreebusy::storeDuration);
    vfStoreTable[4].set(JulianKeyword::Instance()->ms_ATOM_DTEND.hashCode(), &VFreebusy::storeDTEnd);
    vfStoreTable[5].set(JulianKeyword::Instance()->ms_ATOM_DTSTART.hashCode(), &VFreebusy::storeDTStart);
    vfStoreTable[6].set(JulianKeyword::Instance()->ms_ATOM_DTSTAMP.hashCode(), &VFreebusy::storeDTStamp);
    vfStoreTable[7].set(JulianKeyword::Instance()->ms_ATOM_FREEBUSY.hashCode(), &VFreebusy::storeFreebusy);
    vfStoreTable[8].set(JulianKeyword::Instance()->ms_ATOM_LASTMODIFIED.hashCode(), &VFreebusy::storeLastModified);
    vfStoreTable[9].set(JulianKeyword::Instance()->ms_ATOM_ORGANIZER.hashCode(), &VFreebusy::storeOrganizer);
    vfStoreTable[10].set(JulianKeyword::Instance()->ms_ATOM_REQUESTSTATUS.hashCode(), &VFreebusy::storeRequestStatus);
    vfStoreTable[11].set(JulianKeyword::Instance()->ms_ATOM_SEQUENCE.hashCode(), &VFreebusy::storeSequence);
    vfStoreTable[12].set(JulianKeyword::Instance()->ms_ATOM_UID.hashCode(), &VFreebusy::storeUID);
    vfStoreTable[13].set(JulianKeyword::Instance()->ms_ATOM_URL.hashCode(), &VFreebusy::storeURL);
    vfStoreTable[14].set(0,0);

    vtStoreTable[0].set(JulianKeyword::Instance()->ms_ATOM_COMPLETED.hashCode(), &VTodo::storeCompleted);
    vtStoreTable[1].set(JulianKeyword::Instance()->ms_ATOM_DUE.hashCode(), &VTodo::storeDue);
    vtStoreTable[2].set(JulianKeyword::Instance()->ms_ATOM_DURATION.hashCode(), &VTodo::storeDuration);
    vtStoreTable[3].set(JulianKeyword::Instance()->ms_ATOM_GEO.hashCode(), &VTodo::storeGEO);
    vtStoreTable[4].set(JulianKeyword::Instance()->ms_ATOM_LOCATION.hashCode(), &VTodo::storeLocation);
    vtStoreTable[5].set(JulianKeyword::Instance()->ms_ATOM_PERCENTCOMPLETE.hashCode(), &VTodo::storePercentComplete);
    vtStoreTable[6].set(JulianKeyword::Instance()->ms_ATOM_PRIORITY.hashCode(), &VTodo::storePriority);
    vtStoreTable[7].set(JulianKeyword::Instance()->ms_ATOM_RESOURCES.hashCode(), &VTodo::storeResources);
    vtStoreTable[8].set(0,0);
}

//---------------------------------------------------------------------

JulianFunctionTable * JulianFunctionTable::Instance()
{
    if (m_Instance == 0)
        m_Instance = new JulianFunctionTable();
    PR_ASSERT(m_Instance != 0);
    return m_Instance;
}

//---------------------------------------------------------------------



