/* -*- Mode: C -*-
  ======================================================================
  FILE: icaltime.c
  CREATOR: eric 02 June 2000
  
  $Id: icaltime.c,v 1.7 2003/10/08 06:44:35 cls%seawood.org Exp $
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icaltime.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define JDAY

#ifdef JDAY
#include "astime.h"
#endif

/*
#ifdef ICAL_NO_LIBICAL
#define icalerror_set_errno(x)
#define  icalerror_check_arg_rv(x,y)
#define  icalerror_check_arg_re(x,y,z)
#endif
*/

#include "icalerror.h"
#include "icalmemory.h"

#include "icaltimezone.h"

#ifdef WIN32
#include <windows.h>

#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif

#ifdef XP_MAC
#include "prenv.h"
#define getenv PR_GetEnv
#define putenv PR_SetEnv
#endif

/**********************************************************************
 * Deprecated TIme functions
 * The following four routines are required under UNIX systems to account for the lack 
 * of two time routines, a version of mktime() that operates in UTC ( the inverse of 
 * gmtime() ), and a routine to get the UTC offset for a particular timezone. 
 ***********************************************************************/


/* Structure used by set_tz to hold an old value of TZ, and the new
   value, which is in memory we will have to free in unset_tz */
struct set_tz_save {char* orig_tzid; char* new_env_str;};

/* Structure used by set_tz to hold an old value of TZ, and the new
   value, which is in memory we will have to free in unset_tz */
/* This will hold the last "TZ=XXX" string we used with putenv(). After we
   call putenv() again to set a new TZ string, we can free the previous one.
   As far as I know, no libc implementations actually free the memory used in
   the environment variables (how could they know if it is a static string or
   a malloc'ed string?), so we have to free it ourselves. */

struct set_tz_save saved_tz;


/* Temporarily change the TZ environmental variable. */
struct set_tz_save set_tz(const char* tzid)
{

    char *orig_tzid = 0;
    char *new_env_str;
    struct set_tz_save savetz;
    size_t tmp_sz; 

    savetz.orig_tzid = 0;
    savetz.new_env_str = 0;

    if(getenv("TZ") != 0){
	orig_tzid = (char*)icalmemory_strdup(getenv("TZ"));

	if(orig_tzid == 0){
            icalerror_set_errno(ICAL_NEWFAILED_ERROR);
			free(orig_tzid);
	    return savetz;
	}
    }

    tmp_sz =strlen(tzid)+4; 
    new_env_str = (char*)malloc(tmp_sz);

    if(new_env_str == 0){
        icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return savetz;
    }
    
    /* Copy the TZid into a string with the form that putenv expects. */
    strcpy(new_env_str,"TZ=");
    strcpy(new_env_str+3,tzid);

    putenv(new_env_str); 

    /* Old value of TZ and the string we will have to free later */
    savetz.orig_tzid = orig_tzid;
    savetz.new_env_str = new_env_str;

    return savetz;
}

void unset_tz(struct set_tz_save savetz)
{
    /* restore the original TZ environment */

    char* orig_tzid = savetz.orig_tzid;

    if(orig_tzid!=0){	
	size_t tmp_sz =strlen(orig_tzid)+4; 
	char* orig_env_str = (char*)icalmemory_tmp_buffer(tmp_sz);

	if(orig_env_str == 0){
            icalerror_set_errno(ICAL_NEWFAILED_ERROR);
            return;
	}
	
	strcpy(orig_env_str,"TZ=");
	strcpy(orig_env_str+3,orig_tzid);

	putenv(orig_env_str);

	free(orig_tzid);
    } else {
	putenv("TZ="); /* Delete from environment */
    } 

    if(savetz.new_env_str != 0){
	free(savetz.new_env_str);
    }
}

/* A UTC version of mktime() */
time_t icaltimegm(struct tm* stm)
{

    time_t t;
#ifndef WIN32
    struct set_tz_save old_tz = set_tz("UTC");
    t = mktime(stm);
    unset_tz(old_tz);
    return t;
#else
    TIME_ZONE_INFORMATION tz;
    char * szZone;
    icaltimezone* zone;
    int offset_tt;
    
    t = mktime(stm);
    
    GetTimeZoneInformation(&tz);
    
    t -= tz.Bias*60;

    return t;
#endif


}

/* Get the offset for a named timezone. */
int icaltime_utc_offset(struct icaltimetype ictt, const char* tzid)
{
    time_t offset_tt;
#ifndef WIN32
    icaltimezone *utc_zone, *local_zone;
    struct tm gtm;
    struct set_tz_save old_tz; 

    time_t tt = icaltime_as_timet(ictt);

    local_zone =  icaltimezone_get_builtin_timezone(tzid);
    utc_zone = icaltimezone_get_utc_timezone ();

    icaltimezone_get_utc_offset(local_zone, &ictt, &ictt.is_daylight);


#ifndef NO_WARN_DEPRECATED
	fprintf(stderr, "%s: %d: WARNING: icaltime_utc_offset is deprecated\n", __FILE__, __LINE__);
#endif	
	/* This function, combined with the set_tz/unset_tz calls in regression.c
	makes for very convoluted behaviour (due to the setting of the tz environment variable)
	arrghgh, at least this is what I think: benjaminlee */
	
    if(tzid != 0){
	old_tz = set_tz(tzid);
    }
 
    /* Mis-interpret a UTC broken out time as local time */
    gtm = *(gmtime(&tt));
    gtm.tm_isdst = localtime(&tt)->tm_isdst;    
    offset_tt = mktime(&gtm);

    if(tzid != 0){
	unset_tz(old_tz);
    }

#if 0
#ifndef NO_WARN_DEPRECATED
	fprintf(stderr, "%s: %d: WARNING: offset %d tt-offset_tt %d\n", __FILE__, __LINE__, offset, tt-offset_tt);
#endif	
	assert(offset == tt-offset_tt);
#endif
    return tt-offset_tt;
#else
	int is_daylight;

	if ( tzid == 0 )
	{
		TIME_ZONE_INFORMATION tz;

		GetTimeZoneInformation(&tz);

		return -tz.Bias*60;
	}
	else
	{
		icaltimezone* zone = icaltimezone_get_builtin_timezone(tzid);


		offset_tt = icaltimezone_get_utc_offset_of_utc_time     (zone,
											   &ictt,
											   &is_daylight);

		return offset_tt;
	}
#endif
}

/***********************************************************************/


/***********************************************************************
 * Old conversion routines that use a string to represent the timezone 
 ***********************************************************************/

/* convert tt, of timezone tzid, into a utc time */
struct icaltimetype icaltime_as_utc(struct icaltimetype tt,const char* tzid)
{
    int tzid_offset;

    if(tt.is_utc == 1 || tt.is_date == 1){
	return tt;
    }

    tzid_offset = icaltime_utc_offset(tt,tzid);

    tt.second -= tzid_offset;

    tt.is_utc = 1;

#ifndef NO_WARN_DEPRECATED
	fprintf(stderr, "%s: %d: WARNING: icaltime_as_utc is deprecated: '%s'\n", __FILE__, __LINE__, tzid);
#endif	
    return icaltime_normalize(tt);
}

/* convert tt, a time in UTC, into a time in timezone tzid */
struct icaltimetype icaltime_as_zone(struct icaltimetype tt,const char* tzid)
{
    int tzid_offset;

    tzid_offset = icaltime_utc_offset(tt,tzid);

    tt.second += tzid_offset;

    tt.is_utc = 0;

#ifndef NO_WARN_DEPRECATED
	fprintf(stderr, "%s: %d: WARNING: icaltime_as_zone is deprecated: '%s'\n", __FILE__, __LINE__, tzid);
#endif	
    return icaltime_normalize(tt);

}

/* **********************************************************************/


struct icaltimetype 
icaltime_from_timet(time_t tm, int is_date)
{
    struct icaltimetype tt = icaltime_null_time();
    struct tm t;

    t = *(gmtime(&tm));
     
    if(is_date == 0){ 
	tt.second = t.tm_sec;
	tt.minute = t.tm_min;
	tt.hour = t.tm_hour;
    } else {
	tt.second = tt.minute =tt.hour = 0 ;
    }

    tt.day = t.tm_mday;
    tt.month = t.tm_mon + 1;
    tt.year = t.tm_year+ 1900;
    
    tt.is_utc = 1;
    tt.is_date = is_date; 

    return tt;
}

struct icaltimetype 
icaltime_from_timet_with_zone(time_t tm, int is_date, icaltimezone *zone)
{
    struct icaltimetype tt;
    struct tm t;
    icaltimezone *utc_zone;

    utc_zone = icaltimezone_get_utc_timezone ();

    /* Convert the time_t to a struct tm in UTC time. We can trust gmtime
       for this. */
    t = *(gmtime(&tm));
     
    tt.year   = t.tm_year + 1900;
    tt.month  = t.tm_mon + 1;
    tt.day    = t.tm_mday;

    tt.is_utc = (zone == utc_zone) ? 1 : 0;
    tt.is_date = is_date; 
    tt.is_daylight = 0;
    tt.zone = NULL;

    if (is_date) { 
	/* We don't convert DATE values between timezones. */
	tt.hour   = 0;
	tt.minute = 0;
	tt.second = 0;
    } else {
	tt.hour   = t.tm_hour;
	tt.minute = t.tm_min;
	tt.second = t.tm_sec;

	/* Use our timezone functions to convert to the required timezone. */
	icaltimezone_convert_time (&tt, utc_zone, zone);
    }

    return tt;
}

/* Returns the current time in the given timezone, as an icaltimetype. */
struct icaltimetype icaltime_current_time_with_zone(icaltimezone *zone)
{
    return icaltime_from_timet_with_zone (time (NULL), 0, zone);
}

/* Returns the current day as an icaltimetype, with is_date set. */
struct icaltimetype icaltime_today(void)
{
    return icaltime_from_timet_with_zone (time (NULL), 1, NULL);
}


time_t icaltime_as_timet(struct icaltimetype tt)
{
    struct tm stm;
    time_t t;
#ifdef WIN32
    TIME_ZONE_INFORMATION tz;
    char * szZone;
    icaltimezone* zone;
    int offset_tt;
#endif
    memset(&stm,0,sizeof( struct tm));

    if(icaltime_is_null_time(tt)) {
	return 0;
    }

    stm.tm_sec = tt.second;
    stm.tm_min = tt.minute;
    stm.tm_hour = tt.hour;
    stm.tm_mday = tt.day;
    stm.tm_mon = tt.month-1;
    stm.tm_year = tt.year-1900;
    stm.tm_isdst = -1;

    if(tt.is_utc == 1 || tt.is_date == 1){
        t = icaltimegm(&stm);
    } else {
	t = mktime(&stm);
#ifdef WIN32
	/* Arg, mktime on Win32 always returns the time is localtime
           zone, so we'll figure out what time we are looking for and adjust it */
        
	szZone = getenv("TZ");
	if ( szZone != NULL && strlen(szZone) != 0 ){
            GetTimeZoneInformation(&tz);
            zone = icaltimezone_get_builtin_timezone(szZone);
            offset_tt = icaltimezone_get_utc_offset_of_utc_time	(zone,
                                                                 &tt,
                                                                 NULL);		
            t += -offset_tt - tz.Bias*60;
        }
        
#endif
    }

    return t;

}

time_t icaltime_as_timet_with_zone(struct icaltimetype tt, icaltimezone *zone)
{
    icaltimezone *utc_zone;
    struct tm stm;
    time_t t;

    utc_zone = icaltimezone_get_utc_timezone ();

    /* If the time is the special null time, return 0. */
    if (icaltime_is_null_time(tt)) {
	return 0;
    }

    /* Use our timezone functions to convert to UTC. */
    if (!tt.is_date)
	icaltimezone_convert_time (&tt, zone, utc_zone);

    /* Copy the icaltimetype to a struct tm. */
    memset (&stm, 0, sizeof (struct tm));

    stm.tm_sec = tt.second;
    stm.tm_min = tt.minute;
    stm.tm_hour = tt.hour;
    stm.tm_mday = tt.day;
    stm.tm_mon = tt.month-1;
    stm.tm_year = tt.year-1900;
    stm.tm_isdst = -1;

    t = icaltimegm(&stm);   

    return t;
}

char* icaltime_as_ical_string(struct icaltimetype tt)
{
    size_t size = 17;
    char* buf = icalmemory_new_buffer(size);

    if(tt.is_date){
	snprintf(buf, size,"%04d%02d%02d",tt.year,tt.month,tt.day);
    } else {
	char* fmt;
	if(tt.is_utc){
	    fmt = "%04d%02d%02dT%02d%02d%02dZ";
	} else {
	    fmt = "%04d%02d%02dT%02d%02d%02d";
	}
	snprintf(buf, size,fmt,tt.year,tt.month,tt.day,
		 tt.hour,tt.minute,tt.second);
    }
    
    icalmemory_add_tmp_buffer(buf);

    return buf;

}

/* Normalize the icaltime, so that all fields are within the normal range. */

struct icaltimetype icaltime_normalize(struct icaltimetype tt)
{
  icaltime_adjust (&tt, 0, 0, 0, 0);
    return tt;
}


#ifndef ICAL_NO_LIBICAL
#include "icalvalue.h"

struct icaltimetype icaltime_from_string(const char* str)
{
    struct icaltimetype tt = icaltime_null_time();
    int size;

    icalerror_check_arg_re(str!=0,"str",icaltime_null_time());

    size = strlen(str);
    
    if(size == 15) { /* floating time */
	tt.is_utc = 0;
	tt.is_date = 0;
    } else if (size == 16) { /* UTC time, ends in 'Z'*/
	tt.is_utc = 1;
	tt.is_date = 0;

	if(str[15] != 'Z'){
	    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
	    return icaltime_null_time();
	}
	    
    } else if (size == 8) { /* A DATE */
	tt.is_utc = 1;
	tt.is_date = 1;
    } else { /* error */
	icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
	return icaltime_null_time();
    }

    if(tt.is_date == 1){
	sscanf(str,"%04d%02d%02d",&tt.year,&tt.month,&tt.day);
    } else {
	char tsep;
	sscanf(str,"%04d%02d%02d%c%02d%02d%02d",&tt.year,&tt.month,&tt.day,
	       &tsep,&tt.hour,&tt.minute,&tt.second);

	if(tsep != 'T'){
	    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
	    return icaltime_null_time();
	}

    }

    return tt;    
}
#endif

char ctime_str[32];
char* icaltime_as_ctime(struct icaltimetype t)
{
    time_t tt;
 
    tt = icaltime_as_timet(t);
    sprintf(ctime_str,"%s",ctime(&tt));

    ctime_str[strlen(ctime_str)-1] = 0;

    return ctime_str;
}

/* Returns whether the specified year is a leap year. Year is the normal year,
   e.g. 2001. */
int
icaltime_is_leap_year (int year)
{

    if (year <= 1752)
        return (year % 4 == 0);
    else
        return ( (year % 4==0) && (year % 100 !=0 )) || (year % 400 == 0);
}

short days_in_month[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

short icaltime_days_in_month(short month,short year)
{

    int days = days_in_month[month];

    assert(month > 0);
    assert(month <= 12);

    if( month == 2){
	days += icaltime_is_leap_year(year);
    }

    return days;
}

/* 1-> Sunday, 7->Saturday */
short icaltime_day_of_week(struct icaltimetype t){
#ifndef JDAY 
	struct tm stm;

    stm.tm_year = t.year - 1900;
    stm.tm_mon = t.month - 1;
    stm.tm_mday = t.day;
    stm.tm_hour = 0;
    stm.tm_min = 0;
    stm.tm_sec = 0;
    stm.tm_isdst = -1;

    mktime (&stm);

    return stm.tm_wday + 1;
#else /* JDAY implementation */
	UTinstant jt;

	memset(&jt,0,sizeof(UTinstant));

	jt.year = t.year;
    jt.month = t.month;
    jt.day = t.day;
    jt.i_hour = 0;
    jt.i_minute = 0;
    jt.i_second = 0;

	juldat(&jt);

	return jt.weekday + 1;
#endif
}

/* Day of the year that the first day of the week (Sunday) is on.
   FIXME: Doesn't take into account different week start days. */
short icaltime_start_doy_of_week(struct icaltimetype t){
#ifndef JDAY
    struct tm stm;
    int start;

    stm.tm_year = t.year - 1900;
    stm.tm_mon = t.month - 1;
    stm.tm_mday = t.day;
    stm.tm_hour = 0;
    stm.tm_min = 0;
    stm.tm_sec = 0;
    stm.tm_isdst = -1;

    mktime (&stm);

    /* Move back to the start of the week. */
    stm.tm_mday -= stm.tm_wday;
    
    mktime (&stm);


    /* If we are still in the same year as the original date, we just return
       the day of the year. */
    if (t.year - 1900 == stm.tm_year){
	start = stm.tm_yday+1;
    } else {
	/* return negative to indicate that start of week is in
           previous year. */

	int is_leap = icaltime_is_leap_year(stm.tm_year+1900);

	start = (stm.tm_yday+1)-(365+is_leap);
    }

    return start;
#else
	UTinstant jt;

	memset(&jt,0,sizeof(UTinstant));

	jt.year = t.year;
    jt.month = t.month;
    jt.day = t.day;
    jt.i_hour = 0;
    jt.i_minute = 0;
    jt.i_second = 0;

	juldat(&jt);
	caldat(&jt);

	return jt.day_of_year - jt.weekday;
#endif
}

/* FIXME: Doesn't take into account the start day of the week. strftime assumes
   that weeks start on Monday. */
short icaltime_week_number(struct icaltimetype ictt)
{
#ifndef JDAY
    struct tm stm;
    int week_no;
    char str[8];

    stm.tm_year = ictt.year - 1900;
    stm.tm_mon = ictt.month - 1;
    stm.tm_mday = ictt.day;
    stm.tm_hour = 0;
    stm.tm_min = 0;
    stm.tm_sec = 0;
    stm.tm_isdst = -1;

    mktime (&stm);
 
    strftime(str,5,"%V", &stm);

    week_no = atoi(str);

    return week_no;
#else
	UTinstant jt;

	memset(&jt,0,sizeof(UTinstant));

	jt.year = ictt.year;
    jt.month = ictt.month;
    jt.day = ictt.day;
    jt.i_hour = 0;
    jt.i_minute = 0;
    jt.i_second = 0;

	juldat(&jt);
	caldat(&jt);

	return (jt.day_of_year - jt.weekday) / 7;
#endif
}

/* The first array is for non-leap years, the seoncd for leap years*/
static const short days_in_year[2][13] = 
{ /* jan feb mar apr may  jun  jul  aug  sep  oct  nov  dec */
  {  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 }, 
  {  0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

/* Returns the day of the year, counting from 1 (Jan 1st). */
short icaltime_day_of_year(struct icaltimetype t){
  int is_leap = icaltime_is_leap_year (t.year);

  return days_in_year[is_leap][t.month - 1] + t.day;
}

/* Jan 1 is day #1, not 0 */
struct icaltimetype icaltime_from_day_of_year(short doy,  short year)
{
    struct icaltimetype tt={0};
    int is_leap;
    int month;

    is_leap = icaltime_is_leap_year(year);

    /* Zero and neg numbers represent days  of the previous year */
    if(doy <1){
        year--;
        is_leap = icaltime_is_leap_year(year);
        doy +=  days_in_year[is_leap][12];
    } else if(doy > days_in_year[is_leap][12]){
        /* Move on to the next year*/
        is_leap = icaltime_is_leap_year(year);
        doy -=  days_in_year[is_leap][12];
        year++;
    }

    tt.year = year;

    for (month = 11; month >= 0; month--) {
      if (doy > days_in_year[is_leap][month]) {
	tt.month = month + 1;
	tt.day = doy - days_in_year[is_leap][month];
	return tt;
      }
    }

    /* Shouldn't reach here. */
    assert (0);
}

struct icaltimetype icaltime_null_time()
{
    struct icaltimetype t;
    memset(&t,0,sizeof(struct icaltimetype));

    return t;
}


int icaltime_is_valid_time(struct icaltimetype t){
    if(t.is_utc > 1 || t.is_utc < 0 ||
       t.year < 0 || t.year > 3000 ||
       t.is_date > 1 || t.is_date < 0){
	return 0;
    } else {
	return 1;
    }

}

int icaltime_is_null_time(struct icaltimetype t)
{
    if (t.second +t.minute+t.hour+t.day+t.month+t.year == 0){
	return 1;
    }

    return 0;

}

int icaltime_compare(struct icaltimetype a, struct icaltimetype b)
{
    int retval;

    if (a.year > b.year)
	retval = 1;
    else if (a.year < b.year)
	retval = -1;

    else if (a.month > b.month)
	retval = 1;
    else if (a.month < b.month)
	retval = -1;

    else if (a.day > b.day)
	retval = 1;
    else if (a.day < b.day)
	retval = -1;

    else if (a.hour > b.hour)
	retval = 1;
    else if (a.hour < b.hour)
	retval = -1;

    else if (a.minute > b.minute)
	retval = 1;
    else if (a.minute < b.minute)
	retval = -1;

    else if (a.second > b.second)
	retval = 1;
    else if (a.second < b.second)
	retval = -1;

    else
	retval = 0;

    return retval;
}

int
icaltime_compare_date_only (struct icaltimetype a, struct icaltimetype b)
{
    int retval;

    if (a.year > b.year)
	retval = 1;
    else if (a.year < b.year)
	retval = -1;

    else if (a.month > b.month)
	retval = 1;
    else if (a.month < b.month)
	retval = -1;

    else if (a.day > b.day)
	retval = 1;
    else if (a.day < b.day)
	retval = -1;

    else
	retval = 0;

    return retval;
}

/* These are defined in icalduration.c:
struct icaltimetype  icaltime_add(struct icaltimetype t,
				  struct icaldurationtype  d)
struct icaldurationtype  icaltime_subtract(struct icaltimetype t1,
					   struct icaltimetype t2)
*/



/* Adds (or subtracts) a time from a icaltimetype.
   NOTE: This function is exactly the same as icaltimezone_adjust_change()
   except for the type of the first parameter. */
void
icaltime_adjust				(struct icaltimetype	*tt,
					 int		 days,
					 int		 hours,
					 int		 minutes,
					 int		 seconds)
{
    int second, minute, hour, day;
    int minutes_overflow, hours_overflow, days_overflow, years_overflow;
    int days_in_month;

    /* Add on the seconds. */
    second = tt->second + seconds;
    tt->second = second % 60;
    minutes_overflow = second / 60;
    if (tt->second < 0) {
	tt->second += 60;
	minutes_overflow--;
    }

    /* Add on the minutes. */
    minute = tt->minute + minutes + minutes_overflow;
    tt->minute = minute % 60;
    hours_overflow = minute / 60;
    if (tt->minute < 0) {
	tt->minute += 60;
	hours_overflow--;
    }

    /* Add on the hours. */
    hour = tt->hour + hours + hours_overflow;
    tt->hour = hour % 24;
    days_overflow = hour / 24;
    if (tt->hour < 0) {
	tt->hour += 24;
	days_overflow--;
    }

    /* Normalize the month. We do this before handling the day since we may
       need to know what month it is to get the number of days in it.
       Note that months are 1 to 12, so we have to be a bit careful. */
    if (tt->month >= 13) {
	years_overflow = (tt->month - 1) / 12;
	tt->year += years_overflow;
	tt->month -= years_overflow * 12;
    } else if (tt->month <= 0) {
	/* 0 to -11 is -1 year out, -12 to -23 is -2 years. */
	years_overflow = (tt->month / 12) - 1;
	tt->year += years_overflow;
	tt->month -= years_overflow * 12;
    }

    /* Add on the days. */
    day = tt->day + days + days_overflow;
    if (day > 0) {
	for (;;) {
	    days_in_month = icaltime_days_in_month (tt->month, tt->year);
	    if (day <= days_in_month)
		break;

	    tt->month++;
	    if (tt->month >= 13) {
		tt->year++;
		tt->month = 1;
	    }

	    day -= days_in_month;
	}
    } else {
	while (day <= 0) {
	    if (tt->month == 1) {
		tt->year--;
		tt->month = 12;
	    } else {
		tt->month--;
	    }

	    day += icaltime_days_in_month (tt->month, tt->year);
	}
    }
    tt->day = day;
}
