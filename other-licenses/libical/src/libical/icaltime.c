/* -*- Mode: C -*-
  ======================================================================
  FILE: icaltime.c
  CREATOR: eric 02 June 2000
  
  $Id: icaltime.c,v 1.2 2001/11/22 19:21:49 mikep%oeone.com Exp $
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
#include <config.h>
#endif

#include "icaltime.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef ICAL_NO_LIBICAL
#define icalerror_set_errno(x)
#define  icalerror_check_arg_rv(x,y)
#define  icalerror_check_arg_re(x,y,z)
#else
#include "icalerror.h"
#include "icalmemory.h"
#endif


#ifdef WIN32
#include <time.h>
#define snprintf	_snprintf
#define strcasecmp	stricmp
#endif

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

/* Structure used by set_tz to hold an old value of TZ, and the new
   value, which is in memory we will have to free in unset_tz */
struct set_tz_save {char* orig_tzid; char* new_env_str;};

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
#ifdef WIN32
	_tzset();
#endif

    return savetz;
}

void unset_tz(struct set_tz_save savetz)
{
    /* restore the original TZ environment */

    char* orig_tzid = savetz.orig_tzid;

    if(orig_tzid!=0){	
	size_t tmp_sz =strlen(orig_tzid)+4; 
	char* orig_env_str = (char*)malloc(tmp_sz);

	if(orig_env_str == 0){
            icalerror_set_errno(ICAL_NEWFAILED_ERROR);
            return;
	}
	
	strcpy(orig_env_str,"TZ=");
	strcpy(orig_env_str+3,orig_tzid);

	putenv(orig_env_str);

	free(orig_tzid);
    } else {
	putenv("TZ"); /* Delete from environment */
    } 

#ifdef WIN32
	_tzset();
#endif

    if(savetz.new_env_str != 0){
	free(savetz.new_env_str);
    }
}


time_t icaltime_as_timet(struct icaltimetype tt)
{
    struct tm stm;
    time_t t;

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
#ifndef WIN32
	struct set_tz_save old_tz = set_tz("UTC");
	t = mktime(&stm);
	unset_tz(old_tz);
#else
	t = mktime(&stm);
	_tzset();
	t -= _timezone;
#endif
   } else {
	t = mktime(&stm);
    }

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

    return icaltime_normalize(tt);
}

/* convert tt, a time in UTC, into a time in timezone tzid */
struct icaltimetype icaltime_as_zone(struct icaltimetype tt,const char* tzid)
{
    int tzid_offset;

    tzid_offset = icaltime_utc_offset(tt,tzid);

    tt.second += tzid_offset;

    tt.is_utc = 0;

    return icaltime_normalize(tt);

}


/* Return the offset of the named zone as seconds. tt is a time
   indicating the date for which you want the offset */
int icaltime_utc_offset(struct icaltimetype ictt, const char* tzid)
{

    time_t tt = icaltime_as_timet(ictt);
    time_t offset_tt;
    struct tm gtm;
    struct set_tz_save old_tz; 

    if(tzid != 0){
	old_tz = set_tz(tzid);
    }
 
#ifndef WIN32
    /* Mis-interpret a UTC broken out time as local time */
    gtm = *(gmtime(&tt));
    gtm.tm_isdst = localtime(&tt)->tm_isdst;    
    offset_tt = mktime(&gtm);
#else
	_tzset();
	offset_tt = _timezone;
	if ( localtime(&tt)->tm_isdst )
		offset_tt -= 3600;
#endif    
    if(tzid != 0){
	unset_tz(old_tz);
    }

#ifndef WIN32
    return tt-offset_tt;
#else
	return -offset_tt;
#endif
}



/* Normalize by converting from localtime to utc and back to local
   time. This uses localtime because localtime and mktime are inverses
   of each other */

struct icaltimetype icaltime_normalize(struct icaltimetype tt)
{
    struct tm stm;
    time_t tut;

    memset(&stm,0,sizeof( struct tm));

    stm.tm_sec = tt.second;
    stm.tm_min = tt.minute;
    stm.tm_hour = tt.hour;
    stm.tm_mday = tt.day;
    stm.tm_mon = tt.month-1;
    stm.tm_year = tt.year-1900;
    stm.tm_isdst = -1; /* prevents mktime from changing hour based on
			  daylight savings */

    tut = mktime(&stm);

    stm = *(localtime(&tut));

    tt.second = stm.tm_sec;
    tt.minute = stm.tm_min;
    tt.hour = stm.tm_hour;
    tt.day = stm.tm_mday;
    tt.month = stm.tm_mon +1;
    tt.year = stm.tm_year+1900;

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


short days_in_month[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

short icaltime_days_in_month(short month,short year)
{

    int is_leap =0;
    int days = days_in_month[month];

    assert(month > 0);
    assert(month <= 12);

    if( (year % 4 == 0 && year % 100 != 0) ||
	year % 400 == 0){
	is_leap =1;
    }

    if( month == 2){
	days += is_leap;
    }

    return days;
}

/* 1-> Sunday, 7->Saturday */
short icaltime_day_of_week(struct icaltimetype t){

    time_t tt = icaltime_as_timet(t);
    struct tm *tm;

    if(t.is_utc == 1){
	tm = gmtime(&tt);
    } else {
	tm = localtime(&tt);
    }

    return tm->tm_wday+1;
}

/* Day of the year that the first day of the week (Sunday) is on  */
short icaltime_start_doy_of_week(struct icaltimetype t){
    time_t tt = icaltime_as_timet(t);
    time_t start_tt;
    struct tm *stm;
    int syear;

    stm = gmtime(&tt);
    syear = stm->tm_year;

    start_tt = tt - stm->tm_wday*(60*60*24);

    stm = gmtime(&start_tt);
    
    if(syear == stm->tm_year){
	return stm->tm_yday+1;
    } else {
	/* return negative to indicate that start of week is in
           previous year. */
	int is_leap = 0;
	int year = stm->tm_year;

	if( (year % 4 == 0 && year % 100 != 0) ||
	    year % 400 == 0){
	    is_leap =1;
	}

	return (stm->tm_yday+1)-(365+is_leap);
    }
    
}

short icaltime_week_number(struct icaltimetype ictt)
{
    char str[5];
    time_t t = icaltime_as_timet(ictt);
    int week_no;

    strftime(str,5,"%V", gmtime(&t));

    week_no = atoi(str);

    return week_no;

}


short icaltime_day_of_year(struct icaltimetype t){
    time_t tt = icaltime_as_timet(t);
    struct tm *stm;

    if(t.is_utc==1){
	stm = gmtime(&tt);
    } else {
	stm = localtime(&tt);
    }

    return stm->tm_yday+1;
    
}

/* Jan 1 is day #1, not 0 */
struct icaltimetype icaltime_from_day_of_year(short doy,  short year)
{
    struct tm stm; 
    time_t tt;
    struct set_tz_save old_tz = set_tz("UTC");

    /* Get the time of january 1 of this year*/
    memset(&stm,0,sizeof(struct tm)); 
    stm.tm_year = year-1900;
    stm.tm_mday = 1;

    tt = mktime(&stm);
    unset_tz(old_tz);


    /* Now add in the days */

    doy--;
    tt += doy *60*60*24;

    return icaltime_from_timet(tt, 1);
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

int icaltime_compare(struct icaltimetype a,struct icaltimetype b)
{
    time_t t1 = icaltime_as_timet(a);
    time_t t2 = icaltime_as_timet(b);

    if (t1 > t2) { 
	return 1; 
    } else if (t1 < t2) { 
	return -1;
    } else { 
	return 0; 
    }

}

int
icaltime_compare_date_only (struct icaltimetype a, struct icaltimetype b)
{
    time_t t1;
    time_t t2;

    if (a.year == b.year && a.month == b.month && a.day == b.day)
        return 0;

    t1 = icaltime_as_timet (a);
    t2 = icaltime_as_timet (b);

    if (t1 > t2) 
	return 1; 
    else if (t1 < t2)
	return -1;
    else {
	/* not reached */
	assert (0);
	return 0;
    }
}


/* These are defined in icalduration.c:
struct icaltimetype  icaltime_add(struct icaltimetype t,
				  struct icaldurationtype  d)
struct icaldurationtype  icaltime_subtract(struct icaltimetype t1,
					   struct icaltimetype t2)
*/

