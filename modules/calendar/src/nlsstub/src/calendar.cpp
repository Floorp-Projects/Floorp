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
#include "calendar.h"
//#include "timezone.h"


Calendar::Calendar()
{
}

Calendar::~Calendar()
{
}

Date Calendar::getNow()
{
  return ((Date) nsnull);
    
}

void Calendar::setTimeZone(const TimeZone& aZone)
{
  return ;
}

Date Calendar::getTime(ErrorCode& aStatus) const
{
  return ((Date)nsnull);
}


PRInt32 Calendar::get(EDateFields aField, ErrorCode& aStatus) const
{
  return (0);
}

void Calendar::setTime(Date aDate, ErrorCode& aStatus)
{
  return;
}

void Calendar::set(EDateFields aField, PRInt32 aValue)
{
  return ;
}

void Calendar::set(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate)
{
  return ;
}

void Calendar::set(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, PRInt32 aHour, PRInt32 aMinute)
{
  return ;
}

void Calendar::set(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, PRInt32 aHour, PRInt32 aMinute, PRInt32 aSecond)
{
  return ;
}

void Calendar::clear()
{
  return ;
}

void Calendar::clear(EDateFields aField)
{
  return ;
}

