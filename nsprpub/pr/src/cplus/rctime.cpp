/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

/*
** Class implementation for calendar time routines (ref: prtime.h)
*/

#include "rctime.h"

RCTime::~RCTime() { }

RCTime::RCTime(PRTime time): RCBase() { gmt = time; }
RCTime::RCTime(const RCTime& his): RCBase() { gmt = his.gmt; }
RCTime::RCTime(RCTime::Current): RCBase() { gmt = PR_Now(); }
RCTime::RCTime(const PRExplodedTime& time): RCBase()
{ gmt = PR_ImplodeTime(&time); }

void RCTime::operator=(const PRExplodedTime& time)
{ gmt = PR_ImplodeTime(&time); }

RCTime RCTime::operator+(const RCTime& his)
{ RCTime sum(gmt + his.gmt); return sum; }

RCTime RCTime::operator-(const RCTime& his)
{ RCTime difference(gmt - his.gmt); return difference; }

RCTime RCTime::operator/(PRUint64 his)
{ RCTime quotient(gmt / gmt); return quotient; }

RCTime RCTime::operator*(PRUint64 his)
{ RCTime product(gmt * his); return product; }

