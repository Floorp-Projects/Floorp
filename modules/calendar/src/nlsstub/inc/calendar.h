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

#ifndef calendar_h__
#define calendar_h__

#include "ptypes.h"
#include "parsepos.h"

class TimeZone;
class ParsePosition;

class Calendar 
{

public:

  enum EMonths {
    JANUARY,
    FEBRUARY,
    MARCH,
    APRIL,
    MAY,
    JUNE,
    JULY,
    AUGUST,
    SEPTEMBER,
    OCTOBER,
    NOVEMBER,
    DECEMBER,
    UNDECIMBER
  };

  enum EDateFields 
  {
    SECOND,
    DAY_OF_WEEK_IN_MONTH,
    MINUTE,
    HOUR,
    DAY_OF_YEAR,
    WEEK_OF_YEAR,
    MONTH,
    DATE,
    DAY_OF_WEEK,
    DAY_OF_MONTH,
    HOUR_OF_DAY,
    YEAR
  };

  enum EDaysOfWeek 
  {
    SUNDAY = 1,
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY
  };

public:
  Calendar();
  ~Calendar();

  static Date getNow();
  void setTimeZone(const TimeZone& aZone);
  Date getTime(ErrorCode& aStatus) const;
  PRInt32 get(EDateFields aField, ErrorCode& aStatus) const;
  void setTime(Date aDate, ErrorCode& aStatus);
  void set(EDateFields aField, PRInt32 aValue);
  void set(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate);
  void set(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, PRInt32 aHour, PRInt32 aMinute);
  void set(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, PRInt32 aHour, PRInt32 aMinute, PRInt32 aSecond);
  virtual void add(EDateFields aField, PRInt32 aAmount, ErrorCode& aStatus) = 0;

  void clear();
  void clear(EDateFields aField);

};

#endif
