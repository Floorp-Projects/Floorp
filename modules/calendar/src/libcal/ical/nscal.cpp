/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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

// nscal.h
// John Sun
// 1:35 PM Febuary 18 1998

#include "stdafx.h"
#include "jdefines.h"

#include "nscal.h"
#include "prprty.h"
#include "prprtyfy.h"
#include "icalcomp.h"
#include "icompfy.h"
#include "icalredr.h"
#include "vevent.h"
#include "vtodo.h"
#include "vjournal.h"
#include "vfrbsy.h"
#include "vtimezne.h"
#include "jlog.h"
#include "jutility.h"
#include "keyword.h"

#include "uidrgntr.h"
#include "datetime.h"
#include "period.h"
#include "recid.h"

/** to work on UNIX */
//#define TRACE printf

//---------------------------------------------------------------------
t_bool NSCalendar::m_bLoadMultipleCalendars = FALSE;
t_bool NSCalendar::m_bSmartLoading = TRUE;
//---------------------------------------------------------------------

// never use
#if 0
NSCalendar::NSCalendar()
{
    PR_ASSERT(FALSE);
}
#endif

//---------------------------------------------------------------------

NSCalendar::NSCalendar(JLog * initLog)
: m_VJournalVctr(0), m_VEventVctr(0), m_VTodoVctr(0), 
  m_VTimeZoneVctr(0), m_VFreebusyVctr(0), m_XTokensVctr(0),
  m_CalScale(0), 
#if 0
  m_Source(0), m_Name(0), 
#endif
  m_Version(0),
  m_Prodid(0), m_iMethod(0)
{
    //PR_ASSERT(m_Log != 0);
    m_Log = initLog;
}
//---------------------------------------------------------------------
NSCalendar::NSCalendar(NSCalendar & that)
: m_VJournalVctr(0), m_VEventVctr(0), m_VTodoVctr(0),
  m_VTimeZoneVctr(0), m_VFreebusyVctr(0), m_XTokensVctr(0),
#if 0
  m_Source(0), m_Name(0),
#endif
  m_CalScale(0), m_Version(0), m_Prodid(0)
{
    m_iMethod = that.m_iMethod;
    // copy component vectors
    if (that.m_VJournalVctr != 0)
    {
        m_VJournalVctr = new JulianPtrArray(); PR_ASSERT(m_VJournalVctr != 0);
        if (m_VJournalVctr != 0)
        {
            ICalComponent::cloneICalComponentVector(m_VJournalVctr, that.m_VJournalVctr);
        }
    }
    if (that.m_VEventVctr != 0)
    {
        m_VEventVctr = new JulianPtrArray(); PR_ASSERT(m_VEventVctr != 0);
        if (m_VEventVctr != 0)
        {
            ICalComponent::cloneICalComponentVector(m_VEventVctr, that.m_VEventVctr);
        }
    }
    if (that.m_VTodoVctr != 0)
    {
        m_VTodoVctr = new JulianPtrArray(); PR_ASSERT(m_VTodoVctr != 0);
        if (m_VTodoVctr != 0)
        {
            ICalComponent::cloneICalComponentVector(m_VTodoVctr, that.m_VTodoVctr);
        }
    }
    if (that.m_VTimeZoneVctr != 0)
    {
        m_VTimeZoneVctr = new JulianPtrArray(); PR_ASSERT(m_VTimeZoneVctr != 0);
        if (m_VTimeZoneVctr != 0)
        {
            ICalComponent::cloneICalComponentVector(m_VTimeZoneVctr, that.m_VTimeZoneVctr);
        }
    }
    if (that.m_VFreebusyVctr != 0)
    {
        m_VFreebusyVctr = new JulianPtrArray(); PR_ASSERT(m_VFreebusyVctr != 0);
        if (m_VFreebusyVctr != 0)
        {
            ICalComponent::cloneICalComponentVector(m_VFreebusyVctr, that.m_VFreebusyVctr);
        }
    }
    // clone xtokens
    if (that.m_XTokensVctr != 0)
    {
        m_XTokensVctr = new JulianPtrArray(); PR_ASSERT(m_XTokensVctr != 0);
        if (m_XTokensVctr != 0)
        {
            ICalProperty::CloneUnicodeStringVector(that.m_XTokensVctr, m_XTokensVctr);
        }
    }
    // clone ICalProperties.
    if (that.m_CalScale != 0) 
    { 
        m_CalScale = that.m_CalScale->clone(m_Log); 
    }
    if (that.m_Version != 0) 
    { 
        m_Version = that.m_Version->clone(m_Log); 
    }
    if (that.m_Prodid != 0) 
    { 
        m_Prodid = that.m_Prodid->clone(m_Log); 
    }
#if 0
    if (that.m_Source != 0) 
    { 
        m_Source = that.m_Source->clone(m_Log); 
    }
    if (that.m_Name != 0) 
    { 
        m_Name = that.m_Name->clone(m_Log); 
    }
#endif
    

}
//---------------------------------------------------------------------
NSCalendar::~NSCalendar()
{
    if (m_VEventVctr != 0) 
    {
        ICalComponent::deleteICalComponentVector(m_VEventVctr); 
        delete m_VEventVctr; m_VEventVctr = 0;
    }
    if (m_VFreebusyVctr != 0) 
    {
        ICalComponent::deleteICalComponentVector(m_VFreebusyVctr); 
        delete m_VFreebusyVctr; m_VFreebusyVctr = 0;
    }
    if (m_VJournalVctr != 0) 
    {
        ICalComponent::deleteICalComponentVector(m_VJournalVctr); 
        delete m_VJournalVctr; m_VJournalVctr = 0;
    }
    if (m_VTodoVctr!= 0) 
    {
        ICalComponent::deleteICalComponentVector(m_VTodoVctr); 
        delete m_VTodoVctr; m_VTodoVctr = 0;
    }
    if (m_VTimeZoneVctr!= 0) 
    {
        ICalComponent::deleteICalComponentVector(m_VTimeZoneVctr); 
        delete m_VTimeZoneVctr; m_VTimeZoneVctr = 0;
    }
    
    if (m_CalScale != 0) 
    { 
        delete m_CalScale; m_CalScale = 0; 
    }
    if (m_Prodid != 0) 
    { 
        delete m_Prodid; m_Prodid = 0; 
    }
    if (m_Version != 0) 
    { 
        delete m_Version; m_Version = 0; 
    }
#if 0
    if (m_Name != 0) 
    { 
        delete m_Name; m_Name = 0; 
    }
    if (m_Source != 0) 
    { 
        delete m_Source; m_Source = 0; 
    }
#endif

    //if (m_Method != 0) { delete m_Method; m_Method = 0; }
    
    if (m_XTokensVctr != 0) 
    { 
        ICalComponent::deleteUnicodeStringVector(m_XTokensVctr);
        delete m_XTokensVctr; m_XTokensVctr = 0; 
    }
}
//---------------------------------------------------------------------

/*
 *  Returns:
 *    TRUE if export worked
 *    FALSE if there was a problem
 */
void
NSCalendar::export(const char * filename, t_bool & status)
{
    FILE *f = fopen(filename, "w");
    if (f != 0)
    {
        char * ucc = 0;
#if 1
        UnicodeString sResult, out;
        status = TRUE;

        sResult = JulianKeyword::Instance()->ms_sBEGIN_VCALENDAR;
        sResult += JulianKeyword::Instance()->ms_sLINEBREAK;
        ucc = sResult.toCString("");
        if (ucc != 0)
        {
            fprintf(f, ucc);
            delete [] ucc; ucc = 0;
        }
        else 
            status = FALSE;

        // PRINT Calendar Properties
        sResult = createCalendarHeader(out);
        ucc = sResult.toCString("");
        if (ucc != 0)
        {
            fprintf(f, ucc);
            delete [] ucc; ucc = 0;
        }
        else 
            status = FALSE;
        
        // PRINT X-TOKENS
        sResult = ICalProperty::vectorToICALString(getXTokens(), out);
        ucc = sResult.toCString("");
        if (ucc != 0)
        {
            fprintf(f, ucc);
            delete [] ucc; ucc = 0;
        }
        else 
            status = FALSE;
        
        // PRINT Calendar Components
        status |= printComponentVectorToFile(getTimeZones(), f);
        status |= printComponentVectorToFile(getEvents(), f);
        status |= printComponentVectorToFile(getTodos(), f);
        status |= printComponentVectorToFile(getJournals(), f);
        status |= printComponentVectorToFile(getVFreebusy(), f);

        sResult = JulianKeyword::Instance()->ms_sEND_VCALENDAR;
        sResult += JulianKeyword::Instance()->ms_sLINEBREAK;
        ucc = sResult.toCString("");
        if (ucc != 0)
        {
            fprintf(f, ucc);
            delete [] ucc; ucc = 0;
        }
        else 
            status = FALSE;

#else
        char * ucc = toICALString().toCString("");
        if (ucc != 0)
        {
            size_t iLen = strlen(ucc);
            size_t iCountWritten = fwrite(ucc, iLen, 1, f);
            delete [] ucc; ucc = 0;
            status = (iCountWritten == 1) ? TRUE : FALSE;
        }
        else
        {
            status = FALSE;
        }
#endif
        int iRet = fclose(f);
        if (0 != iRet)
        {
            //int iError =errno;
            status = FALSE;
        }
    }
    else
    {
        status = FALSE;
    }
}

//---------------------------------------------------------------------

NSCalendar *
NSCalendar::clone(JLog * initLog)
{
    m_Log = initLog;
    return new NSCalendar(*this);
}

//---------------------------------------------------------------------

void 
NSCalendar::parse(ICalReader * brFile,
                  UnicodeString & fileName,
                  JulianUtility::MimeEncoding encoding)
{
    
    UnicodeString strLine, propName, propVal;
    
    JulianPtrArray * parameters = new JulianPtrArray();
    PR_ASSERT(parameters != 0 && brFile != 0);

    if (parameters == 0 || brFile == 0)
        return;

    t_bool bNextComponent = FALSE;

    ErrorCode status = ZERO_ERROR;
    UnicodeString sOK;
    UnicodeString u, sName;
    ICalComponent * ic;
    t_bool bUpdatedComponent;

    UnicodeString uid;
#if 0
    DateTime d;
    UnicodeString rid;
#endif
    if (fileName.size() > 0)
    {
        // TODO: set filename
    }
    while (TRUE)
    {
        PR_ASSERT(brFile != 0);
        brFile->readFullLine(strLine, status);
        ICalProperty::Trim(strLine);

        if (FAILURE(status) && strLine.size() == 0)
            break;

        ICalProperty::parsePropertyLine(strLine, propName, 
            propVal, parameters);
        
        if (strLine.size() == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            continue;
        }

        // break for "END:" line
        else if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
            (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0))
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            
            // if parsing of multiple calendars allowed, continue in loop.
            if (m_bLoadMultipleCalendars)
                continue;
            else 
                break;
        }
        else
        {
            if (propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0)
            {
                ICalProperty::deleteICalParameterVector(parameters);
                parameters->RemoveAll();
            
                bNextComponent = TRUE;
                sName = propVal;

                while (bNextComponent)
                {
                    // TODO: finish
                    ic = ICalComponentFactory::Make(sName, m_Log);
                    if (ic != 0)
                    {
                        if (m_Log) m_Log->addEventErrorEntry();
                        sOK = ic->parse(brFile, methodToString(getMethod(), u),
                            sOK, getTimeZones());

                        // TODO: finish smart, multiple handling
#if 0
                        if (m_bLoadMultipleCalendars)
                        {
                            // if multiple calendars can be loaded,
                            // then I could overwrite events no matter what (lazy) , or check
                            // if the sequence number is greater and then overwrite (smart)
                            // TODO: finish

                            JulianPtrArray * out = new JulianPtrArray();
                            PR_ASSERT(out != 0);

                            if (m_bSmartLoading)
                            {
                                // Smart multiple handling, overwrite only if component is old 
                                // TODO: make it getMatchingComponents (handle recid)
                                getComponentsWithType(out, ic->getUID(), ic->GetType());
                                if (out->GetSize() == 0)
                                {
                                    if (ic->isValid())
                                    {
                                        bUpdatedComponent = addComponentWithType(ic, ic->GetType()); 
                                        if (bUpdatedComponent)
                                        {
                                            delete ic; ic = 0;
                                        }
                                    }
                                    else
                                    {
                                        if (m_Log) m_Log->logError(
                                            JulianLogErrorMessage::Instance()->ms_iInvalidComponent, 300);
                                        delete ic; ic = 0;
                                    }
                                }
                                else
                                {
                                    if (ic->getSequence() > getMaximumSequence(out) && 
                                         (ic->isValid()))
                                    {
                                        removeComponents(out);
                                        bUpdatedComponent = addComponentWithType(ic, ic->GetType()); 
                                        if (bUpdatedComponent)
                                        {
                                            delete ic; ic = 0;
                                        }
                                    }
                                    else
                                    {
                                        if (m_Log) m_Log->logError(
                                            JulianLogErrorMessage::Instance()->ms_iInvalidComponent, 300);
                                        delete ic; ic = 0;
                                    }
                                }
                            }
                            else
                            {
                                // Lazy multiple handling, overwrite if component matches
                                // TODO: make it getMatchingComponents (handle recid)
                                getComponentsWithType(out, ic->getUID(), ic->GetType());
                                if (out->GetSize() == 0)
                                {
                                    if (ic->isValid())
                                    {
                                        bUpdatedComponent = addComponentWithType(ic, ic->GetType());
                                        if (bUpdatedComponent)
                                        {
                                            delete ic; ic = 0;
                                        }
                                    }
                                    else
                                    {
                                        if (m_Log) m_Log->logError(
                                            JulianLogErrorMessage::Instance()->ms_iInvalidComponent, 300);
                                        delete ic; ic = 0;
                                    }
                                }
                                else
                                {
                                    if (ic->isValid())
                                    {
                                        removeComponents(out);
                                        bUpdatedComponent = addComponentWithType(ic, ic->GetType());
                                        if (bUpdatedComponent)
                                        {
                                            delete ic; ic = 0;
                                        }
                                    }
                                    else
                                    {
                                        if (m_Log) m_Log->logError(
                                            JulianLogErrorMessage::Instance()->ms_iInvalidComponent, 300);
                                        delete ic; ic = 0;
                                    }
                                }
                            }
                            delete out; out = 0;
                        }
                        else 
                        {
                            // not multiple handling
                            if (ic->isValid())
                            {
                                bUpdatedComponent = addComponentWithType(ic, ic->GetType());
                                if (bUpdatedComponent)
                                {
                                    delete ic; ic = 0;
                                }
                                //addComponent(ic, sName);
                            }
                            else
                            {
                                if (m_Log) m_Log->logError(
                                    JulianLogErrorMessage::Instance()->ms_iInvalidComponent, 300);
                                delete ic; ic = 0;
                            }
                        }
#else // #if 0
               
                        //if (ic->isValid() && ic->getUID().size() > 0)
                        if (ic->isValid())
                        {
                            //addComponent(ic, sName);
                            bUpdatedComponent = addComponentWithType(ic, ic->GetType());                            
                            if (m_Log) 
                            {   
                                m_Log->setCurrentEventLogComponentType((JulianLogErrorVector::ECompType) ic->GetType());
                                m_Log->setCurrentEventLogValidity(TRUE);
                                uid = "";
#if 0
                                rid = "";
#endif
                                if (ic->GetType() == ICalComponent::ICAL_COMPONENT_VEVENT || 
                                    ic->GetType() == ICalComponent::ICAL_COMPONENT_VTODO || 
                                    ic->GetType() == ICalComponent::ICAL_COMPONENT_VJOURNAL)
                                {
                                    uid = ((TimeBasedEvent *) ic)->getUID();
#if 0
                                    d = ((TimeBasedEvent *) ic)->getRecurrenceID();
                                    if (d.isValid())
                                        rid = d.toISO8601();
#endif
                                }
                                else if (ic->GetType() == ICalComponent::ICAL_COMPONENT_VFREEBUSY)
                                {
                                    uid = ((VFreebusy *) ic)->getUID();
                                }
                                else if (ic->GetType() == ICalComponent::ICAL_COMPONENT_VTIMEZONE)
                                {
                                    uid = ((VTimeZone *) ic)->getTZID();
                                }
#if 0
                                m_Log->setUIDRecurrenceID(uid, rid);        
#else
                                m_Log->setUID(uid);
#endif
                            }
                            if (bUpdatedComponent)
                            {
                                delete ic; ic = 0;
                            }
                        }
                        else
                        {
                            if (m_Log) 
                            {
                                //m_Log->setCurrentEventLogComponentType((JulianLogErrorVector::ECompType) ic->GetType());
                                // make invalid event errors go to NSCALENDAR
                                m_Log->setCurrentEventLogComponentType(JulianLogErrorVector::ECompType_NSCALENDAR);
                                m_Log->setCurrentEventLogValidity(FALSE);
                                uid = "";
#if 0
                                rid = "";
#endif
                                if (ic->GetType() == ICalComponent::ICAL_COMPONENT_VEVENT || 
                                    ic->GetType() == ICalComponent::ICAL_COMPONENT_VTODO || 
                                    ic->GetType() == ICalComponent::ICAL_COMPONENT_VJOURNAL)
                                {
                                    uid = ((TimeBasedEvent *) ic)->getUID();
#if 0
                                    d = ((TimeBasedEvent *) ic)->getRecurrenceID();
                                    if (d.isValid())
                                        rid = d.toISO8601();
#endif
                                }
                                else if (ic->GetType() == ICalComponent::ICAL_COMPONENT_VFREEBUSY)
                                {
                                    uid = ((VFreebusy *) ic)->getUID();
                                }
                                else if (ic->GetType() == ICalComponent::ICAL_COMPONENT_VTIMEZONE)
                                {
                                    uid = ((VTimeZone *) ic)->getTZID();
                                }
#if 0
                                m_Log->setUIDRecurrenceID(uid, rid);                                
#else
                                m_Log->setUID(uid);
#endif
                            }
                            if (m_Log) m_Log->logError(
                                JulianLogErrorMessage::Instance()->ms_iInvalidComponent, 300);
#if 0
                            if (m_Log) 
                            {
                                // Print the REQUEST-STATUS of the deleted event.
                                if (ic->getRequestStatus() != 0)
                                {
                                    UnicodeString * uPtr;
                                    t_int32 ii;
                                    for (ii = 0; ii < ic->getRequestStatus()->GetSize(); ii++)
                                    {
                                        uPtr = (UnicodeString *) ic->getRequestStatus()->GetAt(ii);
                                        m_Log->logError(*uPtr);
                                    }
                                }
                            }
#endif // #if 0
                            delete ic; ic = 0;
                        }
#endif // #else
                        if (sOK.compareIgnoreCase(JulianKeyword::Instance()->ms_sOK) == 0)
                            bNextComponent = FALSE;
                        else 
                        {
                            if (sOK.startsWith("BEGIN:"))
                            {
                                bNextComponent = TRUE;
                                // TODO: i'm extracting after 6, that means I'm assuming "BEGIN:whatever"
                                sName = sOK.extractBetween(6, sOK.size(), sName);
                            }
                            else
                                bNextComponent = FALSE;
                        }
                    }
                    else
                        bNextComponent = FALSE;

                    //-----
                    if (ICalProperty::IsXToken(sName))
                    {
                        ICalProperty::deleteICalParameterVector(parameters);
                        parameters->RemoveAll();
                        if (m_Log) 
                        {
                            m_Log->addEventErrorEntry();
                            //m_Log->setCurrentEventLogComponentType(JulianLogErrorVector::ECompType_XCOMPONENT);
                            // make x-toekn errors go to NSCALENDAR
                            m_Log->setCurrentEventLogComponentType(JulianLogErrorVector::ECompType_NSCALENDAR);
                        }   
                        if (m_Log) m_Log->logError(
                            JulianLogErrorMessage::Instance()->ms_iXTokenComponentIgnored, 
                            JulianKeyword::Instance()->ms_sNSCALENDAR, sName, 200);

                        // Ignore all lines between BEGIN:X-COMPONENT and END:X-COMPONENT
                        while (TRUE)
                        {
                            brFile->readFullLine(strLine, status);
                            if (FAILURE(status) && strLine.size() == 0)
                                break;
                            ICalProperty::parsePropertyLine(strLine, propName, propVal, parameters);
                            ICalProperty::deleteICalParameterVector(parameters);
                            parameters->RemoveAll();
                            
                            if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
                                (propVal.compareIgnoreCase(sName) == 0))
                            {
                                // correct end of X-Component parsing
                                break;
                            }
                            else if (
                                ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) && 
                                (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0)) ||
                                
                                ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0) && 
                                ((propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVCALENDAR) == 0) ||
                                (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVEVENT) == 0) ||
                                (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTODO) == 0) ||
                                (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVJOURNAL) == 0) ||
                                (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVFREEBUSY) == 0) ||
                                (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0) ||
                                (ICalProperty::IsXToken(propVal)))))
                                
                            {
                                // incorrect end of X-Component parsing, log an error
                                // ends with END:VCALENDAR or BEGIN:VCALENDAR, VEVENT, VTODO, VJOURNAL, VFREEBUSY, VTIMEZONE 
                                // or BEGIN:X-token
                            
                                if (m_Log) m_Log->logError(
                                    JulianLogErrorMessage::Instance()->ms_iAbruptEndOfParsing, 
                                    JulianKeyword::Instance()->ms_sNSCALENDAR, strLine, 300);
                                sOK = propName;
                                sOK += JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
                                sOK += propVal;
                                bNextComponent = TRUE;
                                sName = propVal;
                                break;
                            }
                        }
                    }
                    //-----
                    //if (ic == 0 || (!bNextComponent))
                    if (ic == 0 && (!bNextComponent))
                    {
                        break;
                    }
                }
            }
            else 
            {
                if (m_Log) m_Log->setCurrentEventLogComponentType(JulianLogErrorVector::ECompType_NSCALENDAR);
                storeData(strLine, propName, propVal, parameters);

                ICalProperty::deleteICalParameterVector(parameters);
                parameters->RemoveAll();
            }
        }
    }
    ICalProperty::deleteICalParameterVector(parameters);
    parameters->RemoveAll();
    delete parameters; parameters = 0;
    expandAllComponents();
    //selfCheck();
}

//---------------------------------------------------------------------

t_bool NSCalendar::storeData(UnicodeString & strLine, UnicodeString & propName,
                             UnicodeString & propVal, JulianPtrArray * parameters)
{
    t_int32 i;
   
    t_int32 hashCode = propName.hashCode();
    if (JulianKeyword::Instance()->ms_ATOM_VERSION == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVCALENDAR, strLine, 100);
        }
        setVersion(propVal, parameters);       
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_PRODID == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVCALENDAR, strLine, 100);
        }
        setProdid(propVal, parameters);
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_CALSCALE == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVCALENDAR, strLine, 100);
        }
        // if propVal != CALSCALE, just log, 
        // will set to GREGORIAN later.
        if (JulianKeyword::Instance()->ms_ATOM_GREGORIAN != propVal.hashCode())
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidPropertyValue, 
                JulianKeyword::Instance()->ms_sVCALENDAR,
                propName, propVal, 200);
        }

        setCalScale(propVal, parameters);
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_METHOD == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVCALENDAR, strLine, 100);
        }
        i = stringToMethod(propVal);
        if (i < 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidPropertyValue, 
                JulianKeyword::Instance()->ms_sVCALENDAR,
                propName, propVal, 200);
        }
        else 
        {
            if (getMethod() >= 0)
            {
                if (m_Log) m_Log->logError(
                    JulianLogErrorMessage::Instance()->ms_iDuplicatedProperty, 
                    JulianKeyword::Instance()->ms_sVCALENDAR,
                    propName, propVal, 100);
            }
            setMethod((NSCalendar::METHOD) i);
        }
        return TRUE;
    }
    else if (ICalProperty::IsXToken(propName))
    {
        addXTokens(strLine);

        return TRUE;
    }
#if 0
    else if (JulianKeyword::Instance()->ms_ATOM_SOURCE == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {            
           if (m_Log) m_Log->logError(
               JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
               JulianKeyword::Instance()->ms_sVCALENDAR, strLine, 100);
        }
        setSource(propVal, parameters);
        return TRUE;
    }
    else if (JulianKeyword::Instance()->ms_ATOM_NAME == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logError(
                JulianLogErrorMessage::Instance()->ms_iInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVCALENDAR, strLine, 100);
        }
        setName(propVal, parameters);
        return TRUE;
    }
#endif
    else
    {
        if (m_Log) m_Log->logError(
            JulianLogErrorMessage::Instance()->ms_iInvalidPropertyName, 
            JulianKeyword::Instance()->ms_sVCALENDAR,
            propName, 200);

        return FALSE;
    }
}
//---------------------------------------------------------------------

void
NSCalendar::expandAllComponents()
{
    JulianPtrArray * v = 0;
    v = getEvents();
    expandTBEVector(v, ICalComponent::ICAL_COMPONENT_VEVENT);

    v = getTodos();
    expandTBEVector(v, ICalComponent::ICAL_COMPONENT_VTODO);

    v = getJournals();
    expandTBEVector(v, ICalComponent::ICAL_COMPONENT_VJOURNAL);
}

//---------------------------------------------------------------------

void 
NSCalendar::expandTBEVector(JulianPtrArray * v, 
                            ICalComponent::ICAL_COMPONENT type)
{
    // no need to expand if method is a decline-counter or refresh
    if (v != 0 && (m_iMethod != METHOD_DECLINECOUNTER && m_iMethod != METHOD_REFRESH))
    {
        t_int32 i;
        TimeBasedEvent * tbe = 0;
        for (i = v->GetSize() - 1; i >= 0; i--)
        {
            tbe = (TimeBasedEvent *) v->GetAt(i);
            if (tbe->isExpandableEvent())
            {
                expandComponent(tbe, type);

                // remove old event
                v->RemoveAt(i);
                delete tbe; tbe = 0;
            }
        }
    }
}

//---------------------------------------------------------------------

void 
NSCalendar::expandComponent(TimeBasedEvent * tbe,
                            ICalComponent::ICAL_COMPONENT type)
{
    JulianPtrArray * v = new JulianPtrArray(); PR_ASSERT(v != 0);
    if (v != 0)
    {
        // create the components and fill them in v, passing in timezone vector
        tbe->createRecurrenceEvents(v, getTimeZones());
    
        // add each component to correct vector.
        if (v->GetSize() > 0)
        {
            //UnicodeString uDebug;
            //if (FALSE) TRACE("Expandable event\r\n\t%s\n\tbeing expanded", tbe->toString(uDebug).toCString(""));
            t_int32 i;
            ICalComponent * ic = 0;
            t_bool bUpdatedComponent = FALSE;
            for (i = 0; i < v->GetSize(); i++)
            {
                ic = (ICalComponent *) v->GetAt(i);
                bUpdatedComponent = addComponentWithType(ic, type);
                if (bUpdatedComponent)
                {
                    delete ic; ic = 0;
                }
            }
        }   
        delete v; v = 0;
    }
}

//---------------------------------------------------------------------

JulianPtrArray * 
NSCalendar::getLogVector(ICalComponent * ic)
{
    if (m_Log && ic)
    {
        JulianPtrArray * vvError = 0;
        vvError = m_Log->GetErrorVector();
        if (vvError != 0)
        {
            t_int32 i = 0;
            JulianLogErrorVector * lev = 0;
            UnicodeString uid;
#if 0
            UnicodeString rid;
#endif
            ICalComponent::ICAL_COMPONENT icType;
            icType = ic->GetType();
            if (icType == ICalComponent::ICAL_COMPONENT_VEVENT || 
                icType == ICalComponent::ICAL_COMPONENT_VTODO ||
                icType == ICalComponent::ICAL_COMPONENT_VJOURNAL)
            {
#if 0
                DateTime d;
                d = ((TimeBasedEvent *) ic)->getRecurrenceID();
                if (d.isValid())
                    rid = d.toISO8601();
#endif
                uid = ((TimeBasedEvent *) ic)->getUID();
            }
            else if (icType == ICalComponent::ICAL_COMPONENT_VFREEBUSY)
            {
                uid = ((VFreebusy *) ic)->getUID();
            }
            else if (icType == ICalComponent::ICAL_COMPONENT_VTIMEZONE)
            {
                uid = ((VTimeZone *) ic)->getTZID();
            }

            for (i = 0; i < vvError->GetSize(); i++)
            {
                lev = (JulianLogErrorVector *) vvError->GetAt(i);
                if ((lev->GetComponentType() == (JulianLogErrorVector::ECompType) ic->GetType()) &&
                    (lev->GetUID() == uid) 
#if 0
                    && (lev->GetRecurrenceID() == rid)
#endif
                    )
                    return (lev->GetErrors());
            }
        }
    }
    return 0;
}

//---------------------------------------------------------------------

JulianPtrArray *
NSCalendar::getLogVector()
{
    if (m_Log)
    {
        JulianPtrArray * vvError = 0;
        vvError = m_Log->GetErrorVector();
        if (vvError != 0)
        {
            t_int32 i = 0;
            JulianLogErrorVector * lev = 0;
            for (i = 0; i < vvError->GetSize(); i++)
            {
                lev = (JulianLogErrorVector *) vvError->GetAt(i);
                if (lev->GetComponentType() == JulianLogErrorVector::ECompType_NSCALENDAR)
                    return (lev->GetErrors());
            }
        }
    }
    return 0;
}

//---------------------------------------------------------------------

UnicodeString NSCalendar::toString()
{
    UnicodeString sResult, s, out;
    s = JulianKeyword::Instance()->ms_sPRODID;
    sResult += ICalProperty::propertyToICALString(s, getProdidProperty(), out);
    s = JulianKeyword::Instance()->ms_sCALSCALE;
    sResult += ICalProperty::propertyToICALString(s, getCalScaleProperty(), out);
    s = JulianKeyword::Instance()->ms_sVERSION;
    sResult += ICalProperty::propertyToICALString(s, getVersionProperty(), out);
#if 0
    s = JulianKeyword::Instance()->ms_sNAME;
    sResult += ICalProperty::propertyToICALString(s, getNameProperty(), out);
    s = JulianKeyword::Instance()->ms_sSOURCE;
    sResult += ICalProperty::propertyToICALString(s, getSourceProperty(), out);
#endif
    // print method
    sResult += JulianKeyword::Instance()->ms_sMETHOD;
    sResult += JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
    sResult += methodToString(getMethod(), out); 

    sResult += JulianKeyword::Instance()->ms_sLINEBREAK;
    //sResult += ICalProperty::propertyToICALString(s, getMethodProperty(), sResult);

    return sResult;
}

//---------------------------------------------------------------------

UnicodeString & 
NSCalendar::createCalendarHeader(UnicodeString & sResult)
{
    UnicodeString s, out;
    sResult = "";
    s = JulianKeyword::Instance()->ms_sPRODID;
    sResult += ICalProperty::propertyToICALString(s, getProdidProperty(), out);
    s = JulianKeyword::Instance()->ms_sCALSCALE;
    sResult += ICalProperty::propertyToICALString(s, getCalScaleProperty(), out);
    s = JulianKeyword::Instance()->ms_sVERSION;
    sResult += ICalProperty::propertyToICALString(s, getVersionProperty(), out);
#if 0
    s = JulianKeyword::Instance()->ms_sNAME;
    sResult += ICalProperty::propertyToICALString(s, getNameProperty(), out);
    s = JulianKeyword::Instance()->ms_sSOURCE;
    sResult += ICalProperty::propertyToICALString(s, getSourceProperty(), out);
#endif    
    // print method enum
    sResult += JulianKeyword::Instance()->ms_sMETHOD; 
    sResult += JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
    sResult += methodToString(getMethod(), out); 
    sResult += JulianKeyword::Instance()->ms_sLINEBREAK;
    //sResult += ICalProperty::propertyToICALString(s, getMethodProperty(), out);
    
    return sResult;
}

//---------------------------------------------------------------------

UnicodeString NSCalendar::toICALString()
{
    UnicodeString sResult, out;
    sResult = JulianKeyword::Instance()->ms_sBEGIN_VCALENDAR;
    sResult += JulianKeyword::Instance()->ms_sLINEBREAK;
    sResult += createCalendarHeader(out);
    sResult += ICalProperty::vectorToICALString(getXTokens(), out);
    //if (FALSE) TRACE("out = %s", out.toCString(""));

    // PRINT Calendar Components
    sResult += printComponentVector(getTimeZones(), out);
    sResult += printComponentVector(getEvents(), out);
    sResult += printComponentVector(getVFreebusy(), out);
    sResult += printComponentVector(getTodos(), out);
    sResult += printComponentVector(getJournals(), out);
    sResult += JulianKeyword::Instance()->ms_sEND_VCALENDAR;
    sResult += JulianKeyword::Instance()->ms_sLINEBREAK;
    return sResult;
}
//---------------------------------------------------------------------
VEvent * NSCalendar::getEvent(UnicodeString sUID, t_int32 iSeqNo)
{
    return (VEvent *) NSCalendar::getComponent(getEvents(), sUID, iSeqNo);
}
//--------------------------------------------------------------------- 
VTodo * NSCalendar::getTodo(UnicodeString sUID, t_int32 iSeqNo)
{
    return (VTodo *) NSCalendar::getComponent(getTodos(), sUID, iSeqNo);
}
//---------------------------------------------------------------------
VJournal * NSCalendar::getJournal(UnicodeString sUID, t_int32 iSeqNo)
{
    return (VJournal *) NSCalendar::getComponent(getJournals(), sUID, iSeqNo);
}
//---------------------------------------------------------------------
VFreebusy * NSCalendar::getVFreebusy(UnicodeString sUID, t_int32 iSeqNo)
{
    return (VFreebusy *) NSCalendar::getComponent(getVFreebusy(), sUID, iSeqNo);
}
//---------------------------------------------------------------------

ICalComponent * 
NSCalendar::getComponent(JulianPtrArray * vTBE,
                         UnicodeString & sUID, t_int32 iSeqNo) 
                              
{
    ICalComponent * e = 0;
    t_int32 i;
    if (vTBE != 0 && sUID.size() > 0) 
    {
      if (iSeqNo != -1) {
        
          for (i = 0; i < vTBE->GetSize(); i++)
          {
              e = (ICalComponent *) vTBE->GetAt(i);
              if (e->MatchUID_seqNO(sUID, iSeqNo))
                return e;
          }
      }
      else
      {
          for (i = 0; i < vTBE->GetSize(); i++)
          {
              e = (ICalComponent *) vTBE->GetAt(i);
              if (e->getUID().compareIgnoreCase(sUID) == 0)
                  return e;
          }
      }
    }
    return 0;
}
//---------------------------------------------------------------------
JulianPtrArray * NSCalendar::changeEventsOwnership()
{
    JulianPtrArray * out = m_VEventVctr;
    m_VEventVctr = 0;
    return out;
}
//---------------------------------------------------------------------
void NSCalendar::updateEventsRange(VEvent * v)
{
    DateTime start, end;
    PR_ASSERT(v->isValid());
    start = v->getDTStart();
    end = v->getDTEnd();
    if (start.isValid())
    {
         if (!m_EventsSpanStart.isValid()) 
             m_EventsSpanStart = start;
         else
         {
             if (m_EventsSpanStart.afterDateTime(start))
                 m_EventsSpanStart = start;
         }
         // set m_EventsSpanEnd to start if end not valid and m_EventsSpanNotSet
         if (!m_EventsSpanEnd.isValid() && (!end.isValid()))
         {
             m_EventsSpanEnd = start;
         }
    }
    if (end.isValid())
    {
         if (!m_EventsSpanEnd.isValid())
             m_EventsSpanEnd = end;
         else
         {
             if (m_EventsSpanEnd.beforeDateTime(end))
                  m_EventsSpanEnd = end;
         }
    }
}
//---------------------------------------------------------------------
void NSCalendar::addEvent(ICalComponent * v)
{
    if (m_VEventVctr == 0)
        m_VEventVctr = new JulianPtrArray();
    PR_ASSERT(m_VEventVctr != 0);
    if (m_VEventVctr != 0)
    {
        PR_ASSERT(ICalComponent::ICAL_COMPONENT_VEVENT == v->GetType());
        updateEventsRange((VEvent *) v);
        m_VEventVctr->Add(v);
        //m_VEventVctr->InsertBinary(v, TimeBasedEvent::CompareTimeBasedEventsByDTStart);
    }
}
//---------------------------------------------------------------------
void NSCalendar::addTodo(ICalComponent * v)
{
    if (m_VTodoVctr == 0)
        m_VTodoVctr = new JulianPtrArray();
    PR_ASSERT(m_VTodoVctr != 0);
    if (m_VTodoVctr != 0)
    {
        m_VTodoVctr->Add(v);
        //m_VTodoVctr->InsertBinary(v, TimeBasedEvent::CompareTimeBasedEventsByDTStart);
    }
}
//---------------------------------------------------------------------
void NSCalendar::addJournal(ICalComponent * v)
{
    if (m_VJournalVctr == 0)
        m_VJournalVctr = new JulianPtrArray();
    PR_ASSERT(m_VJournalVctr != 0);
    if (m_VJournalVctr != 0)
    {
        m_VJournalVctr->Add(v);
        //m_VJournalVctr->InsertBinary(v, TimeBasedEvent::CompareTimeBasedEventsByDTStart);
    }
}
//---------------------------------------------------------------------
void NSCalendar::addVFreebusy(ICalComponent * v)
{
    if (m_VFreebusyVctr == 0)
        m_VFreebusyVctr = new JulianPtrArray();
    PR_ASSERT(m_VFreebusyVctr != 0);
    if (m_VFreebusyVctr != 0)
    {
        m_VFreebusyVctr->Add(v);
        //m_VFreebusyVctr->InsertBinary(v, VFreebusy::CompareVFreebusyByDTStart);
    }
}
//---------------------------------------------------------------------
void NSCalendar::addTimeZone(ICalComponent * v)
{
    if (m_VTimeZoneVctr == 0)
        m_VTimeZoneVctr = new JulianPtrArray();
    PR_ASSERT(m_VTimeZoneVctr != 0);
    if (m_VTimeZoneVctr != 0)
    {
        m_VTimeZoneVctr->Add(v);
    }
}
//---------------------------------------------------------------------

t_bool NSCalendar::addComponentWithType(ICalComponent * ic, 
                                      ICalComponent::ICAL_COMPONENT type)
{

    /////////////
    // if already have component, just update it
    JulianPtrArray * v = 0;
    t_bool bUpdatedAnyComponents = FALSE;

    switch(ic->GetType())
    {
    case ICalComponent::ICAL_COMPONENT_VEVENT:
        v = m_VEventVctr;
        break;
    case ICalComponent::ICAL_COMPONENT_VTODO:
        v = m_VTodoVctr;
        break;
    case ICalComponent::ICAL_COMPONENT_VJOURNAL:
        v = m_VJournalVctr;
        break;
    case ICalComponent::ICAL_COMPONENT_VTIMEZONE:
        v = m_VTimeZoneVctr;
        break;
    case ICalComponent::ICAL_COMPONENT_VFREEBUSY:
        v = m_VFreebusyVctr;
        break;
    default:
        v = 0;
    }
    if (v != 0)
    {
        t_bool bUpdated = FALSE;
        t_int32 i;
        ICalComponent * comp = 0;
        for (i = 0; i < v->GetSize(); i++)
        {
            comp = (ICalComponent *) v->GetAt(i);
            bUpdated = comp->updateComponent(ic);
            bUpdatedAnyComponents |= bUpdated;
        }
    }

    if (!bUpdatedAnyComponents)
    {
        // Add only if not updated.
        switch (type)
        {             
        case ICalComponent::ICAL_COMPONENT_VEVENT:
            addEvent(ic);
            break;
        case ICalComponent::ICAL_COMPONENT_VTODO:
            addTodo(ic);
            break;
        case ICalComponent::ICAL_COMPONENT_VJOURNAL:
            addJournal(ic);
            break;
        case ICalComponent::ICAL_COMPONENT_VTIMEZONE:
            addTimeZone(ic);
            break;
        case ICalComponent::ICAL_COMPONENT_VFREEBUSY:
            addVFreebusy(ic);
            break; 
        default:
            break;
        }
    }

    return bUpdatedAnyComponents;
}

//---------------------------------------------------------------------
#if 0
void NSCalendar::addComponentsWithType(JulianPtrArray * components,
                                       ICalComponent::ICAL_COMPONENT type)
{
    t_int32 i;
    PR_ASSERT(components != 0);
    if (components != 0)
    {
        for (i = 0; i < components->GetSize(); i++)
        {
            ICalComponent * ic = (ICalComponent *) components->GetAt(i);
            addComponentWithType(ic, type);
        }
    }
}
#endif
//---------------------------------------------------------------------

void
NSCalendar::createVFreebusyHelper(Freebusy * f, DateTime start, 
                                  DateTime end)
{
    if (f != 0) 
    {       
        t_int32 i;
        
        DateTime startP;
    
        DateTime endP;
    
        Period period;
        period.setStart(start);
        period.setEnd(end);

        VEvent * ev;
    
        Period * p;

        f->setType(Freebusy::FB_TYPE_BUSY);
#if 0
        f->setStatus(Freebusy::FB_STATUS_BUSY);
#endif
        JulianPtrArray * v = getEvents();

        if (v != 0)
        {    
            for (i = 0; i < v->GetSize(); i++)
            {
                ev = (VEvent *) v->GetAt(i);

                // must be opaque
                if (ev->getTransp() == JulianKeyword::Instance()->ms_sOPAQUE)
                {
                    startP = ev->getDTStart();
                    endP = ((ev->isAllDayEvent()) ? start : ev->getDTEnd());
    
                    if (period.isIntersecting(startP, endP))
                    {
                        startP = ((start > startP) ? start : startP);
                    
                        endP = ((end < endP) ? end : endP);
            
                        p = new Period(); PR_ASSERT(p != 0);
                        if (p != 0)
                        {
                            p->setStart(startP);
                            p->setEnd(endP); 
                            f->addPeriod(p);
                            delete p; p = 0;
                        }   
                    }
                }
            }
        }
    } 
}

//---------------------------------------------------------------------

VFreebusy * 
NSCalendar::createVFreebusy(DateTime start, DateTime end)
{

    // set freebusy type and status to busy
    Freebusy * f = new Freebusy(m_Log); PR_ASSERT(f != 0);
    createVFreebusyHelper(f, start, end);

    // create the vfreebusy
    VFreebusy * vf = new VFreebusy(m_Log);
    PR_ASSERT(vf != 0);
    if (vf != 0)
    {
        DateTime dCreated;
        /*vf->setCreated(dCreated);*/
        vf->setLastModified(dCreated);
        vf->setDTStart(start);
        vf->setDTEnd(end);
        UnicodeString us = "vfbreply@hostname.com";

        vf->setUID(JulianUIDRandomGenerator::generate(us));
        vf->setSequence(0);
        if (f != 0)
        {
            vf->addFreebusy(f);
        }
        vf->normalize();
    }
    return vf;
}

//---------------------------------------------------------------------

void NSCalendar::calculateVFreebusy(VFreebusy * toFillIn)
{
    if (toFillIn != 0)
    {
        DateTime start, end;
        start = toFillIn->getDTStart();
        end = toFillIn->getDTEnd();

        if (start.isValid() && 
            end.isValid() &&
            !start.afterDateTime(end))
        {
            DateTime currentTime;

            // remove all old freebusy periods
            toFillIn->removeAllFreebusy();

            Freebusy * f = new Freebusy(m_Log); PR_ASSERT(f != 0);
            createVFreebusyHelper(f, start, end); 
            if (f != 0)
            {
                toFillIn->addFreebusy(f);
            }
            toFillIn->normalize();
            toFillIn->setLastModified(currentTime);           
        }
    }
}

//---------------------------------------------------------------------

t_bool
NSCalendar::printComponentVectorToFile(JulianPtrArray * components,
                                       FILE * f)
{
    t_bool bWrittenOK = TRUE;
    if (components != 0 && f != 0)
    {
        UnicodeString out;
        ICalComponent * ic = 0;
        char * occ = 0;
        t_int32 i;
        for (i = 0; i < components->GetSize(); i++)
        {
            ic = (ICalComponent *) components->GetAt(i);
            out = ic->toICALString();
            occ = out.toCString("");
            if (occ != 0)
            {
                fprintf(f, occ);
                delete [] occ; occ = 0;
            }    
            else 
            {
                bWrittenOK = FALSE;
                break;
            }
        }
    }
    return bWrittenOK;
}   

//---------------------------------------------------------------------

UnicodeString & 
NSCalendar::printComponentVector(JulianPtrArray * components, 
                                 UnicodeString & out)
{
    out = "";

    if (components != 0)
    {
        ICalComponent * ic = 0;
        t_int32 i;
        for (i = 0; i < components->GetSize(); i++)
        {
            ic = (ICalComponent *) components->GetAt(i);
            out += ic->toICALString();
        }
    }
    return out;
}

//---------------------------------------------------------------------

UnicodeString & 
NSCalendar::printFilteredComponentVector(JulianPtrArray * components,
                                         UnicodeString & strFmt,
                                         UnicodeString & out)
{
    t_int32 i;
    out = "";

    if (components != 0)
    {
        UnicodeString u;
        for (i = 0; i < components->GetSize(); i++)
        {
            ICalComponent * ic = (ICalComponent *) components->GetAt(i);
            u = ICalComponent::componentToString(ic->GetType());
            out += ic->format(u, strFmt, "", FALSE);
        }
    }
    return out;
}

//---------------------------------------------------------------------
#if 0
UnicodeString &
NSCalendar::printFilteredComponentVectorWithProperties(JulianPtrArray * components,
                                                       char ** ppsPropList,
                                                       t_int32 iPropCount,
                                                       UnicodeString & out)
{
    UnicodeString strFmt;
    out = "";
    strFmt = ICalComponent::makeFormatString(ppsPropList, iPropCount, strFmt);
    out = printFilteredComponentVector(components, strFmt, out);
    return out;
}
#endif
//---------------------------------------------------------------------

UnicodeString & 
NSCalendar::debugPrintComponentVector(JulianPtrArray * components,
                                      const UnicodeString sType,
                                      UnicodeString & out)
{
    t_int32 i;
    out = "";
    
    if (components != 0)
    {
        char num[10];
        sprintf(num, "%d", components->GetSize());
        out += "----";
        out += sType;
        out += "'s ---- N = ";
        out += num;
        out += "-------- \r\n";

        for (i = 0; i < components->GetSize(); i++)
        {
            ICalComponent * ic = (ICalComponent *) components->GetAt(i);
            sprintf(num, "%d", i + 1);
            out += num;
            out += ")\t";
            out += ic->toString();
            out += JulianKeyword::Instance()->ms_sLINEBREAK;
        }
    }
    else
    {
        out += "----"; out += sType; out += "'s ---- N = 0 --------\r\n";
    }
    return out;
}

//---------------------------------------------------------------------

void 
NSCalendar::getUniqueUIDs(JulianPtrArray * retUID, 
                          ICalComponent::ICAL_COMPONENT type)
{
    // only handle VEvent, VTodo, VJournal, VFreebusy
    switch (type)
    {
    case ICalComponent::ICAL_COMPONENT_VEVENT:
        getUniqueUIDsHelper(retUID, getEvents(), type);
        break;
    case ICalComponent::ICAL_COMPONENT_VTODO:
        getUniqueUIDsHelper(retUID, getTodos(), type);
        break;
    case ICalComponent::ICAL_COMPONENT_VJOURNAL:
        getUniqueUIDsHelper(retUID, getJournals(), type);
        break;
    case ICalComponent::ICAL_COMPONENT_VFREEBUSY:
        getUniqueUIDsHelper(retUID, getVFreebusy(), type);
        break;
    default:
        break;
    }
}

//---------------------------------------------------------------------
void NSCalendar::getEventsByRange(JulianPtrArray * out, 
                                  DateTime start, DateTime end)
{
    if (out != 0)
    {
        if (m_VEventVctr != 0)
        {
            t_int32 i;
            VEvent * anEvent;
            for (i = 0; i < m_VEventVctr->GetSize(); i++)
            {   
                anEvent = (VEvent *) m_VEventVctr->GetAt(i);
                if (!(anEvent->getDTStart().beforeDateTime(start)) &&
                    !(anEvent->getDTStart().afterDateTime(end)))
                {   
                    out->Add(anEvent);
                }
            }
        }
    }
}   
//---------------------------------------------------------------------
void NSCalendar::getEvents(JulianPtrArray * out, UnicodeString & sUID)
{
    getComponents(out, getEvents(), sUID);
}
//---------------------------------------------------------------------
void NSCalendar::getEventsByComponentID(JulianPtrArray * out, 
                                        UnicodeString & sUID,
                                        UnicodeString & sRecurrenceID,
                                        UnicodeString & sModifier)
{
    getTBEByComponentID(out, getEvents(), sUID, sRecurrenceID, sModifier);
}
//---------------------------------------------------------------------
void NSCalendar::getTodos(JulianPtrArray * out, UnicodeString & sUID)
{
    getComponents(out, getTodos(), sUID);
}
//---------------------------------------------------------------------
void NSCalendar::getJournals(JulianPtrArray * out, UnicodeString & sUID)
{
    getComponents(out, getJournals(), sUID);
}
//---------------------------------------------------------------------

void 
NSCalendar::sortComponentsByUID(ICalComponent::ICAL_COMPONENT type)
{
    switch (type)
    {
        // only sorts VEvent, VTodo, VJournal, VFreebusy, not VTimeZone
        case ICalComponent::ICAL_COMPONENT_VEVENT:
            sortComponentsByUIDHelper(getEvents(), type);
            break;
        case ICalComponent::ICAL_COMPONENT_VTODO:
            sortComponentsByUIDHelper(getTodos(), type);
            break;
        case ICalComponent::ICAL_COMPONENT_VJOURNAL:
            sortComponentsByUIDHelper(getJournals(), type);
            break;
        case ICalComponent::ICAL_COMPONENT_VFREEBUSY:
            sortComponentsByUIDHelper(getVFreebusy(), type);
            break;
        default:
            break;
    }
}

//---------------------------------------------------------------------

void 
NSCalendar::sortComponentsByDTStart(ICalComponent::ICAL_COMPONENT type)
{
    switch (type)
    {
        // only sort VEvent, VTodo, VJournal, VFreebusy, not VTimeZone
        case ICalComponent::ICAL_COMPONENT_VEVENT:
            sortComponentsByDTStartHelper(getEvents(), type);
            break;
        case ICalComponent::ICAL_COMPONENT_VTODO:
            sortComponentsByDTStartHelper(getTodos(), type);
            break;
        case ICalComponent::ICAL_COMPONENT_VJOURNAL:
            sortComponentsByDTStartHelper(getJournals(), type);
            break;
        case ICalComponent::ICAL_COMPONENT_VFREEBUSY:
            sortComponentsByDTStartHelper(getVFreebusy(), type);
            break;
        default:
            break;
    }
}

//---------------------------------------------------------------------

void 
NSCalendar::sortComponentsByUIDHelper(JulianPtrArray * components, 
                                      ICalComponent::ICAL_COMPONENT type)
{
    if (components != 0)
    {
        switch (type)
        {
            // only sort VEvent, VTodo, VJournal, VFreebusy
        case ICalComponent::ICAL_COMPONENT_VEVENT:
        case ICalComponent::ICAL_COMPONENT_VTODO:
        case ICalComponent::ICAL_COMPONENT_VJOURNAL:
            components->QuickSort(TimeBasedEvent::CompareTimeBasedEventsByUID);
            break;
        case ICalComponent::ICAL_COMPONENT_VFREEBUSY:
            components->QuickSort(VFreebusy::CompareVFreebusyByUID);
            break;
        default:
            break;
        }
    }
}

//---------------------------------------------------------------------

void
NSCalendar::sortComponentsByDTStartHelper(JulianPtrArray * components,
                                          ICalComponent::ICAL_COMPONENT type)
{
    if (components != 0)
    {
        switch (type)
        {
             // only sort VEvent, VTodo, VJournal, VFreebusy
        case ICalComponent::ICAL_COMPONENT_VEVENT:
        case ICalComponent::ICAL_COMPONENT_VTODO:
        case ICalComponent::ICAL_COMPONENT_VJOURNAL:
            components->QuickSort(TimeBasedEvent::CompareTimeBasedEventsByDTStart);
            break;
        case ICalComponent::ICAL_COMPONENT_VFREEBUSY:
            components->QuickSort(VFreebusy::CompareVFreebusyByDTStart);
            break;
        default: 
            break;
        }
    }
}

//---------------------------------------------------------------------

void
NSCalendar::getUniqueUIDsHelper(JulianPtrArray * retUID,
                                JulianPtrArray * components,
                                ICalComponent::ICAL_COMPONENT type)
{
    PR_ASSERT(retUID != 0);
    if (components != 0)
    {
        t_int32 i;
        UnicodeString u;
        ICalComponent * ic;
        sortComponentsByUIDHelper(components, type);
        for (i = 0; i < components->GetSize(); i++)
        {
            ic = (ICalComponent *) components->GetAt(i);
            if (i == 0)
            {
                u = ic->getUID();
                retUID->Add(new UnicodeString(u));
            }
            else if (ic->getUID() != u)
            {
                u = ic->getUID();
                retUID->Add(new UnicodeString(u));
            }
        }
    }
}

//---------------------------------------------------------------------
#if 0
void
NSCalendar::getComponentsWithType(JulianPtrArray * out, 
                                  UnicodeString & uid,
                                  ICalComponent::ICAL_COMPONENT type)
{
    switch (type)
    {
        case ICalComponent::ICAL_COMPONENT_VEVENT:
            getComponents(out, getEvents(), uid);
            break;
        case ICalComponent::ICAL_COMPONENT_VTODO:
            getComponents(out, getTodos(), uid);
            break;
        case ICalComponent::ICAL_COMPONENT_VJOURNAL:
            getComponents(out, getJournals(), uid);
            break;
            // TODO: add VFreebusy
        default:
            break;
    }
}
#endif
//---------------------------------------------------------------------

void
NSCalendar::getTBEWithTypeByComponentID(JulianPtrArray * out, 
                                        UnicodeString & uid,
                                        UnicodeString & sRecurrenceID, 
                                        UnicodeString & sModifier, 
                                        ICalComponent::ICAL_COMPONENT type)
{
    switch (type)
    {
        case ICalComponent::ICAL_COMPONENT_VEVENT:
            getTBEByComponentID(out, getEvents(), uid, sRecurrenceID, sModifier);
            break;
        case ICalComponent::ICAL_COMPONENT_VTODO:
            getTBEByComponentID(out, getTodos(), uid, sRecurrenceID, sModifier);
            break;
        case ICalComponent::ICAL_COMPONENT_VJOURNAL:
            getTBEByComponentID(out, getJournals(), uid, sRecurrenceID, sModifier);
            break;
        default:
            break;
    }
}

//---------------------------------------------------------------------

void
NSCalendar::getTBEByComponentID(JulianPtrArray * out, 
                                  JulianPtrArray * components,
                                  UnicodeString & sUID,
                                  UnicodeString & sRecurrenceID, 
                                  UnicodeString & sModifier)
{
    PR_ASSERT(out != 0);
    if (components != 0)
    {
        t_int32 i;
        UnicodeString uid;
        TimeBasedEvent * ic;

        // NOTE: if sRecurrence is invalid (i.e. 19691231T235959Z) 
        // then just call getComponents
        DateTime d;
        d.setTimeString(sRecurrenceID);
        if (!d.isValid())
        {
            getComponents(out, components, sUID);
        }
        else
        {
            PR_ASSERT(d.isValid());
            JulianRecurrenceID::RANGE aRange = JulianRecurrenceID::RANGE_NONE;
            aRange = JulianRecurrenceID::stringToRange(sModifier);
            DateTime icrid;

            for (i = 0; i < components->GetSize(); i++)
            {
                ic = (TimeBasedEvent *) components->GetAt(i);

                uid = ic->getUID();
                icrid = ic->getRecurrenceID();
                if (uid == sUID)
                {
                    if (icrid.isValid())
                    {
                        if (icrid == d)
                        {
                            // always add exact match
                            out->Add(ic);
                        }
                        else if (aRange == JulianRecurrenceID::RANGE_THISANDPRIOR && 
                            icrid.beforeDateTime(d))
                        {
                            // also add if THISANDPRIOR and icrid < d 
                            out->Add(ic);
                        }
                        else if (aRange == JulianRecurrenceID::RANGE_THISANDFUTURE && 
                            icrid.afterDateTime(d))
                        {
                            // also add if THISANDFUTURE and icrid > d 
                            out->Add(ic);
                        }
                    }
                }
            }    
        }
    }
}

//---------------------------------------------------------------------

void
NSCalendar::getComponents(JulianPtrArray * out, 
                          JulianPtrArray * components,
                          UnicodeString & sUID)
{
    PR_ASSERT(out != 0);
    if (components != 0)
    {
        t_int32 i;
        UnicodeString uid;
        ICalComponent * ic;
        for (i = 0; i < components->GetSize(); i++)
        {
            ic = (ICalComponent *) components->GetAt(i);
            uid = ic->getUID();
            if (uid == sUID)
            {
                out->Add(components->GetAt(i));
            }
            //if (FALSE) TRACE("uid, sUID = (%s,%s)\r\n", uid.toCString(""), sUID.toCString(""));
        }
    }
}

//---------------------------------------------------------------------

NSCalendar::METHOD 
NSCalendar::stringToMethod(UnicodeString & method)
{
    t_int32 hashCode = method.hashCode();

    if (JulianKeyword::Instance()->ms_ATOM_PUBLISH == hashCode) return METHOD_PUBLISH;
    else if (JulianKeyword::Instance()->ms_ATOM_REQUEST == hashCode) return METHOD_REQUEST;
    else if (JulianKeyword::Instance()->ms_ATOM_REPLY == hashCode) return METHOD_REPLY;
    else if (JulianKeyword::Instance()->ms_ATOM_CANCEL == hashCode) return METHOD_CANCEL;
    else if (JulianKeyword::Instance()->ms_ATOM_REFRESH == hashCode) return METHOD_REFRESH;
    else if (JulianKeyword::Instance()->ms_ATOM_COUNTER == hashCode) return METHOD_COUNTER;
    else if (JulianKeyword::Instance()->ms_ATOM_DECLINECOUNTER == hashCode) return METHOD_DECLINECOUNTER;
    else if (JulianKeyword::Instance()->ms_ATOM_ADD == hashCode) return METHOD_ADD;
    else return METHOD_INVALID;
}

//---------------------------------------------------------------------
#if 0
t_bool
NSCalendar::AreStringVectorsEqual(JulianPtrArray * a, JulianPtrArray * b)
{
    // If both are null, then same
    // If one is null, other is not then NOT same
    // If both NOT null and different size then NOT same
    // If both NOT null and same size and same contents then same
    // If both NOT null and same size and different contents then NOT same
    
    if ((a == 0 && b != 0) || (a != 0 && b == 0))
        return FALSE;
    else if (a == 0 && b == 0)
        return TRUE;
    else 
    {
        // a != 0 && b != 0

        if (a->GetSize() != b->GetSize() )
        {
            return FALSE;
        }
        else
        {
            // TODO: inefficient O(n^2)
            t_int32 i, j;
            t_bool bIn = FALSE;
            UnicodeString aus, bus;
            for (i = 0; i < a->GetSize(); i++)
            {
                aus = *((UnicodeString *) a->GetAt(i));
                bIn = FALSE;

                // check for aus in b
                for (j = 0; j < b->GetSize(); j++)
                {
                    bus = *((UnicodeString *) b->GetAt(i));
                    if (aus.compareIgnoreCase(bus) == 0)
                    {
                        bIn = TRUE;
                        break;
                    }
                }

                if (!bIn)
                    return FALSE;
            }
        }
    }
}
#endif
//---------------------------------------------------------------------

UnicodeString & NSCalendar::methodToString(NSCalendar::METHOD method, UnicodeString & out)
{
    switch(method)
    {
    case METHOD_PUBLISH: out = JulianKeyword::Instance()->ms_sPUBLISH; break;
    case METHOD_REQUEST: out = JulianKeyword::Instance()->ms_sREQUEST; break;
    case METHOD_REPLY: out = JulianKeyword::Instance()->ms_sREPLY; break;
    case METHOD_CANCEL: out = JulianKeyword::Instance()->ms_sCANCEL; break;
    case METHOD_REFRESH: out = JulianKeyword::Instance()->ms_sREFRESH; break;
    case METHOD_COUNTER: out = JulianKeyword::Instance()->ms_sCOUNTER; break;
    case METHOD_DECLINECOUNTER: out = JulianKeyword::Instance()->ms_sDECLINECOUNTER; break;
    case METHOD_ADD: out = JulianKeyword::Instance()->ms_sADD; break;
    default:
        // TODO: Log and error??
        out = "";
        break;
    }
    return out;
}

//---------------------------------------------------------------------
// CalScale
void NSCalendar::setCalScale(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_CalScale == 0)
        m_CalScale = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_CalScale->setValue((void *) &s);
        m_CalScale->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_CalScale), s, parameters);
#endif
}
UnicodeString NSCalendar::getCalScale() const 
{
#if 1
    UnicodeString u;
    if (m_CalScale == 0)
        return "";
    else {
        u = *((UnicodeString *) m_CalScale->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_CalScale), us);
    return us;
#endif
}
//---------------------------------------------------------------------
#if 0
// Source
void NSCalendar::setSource(UnicodeString s, JulianPtrArray * parameters)
{
#if 0
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Source == 0)
        m_Source = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Source->setValue((void *) &s);
        m_Source->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_Source), s, parameters);
#endif
}
UnicodeString NSCalendar::getSource() const 
{
#if 0
    UnicodeString u;
    if (m_Source == 0)
        return "";
    else {
        u = *((UnicodeString *) m_Source->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_Source), us);
    return us;
#endif
}
//---------------------------------------------------------------------
// Name
void NSCalendar::setName(UnicodeString s, JulianPtrArray * parameters)
{
#if 0
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Name == 0)
        m_Name = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Name->setValue((void *) &s);
        m_Name->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_Name), s, parameters);
#endif
}
UnicodeString NSCalendar::getName() const 
{
#if 0
    UnicodeString u;
    if (m_Name == 0)
        return "";
    else {
        u = *((UnicodeString *) m_Name->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_Name), us);
    return us;
#endif
}
#endif /* #if 0 */
//---------------------------------------------------------------------
// Version
void NSCalendar::setVersion(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Version == 0)
        m_Version = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Version->setValue((void *) &s);
        m_Version->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_Version), s, parameters);
#endif
}
UnicodeString NSCalendar::getVersion() const 
{
#if 1
    UnicodeString u;
    if (m_Version == 0)
        return "";
    else {
        u = *((UnicodeString *) m_Version->getValue());
        return u;
    }
#else 
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_Version), us);
    return us;
#endif
}
//---------------------------------------------------------------------
// Prodid
void NSCalendar::setProdid(UnicodeString s, JulianPtrArray * parameters)
{
#if 1
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_Prodid == 0)
        m_Prodid = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_Prodid->setValue((void *) &s);
        m_Prodid->setParameters(parameters);
    }
#else
    ICalComponent::setStringValue(((ICalProperty **) &m_Prodid), s, parameters);
#endif
}
UnicodeString NSCalendar::getProdid() const 
{
#if 1
    UnicodeString u;
    if (m_Prodid == 0)
        return "";
    else {
        u = *((UnicodeString *) m_Prodid->getValue());
        return u;
    }
#else
    UnicodeString us;
    ICalComponent::getStringValue(((ICalProperty **) &m_Prodid), us);
    return us;
#endif
}
//---------------------------------------------------------------------
// XTOKENS
void NSCalendar::addXTokens(UnicodeString s)         
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

