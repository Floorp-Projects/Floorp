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

// bydygntr.cpp
// John Sun
// 4:11 PM February 3 1998

#include "stdafx.h"
#include "jdefines.h"

#include <math.h>
#include <assert.h>
#include "bydygntr.h"
#include "jutility.h"

ByDayYearlyGenerator::ByDayYearlyGenerator() 
:
    DateGenerator(JulianUtility::RT_YEARLY, 0),
    m_aiParams(0),
    m_iWkSt((t_int32) Calendar::SUNDAY)
{}

    
//---------------------------------------------------------------------

t_int32 
ByDayYearlyGenerator::getInterval() const { return JulianUtility::RT_DAILY; }

//---------------------------------------------------------------------
// TODO: crash proof
t_bool 
ByDayYearlyGenerator::generate(DateTime * start, JulianPtrArray & dateVector,
                               DateTime * until)
{
    DateTime * t, * nextWkSt;
    t_int32 i, k, iYear = 0, iDayOfYear = 0;
    t_int32 iAbsMod = -1;
    t_bool bOldWkSt = FALSE;
    t_int32 temp = 0;

    PR_ASSERT(start != 0);
    PR_ASSERT(m_aiParams != 0);
    
    if (start == 0 || m_aiParams == 0)
    {
        return FALSE;
    }

    t = new DateTime(start->getTime());  PR_ASSERT(t != 0);
    // bulletproof code
    if (t == 0)
    {
        return FALSE;
    }
    nextWkSt = new DateTime(start->getTime()); PR_ASSERT(nextWkSt != 0);
    // bulletproof code
    if (nextWkSt == 0)
    {
        if (t != 0) { delete t; t = 0; }
        return FALSE;
    }

    for (i = 0; i < m_iParamsLen; i++)
    {
        k = 0;
        t->setTime(start->getTime());

        if (m_aiParams[i][1] >= 0)
            t->add(Calendar::DAY_OF_YEAR,
            -(t->get(Calendar::DAY_OF_YEAR)) + 1);
        else
            t->findLastDayOfYear();

        iYear = t->get(Calendar::YEAR);
        
        if (m_aiParams[i][1] != 0)
        {
            // NOTE: avoid using abs to fix HPUX compiling problems
            temp = m_aiParams[i][1];
            if (temp < 0)
                temp = 0 - temp;
            iAbsMod = temp - 1;

            // replacing below code with above
            //iAbsMod = abs(m_aiParams[i][1]) - 1;
            

            while ((k != iAbsMod) ||
                (t->get(Calendar::DAY_OF_WEEK) != m_aiParams[i][0]))
            {
                if (t->get(Calendar::DAY_OF_WEEK) == m_aiParams[i][0])
                    k++;

                if (m_aiParams[i][1] > 0)
                    t->add(Calendar::DAY_OF_YEAR, 1);
                else
                    t->add(Calendar::DAY_OF_YEAR, -1);

                if (until != 0 && until->isValid() && t->after(until))
                {
                    delete t; t = 0;
                    delete nextWkSt; nextWkSt = 0;
                    return TRUE;
                }
            }
                
            // check also still in same year!
            // prevents bug if asking 53MO, but no 53th monday in this year   
            if ((t->after(start) || t->equals(start)) &&
                (t->get(Calendar::YEAR) == iYear))
            {
                dateVector.Add(new DateTime(t->getTime()));
            }
        }
        else
        {
            nextWkSt->setTime(start->getTime());
            nextWkSt->moveTo(m_iWkSt, 1, TRUE);

            while (t->get(Calendar::YEAR) == iYear)
            {
                bOldWkSt = TRUE;
                if (until != 0 && until->isValid() && t->after(until))
                {
                    delete t; t = 0;
                    delete nextWkSt; nextWkSt = 0;
                    return TRUE;
                }
                else if (t->get(Calendar::DAY_OF_WEEK) == m_aiParams[i][0] &&
                    (t->after(start) || t->equals(start)) &&
                    !(t->after(nextWkSt)))
                {
                    dateVector.Add(new DateTime(t->getTime()));
                    nextWkSt->setTime(t->getTime());
                    nextWkSt->moveTo(m_iWkSt, 1, TRUE);
                }
                else if (t->after(nextWkSt))
                {
                    nextWkSt->setTime(t->getTime());
                    nextWkSt->moveTo(m_iWkSt, 1, TRUE);
                    bOldWkSt = FALSE;
                }
                if ((bOldWkSt) && (!t->after(nextWkSt)))
                {
                    iDayOfYear = t->get(Calendar::DAY_OF_YEAR);
                    t->add(Calendar::DATE, 1);
                    if (iDayOfYear == t->get(Calendar::DAY_OF_YEAR))
                        t->add(Calendar::DAY_OF_YEAR, -1);
                }
            }
        }
    }
    delete t; t = 0;
    delete nextWkSt; nextWkSt = 0;
    return FALSE;
}
//---------------------------------------------------------------------

