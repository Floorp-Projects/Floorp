/* -*- Mode: C -*- */
/*======================================================================
 FILE: icaltime.h
 CREATOR: eric 02 June 2000


 $Id: icaltime.h,v 1.3 2004/07/27 19:31:52 mostafah%oeone.com Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


======================================================================*/

#ifndef ICALTIME_H
#define ICALTIME_H

#include <time.h>

/* An opaque struct representing a timezone. We declare this here to avoid
   a circular dependancy. */
#ifndef ICALTIMEZONE_DEFINED
#define ICALTIMEZONE_DEFINED
typedef struct _icaltimezone		icaltimezone;
#endif

/** icaltime_span is returned by icalcomponent_get_span() */
struct icaltime_span {
	time_t start;   /**< in UTC */
	time_t end;     /**< in UTC */
	int is_busy;    /**< 1->busy time, 0-> free time */
};


struct icaltimetype
{
	int year;	/* Actual year, e.g. 2001. */
	int month;	/* 1 (Jan) to 12 (Dec). */
	int day;
	int hour;
	int minute;
	int second;

	int is_utc; /* 1-> time is in UTC timezone */

	int is_date; /* 1 -> interpret this as date. */
   
	int is_daylight; /* 1 -> time is in daylight savings time. */
   
	const char* zone; /*Ptr to Olsen placename. Libical does not own mem*/
};	

/* Convert seconds past UNIX epoch to a timetype*/
struct icaltimetype icaltime_from_timet(time_t v, int is_date);

/* Newer version of above, using timezones. */
struct icaltimetype icaltime_from_timet_with_zone(time_t tm, int is_date,
						  icaltimezone *zone);

/* Returns the current time in the given timezone, as an icaltimetype. */
struct icaltimetype icaltime_current_time_with_zone(icaltimezone *zone);

/* Returns the current day as an icaltimetype, with is_date set. */
struct icaltimetype icaltime_today(void);

/* Return the time as seconds past the UNIX epoch */
time_t icaltime_as_timet(struct icaltimetype);

/* Newer version of above, using timezones. */
time_t icaltime_as_timet_with_zone(struct icaltimetype tt, icaltimezone *zone);

/* Return a string represention of the time, in RFC2445 format. The
   string is owned by libical */
char* icaltime_as_ical_string(struct icaltimetype tt);

/* Like icaltime_from_timet(), except that the input may be in seconds
   past the epoch in floating time. This routine is deprecated */
struct icaltimetype icaltime_from_int(int v, int is_date, int is_utc);

/* Like icaltime_as_timet, but in a floating epoch. This routine is deprecated */
int icaltime_as_int(struct icaltimetype);

/* create a time from an ISO format string */
struct icaltimetype icaltime_from_string(const char* str);


/* begin WARNING !! DEPRECATED !! functions *****
use new icaltimezone functions, see icaltimezone.h
 */

/* Routines for handling timezones */
/* Return the offset of the named zone as seconds. tt is a time
   indicating the date for which you want the offset */
int icaltime_utc_offset(struct icaltimetype tt, const char* tzid);

/* convert tt, of timezone tzid, into a utc time. Does nothing if the
   time is already UTC.  */
struct icaltimetype icaltime_as_utc(struct icaltimetype tt,
				    const char* tzid);

/* convert tt, a time in UTC, into a time in timezone tzid */
struct icaltimetype icaltime_as_zone(struct icaltimetype tt,
				     const char* tzid);

/* end WARNING !! DEPRECATED !! functions *****
 */



/* Return a null time, which indicates no time has been set. This time represent the beginning of the epoch */
struct icaltimetype icaltime_null_time(void);

/* Return true of the time is null. */
int icaltime_is_null_time(struct icaltimetype t);

/* Returns false if the time is clearly invalid, but is not null. This
   is usually the result of creating a new time type buy not clearing
   it, or setting one of the flags to an illegal value. */
int icaltime_is_valid_time(struct icaltimetype t);

/* Reset all of the time components to be in their normal ranges. For
   instance, given a time with minutes=70, the minutes will be reduces
   to 10, and the hour incremented. This allows the caller to do
   arithmetic on times without worrying about overflow or
   underflow. */
struct icaltimetype icaltime_normalize(struct icaltimetype t);

/* Return the day of the year of the given time */
short icaltime_day_of_year(struct icaltimetype t);

/* Create a new time, given a day of year and a year. */
struct icaltimetype icaltime_from_day_of_year(short doy,  short year);

/* Return the day of the week of the given time. Sunday is 1 */
short icaltime_day_of_week(struct icaltimetype t);

/* Return the day of the year for the Sunday of the week that the
   given time is within. */
short icaltime_start_doy_of_week(struct icaltimetype t);

/* Return a string with the time represented in the same format as ctime(). THe string is owned by libical */
char* icaltime_as_ctime(struct icaltimetype);

/* Return the week number for the week the given time is within */
short icaltime_week_number(struct icaltimetype t);

/* Create a new time from a weeknumber and a year. */
struct icaltimetype icaltime_from_week_number(short week_number, short year);

/* Return -1, 0, or 1 to indicate that a<b, a==b or a>b */
int icaltime_compare(struct icaltimetype a,struct icaltimetype b);

/* like icaltime_compare, but only use the date parts. */
int icaltime_compare_date_only(struct icaltimetype a, struct icaltimetype b);

/* Return the number of days in the given month */
short icaltime_days_in_month(short month,short year);

/* Adds or subtracts a number of days, hours, minutes and seconds. */
void  icaltime_adjust(struct icaltimetype *tt, int days, int hours,
		      int minutes, int seconds);

#endif /* !ICALTIME_H */



