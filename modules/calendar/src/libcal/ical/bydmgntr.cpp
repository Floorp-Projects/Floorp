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

// bydmgntr.cpp
// John Sun
// 4:08 PM February 3 1998

#include "stdafx.h"
#include "jdefines.h"

#include <math.h>
#include <assert.h>
#include "bydmgntr.h"
#include "jutility.h"
#include "calendar.h"


ByDayMonthlyGenerator::ByDayMonthlyGenerator() 
:   DateGenerator(JulianUtility::RT_MONTHLY, 0),
    m_aiParams(0),
    m_iWkSt((t_int32) Calendar::SUNDAY)
{}

//---------------------------------------------------------------------

t_int32 
ByDayMonthlyGenerator::getInterval() const { return JulianUtility::RT_DAILY; }

//---------------------------------------------------------------------
// TODO: crash proof it
t_bool 
ByDayMonthlyGenerator::generate(DateTime * start, JulianPtrArray & dateVector,
                                DateTime * until)
{
    DateTime * t, * nextWkSt;
    t_int32 i, iMonth = 0, iMonth2 = 0;   
    t_bool bOldWkSt;
    t_int32 temp;

    PR_ASSERT(start != 0);
    PR_ASSERT(m_aiParams != 0);
    // bulletproof code
    if (start == 0 || m_aiParams == 0)
    {
        return FALSE;
    }

    // if (FALSE) TRACE("start = %s\r\n", start->toString().toCString(""));

    t = new DateTime(start->getTime()); PR_ASSERT(t != 0);
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
        t->setTime(start->getTime());

        //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));
        //if (FALSE) TRACE("nextWkSt = %s\r\n", nextWkSt->toString().toCString(""));

        if (m_aiParams[i][1] >= 0)
        {
            t->add(Calendar::DATE, -(t->get(Calendar::DATE)) + 1);
        }
        else
        {
            t->findLastDayOfMonth();
        }

        //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));

        iMonth = t->get(Calendar::MONTH);

        if (m_aiParams[i][1] != 0)
        {
            while (t->get(Calendar::DAY_OF_WEEK) != m_aiParams[i][0])
                t->add(Calendar::DATE, (m_aiParams[i][1] > 0) ? 1 : -1);

            //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));

            // NOTE: so not to call abs (to fix HPUX-compiling problem), replaces below commented out code
            temp = m_aiParams[i][1];
            if (temp < 0)
                temp = 0 - temp;

            t->add(Calendar::DAY_OF_YEAR, (temp - 1) *
                (m_aiParams[i][1] > 0 ? 1 : -1) * 7);

            /// replacing this code with above
            //t->add(Calendar::DAY_OF_YEAR, (abs(m_aiParams[i][1]) - 1) *
            //    (m_aiParams[i][1] > 0 ? 1 : -1) * 7);

            

            //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));

            if (until != 0 && until->isValid() && t->after(until))
            {
                delete t; t = 0;
                delete nextWkSt; nextWkSt = 0;
                return TRUE;
            }
            else
            {
                // check also still in same month!
                // prevents bug if asking 5MO, but no 5th monday in this month
                if ((t->after(start) || t->equals(start)) &&
                    (t->get(Calendar::MONTH) == iMonth)
                    )
                {
                    dateVector.Add(new DateTime(t->getTime()));
                }
            }
        }
        else
        {
            nextWkSt->setTime(t->getTime());
            nextWkSt->moveTo(m_iWkSt, 1, TRUE);

            while (t->get(Calendar::MONTH) == iMonth)
            {

                //if (FALSE) TRACE("nextWkSt = %s\r\n", nextWkSt->toString().toCString(""));
                //if (FALSE) TRACE("t = %s\r\n", t->toString().toCString(""));

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
                if ((bOldWkSt) && (!(t->after(nextWkSt))))
                {
                    iMonth2 = t->get(Calendar::DAY_OF_MONTH);
                    t->add(Calendar::DATE, 1);
                    if (iMonth2 == t->get(Calendar::DAY_OF_MONTH))
                        t->add(Calendar::DATE, -1);
                }
            }
        }
    }
    delete t; t = 0;
    delete nextWkSt; nextWkSt = 0;
    return FALSE;
}
//---------------------------------------------------------------------


