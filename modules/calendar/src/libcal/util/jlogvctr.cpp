/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil c-basic-offset: 2 -*- 
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
/* 
 * jlogvctr.cpp
 * John Sun
 * 8/17/98 4:48:46 PM
 */

#include "stdafx.h"
#include "jdefines.h"
#include "jlogvctr.h"

//---------------------------------------------------------------------
JulianLogErrorVector::JulianLogErrorVector()
{
    m_ICalComponentType = ECompType_NSCALENDAR;
    m_ErrorVctr = 0;
    m_bValidEvent = TRUE;
}
//---------------------------------------------------------------------
JulianLogErrorVector::JulianLogErrorVector(ECompType iICalComponentType)
{
    m_ICalComponentType = iICalComponentType;
    m_ErrorVctr = 0;
    m_bValidEvent = TRUE;
}
//---------------------------------------------------------------------
JulianLogErrorVector::~JulianLogErrorVector()
{
    if (m_ErrorVctr != 0)
    {
        JulianLogError::deleteJulianLogErrorVector(m_ErrorVctr);   
        delete m_ErrorVctr; m_ErrorVctr = 0;
    }
}
//---------------------------------------------------------------------
void JulianLogErrorVector::AddError(JulianLogError * error)
{
    if (m_ErrorVctr == 0)
        m_ErrorVctr = new JulianPtrArray();
    if (m_ErrorVctr != 0)
        m_ErrorVctr->Add(error);
}
//---------------------------------------------------------------------
