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

// dprprty.cpp
// John Sun
// 3:15 PM February 12 1998

#include "stdafx.h"
#include "jdefines.h"

#include "dprprty.h"
#include "datetime.h"
#include "prprty.h"

//---------------------------------------------------------------------
// private never use

DateTimeProperty::DateTimeProperty():
m_DateTime(-1)
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

DateTimeProperty::DateTimeProperty(const DateTimeProperty & that)
{
    setParameters(that.m_vParameters);
    m_DateTime = that.m_DateTime;
}

//---------------------------------------------------------------------

DateTimeProperty::DateTimeProperty(DateTime value, JulianPtrArray * parameters)
: StandardProperty(parameters)
{
    //PR_ASSERT(value != 0);
    //m_string = *((UnicodeString *) value);
    m_DateTime = value;
}
//---------------------------------------------------------------------

DateTimeProperty::~DateTimeProperty()
{
    //delete m_DateTime;
    //deleteICalParameterVector(m_vParameters);
    //delete m_vParameters; m_vParameters = 0;
}

//---------------------------------------------------------------------

void * DateTimeProperty::getValue() const
{
    return (void *) &m_DateTime;
}

//---------------------------------------------------------------------
    
void DateTimeProperty::setValue(void * value)
{
    PR_ASSERT(value != 0);
    if (value != 0)
    {
        m_DateTime = *((DateTime *) value);
    }
}

//---------------------------------------------------------------------

ICalProperty * DateTimeProperty::clone(JLog * initLog)
{
    if (initLog) {} // NOTE: Remove later to avoid warnings
    return new DateTimeProperty(*this);   
}

//---------------------------------------------------------------------

t_bool DateTimeProperty::isValid()
{
    //if (m_DateTime == 0)
    //    return FALSE;
    //return m_DateTime->isValid();
    return m_DateTime.isValid();
}

//---------------------------------------------------------------------

UnicodeString & DateTimeProperty::toString(UnicodeString & out)
{

    //PR_ASSERT(m_DateTime != 0);

    //if (m_DateTime->isValid()
        out = m_DateTime.toString();
    //else
    //    out = "NULL";
    return out;
}
//---------------------------------------------------------------------

UnicodeString & 
DateTimeProperty::toString(UnicodeString & dateFmt, UnicodeString & out)
{
    out = m_DateTime.strftime(dateFmt);
    if (out.size() > 0)
        return out;
    else 
        return toString(out);
}

//---------------------------------------------------------------------

UnicodeString & DateTimeProperty::toExportString(UnicodeString & out)
{
    //PR_ASSERT(m_DateTime != 0);
    out = m_DateTime.toISO8601();
    return out;
}
//---------------------------------------------------------------------

