/* -*- Mode: C -*-*/
/*======================================================================
  FILE: ical.i

  (C) COPYRIGHT 1999 Eric Busboom
  http://www.softwarestudio.org

  The contents of this file are subject to the Mozilla Public License
  Version 1.0 (the "License"); you may not use this file except in
  compliance with the License. You may obtain a copy of the License at
  http://www.mozilla.org/MPL/

  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
  the License for the specific language governing rights and
  limitations under the License.

  The original author is Eric Busboom

  Contributions from:
  Graham Davison (g.m.davison@computer.org)

  ======================================================================*/  

%module Net__ICal__Libical

%{
#include "ical.h"

#include <sys/types.h> /* for size_t */
#include <time.h>

%}



typedef void icalcomponent;
typedef void icalproperty;

icalcomponent* icalparser_parse_string(char* str);


icalcomponent* icalcomponent_new(icalcomponent_kind kind);
icalcomponent* icalcomponent_new_clone(icalcomponent* component);
icalcomponent* icalcomponent_new_from_string(char* str);

char* icalcomponent_as_ical_string(icalcomponent* component);

void icalcomponent_free(icalcomponent* component);
int icalcomponent_count_errors(icalcomponent* component);
void icalcomponent_strip_errors(icalcomponent* component);
void icalcomponent_convert_errors(icalcomponent* component);

icalproperty* icalcomponent_get_current_property(icalcomponent* component);

icalproperty* icalcomponent_get_first_property(icalcomponent* component,
					      icalproperty_kind kind);
icalproperty* icalcomponent_get_next_property(icalcomponent* component,
					      icalproperty_kind kind);

icalcomponent* icalcomponent_get_current_component (icalcomponent* component);

icalcomponent* icalcomponent_get_first_component(icalcomponent* component,
					      icalcomponent_kind kind);
icalcomponent* icalcomponent_get_next_component(icalcomponent* component,
					      icalcomponent_kind kind);

void icalcomponent_add_property(icalcomponent* component,
				icalproperty* property);

void icalcomponent_remove_property(icalcomponent* component,
				   icalproperty* property);


icalcomponent* icalcomponent_get_parent(icalcomponent* component);

icalcomponent_kind icalcomponent_isa(icalcomponent* component);

int icalrestriction_check(icalcomponent* comp);


/* actually returns icalproperty_kind */
int icalproperty_string_to_kind(const char* string); 

/* actually takes icalproperty_kind */
icalproperty* icalproperty_new(int kind);

icalproperty* icalproperty_new_from_string(char* str);

char* icalproperty_as_ical_string(icalproperty *prop);

void icalproperty_set_parameter_from_string(icalproperty* prop,
                                          const char* name, const char* value);
void icalproperty_set_value_from_string(icalproperty* prop,const char* value, const char * kind);

const char* icalproperty_get_value_as_string(icalproperty* prop);
const char* icalproperty_get_parameter_as_string(icalproperty* prop,
                                                 const char* name);


icalcomponent* icalproperty_get_parent(icalproperty* property);

typedef enum icalerrorenum {
    
    ICAL_BADARG_ERROR,
    ICAL_NEWFAILED_ERROR,
    ICAL_MALFORMEDDATA_ERROR, 
    ICAL_PARSE_ERROR,
    ICAL_INTERNAL_ERROR, /* Like assert --internal consist. prob */
    ICAL_FILE_ERROR,
    ICAL_ALLOCATION_ERROR,
    ICAL_USAGE_ERROR,
    ICAL_NO_ERROR,
    ICAL_UNKNOWN_ERROR /* Used for problems in input to icalerror_strerror()*/

} icalerrorenum;

/* Make an individual error fatal or non-fatal. */
typedef enum icalererorstate { 
    ICAL_ERROR_FATAL,     /* Not fata */
    ICAL_ERROR_NONFATAL,  /* Fatal */
    ICAL_ERROR_DEFAULT,   /* Use the value of icalerror_errors_are_fatal*/
    ICAL_ERROR_UNKNOWN    /* Asked state for an unknown error type */
} icalerrorstate ;

void icalerror_set_error_state( icalerrorenum error, icalerrorstate);
icalerrorstate icalerror_get_error_state( icalerrorenum error);


const char* icalenum_property_kind_to_string(icalproperty_kind kind);
icalproperty_kind icalenum_string_to_property_kind(const char* string);

const char* icalenum_value_kind_to_string(icalvalue_kind kind);
/*icalvalue_kind icalenum_value_kind_by_prop(icalproperty_kind kind);*/

const char* icalenum_parameter_kind_to_string(icalparameter_kind kind);
icalparameter_kind icalenum_string_to_parameter_kind(const char* string);

const char* icalenum_component_kind_to_string(icalcomponent_kind kind);
icalcomponent_kind icalenum_string_to_component_kind(const char* string);

icalvalue_kind icalenum_property_kind_to_value_kind(icalproperty_kind kind);


int* icallangbind_new_array(int size);
void icallangbind_free_array(int* array);
int icallangbind_access_array(int* array, int index);
int icalrecur_expand_recurrence(char* rule, int start, 
				int count, int* array);


/* Iterate through properties and components using strings for the kind */
icalproperty* icallangbind_get_first_property(icalcomponent *c,
                                              const char* prop);

icalproperty* icallangbind_get_next_property(icalcomponent *c,
                                              const char* prop);

icalcomponent* icallangbind_get_first_component(icalcomponent *c,
                                              const char* comp);

icalcomponent* icallangbind_get_next_component(icalcomponent *c,
                                              const char* comp);


/* Return a string that can be evaluated in perl or python to
   generated a hash that holds the property's name, value and
   parameters. Sep is the hash seperation string, "=>" for perl and
   ":" for python */
const char* icallangbind_property_eval_string(icalproperty* prop, char* sep);

/***********************************************************************
 Time routines 
***********************************************************************/


struct icaltimetype
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;

	int is_utc; /* 1-> time is in UTC timezone */

	int is_date; /* 1 -> interpret this as date. */
   
	const char* zone; /*Ptr to Olsen placename. Libical does not own mem*/
};	


/* Convert seconds past UNIX epoch to a timetype*/
struct icaltimetype icaltime_from_timet(int v, int is_date);

/* Return the time as seconds past the UNIX epoch */
/* Normally, this returns a time_t, but SWIG tries to turn that type
   into a pointer */
int icaltime_as_timet(struct icaltimetype);

/* Return a string represention of the time, in RFC2445 format. The
   string is owned by libical */
char* icaltime_as_ical_string(struct icaltimetype tt);

/* create a time from an ISO format string */
struct icaltimetype icaltime_from_string(const char* str);

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

/* Return the day of the week of the given time. Sunday is 0 */
short icaltime_day_of_week(struct icaltimetype t);

/* Return the day of the year for the Sunday of the week that the
   given time is within. */
short icaltime_start_doy_of_week(struct icaltimetype t);

/* Return a string with the time represented in the same format as ctime(). THe string is owned by libical */
char* icaltime_as_ctime(struct icaltimetype);

/* Return the week number for the week the given time is within */
short icaltime_week_number(struct icaltimetype t);

/* Return -1, 0, or 1 to indicate that a<b, a==b or a>b */
int icaltime_compare(struct icaltimetype a,struct icaltimetype b);

/* like icaltime_compare, but only use the date parts. */
int icaltime_compare_date_only(struct icaltimetype a, struct icaltimetype b);

/* Return the number of days in the given month */
short icaltime_days_in_month(short month,short year);


/***********************************************************************
  Duration Routines 
***********************************************************************/


struct icaldurationtype
{
	int is_neg;
	unsigned int days;
	unsigned int weeks;
	unsigned int hours;
	unsigned int minutes;
	unsigned int seconds;
};

struct icaldurationtype icaldurationtype_from_int(int t);
struct icaldurationtype icaldurationtype_from_string(const char*);
int icaldurationtype_as_int(struct icaldurationtype duration);
char* icaldurationtype_as_ical_string(struct icaldurationtype d);
struct icaldurationtype icaldurationtype_null_duration();
int icaldurationtype_is_null_duration(struct icaldurationtype d);

struct icaltimetype  icaltime_add(struct icaltimetype t,
				  struct icaldurationtype  d);

struct icaldurationtype  icaltime_subtract(struct icaltimetype t1,
					   struct icaltimetype t2);


/***********************************************************************
  Period Routines 
***********************************************************************/


struct icalperiodtype 
{
	struct icaltimetype start;
	struct icaltimetype end;
	struct icaldurationtype duration;
};

struct icalperiodtype icalperiodtype_from_string (const char* str);

const char* icalperiodtype_as_ical_string(struct icalperiodtype p);
struct icalperiodtype icalperiodtype_null_period();
int icalperiodtype_is_null_period(struct icalperiodtype p);
int icalperiodtype_is_valid_period(struct icalperiodtype p);

