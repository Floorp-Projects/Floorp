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

// sdprprty.cpp
// John Sun
// 4:11 PM February 10 1998

#include "stdafx.h"
#include "jdefines.h"

#include "jlog.h"
#include "sdprprty.h"
#include "keyword.h"
#include <unistring.h>

//---------------------------------------------------------------------
StandardProperty::StandardProperty()
: m_vParameters(0)
{
}
//---------------------------------------------------------------------
StandardProperty::StandardProperty(JulianPtrArray * parameters)
: m_vParameters(0)
{
    setParameters(parameters);
}
//---------------------------------------------------------------------
StandardProperty::~StandardProperty()
{
    if (m_vParameters != 0)
    {
        ICalProperty::deleteICalParameterVector(m_vParameters);
        delete m_vParameters; m_vParameters = 0;
    }
}
//---------------------------------------------------------------------
UnicodeString & 
StandardProperty::getParameterValue(UnicodeString & paramName,
                                    UnicodeString & paramValue,
                                    ErrorCode & status)
{
    t_int32 i;
    ICalParameter * param;
    UnicodeString into;
    status = ZERO_ERROR;
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
//---------------------------------------------------------------------

void StandardProperty::addParameter(ICalParameter * aParameter)
{
    // TODO: check for duplicates?
    m_vParameters->Add(aParameter);
}
//---------------------------------------------------------------------

void StandardProperty::parse(UnicodeString & in)
{
#if 0
    UnicodeString name, value;

    if (m_vParameters != 0) {
        ICalProperty::deleteICalParameterVector(m_vParameters);
        delete m_vParameters;
    }
    m_vParameters = 0;
    
    JulianPtrArray * parameters = new JulianPtrArray(); 
    PR_ASSERT(parameters != 0);
    if (parameters != 0)
    {
        // parse in a a line, it will fill in vector
        parsePropertyLine(in, name, value, parameters);

        // TODO: fix. need to cast value to correct type, but I
        // can't find out type here.
        setValue((void *) m_vParameters->GetAt(1));
        setParameters(parameters);

        ICalProperty::deleteICalParameterVector(parameters);
        delete parameters; parameters = 0;
    }
#else 
    // don't use for now
    PR_ASSERT(FALSE);
#endif /* #if 0 */
}
//---------------------------------------------------------------------

void StandardProperty::setParameters(JulianPtrArray * parameters)
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
        PR_ASSERT(m_vParameters != 0);
        if (m_vParameters != 0)
        {
            for (i = 0; i < parameters->GetSize(); i++)
            {
                ip = (ICalParameter *) parameters->GetAt(i);
                PR_ASSERT(ip != 0);
                if (ip != 0)
                {
                    ip2 = ip->clone();
                    PR_ASSERT(ip2 != 0);
                    if (ip2 != 0)
                    {
                        m_vParameters->Add(ip2);
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------------------

UnicodeString & 
StandardProperty::toICALString(UnicodeString & out) 
{
    UnicodeString u, name;
    u = toExportString(u);
    //if (FALSE) TRACE("u = --%s--\r\n", u.toCString(""));
    return propertyToICALString(name, u, m_vParameters, out);
}

//---------------------------------------------------------------------

UnicodeString & 
StandardProperty::toICALString(UnicodeString & sProp,
                           UnicodeString & out) 
{
    UnicodeString u;
    u = toExportString(u);
    //if (FALSE) TRACE("u = --%s--\r\n", u.toCString(""));
    return propertyToICALString(sProp, u, m_vParameters, out);
}//---------------------------------------------------------------------

