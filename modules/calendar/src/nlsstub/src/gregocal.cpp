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

#include "gregocal.h"

GregorianCalendar::GregorianCalendar() : Calendar()
{
}

GregorianCalendar::~GregorianCalendar()
{
}

GregorianCalendar::GregorianCalendar(ErrorCode& aSuccess) : Calendar()
{
}

GregorianCalendar::GregorianCalendar(TimeZone* aZoneToAdopt, ErrorCode& aSuccess) : Calendar()
{
  setTimeZone(*aZoneToAdopt);
}

GregorianCalendar::GregorianCalendar(const TimeZone& aZone, ErrorCode& aSuccess) : Calendar()
{
  setTimeZone(aZone);
}

GregorianCalendar::GregorianCalendar(const Locale& aLocale, ErrorCode& aSuccess) : Calendar()
{
}

GregorianCalendar::GregorianCalendar(TimeZone* aZoneToAdopt, const Locale& aLocale, ErrorCode& aSuccess) : Calendar()
{
  setTimeZone(*aZoneToAdopt);
}

GregorianCalendar::GregorianCalendar(const TimeZone& aZone, const Locale& aLocale, ErrorCode& aSuccess)
                     : Calendar()
{
  setTimeZone(aZone);
}

GregorianCalendar::GregorianCalendar(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, ErrorCode& aSuccess)
                     : Calendar()
{
  set(aYear, aMonth, aDate);
}

GregorianCalendar::GregorianCalendar(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, PRInt32 aHour, PRInt32 aMinute, ErrorCode& aSuccess)
                     : Calendar()
{
  set(aYear, aMonth, aDate, aHour, aMinute);
}

GregorianCalendar::GregorianCalendar(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, PRInt32 aHour, PRInt32 aMinute, PRInt32 aSecond, ErrorCode& aSuccess)
                     : Calendar()
{
  set(aYear, aMonth, aDate, aHour, aMinute, aSecond);
}

void GregorianCalendar::add(EDateFields aField, PRInt32 aAmount, ErrorCode& aStatus)
{
  switch(aField)
  {
    case SECOND:
      mExplodedTime.tm_sec += aAmount;
      break;

    case DAY_OF_WEEK_IN_MONTH:
      break;

    case MINUTE:
      mExplodedTime.tm_min += aAmount;
      break;

    case HOUR:
      mExplodedTime.tm_hour += aAmount;
      break;

    case DAY_OF_YEAR:
      mExplodedTime.tm_mday += aAmount;
      break;

    case WEEK_OF_YEAR:
      break;

    case MONTH:
      mExplodedTime.tm_month += aAmount;
      break;

    case DATE:
      mExplodedTime.tm_mday += aAmount;
      break;

    case DAY_OF_WEEK:
      break;

    case DAY_OF_MONTH:
      break;

    case HOUR_OF_DAY:
      mExplodedTime.tm_hour += aAmount;
      break;

    case YEAR:
      mExplodedTime.tm_year += aAmount;
      break;

    default:
      break;

  }

  PR_NormalizeTime(&mExplodedTime,PR_GMTParameters);

  mTime = PR_ImplodeTime(&mExplodedTime);

  PR_ExplodeTime(mTime, PR_GMTParameters, &mExplodedTime);

  return;
}


