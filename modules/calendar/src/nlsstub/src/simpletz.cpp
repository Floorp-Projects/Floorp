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

#include "nscore.h"
#include "simpletz.h"

SimpleTimeZone::SimpleTimeZone() : TimeZone()
{
}

SimpleTimeZone::~SimpleTimeZone()
{
}

SimpleTimeZone::SimpleTimeZone(PRInt32 aRawOffset, const UnicodeString& aID) : TimeZone()
{
}

SimpleTimeZone::SimpleTimeZone(PRInt32 aRawOffset, const UnicodeString& aID,
                                PRInt8  aStartMonth,      PRInt8 aStartDayOfWeekInMonth,
                                PRInt8  aStartDayOfWeek,  PRInt32 aStartTime,
                                PRInt8  aEndMonth,        PRInt8 aEndDayOfWeekInMonth,
                                PRInt8  aEndDayOfWeek,    PRInt32 aEndTime,
                                PRInt32 aDstSavings) : TimeZone()
{
}

TimeZone* SimpleTimeZone::clone() const
{
  TimeZone * t = (TimeZone*) new TimeZone();
  return t;
}

void SimpleTimeZone::setRawOffset(PRInt32 aOffsetMillis)
{
  return ;
}

void SimpleTimeZone::setStartRule(PRInt32 aMonth, PRInt32 aDayOfWeekInMonth, PRInt32 aDayOfWeek, PRInt32 aTime)
{
  return ;
}

void SimpleTimeZone::setEndRule(PRInt32 aMonth, PRInt32 aDayOfWeekInMonth, PRInt32 aDayOfWeek, PRInt32 aTime)
{
  return ;
}


