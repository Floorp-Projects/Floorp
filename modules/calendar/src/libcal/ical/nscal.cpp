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
#include "jlog.h"
#include "jutility.h"
#include "keyword.h"

#include "uidrgntr.h"
#include "datetime.h"
#include "period.h"

/** to work on UNIX */
#define TRACE printf

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
    
    if (m_CalScale != 0) { delete m_CalScale; m_CalScale = 0; }
    if (m_Prodid != 0) { delete m_Prodid; m_Prodid = 0; }
    if (m_Version != 0) { delete m_Version; m_Version = 0; }
#if 0
    if (m_Name != 0) { delete m_Name; m_Name = 0; }
    if (m_Source != 0) { delete m_Source; m_Source = 0; }
#endif

    //if (m_Method != 0) { delete m_Method; m_Method = 0; }
    
    if (m_XTokensVctr != 0) 
    { 
        ICalComponent::deleteUnicodeStringVector(m_XTokensVctr);
        delete m_XTokensVctr; m_XTokensVctr = 0; 
    }
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
    {
        return;
    }

    t_bool bNextComponent = FALSE;

    ErrorCode status = ZERO_ERROR;
    UnicodeString sOK;
    UnicodeString u, sName;
    ICalComponent * ic;

    //if (FALSE) TRACE("%s", fileName.toCString(""));
    if (fileName.size() > 0)
    {
        // TODO: set filename
    }
    while (TRUE)
    {
        PR_ASSERT(brFile != 0);
        brFile->readFullLine(strLine, status);
        //strLine.trim();
        ICalProperty::Trim(strLine);

        //if (FALSE) TRACE("NSCAL: line (size = %d) = ---%s---\r\n", 
        //   strLine.size(), strLine.toCString(""));           
        
        if (FAILURE(status) && strLine.size() <= 0)
            break;

        ICalProperty::parsePropertyLine(strLine, propName, 
            propVal, parameters);
        
        //if (FALSE) TRACE("NSCAL: propName = --%s--, propVal = --%s--, paramSize = %d\r\n", 
        //    propName.toCString(""), propVal.toCString(""), parameters->GetSize());
      
        if (strLine.size() == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            //delete parameters; parameters = 0;
            
            continue;
        }

        // break for "END:" line
        else if (strLine.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND_VCALENDAR) == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            //delete parameters; parameters = 0;
            
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
                //delete parameters; parameters = 0;
            
                bNextComponent = TRUE;
                sName = propVal;

                while (bNextComponent)
                {
                    // TODO: finish
                    ic = ICalComponentFactory::Make(sName, m_Log);
                    if (ic != 0)
                    {
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
                                        addComponentWithType(ic, ic->GetType()); 
                                    }
                                    else
                                    {
                                        if (m_Log) m_Log->logString(
                                            JulianLogErrorMessage::Instance()->ms_sInvalidComponent, 300);
                                        delete ic; ic = 0;
                                    }
                                }
                                else
                                {
                                    if (ic->getSequence() > getMaximumSequence(out) && 
                                         (ic->isValid()))
                                    {
                                        removeComponents(out);
                                        addComponentWithType(ic, ic->GetType());
                                    }
                                    else
                                    {
                                        if (m_Log) m_Log->logString(
                                            JulianLogErrorMessage::Instance()->ms_sInvalidComponent, 300);
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
                                        addComponentWithType(ic, ic->GetType());
                                    }
                                    else
                                    {
                                        if (m_Log) m_Log->logString(
                                            JulianLogErrorMessage::Instance()->ms_sInvalidComponent, 300);
                                        delete ic; ic = 0;
                                    }
                                }
                                else
                                {
                                    if (ic->isValid())
                                    {
                                        removeComponents(out);
                                        addComponentWithType(ic, ic->GetType());
                                    }
                                    else
                                    {
                                        if (m_Log) m_Log->logString(
                                            JulianLogErrorMessage::Instance()->ms_sInvalidComponent, 300);
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
                                addComponent(ic, sName);
                            }
                            else
                            {
                                if (m_Log) m_Log->logString(
                                    JulianLogErrorMessage::Instance()->ms_sInvalidComponent, 300);
                                delete ic;
                            }
                        }
#else // #if 0
               
                        if (ic->isValid() || ic->getUID().size() > 0)
                        {
                            addComponent(ic, sName);
                        }
                        else
                        {
                            if (m_Log) m_Log->logString(
                                JulianLogErrorMessage::Instance()->ms_sInvalidComponent, 300);
                            delete ic;
                        }
#endif // #if 0
                        if (sOK.compareIgnoreCase(JulianKeyword::Instance()->ms_sOK) == 0)
                        {
                            bNextComponent = FALSE;
                        }
                        else 
                        {
                            if (sOK.compareIgnoreCase("BEGIN:V") == 0)
                            {
                                bNextComponent = TRUE;
                                sName = sOK.extractBetween(6, sOK.size(), sName);
                            }
                            else
                            {
                                bNextComponent = FALSE;
                            }
                        }
                    }
                    if (ic == 0 || (!bNextComponent))
                    {
                        break;
                    }
                }
            }
            else 
            {
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
#if 0
void NSCalendar::importData(ICalReader * brFile, UnicodeString & from)
{
    UnicodeString strLine, propName, propVal;
    JulianPtrArray * parameters = new JulianPtrArray();
    PR_ASSERT(parameters != 0);
    ErrorCode status = ZERO_ERROR;
    UnicodeString sOK, u, sName;
    ICalComponent * ic;

    UnicodeString method;
    t_bool bNextComponent = FALSE;
    JulianPtrArray * vTimeZones = 0;
    UnicodeString sOKString;

    ICalComponent::ICAL_COMPONENT componentType;
    while (TRUE)
    {
        PR_ASSERT(brFile != 0);
        brFile->readFullLine(strLine, status);
        //strLine.trim();
        ICalProperty::Trim(strLine);

        //if (FALSE) TRACE("importData: line (size = %d) = ---%s---\r\n", 
        //   strLine.size(), strLine.toCString(""));       

        if (FAILURE(status) && strLine.size() <= 0)
            break;

        ICalProperty::parsePropertyLine(strLine, propName,
            propVal, parameters);

        //if (FALSE) TRACE("importData: propName = --%s--, propVal = --%s--, 
        //    paramSize = %d\r\n", propName.toCString(""), 
        //    propVal.toCString(""), parameters->GetSize());

        if (strLine.size() == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            continue;
        }

        // break for "END:" line
        else if (strLine.compareIgnoreCase(
            JulianKeyword::Instance()->ms_sEND_VCALENDAR) == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
        }
        else
        {
            if (propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sMETHOD) == 0)
            {
                method = propVal;
                //if (FALSE) TRACE("method is %s\r\n", propVal);
            }
            else if (propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sBEGIN) == 0)
            {
                ICalProperty::deleteICalParameterVector(parameters);
                parameters->RemoveAll();
                bNextComponent = TRUE;
                sName = propVal;

                while (bNextComponent)
                {
                    ic = ICalComponentFactory::Make(sName, m_Log);
                    if (ic != 0)
                    {
                        sOK = ic->parse(brFile, method, sOK, vTimeZones);
                    
                        componentType = ic->GetType();
                        if (componentType == ICalComponent::ICAL_COMPONENT_VFREEBUSY)
                        {
                            importVFreebusy((VFreebusy *) ic, method, vTimeZones, from);
                        }
                        else if (componentType == ICalComponent::ICAL_COMPONENT_VTIMEZONE)
                        {
                            if (ic->isValid())
                            {
                                if (vTimeZones == 0)
                                    vTimeZones = new JulianPtrArray();
                                PR_ASSERT(vTimeZones != 0);
                                if (vTimeZones != 0)
                                {
                                    vTimeZones->Add(ic);
                                }
                            }   
                        }
                        else if (componentType == ICalComponent::ICAL_COMPONENT_VEVENT ||
                                 componentType == ICalComponent::ICAL_COMPONENT_VTODO ||
                                 componentType == ICalComponent::ICAL_COMPONENT_VJOURNAL)
                        {
                            importTimeBasedEvent((TimeBasedEvent *) ic, method, 
                                vTimeZones, from, componentType);
                        }

                        if (sOK.compareIgnoreCase(JulianKeyword::Instance()->ms_sOK) == 0)
                        {
                            bNextComponent = FALSE;
                        }
                        else 
                        {
                            if (sOK.startsWith(JulianKeyword::Instance()->ms_sBEGIN_WITH_COLON))
                            {
                                bNextComponent = TRUE;
                            }
                            else
                                bNextComponent = FALSE;
                        }
                    }

                    if (!sOK.compareIgnoreCase(JulianKeyword::Instance()->ms_sOK) == 0)
                    {
                        sOKString = sOK;
                        break;
                    }
                } // end while(bNextComponent)
            } // end compareIgnoreCase(BEGIN)
        } // end else
    } // end while 
    if (vTimeZones != 0)
    {
        ICalComponent::deleteICalComponentVector(vTimeZones);
        vTimeZones->RemoveAll();
        delete vTimeZones; vTimeZones = 0;
    }

    ICalProperty::deleteICalParameterVector(parameters);
    parameters->RemoveAll();
    delete parameters; parameters = 0;
}
#endif
//---------------------------------------------------------------------
#if 0
void
NSCalendar::importTimeBasedEvent(TimeBasedEvent * tbe, 
                                 UnicodeString & sMethod,
                                 JulianPtrArray * vTimeZones,
                                 UnicodeString & from,
                                 ICalComponent::ICAL_COMPONENT type)
{
    JulianPtrArray * vMatch = 0;
    JulianPtrArray * uidMatchEvents = 0;
    JulianPtrArray * matchingEvents = 0;
    t_int32 seq;

    PR_ASSERT(tbe != 0);
    // TODO: check method validity.
    NSCalendar::METHOD method = stringToMethod(sMethod);
    if (method == METHOD_PUBLISH || method == METHOD_REQUEST ||
        method == METHOD_DECLINECOUNTER)
    {
        // only change local store if new event has a higher sequence numer
        // if event is recurring
        // check if rrules, exrules, rdates, exdats have change
        // if so, unzip new recurring event
        //, and set-substrat dates with old unzipped events
        UnicodeString tbeuid = tbe->getUID();
        t_int32 tbeseq = tbe->getSequence();

        uidMatchEvents = new JulianPtrArray();
        PR_ASSERT(uidMatchEvents != 0);
        getEvents(uidMatchEvents, tbeuid);
        if (uidMatchEvents->GetSize() > 0)
        {
#if 0
            // handle recurrence import
            TimeBasedEvent * firstEvent = (TimeBasedEvent *) uidMatchEvents->GetAt(0);
            if (firstEvent->isRecurring())
            {
                // todo: test for rdates, exdates later
                if (!AreStringVectorsEqual(firstEvent->getRRules(), tbe->getRRules()) ||
                    !AreStringVectorsEqual(firstEvent->getExRules(), tbe->getExRules()))
                {
                    if (tbe->getSequence() > firstEvent->getSequence())
                    {
                        JulianPtrArray * vE = new JulianPtrArray();
                        PR_ASSERT(vE != 0);
                        e->createRecurrenceEvents(vE, vTimeZones);
                        
                    }
                }
            }
#endif        
        }
        matchingEvents = new JulianPtrArray();
        PR_ASSERT(matchingEvents != 0);
        getComponentsWithType(matchingEvents, tbeuid, type);
        if (matchingEvents->GetSize() > 0)
        {
            // Not in my calendar, add event to calendar if valid
            if (tbe->isValid())
            {
                if (tbe->isExpandableEvent())
                {
                    JulianPtrArray * vE = new JulianPtrArray();
                    PR_ASSERT(vE != 0);
                    tbe->createRecurrenceEvents(vE, vTimeZones);
                    addComponentsWithType(vE, type);
                }
                else
                {
                    addComponentWithType(tbe, type);
                }
            }
        }
        else
        {
            // update my calendar
            seq = getMaximumSequence(matchingEvents);
            updateEvents(matchingEvents, tbe, seq, type);
            getComponentsWithType(uidMatchEvents, tbeuid, type);
            setAllSequenceTBE(uidMatchEvents, tbeseq);
        }
    }
    else if (method == METHOD_REPLY || method == METHOD_CANCEL || 
             method == METHOD_REFRESH || method == METHOD_COUNTER)
    {
        if (method == METHOD_CANCEL)
        {
#if 0
            addCancel(tbe, type);
#endif
        }
        else if (method == METHOD_REFRESH)
        {
#if 0
            addRefresh(tbe, type);
#endif
        }
        else 
        {
            DateTime d;
            d = tbe->getDTStamp();
            PR_ASSERT(d.isValid());
            if (!d.isValid())
            {
                // TODO: Log an error
            }
#if 0
            addReplyOrCounter(tbe, method, from);
#endif
        }
    }
}
#endif
//---------------------------------------------------------------------
#if 0
void 
NSCalendar::addCancel(TimeBasedEvent * e, 
                      ICalComponent::ICAL_COMPONENT type,
                      t_bool & bHoldOn)
{
    // if no exdate and no exrules delete all events
    // if there are exdate or exrules, don't delete everything
    // TODO: finish
//#if 0
    if (e->getExRules() == 0 && e->getExDates() == 0)
    {
        // delete everything
        JulianPtrArray * m = new JulianPtrArray();
        PR_ASSERT(m != 0);

        getComponentsWithType(m, 
    }
//#endif
}
#endif                            
//---------------------------------------------------------------------
#if 0
void 
NSCalendar::addReplyOrCounter(TimeBasedEvent * tbe, 
                              NSCalendar::METHOD,
                              UnicodeString & from)
{
    // TODO: finish
}
#endif

//---------------------------------------------------------------------

t_int32
NSCalendar::getMaximumSequence(JulianPtrArray * components)
{
    if (components == 0 || components->GetSize() == 0)
        return -1;
    else
    {
        t_int32 i;
        t_int32 retSeq = -1;
        for (i = 0; i < components->GetSize(); i++)
        {
            ICalComponent * ic = (ICalComponent *) components->GetAt(i);
            if (ic->getSequence() > retSeq)
            {
                retSeq = ic->getSequence();
            }
        }
        return retSeq;
    }
}

//---------------------------------------------------------------------

void
NSCalendar::setAllSequenceTBE(JulianPtrArray * tbes, t_int32 seqNo)
{
    PR_ASSERT(tbes != 0);
    if (tbes != 0)
    {
        t_int32 i;
        t_int32 tbeseq;
        for (i = 0; i < tbes->GetSize(); i++)
        {
            TimeBasedEvent * tbe = (TimeBasedEvent *) tbes->GetAt(i);
            tbeseq = tbe->getSequence();
            if (seqNo > tbeseq)
            {
                tbe->setSequence(seqNo);
            }
        }
    }
}

//---------------------------------------------------------------------
#if 0
void 
NSCalendar::importVFreebusy(VFreebusy * vf, UnicodeString & method,
                            UnicodeString & from)
{
        //TODO: finish
}
#endif
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
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
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
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
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
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVCALENDAR, strLine, 100);
        }
        // if propVal != CALSCALE, just log, 
        // will set to GREGORIAN later.
        if (JulianKeyword::Instance()->ms_ATOM_CALSCALE != propVal.hashCode())
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidPropertyValue, 
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
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVCALENDAR, strLine, 100);
        }
        i = stringToMethod(propVal);
        if (i < 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidPropertyValue, 
                JulianKeyword::Instance()->ms_sVCALENDAR,
                propName, propVal, 200);
        }
        else 
        {
            if (getMethod() >= 0)
            {
                if (m_Log) m_Log->logString(
                    JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
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
           if (m_Log) m_Log->logString(
               JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
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
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sVCALENDAR, strLine, 100);
        }
        setName(propVal, parameters);
        return TRUE;
    }
#endif
    else
    {
        if (m_Log) m_Log->logString(
            JulianLogErrorMessage::Instance()->ms_sInvalidPropertyName, 
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
    if (v != 0)
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
            for (i = 0; i < v->GetSize(); i++)
            {
                addComponentWithType((ICalComponent *) v->GetAt(i), type);
            }
        }   
        delete v; v = 0;
    }
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
void NSCalendar::addEvent(ICalComponent * v)
{
    if (m_VEventVctr == 0)
        m_VEventVctr = new JulianPtrArray();
    PR_ASSERT(m_VEventVctr != 0);
    if (m_VEventVctr != 0)
    {
        m_VEventVctr->Add(v);
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

void NSCalendar::addComponent(ICalComponent * ic, UnicodeString & sName)
{
    ICalComponent::ICAL_COMPONENT type;
    t_bool error;
    type = ICalComponent::stringToComponent(sName, error);
    if (!error)
        addComponentWithType(ic, type);
}

//---------------------------------------------------------------------

void NSCalendar::addComponentWithType(ICalComponent * ic, 
                                      ICalComponent::ICAL_COMPONENT type)
{
    if (type == ICalComponent::ICAL_COMPONENT_VEVENT)
    {
        addEvent(ic);
    }
    if (type == ICalComponent::ICAL_COMPONENT_VTODO)
    {
        addTodo(ic);
    }
    if (type == ICalComponent::ICAL_COMPONENT_VJOURNAL)
    {
        addJournal(ic);
    }
    else if (type == ICalComponent::ICAL_COMPONENT_VFREEBUSY)
    {
        addVFreebusy(ic);
    }
    else if (type == ICalComponent::ICAL_COMPONENT_VTIMEZONE)
    {
        addTimeZone(ic);
    }
}

//---------------------------------------------------------------------

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
        vf->setCreated(dCreated);
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

UnicodeString & 
NSCalendar::printComponentVector(JulianPtrArray * components, 
                                 UnicodeString & out)
{
    t_int32 i;
    out = "";

    if (components != 0)
    {
        for (i = 0; i < components->GetSize(); i++)
        {
            ICalComponent * ic = (ICalComponent *) components->GetAt(i);
            out += ic->toICALString();
        }
    }
    return out;
}
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
    }
}

//---------------------------------------------------------------------
void NSCalendar::getEvents(JulianPtrArray * out, UnicodeString & sUID)
{
    getComponents(out, getEvents(), sUID);
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
}
UnicodeString NSCalendar::getCalScale() const 
{
    UnicodeString u;
    if (m_CalScale == 0)
        return "";
    else {
        u = *((UnicodeString *) m_CalScale->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
#if 0
// Source
void NSCalendar::setSource(UnicodeString s, JulianPtrArray * parameters)
{
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
}
UnicodeString NSCalendar::getSource() const 
{
    UnicodeString u;
    if (m_Source == 0)
        return "";
    else {
        u = *((UnicodeString *) m_Source->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
// Name
void NSCalendar::setName(UnicodeString s, JulianPtrArray * parameters)
{
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
}
UnicodeString NSCalendar::getName() const 
{
    UnicodeString u;
    if (m_Name == 0)
        return "";
    else {
        u = *((UnicodeString *) m_Name->getValue());
        return u;
    }
}
#endif /* #if 0 */
//---------------------------------------------------------------------
// Version
void NSCalendar::setVersion(UnicodeString s, JulianPtrArray * parameters)
{
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
}
UnicodeString NSCalendar::getVersion() const 
{
    UnicodeString u;
    if (m_Version == 0)
        return "";
    else {
        u = *((UnicodeString *) m_Version->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
// Prodid
void NSCalendar::setProdid(UnicodeString s, JulianPtrArray * parameters)
{
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
}
UnicodeString NSCalendar::getProdid() const 
{
    UnicodeString u;
    if (m_Prodid == 0)
        return "";
    else {
        u = *((UnicodeString *) m_Prodid->getValue());
        return u;
    }
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

