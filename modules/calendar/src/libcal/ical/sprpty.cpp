/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// sprprty.cpp
// John Sun
// 5:10 PM February 10 1998

#include "stdafx.h"
#include "jdefines.h"

#include "sprprty.h"

//---------------------------------------------------------------------

// private never use
StringProperty::StringProperty()
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

StringProperty::StringProperty(const StringProperty & that)
{
    setParameters(that.m_vParameters);
    m_string = that.m_string;
}

//---------------------------------------------------------------------

//StringProperty::StringProperty(UnicodeString * value, JulianPtrArray * parameters)
StringProperty::StringProperty(UnicodeString value, JulianPtrArray * parameters)
: StandardProperty(parameters)
{
    //PR_ASSERT(value != 0);
    //m_string = *((UnicodeString *) value);
    m_string = value;
}

//---------------------------------------------------------------------

StringProperty::~StringProperty()
{
    //delete m_string;
    ////deleteICalParameterVector(m_vParameters);
    ////delete m_vParameters; m_vParameters = 0;
}

//---------------------------------------------------------------------

void * StringProperty::getValue() const
{
    //return (void *) m_string;
    return (void *) &m_string;
}

//---------------------------------------------------------------------

void StringProperty::setValue(void * value)
{
    PR_ASSERT(value != 0);
    //if (m_string != 0) { delete m_string; m_string = 0; }
    //m_string = ((UnicodeString *) value);
    if (value != 0)
    {
        m_string = *(UnicodeString *) value;
    }
}

//---------------------------------------------------------------------

ICalProperty * StringProperty::clone(JLog * initLog)
{
    // NOTE: Remove later to avoid warnings
    if (initLog) {} 
    return new StringProperty(*this);   
}

//---------------------------------------------------------------------

t_bool StringProperty::isValid()
{
    return TRUE;
}

//---------------------------------------------------------------------

UnicodeString & StringProperty::toString(UnicodeString & out)
{
    //PR_ASSERT(m_string != 0);
    //out = *m_string;
    out = m_string;
    return out;
}

//---------------------------------------------------------------------

UnicodeString & 
StringProperty::toString(UnicodeString & dateFmt, UnicodeString & out)
{
    // NOTE: remove it later, gets rid of compiler warning
    if (dateFmt.size() > 0) {}
    return toString(out);
}

//---------------------------------------------------------------------

UnicodeString & StringProperty::toExportString(UnicodeString & out)
{
    return toString(out);
}
//---------------------------------------------------------------------


