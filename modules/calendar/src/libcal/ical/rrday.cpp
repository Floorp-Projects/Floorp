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

// rrday.cpp
// John Sun
// 7:54 PM February 4 1998

#include "stdafx.h"
#include "jdefines.h"

#include <ptypes.h>
#include "rrday.h"

//---------------------------------------------------------------------

RRDay::RRDay() 
{
    fields[0] = 0;
    fields[1] = 0;
}

//---------------------------------------------------------------------

RRDay::RRDay(t_int32 aDay, t_int32 aModifier)
{
    fields[0] = aDay;
    fields[1] = aModifier;
}

//---------------------------------------------------------------------

t_int32 RRDay::operator[](t_int32 index) const
{
    PR_ASSERT(index >= 0 && index < 2);
    // Pin args in index is invalid
    if (index < 0)
        index = 0;
    else if (index > 1)
        index = 1;

    return fields[index];
}

//---------------------------------------------------------------------

void RRDay::operator=(RRDay & that)
{
    fields[0] = that.fields[0];
    fields[1] = that.fields[1];
}

//---------------------------------------------------------------------

t_bool RRDay::operator==(RRDay & that)
{
    return (fields[0] == that.fields[0]) && (fields[1] == that.fields[1]);
}

//---------------------------------------------------------------------

t_bool RRDay::operator!=(RRDay & that)
{
    return (fields[0] != that.fields[0]) || (fields[1] != that.fields[1]);
}
//---------------------------------------------------------------------

