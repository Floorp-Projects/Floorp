/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef _NLSCAL_H
#define _NLSCAL_H


#include "nlsxp.h"
#include "nlsuni.h"
#include "nlsloc.h"

#ifdef NLS_CPLUSPLUS
#include "calendar.h"
#include "timezone.h"
#endif


NLS_BEGIN_PROTOS

/******************** Formatting Data Types ************************/

#ifndef NLS_CPLUSPLUS
typedef struct _Calendar			Calendar;
typedef struct _TimeZone			TimeZone;
#endif

typedef int NLS_DateTimeFormatType;
enum _NLS_DateTimeFormatType {
	NLS_DATETIMEFORMAT_TYPE_FULL,
	NLS_DATETIMEFORMAT_TYPE_LONG,
	NLS_DATETIMEFORMAT_TYPE_MEDIUM,
	NLS_DATETIMEFORMAT_TYPE_SHORT,
	NLS_DATETIMEFORMAT_TYPE_DEFAULT = NLS_DATETIMEFORMAT_TYPE_MEDIUM,
	NLS_DATETIMEFORMAT_TYPE_DATE_OFFSET = 4,
	NLS_DATETIMEFORMAT_TYPE_NONE = -1,
	NLS_DATETIMEFORMAT_TYPE_DATE_TIME = 8
};

typedef int NLS_CalendarDateFields;
enum _NLS_CalendarDateFields {
		NLS_CALENDAR_ERA,
		NLS_CALENDAR_YEAR,
		NLS_CALENDAR_MONTH,
		NLS_CALENDAR_WEEK_OF_YEAR,
		NLS_CALENDAR_WEEK_OF_MONTH,
		NLS_CALENDAR_DATE,
		NLS_CALENDAR_DAY_OF_YEAR,
		NLS_CALENDAR_DAY_OF_WEEK,
		NLS_CALENDAR_DAY_OF_WEEK_IN_MONTH,
		NLS_CALENDAR_AM_PM,
		NLS_CALENDAR_HOUR,
		NLS_CALENDAR_HOUR_OF_DAY,
		NLS_CALENDAR_MINUTE,
		NLS_CALENDAR_SECOND,
		NLS_CALENDAR_MILLISECOND,
		NLS_CALENDAR_ZONE_OFFSET,
		NLS_CALENDAR_DST_OFFSET,
		NLS_CALENDAR_FIELD_COUNT
};

typedef int NLS_CalendarDaysOfWeek;
enum _NLS_CalendarDaysOfWeek {
		NLS_CALENDAR_SUNDAY = 1,
		NLS_CALENDAR_MONDAY,
		NLS_CALENDAR_TUESDAY,
		NLS_CALENDAR_WEDNESDAY,
		NLS_CALENDAR_THURSDAY,
		NLS_CALENDAR_FRIDAY,
		NLS_CALENDAR_SATURDAY
};

typedef int NLS_CalendarMonths;
enum _NLS_CalendarMonths {
		NLS_CALENDAR_JANUARY,
		NLS_CALENDAR_FEBRUARY,
		NLS_CALENDAR_MARCH,
		NLS_CALENDAR_APRIL,
		NLS_CALENDAR_MAY,
		NLS_CALENDAR_JUNE,
		NLS_CALENDAR_JULY,
		NLS_CALENDAR_AUGUST,
		NLS_CALENDAR_SEPTEMBER,
		NLS_CALENDAR_OCTOBER,
		NLS_CALENDAR_NOVEMBER,
		NLS_CALENDAR_DECEMBER,
		NLS_CALENDAR_UNDECIMBER
};

typedef int NLS_CalendarAMPM;
enum _NLS_CalendarAMPM {
		NLS_CALENDAR_AM,
		NLS_CALENDAR_PM
};

/******************** Simple functions ************************/

NLSFMTAPI_PUBLIC(Date)
NLS_GetCurrentDate();


/******************** Constructor functions ************************/


NLSFMTAPI_PUBLIC(NLS_ErrorCode) 
NLS_NewCalendar(Calendar ** result, const TimeZone* timeZone, const Locale*	locale);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewCalendarCopy(Calendar ** result, const Calendar * calendar);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewCalendarDate(Calendar ** result, int32 year, int32 month, int32 date);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewCalendarDateTime(Calendar ** result, int32 year, int32 month, int32 date, int32 hour, int32 minute, int32 second);

NLSFMTAPI_PUBLIC(const TimeZone*)
NLS_GetDefaultTimeZone();

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_NewTimeZone(TimeZone ** result, const UnicodeString* ID);

/********************* Destructor functions *******************************/

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteCalendar(Calendar * that);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_DeleteTimeZone(TimeZone * that);

/******************** API ************************/

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarSetDate(Calendar * calendar, int32 year, int32 month, int32 date);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarSetDateTime(Calendar * calendar, int32 year, int32 month, int32 date, int32 hour, int32 minute, int32 second);

NLSFMTAPI_PUBLIC(Date)
NLS_CalendarGetTime(const Calendar * calendar);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarSetTime(Calendar * calendar, Date date);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarAdd(Calendar * calendar, NLS_CalendarDateFields field, int32 amount);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarRoll(Calendar * calendar, NLS_CalendarDateFields field, nlsBool up);

NLSFMTAPI_PUBLIC(const TimeZone*)
NLS_CalendarGetTimeZone(const Calendar * calendar);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarSetTimeZone(Calendar * calendar, const TimeZone* timeZone);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarClear(Calendar * calendar);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarClearField(Calendar * calendar, NLS_CalendarDateFields field);

NLSFMTAPI_PUBLIC(int32)
NLS_CalendarGetField(const Calendar * calendar, NLS_CalendarDateFields field);

NLSFMTAPI_PUBLIC(nlsBool)
NLS_CalendarFieldIsSet(const Calendar * calendar, NLS_CalendarDateFields field);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarSetField(Calendar * calendar, NLS_CalendarDateFields field, int32 value);

NLSFMTAPI_PUBLIC(int32)
NLS_CalendarGetMinimum(const Calendar * calendar, NLS_CalendarDateFields field);

NLSFMTAPI_PUBLIC(int32)
NLS_CalendarGetMaximum(const Calendar * calendar, NLS_CalendarDateFields field);

NLSFMTAPI_PUBLIC(nlsBool)
NLS_CalendarInDaylightTime(const Calendar * calendar);

NLSFMTAPI_PUBLIC(nlsBool)
NLS_CalendarIsEqual(const Calendar * calendar1, const Calendar * calendar2);

NLSFMTAPI_PUBLIC(nlsBool)
NLS_CalendarBefore(const Calendar * calendar1, const Calendar * calendar2);

NLSFMTAPI_PUBLIC(nlsBool)
NLS_CalendarAfter(const Calendar * calendar1, const Calendar * calendar2);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarSetFirstDayOfWeek(Calendar* calendar, NLS_CalendarDaysOfWeek firstDay);

NLSFMTAPI_PUBLIC(NLS_CalendarDaysOfWeek)
NLS_CalendarGetFirstDayOfWeek(const Calendar* calendar);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_CalendarSetMinimalDaysInFirstWeek(Calendar* calendar, uint8 value);

NLSFMTAPI_PUBLIC(uint8)
NLS_CalendarGetMinimalDaysInFirstWeek(const Calendar* calendar);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_TimeZoneGetID(const TimeZone* zone, UnicodeString* IDResult);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_TimeZoneSetID(TimeZone* zone, const UnicodeString* IDResult);

NLSFMTAPI_PUBLIC(nlsBool)
NLS_TimeZoneEqual(const TimeZone* zone1, const TimeZone* zone2);

NLSFMTAPI_PUBLIC(nlsBool)
NLS_TimeZoneUseDaylightTime(const TimeZone* zone);

NLSFMTAPI_PUBLIC(int)
NLS_TimeZoneGetOffset(const TimeZone* zone, 					 
					 uint8			era,
					 int32			year,
					 int32			month,
					 int32			day,
					 uint8			dayOfWeek,
					 int32			millis);

NLSFMTAPI_PUBLIC(int32)
NLS_TimeZoneGetRawOffset(const TimeZone* zone);

NLSFMTAPI_PUBLIC(NLS_ErrorCode)
NLS_TimeZoneSetRawOffset(TimeZone* zone, int32 offset);


/************************ End ******************************/

NLS_END_PROTOS
#endif /* _NLSCAL_H */

