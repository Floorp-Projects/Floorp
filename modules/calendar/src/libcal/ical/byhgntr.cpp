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

// byhgntr.cpp
// John Sun
// 4:18 PM February 3 1998

#include "stdafx.h"
#include "jdefines.h"

#include <assert.h>
#include "byhgntr.h"
#include "jutility.h"
#include "calendar.h"

ByHourGenerator::ByHourGenerator() 
:
    DateGenerator(JulianUtility::RT_DAILY, 0),
    m_aiParams(0)
{}

//---------------------------------------------------------------------

t_int32 
ByHourGenerator::getInterval() const { return JulianUtility::RT_HOURLY; }

//---------------------------------------------------------------------

t_bool 
ByHourGenerator::generate(DateTime * start, JulianPtrArray & dateVector,
                          DateTime * until)
{
    DateTime * t;
    t_int32 i;

    DateTime dt;

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

        while (start->get((Calendar::EDateFields) m_iSpan) == 
            t->get((Calendar::EDateFields) m_iSpan))
        {
             // only allow 0-23, no negatives allowed, using hour_of_day
            // instead of hour since hour does seem to update immediately
             t->set(Calendar::HOUR_OF_DAY, m_aiParams[i]);

             /*
             // allow negatives (not in spec anymore, so commented out)
            
             if (m_aiParams[i] > 0)
                t->set(Calendar::HOUR_OF_DAY, m_aiParams[i]);
             else
                t->set(Calendar::HOUR_OF_DAY, 24 + m_aiParams[i]);
            */

            if (until != 0 && until->isValid() && t->after(until))
            {
                delete t; t = 0;
                return TRUE;
            }
            else if (t->after(start) || t->equals(start))
            {
                //dt = *t;
                //dateVector.Add(t);
                //delete t;
                dateVector.Add(new DateTime(t->getTime()));
            }

            t->add(Calendar::DAY_OF_YEAR, 1);
        }
    }
    delete t; t = 0;
    return FALSE;
}
//---------------------------------------------------------------------


