/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the 'NPL'); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an 'AS IS' basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */
/* 
 * jlogitr.cpp
 * John Sun
 * 8/17/98 6:30:56 PM
 */

#include "stdafx.h"
#include "jdefines.h"
#include <unistring.h>
#include "jlogitr.h"

//---------------------------------------------------------------------
JulianLogIterator::JulianLogIterator(JulianPtrArray * toIterate,
                                     JulianLogErrorVector::ECompType iComponentType,
                                     t_bool bValid):
m_LogToIterateOver(toIterate),
m_iComponentType(iComponentType),
m_bValid(bValid)
{
}
//---------------------------------------------------------------------


JulianLogIterator::JulianLogIterator()
{
}

//---------------------------------------------------------------------

JulianLogIterator * 
JulianLogIterator::createIterator(JulianPtrArray * toIterate, 
                                  JulianLogErrorVector::ECompType iComponentType,
                                  t_bool bValid)
{
    if (toIterate == 0)
        return 0;
    else
        return new JulianLogIterator(toIterate, iComponentType, bValid);
}

//---------------------------------------------------------------------

JulianLogErrorVector * 
JulianLogIterator::firstElement()
{
    return findNextElement(0);
}

//---------------------------------------------------------------------

JulianLogErrorVector * 
JulianLogIterator::nextElement()
{
    return findNextElement(++m_iIndex);
}

//---------------------------------------------------------------------

JulianLogErrorVector * 
JulianLogIterator::findNextElement(t_int32 startIndex)
{
    if (m_LogToIterateOver != 0)
    {
        JulianLogErrorVector * errVctr = 0;
        t_int32 i;
        for (i = startIndex; i < m_LogToIterateOver->GetSize(); i++)
        {
            errVctr = (JulianLogErrorVector *) m_LogToIterateOver->GetAt(i);
            if ((errVctr->GetComponentType() == m_iComponentType) &&
                (errVctr->IsValid() == m_bValid))
            {
                m_iIndex = i;
                return errVctr;
            }
        }
    }
    return 0;
}

//---------------------------------------------------------------------




