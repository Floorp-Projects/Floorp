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

// pprprty.cpp
// John Sun
// 3:15 PM February 12 1998

#include "stdafx.h"
#include "jdefines.h"

#include "pprprty.h"
#include "period.h"
#include "prprty.h"

//---------------------------------------------------------------------

// private never use
PeriodProperty::PeriodProperty()
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

PeriodProperty::PeriodProperty(const PeriodProperty & that)
{
    setParameters(that.m_vParameters);
    m_Period = that.m_Period;
}

//---------------------------------------------------------------------

PeriodProperty::PeriodProperty(Period * value, JulianPtrArray * parameters)
: StandardProperty(parameters)
{
    PR_ASSERT(value != 0);
    //m_string = *((UnicodeString *) value);
    if (value != 0)
    {
        m_Period = value;
    }
}
//---------------------------------------------------------------------

PeriodProperty::~PeriodProperty()
{
    delete m_Period; m_Period = 0;
    //deleteICalParameterVector(m_vParameters);
    //delete m_vParameters; m_vParameters = 0;
}

//---------------------------------------------------------------------

void * PeriodProperty::getValue() const
{
    return (void *) m_Period;
}

//---------------------------------------------------------------------
    
void PeriodProperty::setValue(void * value)
{
    PR_ASSERT(value != 0);
    // NOTE: deleting old period
    if (value != 0)
    {
        if (m_Period != 0) { delete m_Period; m_Period = 0; }
        m_Period = ((Period *) value);
    }
}

//---------------------------------------------------------------------

ICalProperty * PeriodProperty::clone(JLog * initLog)
{
    if (initLog) {} // NOTE: Remove later to avoid warnings
    return new PeriodProperty(*this);   
}

//---------------------------------------------------------------------

t_bool PeriodProperty::isValid()
{
    if (m_Period == 0)
        return FALSE;
    return m_Period->isValid();
}

//---------------------------------------------------------------------

UnicodeString & PeriodProperty::toString(UnicodeString & out)
{

    PR_ASSERT(m_Period != 0);

    if (m_Period != 0)
    {
        //if (m_Period->isValid()
        out = m_Period->toString();
        //else
        //    out = "NULL";
    }
    else
    {
        // Bad period return "".
        out = "";
    }
    return out;
}

//---------------------------------------------------------------------

UnicodeString & 
PeriodProperty::toString(UnicodeString & dateFmt, UnicodeString & out)
{
    // NOTE: remove it later, gets rid of compiler warning
    if (dateFmt.size() > 0) {}
    return toString(out);
}

//---------------------------------------------------------------------

UnicodeString & PeriodProperty::toExportString(UnicodeString & out)
{
    PR_ASSERT(m_Period != 0);
    if (m_Period != 0)
    {
        out = m_Period->toICALString();
    }
    else
    {
        // Bad period return "".
        out = "";
    }
    return out;
}
//---------------------------------------------------------------------


