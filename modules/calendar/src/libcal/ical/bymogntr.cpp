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

// bymogntr.cpp
// John Sun
// 4:21 PM February 3 1998

#include "stdafx.h"
#include "jdefines.h"

#include <assert.h>
#include "bymogntr.h"
#include "jutility.h"

ByMonthGenerator::ByMonthGenerator() 
:
    DateGenerator(JulianUtility::RT_YEARLY, 0),
    m_aiParams(0)
    {}

//---------------------------------------------------------------------

t_int32 
ByMonthGenerator::getInterval() const { return JulianUtility::RT_MONTHLY; }

//---------------------------------------------------------------------

t_bool 
ByMonthGenerator::generate(DateTime * start, JulianPtrArray & dateVector,
                           DateTime * until)
{
    t_int32 i, iMonth = 0;
    DateTime * t;

    PR_ASSERT(start != 0);
    PR_ASSERT(m_aiParams != 0);

    if (start == 0 || m_aiParams == 0)
    {
        return FALSE;
    }
    
    t = new DateTime(start->getTime()); PR_ASSERT(t != 0);

    for (i = 0; i < m_iParamsLen; i++)
    {
        t->setTime(start->getTime());

        // negative month don't exist, only [1-12]
        //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));

        t->add(Calendar::MONTH, m_aiParams[i] - t->get(Calendar::MONTH) - 1);
        t->add(Calendar::DATE, - t->get(Calendar::DATE) + 1);

        //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));

        iMonth = t->get(Calendar::MONTH);

        while (t->get(Calendar::MONTH) == iMonth)
        {
            if (until != 0 && until->isValid() & t->after(until))
            {
                delete t; t = 0;
                return TRUE;
            }
            if (t->after(start) || t->equals(start))
                dateVector.Add(new DateTime(t->getTime()));

            t->add(Calendar::DATE, 1);
        }
    }
    delete t; t = 0;
    return FALSE;
}
//---------------------------------------------------------------------

