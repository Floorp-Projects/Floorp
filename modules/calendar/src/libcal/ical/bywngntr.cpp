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

// bywngntr.cpp
// John Sun
// 4:23 PM February 3 1998

#include "stdafx.h"
#include "jdefines.h"

#include <assert.h>
#include "bywngntr.h"
#include "jutility.h"

ByWeekNoGenerator::ByWeekNoGenerator() 
:
    DateGenerator(JulianUtility::RT_YEARLY, 0),
    m_aiParams(0)
    {}

//---------------------------------------------------------------------

t_int32 
ByWeekNoGenerator::getInterval() const { return JulianUtility::RT_WEEKLY; }

//---------------------------------------------------------------------

t_bool 
ByWeekNoGenerator::generate(DateTime * start, JulianPtrArray & dateVector,
                            DateTime * until)
{
    DateTime * t;
    t_int32 i, k;

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
        
        t->setByISOWeek(m_aiParams[i]);

        for (k = 0; k < 7; k++)
        {
            if (until != 0 && until->isValid() && t->after(until) && i == m_iParamsLen - 1)
            {
                delete t; t = 0;
                return TRUE;
            }
            else if (until != 0 && until->isValid() && t->after(until))
            {
                //delete t;
                break;
            }
            if (t->after(start) || t->equals(start))
                dateVector.Add(new DateTime(t->getTime()));

            t->add(Calendar::DAY_OF_YEAR, 1);

        }
    }
    delete t; t = 0;
    return FALSE;
}
//---------------------------------------------------------------------

