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

// iprprty.cpp
// John Sun
// 3:15 PM February 12 1998

#include "stdafx.h"
#include "jdefines.h"

#include "iprprty.h"
#include "prprty.h"


//---------------------------------------------------------------------
// private never use
IntegerProperty::IntegerProperty()
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

IntegerProperty::IntegerProperty(const IntegerProperty & that)
{
    setParameters(that.m_vParameters);
    m_Integer = that.m_Integer;
}

//---------------------------------------------------------------------

IntegerProperty::IntegerProperty(t_int32 value, JulianPtrArray * parameters)
: StandardProperty(parameters)
{
    m_Integer = value;
}

//---------------------------------------------------------------------

IntegerProperty::~IntegerProperty()
{
    //deleteICalParameterVector(m_vParameters);
    //delete m_vParameters; m_vParameters = 0;
}

//---------------------------------------------------------------------

void * IntegerProperty::getValue() const
{
    return (void *) &m_Integer;
}

//---------------------------------------------------------------------
    
void IntegerProperty::setValue(void * value)
{
    PR_ASSERT(value != 0);
    if (value != 0)
    {
        m_Integer = *((t_int32 *) value);
    }
}

//---------------------------------------------------------------------

ICalProperty * IntegerProperty::clone(JLog * initLog)
{
    if (initLog) {} // NOTE: Remove later to avoid warnings
    return new IntegerProperty(*this);   
}

//---------------------------------------------------------------------

t_bool IntegerProperty::isValid()
{
    return TRUE;
}

//---------------------------------------------------------------------

UnicodeString & IntegerProperty::toString(UnicodeString & out)
{
    char sBuf[10];
    sprintf(sBuf, "%d", m_Integer);
    out = sBuf;
    return out;
}

//---------------------------------------------------------------------
UnicodeString & 
IntegerProperty::toString(UnicodeString & dateFmt, UnicodeString & out)
{
    // NOTE: remove it later, gets rid of compiler warning
    if (dateFmt.size() > 0) {}
    return toString(out);
}
//---------------------------------------------------------------------

UnicodeString & IntegerProperty::toExportString(UnicodeString & out)
{
    return toString(out);
}
//---------------------------------------------------------------------


