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

// vfrbsy.cpp
// John Sun
// 4:46 PM Febuary 19 1998

#include "stdafx.h"
#include "jdefines.h"

#include "vfrbsy.h"
#include "prprty.h"
#include "prprtyfy.h"
#include "freebusy.h"
#include "jutility.h"
#include "jlog.h"
#include "vtimezne.h"
#include "keyword.h"
#include "orgnzr.h"
//---------------------------------------------------------------------
/*
//const 
UnicodeString VFreebusy::ms_sRequestMessage =
"%v%C%B%e%s%U%Z";
//const
UnicodeString VFreebusy::ms_sPublishMessage =
"%v%Y%H%B%e%C%J%o%U%K%t%M%Z";
//const 
UnicodeString VFreebusy::ms_sReplyMessage = 
"%v%Y%C%B%e%o%T%U%K%t%M%u%Z";
//const 
UnicodeString VFreebusy::ms_sAllMessage = 
"%v%Y%C%B%e%D%o%T%U%K%t%M%u%s%Z";

UnicodeString VFreebusy::m_strDefaultFmt = 
"%(EEE MM/dd/yy hh:mm:ss z)B - %(EEE MM/dd/yy hh:mm:ss z)e, \r\n\t %U, %s, \r\n%Y\r\n %(\r\n\t^N ^R, ^S)v %w";     
//"%(EEE MM/dd/yy hh:mm:ss z)B - %(EEE MM/dd/yy hh:mm:ss z)e, \r\n\t %U, %s, \r\n%Y %v %w";     
*/

//---------------------------------------------------------------------

// private never use
#if 0
VFreebusy::VFreebusy()
{
    PR_ASSERT(FALSE);
}
#endif
  
//---------------------------------------------------------------------
void VFreebusy::selfCheck()
{
    if (getFreebusy() != 0)
        normalize();
}
//---------------------------------------------------------------------

void VFreebusy::normalize()
{
    // Run the merge algorithm to get
    // from

    // FB1: pA, pB, pC
    // FB2: pD, pE, pF
    // PB3: pG, pH

    // to:

    // FB1: pA
    // FB2: pB
    // FB3: pC
    // FB4: pD
    // ../etc.

    // first merge similar freebusy.
    // 1) combine same params freebusy
    //UnicodeString sResult, s;

    JulianPtrArray * v = 0;
    Freebusy * f1 = 0, * f2 = 0;
    t_int32 i = 0, j = 0;
    v = getFreebusy();
    if (v != 0 && v->GetSize() >= 2)
    {
        // merge similar freebusy
        for (i = 0; i < v->GetSize() - 1; i++)
        {
            f1 = (Freebusy *) v->GetAt(i);
            for (j = i + 1; j < v->GetSize() ; j++)
            {
                f2 = (Freebusy *) v->GetAt(j);
                if (Freebusy::HasSameParams(f1, f2))
                {
                    // add all periods in f1 into f2
                    // remove f1 from v. delete f1
                    f1->addAllPeriods(f2->getPeriod());
                    delete f2; f2 = 0;
                    v->RemoveAt(j);
                    j--;
                }
            }
        }
    }

    //---------------------------------------------------------------------
    // DEBUG PRINT
    /*
    v = getFreebusy();
    if (v != 0)
    {
        for (i = 0; i < v->GetSize(); i++)
        {   
            s = ((Freebusy *) v->GetAt(i))->toICALString(s);
            sResult += ICalProperty::multiLineFormat(s);
        }
    }
    //if (FALSE) TRACE("after merge 1) = %s\r\n", sResult.toCString(""));    
    */
    //---------------------------------------------------------------------


    // 2) normalize each freebusy (compress overlapping periods)
    v = getFreebusy();
    PR_ASSERT(v != 0);
    for (i = 0; i < v->GetSize(); i++)
    {
        f1 = (Freebusy *) v->GetAt(i);
        f1->normalizePeriods();
    }

    //---------------------------------------------------------------------
    //  DEBUG PRINT
    /*
    sResult = "";
    v = getFreebusy();
    if (v != 0)
    {
        for (i = 0; i < v->GetSize(); i++)
        {   
            s = ((Freebusy *) v->GetAt(i))->toICALString(s);
            sResult += ICalProperty::multiLineFormat(s);
        }
    }
    //if (FALSE) TRACE("after normalize 2) = %s\r\n", sResult.toCString(""));    
    */
    //---------------------------------------------------------------------

    // 3) break up freebusy vector like so:
    // FB1: pA, pB, pC
    // FB2: pD, pE, pF
    // PB3: pG, pH

    // to:

    // FB1: pA
    // FB2: pB
    // FB3: pC
    // FB4: pD
    // ../etc.
    v = getFreebusy();
    PR_ASSERT(v != 0);
    t_int32 oldSize = v->GetSize();
    for (i = oldSize - 1; i >= 0; i--)
    {
        f1 = (Freebusy *) v->GetAt(i);
        if (f1->getPeriod() != 0)
        {
            // foreach period in f1, create a new freebusy for it,
            // add to freebusy vector
            for (j = 0; j < f1->getPeriod()->GetSize(); j++)
            {
                f2 = new Freebusy(m_Log);
                PR_ASSERT(f2 != 0);
                f2->setType(f1->getType());
#if 0
                f2->setStatus(f1->getStatus());
#endif
                f2->addPeriod((Period *) f1->getPeriod()->GetAt(j));
                addFreebusy(f2);
            }
        }
        // now delete f1
        delete f1; f1 = 0;
        v->RemoveAt(i);
    }

    //---------------------------------------------------------------------
    //  DEBUG PRINT
    /*
    sResult = "";
    v = getFreebusy();
    if (v != 0)
    {
        for (i = 0; i < v->GetSize(); i++)
        {   
            s = ((Freebusy *) v->GetAt(i))->toICALString(s);
            sResult += ICalProperty::multiLineFormat(s);
        }
    }
    //if (FALSE) TRACE("after breakup 3) = %s\r\n", sResult.toCString(""));    
    */
    //---------------------------------------------------------------------

    // Step 4) I will now add FREE freebusy periods to those periods not
    // already covered.
    v = getFreebusy();
    PR_ASSERT(v != 0);
    Freebusy * fb;
    Period *p;
    oldSize = v->GetSize();
    if (getDTStart().isValid() && getDTEnd().isValid())
    {
        sortFreebusy();
        DateTime startTime, nextTime, endTime;
        startTime = getDTStart();
        nextTime = startTime;
        for (i = 0; i < oldSize; i++)
        {
            fb = (Freebusy *) v->GetAt(i);

            PR_ASSERT(fb != 0 && fb->getPeriod()->GetSize() == 1);
            
            p = (Period *) fb->getPeriod()->GetAt(0);

            nextTime = p->getEndingTime(nextTime);
            endTime = p->getStart();

            if (startTime < endTime)
            {
                // add a new freebusy period
                fb = new Freebusy(m_Log); PR_ASSERT(fb != 0);
                fb->setType(Freebusy::FB_TYPE_FREE);
#if 0
                fb->setStatus(Freebusy::FB_STATUS_TENTATIVE); // for dbg
#endif
                p = new Period();
                p->setStart(startTime);
                p->setEnd(endTime);
                fb->addPeriod(p); // ownership of p not taken, must delete
                delete p; p = 0;
                v->Add(fb); // fb ownership taken
            }
            startTime = nextTime;
        }
        endTime = getDTEnd();
        if (nextTime < endTime)
        {
            // add final period 
            // add a new freebusy period
            fb = new Freebusy(m_Log); PR_ASSERT(fb != 0);
            fb->setType(Freebusy::FB_TYPE_FREE);
#if 0
            fb->setStatus(Freebusy::FB_STATUS_TENTATIVE); // for dbg
#endif
            p = new Period();
            p->setStart(nextTime);
            p->setEnd(endTime);
            fb->addPeriod(p); // ownership of p not taken, must delete
            delete p; p = 0;
            v->Add(fb); // fb ownership taken
        }
        sortFreebusy();
    }

    //---------------------------------------------------------------------
    //  DEBUG PRINT
    /*
    sResult = "";
    v = getFreebusy();
    if (v != 0)
    {
        for (i = 0; i < v->GetSize(); i++)
        {   
            s = ((Freebusy *) v->GetAt(i))->toICALString(s);
            sResult += ICalProperty::multiLineFormat(s);
        }
    }
    //if (FALSE) TRACE("after make free 4) = %s\r\n", sResult.toCString(""));    
    */
    //---------------------------------------------------------------------
}
//---------------------------------------------------------------------

void VFreebusy::sortFreebusy()
{
    if (m_FreebusyVctr != 0)
        m_FreebusyVctr->QuickSort(Freebusy::CompareFreebusy);
}

//---------------------------------------------------------------------

void VFreebusy::stamp()
{
    DateTime d;
    setDTStamp(d);
}

//---------------------------------------------------------------------

VFreebusy::VFreebusy(JLog * initLog)
: m_AttendeesVctr(0), m_CommentVctr(0), m_Created(0), m_Duration(0), 
  m_DTEnd(0), m_DTStart(0), m_DTStamp(0), m_FreebusyVctr(0), 
  m_LastModified(0), 
  m_Organizer(0),
  m_RequestStatus(0), 
  //m_RelatedToVctr(0), 
  m_Sequence(0), m_UID(0),
  m_URL(0),
  //m_URLVctr(0), 
  m_XTokensVctr(0),
  m_Log(initLog)
{
    //PR_ASSERT(initLog != 0);
}

//---------------------------------------------------------------------

VFreebusy::VFreebusy(VFreebusy & that)
: m_AttendeesVctr(0), m_CommentVctr(0), m_Created(0), m_Duration(0), 
  m_DTEnd(0), m_DTStart(0), m_DTStamp(0), m_FreebusyVctr(0), 
  m_LastModified(0), 
  m_Organizer(0),
  m_RequestStatus(0), 
  //m_RelatedToVctr(0), 
  m_Sequence(0), m_UID(0),
  m_URL(0),
  //m_URLVctr(0), 
  m_XTokensVctr(0)
{
    if (that.m_AttendeesVctr != 0)
    {
        m_AttendeesVctr = new JulianPtrArray(); PR_ASSERT(m_AttendeesVctr != 0);
        if (m_AttendeesVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_AttendeesVctr, m_AttendeesVctr, m_Log);
        }
    }
    if (that.m_CommentVctr != 0)
    {
        m_CommentVctr = new JulianPtrArray(); PR_ASSERT(m_CommentVctr != 0);
        if (m_CommentVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_CommentVctr, m_CommentVctr, m_Log);
        }
    }
    if (that.m_Created != 0) { m_Created = that.m_Created->clone(m_Log); }
    if (that.m_Duration != 0) { m_Duration = that.m_Duration->clone(m_Log); }
    if (that.m_DTEnd != 0) { m_DTEnd = that.m_DTEnd->clone(m_Log); }
    if (that.m_DTStart != 0) { m_DTStart = that.m_DTStart->clone(m_Log); }
    if (that.m_DTStamp != 0) { m_DTStamp = that.m_DTStamp->clone(m_Log); }

    if (that.m_FreebusyVctr != 0)
    {
        m_FreebusyVctr = new JulianPtrArray(); PR_ASSERT(m_FreebusyVctr != 0);
        if (m_FreebusyVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_FreebusyVctr, m_FreebusyVctr, m_Log);
        }
    }

    if (that.m_LastModified != 0) { m_LastModified = that.m_LastModified->clone(m_Log); }
    if (that.m_Organizer != 0) { m_Organizer = that.m_Organizer->clone(m_Log); }
    if (that.m_RequestStatus != 0) { m_RequestStatus = that.m_RequestStatus->clone(m_Log); }
    if (that.m_Sequence != 0) { m_Sequence = that.m_Sequence->clone(m_Log); }
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

ICalComponent * 
VFreebusy::clone(JLog * initLog)
{
    m_Log = initLog; 
    //PR_ASSERT(m_Log != 0);
    return new VFreebusy(*this);
}

//---------------------------------------------------------------------
VFreebusy::~VFreebusy()
{
    // datetime properties
    if (m_Created != 0) { delete m_Created; m_Created = 0; }
    if (m_DTStart != 0) { delete m_DTStart; m_DTStart = 0; }
    if (m_DTStamp != 0) { delete m_DTStamp; m_DTStamp = 0; }
    if (m_LastModified != 0) { delete m_LastModified; m_LastModified = 0; }
    if (m_DTEnd != 0) { delete m_DTEnd; m_DTEnd = 0; }
    if (m_Duration != 0) { delete m_Duration; m_Duration = 0; }

    // string properties
    if (m_RequestStatus != 0) { delete m_RequestStatus; m_RequestStatus = 0; }
    if (m_Organizer != 0) { delete m_Organizer; m_Organizer = 0; }
    if (m_UID != 0) { delete m_UID; m_UID = 0; }
    if (m_URL != 0) { delete m_URL; m_URL = 0; }

    // integer properties
    if (m_Sequence != 0) { delete m_Sequence; m_Sequence = 0; }

    // attendees
    if (m_AttendeesVctr != 0) { 
        Attendee::deleteAttendeeVector(m_AttendeesVctr);
        delete m_AttendeesVctr; m_AttendeesVctr = 0; 
    }
    // freebusy
    if (m_FreebusyVctr != 0) { 
        Freebusy::deleteFreebusyVector(m_FreebusyVctr);
        delete m_FreebusyVctr; m_FreebusyVctr = 0; 
    }

    // string vectors
    if (m_CommentVctr != 0) { ICalProperty::deleteICalPropertyVector(m_CommentVctr);
        delete m_CommentVctr; m_CommentVctr = 0; }
    //if (m_RelatedToVctr != 0) { ICalProperty::deleteICalPropertyVector(m_RelatedToVctr);
    //    delete m_RelatedToVctr; m_RelatedToVctr = 0; }
    //if (m_URLVctr != 0) { ICalProperty::deleteICalPropertyVector(m_URLVctr);
    //    delete m_URLVctr; m_URLVctr = 0; }
    if (m_XTokensVctr != 0) { ICalComponent::deleteUnicodeStringVector(m_XTokensVctr);
        delete m_XTokensVctr; m_XTokensVctr = 0; 
    }
}
//---------------------------------------------------------------------
UnicodeString & 
VFreebusy::parse(ICalReader * brFile, UnicodeString & method, 
                 UnicodeString & parseStatus, JulianPtrArray * vTimeZones,
                 t_bool bIgnoreBeginError, JulianUtility::MimeEncoding encoding)
{
    parseStatus = JulianKeyword::Instance()->ms_sOK;
    UnicodeString strLine, propName, propVal;

    JulianPtrArray * parameters = new JulianPtrArray();
    PR_ASSERT(parameters != 0 && brFile != 0);
    if (parameters == 0 || brFile == 0)
    {
        // ran out of memory, return invalid VFreebusy
        return parseStatus;
    }
    
    ErrorCode status = ZERO_ERROR;

    setMethod(method);

    while (TRUE)
    {
        PR_ASSERT(brFile != 0);
        brFile->readFullLine(strLine, status);
        //strLine.trim();
        ICalProperty::Trim(strLine);

        //if (FALSE) TRACE("line (size = %d) = ---%s---\r\n", strLine.size(), strLine.toCString("")); 
        if (FAILURE(status) && strLine.size() <= 0)
            break;
        
        ICalProperty::parsePropertyLine(strLine, propName, propVal, parameters);

        //if (FALSE) TRACE("propName = --%s--, propVal = --%s--, paramSize = %d\r\n", propName.toCString(""), propVal.toCString(""), parameters->GetSize());
       
        if (strLine.size() == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            
            continue;
        }
        else if (strLine.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND_VFREEBUSY) == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            
            break;
        }
        else if (
            ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) &&
             ((propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVFREEBUSY) == 0) && !bIgnoreBeginError )||
             ((propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0) ||
              (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0) ||
              (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0) ||
              (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0) ||
              (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0))
            ) ||
            ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
            (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0))
            ) //else if
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();

            parseStatus = strLine;
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sAbruptEndOfParsing, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 300);
            break;
        }
        else
        {
//#ifdef TIMING
            //clock_t start, end;
            //start = clock();
//#endif
            storeData(strLine, propName, propVal, parameters, vTimeZones);
//#ifdef TIMING
            //end = clock();
            //double d = end - start;
            //if (FALSE) TRACE("storeData on %s took %f ms.\r\n", propName.toCString(""), d);
//#endif
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();

        }
    } 
    ICalProperty::deleteICalParameterVector(parameters);
    parameters->RemoveAll();
    delete parameters; parameters = 0;

    selfCheck();
    sortFreebusy();
    return parseStatus;
}
//---------------------------------------------------------------------
t_bool VFreebusy::storeData(UnicodeString & strLine, UnicodeString & propName, 
                            UnicodeString & propVal, JulianPtrArray * parameters, 
                            JulianPtrArray * vTimeZones)
{
    //UnicodeString u;
    if (vTimeZones == 0)
    {
    }

    t_int32 hashCode = propName.hashCode();    
    t_bool bParamValid;

    //if (propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sATTENDEE) == 0)
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
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }
        addComment(propVal, parameters);
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_CREATED == hashCode)
    {
        // no parameters (MUST BE IN UTC)
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }

        if (getCreatedProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
        }
        DateTime d;
        d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

        setCreated(d , parameters);   
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_LASTMODIFIED == hashCode)
    {
        // no parameters (MUST BE IN UTC)
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }

        if (getLastModified().getTime() >= 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
        }
        DateTime d;
        d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

        setLastModified(d, parameters);   
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_DTEND == hashCode)
    {
        // no parameters (MUST BE IN UTC)
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }

        // DTEND must be in UTC
        if (getDTEndProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
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
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }

        if (getDurationProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
        }
        Duration d(propVal);
        setDuration(d, parameters);
        return TRUE;
    } 
    else if (JulianKeyword::Instance()->ms_ATOM_DTSTART == hashCode)
    {
        // no parameters (MUST BE IN UTC)
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }

        // DTSTART must be in UTC
        if (getDTStartProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
        }
        DateTime d;
        d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

        setDTStart(d, parameters);
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_DTSTAMP == hashCode)
    {
        // no parameters (MUST BE IN UTC)
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }

        if (getDTStampProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
        }
        DateTime d;
        d = VTimeZone::DateTimeApplyTimeZone(propVal, vTimeZones, parameters);

        setDTStamp(d, parameters);   
        return TRUE;
    }  
    else if (JulianKeyword::Instance()->ms_ATOM_FREEBUSY == hashCode)
    {
        // Freebusy must be in UTC
        Freebusy * freeb = new Freebusy(m_Log);
        PR_ASSERT(freeb != 0);
        if (freeb != 0)
        {
            freeb->parse(propVal, parameters, vTimeZones);
            if (!freeb->isValid())
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sInvalidFreebusy, 200);
                UnicodeString u;
                u = JulianLogErrorMessage::Instance()->ms_sRS202;
                u += '.'; u += ' ';
                u += strLine;
                //setRequestStatus(JulianLogErrorMessage::Instance()->ms_sRS202); 
                setRequestStatus(u);
                delete freeb; freeb = 0;
            }
            else
            {
                addFreebusy(freeb);
            }
        }
        return TRUE;
    }
    //else if (JulianKeyword::Instance()->ms_ATOM_RELATEDTO == hashCode)
    //{
    //    addRelatedTo(propVal, parameters);
    //    return TRUE;
    //}  
    else if (JulianKeyword::Instance()->ms_ATOM_SEQUENCE == hashCode)
    {
        t_bool bParseError = FALSE;
        t_int32 i;
        
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }

        i = JulianUtility::atot_int32(propVal.toCString(""), bParseError, propVal.size());
        if (getSequenceProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
        }
        if (!bParseError)
        {
            setSequence(i, parameters);
        }
        else
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidNumberFormat, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, propVal, 200);
        }
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
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }

        if (getOrganizerProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
        }
        setOrganizer(propVal, parameters);
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
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }
#if 0
        /* Don't print duplicated property on Request Status */
        if (getRequestStatusProperty() != 0)
        {
            if (m_Log) m_Log->logString(
            JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
        }
        
#endif
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
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }
        if (getUIDProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
        }
        setUID(propVal, parameters);
        return TRUE;
    }   
    else if (JulianKeyword::Instance()->ms_ATOM_URL == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, strLine, 100);
        }
         if (getURLProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 100);
        }
        setURL(propVal, parameters);
        //addURL(propVal, parameters);
        return TRUE;
    }   
    else if (ICalProperty::IsXToken(propName))
    {
        addXTokens(strLine);
        return TRUE;
    }
    else 
    {
        if (m_Log) m_Log->logString(
            JulianLogErrorMessage::Instance()->ms_sInvalidPropertyName, 
            JulianKeyword::Instance()->ms_sVFREEBUSY, propName, 200);
        return FALSE;
    }
}
//---------------------------------------------------------------------
t_bool VFreebusy::isValid()
{
    DateTime dS, dE;
    dS = getDTStart();
    dE = getDTEnd();

    // TODO: handle bad method names

    if (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sPUBLISH) == 0)
    {
        // publish must have dtstamp
        if (!getDTStamp().isValid())
            return FALSE;

        // TODO: must have originator attendee address
        if (getAttendees() == 0 || getAttendees()->GetSize() == 0)
            return FALSE;
    }
    else if (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREQUEST) == 0)
    {
        // publish must have dtstamp
        if (!getDTStamp().isValid())
            return FALSE;
     
        // TODO: must have requested attendee address
        if (getAttendees() == 0 || getAttendees()->GetSize() == 0)
            return FALSE;

        // must have organizer
        if (getOrganizer().size() <= 0)
            return FALSE;

        // must have start, end
        if (!dS.isValid() || !dE.isValid() || dS.afterDateTime(dE))
            return FALSE;
    }
    else if (getMethod().compareIgnoreCase(JulianKeyword::Instance()->ms_sREPLY) == 0)
    {
        // both request and reply must have valid dtstamp and a UID
        if ((!getDTStamp().isValid()) || (getUID().size() == 0))
            return FALSE;
    
        // TODO: must have recipient replying address
        if (getAttendees() == 0 || getAttendees()->GetSize() == 0)
            return FALSE;

        // must have originator' organizer address
        if (getOrganizer().size() <= 0)
            return FALSE;

        // must have start, end
        if (!dS.isValid() || !dE.isValid() || dS.afterDateTime(dE))
            return FALSE;
    }

    /*
    // both request and reply must have valid dtstamp and a UID
        if ((!getDTStamp().isValid()) || (getUID().size() == 0))
            return FALSE;
    
    // sequence must be >= 0
    if (getSequence() < 0)
        return FALSE;

    // if a reply
    if (dS.isValid() || dE.isValid())
    {
        // ds and de must be valid and dS cannot be after dE
        if (!dS.isValid() || !dE.isValid() || dS.afterDateTime(dE))
            return FALSE;
    }
    */

    return TRUE;
}
//---------------------------------------------------------------------
UnicodeString VFreebusy::toICALString()
{
    return formatHelper(JulianFormatString::Instance()->ms_sVFreebusyAllMessage);
}
//---------------------------------------------------------------------
UnicodeString VFreebusy::toICALString(UnicodeString sMethod, 
                                      UnicodeString sName,
                                      t_bool isRecurring)
{
    // NOTE: Remove later, to avoid warnings
    if (isRecurring) {}

    if (sMethod.compareIgnoreCase(JulianKeyword::Instance()->ms_sPUBLISH) == 0)
        return formatHelper(JulianFormatString::Instance()->ms_sVFreebusyPublishMessage);
    else if (sMethod.compareIgnoreCase(JulianKeyword::Instance()->ms_sREQUEST) == 0)
        return formatHelper(JulianFormatString::Instance()->ms_sVFreebusyRequestMessage);
    else if (sMethod.compareIgnoreCase(JulianKeyword::Instance()->ms_sREPLY) == 0)
        return formatHelper(JulianFormatString::Instance()->ms_sVFreebusyReplyMessage, sName);
    else
        return toICALString();
}
//---------------------------------------------------------------------
UnicodeString VFreebusy::toString()
{
    return ICalComponent::toStringFmt(
        JulianFormatString::Instance()->ms_VFreebusyStrDefaultFmt);
}
//---------------------------------------------------------------------
UnicodeString VFreebusy::formatHelper(UnicodeString & strFmt, UnicodeString sFilterAttendee)
{
    UnicodeString u = JulianKeyword::Instance()->ms_sVFREEBUSY;
    return ICalComponent::format(u, strFmt, sFilterAttendee);
}
//---------------------------------------------------------------------
UnicodeString 
VFreebusy::formatChar(t_int32 c, UnicodeString sFilterAttendee,
                                t_bool delegateRequest)
{
    UnicodeString sResult, s;
    t_int32 i;
    JulianPtrArray * v;

    // NOTE: remove later, kept in to remove compiler warning
    if (delegateRequest)
    {
    }

    switch (c)
    {    
    case ms_cAttendees:    
        {    
            v = getAttendees();
            if (v != 0)
            {
                if (sFilterAttendee.size() > 0) 
                {
                    Attendee * a = getAttendee(sFilterAttendee);
                    if (a != 0)
                        s = a->toICALString(s);
                    else
                        s = "";
                    sResult += ICalProperty::multiLineFormat(s);
                }
                else 
                {
                    for (i=0; i< v->GetSize(); i++) 
                    {
                        s = ((Attendee * ) v->GetAt(i))->toICALString(s);
                        sResult += ICalProperty::multiLineFormat(s);
                    }
                }    
            }
            return sResult;    
        }
    case ms_cFreebusy:
        s = JulianKeyword::Instance()->ms_sFREEBUSY;
        return ICalProperty::propertyVectorToICALString(s, getFreebusy(), sResult);
    case ms_cComment: 
        s = JulianKeyword::Instance()->ms_sCOMMENT;
        return ICalProperty::propertyVectorToICALString(s, getComment(), sResult);     
    case ms_cCreated: 
        s = JulianKeyword::Instance()->ms_sCREATED;
        return ICalProperty::propertyToICALString(s, getCreatedProperty(), sResult);  
    case ms_cDTEnd:
        s = JulianKeyword::Instance()->ms_sDTEND;
        return ICalProperty::propertyToICALString(s, getDTEndProperty(), sResult);
    case ms_cDuration:
        s = JulianKeyword::Instance()->ms_sDURATION;
        return ICalProperty::propertyToICALString(s, getDurationProperty(), sResult);
    case ms_cDTStart: 
        s = JulianKeyword::Instance()->ms_sDTSTART;
        return ICalProperty::propertyToICALString(s, getDTStartProperty(), sResult);  
    case ms_cDTStamp:
        s = JulianKeyword::Instance()->ms_sDTSTAMP;
        return ICalProperty::propertyToICALString(s, getDTStampProperty(), sResult);  
    //case ms_cRelatedTo:
    //    s = JulianKeyword::Instance()->ms_sRELATEDTO;
    //    return ICalProperty::propertyVectorToICALString(s, getRelatedTo(), sResult);
    case ms_cOrganizer:
        s = JulianKeyword::Instance()->ms_sORGANIZER;
        return ICalProperty::propertyToICALString(s, getOrganizerProperty(), sResult);
    case ms_cRequestStatus:
        s = JulianKeyword::Instance()->ms_sREQUESTSTATUS;
        return ICalProperty::propertyToICALString(s, getRequestStatusProperty(), sResult);
    case ms_cSequence:
        s = JulianKeyword::Instance()->ms_sSEQUENCE;
        return ICalProperty::propertyToICALString(s, getSequenceProperty(), sResult);
    case ms_cUID:
        s = JulianKeyword::Instance()->ms_sUID;
        return ICalProperty::propertyToICALString(s, getUIDProperty(), sResult);
    case ms_cURL: 
        s = JulianKeyword::Instance()->ms_sURL;
        //return ICalProperty::propertyVectorToICALString(s, getURL(), sResult);
        return ICalProperty::propertyToICALString(s, getURLProperty(), sResult);
    case ms_cXTokens: 
        return ICalProperty::vectorToICALString(getXTokens(), sResult);
    default:
        {
          //DebugMsg.Instance().println(0,"No such member in VFreebusy");
          //sResult = ParserUtil.multiLineFormat(sResult);
          return "";
        }   
    }
}
//---------------------------------------------------------------------
UnicodeString VFreebusy::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
   
    UnicodeString sResult;

    switch(c)
    {
    case ms_cAttendees:
        return ICalProperty::propertyVectorToString(getAttendees(), dateFmt, sResult);
    case ms_cCreated:
        return ICalProperty::propertyToString(getCreatedProperty(), dateFmt, sResult);
    case ms_cDuration:
        return ICalProperty::propertyToString(getDurationProperty(), dateFmt, sResult);
    case ms_cDTEnd:
        return ICalProperty::propertyToString(getDTEndProperty(), dateFmt, sResult);
    case ms_cDTStart:
        return ICalProperty::propertyToString(getDTStartProperty(), dateFmt, sResult); 
    case ms_cDTStamp:
        return ICalProperty::propertyToString(getDTStampProperty(), dateFmt, sResult); 
    case ms_cLastModified:
        return ICalProperty::propertyToString(getLastModifiedProperty(), dateFmt, sResult); 
    case ms_cOrganizer:
        return ICalProperty::propertyToString(getOrganizerProperty(), dateFmt, sResult); 
    case ms_cRequestStatus:
        return ICalProperty::propertyToString(getRequestStatusProperty(), dateFmt, sResult); 
    case ms_cSequence:
        return ICalProperty::propertyToString(getSequenceProperty(), dateFmt, sResult); 
    case ms_cUID:
        return ICalProperty::propertyToString(getUIDProperty(), dateFmt, sResult);
    case ms_cURL:
        return ICalProperty::propertyToString(getURLProperty(), dateFmt, sResult);
    case ms_cFreebusy:
        return ICalProperty::propertyVectorToString(getFreebusy(), dateFmt, sResult);
    default:
        {
            return "";
        }   
    }
}
//---------------------------------------------------------------------
t_bool VFreebusy::MatchUID_seqNO(UnicodeString sUID, t_int32 iSeqNo)
{
    //t_int32 seq = getSequence();
    //UnicodeString uid = getUID();

    if (getSequence() == iSeqNo && getUID().compareIgnoreCase(sUID) == 0)
      return TRUE;
    else
      return FALSE;
}

//---------------------------------------------------------------------
void
VFreebusy::setAttendeeStatus(UnicodeString & sAttendeeFilter,
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

Attendee *
VFreebusy::getAttendee(UnicodeString sAttendee)
{
    return Attendee::getAttendee(getAttendees(), sAttendee);
}

//---------------------------------------------------------------------

int
VFreebusy::CompareVFreebusyByUID(const void * a,
                                  const void * b)
{
    PR_ASSERT(a != 0 && b != 0);
    VFreebusy * ta = *(VFreebusy **) a;
    VFreebusy * tb = *(VFreebusy **) b;
    
    return (int) ta->getUID().compare(tb->getUID());
}

//---------------------------------------------------------------------

int
VFreebusy::CompareVFreebusyByDTStart(const void * a,
                                      const void * b)
{
    PR_ASSERT(a != 0 && b != 0);
    VFreebusy * ta = *(VFreebusy **) a;
    VFreebusy * tb = *(VFreebusy **) b;

    DateTime da, db;
    da = ta->getDTStart();
    db = tb->getDTStart();

    return (int) (da.compareTo(db));
}

//---------------------------------------------------------------------

void VFreebusy::removeAllFreebusy()
{
    if (m_FreebusyVctr != 0) 
    { 
        Freebusy::deleteFreebusyVector(m_FreebusyVctr);
        delete m_FreebusyVctr; m_FreebusyVctr = 0; 
    }
}

//---------------------------------------------------------------------
// GETTERS AND SETTERS here
//---------------------------------------------------------------------
// attendee
void VFreebusy::addAttendee(Attendee * a)
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
// freebusy
void VFreebusy::addFreebusy(Freebusy * f)    
{ 
    if (m_FreebusyVctr == 0)
        m_FreebusyVctr = new JulianPtrArray(); 
    PR_ASSERT(m_FreebusyVctr != 0);
    if (m_FreebusyVctr != 0)
    {
        m_FreebusyVctr->Add(f);
    }
}
//---------------------------------------------------------------------
///DTEnd
void VFreebusy::setDTEnd(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_DTEnd == 0)
        m_DTEnd = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_DTEnd->setValue((void *) &s);
        m_DTEnd->setParameters(parameters);
    }
}
DateTime VFreebusy::getDTEnd() const 
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
void VFreebusy::setDuration(Duration s, JulianPtrArray * parameters)
{ 
    if (m_Duration == 0)
        m_Duration = ICalPropertyFactory::Make(ICalProperty::DURATION, 
                                            (void *) &s, parameters);
    else
    {
        m_Duration->setValue((void *) &s);
        m_Duration->setParameters(parameters);
    }
}
Duration VFreebusy::getDuration() const 
{
    Duration d; d.setMonth(-1);
    if (m_Duration == 0)
        return d; // return 0;
    else
    {
        d = *((Duration *) m_Duration->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
//LAST-MODIFIED
void VFreebusy::setLastModified(DateTime s, JulianPtrArray * parameters)
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

DateTime VFreebusy::getLastModified() const
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
void VFreebusy::setCreated(DateTime s, JulianPtrArray * parameters)
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
DateTime VFreebusy::getCreated() const
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
//DTStamp
void VFreebusy::setDTStamp(DateTime s, JulianPtrArray * parameters)
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
DateTime VFreebusy::getDTStamp() const
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
void VFreebusy::setDTStart(DateTime s, JulianPtrArray * parameters)
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

DateTime VFreebusy::getDTStart() const
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
//Organizer
void VFreebusy::setOrganizer(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);
    
    if (m_Organizer == 0)
    {
        //m_Organizer = ICalPropertyFactory::Make(ICalProperty::TEXT, 
        //                                    (void *) &s, parameters);

        m_Organizer = (ICalProperty *) new JulianOrganizer(m_Log);
        PR_ASSERT(m_Organizer != 0);
        m_Organizer->setValue((void *) &s);
        m_Organizer->setParameters(parameters);        
    }
    else
    {
        m_Organizer->setValue((void *) &s);
        m_Organizer->setParameters(parameters);
    }
}
UnicodeString VFreebusy::getOrganizer() const 
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
//RequestStatus
void VFreebusy::setRequestStatus(UnicodeString s, JulianPtrArray * parameters)
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
UnicodeString VFreebusy::getRequestStatus() const 
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
void VFreebusy::setUID(UnicodeString s, JulianPtrArray * parameters)
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
UnicodeString VFreebusy::getUID() const 
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
void VFreebusy::setSequence(t_int32 i, JulianPtrArray * parameters)
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
t_int32 VFreebusy::getSequence() const 
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
void VFreebusy::addComment(UnicodeString s, JulianPtrArray * parameters)
{
    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
            (void *) &s, parameters);
    addCommentProperty(prop);
}
void VFreebusy::addCommentProperty(ICalProperty * prop)
{
    if (m_CommentVctr == 0)
        m_CommentVctr = new JulianPtrArray();    
    PR_ASSERT(m_CommentVctr != 0);
    if (m_CommentVctr != 0)
    {
        m_CommentVctr->Add(prop);
    }
}
//---------------------------------------------------------------------
// RelatedTo
//void VFreebusy::addRelatedTo(UnicodeString s, JulianPtrArray * parameters)
//{
//    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
//            (void *) &s, parameters);
//    addRelatedToProperty(prop);
//} 
//void VFreebusy::addRelatedToProperty(ICalProperty * prop)      
//{ 
//    if (m_RelatedToVctr == 0)
//        m_RelatedToVctr = new JulianPtrArray(); 
//    PR_ASSERT(m_RelatedToVctr != 0);
//    m_RelatedToVctr->Add(prop);
//}
//---------------------------------------------------------------------
// URL
//void VFreebusy::addURL(UnicodeString s, JulianPtrArray * parameters)
//{
//    ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
//            (void *) &s, parameters);
//    addURLProperty(prop);
//}
//void VFreebusy::addURLProperty(ICalProperty * prop)    
//{ 
//    if (m_URLVctr == 0)
//        m_URLVctr = new JulianPtrArray(); 
//    PR_ASSERT(m_URLVctr != 0);
//    m_URLVctr->Add(prop);
//}
void VFreebusy::setURL(UnicodeString s, JulianPtrArray * parameters)
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
UnicodeString VFreebusy::getURL() const 
{
    UnicodeString u;
    if (m_URL == 0)
        return "";
    else {
        u = *((UnicodeString *) m_URL->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
// XTOKENS
void VFreebusy::addXTokens(UnicodeString s)         
{
    if (m_XTokensVctr == 0)
        m_XTokensVctr = new JulianPtrArray(); 
    PR_ASSERT(m_XTokensVctr != 0);
    if (m_XTokensVctr != 0)
    {
        m_XTokensVctr->Add(new UnicodeString(s));
    }
}
//---------------------------------------------------------------------

