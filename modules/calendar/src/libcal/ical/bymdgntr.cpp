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

// bymdgntr.cpp
// John Sun
// 4:08 PM February 3 1998

#include "stdafx.h"
#include "jdefines.h"

#include <assert.h>
#include "bymdgntr.h"
#include "jutility.h"

ByMonthDayGenerator::ByMonthDayGenerator() 
: 
    DateGenerator(JulianUtility::RT_MONTHLY, 0),
    m_aiParams(0)
    {}

//---------------------------------------------------------------------

t_int32 
ByMonthDayGenerator::getInterval() const { return JulianUtility::RT_DAILY; }

//---------------------------------------------------------------------

t_bool 
ByMonthDayGenerator::generate(DateTime * start, JulianPtrArray & dateVector,
                              DateTime * until)
{
    DateTime * t;
    t_int32 i, iMonth = 0;

    PR_ASSERT(start != 0);
    PR_ASSERT(m_aiParams != 0);

    if (start == 0 || m_aiParams == 0)
    {
        return FALSE;
    }

    t = new DateTime(start->getTime()); PR_ASSERT(t != 0);

    //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));

    for (i = 0; i < m_iParamsLen; i++)
    {
        t->setTime(start->getTime());

        //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));

        if (m_aiParams[i] > 0)
            t->add(Calendar::DATE, - (t->get(Calendar::DATE)) + 1);
        else
            t->findLastDayOfMonth();

        //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));

        iMonth = t->get(Calendar::MONTH);
        
        t->add(Calendar::DATE, 
            (m_aiParams[i] > 0) ? m_aiParams[i] - 1 : m_aiParams[i] + 1);

        //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));

        if (until != 0 && until->isValid() && t->after(until))
        {
            delete t; t = 0;
            return TRUE;
        }

        // make sure in same month
        if ((t->after(start) || t->equals(start)) &&
            (t->get(Calendar::MONTH) == iMonth))
        {
            dateVector.Add(new DateTime(t->getTime()));
        }
    }
    delete t; t = 0;
    return FALSE;
}
//---------------------------------------------------------------------


