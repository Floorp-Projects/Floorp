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

/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*- */
// uidrgntr.cpp
// John Sun
// 3/19/98 5:41:26 PM

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "stdafx.h"
#include "jdefines.h"

#include "uidrgntr.h"
#include "datetime.h"

//---------------------------------------------------------------------

JulianUIDRandomGenerator::JulianUIDRandomGenerator()
{
}

//---------------------------------------------------------------------

JulianUIDRandomGenerator::~JulianUIDRandomGenerator()
{
}

//---------------------------------------------------------------------

UnicodeString 
JulianUIDRandomGenerator::generate(UnicodeString us)
{
    UnicodeString u;
    DateTime d;
    u += d.toISO8601();
    u += '-';
    srand((unsigned) time(0));
    char sBuf[10];
    sprintf(sBuf, "%.9x", rand());
    u += sBuf;
    // TODO: Append hostname or email
    u += '-';
    u += us;
    return u;
}

//---------------------------------------------------------------------

UnicodeString
JulianUIDRandomGenerator::generate()
{
    return generate("");
}

//---------------------------------------------------------------------
