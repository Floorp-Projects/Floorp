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

// tmbevent.cpp
// John Sun
// 3:21 PM February 6 1998

#define TIMING 1

#include "stdafx.h"
#include "jdefines.h"

#include "icalcomp.h"
#include "tmbevent.h"
#include "attendee.h"
#include "jutility.h"
#include "prprty.h"
#include "prprtyfy.h"
#include "unistrto.h"
#include "jlog.h"
#include "vtimezne.h"
#include "keyword.h"

#include "rcrrence.h"
#include "period.h"
#include "sprprty.h"
#include "recid.h"
#include "orgnzr.h"
//---------------------------------------------------------------------

//static const UnicodeString JulianKeyword::Instance()->ms_sTIMEBASEDEVENT = "TimeBasedEvent";
/*
//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sAllPropertiesMessage = 
"%v%a%k%c%K%t%i%D%B%e%F%G%H%P%C%X%E%O%M%L%J%p%x%y%R%o%T%r%q%s%g%S%h%U%u%w%Z";

//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sCancelMessage =
"%K%s%R%U%J%C%g%X%E%Z"; 

//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sRequestMessage =
"%v%a%k%c%K%t%i%B%e%F%P%C%X%E%O%M%L%J%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";

//const
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sRecurRequestMessage =
"%v%a%k%c%K%t%i%B%e%F%P%C%X%E%O%M%L%J%p%x%y%o%r%s%g%S%h%U%u%w%Z";
    
//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sCounterMessage =
"%v%a%k%c%K%t%i%B%e%F%P%C%X%E%O%M%L%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";
    
//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sTodoCounterMessage=
"%v%a%k%c%K%t%i%B%e%F%P%C%X%E%O%M%L%J%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";

//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sDeclineCounterMessage =
"%K%R%U%s%J%T%C%Z";

//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sAddMessage =
"%v%a%k%c%K%t%i%B%e%F%P%C%X%E%O%M%L%J%p%x%y%R%o%r%s%g%S%h%U%u%w%Z";

UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sTodoDeclineCounterMessage =
"%K%R%U%s%T%C%Z";
    
//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sRefreshMessage =
"%v%R%U%C%K%Z";
    
//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sReplyMessage =
"%v%P%K%R%U%s%J%T%C%X%E%Z";
    
//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sCompletedReplyMessage =
"%v%K%R%U%s%J%T%C%X%E%Z";
    
//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sPublishMessage =
"%v%a%k%c%K%t%i%B%e%F%P%C%X%E%O%M%L%J%p%x%y%R%o%r%s%g%S%U%u%w%Z";
    
//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sRecurPublishMessage =
"%v%a%k%c%K%t%i%B%e%F%P%C%X%E%O%M%L%J%p%x%y%o%r%s%g%S%U%u%w%Z";
    
//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sDelegateRequestMessage =
"%v%K%B%e%i%R%U%s%C";

//const 
UnicodeString TimeBasedEvent::JulianKeyword::Instance()->ms_sRecurDelegateRequestMessage =
"%v%K%B%e%i%U%s%C";

const UnicodeString TimeBasedEvent::ms_asCOMPONENT_TYPES[] = 
{
     JulianKeyword::Instance()->ms_sVEVENT, JulianKeyword::Instance()->ms_sVTODO, JulianKeyword::Instance()->ms_sVJOURNAL
};

const UnicodeString TimeBasedEvent::ms_aCLASS[] =
{ JulianKeyword::Instance()->ms_sPUBLIC, JulianKeyword::Instance()->ms_sPRIVATE, JulianKeyword::Instance()->ms_sCONFIDENTIAL };

const UnicodeString TimeBasedEvent::ms_aSTATUS[]   =
{   JulianKeyword::Instance()->ms_sNEEDSACTION, JulianKeyword::Instance()->ms_sCOMPLETED,
    JulianKeyword::Instance()->ms_sINPROCESS, JulianKeyword::Instance()->ms_sTENTATIVE,
    JulianKeyword::Instance()->ms_sCONFIRMED, JulianKeyword::Instance()->ms_sCANCELLED
};
*/

t_int32 TimeBasedEvent::ms_iBound	= 60;  

//---------------------------------------------------------------------

//void TimeBasedEvent::init() {}

//---------------------------------------------------------------------

// private never use
#if 0
TimeBasedEvent::TimeBasedEvent()
{
    PR_ASSERT(FALSE);
}
#endif

//---------------------------------------------------------------------

TimeBasedEvent::TimeBasedEvent(JLog * initLog)
: m_bAllDayEvent(FALSE),
  m_AlarmsVctr(0),  m_AttachVctr(0), m_AttendeesVctr(0), 
  m_CategoriesVctr(0), m_CommentVctr(0), m_Created(0), m_DTStart(0), 
  m_DTStamp(0), m_ExDateVctr(0), m_ExRuleVctr(0),
  m_LastModified(0), m_RDateVctr(0), m_RRuleVctr(0),
  m_RecurrenceID(0),
  m_RelatedToVctr(0), 
  m_Sequence(0), m_ContactVctr(0), m_XTokensVctr(0),
  m_Description(0), m_Class(0), m_URL(0), m_Summary(0), m_Status(0),
  m_RequestStatus(0), m_UID(0), m_Organizer(0),
  m_Log(initLog)
{
    //PR_ASSERT(initLog != 0);
}

//---------------------------------------------------------------------

TimeBasedEvent::TimeBasedEvent(TimeBasedEvent & that)
: m_bAllDayEvent(FALSE),
  m_AlarmsVctr(0),  m_AttachVctr(0), m_AttendeesVctr(0), 
  m_CategoriesVctr(0), m_CommentVctr(0), m_Created(0), m_DTStart(0), 
  m_DTStamp(0), m_ExDateVctr(0), m_ExRuleVctr(0),
  m_LastModified(0), m_RDateVctr(0), m_RRuleVctr(0),
  m_RecurrenceID(0),
  m_RelatedToVctr(0), 
  m_Sequence(0), m_ContactVctr(0), m_XTokensVctr(0),
  m_Description(0), m_Class(0), m_URL(0), m_Summary(0), m_Status(0),
  m_RequestStatus(0), m_UID(0), m_Organizer(0)
{
    m_sMethod = that.m_sMethod;
    m_sFileName = that.m_sFileName;
    m_origDTStart = that.m_origDTStart;
    m_origMyDTStart = that.m_origMyDTStart;
    m_bAllDayEvent = that.m_bAllDayEvent;

    // TODO: copy alarms vector

    // copy properties
    if (that.m_AttachVctr != 0)
    {
        m_AttachVctr = new JulianPtrArray(); PR_ASSERT(m_AttachVctr != 0);
        if (m_AttachVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_AttachVctr, m_AttachVctr, m_Log);
        }
    }
    if (that.m_AttendeesVctr != 0)
    {
        m_AttendeesVctr = new JulianPtrArray(); PR_ASSERT(m_AttendeesVctr != 0);
        if (m_AttendeesVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_AttendeesVctr, m_AttendeesVctr, m_Log);
        }
    }
    if (that.m_CategoriesVctr != 0)
    {
        m_CategoriesVctr = new JulianPtrArray(); PR_ASSERT(m_CategoriesVctr != 0);
        if (m_CategoriesVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_CategoriesVctr, m_CategoriesVctr, m_Log);
        }
    }
    if (that.m_Class != 0) { m_Class = that.m_Class->clone(m_Log); }
    
    if (that.m_CommentVctr != 0)
    {
        m_CommentVctr = new JulianPtrArray(); PR_ASSERT(m_CommentVctr != 0);
        if (m_CommentVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_CommentVctr, m_CommentVctr, m_Log);
        }
    }
    if (that.m_ContactVctr != 0)
    {
        m_ContactVctr = new JulianPtrArray(); PR_ASSERT(m_ContactVctr != 0);
        if (m_ContactVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_ContactVctr, m_ContactVctr, m_Log);
        }
    }
    if (that.m_Created != 0) { m_Created = that.m_Created->clone(m_Log); }
    if (that.m_Description != 0) { m_Description = that.m_Description->clone(m_Log); }
    if (that.m_DTStart != 0) { m_DTStart = that.m_DTStart->clone(m_Log); }
    if (that.m_DTStamp != 0) { m_DTStamp = that.m_DTStamp->clone(m_Log); }

    if (that.m_ExDateVctr != 0)
    {
        m_ExDateVctr = new JulianPtrArray(); PR_ASSERT(m_ExDateVctr != 0);
        if (m_ExDateVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_ExDateVctr, m_ExDateVctr, m_Log);
        }
    }
    if (that.m_ExRuleVctr != 0)
    {
        m_ExRuleVctr = new JulianPtrArray(); PR_ASSERT(m_ExRuleVctr != 0);
        if (m_ExRuleVctr != 0)
        {
            // note: unicodestring vector not ICALPropertyVector
            ICalProperty::CloneUnicodeStringVector(that.m_ExRuleVctr, m_ExRuleVctr);
        }
    }

    if (that.m_LastModified != 0) { m_LastModified = that.m_LastModified->clone(m_Log); }
    if (that.m_Organizer != 0) { m_Organizer = that.m_Organizer->clone(m_Log); }
    if (that.m_RecurrenceID != 0) { m_RecurrenceID = that.m_RecurrenceID->clone(m_Log); }

    if (that.m_RDateVctr != 0)
    {
        m_RDateVctr = new JulianPtrArray(); PR_ASSERT(m_RDateVctr != 0);
        if (m_RDateVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_RDateVctr, m_RDateVctr, m_Log);
        }
    }
    if (that.m_RRuleVctr != 0)
    {
        m_RRuleVctr = new JulianPtrArray(); PR_ASSERT(m_RRuleVctr != 0);
        if (m_RRuleVctr != 0)
        {
            // note: unicodestring vector not ICALPropertyVector
            ICalProperty::CloneUnicodeStringVector(that.m_RRuleVctr, m_RRuleVctr);
        }
    }
    if (that.m_RelatedToVctr != 0)
    {
        m_RelatedToVctr = new JulianPtrArray(); PR_ASSERT(m_RelatedToVctr != 0);
        if (m_RelatedToVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_RelatedToVctr, m_RelatedToVctr, m_Log);
        }
    }

    if (that.m_RequestStatus != 0) { m_RequestStatus = that.m_RequestStatus->clone(m_Log); }
    if (that.m_Sequence != 0) { m_Sequence = that.m_Sequence->clone(m_Log); }
    if (that.m_Status != 0) { m_Status = that.m_Status->clone(m_Log); }
    if (that.m_Summary != 0) { m_Summary = that.m_Summary->clone(m_Log); }
    if (that.m_UID != 0) { m_UID = that.m_UID->clone(m_Log); }
    if (that.m_URL != 0) { m_URL = that.m_URL->clone(m_Log); }
 
    if (that.m_XTokensVctr != 0)
    {
        m_XTokensVctr = new JulianPtrArray(); PR_ASSERT(m_XTokensVctr != 0);
        if (m_XTokensVctr != 0)
        {
            ICalProperty::CloneUnicodeStringVector(that.m_XTokensVctr, m_XTokensVctr);
        }
    }
}
//---------------------------------------------------------------------

TimeBasedEvent::~TimeBasedEvent()
{
    if (m_AlarmsVctr != 0)
    {
        // delete all alarms
        // delete vector
    }
    
    // datetime properties
    if (m_Created != 0) { delete m_Created; m_Created = 0; }
    if (m_DTStart != 0) { delete m_DTStart; m_DTStart = 0; }
    if (m_DTStamp != 0) { delete m_DTStamp; m_DTStamp = 0; }
    if (m_LastModified != 0) { delete m_LastModified; m_LastModified = 0; }

    // string properties
    if (m_Description != 0) { delete m_Description; m_Description = 0; }
    if (m_Class != 0) { delete m_Class ; m_Class = 0; }
    if (m_Summary != 0) { delete m_Summary; m_Summary = 0; }
    if (m_Status != 0) { delete m_Status; m_Status = 0; }
    if (m_RequestStatus != 0) 
    { delete m_RequestStatus; m_RequestStatus = 0; }
    
    if (m_UID != 0) { delete m_UID; m_UID = 0; }
    if (m_URL != 0) { delete m_URL; m_URL = 0; }
    if (m_Organizer != 0) { delete m_Organizer; m_Organizer = 0; }
    if (m_RecurrenceID != 0) { delete m_RecurrenceID ; m_RecurrenceID = 0; }
    //if (m_RelatedTo != 0) { delete m_RelatedTo; m_RelatedTo = 0; } // a vector again

    // integer properties
    if (m_Sequence != 0) { delete m_Sequence; m_Sequence = 0; }

    // attendees 
    if (m_AttendeesVctr != 0) { 
        Attendee::deleteAttendeeVector(m_AttendeesVctr);
        delete m_AttendeesVctr; m_AttendeesVctr = 0; 
    }

    // string vectors
    if (m_AttachVctr != 0) 
    { ICalProperty::deleteICalPropertyVector(m_AttachVctr);
        delete m_AttachVctr; m_AttachVctr = 0; }
    if (m_CategoriesVctr != 0) 
    { ICalProperty::deleteICalPropertyVector(m_CategoriesVctr);
        delete m_CategoriesVctr; m_CategoriesVctr = 0; }
    if (m_CommentVctr != 0) 
    { ICalProperty::deleteICalPropertyVector(m_CommentVctr);
        delete m_CommentVctr; m_CommentVctr = 0; }

    if (m_ExDateVctr != 0) { 
        ICalProperty::deleteICalPropertyVector(m_ExDateVctr);
        delete m_ExDateVctr; m_ExDateVctr = 0; 
    }
    if (m_ExRuleVctr != 0) { 
        ICalComponent::deleteUnicodeStringVector(m_ExRuleVctr);
        delete m_ExRuleVctr; m_ExRuleVctr = 0; 
    }
    if (m_RDateVctr != 0) { 
        ICalProperty::deleteICalPropertyVector(m_RDateVctr);
        delete m_RDateVctr; m_RDateVctr = 0; 
    }
    if (m_RRuleVctr != 0) { 
        ICalComponent::deleteUnicodeStringVector(m_RRuleVctr);
        delete m_RRuleVctr; m_RRuleVctr = 0; 
    }
    if (m_RelatedToVctr != 0) { 
        ICalProperty::deleteICalPropertyVector(m_RelatedToVctr);
        delete m_RelatedToVctr; m_RelatedToVctr = 0; 
    }
    if (m_ContactVctr != 0) 
    { ICalProperty::deleteICalPropertyVector(m_ContactVctr);
        delete m_ContactVctr; m_ContactVctr = 0; }
    if (m_XTokensVctr != 0) 
    { ICalComponent::deleteUnicodeStringVector(m_XTokensVctr);
        delete m_XTokensVctr; m_XTokensVctr = 0; 
    }
}

//---------------------------------------------------------------------


UnicodeString 
TimeBasedEvent::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
    UnicodeString s;
    switch ( c )
    {
    
    case ms_cAlarms:
        
        // TODO: not handled alarms yet  
       
        /*
          JulianPtrArray * v;
          t_int32 i;
          Vector v = getAlarms();
          StringBuffer sResult = new StringBuffer("\n\t\n");
          if (v != null) {
            t_int32 i;
            for (i = 0; i < v.size(); i++) {
              if (i < v.size() - 1)
                sResult.append("\n\tAlarm:").append( ((VAlarm)v.elementAt(i)).toString());
              else {
                sResult.append("\n\tAlarm:").append( ((VAlarm)v.elementAt(i)).toString());
                sResult.append("\n\n\t----------------------------------------");
              }
            }
          }
          return sResult.toString();
          */
        return "";
    case ms_cAttach:
        return ICalProperty::propertyVectorToString(getAttach(), dateFmt, s);
    case ms_cAttendees:
        return ICalProperty::propertyVectorToString(getAttendees(), dateFmt, s);
    case ms_cCategories:
        return ICalProperty::propertyVectorToString(getCategories(), dateFmt, s);
    case ms_cComment:
        return ICalProperty::propertyVectorToString(getComment(), dateFmt, s);
    // TODO:
        // exdate, exrules, rdates, rrules

    case ms_cClass:
        return ICalProperty::propertyToString(getClassProperty(), dateFmt, s);
    case ms_cURL:
        return ICalProperty::propertyToString(getURLProperty(), dateFmt, s);
    case ms_cCreated:
        return ICalProperty::propertyToString(getCreatedProperty(), dateFmt, s);
    case ms_cDescription:
        return ICalProperty::propertyToString(getDescriptionProperty(), dateFmt, s);
    case ms_cDTStart:
        return ICalProperty::propertyToString(getDTStartProperty(), dateFmt, s); 
    case ms_cDTStamp:
        return ICalProperty::propertyToString(getDTStampProperty(), dateFmt, s); 
    case ms_cLastModified:
        return ICalProperty::propertyToString(getLastModifiedProperty(), dateFmt, s); 
    case ms_cRelatedTo:
        return ICalProperty::propertyVectorToString(getRelatedTo(), dateFmt, s);
        //return ICalProperty::propertyToString(getRelatedToProperty(), dateFmt, s);
    case ms_cRequestStatus:
        return ICalProperty::propertyToString(getRequestStatusProperty(), dateFmt, s); 
    case ms_cSequence:
        return ICalProperty::propertyToString(getSequenceProperty(), dateFmt, s); 
    case ms_cStatus:
        return ICalProperty::propertyToString(getStatusProperty(), dateFmt, s); 
    case ms_cSummary:
        return ICalProperty::propertyToString(getSummaryProperty(), dateFmt, s); 
    case ms_cUID:
        return ICalProperty::propertyToString(getUIDProperty(), dateFmt, s); 
    case ms_cOrganizer:
        return ICalProperty::propertyToString(getOrganizerProperty(), dateFmt, s); 
    case ms_cRecurrenceID:
        return ICalProperty::propertyToString(getRecurrenceIDProperty(), dateFmt, s); 
    case ms_cContact:
        return ICalProperty::propertyVectorToString(getContact(), dateFmt, s);
        // TODO: handle x-tokens
  
    default:
        //    return (new Character(c)).toString();
       return "";
    }
}

//---------------------------------------------------------------------
 
UnicodeString 
TimeBasedEvent::formatChar(t_int32 c, UnicodeString sFilterAttendee,
                           t_bool delegateRequest) 
{
    UnicodeString sResult, s;
    t_int32 i;
    JulianPtrArray * v;
    
    //System.out.println("c = "+c);
    //System.out.println("sFilterAttendee = "+sFilterAttendee);
    //System.out.println("delegateRequest = "+delegateRequest);
    switch ( c )	    
    {
      case ms_cAlarms: {
          // TODO: finish
          /*
        v = getAlarms();
        if (v != null) {
          for (i=0; i < v.size(); i++) {
            s = ((VAlarm) v.elementAt(i)).toICALString();
            //s = ParserUtil.multiLineFormat(s); (alarm already did this)
            sResult.append(s);
          }
        }
        return sResult.toString();
        */
          return "";
      }
      case ms_cAttach: 
          s = JulianKeyword::Instance()->ms_sATTACH;
          return ICalProperty::propertyVectorToICALString(s, getAttach(), sResult);
      case ms_cAttendees:   
          {
              v = getAttendees();
              if (v != 0) 
              {
                  if (delegateRequest) 
                  {
                      for (i = 0; i < v->GetSize(); i++) 
                      {
                          Attendee * a = (Attendee *) v->GetAt(i);
                          if (a != 0)
                          {
                              if (
                                  //(a->getRole() == Attendee::ROLE_OWNER) ||
                                  //(a->getRole() == Attendee::ROLE_ORGANIZER) ||
                                  (a->getStatus() == Attendee::STATUS_DELEGATED) 
                                  // || (a->getRole() == Attendee::ROLE_DELEGATE)
                                  )                            
                              {
                                  s = a->toICALString(s);
                                  //sResult += s;
                                  sResult += ICalProperty::multiLineFormat(s);                    
                              }
                          }
                      } 
                  }
                  else if (sFilterAttendee.size() > 0) 
                  {
                      Attendee * a = getAttendee(sFilterAttendee);
                      if (a != 0)
                          s = a->toICALString(s);
                      else
                          s = "";
                      //sResult += s;   
                      sResult += ICalProperty::multiLineFormat(s);
                  }
                  else 
                  {
                      for (i = 0; i < v->GetSize(); i++) 
                      {
                          s = ((Attendee * ) v->GetAt(i))->toICALString(s);
                          //sResult += s;
                          sResult += ICalProperty::multiLineFormat(s);
                      }
                  }
              }
              return sResult;
             //        return sResult.toString();
          }
      case ms_cCategories:
          s = JulianKeyword::Instance()->ms_sCATEGORIES;
          return ICalProperty::propertyVectorToICALString(s, getCategories(), sResult);     
      case ms_cClass:
          s = JulianKeyword::Instance()->ms_sCLASS;
          return ICalProperty::propertyToICALString(s, getClassProperty(), sResult);
      case ms_cComment: 
          s = JulianKeyword::Instance()->ms_sCOMMENT;
          return ICalProperty::propertyVectorToICALString(s, getComment(), sResult);
      case ms_cURL: 
          s = JulianKeyword::Instance()->ms_sURL;
          return ICalProperty::propertyToICALString(s, getURLProperty(), sResult);
      case ms_cCreated: 
          s = JulianKeyword::Instance()->ms_sCREATED;
          return ICalProperty::propertyToICALString(s, getCreatedProperty(), sResult);
      case ms_cDescription:  
          s = JulianKeyword::Instance()->ms_sDESCRIPTION;
          return ICalProperty::propertyToICALString(s, getDescriptionProperty(), sResult);  
      case ms_cDTStart: 
          s = JulianKeyword::Instance()->ms_sDTSTART;
          return ICalProperty::propertyToICALString(s, getDTStartProperty(), sResult);  
      case ms_cDTStamp:
          s = JulianKeyword::Instance()->ms_sDTSTAMP;
          return ICalProperty::propertyToICALString(s, getDTStampProperty(), sResult);  
      case ms_cExDate:
          s = JulianKeyword::Instance()->ms_sEXDATE;
          return ICalProperty::propertyVectorToICALString(s, getExDates(), sResult);
      case ms_cExRule:     
          if (getExRules() != 0)
          {
              //s = JulianKeyword::Instance()->ms_sEXRULE;
              //return ICalComponent::propertyVToCalString(s, getExRules(), sResult);
              return ICalProperty::vectorToICALString(getExRules(), sResult);        
          }
          return "";
        //case ms_cGEO: 
        //return ICalComponent::propertyToCalString(JulianKeyword::Instance()->ms_sGEO, getGEO());
      case ms_cLastModified: 
          s = JulianKeyword::Instance()->ms_sLASTMODIFIED;
          return ICalProperty::propertyToICALString(s, getLastModifiedProperty(), sResult);  
      case ms_cRDate: 
          s = JulianKeyword::Instance()->ms_sRDATE;
          return ICalProperty::propertyVectorToICALString(s, getRDates(), sResult);
      case ms_cRRule: 
          if (getRRules() != 0)
          {
              //s = JulianKeyword::Instance()->ms_sRRULE;
              //return ICalComponent::propertyVToCalString(s, getRRules(), sResult);
              return ICalProperty::vectorToICALString(getRRules(), sResult);        
          }    
          return "";
      case ms_cRecurrenceID: {
          s = JulianKeyword::Instance()->ms_sRECURRENCEID;
          return ICalProperty::propertyToICALString(s, getRecurrenceIDProperty(), sResult);
        /*
	if (getRecurrenceID() != null) {
	  sResult.append(getRecurrenceID().toICALString());
        }
        return ParserUtil.multiLineFormat(sResult.toString());
        */
      }
      case ms_cRelatedTo:
          s = JulianKeyword::Instance()->ms_sRELATEDTO;
          return ICalProperty::propertyVectorToICALString(s, getRelatedTo(), sResult);
          //return ICalProperty::propertyToICALString(s, getRelatedToProperty(), sResult);
      case ms_cRequestStatus:
          s = JulianKeyword::Instance()->ms_sREQUESTSTATUS;
          return ICalProperty::propertyToICALString(s, getRequestStatusProperty(), sResult);
      case ms_cSequence:
          s = JulianKeyword::Instance()->ms_sSEQUENCE;
          return ICalProperty::propertyToICALString(s, getSequenceProperty(), sResult);
      case ms_cStatus:
          s = JulianKeyword::Instance()->ms_sSTATUS;
          return ICalProperty::propertyToICALString(s, getStatusProperty(), sResult);
      case ms_cSummary:
          s = JulianKeyword::Instance()->ms_sSUMMARY;
          return ICalProperty::propertyToICALString(s, getSummaryProperty(), sResult);
      case ms_cUID:
          s = JulianKeyword::Instance()->ms_sUID;
          return ICalProperty::propertyToICALString(s, getUIDProperty(), sResult);
      case ms_cOrganizer:
          s = JulianKeyword::Instance()->ms_sORGANIZER;
          return ICalProperty::propertyToICALString(s, getOrganizerProperty(), sResult);
      case ms_cContact: 
          s = JulianKeyword::Instance()->ms_sCONTACT;
          return ICalProperty::propertyVectorToICALString(s, getContact(), sResult);
      case ms_cXTokens: 
          return ICalProperty::vectorToICALString(getXTokens(), sResult);
      default:
          {
          //DebugMsg.Instance().println(0,"No such member in TimeBasedEvent");
          //sResult = ParserUtil.multiLineFormat(sResult);
          return "";
        }
    }  
}

//---------------------------------------------------------------------
// TODO: make crash proof
UnicodeString &
TimeBasedEvent::parseType(UnicodeString & sType, ICalReader * brFile,
                          UnicodeString & sMethod, 
                          UnicodeString & parseStatus, 
                          JulianPtrArray * vTimeZones, t_bool bIgnoreBeginError,
                          JulianUtility::MimeEncoding encoding)
{
    //VAlarm * theAlarm;
    t_bool bNewEvent = FALSE;
    t_bool bNextAlarm = FALSE;

    UnicodeString sOK;
    t_bool parseError = FALSE;
    UnicodeString strLine, propName, propVal;

    JulianPtrArray * parameters = new JulianPtrArray();

    parseStatus = JulianKeyword::Instance()->ms_sOK;
    PR_ASSERT(parameters != 0 && brFile != 0);
    if (parameters == 0 || brFile == 0)
    {
        // Return an invalid event
        return parseStatus;
    }
    //t_int32 strLineHashCode;
    UnicodeString end = JulianKeyword::Instance()->ms_sEND_WITH_COLON; end += sType;
    UnicodeString begin = JulianKeyword::Instance()->ms_sBEGIN_WITH_COLON; begin += sType;
    
    ErrorCode status = ZERO_ERROR;

    // set method
    setMethod(sMethod);

    while (TRUE)
    {
        PR_ASSERT(brFile != 0);
        brFile->readFullLine(strLine, status);
        //strLine.trim();
        ICalProperty::Trim(strLine);

        //if (FALSE) TRACE("TMB: line (size = %d) = ---%s---\r\n", 
        //  strLine.size(), strLine.toCString(""));   

        if (FAILURE(status) && strLine.size() < 0)
            break;
    
        ////////////////////////////////////////////////////////
        // I've made it so parameters are now
        // copied.  Only need to worry about deleting parameter
        // contents here now.  The ICalProperty, Attendee, Freebusy
        // DO NOT take ownership of parameters pointer
        /////////////////////////////////////////////////////////
//#ifdef TIMING
                //clock_t cstart, cfinish;
                //cstart = clock();
//#endif
        ICalProperty::parsePropertyLine(strLine, propName, propVal, parameters);
        
//#ifdef TIMING
                //cfinish = clock();
                //double d = cfinish - cstart;
                //if (FALSE) TRACE("parsePropertyLine took %f ms.\r\n", strLine.toCString(""), d);
//#endif
        //if (FALSE) TRACE("TMB: propName (size = %d) = --%s--, propVal (size = %d) = --%s--, paramSize = %d\r\n", 
        //    propName.size(), propName.toCString(""), 
        //    propVal.size(), propVal.toCString(""), parameters->GetSize());
        
        //    TRACE_ICalParameterVector(parameters, FALSE);
        //    ICalProperty::propertyToICALString(outName, outVal, 
        //        parameters, printOut);

        
        if (strLine.size() == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            
            continue;
        }
        // break on END:
        if (strLine.compareIgnoreCase(end) == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            
            break;
        }
        // TODO: maybe I should break out for all "BEGIN:" except VALARM
        if ((strLine.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN_VCALENDAR) == 0) ||
            (strLine.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND_VCALENDAR) == 0) ||
            ((strLine.compareIgnoreCase(begin) == 0) && !bIgnoreBeginError))
        {

            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            
            parseStatus = strLine;
            
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sAbruptEndOfParsing, 
                sType, strLine, 300);
            
            bNewEvent = TRUE;
            break;
        }
        else 
        {
            if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) &&
                (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVALARM) == 0))
            {

                ICalProperty::deleteICalParameterVector(parameters);
                parameters->RemoveAll();

                // Temporary hack to ignore alarm properties, remove this when
                // VAlarm is ready

                while (TRUE)
                {

                    brFile->readFullLine(strLine, status);
                    if (FAILURE(status) && strLine.size() < 0)
                        break;
                    ICalProperty::parsePropertyLine(strLine, propName, propVal, parameters);
                     
                    //if (FALSE) TRACE("TMB: propName (size = %d) = --%s--, propVal (size = %d) = --%s--, paramSize = %d\r\n", 
                    //    propName.size(), propName.toCString(""), 
                    //    propVal.size(), propVal.toCString(""), parameters->GetSize());

                    ICalProperty::deleteICalParameterVector(parameters);
                    parameters->RemoveAll();
                    if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
                        (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVALARM) == 0))
                    {
                        // correct end of VAlarm parsing
                        break;
                    }
                    else if (
                        ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) && (propVal.compareIgnoreCase(sType) == 0)) ||
                        ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) && (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0)) ||
                        ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) && (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0)) ||
                        ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) && (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0)) ||
                        ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) && (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0)) ||
                        ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) && (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0)) ||
                        ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) && (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0)) ||
                        ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) && (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVALARM) == 0))
                        )
                    {
                        // incorrect end of VAlarm parsing, log an error
                        if (m_Log) m_Log->logString(
                            JulianLogErrorMessage::Instance()->ms_sAbruptEndOfParsing, 
                            JulianKeyword::Instance()->ms_sVALARM, strLine, 300);
                        break;
                    }

                }
                // end of temporary hack

#if 0
                // TODO: finish alarms parsing when VAlarm class is ready
                if (m_AlarmsVctr == 0)
                {
                    m_AlarmsVctr = new JulianPtrArray();
                    PR_ASSERT(m_AlarmsVctr != 0);
                }
                bNextAlarm = TRUE;
                while (bNextAlarm)
                {
                    /*
                    //theAlarm = new VAlarm(this);
                    // parse alarm, assuming same args order
                    //sOK = theAlarm->parse(brFile, "", sOK, vTimeZones);
                    if (!theAlarm.isValid())
                    {
                        // TODO: log and error
                        //setRequestStatus("Error making alarm");
                    }
                    else
                    {
                        //addAlarm(theAlarm);
                    }
                    if (sOK.compareIgnoreCase(JulianKeyword::Instance()->ms_sOK) == 0)
                        break; //bNextAlarm = FALSE;
                    else 
                    {
                        if (sOK.compareIgnoreCase("BEGIN:VALARM"))
                        {
                            // stay in loop
                        }
                        else
                        {
                            break;
                        }
                    }
                    */
                }
                if (sOK.compareIgnoreCase(JulianKeyword::Instance()->ms_sOK) != 0)
                {
                    parseStatus = sOK;
                    break;
                }
#endif
            }
            else
            {
//#ifdef TIMING
               //clock_t start, end;
               //start = clock();
//#endif
                storeData(strLine, propName, propVal, parameters, vTimeZones);
                ICalProperty::deleteICalParameterVector(parameters);
                parameters->RemoveAll();
//#ifdef TIMING
               //end = clock();
               //double d = end - start;
               //if (FALSE) TRACE("storeData on %s took %f ms.\r\n", propName.toCString(""), d);
//#endif
            }
        }
    }
    ICalProperty::deleteICalParameterVector(parameters);
    parameters->RemoveAll();
    delete parameters; parameters = 0;

    selfCheck();
    //checkRecurrence();
    return parseStatus;
}

//---------------------------------------------------------------------

t_bool 
TimeBasedEvent::storeData(UnicodeString & strLine, UnicodeString & propName, 
                          UnicodeString & propVal, JulianPtrArray * parameters, 
                          JulianPtrArray * vTimeZones)
{
    t_bool bParamValid;

    // just to get rid of compile warning
    if (vTimeZones != 0)
    {
    }

    // TODO: handle categories, resources,
    // check parameters, error-checking

    t_int32 hashCode = propName.hashCode();

    //    if (propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sATTENDEE) == 0)
    if (JulianKeyword::Instance()->ms_ATOM_ATTENDEE == hashCode)
    {
        Attendee * attendee = new Attendee(GetType(), m_Log);
        PR_ASSERT(attendee != 0);
        if (attendee != 0)
        {
            attendee->parse(propVal, parameters);
            if (!attendee->isValid())
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidAttendee, 200);

                UnicodeString u;
                u = JulianLogErrorMessage::Instance()->ms_sRS202;
                u += '.'; u += ' ';
                u += strLine;
                //setRequestStatus(JulianLogErrorMessage::Instance()->ms_sRS202); 
                setRequestStatus(u);
                delete attendee; attendee = 0;
            }
            else
            {
                addAttendee(attendee);
            }
        }
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_COMMENT == hashCode)
    { 
        // check parameters
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asAltrepLanguageParamRange,
            JulianAtomRange::Instance()->ms_asAltrepLanguageParamRangeSize);
        
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        addComment(propVal, parameters);
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_DESCRIPTION == hashCode)
    {
        // check parameters
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asAltrepLanguageParamRange,
            JulianAtomRange::Instance()->ms_asAltrepLanguageParamRangeSize);
        
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        // check for duplicates
        if (getDescriptionProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }

        setDescription(propVal, parameters);     
        return TRUE;
    }  
    else if (JulianKeyword::Instance()->ms_ATOM_RRULE == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }
        // TODO: finish, pass timezones.
        addRRuleString(strLine);
        return TRUE;
    }   
    else if (JulianKeyword::Instance()->ms_ATOM_RDATE == hashCode)
    {
        // check parameters (date, datetime, period)
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asTZIDValueParamRange,
            JulianAtomRange::Instance()->ms_asTZIDValueParamRangeSize,
            JulianAtomRange::Instance()->ms_asDateDateTimePeriodValueRange,
            JulianAtomRange::Instance()->ms_asDateDateTimePeriodValueRangeSize);
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }
        // TODO: finish
        addRDate(propVal, parameters);
        return TRUE;
    }   
    else if (JulianKeyword::Instance()->ms_ATOM_EXRULE == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }
        // TODO: finish, pass timezones.
        addExRuleString(strLine);
        return TRUE;
    }   
    else if (JulianKeyword::Instance()->ms_ATOM_EXDATE == hashCode)
    {
        // check parameters (date, datetime)
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asTZIDValueParamRange,
            JulianAtomRange::Instance()->ms_asTZIDValueParamRangeSize,
            JulianAtomRange::Instance()->ms_asDateDateTimeValueRange,
            JulianAtomRange::Instance()->ms_asDateDateTimeValueRangeSize);
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }
        // DONE:?TODO: finish
        addExDate(propVal, parameters);
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_RECURRENCEID == hashCode)
    {
        // TODO: CHECK PARAMETERS
        if (getRecurrenceIDProperty() != 0)
        {
            if (m_Log) m_Log->logString(
            JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }
        // TODO: finish, check timezones
        DateTime d(propVal);
        
        // NOTE: only set recurrenceID if datetime is of a valid datetime format
        if (d.isValid())
        {
            setRecurrenceID(d, parameters);
        }
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_DTSTART == hashCode)
    {
        // check parameters (date, datetime), TZID
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asTZIDValueParamRange,
            JulianAtomRange::Instance()->ms_asTZIDValueParamRangeSize,
            JulianAtomRange::Instance()->ms_asDateDateTimeValueRange,
            JulianAtomRange::Instance()->ms_asDateDateTimeValueRangeSize);
        
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        if (getDTStartProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
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
                    JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
            }
            setAllDayEvent(TRUE);
        }
        else
        {
            // if there is a VALUE=X parameter, make sure X is DATETIME
            if (out.size() != 0 && (JulianKeyword::Instance()->ms_ATOM_DATETIME != out.hashCode()))
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sPropertyValueTypeMismatch,
                    JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
            }
            setAllDayEvent(FALSE);
        }

        DateTime d;
        //UnicodeString u, out;
        d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

        setDTStart(d, parameters);
        
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_ATTACH == hashCode)
    {
        // check parameters (binary, uri), todo: checkEncoding range
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asEncodingValueParamRange,
            JulianAtomRange::Instance()->ms_asEncodingValueParamRangeSize, 
            JulianAtomRange::Instance()->ms_asBinaryURIValueRange,
            JulianAtomRange::Instance()->ms_asBinaryURIValueRangeSize);        
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }
        addAttach(propVal, parameters);
        return TRUE;
    }   
    else if (JulianKeyword::Instance()->ms_ATOM_CATEGORIES == hashCode)
    {
        // check parameters 
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asLanguageParamRange,
            JulianAtomRange::Instance()->ms_asLanguageParamRangeSize);
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        addCategoriesPropertyVector(propVal, parameters);
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_URL == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        if (getURLProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }

        setURL(propVal, parameters);   
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_RELATEDTO == hashCode)
    {
        // check parameters: todo: check reltype
        bParamValid = ICalProperty::CheckParams(parameters, 
                JulianAtomRange::Instance()->ms_asReltypeParamRange,
                JulianAtomRange::Instance()->ms_asReltypeParamRangeSize);
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        addRelatedTo(propVal, parameters);
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_CONTACT == hashCode)
    {
        // check parameters
        bParamValid = ICalProperty::CheckParams(parameters, 
                JulianAtomRange::Instance()->ms_asAltrepLanguageParamRange,
                JulianAtomRange::Instance()->ms_asAltrepLanguageParamRangeSize);        
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        addContact(propVal, parameters);
        return TRUE;
    }   
    else if (JulianKeyword::Instance()->ms_ATOM_CREATED == hashCode)
    {
        // no parameters (MUST BE IN UTC)
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        if (getCreatedProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }
        DateTime d;
        d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

        setCreated(d, parameters);   
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_LASTMODIFIED == hashCode)
    {
        // no parameters (MUST BE IN UTC)
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        if (getLastModifiedProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }
        DateTime d;
        d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

        setLastModified(d, parameters);   
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_DTSTAMP == hashCode)
    {
        // no parameters (MUST BE IN UTC)
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        if (getDTStampProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }
        DateTime d;
        d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

        setDTStamp(d, parameters);   
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_SUMMARY == hashCode)
    {
        // check parameters 
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asLanguageParamRange,
            JulianAtomRange::Instance()->ms_asLanguageParamRangeSize);
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        if (getSummaryProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }
        setSummary(propVal, parameters);
        return TRUE;
    }   
    else if (JulianKeyword::Instance()->ms_ATOM_CLASS == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        if (getClassProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }
        setClass(propVal, parameters);
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_STATUS == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        if (getStatusProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }
        setStatus(propVal, parameters);
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_REQUESTSTATUS == hashCode)
    {
         // check parameters 
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asLanguageParamRange,
            JulianAtomRange::Instance()->ms_asLanguageParamRangeSize);
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        /*
        if (getRequestStatusProperty() != 0)
        {
            if (m_Log) m_Log->logString(
            JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }
        */
        setRequestStatus(propVal, parameters);
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_UID == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        if (getUIDProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }
        setUID(propVal, parameters);
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_ORGANIZER == hashCode)
    {
        // check parameters 
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asSentByParamRange,
            JulianAtomRange::Instance()->ms_asSentByParamRangeSize);
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        if (getOrganizerProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }
        setOrganizer(propVal, parameters);
        return TRUE;
    }   
    else if (JulianKeyword::Instance()->ms_ATOM_SEQUENCE == hashCode)
    {
        t_bool bParseError = FALSE;
        t_int32 i;

        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, strLine, 100);
        }

        i = JulianUtility::atot_int32(propVal.toCString(""), bParseError, propVal.size());
        if (getSequenceProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName, 100);
        }  
        if (!bParseError)
        {
            setSequence(i, parameters);
        }
        else
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidNumberFormat, 
                JulianKeyword::Instance()->ms_sTIMEBASEDEVENT, propName,
                propVal, 200);
        }
        return TRUE;
    } 
    else if (ICalProperty::IsXToken(propName))
    {
        addXTokens(strLine);  
        return TRUE;
    }
    else 
    {
        return FALSE;
    }
}

//---------------------------------------------------------------------

t_bool TimeBasedEvent::isValid()
{
    return TRUE;
}

//---------------------------------------------------------------------

/**
* prints entire event to ICAL export format
*/
UnicodeString TimeBasedEvent::toICALString() {
    return allMessage();
}

//---------------------------------------------------------------------

UnicodeString TimeBasedEvent::toICALString(UnicodeString sMethod, UnicodeString sName,
                                           t_bool isRecurring)
{
    UnicodeString s;
    
    t_int32 hashCode = sMethod.hashCode();

    if (JulianKeyword::Instance()->ms_ATOM_PUBLISH == hashCode) 
        s = publishMessage();
    else if (JulianKeyword::Instance()->ms_ATOM_REQUEST == hashCode)
        s = requestMessage();
    else if (JulianKeyword::Instance()->ms_ATOM_CANCEL == hashCode)
        s = cancelMessage();
    else if (JulianKeyword::Instance()->ms_ATOM_REPLY == hashCode)
        s = replyMessage(sName);
    else if (JulianKeyword::Instance()->ms_ATOM_REFRESH == hashCode)
        s = refreshMessage(sName);
    else if (JulianKeyword::Instance()->ms_ATOM_COUNTER == hashCode)
        s = counterMessage();
    else if (JulianKeyword::Instance()->ms_ATOM_DECLINECOUNTER == hashCode)
        s = declineCounterMessage();
    else if (JulianKeyword::Instance()->ms_ATOM_ADD == hashCode)
        s = addMessage();
    /*
    else if (JulianKeyword::Instance()->ms_ATOM_DELEGATEREQUEST == hashCode)
        s = delegateRequestMessage(sName, sDelegatedTo, isRecurring);
    else if (JulianKeyword::Instance()->ms_ATOM_DELEGATEREPLY == hashCode)
        s = delegateReplyMessage(sName, sDelegatedTo, isRecurring);
    */
    return s;
}

//---------------------------------------------------------------------
/*
UnicodeString
TimeBasedEvent::delegateReplyMessage(UnicodeString sAttendeeFilter, 
                                     UnicodeString sDelegateTo, t_bool bRecur)
{
    // NOTE: remove later, avoid warning
    if (bRecur) {}
    addDelegate(sAttendeeFilter, sDelegateTo);
    return replyMessage(sAttendeeFilter);
}
*/
//---------------------------------------------------------------------
/*
UnicodeString 
TimeBasedEvent::delegateRequestMessage(UnicodeString sAttendeeFilter,
                                       UnicodeString sDelegateTo, t_bool bRecur)
{
    // NOTE: remove later, avoid warning
    if (bRecur) {}
    addDelegate(sAttendeeFilter, sDelegateTo);
    return requestMessage();
}
*/
//---------------------------------------------------------------------
/*
void
TimeBasedEvent::addDelegate(UnicodeString & sAttendeeFilter, 
                            UnicodeString & sDelegateTo)
{
    Attendee * me;
    me = getAttendee(sAttendeeFilter);
    if (me != 0)
    {
        // set my attendee status to delegated, add delegateTo variable
        me->setStatus(Attendee::STATUS_DELEGATED);
        me->addDelegatedTo(sDelegateTo);
        
        // add new attendee, setting name, role = req_part, delfrom = me,
        // rsvp = TRUE, expect = request, status = needs-action
        Attendee * delegate = Attendee::getDefault(m_Log);
        PR_ASSERT(delegate != 0);
        if (delegate != 0)
        {
            delegate->setName(sDelegateTo);
            delegate->setRole(Attendee::ROLE_REQ_PARTICIPANT);
            delegate->addDelegatedFrom(sAttendeeFilter);
            delegate->setRSVP(Attendee::RSVP_TRUE);
            delegate->setExpect(Attendee::EXPECT_REQUEST);
            delegate->setStatus(Attendee::STATUS_NEEDSACTION);
            addAttendee(delegate);
        }
    }
}
*/
//---------------------------------------------------------------------

void
TimeBasedEvent::setAttendeeStatus(UnicodeString & sAttendeeFilter,
                                  Attendee::STATUS status,
                                  JulianPtrArray * delegatedTo)
{
    Attendee * a;
    a = getAttendee(sAttendeeFilter);
    if (a == 0)
    {
        // add a new attendee to this event (a partycrasher)
        a = Attendee::getDefault(GetType());
        PR_ASSERT(a != 0);
        if (a != 0)
        {
            a->setName(sAttendeeFilter);
            addAttendee(a);
        }
    }
    PR_ASSERT(a != 0);
    if (a != 0)
    {
        a->setStatus(status);
        if (status == Attendee::STATUS_DELEGATED)
        {
            if (delegatedTo != 0)
            {
                t_int32 i;
                UnicodeString u;
                // for each new attendee, set name, role = req_part, delfrom = me,
                // rsvp = TRUE, expect = request, status = needs-action.
                for (i = 0; i < delegatedTo->GetSize(); i++)
                {
                    u = *((UnicodeString *) delegatedTo->GetAt(i));
                    a->addDelegatedTo(u);

                    Attendee * delegate = Attendee::getDefault(GetType(), m_Log);
                    PR_ASSERT(delegate != 0);
                    if (delegate != 0)
                    {
                        delegate->setName(u);
                        delegate->setRole(Attendee::ROLE_REQ_PARTICIPANT);
                        delegate->addDelegatedFrom(sAttendeeFilter);
                        delegate->setRSVP(Attendee::RSVP_TRUE);
                        delegate->setExpect(Attendee::EXPECT_REQUEST);
                        delegate->setStatus(Attendee::STATUS_NEEDSACTION);
                        addAttendee(delegate);
                    }               
                }
            }
        }
    }
}

//---------------------------------------------------------------------

int
TimeBasedEvent::CompareTimeBasedEventsByUID(const void * a,
                                            const void * b)
{
    PR_ASSERT(a != 0 && b != 0);
    TimeBasedEvent * ta = *(TimeBasedEvent **) a;
    TimeBasedEvent * tb = *(TimeBasedEvent **) b;
    
    return (int) ta->getUID().compare(tb->getUID());
}

//---------------------------------------------------------------------

int
TimeBasedEvent::CompareTimeBasedEventsByDTStart(const void * a,
                                                const void * b)
{
    PR_ASSERT(a != 0 && b != 0);
    TimeBasedEvent * ta = *(TimeBasedEvent **) a;
    TimeBasedEvent * tb = *(TimeBasedEvent **) b;

    DateTime da, db;
    da = ta->getDTStart();
    db = tb->getDTStart();

    return (int) (da.compareTo(db));
}

//---------------------------------------------------------------------

// TODO: make delegate request work
/*
UnicodeString TimeBasedEvent::delegateRequestMessage(UnicodeString TimeBasedEvent::sAttendeeFilter) {
    //return delegateRequestMessage(sAttendeeFilter, FALSE);
    return ICalComponent::delegateRequestMessage(sAttendeeFilter, FALSE);
}

//---------------------------------------------------------------------

UnicodeString TimeBasedEvent::delegateRecurRequestMessage(UnicodeString TimeBasedEvent::sAttendeeFilter) {
    //return delegateRequestMessage(sAttendeeFilter, TRUE);
    return ICalComponent::delegateRequestMessage(sAttendeeFilter, TRUE);
}
*/
/**
* Adds delegate to delegatee attendee list
* Prints properties of an DELEGATE-REQUEST message
* @param sAttendeeFilter name of delegate
* @return string with delegateRequest message required properties
*/
/*
UnicodeString TimeBasedEvent::delegateRequestMessage(UnicodeString sAttendeeFilter, 
                                     t_bool bRecur) 
{
    // only required fields
    // attendee-R if exists
    // comment-R, dtstart?, dtend?, exdate?, exrule?, request-status-R, response-seq-R~0, seq-R~0, summary?
    // uid-R, rec-R if exists
    
    Attendee * me = getAttendee(sAttendeeFilter);
    JulianPtrArray * delegateTo;

    ///---PROBABLY SHOULD BE ASSERTIONS-----------------
    if (me == 0) {
      //LogStream.Instance().println(2, Utility.ErrorMsg("DelegateRequestError"));
      //DebugMsg.Instance().println(0,"No such attendee");
      return "";
    }
    if (me.getStatus().compareIgnoreCase(JulianKeyword::Instance()->ms_sDELEGATED) != 0) {      
      //LogStream.Instance().println(2, Utility.ErrorMsg("DelegateRequestError"));
      //DebugMsg.Instance().println(0,"I did not delegate yet");
      return "";
    }
    delegateTo = me->getDelegatedTo();
    if (delegateTo == 0 || delegateTo->GetSize() == 0) {
      
      //LogStream.Instance().println(2, Utility.ErrorMsg("DelegateRequestError"));
      //DebugMsg.Instance().println(0,"No delegated-to");
      return "";
    }
    ///-------------------------------------------------
    UnicodeString delegateName; 
    Attendee * delegate = Attendee.getDefault();
    
    // set new attendee's name to last person in delegateTo vector, assuming that last person you delegated to in reply
    //  will be the next person you call delegate request, thus the request message should immediately be called after the delegation
    //  takes place
    delegateName = *((UnicodeString)delegateTo.lastElement();)

    // according to spec, rsvp, expect should be set to exactly what delegate had
    String sRSVP = PropertyKeywords.JulianKeyword::Instance()->ms_sTRUE;
    String sExpect = PropertyKeywords.JulianKeyword::Instance()->ms_sREQUEST;
    String sRole = PropertyKeywords.JulianKeyword::Instance()->ms_sDELEGATE;

    delegate.setName(delegateName);
    delegate.setRole(PropertyKeywords.JulianKeyword::Instance()->ms_sDELEGATE);
    delegate.setRSVP(PropertyKeywords.JulianKeyword::Instance()->ms_sTRUE);
    delegate.setDelegatedFrom(sAttendeeFilter);
    //delegate.load("ATTENDEE;ROLE=DELEGATE;RSVP=" + sRSVP +";EXPECT="+ sExpect +";DELEGATED-FROM=" + sAttendeeFilter + ":" + delegateName);

    System.out.println("delegate.toICALString == "+ delegate.toICALString());
    Attendee * a2 = getAttendee(delegate.getName());
    if (a2 == 0)
      addAttendee(delegate);
    else {
      DebugMsg.Instance().println(0,"Delegating to a person who is already on attendee list");
    }
    //String attendeeparams = "%S%D%N";
    // UPDATE ATTENDEE PROPERTIES HERE (attendee status is accepted, declined, delegated)
    //   what about response-sequence ? (will we keep it)
    if (bRecur) 
      return format(JulianKeyword::Instance()->ms_sRecurDelegateRequestMessage, TRUE);  // this is a delegate request
    else
      return format(JulianKeyword::Instance()->ms_sDelegateRequestMessage, TRUE);  // this is a delegate request
}
*/

//---------------------------------------------------------------------
  
t_bool TimeBasedEvent::MatchUID_seqNO(UnicodeString sUID, t_int32 iSeqNo)
{
    t_int32 seq = getSequence();
    UnicodeString uid = getUID();

    if (seq == iSeqNo && uid.compareIgnoreCase(sUID) == 0)
      return TRUE;
    else
      return FALSE;
}

//---------------------------------------------------------------------

Attendee * TimeBasedEvent::getAttendee(UnicodeString sAttendee){
    return Attendee::getAttendee(getAttendees(), sAttendee);
}

//---------------------------------------------------------------------

/**
* return vector of all attendees except organizer
* @param vAttendees     vector of attendees
* @return vector of attendees except organizer
*/
/*
JulianPtrArray * getAttendeesExceptOrganizer() {
    JulianPtrArray * attendees = new JulianPtrArray();
    return Attendee.getAttendeesExceptOrganizer(getAttendees(), attendees);
}
*/  

//---------------------------------------------------------------------

void TimeBasedEvent::stamp()
{
    DateTime d;
    setDTStamp(d);
}

//---------------------------------------------------------------------

t_bool
TimeBasedEvent::isExpandableEvent() const
{
    DateTime d;
    d = getRecurrenceID();
    if (d.isValid())
    {
        return FALSE;
    }
    else if ((getRRules() == 0 || getRRules()->GetSize() == 0) && 
            (getRDates() == 0 || getRDates()->GetSize() == 0))
    {
        return FALSE;
    }
    else 
    {
        return TRUE;
    }
}

//---------------------------------------------------------------------

void
TimeBasedEvent::createRecurrenceEvents(JulianPtrArray * vOut, 
                                       JulianPtrArray * vTimeZones)
{
    DateTime start; 
    start = getDTStart();

    JulianPtrArray * vRRules = getRRules();
    JulianPtrArray * vExRules = getExRules();
    JulianPtrArray * vRDates = getRDates();
    JulianPtrArray * vExDates = getExDates();
    t_int32 iBound = getBound();

    if (vRRules == 0 && vExRules == 0 && vRDates == 0 && vExDates == 0)
        return;
    
    JulianPtrArray * dates = new JulianPtrArray(); PR_ASSERT(dates != 0);
      
    if (dates != 0)
    {
        // generate dates of instances
        generateDates(dates, start, vRRules, vExRules, vRDates, vExDates,
            iBound, vTimeZones, m_Log);

        // create events and set start,(end|due) to dates
        populateDates(vOut, dates, start, vTimeZones);

        // CLEANUP
        Recurrence::deleteDateTimeVector(dates);
        delete dates; dates = 0;
    }
}

//---------------------------------------------------------------------

void 
TimeBasedEvent::generateDates(JulianPtrArray * vOut, DateTime start, 
                              JulianPtrArray * vRRules, 
                              JulianPtrArray * vExRules,
                              JulianPtrArray * vRDatesProp, 
                              JulianPtrArray * vExDatesProp, 
                              t_int32 iBound,
                              JulianPtrArray * vTimeZones, JLog * log)
{
    // TODO: SET UNTIL in RRULE, EXRULE to ISO8601 UTC
    
    JulianPtrArray * vRDates = new JulianPtrArray();
    PR_ASSERT(vRDates != 0);
    JulianPtrArray * vExDates = new JulianPtrArray();
    PR_ASSERT(vExDates != 0);

    if (vRDates != 0 && vExDates != 0)
    {
        // split RDATES, EXDATES properties into vector of Date, DateTime, or Period strings.
        splitDates(vRDates, vRDatesProp, TRUE, vTimeZones);
        splitDates(vExDates, vExDatesProp, FALSE, vTimeZones);

        Recurrence::stringEnforce(start, vRRules, vExRules, vRDates, 
            vExDates, iBound, vOut, log);
    
        ICalComponent::deleteUnicodeStringVector(vRDates);
        ICalComponent::deleteUnicodeStringVector(vExDates);
        delete vRDates; vRDates = 0;
        delete vExDates; vExDates = 0;
    }
}

//---------------------------------------------------------------------

// TODO: Make crash proof
void
TimeBasedEvent::splitDates(JulianPtrArray * out,
                           JulianPtrArray * vDateProp, t_bool isRDate,
                           JulianPtrArray * vTimeZones)
{

    if (vDateProp != 0)
    {
        t_int32 i;
        StringProperty * ip;
        UnicodeString u, startP, endP;
        UnicodeStringTokenizer * st;
        Period p;
        DateTime d;
        t_bool hasTimeZone = FALSE;
        ErrorCode status = ZERO_ERROR;
        VTimeZone * vtz;
        TimeZone * tz;

        for (i = 0; i < vDateProp->GetSize(); i++)
        {
            ip = (StringProperty *) vDateProp->GetAt(i);
            
            tz = 0;
    
            // FIRST, get matching timezone if it exists
            // getTZID from date, check for it from VTimezone vector.
            // then get the the NLSTimeZone from that VTimeZone
            u = ip->getParameterValue(
                JulianKeyword::Instance()->ms_sTZID, u, status);
            
            if (!FAILURE(status))
            {
                vtz = VTimeZone::getTimeZone(u, vTimeZones);
                if (vtz != 0)
                {
                    tz = vtz->getNLSTimeZone();
                }
            }

            // Get string, parse tokens seperated by comma
            u = *((UnicodeString *)ip->getValue());
    
            st = new UnicodeStringTokenizer(u, 
                JulianKeyword::Instance()->ms_sCOMMA_SYMBOL);
            PR_ASSERT(st != 0);
            if (st != 0)
            {
                while (st->hasMoreTokens())
                {
                    u = st->nextToken(u, status);
             
                    if (isRDate)
                    {
                        if (Period::IsParseable(u))
                        {
                            //adjust for timezones
                            //according to spec, a VALUE=PERIOD can't have a TZID with it
                            /*
                            if (tz != 0)
                            {
                                startP = u.extractBetween(0, u.indexOf('/'),
                                    startP);
                                endP = u.extractBetween(u.indexOf('/') + 1,
                                    u.size(), endP);
                                d.setTimeString(startP, tz);
                                startP = d.toISO8601();
                            
                                if (DateTime::IsParseableDateTime(endP))
                                {
                                    d.setTimeString(endP, tz);
                                        endP = d.toISO8601();
                                }
                                u = startP;
                                u += '/';
                                u += endP;
                            }
                            */
                            out->Add(new UnicodeString(u));
                        }
                        else if (DateTime::IsParseableDate(u))
                        {
                            out->Add(new UnicodeString(u));
                        }
                        else if (DateTime::IsParseableDateTime(u))
                        {
                            //adjust for timezones
                            if (tz != 0)
                            {
                                d.setTimeString(u, tz);       
                                u = d.toISO8601();
                            }
                            out->Add(new UnicodeString(u));
                        }
                        else
                        {
                               // TODO: Log bad rdate in rdate property vector
                        }
                    }
                    else
                    {
                        if (DateTime::IsParseableDateTime(u))
                        {
                            //adjust for timezones
                            if (tz != 0)
                            {
                                d.setTimeString(u, tz);       
                                u = d.toISO8601();
                            }
                            out->Add(new UnicodeString(u));
                        }
                        else
                        {
                            // TODO: Log bad exdate in exdate property vector
                        }
                    }
                }
                delete st; st = 0;
            }
        }
    }
}

//---------------------------------------------------------------------

void 
TimeBasedEvent::getPeriodRDates(JulianPtrArray * out)
{
    if (getRDates() != 0)
    {
        t_int32 i;
        ICalProperty * ip;
        UnicodeString u;
        UnicodeStringTokenizer * st;
        ErrorCode status = ZERO_ERROR;

        for (i = 0; i < getRDates()->GetSize(); i++)
        {
            ip = (ICalProperty *) getRDates()->GetAt(i);
            u = *((UnicodeString *)ip->getValue());
            st = new UnicodeStringTokenizer(u, 
                JulianKeyword::Instance()->ms_sCOMMA_SYMBOL);
            PR_ASSERT(st != 0);
            if (st != 0)
            {
                while (st->hasMoreTokens())
                {
                    u = st->nextToken(u, status);
                    if (Period::IsParseable(u))
                    {
                        out->Add(new UnicodeString(u));
                    }
                }
                delete st; st = 0;
            }
        }
    }
}

//---------------------------------------------------------------------
// TODO: remove asserts, make crash proof
void
TimeBasedEvent::populateDates(JulianPtrArray * vOut, 
                              JulianPtrArray * dates,
                              DateTime origStart, 
                              JulianPtrArray * vTimeZones)
{
    if (dates == 0)
        return;

    // NOTE: Remove later, to get rid of warnings
    if (vTimeZones) {}

    TimeBasedEvent * tbeClone;
    t_int32 i;
    Date difference = 0;
    DateTime d;

    // vector of strings of period form (i.e. 19980320T112233/PT1H)
    JulianPtrArray * vPeriods = new JulianPtrArray();
    PR_ASSERT(vPeriods != 0);

    getPeriodRDates(vPeriods);
    
    difference = this->difference();

    // TODO: handle alarms difference

    for (i = 0; i < dates->GetSize(); i++)
    {
        d = *((DateTime *) dates->GetAt(i));

        //if (FALSE) TRACE("d = %s\r\n", d.toISO8601().toCString(""));

        tbeClone = (TimeBasedEvent *) this->clone(m_Log);
        tbeClone->setDTStart(d);
        tbeClone->setOrigStart(origStart); // set origDTStart to recurrence dtstart
        tbeClone->setMyOrigStart(d);
        tbeClone->populateDatesHelper(d, difference, vPeriods);
        tbeClone->setRecurrenceID(d);

        // TODO: set alarms of clone
        //if (tbeClone->getAlarms() != 0)
        //{
        //}
    
        vOut->Add(tbeClone);

    }

    ICalComponent::deleteUnicodeStringVector(vPeriods);
    delete vPeriods; vPeriods = 0;
}

//---------------------------------------------------------------------

/**
*  checks recurrence data (rrules, rdates, exrules, exdates)
*  called after loading (postload check)
*/
/*
void checkRecurrence() 
{
    setRRules(Recurrence.rulesCheck(getRRules()));
    setExRules(Recurrence.rulesCheck(getExRules()));
    
    setRDates(dateChecker(getRDates(), TRUE));
    setExDates(dateChecker(getExDates(), FALSE));
}
*/

//---------------------------------------------------------------------

/**
* check the range of the property
* @param sPropName  property names
* @param sPropValue property value
* @param sRange     valid range of property values
*/
/*
void checkRange(UnicodeString sPropName, UnicodeString sPropValue, UnicodeString sRange[]) {
    t_boolean b = FALSE;
    
    if (sPropName.equalsIgnoreCase(PropertyKeywords.JulianKeyword::Instance()->ms_sCLASS)) {
      if (ParserUtil.isXToken(sPropValue))
	return;
      else {
        b = ParserUtil.checkRange(sPropValue, sRange);
        if (!b) {
          DebugMsg.Instance().println(0, "Bad value " + sPropValue + " for property " + sPropName);
          LogStream.Instance().println(2, Utility.ErrorMsg("InvalidPropertyValue") +
                     " TimeBasedEvent:" + sPropValue);
          setRequestStatus(Utility.ErrorMsg("RS201") + ";" + PropertyKeywords.JulianKeyword::Instance()->ms_sCLASS);
          setDefaultProps(sPropName);         
        }
        return;
      }
    }
    else { 
      b = ParserUtil.checkRange(sPropValue, sRange);
      if (!b) {
        DebugMsg.Instance().println(0, "Bad value: " + sPropValue + " for property " + sPropName);
        LogStream.Instance().println(2, Utility.ErrorMsg("InvalidPropertyValue") + 
                   " TimeBasedEvent:" + sPropValue);
        setRequestStatus(Utility.ErrorMsg("RS201") + ";" + sPropName);
        setDefaultProps(sPropName);
      }
      return;
    }
  }

  */

//---------------------------------------------------------------------

/**
 * sets default of property
 * @param sPropName property name
 */
 /*
void TimeBasedEvent::setDefaultProps(UnicodeString sPropName) {

    UnicodeString u;
    t_int32 hashCode = sPropName.hashCode();

    if (JulianKeyword::Instance()->ms_ATOM_DESCRIPTION == hashCode) {
      //LogStream.Instance().println(0, Utility.ErrorMsg("DefaultTBEDescription"));
      // Setting default Description to empty string
      //if (getDescription( == hashCode) {
      //  initializeDescription();
      //  Vector vDescLines = new Vector();
      //  vDescLines.addElement("");
      //  loadDescription(vDescLines);
      //}
        u = getSummary();
        setDescription(u);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_CLASS == hashCode) {
      //LogStream.Instance().println(0, Utility.ErrorMsg("DefaultTBEClass"));
        u = "";
      setClass(u);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_STATUS == hashCode) {
      //LogStream.Instance().println(0, Utility.ErrorMsg("DefaultTBEStatus"));
        u = "";
        setStatus(u);
    }
    //else if (sPropName.equalsIgnoreCase(PropertyKeywords.JulianKeyword::Instance()->ms_sTRANSP)) {
    //  LogStream.Instance().println(0, Utility.ErrorMsg("DefaultTBETransp"));
    //  setTransp(PropertyKeywords.JulianKeyword::Instance()->ms_sOPAQUE);
    //}
    else if (JulianKeyword::Instance()->ms_ATOM_REQUESTSTATUS == hashCode) {
      //LogStream.Instance().println(0, Utility.ErrorMsg("DefaultTBERequestStatus"));
        u = "";
        setRequestStatus(u);
    }
}
*/
//---------------------------------------------------------------------

void TimeBasedEvent::selfCheck()
{
    if (getSummary().size() <= 0)
    {
        // set summary to first 60 characters of description
        if (getDescription().size() > 0)
        {
            UnicodeString u = getDescription();
            if (u.size() > 60)
                u = u.removeBetween(60, u.size());
            setSummary(u);
        }
    }

    // if sequence is null, set to 0.
    if (getSequence() == -1)
        setSequence(0);

    // NOTE: setting default CLASS to PUBLIC if CLASS is invalid or not in range
    if (getClass().size() == 0 || 
        (getClass().compareIgnoreCase(JulianKeyword::Instance()->ms_sPRIVATE) != 0) &&
        (getClass().compareIgnoreCase(JulianKeyword::Instance()->ms_sPUBLIC) != 0) &&
        (getClass().compareIgnoreCase(JulianKeyword::Instance()->ms_sCONFIDENTIAL) != 0))

    {
        setClass(JulianKeyword::Instance()->ms_sPUBLIC);
    }
}

//---------------------------------------------------------------------
// GETTERS AND SETTERS here
//---------------------------------------------------------------------
// method
void TimeBasedEvent::setMethod(UnicodeString & s)
{
    m_sMethod = s;
}
//---------------------------------------------------------------------
// attendees
void TimeBasedEvent::addAttendee(Attendee * a)      
{ 
    if (m_AttendeesVctr == 0)
        m_AttendeesVctr = new JulianPtrArray(); 
    PR_ASSERT(m_AttendeesVctr != 0);
    if (m_AttendeesVctr != 0)
    {
        m_AttendeesVctr->Add(a);
    }
}
//---------------------------------------------------------------------
//LAST-MODIFIED
void TimeBasedEvent::setLastModified(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_LastModified == 0)
        m_LastModified = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_LastModified->setValue((void *) &s);
        m_LastModified->setParameters(parameters);
    }
    
}

DateTime TimeBasedEvent::getLastModified() const
{
    DateTime d(-1);
    if (m_LastModified == 0)
        return d; // return 0;
    else
    {
        d = *((DateTime *) m_LastModified->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
//Created
void TimeBasedEvent::setCreated(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_Created == 0)
        m_Created = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_Created->setValue((void *) &s);
        m_Created->setParameters(parameters);
    }
}
DateTime TimeBasedEvent::getCreated() const
{

    DateTime d(-1);
    if (m_Created == 0)
        return d;//return 0;
    else
    {
        d = *((DateTime *) m_Created->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
//Organizer
void TimeBasedEvent::setOrganizer(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);
    
    if (m_Organizer == 0)
    {
        //m_Organizer = ICalPropertyFactory::Make(ICalProperty::TEXT, 
        //                                    (void *) &s, parameters);
        
        m_Organizer = (ICalProperty *) new JulianOrganizer(m_Log);
        PR_ASSERT(m_Organizer != 0);
        if (m_Organizer != 0)
        {
            m_Organizer->setValue((void *) &s);
            m_Organizer->setParameters(parameters);
        }
    }
    else
    {
        m_Organizer->setValue((void *) &s);
        m_Organizer->setParameters(parameters);
    }
}
UnicodeString TimeBasedEvent::getOrganizer() const 
{
    UnicodeString u;
    if (m_Organizer == 0)
        return "";
    else {
        u = *((UnicodeString *) m_Organizer->getValue());
        return u;
    }
}
//---------------------------------------------------------------------

//RecurrenceID
void TimeBasedEvent::setRecurrenceID(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_RecurrenceID == 0)
    {
        m_RecurrenceID = (ICalProperty *) new JulianRecurrenceID(s, m_Log);
        PR_ASSERT(m_RecurrenceID != 0);
        if (m_RecurrenceID != 0)
        {
            m_RecurrenceID->setParameters(parameters);
        }
    }
    else
    {
        m_RecurrenceID->setValue((void *) &s);
        m_RecurrenceID->setParameters(parameters);
    }
}
DateTime TimeBasedEvent::getRecurrenceID() const
{

    DateTime d(-1);
    if (m_RecurrenceID == 0)
        return d;//return 0;
    else
    {
        d = *((DateTime *) m_RecurrenceID->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
//DTStamp
void TimeBasedEvent::setDTStamp(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_DTStamp == 0)
        m_DTStamp = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_DTStamp->setValue((void *) &s);
        m_DTStamp->setParameters(parameters);
    }
}
DateTime TimeBasedEvent::getDTStamp() const
{
    DateTime d(-1);
    if (m_DTStamp == 0)
        return d;//return 0;
    else
    {
        d = *((DateTime *) m_DTStamp->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
///DTStart
void TimeBasedEvent::setDTStart(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_DTStart == 0)
        m_DTStart = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_DTStart->setValue((void *) &s);
        m_DTStart->setParameters(parameters);
    }
}

DateTime TimeBasedEvent::getDTStart() const
{
    DateTime d(-1);
    if (m_DTStart == 0)
        return d; //return 0;
    else
    {
        d = *((DateTime *) m_DTStart->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
//Description
void TimeBasedEvent::setDescription(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Description == 0)
        m_Description = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Description->setValue((void *) &s);
        m_Description->setParameters(parameters);
    }
}
UnicodeString TimeBasedEvent::getDescription() const 
{
    if (m_Description == 0)
        return "";
    else
    {
        UnicodeString u;
        u = *((UnicodeString *) m_Description->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
//URL
void TimeBasedEvent::setURL(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_URL == 0)
        m_URL = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_URL->setValue((void *) &s);
        m_URL->setParameters(parameters);
    }    
}
UnicodeString TimeBasedEvent::getURL() const 
{
    if (m_URL == 0)
        return "";
    else
    {
        UnicodeString u;
        u = *((UnicodeString *) m_URL->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
//Summary
void TimeBasedEvent::setSummary(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Summary == 0)
        m_Summary = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Summary->setValue((void *) &s);
        m_Summary->setParameters(parameters);
    }
}
UnicodeString TimeBasedEvent::getSummary() const 
{
    if (m_Summary == 0)
        return "";
    else
    {
        UnicodeString u;
        u = *((UnicodeString *) m_Summary->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
//Class
void TimeBasedEvent::setClass(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Class == 0)
        m_Class = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Class->setValue((void *) &s);
        m_Class->setParameters(parameters);
    }
}
UnicodeString TimeBasedEvent::getClass() const 
{
    if (m_Class == 0)
        return "";
    else
    {
        UnicodeString u;
        u = *((UnicodeString *) m_Class->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
//Status
void TimeBasedEvent::setStatus(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Status == 0)
        m_Status = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Status->setValue((void *) &s);
        m_Status->setParameters(parameters);
    }
}

UnicodeString TimeBasedEvent::getStatus() const 
{
    UnicodeString u;
    if (m_Status == 0)
        return "";
    else
    {
        u = *((UnicodeString *) m_Status->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
//RequestStatus
void TimeBasedEvent::setRequestStatus(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_RequestStatus == 0)
        m_RequestStatus = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_RequestStatus->setValue((void *) &s);
        m_RequestStatus->setParameters(parameters);
    }
}
UnicodeString TimeBasedEvent::getRequestStatus() const 
{
    UnicodeString u;
    if (m_RequestStatus == 0)
        return "";
    else 
    {
        u = *((UnicodeString *) m_RequestStatus->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
//UID
void TimeBasedEvent::setUID(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);
    
    if (m_UID == 0)
        m_UID = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_UID->setValue((void *) &s);
        m_UID->setParameters(parameters);
    }
}
UnicodeString TimeBasedEvent::getUID() const 
{
    UnicodeString u;
    if (m_UID == 0)
        return "";
    else {
        u = *((UnicodeString *) m_UID->getValue());
        return u;
    }
}

//---------------------------------------------------------------------
//Sequence
void TimeBasedEvent::setSequence(t_int32 i, JulianPtrArray * parameters)
{ 
    if (m_Sequence == 0)
        m_Sequence = ICalPropertyFactory::Make(ICalProperty::INTEGER, 
                                            (void *) &i, parameters);
    else
    {
        m_Sequence->setValue((void *) &i);
        m_Sequence->setParameters(parameters);
    }
}
t_int32 TimeBasedEvent::getSequence() const 
{
    t_int32 i;
    if (m_Sequence == 0)
        return -1;
    else
    {
        i = *((t_int32 *) m_Sequence->getValue());
        return i;
    }
}
//---------------------------------------------------------------------
//comment
void TimeBasedEvent::addComment(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *) &s, parameters);
    addCommentProperty(prop);
}
void TimeBasedEvent::addCommentProperty(ICalProperty * prop)
{
    if (m_CommentVctr == 0)
        m_CommentVctr = new JulianPtrArray();    
    PR_ASSERT(m_CommentVctr != 0);
    if (m_CommentVctr != 0)
    {
        m_CommentVctr->Add(prop);
    }
}
void TimeBasedEvent::setNewComments(UnicodeString s)
{
    // first delete old comments
    if (m_CommentVctr != 0) 
    { ICalProperty::deleteICalPropertyVector(m_CommentVctr);
        delete m_CommentVctr; m_CommentVctr = 0; }
    addComment(s);
}
//---------------------------------------------------------------------
// attach
void TimeBasedEvent::addAttach(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *) &s, parameters);
    addAttachProperty(prop);
}
void TimeBasedEvent::addAttachProperty(ICalProperty * prop)      
{ 
    if (m_AttachVctr == 0)
        m_AttachVctr = new JulianPtrArray(); 
    PR_ASSERT(m_AttachVctr != 0);
    if (m_AttachVctr != 0)
    {
        m_AttachVctr->Add(prop);
    }
}
//---------------------------------------------------------------------
// RelatedTo
void TimeBasedEvent::addRelatedTo(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *) &s, parameters);
    addRelatedToProperty(prop);
} 
void TimeBasedEvent::addRelatedToProperty(ICalProperty * prop)      
{ 
    if (m_RelatedToVctr == 0)
        m_RelatedToVctr = new JulianPtrArray(); 
    PR_ASSERT(m_RelatedToVctr != 0);
    if (m_RelatedToVctr != 0)
    {
        m_RelatedToVctr->Add(prop);
    }
}
//void TimeBasedEvent::setRelatedTo(UnicodeString s, JulianPtrArray * parameters)
//{
//    //UnicodeString * s_ptr = new UnicodeString(s);
//    //PR_ASSERT(s_ptr != 0);
//    
//    if (m_RelatedTo == 0)
//        m_RelatedTo = ICalPropertyFactory::Make(ICalProperty::TEXT, 
//                                            (void *) &s, parameters);
//    else
//    {
//        m_RelatedTo->setValue((void *) &s);
//        m_RelatedTo->setParameters(parameters);
//    }
//}
//UnicodeString TimeBasedEvent::getRelatedTo() const 
//{
//    UnicodeString u;
//    if (m_RelatedTo == 0)
//        return "";
//    else {
//        u = *((UnicodeString *) m_RelatedTo->getValue());
//       return u;
//    }
//}
//---------------------------------------------------------------------
// Contact
void TimeBasedEvent::addContact(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *)&s, parameters);
    addContactProperty(prop);
}
void TimeBasedEvent::addContactProperty(ICalProperty * prop)    
{ 
    if (m_ContactVctr == 0)
        m_ContactVctr = new JulianPtrArray(); 
    PR_ASSERT(m_ContactVctr != 0);
    if (m_ContactVctr != 0)
    {
        m_ContactVctr->Add(prop);
    }
}
//---------------------------------------------------------------------
// RDate
void TimeBasedEvent::addRDate(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *)&s, parameters);
    addRDateProperty(prop);
}
void TimeBasedEvent::addRDateProperty(ICalProperty * prop)    
{ 
    if (m_RDateVctr == 0)
        m_RDateVctr = new JulianPtrArray(); 
    PR_ASSERT(m_RDateVctr != 0);
    if (m_RDateVctr != 0)
    {
        m_RDateVctr->Add(prop);
    }
}
//---------------------------------------------------------------------
// ExDate
void TimeBasedEvent::addExDate(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *)&s, parameters);
    addExDateProperty(prop);
}
void TimeBasedEvent::addExDateProperty(ICalProperty * prop)    
{ 
    if (m_ExDateVctr == 0)
        m_ExDateVctr = new JulianPtrArray(); 
    PR_ASSERT(m_ExDateVctr != 0);
    if (m_ExDateVctr != 0)
    {
        m_ExDateVctr->Add(prop);
    }
}
//---------------------------------------------------------------------
// Categories
void TimeBasedEvent::addCategories(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *)&s, parameters);
    addCategoriesProperty(prop);
}
void TimeBasedEvent::addCategoriesProperty(ICalProperty * prop)
{
    if (m_CategoriesVctr == 0)
        m_CategoriesVctr = new JulianPtrArray(); 
    PR_ASSERT(m_CategoriesVctr != 0);
    if (m_CategoriesVctr != 0)
    {
        m_CategoriesVctr->Add(prop);
    }
}
//---------------------------------------------------------------------
// XTOKENS
void TimeBasedEvent::addXTokens(UnicodeString s)         
{
    if (m_XTokensVctr == 0)
        m_XTokensVctr = new JulianPtrArray(); 
    PR_ASSERT(m_XTokensVctr != 0);
    if (m_XTokensVctr != 0)
    {
        m_XTokensVctr->Add(new UnicodeString(s));
    }
}
/*
// RDate
void TimeBasedEvent::addRDateString(UnicodeString s)         
{
    if (m_RDateVctr == 0)
        m_RDateVctr = new JulianPtrArray(); 
    PR_ASSERT(m_RDateVctr != 0);
    m_RDateVctr->Add(new UnicodeString(s));
}
*/
// RRule
void TimeBasedEvent::addRRuleString(UnicodeString s)         
{
    if (m_RRuleVctr == 0)
        m_RRuleVctr = new JulianPtrArray(); 
    PR_ASSERT(m_RRuleVctr != 0);
    if (m_RRuleVctr != 0)
    {
        m_RRuleVctr->Add(new UnicodeString(s));
    }
}
/*
// ExDate
void TimeBasedEvent::addExDateString(UnicodeString s)         
{
    if (m_ExDateVctr == 0)
        m_ExDateVctr = new JulianPtrArray(); 
    PR_ASSERT(m_ExDateVctr != 0);
    m_ExDateVctr->Add(new UnicodeString(s));
}
*/
// ExRule
void TimeBasedEvent::addExRuleString(UnicodeString s)         
{
    if (m_ExRuleVctr == 0)
        m_ExRuleVctr = new JulianPtrArray(); 
    PR_ASSERT(m_ExRuleVctr != 0);
    if (m_ExRuleVctr != 0)
    {
        m_ExRuleVctr->Add(new UnicodeString(s));
    }
}
//---------------------------------------------------------------------
/*
void 
TimeBasedEvent::setDateTimeValue(ICalProperty * dateTimeProperty,
                                 DateTime * s)
{ 
    setDateTimeProperty(dateTimeProperty, s, 0); 
} 

void
TimeBasedEvent::setDateTimeProperty(ICalProperty * dateTimeProperty,
                                    DateTime * s, JulianPtrArray * parameters)
{ 
    if (dateTimeProperty == 0)
        dateTimeProperty = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) s, parameters);
    else
        dateTimeProperty->setValue((void *) s);
}

DateTime * TimeBasedEvent::getDateTimeValue(ICalProperty * dateTimeProperty) const 
{
    if (dateTimeProperty == 0)
        return 0;
    else
        return (DateTime *) dateTimeProperty->getValue();
}
//---------------------------------------------------------------------

void 
TimeBasedEvent::setStringValue(ICalProperty * stringProperty,
                               UnicodeString * s)
{
    setStringProperty(stringProperty, s, 0);
}

void
TimeBasedEvent::setStringProperty(ICalProperty * stringProperty,
                                  UnicodeString * s, JulianPtrArray * parameters)
{
    if (stringProperty == 0)
        stringProperty = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) s, parameters);
    else
        stringProperty->setValue((void *) s);
}

UnicodeString TimeBasedEvent::getStringValue(ICalProperty * stringProperty) const
{
    if (stringProperty == 0)
        return "";
    else
        return *((UnicodeString *) stringProperty->getValue());
}
//---------------------------------------------------------------------

void 
TimeBasedEvent::setIntegerValue(ICalProperty * integerProperty,
                                 t_int32 i)
{ setIntegerProperty(integerProperty, i, 0); } 

void
TimeBasedEvent::setIntegerProperty(ICalProperty * integerProperty,
                                    t_int32 i, JulianPtrArray * parameters)
{ 
    if (integerProperty == 0)
        integerProperty = ICalPropertyFactory::Make(ICalProperty::INTEGER, 
                                            (void *) &i, parameters);
    else
        integerProperty->setValue((void *) i);
}

t_int32 TimeBasedEvent::getIntegerValue(ICalProperty * integerProperty) const 
{
    if (integerProperty == 0)
        return -1;
    else
        return *((t_int32 *) integerProperty->getValue());
}
//---------------------------------------------------------------------
void 
TimeBasedEvent::setDurationValue(ICalProperty * durationProperty,
                                 Duration * s)
{ 
    setDurationProperty(durationProperty, s, 0); 
} 

void
TimeBasedEvent::setDurationProperty(ICalProperty * durationProperty,
                                    Duration * s, JulianPtrArray * parameters)
{ 
    if (durationProperty == 0)
        durationProperty = ICalPropertyFactory::Make(ICalProperty::DURATION, 
                                            (void *) s, parameters);
    else
        durationProperty->setValue((void *) s);
}

Duration * TimeBasedEvent::getDurationValue(ICalProperty * durationProperty) const 
{
    if (durationProperty == 0)
        return 0;
    else
        return (Duration *) durationProperty->getValue();
}
*/
//---------------------------------------------------------------------

void
TimeBasedEvent::addCategoriesPropertyVector(UnicodeString & propVal,
                                            JulianPtrArray * parameters)
{  
    //ICalProperty * ip;

    // TODO: trying to break each comma seperated element into own property, but
    // having trouble, for now, don't break up
     //ip = ICalPropertyFactory::Make(ICalProperty::TEXT, new UnicodeString(propVal),
     //       parameters);
     addCategories(propVal, parameters);
 
     /*
    ErrorCode status = ZERO_ERROR;
    UnicodeStringTokenizer * st;
    UnicodeString us;
    UnicodeString sDelim = ",";

    st = new UnicodeStringTokenizer(propVal, sDelim);

    while (st->hasMoreTokens())
    {
        us = st->nextToken(us, status);
        us.trim();
        //TRACE("us = %s, status = %d", us.toCString(""), status);
        // TODO: if us is in categories range then add, else, log and error,
        // should I make copies of parameters or not???

        ip = ICalPropertyFactory::Make(ICalProperty::TEXT, new UnicodeString(us),
            parameters);
        PR_ASSERT(ip != 0);
        addCategoriesProperty(ip);
        ip = 0;
    }
    delete st; st = 0;;
    */
}
//---------------------------------------------------------------------

