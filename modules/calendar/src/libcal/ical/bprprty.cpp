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

// bprprty.cpp
// John Sun
// 3:34 PM February 12 1998

#include "stdafx.h"
#include "jdefines.h"

#include "bprprty.h"
#include "prprty.h"

//---------------------------------------------------------------------

// private never use
BooleanProperty::BooleanProperty()
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

BooleanProperty::BooleanProperty(const BooleanProperty & that)
{
    m_Boolean = that.m_Boolean;
    setParameters(that.m_vParameters);
}

//---------------------------------------------------------------------

BooleanProperty::BooleanProperty(t_bool value, JulianPtrArray * parameters)
: StandardProperty(parameters)
{
    m_Boolean = value;
}

//---------------------------------------------------------------------

BooleanProperty::~BooleanProperty()
{
}

//---------------------------------------------------------------------

void * BooleanProperty::getValue() const
{
    return (void *) &m_Boolean;
}

//---------------------------------------------------------------------
    
void BooleanProperty::setValue(void * value)
{
    PR_ASSERT(value != 0);
    if (value != 0)
    {
        m_Boolean = *((t_bool *) value);
    }
}

//---------------------------------------------------------------------

ICalProperty * BooleanProperty::clone(JLog * initLog)
{
    if (initLog) {} // NOTE: Remove later to avoid warnings
    return new BooleanProperty(*this);   
}

//---------------------------------------------------------------------

t_bool BooleanProperty::isValid()
{
    return TRUE;
}

//---------------------------------------------------------------------

UnicodeString & BooleanProperty::toString(UnicodeString & out)
{

    if (m_Boolean)
        out = "TRUE";
    else
        out = "FALSE";
    return out;
}
//---------------------------------------------------------------------
UnicodeString & 
BooleanProperty::toString(UnicodeString & dateFmt, UnicodeString & out)
{
    // NOTE: remove it later, gets rid of compiler warning
    if (dateFmt.size() > 0) {}
    return toString(out);
}
//---------------------------------------------------------------------

UnicodeString & BooleanProperty::toExportString(UnicodeString & out)
{
    return toString(out);
}
//---------------------------------------------------------------------

