/* -*- Mode: C -*-
  ======================================================================
  FILE: regression.c
  CREATOR: eric 03 April 1999
  
  DESCRIPTION:
  
  $Id: regression.c,v 1.4 2002/03/14 15:18:03 mikep%oeone.com Exp $
  $Locker:  $

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
  The original code is regression.c

    
  ======================================================================*/

#include "ical.h"
#include "icalss.h"

#include <assert.h>
#include <string.h> /* for strdup */
#include <stdlib.h> /* for malloc */
#include <stdio.h> /* for printf */
#include <time.h> /* for time() */
#ifndef WIN32
#include <unistd.h> /* for unlink, fork */
#include <sys/wait.h> /* For waitpid */
#include <sys/time.h> /* for select */
#else
#include <Windows.h>
#endif
#include <sys/types.h> /* For wait pid */

/* For GNU libc, strcmp appears to be a macro, so using strcmp in
 assert results in incomprehansible assertion messages. This
 eliminates the problem */

int regrstrcmp(const char* a, const char* b){
    return strcmp(a,b);
}

/* This example creates and minipulates the ical object that appears
 * in rfc 2445, page 137 */

char str[] = "BEGIN:VCALENDAR\
PRODID:\"-//RDU Software//NONSGML HandCal//EN\"\
VERSION:2.0\
BEGIN:VTIMEZONE\
TZID:America/New_York\
BEGIN:STANDARD\
DTSTART:19981025T020000\
RDATE:19981025T020000\
TZOFFSETFROM:-0400\
TZOFFSETTO:-0500\
TZNAME:EST\
END:STANDARD\
BEGIN:DAYLIGHT\
DTSTART:19990404T020000\
RDATE:19990404T020000\
TZOFFSETFROM:-0500\
TZOFFSETTO:-0400\
TZNAME:EDT\
END:DAYLIGHT\
END:VTIMEZONE\
BEGIN:VEVENT\
DTSTAMP:19980309T231000Z\
UID:guid-1.host1.com\
ORGANIZER;ROLE=CHAIR:MAILTO:mrbig@host.com\
ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com\
DESCRIPTION:Project XYZ Review Meeting\
CATEGORIES:MEETING\
CLASS:PUBLIC\
CREATED:19980309T130000Z\
SUMMARY:XYZ Project Review\
DTSTART;TZID=America/New_York:19980312T083000\
DTEND;TZID=America/New_York:19980312T093000\
LOCATION:1CP Conference Room 4350\
END:VEVENT\
BEGIN:BOOGA\
DTSTAMP:19980309T231000Z\
X-LIC-FOO:Booga\
DTSTOMP:19980309T231000Z\
UID:guid-1.host1.com\
END:BOOGA\
END:VCALENDAR";


icalcomponent* create_simple_component()
{

    icalcomponent* calendar;
    struct icalperiodtype rtime;

    rtime.start = icaltime_from_timet( time(0),0);
    rtime.end = icaltime_from_timet( time(0),0);

    rtime.end.hour++;



    /* Create calendar and add properties */
    calendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);

    
    icalcomponent_add_property(
	calendar,
	icalproperty_new_version("2.0")
	);

    printf("%s\n",icalcomponent_as_ical_string(calendar));
 
    return calendar;
	   
}

/* Create a new component */
icalcomponent* create_new_component()
{

    icalcomponent* calendar;
    icalcomponent* timezone;
    icalcomponent* tzc;
    icalcomponent* event;
    struct icaltimetype atime = icaltime_from_timet( time(0),0);
    struct icaldatetimeperiodtype rtime;
    icalproperty* property;

    rtime.period.start = icaltime_from_timet( time(0),0);
    rtime.period.end = icaltime_from_timet( time(0),0);
    rtime.period.end.hour++;
    rtime.time = icaltime_null_time();



    /* Create calendar and add properties */
    calendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);

    
    icalcomponent_add_property(
	calendar,
	icalproperty_new_version("2.0")
	);
    
    icalcomponent_add_property(
	calendar,
	icalproperty_new_prodid("-//RDU Software//NONSGML HandCal//EN")
	);
    
    /* Create a timezone object and add it to the calendar */

    timezone = icalcomponent_new(ICAL_VTIMEZONE_COMPONENT);

    icalcomponent_add_property(
	timezone,
	icalproperty_new_tzid("America/New_York")
	);

    /* Add a sub-component of the timezone */
    tzc = icalcomponent_new(ICAL_XDAYLIGHT_COMPONENT);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_dtstart(atime)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_rdate(rtime)
	);
	    
    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzoffsetfrom(-4.0)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzoffsetto(-5.0)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzname("EST")
	);

    icalcomponent_add_component(timezone,tzc);

    icalcomponent_add_component(calendar,timezone);

    /* Add a second subcomponent */
    tzc = icalcomponent_new(ICAL_XSTANDARD_COMPONENT);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_dtstart(atime)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_rdate(rtime)
	);
	    
    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzoffsetfrom(-4.0)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzoffsetto(-5.0)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzname("EST")
	);

    icalcomponent_add_component(timezone,tzc);

    /* Add an event */

    event = icalcomponent_new(ICAL_VEVENT_COMPONENT);

    icalcomponent_add_property(
	event,
	icalproperty_new_dtstamp(atime)
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_uid("guid-1.host1.com")
	);

    /* add a property that has parameters */
    property = icalproperty_new_organizer("mrbig@host.com");
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_role(ICAL_ROLE_CHAIR)
	);

    icalcomponent_add_property(event,property);

    /* add another property that has parameters */
    property = icalproperty_new_attendee("employee-A@host.com");
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_role(ICAL_ROLE_REQPARTICIPANT)
	);

    icalproperty_add_parameter(
	property,
	icalparameter_new_rsvp(ICAL_RSVP_TRUE)
	);

    icalproperty_add_parameter(
	property,
	icalparameter_new_cutype(ICAL_CUTYPE_GROUP)
	);

    icalcomponent_add_property(event,property);


    /* more properties */

    icalcomponent_add_property(
	event,
	icalproperty_new_description("Project XYZ Review Meeting")
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_categories("MEETING")
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_class(ICAL_CLASS_PRIVATE)
	);
    
    icalcomponent_add_property(
	event,
	icalproperty_new_created(atime)
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_summary("XYZ Project Review")
	);


    property = icalproperty_new_dtstart(atime);
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_tzid("America/New_York")
	);

    icalcomponent_add_property(event,property);


    property = icalproperty_new_dtend(atime);
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_tzid("America/New_York")
	);

    icalcomponent_add_property(event,property);

    icalcomponent_add_property(
	event,
	icalproperty_new_location("1CP Conference Room 4350")
	);

    icalcomponent_add_component(calendar,event);

    printf("%s\n",icalcomponent_as_ical_string(calendar));

    icalcomponent_free(calendar);

    return 0;
}


/* Create a new component, using the va_args list */

icalcomponent* create_new_component_with_va_args()
{

    icalcomponent* calendar;
    struct icaltimetype atime = icaltime_from_timet( time(0),0);
    struct icaldatetimeperiodtype rtime;
    
    rtime.period.start = icaltime_from_timet( time(0),0);
    rtime.period.end = icaltime_from_timet( time(0),0);
    rtime.period.end.hour++;
    rtime.time = icaltime_null_time();

    calendar = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalproperty_new_version("2.0"),
	    icalproperty_new_prodid("-//RDU Software//NONSGML HandCal//EN"),
	    icalcomponent_vanew(
		ICAL_VTIMEZONE_COMPONENT,
		icalproperty_new_tzid("America/New_York"),
		icalcomponent_vanew(
		    ICAL_XDAYLIGHT_COMPONENT,
		    icalproperty_new_dtstart(atime),
		    icalproperty_new_rdate(rtime),
		    icalproperty_new_tzoffsetfrom(-4.0),
		    icalproperty_new_tzoffsetto(-5.0),
		    icalproperty_new_tzname("EST"),
		    0
		    ),
		icalcomponent_vanew(
		    ICAL_XSTANDARD_COMPONENT,
		    icalproperty_new_dtstart(atime),
		    icalproperty_new_rdate(rtime),
		    icalproperty_new_tzoffsetfrom(-5.0),
		    icalproperty_new_tzoffsetto(-4.0),
		    icalproperty_new_tzname("EST"),
		    0
		    ),
		0
		),
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,
		icalproperty_new_dtstamp(atime),
		icalproperty_new_uid("guid-1.host1.com"),
		icalproperty_vanew_organizer(
		    "mrbig@host.com",
		    icalparameter_new_role(ICAL_ROLE_CHAIR),
		    0
		    ),
		icalproperty_vanew_attendee(
		    "employee-A@host.com",
		    icalparameter_new_role(ICAL_ROLE_REQPARTICIPANT),
		    icalparameter_new_rsvp(ICAL_RSVP_TRUE),
		    icalparameter_new_cutype(ICAL_CUTYPE_GROUP),
		    0
		    ),
		icalproperty_new_description("Project XYZ Review Meeting"),
		icalproperty_new_categories("MEETING"),
		icalproperty_new_class(ICAL_CLASS_PUBLIC),
		icalproperty_new_created(atime),
		icalproperty_new_summary("XYZ Project Review"),
		icalproperty_vanew_dtstart(
		    atime,
		    icalparameter_new_tzid("America/New_York"),
		    0
		    ),
		icalproperty_vanew_dtend(
		    atime,
		    icalparameter_new_tzid("America/New_York"),
		    0
		    ),
		icalproperty_new_location("1CP Conference Room 4350"),
		0
		),
	    0
	    );
	
    printf("%s\n",icalcomponent_as_ical_string(calendar));
    

    icalcomponent_free(calendar);

    return 0;
}


/* Return a list of all attendees who are required. */
   
char** get_required_attendees(icalproperty* event)
{
    icalproperty* p;
    icalparameter* parameter;

    char **attendees;
    int max = 10;
    int c = 0;

    attendees = malloc(max * (sizeof (char *)));

    assert(event != 0);
    assert(icalcomponent_isa(event) == ICAL_VEVENT_COMPONENT);
    
    for(
	p = icalcomponent_get_first_property(event,ICAL_ATTENDEE_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(event,ICAL_ATTENDEE_PROPERTY)
	) {
	
	parameter = icalproperty_get_first_parameter(p,ICAL_ROLE_PARAMETER);

	if ( icalparameter_get_role(parameter) == ICAL_ROLE_REQPARTICIPANT) 
	{
	    attendees[c++] = icalmemory_strdup(icalproperty_get_attendee(p));

            if (c >= max) {
                max *= 2; 
                attendees = realloc(attendees, max * (sizeof (char *)));
            }

	}
    }

    return attendees;
}

/* If an attendee has a PARTSTAT of NEEDSACTION or has no PARTSTAT
   parameter, change it to TENTATIVE. */
   
void update_attendees(icalproperty* event)
{
    icalproperty* p;
    icalparameter* parameter;


    assert(event != 0);
    assert(icalcomponent_isa(event) == ICAL_VEVENT_COMPONENT);
    
    for(
	p = icalcomponent_get_first_property(event,ICAL_ATTENDEE_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(event,ICAL_ATTENDEE_PROPERTY)
	) {
	
	parameter = icalproperty_get_first_parameter(p,ICAL_PARTSTAT_PARAMETER);

	if (parameter == 0) {

	    icalproperty_add_parameter(
		p,
		icalparameter_new_partstat(ICAL_PARTSTAT_TENTATIVE)
		);

	} else if (icalparameter_get_partstat(parameter) == ICAL_PARTSTAT_NEEDSACTION) {

	    icalproperty_remove_parameter(p,ICAL_PARTSTAT_PARAMETER);
	    
	    icalparameter_free(parameter);

	    icalproperty_add_parameter(
		p,
		icalparameter_new_partstat(ICAL_PARTSTAT_TENTATIVE)
		);
	}

    }
}


void test_values()
{
    icalvalue *v; 
    icalvalue *copy; 

    v = icalvalue_new_caladdress("cap://value/1");
    printf("caladdress 1: %s\n",icalvalue_get_caladdress(v));
    icalvalue_set_caladdress(v,"cap://value/2");
    printf("caladdress 2: %s\n",icalvalue_get_caladdress(v));
    printf("String: %s\n",icalvalue_as_ical_string(v));
    
    copy = icalvalue_new_clone(v);
    printf("Clone: %s\n",icalvalue_as_ical_string(v));
    icalvalue_free(v);
    icalvalue_free(copy);


    v = icalvalue_new_boolean(1);
    printf("caladdress 1: %d\n",icalvalue_get_boolean(v));
    icalvalue_set_boolean(v,2);
    printf("caladdress 2: %d\n",icalvalue_get_boolean(v));
    printf("String: %s\n",icalvalue_as_ical_string(v));

    copy = icalvalue_new_clone(v);
    printf("Clone: %s\n",icalvalue_as_ical_string(v));
    icalvalue_free(v);
    icalvalue_free(copy);


    v = icalvalue_new_date(icaltime_from_timet( time(0),0));
    printf("date 1: %s\n",icalvalue_as_ical_string(v));
    icalvalue_set_date(v,icaltime_from_timet( time(0)+3600,0));
    printf("date 2: %s\n",icalvalue_as_ical_string(v));

    copy = icalvalue_new_clone(v);
    printf("Clone: %s\n",icalvalue_as_ical_string(v));
    icalvalue_free(v);
    icalvalue_free(copy);


    v = icalvalue_new(-1);

    printf("Invalid type: %p\n",v);

    if (v!=0) icalvalue_free(v);

    assert(ICAL_BOOLEAN_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_BOOLEAN));
    assert(ICAL_UTCOFFSET_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_UTCOFFSET));
    assert(ICAL_RECUR_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_RECUR));
    assert(ICAL_CALADDRESS_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_CALADDRESS));
    assert(ICAL_PERIOD_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_PERIOD));
    assert(ICAL_BINARY_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_BINARY));
    assert(ICAL_TEXT_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_TEXT));
    assert(ICAL_DURATION_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_DURATION));
    assert(ICAL_INTEGER_VALUE == icalparameter_value_to_value_kind(ICAL_VALUE_INTEGER));

    assert(ICAL_URI_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_URI));
    assert(ICAL_FLOAT_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_FLOAT));
    assert(ICAL_X_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_X));
    assert(ICAL_DATETIME_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_DATETIME));
    assert(ICAL_DATE_VALUE == 
           icalparameter_value_to_value_kind(ICAL_VALUE_DATE));

    /*    v = icalvalue_new_caladdress(0);

    printf("Bad string: %p\n",v);

    if (v!=0) icalvalue_free(v); */

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR, ICAL_ERROR_NONFATAL);
    v = icalvalue_new_from_string(ICAL_RECUR_VALUE,"D2 #0");
    assert(v == 0);

    v = icalvalue_new_from_string(ICAL_TRIGGER_VALUE,"Gonk");
    assert(v == 0);

    v = icalvalue_new_from_string(ICAL_REQUESTSTATUS_VALUE,"Gonk");
    assert(v == 0);

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR, ICAL_ERROR_DEFAULT);

}

void test_properties()
{
    icalproperty *prop;
    icalparameter *param;

    icalproperty *clone;

    prop = icalproperty_vanew_comment(
	"Another Comment",
	icalparameter_new_cn("A Common Name 1"),
	icalparameter_new_cn("A Common Name 2"),
	icalparameter_new_cn("A Common Name 3"),
       	icalparameter_new_cn("A Common Name 4"),
	0); 

    for(param = icalproperty_get_first_parameter(prop,ICAL_ANY_PARAMETER);
	param != 0; 
	param = icalproperty_get_next_parameter(prop,ICAL_ANY_PARAMETER)) {
						
	printf("Prop parameter: %s\n",icalparameter_get_cn(param));
    }    

    printf("Prop value: %s\n",icalproperty_get_comment(prop));


    printf("As iCAL string:\n %s\n",icalproperty_as_ical_string(prop));
    
    clone = icalproperty_new_clone(prop);

    printf("Clone:\n %s\n",icalproperty_as_ical_string(prop));
    
    icalproperty_free(clone);
    icalproperty_free(prop);

    prop = icalproperty_new(-1);

    printf("Invalid type: %p\n",prop);

    if (prop!=0) icalproperty_free(prop);

    /*
    prop = icalproperty_new_method(0);

    printf("Bad string: %p\n",prop);
   

    if (prop!=0) icalproperty_free(prop);
    */
}

void test_parameters()
{
    icalparameter *p;
    int i;
    int enums[] = {ICAL_CUTYPE_INDIVIDUAL,ICAL_CUTYPE_RESOURCE,ICAL_FBTYPE_BUSY,ICAL_PARTSTAT_NEEDSACTION,ICAL_ROLE_NONPARTICIPANT,ICAL_XLICCOMPARETYPE_LESSEQUAL,ICAL_XLICERRORTYPE_MIMEPARSEERROR,-1};

    char* str1 = "A Common Name";

    p = icalparameter_new_cn(str1);

    printf("Common Name: %s\n",icalparameter_get_cn(p));

    assert(regrstrcmp(str1,icalparameter_get_cn(p)) == 0);

    printf("As String: %s\n",icalparameter_as_ical_string(p));

    assert(regrstrcmp(icalparameter_as_ical_string(p),"CN=A Common Name")==0);

    icalparameter_free(p);


    p = icalparameter_new_from_string("PARTSTAT=ACCEPTED");
    assert(icalparameter_isa(p) == ICAL_PARTSTAT_PARAMETER);
    assert(icalparameter_get_partstat(p) == ICAL_PARTSTAT_ACCEPTED);
 
	icalparameter_free(p);

    p = icalparameter_new_from_string("ROLE=CHAIR");
    assert(icalparameter_isa(p) == ICAL_ROLE_PARAMETER);
    assert(icalparameter_get_partstat(p) == ICAL_ROLE_CHAIR);
 
	icalparameter_free(p);

    p = icalparameter_new_from_string("PARTSTAT=X-FOO");
    assert(icalparameter_isa(p) == ICAL_PARTSTAT_PARAMETER);
    assert(icalparameter_get_partstat(p) == ICAL_PARTSTAT_X);
 
	icalparameter_free(p);

    p = icalparameter_new_from_string("X-PARAM=X-FOO");
    assert(icalparameter_isa(p) == ICAL_X_PARAMETER);
 
	icalparameter_free(p);


    for (i=0;enums[i] != -1; i++){
    
        printf("%s\n",icalparameter_enum_to_string(enums[i]));
        assert(icalparameter_string_to_enum(
            icalparameter_enum_to_string(enums[i]))==enums[i]);
    }


}


void test_components()
{

    icalcomponent* c;
    icalcomponent* child;

    c = icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalproperty_new_version("2.0"),
	icalproperty_new_prodid("-//RDU Software//NONSGML HandCal//EN"),
	icalproperty_vanew_comment(
	    "A Comment",
	    icalparameter_new_cn("A Common Name 1"),
	    0),
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_new_version("2.0"),
	    icalproperty_new_description("This is an event"),
	    icalproperty_vanew_comment(
		"Another Comment",
		icalparameter_new_cn("A Common Name 1"),
		icalparameter_new_cn("A Common Name 2"),
		icalparameter_new_cn("A Common Name 3"),
		icalparameter_new_cn("A Common Name 4"),
		0),
	    icalproperty_vanew_xlicerror(
		"This is only a test",
		icalparameter_new_xlicerrortype(ICAL_XLICERRORTYPE_COMPONENTPARSEERROR),
		0),
	    
	    0
	    ),
	0
	);

    printf("Original Component:\n%s\n\n",icalcomponent_as_ical_string(c));

    child = icalcomponent_get_first_component(c,ICAL_VEVENT_COMPONENT);

    printf("Child Component:\n%s\n\n",icalcomponent_as_ical_string(child));
    
    icalcomponent_free(c);

}

void test_memory()
{
    size_t bufsize = 256;
    int i;
    char *p;

    char S1[] = "1) When in the Course of human events, ";
    char S2[] = "2) it becomes necessary for one people to dissolve the political bands which have connected them with another, ";
    char S3[] = "3) and to assume among the powers of the earth, ";
    char S4[] = "4) the separate and equal station to which the Laws of Nature and of Nature's God entitle them, ";
    char S5[] = "5) a decent respect to the opinions of mankind requires that they ";
    char S6[] = "6) should declare the causes which impel them to the separation. ";
    char S7[] = "7) We hold these truths to be self-evident, ";
    char S8[] = "8) that all men are created equal, ";

/*    char S9[] = "9) that they are endowed by their Creator with certain unalienable Rights, ";
    char S10[] = "10) that among these are Life, Liberty, and the pursuit of Happiness. ";
    char S11[] = "11) That to secure these rights, Governments are instituted among Men, ";
    char S12[] = "12) deriving their just powers from the consent of the governed. "; 
*/


    char *f, *b1, *b2, *b3, *b4, *b5, *b6, *b7, *b8;

#define BUFSIZE 1024

    f = icalmemory_new_buffer(bufsize);
    p = f;
    b1 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b1, S1);
    icalmemory_append_string(&f, &p, &bufsize, b1);

    b2 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b2, S2);
    icalmemory_append_string(&f, &p, &bufsize, b2);

    b3 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b3, S3);
    icalmemory_append_string(&f, &p, &bufsize, b3);

    b4 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b4, S4);
    icalmemory_append_string(&f, &p, &bufsize, b4);

    b5 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b5, S5);
    icalmemory_append_string(&f, &p, &bufsize, b5);

    b6 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b6, S6);
    icalmemory_append_string(&f, &p, &bufsize, b6);

    b7 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b7, S7);
    icalmemory_append_string(&f, &p, &bufsize, b7);

    b8 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b8, S8);
    icalmemory_append_string(&f, &p, &bufsize, b8);


    printf("1: %p %s \n",b1,b1);
    printf("2: %p %s\n",b2,b2);
    printf("3: %p %s\n",b3,b3);
    printf("4: %p %s\n",b4,b4);
    printf("5: %p %s\n",b5,b5);
    printf("6: %p %s\n",b6,b6);
    printf("7: %p %s\n",b7,b7);
    printf("8: %p %s\n",b8,b8);

    
    printf("Final: %s\n", f);

    printf("Final buffer size: %d\n",bufsize);

    free(f);
    
    bufsize = 4;

    f = icalmemory_new_buffer(bufsize);

    memset(f,0,bufsize);
    p = f;

    icalmemory_append_char(&f, &p, &bufsize, 'a');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'b');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'c');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'd');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'e');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'f');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'g');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'h');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'i');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'j');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'a');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'b');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'c');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'd');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'e');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'f');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'g');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'h');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'i');
    printf("Char-by-Char buffer: %s\n", f);
    icalmemory_append_char(&f, &p, &bufsize, 'j');
    printf("Char-by-Char buffer: %s\n", f);
 
	free(f);
  
    for(i=0; i<100; i++){
	f = icalmemory_tmp_buffer(bufsize);
	
	assert(f!=0);

	memset(f,0,bufsize);
	sprintf(f,"%d",i);
    }

}


int test_store()
{

    icalcomponent *c, *gauge;
    icalerrorenum error;
    icalcomponent *next, *itr;
    icalfileset* cluster;
    struct icalperiodtype rtime;
    icaldirset *s = icaldirset_new("store");
    int i;

    rtime.start = icaltime_from_timet( time(0),0);

    cluster = icalfileset_new("clusterin.vcd");

    if (cluster == 0){
	printf("Failed to create cluster: %s\n",icalerror_strerror(icalerrno));
	return 0;
    }

#define NUMCOMP 4

    /* Duplicate every component in the cluster NUMCOMP times */

    icalerror_clear_errno();

    for (i = 1; i<NUMCOMP+1; i++){

	/*rtime.start.month = i%12;*/
	rtime.start.month = i;
	rtime.end = rtime.start;
	rtime.end.hour++;
	
	for (itr = icalfileset_get_first_component(cluster);
	     itr != 0;
	     itr = icalfileset_get_next_component(cluster)){
	    icalcomponent *clone;
	    icalproperty *p;

	    
	    if(icalcomponent_isa(itr) != ICAL_VEVENT_COMPONENT){
		continue;
	    }

	    assert(itr != 0);
	    
	    /* Change the dtstart and dtend times in the component
               pointed to by Itr*/

	    clone = icalcomponent_new_clone(itr);
	    assert(icalerrno == ICAL_NO_ERROR);
	    assert(clone !=0);

	    /* DTSTART*/
	    p = icalcomponent_get_first_property(clone,ICAL_DTSTART_PROPERTY);
	    assert(icalerrno  == ICAL_NO_ERROR);

	    if (p == 0){
		p = icalproperty_new_dtstart(rtime.start);
		icalcomponent_add_property(clone,p);
	    } else {
		icalproperty_set_dtstart(p,rtime.start);
	    }
	    assert(icalerrno  == ICAL_NO_ERROR);

	    /* DTEND*/
	    p = icalcomponent_get_first_property(clone,ICAL_DTEND_PROPERTY);
	    assert(icalerrno  == ICAL_NO_ERROR);

	    if (p == 0){
		p = icalproperty_new_dtstart(rtime.end);
		icalcomponent_add_property(clone,p);
	    } else {
		icalproperty_set_dtstart(p,rtime.end);
	    }
	    assert(icalerrno  == ICAL_NO_ERROR);
	    
	    printf("\n----------\n%s\n---------\n",icalcomponent_as_ical_string(clone));

	    error = icaldirset_add_component(s,clone);
	    
	    assert(icalerrno  == ICAL_NO_ERROR);

	}

    }
    
    gauge = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,  
		icalproperty_vanew_summary(
		    "Submit Income Taxes",
		    icalparameter_new_xliccomparetype(ICAL_XLICCOMPARETYPE_EQUAL),
		    0),
		0),
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,  
		icalproperty_vanew_summary(
		    "Bastille Day Party",
		    icalparameter_new_xliccomparetype(ICAL_XLICCOMPARETYPE_EQUAL),
		    0),
		0),
	    0);

#if 0


    icaldirset_select(s,gauge);

    for(c = icaldirset_first(s); c != 0; c = icaldirset_next(s)){
	
	printf("Got one! (%d)\n", count++);
	
	if (c != 0){
	    printf("%s", icalcomponent_as_ical_string(c));;
	    if (icaldirset_store(s2,c) == 0){
		printf("Failed to write!\n");
	    }
	    icalcomponent_free(c);
	} else {
	    printf("Failed to get component\n");
	}
    }


    icaldirset_free(s2);
#endif


    for(c = icaldirset_get_first_component(s); 
	c != 0; 
	c =  next){
	
	next = icaldirset_get_next_component(s);

	if (c != 0){
	    /*icaldirset_remove_component(s,c);*/
	    printf("%s", icalcomponent_as_ical_string(c));;
	} else {
	    printf("Failed to get component\n");
	}


    }

    icaldirset_free(s);
    return 0;
}

int test_compare()
{
    icalvalue *v1, *v2;

    v1 = icalvalue_new_caladdress("cap://value/1");
    v2 = icalvalue_new_clone(v1);

    printf("%d\n",icalvalue_compare(v1,v2));

	icalvalue_free(v1);
	icalvalue_free(v2);

    v1 = icalvalue_new_caladdress("A");
    v2 = icalvalue_new_caladdress("B");

    printf("%d\n",icalvalue_compare(v1,v2));

	icalvalue_free(v1);
	icalvalue_free(v2);

    v1 = icalvalue_new_caladdress("B");
    v2 = icalvalue_new_caladdress("A");

    printf("%d\n",icalvalue_compare(v1,v2));

	icalvalue_free(v1);
	icalvalue_free(v2);

    v1 = icalvalue_new_integer(5);
    v2 = icalvalue_new_integer(5);

    printf("%d\n",icalvalue_compare(v1,v2));

	icalvalue_free(v1);
	icalvalue_free(v2);

    v1 = icalvalue_new_integer(5);
    v2 = icalvalue_new_integer(10);

    printf("%d\n",icalvalue_compare(v1,v2));

	icalvalue_free(v1);
	icalvalue_free(v2);

    v1 = icalvalue_new_integer(10);
    v2 = icalvalue_new_integer(5);

    printf("%d\n",icalvalue_compare(v1,v2));

	icalvalue_free(v1);
	icalvalue_free(v2);

    return 0;
}

void test_restriction()
{
    icalcomponent *comp;
    struct icaltimetype atime = icaltime_from_timet( time(0),0);
    int valid; 

    struct icaldatetimeperiodtype rtime;

    rtime.period.start = icaltime_from_timet( time(0),0);
    rtime.period.end = icaltime_from_timet( time(0),0);
    rtime.period.end.hour++;
    rtime.time = icaltime_null_time();

    comp = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalproperty_new_version("2.0"),
	    icalproperty_new_prodid("-//RDU Software//NONSGML HandCal//EN"),
	    icalproperty_new_method(ICAL_METHOD_REQUEST),
	    icalcomponent_vanew(
		ICAL_VTIMEZONE_COMPONENT,
		icalproperty_new_tzid("America/New_York"),
		icalcomponent_vanew(
		    ICAL_XDAYLIGHT_COMPONENT,
		    icalproperty_new_dtstart(atime),
		    icalproperty_new_rdate(rtime),
		    icalproperty_new_tzoffsetfrom(-4.0),
		    icalproperty_new_tzoffsetto(-5.0),
		    icalproperty_new_tzname("EST"),
		    0
		    ),
		icalcomponent_vanew(
		    ICAL_XSTANDARD_COMPONENT,
		    icalproperty_new_dtstart(atime),
		    icalproperty_new_rdate(rtime),
		    icalproperty_new_tzoffsetfrom(-5.0),
		    icalproperty_new_tzoffsetto(-4.0),
		    icalproperty_new_tzname("EST"),
		    0
		    ),
		0
		),
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,
		icalproperty_new_dtstamp(atime),
		icalproperty_new_uid("guid-1.host1.com"),
		icalproperty_vanew_organizer(
		    "mrbig@host.com",
		    icalparameter_new_role(ICAL_ROLE_CHAIR),
		    0
		    ),
		icalproperty_vanew_attendee(
		    "employee-A@host.com",
		    icalparameter_new_role(ICAL_ROLE_REQPARTICIPANT),
		    icalparameter_new_rsvp(ICAL_RSVP_TRUE),
		    icalparameter_new_cutype(ICAL_CUTYPE_GROUP),
		    0
		    ),
		icalproperty_new_description("Project XYZ Review Meeting"),
		icalproperty_new_categories("MEETING"),
		icalproperty_new_class(ICAL_CLASS_PUBLIC),
		icalproperty_new_created(atime),
		icalproperty_new_summary("XYZ Project Review"),
                /*		icalproperty_new_dtstart(
		    atime,
		    icalparameter_new_tzid("America/New_York"),
		    0
		    ),*/
		icalproperty_vanew_dtend(
		    atime,
		    icalparameter_new_tzid("America/New_York"),
		    0
		    ),
		icalproperty_new_location("1CP Conference Room 4350"),
		0
		),
	    0
	    );

    valid = icalrestriction_check(comp);

    printf("#### %d ####\n%s\n",valid, icalcomponent_as_ical_string(comp));

	icalcomponent_free(comp);

}

#if 0
void test_calendar()
{
    icalcomponent *comp;
    icalfileset *c;
    icaldirset *s;
    icalcalendar* calendar = icalcalendar_new("calendar");
    icalerrorenum error;
    struct icaltimetype atime = icaltime_from_timet( time(0),0);

    comp = icalcomponent_vanew(
	ICAL_VEVENT_COMPONENT,
	icalproperty_new_version("2.0"),
	icalproperty_new_description("This is an event"),
	icalproperty_new_dtstart(atime),
	icalproperty_vanew_comment(
	    "Another Comment",
	    icalparameter_new_cn("A Common Name 1"),
	    icalparameter_new_cn("A Common Name 2"),
	    icalparameter_new_cn("A Common Name 3"),
		icalparameter_new_cn("A Common Name 4"),
	    0),
	icalproperty_vanew_xlicerror(
	    "This is only a test",
	    icalparameter_new_xlicerrortype(ICAL_XLICERRORTYPE_COMPONENTPARSEERROR),
	    0),
	
	0);
	
	
    s = icalcalendar_get_booked(calendar);

    error = icaldirset_add_component(s,comp);
    
    assert(error == ICAL_NO_ERROR);

    c = icalcalendar_get_properties(calendar);

    error = icalfileset_add_component(c,icalcomponent_new_clone(comp));

    assert(error == ICAL_NO_ERROR);

    icalcalendar_free(calendar);

}
#endif

void test_increment(void);

void print_occur(struct icalrecurrencetype recur, struct icaltimetype start)
{
    struct icaltimetype next;
    icalrecur_iterator* ritr;
	
    time_t tt = icaltime_as_timet(start);

    printf("#### %s\n",icalrecurrencetype_as_string(&recur));
    printf("#### %s\n",ctime(&tt ));
    
    for(ritr = icalrecur_iterator_new(recur,start),
	    next = icalrecur_iterator_next(ritr); 
	!icaltime_is_null_time(next);
	next = icalrecur_iterator_next(ritr)){
	
	tt = icaltime_as_timet(next);
	
	printf("  %s",ctime(&tt ));		
	
    }

    icalrecur_iterator_free(ritr);
}

void test_recur()
{
   struct icalrecurrencetype rt;
   struct icaltimetype start;
   time_t array[25];
   int i;

   rt = icalrecurrencetype_from_string("FREQ=MONTHLY;UNTIL=19971224T000000Z;INTERVAL=1;BYDAY=TU,2FR,3SA");
   start = icaltime_from_string("19970905T090000Z");

   print_occur(rt,start);   

   printf("\n  Using icalrecur_expand_recurrence\n");

   icalrecur_expand_recurrence("FREQ=MONTHLY;UNTIL=19971224T000000Z;INTERVAL=1;BYDAY=TU,2FR,3SA",
			       icaltime_as_timet(start),
			       25,
			       array);

   for(i =0; array[i] != 0 && i < 25 ; i++){
       
       printf("  %s",ctime(&(array[i])));
   }

   
/*    test_increment();*/

}

void test_expand_recurrence(){

    time_t arr[10];
    time_t now = 931057385;
    int i;
    icalrecur_expand_recurrence( "FREQ=MONTHLY;BYDAY=MO,WE", now,
                                 5, arr );

    printf("Start %s",ctime(&now) );
    for (i=0; i<5; i++)
        {
            printf("i=%d  %s\n", i, ctime(&arr[i]) );
        }
            
}



enum byrule {
    NO_CONTRACTION = -1,
    BY_SECOND = 0,
    BY_MINUTE = 1,
    BY_HOUR = 2,
    BY_DAY = 3,
    BY_MONTH_DAY = 4,
    BY_YEAR_DAY = 5,
    BY_WEEK_NO = 6,
    BY_MONTH = 7,
    BY_SET_POS
};

struct icalrecur_iterator_impl {
	
	struct icaltimetype dtstart; 
	struct icaltimetype last; /* last time return from _iterator_next*/
	int occurrence_no; /* number of step made on this iterator */
	struct icalrecurrencetype rule;

	short days[366];
	short days_index;

	enum byrule byrule;
	short by_indices[9];


	short *by_ptrs[9]; /* Pointers into the by_* array elements of the rule */
};

void icalrecurrencetype_test()
{
    icalvalue *v = icalvalue_new_from_string(
	ICAL_RECUR_VALUE,
	"FREQ=YEARLY;UNTIL=20060101T000000;INTERVAL=2;BYDAY=SU,WE;BYSECOND=15,30; BYMONTH=1,6,11");

    struct icalrecurrencetype r = icalvalue_get_recur(v);
    struct icaltimetype t = icaltime_from_timet( time(0), 0);
    struct icaltimetype next;
    time_t tt;

    struct icalrecur_iterator_impl* itr 
	= (struct icalrecur_iterator_impl*) icalrecur_iterator_new(r,t);

    do {

	next = icalrecur_iterator_next(itr);
	tt = icaltime_as_timet(next);

	printf("%s",ctime(&tt ));		

    } while( ! icaltime_is_null_time(next));

	icalvalue_free(v);
 
}

/* From Federico Mena Quintero <federico@helixcode.com>    */
void test_recur_parameter_bug(){

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
#include <stdio.h>
#include <stdlib.h>
#include <ical.h>
 
    static const char test_icalcomp_str[] =
"BEGIN:VEVENT\r\n"
"RRULE\r\n"
" ;X-EVOLUTION-ENDDATE=20030209T081500\r\n"
" :FREQ=DAILY;COUNT=10;INTERVAL=6\r\n"
"END:VEVENT\r\n";
  
    icalcomponent *icalcomp;
    icalproperty *prop;
    struct icalrecurrencetype recur;
    int n_errors;
    
    icalcomp = icalparser_parse_string ((char *) test_icalcomp_str);
    if (!icalcomp) {
	fprintf (stderr, "main(): could not parse the component\n");
	exit (EXIT_FAILURE);
    }
    
    printf("%s\n\n",icalcomponent_as_ical_string(icalcomp));

    n_errors = icalcomponent_count_errors (icalcomp);
    if (n_errors) {
	icalproperty *p;
	
	for (p = icalcomponent_get_first_property (icalcomp,
						   ICAL_XLICERROR_PROPERTY);
	     p;
	     p = icalcomponent_get_next_property (icalcomp,
						  ICAL_XLICERROR_PROPERTY)) {
	    const char *str;
	    
	    str = icalproperty_as_ical_string (p);
	    fprintf (stderr, "error: %s\n", str);
	}
    }
    
    prop = icalcomponent_get_first_property (icalcomp, ICAL_RRULE_PROPERTY);
    if (!prop) {
	fprintf (stderr, "main(): could not get the RRULE property");
	exit (EXIT_FAILURE);
    }
    
    recur = icalproperty_get_rrule (prop);
    
    printf("%s\n",icalrecurrencetype_as_string(&recur));

	icalcomponent_free(icalcomp);
}


void test_duration()
{


    struct icaldurationtype d;

    d = icaldurationtype_from_string("PT8H30M");
    printf("%s\n",icaldurationtype_as_ical_string(d));
    assert(icaldurationtype_as_int(d) == 30600);
    
    d = icaldurationtype_from_string("-PT8H30M");
    printf("%s\n",icaldurationtype_as_ical_string(d));
    assert(icaldurationtype_as_int(d) == -30600);

    d = icaldurationtype_from_string("PT10H10M10S");
    printf("%s\n",icaldurationtype_as_ical_string(d));
    assert(icaldurationtype_as_int(d) == 36610);

    d = icaldurationtype_from_string("P7W");
    printf("%s\n",icaldurationtype_as_ical_string(d));
    assert(icaldurationtype_as_int(d) == 4233600);

    d = icaldurationtype_from_string("P2DT8H30M");
    printf("%s\n",icaldurationtype_as_ical_string(d));
    assert(icaldurationtype_as_int(d) == 203400);

    icalerror_errors_are_fatal = 0;

    d = icaldurationtype_from_string("P-2DT8H30M");
    printf("%s\n",icaldurationtype_as_ical_string(d));
    assert(icaldurationtype_as_int(d) == 0);

    d = icaldurationtype_from_string("P7W8H");
    printf("%s\n",icaldurationtype_as_ical_string(d));
    assert(icaldurationtype_as_int(d) == 0);

    d = icaldurationtype_from_string("T10H");
    printf("%s\n",icaldurationtype_as_ical_string(d));
    assert(icaldurationtype_as_int(d) == 0);


    icalerror_errors_are_fatal = 1;

}

void test_period()
{

    struct icalperiodtype p;
    icalvalue *v;

    p = icalperiodtype_from_string("19971015T050000Z/PT8H30M");
    printf("%s\n",icalperiodtype_as_ical_string(p));
    assert(strcmp(icalperiodtype_as_ical_string(p),
                  "19971015T050000Z/PT8H30M") == 0);

    p = icalperiodtype_from_string("19971015T050000Z/19971015T060000Z");
    printf("%s\n",icalperiodtype_as_ical_string(p));
    assert(strcmp(icalperiodtype_as_ical_string(p),
                  "19971015T050000Z/19971015T060000Z") == 0);

    p = icalperiodtype_from_string("19970101T120000/PT3H");
    printf("%s\n",icalperiodtype_as_ical_string(p));
    assert(strcmp(icalperiodtype_as_ical_string(p),
                  "19970101T120000/PT3H") == 0);

    v = icalvalue_new_from_string(ICAL_PERIOD_VALUE,"19970101T120000/PT3H");
    printf("%s\n",icalvalue_as_ical_string(v));
    assert(strcmp(icalvalue_as_ical_string(v),
                  "19970101T120000/PT3H") == 0);


	icalvalue_free(v);


}

void test_strings(){

    icalvalue *v;

    v = icalvalue_new_text("foo;bar;bats");
    
    printf("%s\n",icalvalue_as_ical_string(v));

    icalvalue_free(v);

    v = icalvalue_new_text("foo\\;b\nar\\;ba\tts");
    
    printf("%s\n",icalvalue_as_ical_string(v));
    
    icalvalue_free(v);


}

void test_requeststat()
{
    icalproperty *p;
    icalrequeststatus s;
    struct icalreqstattype st, st2;
    char temp[1024];
    
    s = icalenum_num_to_reqstat(2,1);
    
    assert(s == ICAL_2_1_FALLBACK_STATUS);
    
    assert(icalenum_reqstat_major(s) == 2);
    assert(icalenum_reqstat_minor(s) == 1);
    
    printf("2.1: %s\n",icalenum_reqstat_desc(s));
    
    st.code = s;
    st.debug = "booga";
    st.desc = 0;
    
    printf("%s\n",icalreqstattype_as_string(st));
    
    st.desc = " A non-standard description";
    
    printf("%s\n",icalreqstattype_as_string(st));
    
    
    st.desc = 0;
    
    sprintf(temp,"%s\n",icalreqstattype_as_string(st));
    
    
    st2 = icalreqstattype_from_string("2.1;Success but fallback taken  on one or more property  values.;booga");
    
    /*    printf("%d --  %d --  %s -- %s\n",*/
    assert(icalenum_reqstat_major(st2.code) == 2);
    assert(icalenum_reqstat_minor(st2.code) == 1);
    assert(regrstrcmp( icalenum_reqstat_desc(st2.code),"Success but fallback taken  on one or more property  values.")==0);
    
    st2 = icalreqstattype_from_string("2.1;Success but fallback taken  on one or more property  values.;booga");
    printf("%s\n",icalreqstattype_as_string(st2));
    
    st2 = icalreqstattype_from_string("2.1;Success but fallback taken  on one or more property  values.;");
    printf("%s\n",icalreqstattype_as_string(st2));
    
    st2 = icalreqstattype_from_string("2.1;Success but fallback taken  on one or more property  values.");
    printf("%s\n",icalreqstattype_as_string(st2));
    
    st2 = icalreqstattype_from_string("2.1;");
    printf("%s\n",icalreqstattype_as_string(st2));
    assert(regrstrcmp(icalreqstattype_as_string(st2),"2.1;Success but fallback taken  on one or more property  values.")==0);
    
    st2 = icalreqstattype_from_string("2.1");
    printf("%s\n",icalreqstattype_as_string(st2));
    assert(regrstrcmp(icalreqstattype_as_string(st2),"2.1;Success but fallback taken  on one or more property  values.")==0);
    
    p = icalproperty_new_from_string("REQUEST-STATUS:2.1;Success but fallback taken  on one or more property  values.;booga");

    printf("%s\n",icalproperty_as_ical_string(p));
    assert(regrstrcmp(icalproperty_as_ical_string(p),"REQUEST-STATUS\n :2.1;Success but fallback taken  on one or more property  values.;booga\n")==0);

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_NONFATAL);
    st2 = icalreqstattype_from_string("16.4");
    assert(st2.code == ICAL_UNKNOWN_STATUS);
    
    st2 = icalreqstattype_from_string("1.");
    assert(st2.code == ICAL_UNKNOWN_STATUS);
    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_DEFAULT);

	icalproperty_free(p);
    
}

char ictt_str[1024];
char* ictt_as_string(struct icaltimetype t)
{

    sprintf(ictt_str,"%02d-%02d-%02d %02d:%02d:%02d%s",t.year,t.month,t.day,
	    t.hour,t.minute,t.second,t.is_utc?" Z":"");

    return ictt_str;
}


char* ical_timet_string(time_t t)
{
    struct tm stm = *(gmtime(&t));    

    sprintf(ictt_str,"%02d-%02d-%02d %02d:%02d:%02d Z",stm.tm_year+1900,
	    stm.tm_mon+1,stm.tm_mday,stm.tm_hour,stm.tm_min,stm.tm_sec);

    return ictt_str;
    
}

void test_dtstart(){
    
    struct icaltimetype tt,tt2;
    
    icalproperty *p;
    

    tt = icaltime_from_string("19970101");

    assert(tt.is_date == 1);

    p = icalproperty_new_dtstart(tt);

    printf("%s\n",icalvalue_kind_to_string(icalvalue_isa(icalproperty_get_value(p))));
    assert(icalvalue_isa(icalproperty_get_value(p))==ICAL_DATE_VALUE);

    tt2 = icalproperty_get_dtstart(p);
    assert(tt2.is_date == 1);

    printf("%s\n",icalproperty_as_ical_string(p));


    tt = icaltime_from_string("19970101T103000");

    assert(tt.is_date == 0);

	icalproperty_free(p);

    p = icalproperty_new_dtstart(tt);

    printf("%s\n",icalvalue_kind_to_string(icalvalue_isa(icalproperty_get_value(p))));
    assert(icalvalue_isa(icalproperty_get_value(p))==ICAL_DATETIME_VALUE);

    tt2 = icalproperty_get_dtstart(p);
    assert(tt2.is_date == 0);

    printf("%s\n",icalproperty_as_ical_string(p));

	icalproperty_free(p);



}

void do_test_time(char* zone)
{
    struct icaltimetype ictt, icttutc, icttutczone, icttdayl, 
	icttla, icttny,icttphoenix, icttlocal, icttnorm;
    time_t tt,tt2, tt_p200;
    int offset_la, offset_tz;
    icalvalue *v;
    short day_of_week,start_day_of_week, day_of_year;
	icaltimezone* lazone;

    icalerror_errors_are_fatal = 0;

    ictt = icaltime_from_string("20001103T183030Z");

    tt = icaltime_as_timet(ictt);

    assert(tt==973276230); /* Fri Nov  3 10:30:30 PST 2000 in PST 
                               Fri Nov  3 18:30:30 PST 2000 in UTC */
    
    offset_la = icaltime_utc_offset(ictt,"America/Los_Angeles");
    offset_tz = icaltime_utc_offset(ictt, zone);

    printf(" Normalize \n");
    printf("Orig (ical) : %s\n", ictt_as_string(ictt));
    icttnorm = ictt;
    icttnorm.second -= 60 * 60 * 24 * 5;
    icttnorm = icaltime_normalize(icttnorm);
    printf("-5d in sec  : %s\n", ictt_as_string(icttnorm));
    icttnorm.day += 60;
    icttnorm = icaltime_normalize(icttnorm);
    printf("+60 d       : %s\n", ictt_as_string(icttnorm));


    printf("\n As time_t \n");

    tt2 = icaltime_as_timet(ictt);
    printf("20001103T183030Z (timet): %s\n",ical_timet_string(tt2));
    printf("20001103T183030Z        : %s\n",ictt_as_string(ictt));
    assert(tt2 == tt);

    icttlocal = icaltime_from_string("20001103T183030");
    tt2 = icaltime_as_timet(icttlocal);
    printf("20001103T183030  (timet): %s\n",ical_timet_string(tt2));
    printf("20001103T183030         : %s\n",ictt_as_string(icttlocal));
    assert(tt-tt2 == offset_tz);

    printf("\n From time_t \n");

    printf("Orig        : %s\n",ical_timet_string(tt));
    printf("As utc      : %s\n", ictt_as_string(ictt));

    icttlocal = icaltime_as_zone(ictt,zone);
    printf("As local    : %s\n", ictt_as_string(icttlocal));
    

    printf("\n Convert to and from lib c \n");

    printf("System time is: %s\n",ical_timet_string(tt));

    v = icalvalue_new_datetime(ictt);

    printf("System time from libical: %s\n",icalvalue_as_ical_string(v));

	icalvalue_free(v);

    tt2 = icaltime_as_timet(ictt);
    printf("Converted back to libc: %s\n",ical_timet_string(tt2));
    
    printf("\n Incrementing time  \n");

    icttnorm = ictt;

    icttnorm.year++;
    tt2 = icaltime_as_timet(icttnorm);
    printf("Add a year: %s\n",ical_timet_string(tt2));

    icttnorm.month+=13;
    tt2 = icaltime_as_timet(icttnorm);
    printf("Add 13 months: %s\n",ical_timet_string(tt2));

    icttnorm.second+=90;
    tt2 = icaltime_as_timet(icttnorm);
    printf("Add 90 seconds: %s\n",ical_timet_string(tt2));

    printf("\n Day Of week \n");

    day_of_week = icaltime_day_of_week(ictt);
    start_day_of_week = icaltime_start_doy_of_week(ictt);
    day_of_year = icaltime_day_of_year(ictt);


    printf("Today is day of week %d, day of year %d\n",day_of_week,day_of_year);
    printf("Week started n doy of %d\n",start_day_of_week);
    assert(day_of_week == 6);
    assert(day_of_year == 308);
    assert(start_day_of_week == 303 );

    printf("\n TimeZone Conversions \n");

	lazone = icaltimezone_get_builtin_timezone("America/Los_Angeles");
	icttla = ictt;
	icaltimezone_convert_time(&icttla,
						 icaltimezone_get_utc_timezone(),
						 lazone);


    icttla = icaltime_as_zone(ictt,"America/Los_Angeles");
    assert(icttla.hour == 10);

    icttutc = icaltime_as_utc(icttla,"America/Los_Angeles");
    assert(icaltime_compare(icttla,
			 icaltime_from_string("20001103T103030"))==0);

    icttutczone = icaltime_as_zone(ictt,"Etc/GMT0");
    icttutczone.is_utc = 1;
    assert(icaltime_compare(icttutc, icttutczone) == 0); 
    assert(icaltime_compare(icttutc, ictt) == 0); 

    icttny  = icaltime_as_zone(ictt,"America/New_York");

    icttphoenix = icaltime_as_zone(ictt,"America/Phoenix");

    printf("Orig (ctime): %s\n", ical_timet_string(tt) );
    printf("Orig (ical) : %s\n", ictt_as_string(ictt));
    printf("UTC         : %s\n", ictt_as_string(icttutc));
    printf("Los Angeles : %s\n", ictt_as_string(icttla));
    printf("Phoenix     : %s\n", ictt_as_string(icttphoenix));
    printf("New York    : %s\n", ictt_as_string(icttny));


    /* Daylight savings test for New York */
    printf("\n Daylight Savings \n");

    printf("Orig (ctime): %s\n", ical_timet_string(tt) );
    printf("Orig (ical) : %s\n", ictt_as_string(ictt));
    printf("NY          : %s\n", ictt_as_string(icttny));

    assert(strcmp(ictt_as_string(icttny),"2000-11-03 13:30:30")==0);

    tt_p200 = tt +  200 * 24 * 60 * 60 ; /* Add 200 days */

    icttdayl = icaltime_from_timet(tt_p200,0);    
    icttny = icaltime_as_zone(icttdayl,"America/New_York");
    
    printf("Orig +200d  : %s\n", ical_timet_string(tt_p200) );
    printf("NY+200D     : %s\n", ictt_as_string(icttny));

    assert(strcmp(ictt_as_string(icttny),"2001-05-22 14:30:30")==0);

    /* Daylight savings test for Los Angeles */

    icttla  = icaltime_as_zone(ictt,"America/Los_Angeles");

    printf("\nOrig (ctime): %s\n", ical_timet_string(tt) );
    printf("Orig (ical) : %s\n", ictt_as_string(ictt));
    printf("LA          : %s\n", ictt_as_string(icttla));

    assert(strcmp(ictt_as_string(icttla),"2000-11-03 10:30:30")==0);
    
    icttla = icaltime_as_zone(icttdayl,"America/Los_Angeles");
    
    printf("Orig +200d  : %s\n", ical_timet_string(tt_p200) );
    printf("LA+200D     : %s\n", ictt_as_string(icttla));

    assert(strcmp(ictt_as_string(icttla),"2001-05-22 11:30:30")==0);

    icalerror_errors_are_fatal = 1;
}

void test_iterators()
{
    icalcomponent *c,*inner,*next;
    icalcompiter i;

    c= icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalcomponent_vanew(ICAL_VEVENT_COMPONENT,
			    icalproperty_new_version("1"),0),
	icalcomponent_vanew(ICAL_VEVENT_COMPONENT,
			    icalproperty_new_version("2"),0),
	icalcomponent_vanew(ICAL_VEVENT_COMPONENT,
			    icalproperty_new_version("3"),0),
	icalcomponent_vanew(ICAL_VEVENT_COMPONENT,
			    icalproperty_new_version("4"),0),
	icalcomponent_vanew(ICAL_VTODO_COMPONENT,
			    icalproperty_new_version("5"),0),
	icalcomponent_vanew(ICAL_VJOURNAL_COMPONENT,
			    icalproperty_new_version("6"),0),
	icalcomponent_vanew(ICAL_VEVENT_COMPONENT,
			    icalproperty_new_version("7"),0),
	icalcomponent_vanew(ICAL_VJOURNAL_COMPONENT,
			    icalproperty_new_version("8"),0),
	icalcomponent_vanew(ICAL_VJOURNAL_COMPONENT,
			    icalproperty_new_version("9"),0),
	icalcomponent_vanew(ICAL_VJOURNAL_COMPONENT,
			    icalproperty_new_version("10"),0),
	0);
   
    printf("1: ");

    /* List all of the VEVENTS */
    for(i = icalcomponent_begin_component(c,ICAL_VEVENT_COMPONENT);
	icalcompiter_deref(&i)!= 0; icalcompiter_next(&i)){
	
	icalcomponent *this = icalcompiter_deref(&i);

	icalproperty *p = 
	    icalcomponent_get_first_property(this,
					     ICAL_VERSION_PROPERTY);
	const char* s = icalproperty_get_version(p);

	printf("%s ",s);
	    
    }

    printf("\n2: ");

#if 0
    for(inner = icalcomponent_get_first_component(c,ICAL_VEVENT_COMPONENT); 
	inner != 0; 
	inner = next){
	
	next = icalcomponent_get_next_component(c,ICAL_VEVENT_COMPONENT);

	icalcomponent_remove_component(c,inner);

	icalcomponent_free(inner);
    }
#endif

    /* Delete all of the VEVENTS */
    /* reset iterator */
    icalcomponent_get_first_component(c,ICAL_VEVENT_COMPONENT);

    while((inner=icalcomponent_get_current_component(c)) != 0 ){
	if(icalcomponent_isa(inner) == ICAL_VEVENT_COMPONENT){
	    icalcomponent_remove_component(c,inner);
		icalcomponent_free(inner);
	} else {
	    icalcomponent_get_next_component(c,ICAL_VEVENT_COMPONENT);
	}
    }
	


    /* List all remaining components */
    for(inner = icalcomponent_get_first_component(c,ICAL_ANY_COMPONENT); 
	inner != 0; 
	inner = icalcomponent_get_next_component(c,ICAL_ANY_COMPONENT)){
	

	icalproperty *p = 
	    icalcomponent_get_first_property(inner,ICAL_VERSION_PROPERTY);

	const char* s = icalproperty_get_version(p);	

	printf("%s ",s);
    }

    printf("\n3: ");

    
    /* Remove all remaining components */
    for(inner = icalcomponent_get_first_component(c,ICAL_ANY_COMPONENT); 
	inner != 0; 
	inner = next){

	icalcomponent *this;
	icalproperty *p;
	const char* s;
	next = icalcomponent_get_next_component(c,ICAL_ANY_COMPONENT);

	p=icalcomponent_get_first_property(inner,ICAL_VERSION_PROPERTY);
	s = icalproperty_get_version(p);	
	printf("rem:%s ",s);

	icalcomponent_remove_component(c,inner);

	this = icalcomponent_get_current_component(c);

	if(this != 0){
	    p=icalcomponent_get_first_property(this,ICAL_VERSION_PROPERTY);
	    s = icalproperty_get_version(p);	
	    printf("next:%s; ",s);
	}

	icalcomponent_free(inner);
    }

    printf("\n4: ");


    /* List all remaining components */
    for(inner = icalcomponent_get_first_component(c,ICAL_ANY_COMPONENT); 
	inner != 0; 
	inner = icalcomponent_get_next_component(c,ICAL_ANY_COMPONENT)){
	
	icalproperty *p = 
	    icalcomponent_get_first_property(inner,ICAL_VERSION_PROPERTY);

	const char* s = icalproperty_get_version(p);	

	printf("%s ",s);
    }

    printf("\n");

	icalcomponent_free(c);
}

struct set_tz_save {char* orig_tzid; char* new_env_str;};
struct set_tz_save set_tz(const char* tzid);
void unset_tz(struct set_tz_save savetz);


void test_time()
{
    char  zones[6][40] = { "America/Los_Angeles","America/New_York","Europe/London","Asia/Shanghai", ""};
    int i;
    struct set_tz_save old_tz;
    int orig_month;
    time_t tt;
    struct tm stm;

    tt = time(0);

    stm = *(localtime(&tt));

    orig_month = stm.tm_mon;

    do_test_time(0);

    for(i = 0; zones[i][0] != 0; i++){

	printf(" ######### Timezone: %s ############\n",zones[i]);
        
        old_tz = set_tz(zones[i]);
	
	do_test_time(zones[i]);

        unset_tz(old_tz);

    } 

}


void test_icalset()
{
    icalcomponent *c; 

    icalset* f = icalset_new_file("2446.ics");
    icalset* d = icalset_new_dir("outdir");

    assert(f!=0);
    assert(d!=0);

    for(c = icalset_get_first_component(f);
	c != 0;
	c = icalset_get_next_component(f)){

	icalcomponent *clone; 

	clone = icalcomponent_new_clone(c);

	icalset_add_component(d,clone);

	printf(" class %d\n",icalclassify(c,0,"user"));

    }

	icalset_free(f);
	icalset_free(d);
}

void test_classify()
{
    icalcomponent *c,*match; 

    icalset* f = icalset_new_file("../../test-data/classify.ics");

    assert(f!=0);

    c = icalset_get_first_component(f);
    match = icalset_get_next_component(f);
    
    printf("Class %d\n",icalclassify(c,match,"A@example.com"));

	icalset_free(f);
   
}

void print_span(int c, struct icaltime_span span ){

    printf("#%02d start: %s\n",c,ical_timet_string(span.start));
    printf("    end  : %s\n",ical_timet_string(span.end));

}

struct icaltimetype icaltime_as_local(struct icaltimetype tt) {
    return  icaltime_as_zone(tt,0);
}

void test_span()
{
    time_t tm1 = 973378800; /*Sat Nov  4 23:00:00 UTC 2000,
			      Sat Nov  4 15:00:00 PST 2000 */
    time_t tm2 = 973382400; /*Sat Nov  5 00:00:00 UTC 2000 
			      Sat Nov  4 16:00:00 PST 2000 */
    struct icaldurationtype dur;
    struct icaltime_span span;
    icalcomponent *c; 

    memset(&dur,0,sizeof(dur));
    dur.minutes = 30;

    span.start = tm1;
    span.end = tm2;
    print_span(0,span);

    /* Specify save timezone as in commend above */
    c = 
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
		icalproperty_vanew_dtstart(
		    icaltime_as_local(icaltime_from_timet(tm1,0)),
		    icalparameter_new_tzid("America/Los_Angeles"),0),
		icalproperty_vanew_dtend(
		    icaltime_as_local(icaltime_from_timet(tm2,0)),
		    icalparameter_new_tzid("America/Los_Angeles"),0),
	    0
	    );

    printf("%s\n",icalcomponent_as_ical_string(c));

    span = icalcomponent_get_span(c);

    print_span(1,span);

    icalcomponent_free(c);

    /* Use machine's local timezone. Same as above if run in America/Los_Angeles */
    c = 
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_vanew_dtstart(icaltime_from_timet(tm1,0),0),
	    icalproperty_vanew_dtend(icaltime_from_timet(tm2,0),0),
	    0
	    );

    span = icalcomponent_get_span(c);

    print_span(2,span);

    icalcomponent_free(c);

    /* Specify different timezone */
    c = 
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
		icalproperty_vanew_dtstart(
		    icaltime_as_local(icaltime_from_timet(tm1,0)),
		    icalparameter_new_tzid("America/New_York"),0),
		icalproperty_vanew_dtend(
		    icaltime_as_local(icaltime_from_timet(tm2,0)),
		    icalparameter_new_tzid("America/New_York"),0),
	    0
	    );
    span = icalcomponent_get_span(c);
    print_span(3,span);

    icalcomponent_free(c);


    /* Specify different timezone for start and end*/
    c = 
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
		icalproperty_vanew_dtstart(
		    icaltime_as_local(icaltime_from_timet(tm1,0)),
		    icalparameter_new_tzid("America/New_York"),0),
		icalproperty_vanew_dtend(
		    icaltime_as_local(icaltime_from_timet(tm2,0)),
		    icalparameter_new_tzid("America/Los_Angeles"),0),
	    0
	    );
    span = icalcomponent_get_span(c);
    print_span(4,span);

    icalcomponent_free(c);

    /* Use Duration */
    c = 
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
		icalproperty_vanew_dtstart(
		    icaltime_as_local(icaltime_from_timet(tm1,0)),
		    icalparameter_new_tzid("America/Los_Angeles"),0),
	    icalproperty_new_duration(dur),

	    0
	    );
    span = icalcomponent_get_span(c);
    print_span(5,span);

    icalcomponent_free(c);


#ifndef ICAL_ERRORS_ARE_FATAL
    /* Both UTC and Timezone -- an error */
    icalerror_clear_errno();
    c = 
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
		icalproperty_vanew_dtstart(icaltime_from_timet(tm1,0),
		    icalparameter_new_tzid("America/New_York"),0),
		icalproperty_vanew_dtend(icaltime_from_timet(tm2,0),
		    icalparameter_new_tzid("America/New_York"),0),
	    0
	    );

    span = icalcomponent_get_span(c);
    assert(icalerrno != ICAL_NO_ERROR);

    icalcomponent_free(c);

#endif /*ICAL_ERRORS_ARE_FATAL*/

}

icalcomponent* icalclassify_find_overlaps(icalset* set, icalcomponent* comp);

void test_overlaps()
{

#if 0 /* Hack, not working right now */
    icalcomponent *cset,*c;
    icalset *set;
    time_t tm1 = 973378800; /*Sat Nov  4 23:00:00 UTC 2000,
			      Sat Nov  4 15:00:00 PST 2000 */
    time_t tm2 = 973382400; /*Sat Nov  5 00:00:00 UTC 2000 
			      Sat Nov  4 16:00:00 PST 2000 */

    time_t hh = 1800; /* one half hour */

    set = icalset_new_file("../../test-data/overlaps.ics");

    printf("-- 1 -- \n");
    c = icalcomponent_vanew(
	ICAL_VEVENT_COMPONENT,
	icalproperty_vanew_dtstart(icaltime_from_timet(tm1-hh,0),0),
	icalproperty_vanew_dtend(icaltime_from_timet(tm2-hh,0),0),
	0
	);

    cset =  icalclassify_find_overlaps(set,c);

    printf("%s\n",icalcomponent_as_ical_string(cset));

    printf("-- 2 -- \n");
    c = icalcomponent_vanew(
	ICAL_VEVENT_COMPONENT,
	icalproperty_vanew_dtstart(icaltime_from_timet(tm1-hh,0),0),
	icalproperty_vanew_dtend(icaltime_from_timet(tm2,0),0),
	0
	);

    cset =  icalclassify_find_overlaps(set,c);

    printf("%s\n",icalcomponent_as_ical_string(cset));

    printf("-- 3 -- \n");
    c = icalcomponent_vanew(
	ICAL_VEVENT_COMPONENT,
	icalproperty_vanew_dtstart(icaltime_from_timet(tm1+5*hh,0),0),
	icalproperty_vanew_dtend(icaltime_from_timet(tm2+5*hh,0),0),
	0
	);

    cset =  icalclassify_find_overlaps(set,c);

    printf("%s\n",icalcomponent_as_ical_string(cset));

#endif 

}

void test_fblist()
{
    icalspanlist *sl;
    icalset* set = icalset_new_file("../../test-data/spanlist.ics");
    struct icalperiodtype period;

    sl = icalspanlist_new(set,
		     icaltime_from_string("19970324T120000Z"),
		     icaltime_from_string("19990424T020000Z"));
		
    printf("Restricted spanlist\n");
    icalspanlist_dump(sl);

    period= icalspanlist_next_free_time(sl,
				      icaltime_from_string("19970801T120000Z"));


    printf("Next Free time: %s\n",icaltime_as_ctime(period.start));
    printf("                %s\n",icaltime_as_ctime(period.end));


    icalspanlist_free(sl);

    printf("Unrestricted spanlist\n");

    sl = icalspanlist_new(set,
		     icaltime_from_string("19970324T120000Z"),
			  icaltime_null_time());
		
    printf("Restricted spanlist\n");

    icalspanlist_dump(sl);

    period= icalspanlist_next_free_time(sl,
				      icaltime_from_string("19970801T120000Z"));


    printf("Next Free time: %s\n",icaltime_as_ctime(period.start));
    printf("                %s\n",icaltime_as_ctime(period.end));


    icalspanlist_free(sl);

	icalset_free(set);

}

void test_convenience(){
    
    icalcomponent *c;
    int duration;

    c = icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_new_dtstart(icaltime_from_string("19970801T120000")),
	    icalproperty_new_dtend(icaltime_from_string("19970801T130000")),
	    0
	    ),
	0);

    printf("** 1 DTSTART and DTEND **\n%s\n\n",
	   icalcomponent_as_ical_string(c));

    duration = icaldurationtype_as_int(icalcomponent_get_duration(c))/60;

    printf("Start: %s\n",ictt_as_string(icalcomponent_get_dtstart(c)));
    printf("End:   %s\n",ictt_as_string(icalcomponent_get_dtend(c)));
    printf("Dur:   %d m\n",duration);

    icalcomponent_free(c);

    c = icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_new_dtstart(icaltime_from_string("19970801T120000Z")),
	    icalproperty_new_duration(icaldurationtype_from_string("PT1H30M")),
	    0
	    ),
	0);

    printf("\n** 2 DTSTART and DURATION **\n%s\n\n",
	   icalcomponent_as_ical_string(c));

    duration = icaldurationtype_as_int(icalcomponent_get_duration(c))/60;

    printf("Start: %s\n",ictt_as_string(icalcomponent_get_dtstart(c)));
    printf("End:   %s\n",ictt_as_string(icalcomponent_get_dtend(c)));
    printf("Dur:   %d m\n",duration);

    icalcomponent_free(c);

    c = icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_new_dtstart(icaltime_from_string("19970801T120000")),
	    icalproperty_new_dtend(icaltime_from_string("19970801T130000")),
	    0
	    ),
	0);

    icalcomponent_set_duration(c,icaldurationtype_from_string("PT1H30M"));

    printf("** 3 DTSTART and DTEND, Set DURATION **\n%s\n\n",
	   icalcomponent_as_ical_string(c));

    duration = icaldurationtype_as_int(icalcomponent_get_duration(c))/60;

    printf("Start: %s\n",ictt_as_string(icalcomponent_get_dtstart(c)));
    printf("End:   %s\n",ictt_as_string(icalcomponent_get_dtend(c)));
    printf("Dur:   %d m\n",duration);

    icalcomponent_free(c);

    c = icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_new_dtstart(icaltime_from_string("19970801T120000Z")),
	    icalproperty_new_duration(icaldurationtype_from_string("PT1H30M")),
	    0
	    ),
	0);

    icalcomponent_set_dtend(c,icaltime_from_string("19970801T133000Z"));

    printf("\n** 4 DTSTART and DURATION, set DTEND **\n%s\n\n",
	   icalcomponent_as_ical_string(c));

    duration = icaldurationtype_as_int(icalcomponent_get_duration(c))/60;

    printf("Start: %s\n",ictt_as_string(icalcomponent_get_dtstart(c)));
    printf("End:   %s\n",ictt_as_string(icalcomponent_get_dtend(c)));
    printf("Dur:   %d m\n",duration);

    icalcomponent_free(c);

    c = icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    0
	    ),
	0);

    icalcomponent_set_dtstart(c,icaltime_from_string("19970801T120000Z"));
    icalcomponent_set_dtend(c,icaltime_from_string("19970801T133000Z"));

    printf("\n** 5 Set DTSTART and DTEND **\n%s\n\n",
	   icalcomponent_as_ical_string(c));


    duration = icaldurationtype_as_int(icalcomponent_get_duration(c))/60;

    printf("Start: %s\n",ictt_as_string(icalcomponent_get_dtstart(c)));
    printf("End:   %s\n",ictt_as_string(icalcomponent_get_dtend(c)));
    printf("Dur:   %d m\n",duration);

    icalcomponent_free(c);

    c = icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    0
	    ),
	0);


    icalcomponent_set_dtstart(c,icaltime_from_string("19970801T120000Z"));
    icalcomponent_set_duration(c,icaldurationtype_from_string("PT1H30M"));

    printf("\n** 6 Set DTSTART and DURATION **\n%s\n\n",
	   icalcomponent_as_ical_string(c));


    duration = icaldurationtype_as_int(icalcomponent_get_duration(c))/60;

    printf("Start: %s\n",ictt_as_string(icalcomponent_get_dtstart(c)));
    printf("End:   %s\n",ictt_as_string(icalcomponent_get_dtend(c)));
    printf("Dur:   %d m\n",duration);

    icalcomponent_free(c);

}

void test_time_parser()
{
    struct icaltimetype tt;

    icalerror_errors_are_fatal = 0;

    tt = icaltime_from_string("19970101T1000");
    assert(icaltime_is_null_time(tt));

    tt = icaltime_from_string("19970101X100000");
    assert(icaltime_is_null_time(tt));

    tt = icaltime_from_string("19970101T100000");
    assert(!icaltime_is_null_time(tt));
    printf("%s\n",icaltime_as_ctime(tt));

    tt = icaltime_from_string("19970101T100000Z");
    assert(!icaltime_is_null_time(tt));
    printf("%s\n",icaltime_as_ctime(tt));

    tt = icaltime_from_string("19970101");
    assert(!icaltime_is_null_time(tt));
    printf("%s\n",icaltime_as_ctime(tt));

    icalerror_errors_are_fatal = 1;

}

void test_recur_parser()
{
    struct icalrecurrencetype rt; 

    printf("FREQ=YEARLY;UNTIL=20000131T090000Z;BYMONTH=1,2,3,4,8;BYYEARDAY=34,65,76,78;BYDAY=-1TU,3WE,-4FR,SU,SA\n");

    rt = icalrecurrencetype_from_string("FREQ=YEARLY;UNTIL=20000131T090000Z;BYMONTH=1,2,3,4,8;BYYEARDAY=34,65,76,78;BYDAY=-1TU,3WE,-4FR,SU,SA");

    printf("%s\n\n",icalrecurrencetype_as_string(&rt));

    printf("FREQ=DAILY;COUNT=3;BYMONTH=1,2,3,4,8;BYYEARDAY=34,65,76,78;BYDAY=-1TU,3WE,-4FR,SU,S\n");

    rt = icalrecurrencetype_from_string("FREQ=DAILY;COUNT=3;BYMONTH=1,2,3,4,8;BYYEARDAY=34,65,76,78;BYDAY=-1TU,3WE,-4FR,SU,SA");

    printf("%s\n",icalrecurrencetype_as_string(&rt));

}

char* ical_strstr(const char *haystack, const char *needle){
    return strstr(haystack,needle);
}

void test_start_of_week()
{
    struct icaltimetype tt2;
    struct icaltimetype tt1 = icaltime_from_string("19900110");
    int dow, doy,start_dow;

    do{
        struct tm stm;
        
        tt1 = icaltime_normalize(tt1);

        doy = icaltime_start_doy_of_week(tt1);
        dow = icaltime_day_of_week(tt1);

        tt2 = icaltime_from_day_of_year(doy,tt1.year);
        start_dow = icaltime_day_of_week(tt2);

        if(start_dow != 1){ /* Sunday is 1 */
            printf("failed: Start of week (%s) is not a Sunday \n for %s (doy=%d,dow=%d)\n",ictt_as_string(tt2), ictt_as_string(tt1),dow,start_dow);
        }


        assert(start_dow == 1);

        if(doy == 1){
            putc('.',stdout);
            fflush(stdout);
        }


        tt1.day+=1;
        
    } while(tt1.year < 2010);

    printf("\n");
}

void test_doy()
{
    struct icaltimetype tt1, tt2;
    int year, month;
    short doy,doy2;

	doy = -1;

    tt1 = icaltime_from_string("19900101");

    printf("Test icaltime_day_of_year() agreement with mktime\n");

    do{
        struct tm stm;

        tt1 = icaltime_normalize(tt1);

        stm.tm_sec = tt1.second;
        stm.tm_min = tt1.minute;
        stm.tm_hour = tt1.hour;
        stm.tm_mday = tt1.day;
        stm.tm_mon = tt1.month-1;
        stm.tm_year = tt1.year-1900;
        stm.tm_isdst = -1;

        mktime(&stm);

        doy = icaltime_day_of_year(tt1);

        if(doy == 1){
            putc('.',stdout);
            fflush(stdout);
        }

        doy2 = stm.tm_yday+1;

        if(doy != doy2){
            printf("Failed for %s (%d,%d)\n",ictt_as_string(tt1),doy,doy2);
        }

        assert(doy == doy2);

        tt1.day+=1;
        
    } while(tt1.year < 2010);

    printf("\nTest icaltime_day_of_year() agreement with icaltime_from_day_of_year()\n");

    tt1 = icaltime_from_string("19900101");

    do{      
        if(doy == 1){
            putc('.',stdout);
            fflush(stdout);
        }

        doy = icaltime_day_of_year(tt1);
        tt2 = icaltime_from_day_of_year(doy,tt1.year);
        doy2 = icaltime_day_of_year(tt2);

        assert(doy2 == doy);
        assert(icaltime_compare(tt1,tt2) == 0);

        tt1.day+=1;
        tt1 = icaltime_normalize(tt1);
        
    } while(tt1.year < 2010);

    printf("\n");


    tt1 = icaltime_from_string("19950301");
    doy = icaltime_day_of_year(tt1);
    tt2 = icaltime_from_day_of_year(doy,1995);
    printf("%d %s %s\n",doy, icaltime_as_ctime(tt1),icaltime_as_ctime(tt2));
    assert(tt2.day == 1 && tt2.month == 3);
    assert(doy == 60);

    tt1 = icaltime_from_string("19960301");
    doy = icaltime_day_of_year(tt1);
    tt2 = icaltime_from_day_of_year(doy,1996);
    printf("%d %s %s\n",doy, icaltime_as_ctime(tt1),icaltime_as_ctime(tt2));
    assert(tt2.day == 1 && tt2.month == 3);
    assert(doy == 61);

    tt1 = icaltime_from_string("19970301");
    doy = icaltime_day_of_year(tt1);
    tt2 = icaltime_from_day_of_year(doy,1997);
    printf("%d %s %s\n",doy, icaltime_as_ctime(tt1),icaltime_as_ctime(tt2));
    assert(tt2.day == 1 && tt2.month == 3);
    assert(doy == 60);

}

void test_x(){

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
#include <stdio.h>
#include <stdlib.h>
#include <ical.h>
 
    static const char test_icalcomp_str[] =
"BEGIN:VEVENT\r\n"
"RRULE\r\n"
" ;X-EVOLUTION-ENDDATE=20030209T081500\r\n"
" :FREQ=DAILY;COUNT=10;INTERVAL=6\r\n"
"X-COMMENT;X-FOO=BAR: Booga\r\n"
"END:VEVENT\r\n";
  
    icalcomponent *icalcomp;
    icalproperty *prop;
    struct icalrecurrencetype recur;
    int n_errors;
    
    icalcomp = icalparser_parse_string ((char *) test_icalcomp_str);
    if (!icalcomp) {
	fprintf (stderr, "main(): could not parse the component\n");
	exit (EXIT_FAILURE);
    }
    
    printf("%s\n\n",icalcomponent_as_ical_string(icalcomp));

    n_errors = icalcomponent_count_errors (icalcomp);
    if (n_errors) {
	icalproperty *p;
	
	for (p = icalcomponent_get_first_property (icalcomp,
						   ICAL_XLICERROR_PROPERTY);
	     p;
	     p = icalcomponent_get_next_property (icalcomp,
						  ICAL_XLICERROR_PROPERTY)) {
	    const char *str;
	    
	    str = icalproperty_as_ical_string (p);
	    fprintf (stderr, "error: %s\n", str);
	}
    }
    
    prop = icalcomponent_get_first_property (icalcomp, ICAL_RRULE_PROPERTY);
    if (!prop) {
	fprintf (stderr, "main(): could not get the RRULE property");
	exit (EXIT_FAILURE);
    }
    
    recur = icalproperty_get_rrule (prop);
    
    printf("%s\n",icalrecurrencetype_as_string(&recur));

	icalcomponent_free(icalcomp);

}

void test_gauge_sql() {


    icalgauge *g;
    char* str;
    
    str= "SELECT DTSTART,DTEND,COMMENT FROM VEVENT,VTODO WHERE VEVENT.SUMMARY = 'Bongoa' AND SEQUENCE < 5";

    printf("\n%s\n",str);

    g = icalgauge_new_from_sql(str);
    
    icalgauge_dump(g);

    icalgauge_free(g);

    str="SELECT * FROM VEVENT,VTODO WHERE VEVENT.SUMMARY = 'Bongoa' AND SEQUENCE < 5 OR METHOD != 'CREATE'";

    printf("\n%s\n",str);

    g = icalgauge_new_from_sql(str);
    
    icalgauge_dump(g);

    icalgauge_free(g);

    str="SELECT * FROM VEVENT WHERE SUMMARY == 'BA301'";

    printf("\n%s\n",str);

    g = icalgauge_new_from_sql(str);
    
    icalgauge_dump(g);

    icalgauge_free(g);

    str="SELECT * FROM VEVENT WHERE SUMMARY == 'BA301'";

    printf("\n%s\n",str);

    g = icalgauge_new_from_sql(str);
    
    icalgauge_dump(g);

    icalgauge_free(g);

    str="SELECT * FROM VEVENT WHERE LOCATION == '104 Forum'";

    printf("\n%s\n",str);

    g = icalgauge_new_from_sql(str);
    
    icalgauge_dump(g);

    icalgauge_free(g);

}

void test_gauge_compare() {

    icalgauge *g;
    icalcomponent *c;
    char* str;

    /* Equality */

    c =  icalcomponent_vanew(ICAL_VCALENDAR_COMPONENT,
	      icalcomponent_vanew(ICAL_VEVENT_COMPONENT,
		  icalproperty_new_dtstart(
		      icaltime_from_string("20000101T000002")),0),0);

    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART = '20000101T000002'");

    printf("SELECT * FROM VEVENT WHERE DTSTART = '20000101T000002'\n");
    assert(c!=0);
    assert(g!=0);

    assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);


    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART = '20000101T000001'");

    printf("SELECT * FROM VEVENT WHERE DTSTART = '20000101T000001'\n");

    assert(g!=0);
    assert(icalgauge_compare(g,c) == 0);

    icalgauge_free(g);

    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART != '20000101T000003'");

    printf("SELECT * FROM VEVENT WHERE DTSTART != '20000101T000003'\n");


    assert(g!=0);
    assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);


    /* Less than */

    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART < '20000101T000003'");

    printf("SELECT * FROM VEVENT WHERE DTSTART < '20000101T000003'\n");

    assert(icalgauge_compare(g,c) == 1);

    assert(g!=0);
    icalgauge_free(g);

    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART < '20000101T000002'");

    printf("SELECT * FROM VEVENT WHERE DTSTART < '20000101T000002'\n");


    assert(g!=0);
    assert(icalgauge_compare(g,c) == 0);

    icalgauge_free(g);

    /* Greater than */

    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART > '20000101T000001'");

    printf("SELECT * FROM VEVENT WHERE DTSTART > '20000101T000001'\n");


    assert(g!=0);
    assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);

    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART > '20000101T000002'");

    printf("SELECT * FROM VEVENT WHERE DTSTART > '20000101T000002'\n");


    assert(g!=0);
    assert(icalgauge_compare(g,c) == 0);

    icalgauge_free(g);


    /* Greater than or Equal to */

    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART >= '20000101T000002'");

    printf("SELECT * FROM VEVENT WHERE DTSTART >= '20000101T000002'\n");


    assert(g!=0);
    assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);

    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART >= '20000101T000003'");

    printf("SELECT * FROM VEVENT WHERE DTSTART >= '20000101T000003'\n");


    assert(g!=0);
    assert(icalgauge_compare(g,c) == 0);

    icalgauge_free(g);

    /* Less than or Equal to */

    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART <= '20000101T000002'");

    printf("SELECT * FROM VEVENT WHERE DTSTART <= '20000101T000002'\n");


    assert(g!=0);
    assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);

    g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART <= '20000101T000001'");

    printf("SELECT * FROM VEVENT WHERE DTSTART <= '20000101T000001'\n");


    assert(g!=0);
    assert(icalgauge_compare(g,c) == 0);

    icalgauge_free(g);

    icalcomponent_free(c);

    /* Combinations */

    c =  icalcomponent_vanew(ICAL_VCALENDAR_COMPONENT,
	      icalcomponent_vanew(ICAL_VEVENT_COMPONENT,
		  icalproperty_new_dtstart(
		      icaltime_from_string("20000102T000000")),0),0);


    str =  "SELECT * FROM VEVENT WHERE DTSTART > '20000101T000000' and DTSTART < '20000103T000000'";

    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0); assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);

    str =  "SELECT * FROM VEVENT WHERE DTSTART > '20000101T000000' and DTSTART < '20000102T000000'";

    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0); assert(icalgauge_compare(g,c) == 0);

    icalgauge_free(g);

    str =  "SELECT * FROM VEVENT WHERE DTSTART > '20000101T000000' or DTSTART < '20000102T000000'";

    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0); assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);


    icalcomponent_free(c);

    /* Combinations, non-cannonical component */

    c = icalcomponent_vanew(ICAL_VEVENT_COMPONENT,
		  icalproperty_new_dtstart(
		      icaltime_from_string("20000102T000000")),0);


    str =  "SELECT * FROM VEVENT WHERE DTSTART > '20000101T000000' and DTSTART < '20000103T000000'";

    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0); assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);

    str =  "SELECT * FROM VEVENT WHERE DTSTART > '20000101T000000' and DTSTART < '20000102T000000'";

    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0); assert(icalgauge_compare(g,c) == 0);

    icalgauge_free(g);

    str =  "SELECT * FROM VEVENT WHERE DTSTART > '20000101T000000' or DTSTART < '20000102T000000'";

    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0); assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);


    icalcomponent_free(c);


    /* Complex comparisions */

    c =  icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalproperty_new_method(ICAL_METHOD_REQUEST),
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_new_dtstart(
		icaltime_from_string("20000101T000002")),
	    icalproperty_new_comment("foo"),
	    icalcomponent_vanew(
		ICAL_VALARM_COMPONENT,
		icalproperty_new_dtstart(
		    icaltime_from_string("20000101T120000")),
		
		0),
	    0),
	0);
    
    
    str = "SELECT * FROM VEVENT WHERE VALARM.DTSTART = '20000101T120000'";

    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0);assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);

    str = "SELECT * FROM VEVENT WHERE COMMENT = 'foo'";
    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0);assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);
	
    str = "SELECT * FROM VEVENT WHERE COMMENT = 'foo' AND  VALARM.DTSTART = '20000101T120000'";
    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0);assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);

    str = "SELECT * FROM VEVENT WHERE COMMENT = 'bar' AND  VALARM.DTSTART = '20000101T120000'";
    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0);assert(icalgauge_compare(g,c) == 0);

    icalgauge_free(g);

    str = "SELECT * FROM VEVENT WHERE COMMENT = 'bar' or  VALARM.DTSTART = '20000101T120000'";
    g = icalgauge_new_from_sql(str);
    printf("%s\n",str);
    assert(g!=0);assert(icalgauge_compare(g,c) == 1);

    icalgauge_free(g);

    icalcomponent_free(c);

}

icalcomponent* make_component(int i){

    icalcomponent *c;

    struct icaltimetype t = icaltime_from_string("20000101T120000Z");

    t.day += i;

    icaltime_normalize(t);

    c =  icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalproperty_new_method(ICAL_METHOD_REQUEST),
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_new_dtstart(t),
	    0),
	0);

    assert(c != 0);

    return c;

}
void test_fileset()
{
    icalfileset *fs;
    icalcomponent *c;
    int i;
    char *path = "test_fileset.ics";
    icalgauge  *g = icalgauge_new_from_sql(
	"SELECT * FROM VEVENT WHERE DTSTART > '20000103T120000Z' AND DTSTART <= '20000106T120000Z'");


    unlink(path);

    fs = icalfileset_new(path);
    
    assert(fs != 0);

    for (i = 0; i!= 10; i++){
	c = make_component(i);
	icalfileset_add_component(fs,c);
    }

    icalfileset_commit(fs);

    icalfileset_free(fs);
    fs = icalfileset_new(path);


    printf("== No Selections \n");

    for (c = icalfileset_get_first_component(fs);
	 c != 0;
	 c = icalfileset_get_next_component(fs)){
	struct icaltimetype t = icalcomponent_get_dtstart(c);

	printf("%s\n",icaltime_as_ctime(t));
    }

    icalfileset_select(fs,g);

    printf("\n== DTSTART > '20000103T120000Z' AND DTSTART <= '20000106T120000Z' \n");

    for (c = icalfileset_get_first_component(fs);
	 c != 0;
	 c = icalfileset_get_next_component(fs)){
	struct icaltimetype t = icalcomponent_get_dtstart(c);

	printf("%s\n",icaltime_as_ctime(t));
    }

    icalfileset_free(fs);

	icalgauge_free(g);
    
}

void microsleep(int us)
{
#ifndef WIN32
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = us;

    select(0,0,0,0,&tv);
#else
	Sleep(us);
#endif
}


void test_file_locks()
{
#ifndef WIN32
    pid_t pid;
    char *path = "test_fileset_locktest.ics";
    icalfileset *fs;
    icalcomponent *c, *c2;
    struct icaldurationtype d;
    int i;
    int final,sec;

    icalfileset_safe_saves = 1;

    icalerror_clear_errno();

    unlink(path);

    fs = icalfileset_new(path);

    if(icalfileset_get_first_component(fs)==0){
	c = make_component(0);
	
	d = icaldurationtype_from_int(1);
	
	icalcomponent_set_duration(c,d);
	
	icalfileset_add_component(fs,c);
	
	c2 = icalcomponent_new_clone(c);
	
	icalfileset_add_component(fs,c2);
	
	icalfileset_commit(fs);
    }
    
    icalfileset_free(fs);
    
    assert(icalerrno == ICAL_NO_ERROR);
    
    pid = fork();
    
    assert(pid >= 0);
    
    if(pid == 0){
	/*child*/
	int i;

	microsleep(rand()/(RAND_MAX/100));

	for(i = 0; i< 50; i++){
	    fs = icalfileset_new(path);


	    assert(fs != 0);

	    c = icalfileset_get_first_component(fs);

	    assert(c!=0);
	   
	    d = icalcomponent_get_duration(c);
	    d = icaldurationtype_from_int(icaldurationtype_as_int(d)+1);
	    
	    icalcomponent_set_duration(c,d);
	    icalcomponent_set_summary(c,"Child");	  
  
	    c2 = icalcomponent_new_clone(c);
	    icalcomponent_set_summary(c2,"Child");
	    icalfileset_add_component(fs,c2);

	    icalfileset_mark(fs);
	    icalfileset_commit(fs);

	    icalfileset_free(fs);

	    microsleep(rand()/(RAND_MAX/20));


	}

	exit(0);

    } else {
	/* parent */
	int i;
	
	for(i = 0; i< 50; i++){
	    fs = icalfileset_new(path);

	    assert(fs != 0);

	    c = icalfileset_get_first_component(fs);
	    
	    assert(c!=0);

	    d = icalcomponent_get_duration(c);
	    d = icaldurationtype_from_int(icaldurationtype_as_int(d)+1);
	    
	    icalcomponent_set_duration(c,d);
	    icalcomponent_set_summary(c,"Parent");

	    c2 = icalcomponent_new_clone(c);
	    icalcomponent_set_summary(c2,"Parent");
	    icalfileset_add_component(fs,c2);

	    icalfileset_mark(fs);
	    icalfileset_commit(fs);
	    icalfileset_free(fs);

	    putc('.',stdout);
	    fflush(stdout);

	}
    }

    assert(waitpid(pid,0,0)==pid);
	

    fs = icalfileset_new(path);
    
    i=1;

    c = icalfileset_get_first_component(fs);
    final = icaldurationtype_as_int(icalcomponent_get_duration(c));
    for (c = icalfileset_get_next_component(fs);
	 c != 0;
	 c = icalfileset_get_next_component(fs)){
	struct icaldurationtype d = icalcomponent_get_duration(c);
	sec = icaldurationtype_as_int(d);

	/*printf("%d,%d ",i,sec);*/
	assert(i == sec);
	i++;
    }

    printf("\nFinal: %d\n",final);

    
    assert(sec == final);
#endif
}

void test_action()
{
    icalcomponent *c;
    icalproperty *p;
    
    static const char test_icalcomp_str[] =
"BEGIN:VEVENT\n"
"ACTION:EMAIL\n"
"ACTION:PROCEDURE\n"
"ACTION:AUDIO\n"
"ACTION:FUBAR\n"
"END:VEVENT\r\n";

     
    c = icalparser_parse_string ((char *) test_icalcomp_str);
    if (!c) {
	fprintf (stderr, "main(): could not parse the component\n");
	exit (EXIT_FAILURE);
    }
    
    printf("%s\n\n",icalcomponent_as_ical_string(c));

    p = icalcomponent_get_first_property(c,ICAL_ACTION_PROPERTY);

    assert(icalproperty_get_action(p) == ICAL_ACTION_EMAIL);

    p = icalcomponent_get_next_property(c,ICAL_ACTION_PROPERTY);

    assert(icalproperty_get_action(p) == ICAL_ACTION_PROCEDURE);

    p = icalcomponent_get_next_property(c,ICAL_ACTION_PROPERTY);

    assert(icalproperty_get_action(p) == ICAL_ACTION_AUDIO);

    p = icalcomponent_get_next_property(c,ICAL_ACTION_PROPERTY);

    assert(icalproperty_get_action(p) == ICAL_ACTION_X);
    assert(regrstrcmp(icalvalue_get_x(icalproperty_get_value(p)), "FUBAR")==0);

	icalcomponent_free(c);
}

        

void test_trigger()
{

    struct icaltriggertype tr;
    icalcomponent *c;
    icalproperty *p;
    const char* str;
    
    static const char test_icalcomp_str[] =
"BEGIN:VEVENT\n"
"TRIGGER;VALUE=DATE-TIME:19980403T120000\n"
"TRIGGER:-PT15M\n"
"TRIGGER:19980403T120000\n"
"TRIGGER;VALUE=DURATION:-PT15M\n"
"END:VEVENT\r\n";
  
    
    c = icalparser_parse_string ((char *) test_icalcomp_str);
    if (!c) {
	fprintf (stderr, "main(): could not parse the component\n");
	exit (EXIT_FAILURE);
    }
    
    printf("%s\n\n",icalcomponent_as_ical_string(c));

    for(p = icalcomponent_get_first_property(c,ICAL_TRIGGER_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(c,ICAL_TRIGGER_PROPERTY)){
	tr = icalproperty_get_trigger(p);

	if(!icaltime_is_null_time(tr.time)){
	    printf("value=DATE-TIME:%s\n", icaltime_as_ical_string(tr.time));
	} else {
	    printf("value=DURATION:%s\n", icaldurationtype_as_ical_string(tr.duration));
	}   
    }

	icalcomponent_free(c);

    /* Trigger, as a DATETIME */
    tr.duration = icaldurationtype_null_duration();
    tr.time = icaltime_from_string("19970101T120000");
    p = icalproperty_new_trigger(tr);
    str = icalproperty_as_ical_string(p);

    printf("%s\n",str);
    assert(regrstrcmp("TRIGGER\n ;VALUE=DATE-TIME\n :19970101T120000\n",str) == 0);
    icalproperty_free(p);

    /* TRIGGER, as a DURATION */
    tr.time = icaltime_null_time();
    tr.duration = icaldurationtype_from_string("P3DT3H50M45S");
    p = icalproperty_new_trigger(tr);
    str = icalproperty_as_ical_string(p);
    
    printf("%s\n",str);
    assert(regrstrcmp("TRIGGER\n ;VALUE=DURATION\n :P3DT3H50M45S\n",str) == 0);
    icalproperty_free(p);

    /* TRIGGER, as a DATETIME, VALUE=DATETIME*/
    tr.duration = icaldurationtype_null_duration();
    tr.time = icaltime_from_string("19970101T120000");
    p = icalproperty_new_trigger(tr);
    icalproperty_add_parameter(p,icalparameter_new_value( ICAL_VALUE_DATETIME));
    str = icalproperty_as_ical_string(p);

    printf("%s\n",str);
    assert(regrstrcmp("TRIGGER\n ;VALUE=DATE-TIME\n :19970101T120000\n",str) == 0);
    icalproperty_free(p);

    /*TRIGGER, as a DURATION, VALUE=DATETIME */
    tr.time = icaltime_null_time();
    tr.duration = icaldurationtype_from_string("P3DT3H50M45S");
    p = icalproperty_new_trigger(tr);
    icalproperty_add_parameter(p,icalparameter_new_value( ICAL_VALUE_DATETIME ));

    str = icalproperty_as_ical_string(p);
    
    printf("%s\n",str);
    assert(regrstrcmp("TRIGGER\n ;VALUE=DURATION\n :P3DT3H50M45S\n",str) == 0);
    icalproperty_free(p);

    /* TRIGGER, as a DATETIME, VALUE=DURATION*/
    tr.duration = icaldurationtype_null_duration();
    tr.time = icaltime_from_string("19970101T120000");
    p = icalproperty_new_trigger(tr);
    icalproperty_add_parameter(p,icalparameter_new_value( ICAL_VALUE_DURATION));
    str = icalproperty_as_ical_string(p);

    printf("%s\n",str);
    assert(regrstrcmp("TRIGGER\n ;VALUE=DATE-TIME\n :19970101T120000\n",str) == 0);
    icalproperty_free(p);

    /*TRIGGER, as a DURATION, VALUE=DURATION */
    tr.time = icaltime_null_time();
    tr.duration = icaldurationtype_from_string("P3DT3H50M45S");
    p = icalproperty_new_trigger(tr);
    icalproperty_add_parameter(p,icalparameter_new_value( ICAL_VALUE_DURATION));

    str = icalproperty_as_ical_string(p);
    
    printf("%s\n",str);
    assert(regrstrcmp("TRIGGER\n ;VALUE=DURATION\n :P3DT3H50M45S\n",str) == 0);
    icalproperty_free(p);


   /* TRIGGER, as a DATETIME, VALUE=BINARY  */
    tr.duration = icaldurationtype_null_duration();
    tr.time = icaltime_from_string("19970101T120000");
    p = icalproperty_new_trigger(tr);
    icalproperty_add_parameter(p,icalparameter_new_value(ICAL_VALUE_BINARY));
    str = icalproperty_as_ical_string(p);

    printf("%s\n",str);
    assert(regrstrcmp("TRIGGER\n ;VALUE=DATE-TIME\n :19970101T120000\n",str) == 0);
    icalproperty_free(p);

    /*TRIGGER, as a DURATION, VALUE=BINARY   */
    tr.time = icaltime_null_time();
    tr.duration = icaldurationtype_from_string("P3DT3H50M45S");
    p = icalproperty_new_trigger(tr);
    icalproperty_add_parameter(p,icalparameter_new_value(ICAL_VALUE_BINARY));

    str = icalproperty_as_ical_string(p);
    
    printf("%s\n",str);
    assert(regrstrcmp("TRIGGER\n ;VALUE=DURATION\n :P3DT3H50M45S\n",str) == 0);
    icalproperty_free(p);



}

void test_rdate()
{

    struct icaldatetimeperiodtype dtp;
    icalproperty *p;
    const char* str;
    struct icalperiodtype period;

    period.start = icaltime_from_string("19970101T120000");
    period.end = icaltime_null_time();
    period.duration = icaldurationtype_from_string("PT3H10M15S");

    /* RDATE, as DATE-TIME */
    dtp.time = icaltime_from_string("19970101T120000");
    dtp.period = icalperiodtype_null_period();
    p = icalproperty_new_rdate(dtp);
    str = icalproperty_as_ical_string(p);
    printf("%s\n",str);
    assert(regrstrcmp("RDATE\n ;VALUE=DATE-TIME\n :19970101T120000\n",str) == 0);
    icalproperty_free(p); 


    /* RDATE, as PERIOD */
    dtp.time = icaltime_null_time();
    dtp.period = period;
    p = icalproperty_new_rdate(dtp);

    str = icalproperty_as_ical_string(p);
    printf("%s\n",str);
    assert(regrstrcmp("RDATE\n ;VALUE=PERIOD\n :19970101T120000/PT3H10M15S\n",str) == 0);
    icalproperty_free(p); 

    /* RDATE, as DATE-TIME, VALUE=DATE-TIME */
    dtp.time = icaltime_from_string("19970101T120000");
    dtp.period = icalperiodtype_null_period();
    p = icalproperty_new_rdate(dtp);
    icalproperty_add_parameter(p,icalparameter_new_value(ICAL_VALUE_DATETIME));
    str = icalproperty_as_ical_string(p);
    printf("%s\n",str);
    assert(regrstrcmp("RDATE\n ;VALUE=DATE-TIME\n :19970101T120000\n",str) == 0);
    icalproperty_free(p); 


    /* RDATE, as PERIOD, VALUE=DATE-TIME */
    dtp.time = icaltime_null_time();
    dtp.period = period;
    p = icalproperty_new_rdate(dtp);
    icalproperty_add_parameter(p,icalparameter_new_value(ICAL_VALUE_DATETIME));
    str = icalproperty_as_ical_string(p);
    printf("%s\n",str);
    assert(regrstrcmp("RDATE\n ;VALUE=PERIOD\n :19970101T120000/PT3H10M15S\n",str) == 0);
    icalproperty_free(p); 


    /* RDATE, as DATE-TIME, VALUE=PERIOD */
    dtp.time = icaltime_from_string("19970101T120000");
    dtp.period = icalperiodtype_null_period();
    p = icalproperty_new_rdate(dtp);
    icalproperty_add_parameter(p,icalparameter_new_value(ICAL_VALUE_PERIOD));
    str = icalproperty_as_ical_string(p);
    printf("%s\n",str);
    assert(regrstrcmp("RDATE\n ;VALUE=DATE-TIME\n :19970101T120000\n",str) == 0);
    icalproperty_free(p); 


    /* RDATE, as PERIOD, VALUE=PERIOD */
    dtp.time = icaltime_null_time();
    dtp.period = period;
    p = icalproperty_new_rdate(dtp);
    icalproperty_add_parameter(p,icalparameter_new_value(ICAL_VALUE_PERIOD));
    str = icalproperty_as_ical_string(p);
    printf("%s\n",str);
    assert(regrstrcmp("RDATE\n ;VALUE=PERIOD\n :19970101T120000/PT3H10M15S\n",str) == 0);
    icalproperty_free(p); 


    /* RDATE, as DATE-TIME, VALUE=BINARY */
    dtp.time = icaltime_from_string("19970101T120000");
    dtp.period = icalperiodtype_null_period();
    p = icalproperty_new_rdate(dtp);
    icalproperty_add_parameter(p,icalparameter_new_value(ICAL_VALUE_BINARY));
    str = icalproperty_as_ical_string(p);
    printf("%s\n",str);
    assert(regrstrcmp("RDATE\n ;VALUE=DATE-TIME\n :19970101T120000\n",str) == 0);
    icalproperty_free(p); 


    /* RDATE, as PERIOD, VALUE=BINARY */
    dtp.time = icaltime_null_time();
    dtp.period = period;
    p = icalproperty_new_rdate(dtp);
    icalproperty_add_parameter(p,icalparameter_new_value(ICAL_VALUE_BINARY));
    str = icalproperty_as_ical_string(p);
    printf("%s\n",str);
    assert(regrstrcmp("RDATE\n ;VALUE=PERIOD\n :19970101T120000/PT3H10M15S\n",str) == 0);
    icalproperty_free(p); 


}


void test_langbind()
{
    icalcomponent *c, *inner; 
    icalproperty *p;

    static const char test_str[] =
"BEGIN:VEVENT\n"
"ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com\n"
"COMMENT: Comment that \n spans a line\n"
"COMMENT: Comment with \"quotable\" \'characters\' and other \t bad magic \n things \f Yeah.\n"
"DTSTART:19970101T120000\n"
"DTSTART:19970101T120000Z\n"
"DTSTART:19970101\n"
"DURATION:P3DT4H25M\n"
"FREEBUSY:19970101T120000/19970101T120000\n"
"FREEBUSY:19970101T120000/P3DT4H25M\n"
"END:VEVENT\n";
  

    printf("%s\n",test_str);

    c = icalparser_parse_string(test_str);

    printf("-----------\n%s\n-----------\n",icalcomponent_as_ical_string(c));

    inner = icalcomponent_get_inner(c);


    for(
	p = icallangbind_get_first_property(inner,"ANY");
	p != 0;
	p = icallangbind_get_next_property(inner,"ANY")
	) { 

	printf("%s\n",icallangbind_property_eval_string(p,":"));
    }



    p = icalcomponent_get_first_property(inner,ICAL_ATTENDEE_PROPERTY);

    icalproperty_set_parameter_from_string(p,"CUTYPE","INDIVIDUAL");

    printf("%s\n",icalproperty_as_ical_string(p));
        

    icalproperty_set_value_from_string(p,"mary@foo.org","TEXT");

    printf("%s\n",icalproperty_as_ical_string(p));

	icalcomponent_free(c);
}

void test_property_parse()
{
    icalproperty *p;

    p= icalproperty_new_from_string(
                         "ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com");

    assert (p !=  0);
    printf("%s\n",icalproperty_as_ical_string(p));

	icalproperty_free(p);

    p= icalproperty_new_from_string("DTSTART:19970101T120000Z\n");

    assert (p !=  0);
    printf("%s\n",icalproperty_as_ical_string(p));

	icalproperty_free(p);

}


void test_value_parameter() 
{

    icalcomponent *c;
    icalproperty *p;
    icalparameter *param;
    
    static const char test_icalcomp_str[] =
"BEGIN:VEVENT\n"
"DTSTART;VALUE=DATE-TIME:19971123T123000\n"
"DTSTART;VALUE=DATE:19971123\n"
"DTSTART;VALUE=FOO:19971123T123000\n"
"END:VEVENT\n";
     
    c = icalparser_parse_string ((char *) test_icalcomp_str);
    if (!c) {
	fprintf (stderr, "main(): could not parse the component\n");
	exit (EXIT_FAILURE);
    }

    printf("%s",icalcomponent_as_ical_string(c));

    p = icalcomponent_get_first_property(c,ICAL_DTSTART_PROPERTY);
    param = icalproperty_get_first_parameter(p,ICAL_VALUE_PARAMETER);
    assert(icalparameter_get_value(param) == ICAL_VALUE_DATETIME);

    p = icalcomponent_get_next_property(c,ICAL_DTSTART_PROPERTY);
    param = icalproperty_get_first_parameter(p,ICAL_VALUE_PARAMETER);
    assert(icalparameter_get_value(param) == ICAL_VALUE_DATE);

	icalcomponent_free(c);
}


void test_x_property() 
{
    icalproperty *p;
    
    p= icalproperty_new_from_string(
       "X-LIC-PROPERTY: This is a note");

    printf("%s\n",icalproperty_as_ical_string(p));

    assert(icalproperty_isa(p) == ICAL_X_PROPERTY);
    assert(regrstrcmp(icalproperty_get_x_name(p),"X-LIC-PROPERTY")==0);
    assert(regrstrcmp(icalproperty_get_x(p)," This is a note")==0);

	icalproperty_free(p);
}

void test_utcoffset()
{
    icalproperty *p;

    p = icalproperty_new_from_string("TZOFFSETFROM:-001608");
    printf("%s\n",icalproperty_as_ical_string(p));

	icalproperty_free(p);
}

int main(int argc, char *argv[])
{
    int c;
    extern char *optarg;
    extern int optopt;
    int errflg=0;
    char* program_name = strrchr(argv[0],'/');
    int ttime=0, trecur=0,tspan=0, tmisc=0, tgauge = 0, tfile = 0,
        tbasic = 0;

	set_zone_directory("../../zoneinfo");

	putenv("TZ=");

    if(argc==1) {
	ttime = trecur = tspan = tmisc = tgauge = tfile = tbasic = 1;
    }

#ifndef WIN32
    while ((c = getopt(argc, argv, "t:s:r:m:g:f:b:")) != -1) {
	switch (c) {

	    case 'b': {
		tbasic = atoi(optarg);
		break;
	    }

	    case 't': {
		ttime = atoi(optarg);
		break;
	    }

	    case 's': {
		tspan = atoi(optarg);
		break;
	    }

	    case 'r': {
		trecur = atoi(optarg);
		break;
	    }


	    case 'm': {
		tmisc = atoi(optarg);
		break;
	    }
	    

	    case 'g': {
		tgauge = atoi(optarg);
		break;
	    }

	    case 'f': {
		tfile = atoi(optarg);
		break;
	    }
	    
	    case ':': {/* Option given without an operand */
		fprintf(stderr,
			"%s: Option -%c requires an operand\n", 
			program_name,optopt);
		errflg++;
		break;
	    }
	    case '?': {
		errflg++;
	    }
	    
	}
    } 
#endif


    if(ttime==1 || ttime==2){
	printf("\n------------Test time parser ----------\n");
	test_time_parser();
	    
    }

    if(ttime==1 || ttime==3){
	printf("\n------------Test time----------------\n");
	test_time();
    }
 
    if(ttime==1 || ttime==4){
	printf("\n------------Test day of year---------\n");
	test_doy();
    }

    if(ttime==1 || ttime==5){
	printf("\n------------Test duration---------------\n");
	test_duration();
    }	

    if(ttime==1 || ttime==6){
	printf("\n------------Test period ----------------\n");
	test_period();
    }	

    if(ttime==1 || ttime==7){
	printf("\n------------Test DTSTART ----------------\n");
        test_dtstart();
    }
	
    if(ttime==1 || ttime==8){
	printf("\n------------Test day of year of week start----\n");
	test_start_of_week();
    }
	    

	    
    if(trecur==1 || trecur==2){
	printf("\n------------Test recur parser ----------\n");
	test_recur_parser();
    }
     
    if(trecur==1 || trecur==3){
	printf("\n------------Test recur---------------\n");
	test_recur();
    }

    if(trecur==1 || trecur==4){
	printf("\n------------Test parameter bug---------\n");
	test_recur_parameter_bug();
    }

    if(trecur==1 || trecur==5){
	printf("\n------------Test Array Expansion---------\n");
	test_expand_recurrence();
    }

    


    if(tspan==1 || tspan==2){
	printf("\n------------Test FBlist------------\n");
	test_fblist();
    }

    if(tspan==1 || tspan==3){
	printf("\n------------Test Overlaps------------\n");
	test_overlaps();
    }

    if(tspan==1 || tspan==4){
	printf("\n------------Test Span----------------\n");
	test_span();
    }

    if(tgauge == 1 || tgauge == 2){
	printf("\n------------Test Gauge SQL----------------\n");
	test_gauge_sql();
    }	

    if(tgauge == 1 || tgauge == 3){
	printf("\n------------Test Gauge Compare--------------\n");
	test_gauge_compare();
    }	

    if(tfile ==1 || tfile == 2){
	printf("\n------------Test File Set--------------\n");
	test_fileset();
    }

    /*
      File locking is currently broken
    if(tfile ==1 || tfile == 3){
	printf("\n------------Test File Locks--------------\n");
	test_file_locks();
    }
    */


    if(tmisc == 1 || tmisc  == 2){
	printf("\n------------Test X Props and Params--------\n");
	test_x();
    }

    if(tmisc == 1 || tmisc  == 3){
	printf("\n------------Test Trigger ------------------\n");
	test_trigger();
    }

    if(tmisc == 1 || tmisc  == 4){

	printf("\n------------Test Restriction---------------\n");
	test_restriction();
    }

    if(tmisc == 1 || tmisc  == 5){

	printf("\n------------Test RDATE---------------\n");
	test_rdate();
    }

    if(tmisc == 1 || tmisc  == 6){

	printf("\n------------Test language binding---------------\n");
	test_langbind();
    }


    if(tmisc == 1 || tmisc  == 7){

	printf("\n------------Test property parser---------------\n");
	test_property_parse();
    }

    if(tmisc == 1 || tmisc  == 8){
	printf("\n------------Test Action ------------------\n");
	test_action();
    }

    if(tmisc == 1 || tmisc  == 9){
	printf("\n------------Test Value Parameter ------------------\n");
	test_value_parameter();
    }

    if(tmisc == 1 || tmisc  == 10){
	printf("\n------------Test X property ------------------\n");
	test_x_property();
    }

    if(tmisc == 1 || tmisc  == 11){
	printf("\n-----------Test request status-------\n");
	test_requeststat();
    }


    if(tmisc == 1 || tmisc  == 12){
	printf("\n-----------Test UTC-OFFSET-------\n");
	test_utcoffset();
    }


   

    if(tbasic == 1 || tbasic  == 2){
	printf("\n------------Test Values---------------\n");
	test_values();
    }
    
    if(tbasic == 1 || tbasic  == 3){
	printf("\n------------Test Parameters-----------\n");
	test_parameters();
    }

    if(tbasic == 1 || tbasic  == 4){
	printf("\n------------Test Properties-----------\n");
	test_properties();
    }

    if(tbasic == 1 || tbasic  == 5){
	printf("\n------------Test Components ----------\n");
	test_components();
    }	

    if(tmisc == 1){

	printf("\n------------Test Convenience ------------\n");
	test_convenience();
	
	
	printf("\n------------Test classify ---------------\n");
	test_classify();
	
	
	printf("\n------------Test Iterators-----------\n");
	test_iterators();
	
	
	
	printf("\n------------Test strings---------------\n");
	test_strings();
	
	printf("\n------------Test Compare---------------\n");
	test_compare();
	
	printf("\n------------Create Components --------\n");
	create_new_component();
	
	printf("\n----- Create Components with vaargs ---\n");
	create_new_component_with_va_args();

	printf("\n------------Test Memory---------------\n");
	test_memory();
    }


    printf("-------------- Tests complete -----------\n");

	icaltimezone_free_builtin_timezones();

	icalmemory_free_ring();

	free_zone_directory();

    return 0;
}



