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

// deftgntr.cpp
// John Sun
// 4:01 PM Febrary 3 1998

#include "stdafx.h"
#include "jdefines.h"

#include <assert.h>
#include "deftgntr.h"
#include "jutility.h"
#include "duration.h"

//t_int32 DefaultGenerator::m_iParamsLen = 3;

DefaultGenerator::DefaultGenerator()
: DateGenerator(JulianUtility::RT_MONTHLY, 3),
  isActive(FALSE)
{}

//---------------------------------------------------------------------

t_int32 
DefaultGenerator::getInterval() const { return JulianUtility::RT_MONTHLY; }

//---------------------------------------------------------------------

t_bool 
DefaultGenerator::generate(DateTime * start, JulianPtrArray & dateVector, 
                           DateTime * until)
{
    t_int32 i;
    DateTime * t;
    Duration * d;

    PR_ASSERT(start != 0);
    PR_ASSERT(m_aiParams != 0);

    if (start == 0 || m_aiParams == 0)
    {
        return FALSE;
    }

    t = new DateTime(start->getTime()); PR_ASSERT(t != 0);
    d = new Duration(m_aiParams[0], m_aiParams[1]); PR_ASSERT(d != 0);

    dateVector.Add(new DateTime(t->getTime()));

    for (i = 0; i < m_aiParams[2] - 1; i++)
    {
        t->add(*d);
        if (until != 0 && until->isValid() && t->after(until))
        {
            //t = NULL;
            delete t; t = 0;// replaced above line
            delete d; d = 0;
            return TRUE;
        }
        dateVector.Add(new DateTime(t->getTime()));
    }
    delete t; t = 0;
    delete d; d = 0;
    return FALSE;
}
//---------------------------------------------------------------------

