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

// tzpart.h
// John Sun
// 10:04 AM February 24 1998

#include "stdafx.h"
#include "jdefines.h"

#include "tzpart.h"
#include "prprtyfy.h"
#include "rcrrence.h"
#include "jlog.h"
#include "keyword.h"

//UnicodeString TZPart::m_strDefaultFmt = "\r\n\t%(EEE MM/dd/yy hh:mm:ss z)B,%x%(EEE MM/dd/yy hh:mm:ss z)y, NAME: %n, FROM: %f, TO: %d";
//UnicodeString TZPart::ms_sAllMessage = "%B%x%y%f%d%n%K";

//static UnicodeString ms_sRRULE_WITH_SEMICOLON = "RRULE;";
//---------------------------------------------------------------------

// private never use
#if 0
TZPart::TZPart()
{
    PR_ASSERT(FALSE);
}
#endif
//---------------------------------------------------------------------

TZPart::TZPart(JLog * initLog) 
: m_CommentVctr(0), m_DTStart(0), m_TZNameVctr(0), m_TZOffsetTo(0),
  m_TZOffsetFrom(0), m_RDate(0), m_RRule(0),
  m_StartMonth(-1), m_StartDay(-1), m_StartWeek(0), m_StartTime(0),
  m_XTokensVctr(0),
  m_Log(initLog)
{
    
    m_Until.setTime(-1);
}

//---------------------------------------------------------------------

TZPart::TZPart(TZPart & that) 
: m_CommentVctr(0), m_DTStart(0), m_TZNameVctr(0), m_TZOffsetTo(0),
  m_TZOffsetFrom(0), m_RDate(0), m_RRule(0), 
  m_XTokensVctr(0)
{
    m_StartMonth = that.m_StartMonth;
    m_StartDay = that.m_StartDay;
    m_StartWeek = that.m_StartWeek;
    m_StartTime = that.m_StartTime;
    m_Until = that.m_Until;
    m_Name = that.m_Name;

    if (that.m_CommentVctr != 0)
    {
        m_CommentVctr = new JulianPtrArray(); PR_ASSERT(m_CommentVctr != 0);
        if (m_CommentVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_CommentVctr, m_CommentVctr, m_Log);
        }
    }

    if (that.m_DTStart != 0) { m_DTStart = that.m_DTStart->clone(m_Log); }

    if (that.m_TZNameVctr != 0)
    {
        m_TZNameVctr = new JulianPtrArray(); PR_ASSERT(m_TZNameVctr != 0);
        if (m_TZNameVctr != 0)
        {
            ICalProperty::CloneICalPropertyVector(that.m_TZNameVctr, m_TZNameVctr, m_Log);
        }
    }
    if (that.m_TZOffsetTo != 0) { m_TZOffsetTo = that.m_TZOffsetTo->clone(m_Log); }
    if (that.m_TZOffsetFrom != 0) { m_TZOffsetFrom = that.m_TZOffsetFrom->clone(m_Log); }
    if (that.m_RDate != 0) { m_RDate = that.m_RDate->clone(m_Log); }
    if (that.m_RRule != 0) { m_RRule = that.m_RRule->clone(m_Log); }
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
TZPart::clone(JLog * initLog)
{
    m_Log = initLog; 
    //PR_ASSERT(m_Log != 0);
    return new TZPart(*this);
}
//---------------------------------------------------------------------

TZPart::~TZPart()
{
    if (m_CommentVctr != 0) 
    {
        ICalProperty::deleteICalPropertyVector(m_CommentVctr);
        delete m_CommentVctr; m_CommentVctr = 0;
    }
    if (m_TZNameVctr != 0) 
    {
        ICalProperty::deleteICalPropertyVector(m_TZNameVctr);
        delete m_TZNameVctr; m_TZNameVctr = 0;
    }      
    if (m_DTStart != 0) { delete m_DTStart; m_DTStart = 0; }
    if (m_RDate != 0) { delete m_RDate; m_RDate = 0; }
    if (m_RRule != 0) { delete m_RRule; m_RRule = 0; }
    if (m_TZOffsetTo != 0) { delete m_TZOffsetTo; m_TZOffsetTo = 0; }
    if (m_TZOffsetFrom != 0) { delete m_TZOffsetFrom; m_TZOffsetFrom = 0; }
    if (m_XTokensVctr != 0) 
    { 
        ICalComponent::deleteUnicodeStringVector(m_XTokensVctr);
        delete m_XTokensVctr; m_XTokensVctr = 0; 
    }
}

//---------------------------------------------------------------------
UnicodeString &
TZPart::parse(ICalReader * brFile, UnicodeString & sType, 
              UnicodeString & parseStatus, JulianPtrArray * vTimeZones,
              t_bool bIgnoreBeginError,
              JulianUtility::MimeEncoding encoding)
{
    UnicodeString strLine, propName, propVal;
    JulianPtrArray * parameters = new JulianPtrArray();
    parseStatus = JulianKeyword::Instance()->ms_sOK;

    PR_ASSERT(parameters != 0 && brFile != 0);
    if (parameters == 0 || brFile == 0)
    {
        // ran out of memory, return invalid tzpart
        return parseStatus;
    }
    ErrorCode status = ZERO_ERROR;
    
    setName(sType);

    // NOTE: Remove later to remove a stupid compile warning
    if (vTimeZones && bIgnoreBeginError){}

    while (TRUE)
    {
        PR_ASSERT(brFile != 0);
        brFile->readFullLine(strLine, status);
        //strLine.trim();
        ICalProperty::Trim(strLine);

        //if (FALSE) TRACE("line (size = %d) = ---%s---\r\n", strLine.size(), strLine.toCString(""));
        
        if (FAILURE(status) && strLine.size() < 0)
            break;
        
        ICalProperty::parsePropertyLine(strLine, propName, propVal, parameters);
        //if (FALSE) TRACE("propName = --%s--, propVal = --%s--, paramSize = %d\r\n", propName.toCString(""), propVal.toCString(""), parameters->GetSize());
      
        if (strLine.size() == 0)
        {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();
            
            continue;
        }
        // END:STANDARD OR END:DAYLIGHT (sType = daylight or standard)
         else if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
                  (propVal.compareIgnoreCase(sType) == 0))
         {
            ICalProperty::deleteICalParameterVector(parameters);
            parameters->RemoveAll();

            break;
         }
         else if ((propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sEND) == 0) &&
                  (propVal.compareIgnoreCase(JulianKeyword::Instance()->ms_sVTIMEZONE) == 0))
                // TODO: handle getting ABRUPT (BEGIN:DAYLIGHT or BEGIN:STANDARD)
                // TODO: handle any BEGIN: (i.e BEGIN:VEVENT, BEGIN:VTODO, etc.)
                // TODO: handle END:VCALENDAR.
         {
             if (m_Log) m_Log->logString(
                 JulianLogErrorMessage::Instance()->ms_sAbruptEndOfParsing, 
                 JulianKeyword::Instance()->ms_sTZPART, strLine, 300);
             ICalProperty::deleteICalParameterVector(parameters);
             parameters->RemoveAll();

             parseStatus = strLine;
             break;            
         }
         else 
         {
             storeData(strLine, propName, propVal, parameters);
             ICalProperty::deleteICalParameterVector(parameters);
             parameters->RemoveAll();
         }
    }
    ICalProperty::deleteICalParameterVector(parameters);
    parameters->RemoveAll();
    delete parameters; parameters = 0;

    selfCheck();
    return parseStatus;
}

//---------------------------------------------------------------------
void TZPart::storeData(UnicodeString & strLine, UnicodeString & propName,
                       UnicodeString & propVal, JulianPtrArray * parameters)
{

    if (strLine.size() > 0) 
    {
    } 

    t_bool bParamValid;

    //UnicodeString u;
    t_int32 hashCode = propName.hashCode();
    //if (propName.compareIgnoreCase(JulianKeyword::Instance()->ms_sCOMMENT) == 0)
   
    if (JulianKeyword::Instance()->ms_ATOM_COMMENT == hashCode)
    {
        // check parameters
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asAltrepLanguageParamRange,
            JulianAtomRange::Instance()->ms_asAltrepLanguageParamRangeSize);
        
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
        }

        addComment(propVal, parameters);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_TZNAME == hashCode)
    {
        // check parameters 
        bParamValid = ICalProperty::CheckParams(parameters, 
            JulianAtomRange::Instance()->ms_asLanguageParamRange,
            JulianAtomRange::Instance()->ms_asLanguageParamRangeSize);
        if (!bParamValid)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
        }

        addTZName(propVal, parameters);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_DTSTART == hashCode)
    {
        // assert in local time
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
        }

        if (getDTStartProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTZPART, propName, 100);
        }
        DateTime d(propVal);
        t_int32 yr, mo, dy, hr, mn, sc;
        if (DateTime::IsParseable(propVal, yr, mo, dy, hr, mn, sc))
        {
            m_StartTime = (t_int32) ((hr * 3600 + mn * 60) * 1000); // set start time in millis
        }
        setDTStart(d, parameters);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_RDATE == hashCode)
    {
        // assert in local time
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
        }

        if (getRDateProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTZPART, propName, 100);
        }
        // DONE:?TODO: Finish
        DateTime d(propVal);
        setRDate(d, parameters);
    }
    else if (JulianKeyword::Instance()->ms_ATOM_RRULE == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
        }

        if (getRRuleProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTZPART, propName, 100);
        }
        setRRule(propVal, parameters); 
    }
    else if (JulianKeyword::Instance()->ms_ATOM_TZOFFSETTO == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
        }

        if (getTZOffsetToProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTZPART, propName, 100);
        }
        setTZOffsetTo(propVal, parameters); 
    }
    else if (JulianKeyword::Instance()->ms_ATOM_TZOFFSETFROM == hashCode)
    {
        // no parameters
        if (parameters->GetSize() > 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sInvalidOptionalParam, 
                JulianKeyword::Instance()->ms_sTZPART, strLine, 100);
        }

        if (getTZOffsetFromProperty() != 0)
        {
            if (m_Log) m_Log->logString(
                JulianLogErrorMessage::Instance()->ms_sDuplicatedProperty, 
                JulianKeyword::Instance()->ms_sTZPART, propName, 100);
        }
        setTZOffsetFrom(propVal, parameters);
    }
    else if (ICalProperty::IsXToken(propName))
    {
        addXTokens(strLine);  
    }
    else 
    {
        // actually tzpart
        if (m_Log) m_Log->logString(
            JulianLogErrorMessage::Instance()->ms_sInvalidPropertyName, 
            JulianKeyword::Instance()->ms_sTZPART,
            propName, 200);
    }
}
//---------------------------------------------------------------------
void TZPart::selfCheck()
{
    //if (FALSE) TRACE("starttime = %d, startmonth = %d, startday = %d, startweek = %d\r\n", m_StartTime, m_StartMonth, m_StartDay, m_StartWeek);
    // TODO: finish
    //parseRule();
}
//---------------------------------------------------------------------
t_bool TZPart::isValid()
{
    UnicodeString u;
    // required to have valid dtstart, tzoffsetTo, tzoffsetFrom
    if (!getDTStart().isValid())
        return FALSE;
    
    u = getTZOffsetTo();
    if (!DateTime::IsParseableUTCOffset(u))
        return FALSE;

    u = getTZOffsetFrom();
    if (!DateTime::IsParseableUTCOffset(u))
        return FALSE;
    
    return TRUE;
}

//---------------------------------------------------------------------

UnicodeString TZPart::toICALString()
{
    return ICalComponent::format(m_Name, JulianFormatString::Instance()->ms_sTZPartAllMessage);
}

//---------------------------------------------------------------------

UnicodeString TZPart::toICALString(UnicodeString method, 
                                   UnicodeString name,
                                   t_bool isRecurring)
{
    // NOTE: remove later avoid warnings
    if (isRecurring) {}
    return toICALString();
}

//---------------------------------------------------------------------
UnicodeString TZPart::formatChar(t_int32 c, UnicodeString sFilterAttendee,
                                 t_bool delegateRequest)
{
    UnicodeString s, sResult;
    
    // NOTE: remove here is get rid of compiler warnings
    if (sFilterAttendee.size() > 0 || delegateRequest)
    {
    }
    
    switch (c)
    {
    case ms_cComment:
        s = JulianKeyword::Instance()->ms_sCOMMENT;
        return ICalProperty::propertyVectorToICALString(s, getComment(), sResult); 
    case ms_cTZName:
        s = JulianKeyword::Instance()->ms_sTZNAME;
        return ICalProperty::propertyVectorToICALString(s, getTZName(), sResult); 
    case ms_cDTStart:
        s = JulianKeyword::Instance()->ms_sDTSTART;
        return ICalProperty::propertyToICALString(s, getDTStartProperty(), sResult); 
    case ms_cRDate:
        s = JulianKeyword::Instance()->ms_sRDATE;
        return ICalProperty::propertyToICALString(s, getRDateProperty(), sResult); 
    case ms_cRRule:
        s = JulianKeyword::Instance()->ms_sRRULE;
        return ICalProperty::propertyToICALString(s, getRRuleProperty(), sResult); 
    case ms_cTZOffsetTo:
        s = JulianKeyword::Instance()->ms_sTZOFFSETTO;
        return ICalProperty::propertyToICALString(s, getTZOffsetToProperty(), sResult); 
    case ms_cTZOffsetFrom:
        s = JulianKeyword::Instance()->ms_sTZOFFSETFROM;
        return ICalProperty::propertyToICALString(s, getTZOffsetFromProperty(), sResult); 
    case ms_cXTokens: 
          return ICalProperty::vectorToICALString(getXTokens(), sResult);
    default:
        return "";
    }
}

//---------------------------------------------------------------------
UnicodeString TZPart::toStringChar(t_int32 c, UnicodeString & dateFmt)
{
    UnicodeString s;
    switch (c)
    {
    case ms_cComment:
        return ICalProperty::propertyVectorToString(getComment(), s); 
    case ms_cTZName:
        return ICalProperty::propertyVectorToString(getTZName(), s); 
    case ms_cDTStart:
        return ICalProperty::propertyToString(getDTStartProperty(), dateFmt, s); 
    case ms_cRDate:
        return ICalProperty::propertyToString(getRDateProperty(), dateFmt, s); 
    case ms_cRRule:
        return ICalProperty::propertyToString(getRRuleProperty(), dateFmt, s); 
    case ms_cTZOffsetTo:
        return ICalProperty::propertyToString(getTZOffsetToProperty(), dateFmt, s); 
    case ms_cTZOffsetFrom:
        return ICalProperty::propertyToString(getTZOffsetFromProperty(), dateFmt, s); 
    default:
        return "";
    }
}
//---------------------------------------------------------------------

UnicodeString TZPart::toString()
{    
    return ICalComponent::toStringFmt(
        JulianFormatString::Instance()->ms_TZPartStrDefaultFmt);
}

//---------------------------------------------------------------------
void TZPart::setName(UnicodeString s) 
{ 
    m_Name = s; 
}
//---------------------------------------------------------------------

t_bool TZPart::parseRule()
{
    if (getRDate().isValid())
    {
        parseRDate();
        return TRUE;
    }
    else
    {
        return parseRRule();
    }
}

//---------------------------------------------------------------------
void TZPart::parseRDate()
{
    PR_ASSERT(getRDate().isValid());
    if (getRDate().isValid())
    {
        DateTime d;
        d = getRDate();

        m_StartMonth = d.get(Calendar::MONTH);
        m_StartWeek = d.get(Calendar::DAY_OF_WEEK_IN_MONTH);
        m_StartDay = d.get(Calendar::DAY_OF_WEEK);   
    }
}
//---------------------------------------------------------------------
t_bool TZPart::parseRRule()
{
    t_int32 i;
    UnicodeString pN, pV, s, strLine;
    ICalParameter * ip;
    t_bool bParseError = FALSE;
    ErrorCode status = ZERO_ERROR;

    // TODO: insert assertion that Recurrence::isValidRRule(getRRule());
    strLine = getRRule();
    //if (FALSE) TRACE("line (size = %d) = ---%s---\r\n", strLine.size(), strLine.toCString(""));   
      
    // manipulating string so I can use the parsePropertyLine method
    strLine += ':'; //JulianKeyword::Instance()->ms_sCOLON_SYMBOL;
    strLine.insert(0, JulianKeyword::Instance()->ms_sRRULE_WITH_SEMICOLON);
    //if (FALSE) TRACE("line (size = %d) = ---%s---\r\n", strLine.size(), strLine.toCString(""));   

    JulianPtrArray * parameters = new JulianPtrArray();
    PR_ASSERT(parameters != 0);
    if (parameters == 0)
    {
        return FALSE;
    }

    ICalProperty::parsePropertyLine(strLine, pN, pV, parameters);
    //if (FALSE) TRACE("pN = --%s--, pV = --%s--, paramSize = %d\r\n", pN.toCString(""), pV.toCString(""), parameters->GetSize());
      
    for (i = 0; i < parameters->GetSize(); i++)
    {
        ip = (ICalParameter *) parameters->GetAt(i);
   
        pN = ip->getParameterName(pN);
        pV = ip->getParameterValue(pV);
        //if (FALSE) TRACE("pN = --%s--, pV = --%s--, paramSize = %d\r\n", pN.toCString(""), pV.toCString(""), parameters->GetSize());
       
        if (pN.compareIgnoreCase(JulianKeyword::Instance()->ms_sBYMONTH) == 0)
        {
            // since month is 0-based, must subtract 1
            m_StartMonth = JulianUtility::atot_int32(pV.toCString(""), bParseError, pV.size()) - 1;
        }
        else if (pN.compareIgnoreCase(JulianKeyword::Instance()->ms_sBYDAY) == 0)
        {
            if (pV.size() == 3)
            {
                // positive week
                m_StartWeek = JulianUtility::atot_int32(pV.extract(0, 1, s).toCString(""), bParseError, 1);
                m_StartDay = Recurrence::stringToDay(pV.extract(1, 2, s), bParseError);
            }
            else
            {
                PR_ASSERT(pV.size() == 4);  // note: assumption is pV.size() == 4
                m_StartWeek = JulianUtility::atot_int32(pV.extract(1, 1, s).toCString(""), bParseError, 1); // (always len = 1)
                m_StartDay = Recurrence::stringToDay(pV.extract(2, 2, s), bParseError); // SU, MO, TU, etc. (always len = 2)

                if (pV[(TextOffset) 0] == '-')
                    m_StartWeek = 0 - m_StartWeek;
                else if (pV[(TextOffset) 0] == '+') { }
                else { bParseError = TRUE; }
                
                // ugly way to handle sign handling
                //s = pV.extract(0, 1, s);
                //if (s == "-") m_StartWeek = - m_StartWeek;
                //else if (s == "+") {}
                //else { bParseError = TRUE; }
            }
        }
        else if (pN.compareIgnoreCase(JulianKeyword::Instance()->ms_sUNTIL) == 0)
        {
            m_Until.setTimeString(pV);
        }
    }
    ICalProperty::deleteICalParameterVector(parameters);
    delete parameters; parameters = 0; 
    // if assertion failed return FALSE.
    if(!(m_StartMonth != -1 && m_StartDay != -1 && m_StartWeek != 0))
    {
        return FALSE;
    }
    if (bParseError || FAILURE(status))
    {
        // BAD RRULE was parsed
        m_StartMonth = -1;
        m_StartDay = -1;
        m_StartWeek = 0;
        return FALSE;
    }
    return TRUE;
}
//---------------------------------------------------------------------
float TZPart::UTCOffsetToFloat(UnicodeString & utcOffset)
{
    t_int32 hour = 0, minute = 0;
    if (DateTime::IsParseableUTCOffset(utcOffset, hour, minute))
    {
        return (float) (hour + (minute / 60));
    }
    PR_ASSERT(FALSE); // all calls should return from above return statement 
    return 0;
}

//---------------------------------------------------------------------
void TZPart::deleteTZPartVector(JulianPtrArray * tzpartVector)
{
    t_int32 i;
    TZPart * ip;
    if (tzpartVector != 0)
    {
        for (i = tzpartVector->GetSize() - 1; i >= 0; i--)
        {
            ip = (TZPart *) tzpartVector->GetAt(i);
            delete ip; ip = 0;
        }
    }
}
//---------------------------------------------------------------------

//Comment
void TZPart::addComment(UnicodeString s, JulianPtrArray * parameters)
{
    //ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
    //        new UnicodeString(s), 0);

    ICalProperty * prop = 
            ICalPropertyFactory::Make(ICalProperty::TEXT, 
            (void *) &s, parameters);
    
    addCommentProperty(prop);
}
void TZPart::addCommentProperty(ICalProperty * prop)
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
//TZName
void TZPart::addTZName(UnicodeString s, JulianPtrArray * parameters)
{
    //ICalProperty * prop = ICalPropertyFactory::Make(ICalProperty::TEXT,
    //        new UnicodeString(s), 0);

    ICalProperty * prop = 
            ICalPropertyFactory::Make(ICalProperty::TEXT, 
            (void *) &s, parameters);

    addTZNameProperty(prop);
}
void TZPart::addTZNameProperty(ICalProperty * prop)
{
    if (m_TZNameVctr == 0)
        m_TZNameVctr = new JulianPtrArray();    
    PR_ASSERT(m_TZNameVctr != 0);
    if (m_TZNameVctr != 0)
    {
       m_TZNameVctr->Add(prop);
    }
}
//---------------------------------------------------------------------
// DTStart
void TZPart::setDTStart(DateTime s, JulianPtrArray * parameters)
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

DateTime TZPart::getDTStart() const
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
// RDate
void TZPart::setRDate(DateTime s, JulianPtrArray * parameters)
{ 
    if (m_RDate == 0)
        m_RDate = ICalPropertyFactory::Make(ICalProperty::DATETIME, 
                                            (void *) &s, parameters);
    else
    {
        m_RDate->setValue((void *) &s);
        m_RDate->setParameters(parameters);
    }
    parseRDate();
}

DateTime TZPart::getRDate() const
{
    DateTime d(-1);
    if (m_RDate == 0)
        return d; //return 0;
    else
    {
        d = *((DateTime *) m_RDate->getValue());
        return d;
    }
}
//---------------------------------------------------------------------
//RRule
void TZPart::setRRule(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_RRule == 0)
        m_RRule = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_RRule->setValue((void *) &s);
        m_RRule->setParameters(parameters);
    }
    parseRRule();
}
UnicodeString TZPart::getRRule() const 
{
    UnicodeString u;
    if (m_RRule == 0)
        return "";
    else
    {
        u = *((UnicodeString *) m_RRule->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
//TZOffsetTo
void TZPart::setTZOffsetTo(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_TZOffsetTo == 0)
        m_TZOffsetTo = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_TZOffsetTo->setValue((void *) &s);
        m_TZOffsetTo->setParameters(parameters);
    }    
}
UnicodeString TZPart::getTZOffsetTo() const 
{
    UnicodeString u;
    if (m_TZOffsetTo == 0)
        return "";
    else
    {
        u = *((UnicodeString *) m_TZOffsetTo->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
//TZOffsetFrom
void TZPart::setTZOffsetFrom(UnicodeString s, JulianPtrArray * parameters)
{
    //UnicodeString * s_ptr = new UnicodeString(s);
    //PR_ASSERT(s_ptr != 0);

    if (m_TZOffsetFrom == 0)
        m_TZOffsetFrom = ICalPropertyFactory::Make(ICalProperty::TEXT, 
                                            (void *) &s, parameters);
    else
    {
        m_TZOffsetFrom->setValue((void *) &s);
        m_TZOffsetFrom->setParameters(parameters);
    }    
}
UnicodeString TZPart::getTZOffsetFrom() const 
{
    UnicodeString u;
    if (m_TZOffsetFrom == 0)
        return "";
    else
    {
        u = *((UnicodeString *) m_TZOffsetFrom->getValue());
        return u;
    }
}
//---------------------------------------------------------------------
// XTOKENS
void TZPart::addXTokens(UnicodeString s)         
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



