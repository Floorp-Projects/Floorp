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

// duprprty.cpp
// John Sun
// 3:15 PM February 12 1998

#include "stdafx.h"
#include "jdefines.h"

#include "duprprty.h"
#include "duration.h"
#include "prprty.h"

//---------------------------------------------------------------------

// private never use
DurationProperty::DurationProperty()
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

DurationProperty::DurationProperty(const DurationProperty & that)
{
    setParameters(that.m_vParameters);
    m_Duration = that.m_Duration;
}

//---------------------------------------------------------------------

DurationProperty::DurationProperty(Duration value, JulianPtrArray * parameters)
: StandardProperty(parameters)
{
    //PR_ASSERT(value != 0);
    //m_string = *((UnicodeString *) value);
    m_Duration = value;
}
//---------------------------------------------------------------------

DurationProperty::~DurationProperty()
{
    //delete m_Duration;
    //deleteICalParameterVector(m_vParameters);
    //delete m_vParameters; m_vParameters = 0;
}

//---------------------------------------------------------------------

void * DurationProperty::getValue() const
{
    return (void *) &m_Duration;
}

//---------------------------------------------------------------------
    
void DurationProperty::setValue(void * value)
{
    PR_ASSERT(value != 0);
    if (value != 0)
    {
        m_Duration = *((Duration *) value);
    }
}

//---------------------------------------------------------------------

ICalProperty * DurationProperty::clone(JLog * initLog)
{
    if (initLog) {} // NOTE: Remove later to avoid warnings
    return new DurationProperty(*this);   
}

//---------------------------------------------------------------------

t_bool DurationProperty::isValid()
{
    //if (m_Duration == 0)
    //    return FALSE;
    //return m_Duration->isValid();
    return m_Duration.isValid();
}

//---------------------------------------------------------------------

UnicodeString & DurationProperty::toString(UnicodeString & out)
{

    //PR_ASSERT(m_Duration != 0);

    //if (m_Duration->isValid()
        out = m_Duration.toString();
    //else
    //    out = "NULL";
    return out;
}

//---------------------------------------------------------------------

UnicodeString & 
DurationProperty::toString(UnicodeString & dateFmt, UnicodeString & out)
{
    // NOTE: remove it later, gets rid of compiler warning
    if (dateFmt.size() > 0) {}
    return toString(out);
}
//---------------------------------------------------------------------

UnicodeString & DurationProperty::toExportString(UnicodeString & out)
{
    //PR_ASSERT(m_Duration != 0);
    out = m_Duration.toICALString();
    return out;
}
//---------------------------------------------------------------------


