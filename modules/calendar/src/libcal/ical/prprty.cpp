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

// prprty.cpp
// John Sun
// 4:11 PM February 10 1998

#include "stdafx.h"
#include "jdefines.h"

#include <unistring.h>
#include <unicode.h>
#include "unistrto.h"
#include "prprty.h"
#include "ptrarray.h"
#include "icalprm.h"
#include "period.h"
#include "jutility.h"
#include "keyword.h"

//---------------------------------------------------------------------
//static UnicodeString s_sDelim = ",";
//static UnicodeString s_sCommaSymbol = ",";
//static UnicodeString s_sColonSymbol = ":";
//static UnicodeString s_sSemiColonSymbol = ";";
//static UnicodeString s_sEqualSymbol = "=";
//static UnicodeString s_sQuoteSymbol = "\"";
//static UnicodeString s_sLineBreak = "\r\n";
//static UnicodeString s_sLineBreakSpace = "\r\n ";
//static UnicodeString s_sLineFeedSpace = "\n ";
//static UnicodeString s_sSmallXDash = "x-";
//static UnicodeString s_sBigXDash= "X-";
//static UnicodeString s_sAltRep = ";ALTREP=\"";
//static UnicodeString s_sSendBy = ";SENT-BY=\"";

// contact, comment, description, location
/*
JAtom ICalProperty::ms_asAltrepLanguageParamRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_ALTREP, JulianKeyword::Instance()->ms_ATOM_LANGUAGE
};
const t_int32 ICalProperty::ms_asAltrepLanguageParamRangeSize = 2;

// exdate, rdate, dtend, due, dtstart
JAtom ICalProperty::ms_asTZIDValueParamRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_VALUE, JulianKeyword::Instance()->ms_ATOM_TZID
};
const t_int32 ICalProperty::ms_asTZIDValueParamRangeSize = 2;

// categories, request-status, resources, tzname
JAtom ICalProperty::ms_asLanguageParamRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_LANGUAGE
};
const t_int32 ICalProperty::ms_asLanguageParamRangeSize = 1;

// attach
JAtom ICalProperty::ms_asEncodingValueParamRange[] = 
{
    JulianKeyword::Instance()->ms_ATOM_ENCODING, JulianKeyword::Instance()->ms_ATOM_VALUE
};
const t_int32 ICalProperty::ms_asEncodingValueParamRangeSize = 2;

// organizer
JAtom ICalProperty::ms_asSentByParamRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_SENTBY
};
const t_int32 ICalProperty::ms_asSentByParamRangeSize = 1;

// related-to
JAtom ICalProperty::ms_asReltypeParamRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_RELTYPE
};
const t_int32 ICalProperty::ms_asReltypeParamRangeSize = 1;

// trigger
JAtom ICalProperty::ms_asRelatedValueParamRange[] = 
{
    JulianKeyword::Instance()->ms_ATOM_VALUE, JulianKeyword::Instance()->ms_ATOM_RELATED
};
const t_int32 ICalProperty::ms_asRelatedValueParamRangeSize = 2;

//---------------------------------------------------------------------
///////// PARAMETER VALUE RANGES /////////////
JAtom ICalProperty::ms_asBinaryURIValueRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_BINARY, JulianKeyword::Instance()->ms_ATOM_URI
};
const t_int32 ICalProperty::ms_asBinaryURIValueRangeSize = 2;


JAtom ICalProperty::ms_asDateDateTimeValueRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_DATE, JulianKeyword::Instance()->ms_ATOM_DATETIME
};
const t_int32 ICalProperty::ms_asDateDateTimeValueRangeSize = 2;

JAtom ICalProperty::ms_asDurationDateTimeValueRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_DURATION, JulianKeyword::Instance()->ms_ATOM_DATETIME
};
const t_int32 ICalProperty::ms_asDurationDateTimeValueRangeSize = 2;

JAtom ICalProperty::ms_asDateDateTimePeriodValueRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_DATE, JulianKeyword::Instance()->ms_ATOM_DATETIME, JulianKeyword::Instance()->ms_ATOM_PERIOD
};
const t_int32 ICalProperty::ms_asDateDateTimePeriodValueRangeSize = 3;

static JAtom s_asRelTypeRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_PARENT, JulianKeyword::Instance()->ms_ATOM_CHILD, JulianKeyword::Instance()->ms_ATOM_SIBLING
};

static JAtom s_asRelatedRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_START, JulianKeyword::Instance()->ms_ATOM_END
};

static JAtom s_asParameterRange[] = 
{ 
    JulianKeyword::Instance()->ms_ATOM_ALTREP, JulianKeyword::Instance()->ms_ATOM_ENCODING, JulianKeyword::Instance()->ms_ATOM_LANGUAGE, JulianKeyword::Instance()->ms_ATOM_TZID,
        JulianKeyword::Instance()->ms_ATOM_VALUE        
};
static s_iParameterRangeSize = 5;

static JAtom s_asIrregularProperties[] = 
{
    JulianKeyword::Instance()->ms_ATOM_ATTENDEE, JulianKeyword::Instance()->ms_ATOM_FREEBUSY, JulianKeyword::Instance()->ms_ATOM_RECURRENCEID
};
static s_iIrregularPropertiesSize = 3;

static JAtom s_asValueRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_BINARY, JulianKeyword::Instance()->ms_ATOM_BOOLEAN, JulianKeyword::Instance()->ms_ATOM_CALADDRESS, JulianKeyword::Instance()->ms_ATOM_DATE,
    JulianKeyword::Instance()->ms_ATOM_DATETIME, JulianKeyword::Instance()->ms_ATOM_DURATION, JulianKeyword::Instance()->ms_ATOM_FLOAT, JulianKeyword::Instance()->ms_ATOM_INTEGER,
    JulianKeyword::Instance()->ms_ATOM_PERIOD, JulianKeyword::Instance()->ms_ATOM_RECUR, JulianKeyword::Instance()->ms_ATOM_TEXT, JulianKeyword::Instance()->ms_ATOM_TIME,
    JulianKeyword::Instance()->ms_ATOM_URI, JulianKeyword::Instance()->ms_ATOM_UTCOFFSET
};
static s_iValueRangeSize = 14;

static JAtom s_asEncodingRange[] =
{
    JulianKeyword::Instance()->ms_ATOM_8bit, JulianKeyword::Instance()->ms_ATOM_B
};
static s_iEncodingRangeSize = 2;
*/
//---------------------------------------------------------------------
/*
ICalProperty::ICalProperty()
: m_vParameters(0)
{
}
*/
//---------------------------------------------------------------------
/*
ICalProperty::ICalProperty(JulianPtrArray * parameters)
: m_vParameters(0)
{
    setParameters(parameters);

   
}
*/
//---------------------------------------------------------------------
/*
ICalProperty::~ICalProperty()
{
    if (m_vParameters != 0)
    {
        deleteICalParameterVector(m_vParameters);
        delete m_vParameters; 
        m_vParameters = 0;
    }
}
*/
//---------------------------------------------------------------------
/*
UnicodeString & 
ICalProperty::getParameterValue(UnicodeString & paramName,
                                UnicodeString & paramValue,
                                ErrorCode & status)
{
    t_int32 i;
    ICalParameter * param;
    UnicodeString into;

    if (m_vParameters == 0)
    {
        status = 1;
        paramValue = "";
        return paramValue;
    }
    else 
    {
        for (i = 0; i < m_vParameters->GetSize(); i++)
        {
            param = (ICalParameter *) m_vParameters->GetAt(i);
            if (param->getParameterName(into).compareIgnoreCase(paramName) == 0)
            {
                // a match
                paramValue = param->getParameterValue(into);
                return paramValue;
            }
        }
        status = 2;
        paramValue = "";
        return paramValue;
    }
}
*/
//---------------------------------------------------------------------
/*
void ICalProperty::addParameter(ICalParameter * aParameter)
{
    // TODO: check for duplicates?
    m_vParameters->Add(aParameter);
}
*/
//-------------------------------------------------
/*
void ICalProperty::parse(UnicodeString & in)
{
    UnicodeString name, value;

    if (m_vParameters != 0) {
        ICalProperty::deleteICalParameterVector(m_vParameters);
        delete m_vParameters;
    }
    m_vParameters = 0;
    
    JulianPtrArray * parameters = new JulianPtrArray(); 
    PR_ASSERT(parameters != 0);
    
    // parse in a a line, it will fill in vector
    parsePropertyLine(in, name, value, parameters);
    // TODO: fix
    //PR_ASSERT(m_vParameters != 0 && m_vParameters.GetSize() == 2);

    setValue((void *) m_vParameters->GetAt(1));
    setParameters(parameters);

    ICalProperty::deleteICalParameterVector(parameters);
    delete parameters; parameters = 0;
}
*/
//---------------------------------------------------------------------
/*
void ICalProperty::setParameters(JulianPtrArray * parameters)
{
    if (m_vParameters != 0)
    {
        ICalProperty::deleteICalParameterVector(m_vParameters);
        delete m_vParameters; m_vParameters = 0;
    }

    // NOTE: I am making a copy of the parameters
    // I am not storing the ptr.
    if (parameters != 0)
    {
        t_int32 i;
        ICalParameter * ip, * ip2;
        m_vParameters = new JulianPtrArray();

        for (i = 0; i < parameters->GetSize(); i++)
        {
            ip = (ICalParameter *) parameters->GetAt(i);
            PR_ASSERT(ip != 0);
            ip2 = ip->clone();
            PR_ASSERT(ip2 != 0);
            m_vParameters->Add(ip2);
        }
    }
   
    //m_vParameters = parameters; // OLD WAY: just storing ptr.
}
*/
//---------------------------------------------------------------------
/*
UnicodeString & 
ICalProperty::toICALString(UnicodeString & out) 
{
    UnicodeString u, name;
    u = toExportString(u);
    //if (FALSE) TRACE("u = --%s--\r\n", u.toCString(""));
    return propertyToICALString(name, u, m_vParameters, out);
}
*/
//---------------------------------------------------------------------
/*
UnicodeString & 
ICalProperty::toICALString(UnicodeString & sProp,
                           UnicodeString & out) 
{
    UnicodeString u;
    u = toExportString(u);
    //if (FALSE) TRACE("u = --%s--\r\n", u.toCString(""));
    return propertyToICALString(sProp, u, m_vParameters, out);
}
*/
//---------------------------------------------------------------------

ICalProperty::ICalProperty() {}
ICalProperty::ICalProperty(ICalProperty & that) { if (&that != 0) {} }

//---------------------------------------------------------------------

UnicodeString &
ICalProperty::propertyToICALString(UnicodeString & sProp, 
                                   UnicodeString & sVal,
                                   JulianPtrArray * parameters,
                                   UnicodeString & retVal)
{
    //UnicodeString sName, sVal;
    ICalParameter * aName;
    retVal = "";
    t_int32 size = 0;
    t_int32 i;


    if (parameters != 0)
        size = parameters->GetSize();
    if (sVal.size() > 0)
    {
        UnicodeString u;
        retVal = sProp;
        for (i = 0; i < size; i++)
        {
            aName = (ICalParameter *) parameters->GetAt(i);
            retVal += aName->toICALString(u);
        }
        retVal += ':'; retVal += sVal; retVal += JulianKeyword::Instance()->ms_sLINEBREAK;
    }
    //if (FALSE) TRACE("retVal = %s\r\n", retVal.toCString(""));
    return retVal;
}

//---------------------------------------------------------------------
// TRUE = in range, FALSE = out of range.
t_bool ICalProperty::CheckParams(JulianPtrArray * parameters,
                                 JAtom validParamNameRange[], 
                                 t_int32 validParamNameRangeSize)
{
    if (parameters == 0 || parameters->GetSize() == 0)
        return TRUE;

    // more parameters that range
    if (parameters->GetSize() > validParamNameRangeSize)
        return FALSE;

    UnicodeString u;
    t_int32 i;
    ICalParameter * ip;

    for (i = 0; i < parameters->GetSize(); i++)
    {
        ip = (ICalParameter *) parameters->GetAt(i);
        u = ip->getParameterName(u);

        //if (FALSE) TRACE("u = %s\r\n", u.toCString(""));

        if (!JulianUtility::checkRange(u.hashCode(), validParamNameRange, 
            validParamNameRangeSize))
        {
            return FALSE;
        }
    }
    return TRUE;
}
//---------------------------------------------------------------------
// TRUE = in range, FALSE = out of range.
t_bool ICalProperty::CheckParams(JulianPtrArray * parameters,
                                 JAtom validParamNameRange[], 
                                 t_int32 validParamNameRangeSize,
                                 JAtom validValueRange[],
                                 t_int32 validValueRangeSize)
{
   
    
    // no parameters
    if (parameters == 0 || parameters->GetSize() == 0)
        return TRUE;

    // more parameters that range
    if (parameters->GetSize() > validParamNameRangeSize)
        return FALSE;
    
    UnicodeString u;
    ICalParameter * ip;
    t_int32 i;

    for (i = 0; i < parameters->GetSize(); i++)
    {
        ip = (ICalParameter *) parameters->GetAt(i);
        u = ip->getParameterName(u);

        //if (FALSE) TRACE("u = %s\r\n", u.toCString(""));

        if (!JulianUtility::checkRange(u.hashCode(), validParamNameRange, 
            validParamNameRangeSize))
        {
            return FALSE;
        }

        if (JulianKeyword::Instance()->ms_ATOM_VALUE == u.hashCode()) // ONLY check if VALUE
        {
            u = ip->getParameterValue(u);
            if (!JulianUtility::checkRange(u.hashCode(), validValueRange,
                validValueRangeSize))
            {
                return FALSE;
            }
        }
    }
    return TRUE;
}

//---------------------------------------------------------------------

// return TRUE if error, FALSE otherwise
t_bool ICalProperty::checkParam(UnicodeString & propName, 
                                UnicodeString & paramName,
                                UnicodeString & paramVal)
{
    t_int32 hashCode = propName.hashCode();
    
    if (JulianUtility::checkRange(hashCode, 
        JulianAtomRange::Instance()->ms_asIrregularProperties,
        JulianAtomRange::Instance()->ms_iIrregularPropertiesSize))
    {
        // if special property, then don't check parameters
        return FALSE;
    }
    else
    {
        t_int32 pNHC = paramName.hashCode();
        
        // X-TOKEN, no error
        if (IsXToken(paramName))
            return FALSE;

        // not a valid parameter for th
        if (!JulianUtility::checkRange(pNHC, 
            JulianAtomRange::Instance()->ms_asParameterRange, 
            JulianAtomRange::Instance()->ms_iParameterRangeSize))
            return TRUE;
        
        else
        {
            t_int32 pVHC = paramVal.hashCode();
            // check parameter name ranges here
            // TODO: For now check only VALUE, ENCODING, 
            // TODO: Check LANGUAGE later
            if (JulianKeyword::Instance()->ms_ATOM_VALUE == pNHC)
            {
                if ((!JulianUtility::checkRange(pVHC, 
                    JulianAtomRange::Instance()->ms_asValueRange, 
                    JulianAtomRange::Instance()->ms_iValueRangeSize)) 
                    && (!IsXToken(paramVal)))
                    return TRUE;
            }
            else if (JulianKeyword::Instance()->ms_ATOM_ENCODING == pNHC)
            {
                if ((!JulianUtility::checkRange(pVHC, 
                    JulianAtomRange::Instance()->ms_asEncodingRange, 
                    JulianAtomRange::Instance()->ms_iEncodingRangeSize)) && (!IsXToken(paramVal)))
                    return TRUE;
            }
            return FALSE;
        }
    }
}
//---------------------------------------------------------------------
// checks for parameters here.
// If method encounters a bad parameter, 
// then method won't add it to the parameters vector,
// and set parseError to TRUE

void ICalProperty::parsePropertyLine(UnicodeString & strLine,
                                     UnicodeString & outName, 
                                     UnicodeString & outVal,
                                     JulianPtrArray * parameters)
{
    ErrorCode status = ZERO_ERROR;

    t_int32 lineLength = strLine.size();
    // NOTE: won't work with backslashed :, ;.  Spec doesn't
    // mention any special handling for backslashed :, ;.
    t_int32 iColon = strLine.indexOf(':');
    t_int32 iSColon = strLine.indexOf(';');
    t_int32 iDQuote = strLine.indexOf('\"');
    t_int32 i, iEDQuote;

    //if (FALSE) TRACE("strLine = %s\r\n", strLine.toCString(""));

    // Ignore colons, semicolons inside double quotes
    while (iDQuote >= 0 && iColon > iDQuote)
    {
        // get second quote
        iEDQuote = strLine.indexOf('\"', iDQuote + 1);
        if (iEDQuote > 0 && iColon < iEDQuote)
        {
            i = strLine.indexOf(':', iEDQuote);
            iColon = ((i != -1) ? i : iColon);
        }
        if (iEDQuote > 0)
        {
            iDQuote = strLine.indexOf('\"', iEDQuote + 1);
        }
        else
        {
            iDQuote = -1;
        }
    }

    // no parameters
    if (iSColon < 0)
    {
        if (iColon >= 0)
        {
            outName = strLine.extractBetween(0, iColon, outName).toUpper();
            //outName = strLine.extractBetween(0, iColon, outName);
            //outName = JulianUtility::ToUpper(outName);

            outVal = strLine.extractBetween(iColon + 1, lineLength, outVal);
        }
        else
        {   
            outName = strLine.toUpper();
            //outName = strLine;
            //outName = JulianUtility::ToUpper(outName);

            outVal = "";      
        }
    }
    else 
    {
        // has parameters
        UnicodeString u;
        t_int32 iIndex = -1; 
        
        if (iColon > 0)
        {
            iIndex = (iColon < iSColon) ? iColon : iSColon;
            
            outName = strLine.extractBetween(0, iIndex, u).toUpper();
            //outName = strLine.extractBetween(0, iIndex, u);
            //outName = JulianUtility::ToUpper(outName);

            outVal = strLine.extractBetween(iColon + 1, lineLength, u);
        }
        if (iIndex < iColon)
            u = strLine.extractBetween(iIndex + 1, iColon, u);
        else
            u = "";

        //if (FALSE) TRACE("u = %s\r\n", u.toCString("")); // u = sParams now.
        if (u.size() > 0)
        {
            // NOTE: TODO: This assumes semicolons can't exist between DQUOTE
            UnicodeStringTokenizer * st = new UnicodeStringTokenizer(u, 
                JulianKeyword::Instance()->ms_sSEMICOLON_SYMBOL);
            PR_ASSERT(st != 0);
            PR_ASSERT(parameters != 0);

            if (st != 0 && parameters != 0)
            {
                UnicodeString paramName;
                UnicodeString paramVal;
                t_int32 iEq = -1;
            
                //t_bool error;
                //t_int32 paramsLen = -1;

                while (st->hasMoreTokens())
                {
                    u = st->nextToken(u, status);
                    u.trim();        

                    //if (FALSE) TRACE("u = %s\r\n", u.toCString(""));

                    iEq = u.indexOf('=');
                    if (iEq < 0)
                    {
                        paramName = u.extractBetween(0, u.size(), paramName).toUpper();
                        //paramName = u.extractBetween(0, u.size(), paramName);
                        //paramName = JulianUtility::ToUpper(paramName);

                        paramVal = "";
                    }
                    else
                    {
                        paramName = u.extractBetween(0, iEq, paramName).toUpper();
                        //paramName = u.extractBetween(0, u.size(), paramName);
                        //paramName = JulianUtility::ToUpper(paramName);
    
                        paramVal = u.extractBetween(iEq + 1, u.size(), paramVal);
                    }
    
                    //if (FALSE) TRACE("paramName = %s\r\n", paramName.toCString(""));
                    //if (FALSE) TRACE("paramVal = %s\r\n", paramVal.toCString(""));
    
                    // checking parameters here 
                    // NOTE: moved error checking to the storeData method
                    //error = checkParam(outName, paramName, paramVal);
                    //if (!error)
                    //{
                        parameters->Add(new ICalParameter(paramName, paramVal));
                    //}
                    //else
                    //{
                    //    parseError = TRUE;
                    //}
                }
                delete st; st = 0;
            }
        }
    }
    //if (FALSE) TRACE("outName = --%s--, outVal = --%s--, paramSize = %d\r\n", outName.toCString(""), outVal.toCString(""), parameters->GetSize());

    //return parseError;
    //return FALSE;
}

//---------------------------------------------------------------------

void 
ICalProperty::deleteICalParameterVector(JulianPtrArray * parameters)
{
    ICalParameter * ip;
    t_int32 i;
    if (parameters != 0)
    {
        for (i = parameters->GetSize() - 1; i >= 0; i--)
        {
            ip = (ICalParameter *) parameters->GetAt(i);
            delete ip; ip = 0;
        }
    }
}

//---------------------------------------------------------------------

// Trims UnicodeStrings.  The UnicodeString::trim() method seems to 
// only trim space separators (ASCII 12 and ' ').  It doesn't trim
// ('\r' ASCII 13).  Note assumes only 8-bit string
UnicodeString &
ICalProperty::Trim(UnicodeString & s)
{
    TextOffset endOffset = s.size();
    TextOffset startOffset = 0;
    while ((startOffset < endOffset) &&
            (s[(TextOffset) startOffset] == ' ' ||
             s[(TextOffset) startOffset] == '\t' ||
             s[(TextOffset) startOffset] == '\r' ||
             s[(TextOffset) startOffset] == '\n'))
    {
        startOffset++;
    }
    while ((startOffset < endOffset) &&
            (s[(TextOffset) (endOffset - 1)] == ' ' ||
             s[(TextOffset) (endOffset - 1)] == '\t' ||
             s[(TextOffset) (endOffset - 1)] == '\r' ||
             s[(TextOffset) (endOffset - 1)] == '\n'))
    {
        endOffset--;
    }
    if ((startOffset > 0) || (endOffset < s.size()))
        s.extractBetween(startOffset, endOffset, s);
    return s;
}

//---------------------------------------------------------------------

void 
ICalProperty::deleteICalPropertyVector(JulianPtrArray * properties)
{
    ICalProperty * ip;
    t_int32 i;
    if (properties != 0)
    {
        for (i = properties->GetSize() - 1; i >= 0; i--)
        {
            ip = (ICalProperty *) properties->GetAt(i);
            delete ip; ip = 0;
        }
    }
}
//---------------------------------------------------------------------
void 
ICalProperty::deletePeriodVector(JulianPtrArray * periods)
{
    Period * ip;
    t_int32 i;
    if (periods != 0)
    {
        for (i = periods->GetSize() - 1; i >= 0; i--)
        {
            ip = (Period *) periods->GetAt(i);
            delete ip; ip = 0;
        }
    }
}
//---------------------------------------------------------------------
UnicodeString &
ICalProperty::propertyToString(ICalProperty * property, 
                               UnicodeString & dateFmt,
                               UnicodeString & retVal)
{
    if (property == 0)
        retVal = "";
    else
    {
        retVal = property->toString(dateFmt, retVal);
    }
    return retVal;
}

//---------------------------------------------------------------------

UnicodeString &
ICalProperty::propertyToString(ICalProperty * property, 
                               UnicodeString & retVal)
{
    if (property == 0)
        retVal = "";
    else
    {
        retVal = property->toString(retVal);
    }
    return retVal;
}
//---------------------------------------------------------------------
UnicodeString &
ICalProperty::propertyVectorToString(JulianPtrArray * properties, 
                                     UnicodeString & retVal)
{
    retVal = "";
    if (properties == 0)
    {
    }
    else
    {
        UnicodeString u;
        ICalProperty * prop;
        t_int32 i;

        for (i = 0; i < properties->GetSize(); i++)
        {
            prop = (ICalProperty *) properties->GetAt(i);
            PR_ASSERT(prop != 0);
            if (prop != 0)
            {
                retVal += prop->toString(u);
                if (i < properties->GetSize() - 1)
                    retVal += ','; //ms_sCOMMA_SYMBOL;
            }
        }
    }
    return retVal;
}
//---------------------------------------------------------------------
UnicodeString &
ICalProperty::propertyVectorToString(JulianPtrArray * properties, 
                                     UnicodeString & dateFmt, 
                                     UnicodeString & retVal)
{
    
    retVal = "";
    if (properties == 0)
    {
    }
    else
    {
        t_int32 i;
        UnicodeString u;
        ICalProperty * prop;
        
        for (i = 0; i < properties->GetSize(); i++)
        {
            prop = (ICalProperty *) properties->GetAt(i);
            PR_ASSERT(prop != 0);
            if (prop != 0)
            {
                retVal += prop->toString(dateFmt, u);
    
                if (i < properties->GetSize() - 1)
                    retVal += ','; //ms_sCOMMA_SYMBOL;
            }
        }
    }
    return retVal;
}
//---------------------------------------------------------------------

UnicodeString &
ICalProperty::parameterToCalString(UnicodeString sParam, 
                                   UnicodeString & sVal,
                                   UnicodeString & out) 
{
    out = "";
    if (sVal.size() > 0) 
    {
        out = JulianKeyword::Instance()->ms_sSEMICOLON_SYMBOL; 
        out += sParam; 
        out += '='; 
        out += sVal;
    }
    return out;
}

//---------------------------------------------------------------------


UnicodeString &
ICalProperty::parameterVToCalString(UnicodeString sParam, 
                                    JulianPtrArray * v,
                                    UnicodeString & out) 
{
    UnicodeString ss;
    t_int32 i = 0;
    out = "";
    if (v != 0) {

        for (i = 0; i < v->GetSize(); i++)
        {
            ss = *((UnicodeString *) v->GetAt(i));
            if (i == 0)
            {
                out += ';'; out += sParam; 
                out += '='; out += ss;
            }
            else
            {
                out += ';'; out += ss;
            }
        }
    }
    return out;
}

//---------------------------------------------------------------------
UnicodeString &
ICalProperty::propertyToICALString(UnicodeString & propName,
                                   ICalProperty * property, 
                                   UnicodeString & retVal)
{
    if (property == 0)
        retVal = "";
    else
    {
        UnicodeString out;
        out = property->toICALString(propName, retVal);
        retVal = multiLineFormat(retVal);
        //retVal = multiLineFormat(property->toICALString(propName, retVal));
    }
    return retVal;
}
//---------------------------------------------------------------------
UnicodeString &
ICalProperty::propertyVectorToICALString(UnicodeString & propName,
                                         JulianPtrArray * properties, 
                                         UnicodeString & retVal)
{
    t_int32 i;
    UnicodeString u;
    ICalProperty * prop;
    retVal = "";
    if (properties == 0)
    {
    }
    else
    {
        //retVal = "";
        for (i = 0; i < properties->GetSize(); i++)
        {
            prop = (ICalProperty *) properties->GetAt(i);
            PR_ASSERT(prop != 0);
            if (prop != 0)
            {
                retVal += multiLineFormat(prop->toICALString(propName, u));
            }
        }
    }
    return retVal;
}
//---------------------------------------------------------------------
UnicodeString &
ICalProperty::vectorToICALString(JulianPtrArray * strings, UnicodeString & retVal)
{
    retVal = "";
    if (strings == 0)
    {
    }
    else
    {
        t_int32 i;
        UnicodeString u;
 
        for (i = 0; i < strings->GetSize(); i++)
        {
            u = *((UnicodeString *) strings->GetAt(i));
            u += JulianKeyword::Instance()->ms_sLINEBREAK;
            retVal += multiLineFormat(u);
        }
    }
    return retVal;
}

//---------------------------------------------------------------------

UnicodeString &
ICalProperty::multiLineFormat(UnicodeString & s) 
{
    t_int32 size = s.size();

    if (size == 0)
      return s;
    if (size <= ms_iMAX_LINE_LENGTH) 
      return s;
    else 
    {
        UnicodeString sResult, t;

        sResult = s.extractBetween(0, ms_iMAX_LINE_LENGTH, sResult);
        t = s.extractBetween(ms_iMAX_LINE_LENGTH, size, t);
        t.insert(0, JulianKeyword::Instance()->ms_sLINEBREAKSPACE);
        sResult += multiLineFormat(t);

        // end recursion
        size = sResult.size();
        if (sResult.endsWith(JulianKeyword::Instance()->ms_sLINEFEEDSPACE))
        sResult.removeBetween(size - 1, size);
        s = sResult;
        return s;
    }
}
   
//---------------------------------------------------------------------
t_bool 
ICalProperty::IsXToken(UnicodeString & s)
{
    if (s.size() == 0)
        return FALSE;
    if (s.size() < 2)
        return FALSE;
    if ((s[(TextOffset) 0] != 'X' && s[(TextOffset) 0] != 'x') ||
        (s[(TextOffset) 1] != '-'))
        return FALSE;
    else 
        return TRUE;
}
//---------------------------------------------------------------------

void 
ICalProperty::CloneICalPropertyVector(JulianPtrArray * propertiesToClone, 
                                      JulianPtrArray * out,
                                      JLog * initLog)
{
    if (out == 0 || propertiesToClone == 0)
        return;
    else
    {
        t_int32 i;
        ICalProperty * ip;
        for (i = 0; i < propertiesToClone->GetSize(); i++)
        {
            ip = ((ICalProperty *) propertiesToClone->GetAt(i))->clone(initLog);
            PR_ASSERT(ip != 0);
            out->Add(ip);
        }
    }
}

//---------------------------------------------------------------------

void 
ICalProperty::CloneUnicodeStringVector(JulianPtrArray * ustringsToClone, 
                                       JulianPtrArray * out)
{
    if (out == 0 || ustringsToClone == 0)
        return;
    else
    {
        t_int32 i;
        UnicodeString * u;
        for (i = 0; i < ustringsToClone->GetSize(); i++)
        {
            u = new UnicodeString(*((UnicodeString *) ustringsToClone->GetAt(i)));
            PR_ASSERT(u != 0);
            out->Add(u);
        }
    }
}
//---------------------------------------------------------------------

