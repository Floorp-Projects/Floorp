/* -*- Mode: C -*-
  ======================================================================
  FILE: usecases.c
  CREATOR: eric 03 April 1999
  
  DESCRIPTION:
  
  $Id: storage.c,v 1.2 2001/12/21 18:56:57 mikep%oeone.com Exp $
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
  The original code is usecases.c

    
  ======================================================================*/

#include "ical.h"
#include <assert.h>
#include <string.h> /* for strdup */
#include <stdlib.h> /* for malloc */
#include <stdio.h> /* for printf */
#include <time.h> /* for time() */
#include "icalmemory.h"
#include "icaldirset.h"
#include "icalfileset.h"
#include "icalerror.h"
#include "icalrestriction.h"
#include "icalcalendar.h"

#define OUTPUT_FILE "filesetout.ics"

char str[] = "BEGIN:VCALENDAR\n\
PRODID:\"-//RDU Software//NONSGML HandCal//EN\"\n\
VERSION:2.0\n\
BEGIN:VTIMEZONE\n\
TZID:US-Eastern\n\
BEGIN:STANDARD\n\
DTSTART:19981025T020000\n\
RDATE:19981025T020000\n\
TZOFFSETFROM:-0400\n\
TZOFFSETTO:-0500\n\
TZNAME:EST\n\
END:STANDARD\n\
BEGIN:DAYLIGHT\n\
DTSTART:19990404T020000\n\
RDATE:19990404T020000\n\
TZOFFSETFROM:-0500\n\
TZOFFSETTO:-0400\n\
TZNAME:EDT\n\
END:DAYLIGHT\n\
END:VTIMEZONE\n\
BEGIN:VEVENT\n\
DTSTAMP:19980309T231000Z\n\
UID:guid-1.host1.com\n\
ORGANIZER;ROLE=CHAIR:MAILTO:mrbig@host.com\n\
ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com\n\
DESCRIPTION:Project XYZ Review Meeting\n\
CATEGORIES:MEETING\n\
CLASS:PUBLIC\n\
CREATED:19980309T130000Z\n\
SUMMARY:XYZ Project Review\n\
DTSTART;TZID=US-Eastern:19980312T083000\n\
DTEND;TZID=US-Eastern:19980312T093000\n\
LOCATION:1CP Conference Room 4350\n\
END:VEVENT\n\
BEGIN:BOOGA\n\
DTSTAMP:19980309T231000Z\n\
X-LIC-FOO:Booga\n\
DTSTOMP:19980309T231000Z\n\
UID:guid-1.host1.com\n\
END:BOOGA\n\
END:VCALENDAR";

char str2[] = "BEGIN:VCALENDAR\n\
PRODID:\"-//RDU Software//NONSGML HandCal//EN\"\n\
VERSION:2.0\n\
BEGIN:VEVENT\n\
DTSTAMP:19980309T231000Z\n\
UID:guid-1.host1.com\n\
ORGANIZER;ROLE=CHAIR:MAILTO:mrbig@host.com\n\
ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com\n\
DESCRIPTION:Project XYZ Review Meeting\n\
CATEGORIES:MEETING\n\
CLASS:PUBLIC\n\
CREATED:19980309T130000Z\n\
SUMMARY:XYZ Project Review\n\
DTSTART;TZID=US-Eastern:19980312T083000\n\
DTEND;TZID=US-Eastern:19980312T093000\n\
LOCATION:1CP Conference Room 4350\n\
END:VEVENT\n\
END:VCALENDAR\n\
";


void test_fileset()
{
    icalfileset *cout;
    int month = 0;
    int count=0;
    struct icaltimetype start, end;
    icalcomponent *c,*clone, *itr;

    start = icaltime_from_timet( time(0),0);
    end = start;
    end.hour++;

    cout = icalfileset_new(OUTPUT_FILE);
    assert(cout != 0);

    c = icalparser_parse_string(str2);
    assert(c != 0);

    /* Add data to the file */

    for(month = 1; month < 10; month++){
	icalcomponent *event;
	icalproperty *dtstart, *dtend;

        cout = icalfileset_new(OUTPUT_FILE);
        assert(cout != 0);

	start.month = month; 
	end.month = month;
	
	clone = icalcomponent_new_clone(c);
	assert(clone !=0);
	event = icalcomponent_get_first_component(clone,ICAL_VEVENT_COMPONENT);
	assert(event != 0);

	dtstart = icalcomponent_get_first_property(event,ICAL_DTSTART_PROPERTY);
	assert(dtstart!=0);
	icalproperty_set_dtstart(dtstart,start);
        
	dtend = icalcomponent_get_first_property(event,ICAL_DTEND_PROPERTY);
	assert(dtend!=0);
	icalproperty_set_dtend(dtend,end);
	
	icalfileset_add_component(cout,clone);
	icalfileset_commit(cout);

        icalfileset_free(cout);

    }


    /* Print them out */


    cout = icalfileset_new(OUTPUT_FILE);
    assert(cout != 0);
    
    for (itr = icalfileset_get_first_component(cout);
         itr != 0;
         itr = icalfileset_get_next_component(cout)){

      icalcomponent *event;
      icalproperty *dtstart, *dtend;

      count++;

      event = icalcomponent_get_first_component(itr,ICAL_VEVENT_COMPONENT);

      dtstart = icalcomponent_get_first_property(event,ICAL_DTSTART_PROPERTY);
      dtend = icalcomponent_get_first_property(event,ICAL_DTEND_PROPERTY);
      
      printf("%d %s %s\n",count, icalproperty_as_ical_string(dtstart),
             icalproperty_as_ical_string(dtend));

    }

    /* Remove all of them */

    icalfileset_free(cout);

    cout = icalfileset_new(OUTPUT_FILE);
    assert(cout != 0);
    
    for (itr = icalfileset_get_first_component(cout);
         itr != 0;
         itr = icalfileset_get_next_component(cout)){


      icalfileset_remove_component(cout, itr);
    }

    icalfileset_free(cout);


    /* Print them out again */

    cout = icalfileset_new(OUTPUT_FILE);
    assert(cout != 0);
    count =0;
    
    for (itr = icalfileset_get_first_component(cout);
         itr != 0;
         itr = icalfileset_get_next_component(cout)){

      icalcomponent *event;
      icalproperty *dtstart, *dtend;

      count++;

      event = icalcomponent_get_first_component(itr,ICAL_VEVENT_COMPONENT);

      dtstart = icalcomponent_get_first_property(event,ICAL_DTSTART_PROPERTY);
      dtend = icalcomponent_get_first_property(event,ICAL_DTEND_PROPERTY);
      
      printf("%d %s %s\n",count, icalproperty_as_ical_string(dtstart),
             icalproperty_as_ical_string(dtend));

    }

    icalfileset_free(cout);


}



int test_dirset()
{

    icalcomponent *c, *gauge;
    icalerrorenum error;
    icalcomponent *itr;
    icalfileset* cluster;
    struct icalperiodtype rtime;
    icaldirset *s = icaldirset_new("store");
    int i;

    assert(s != 0);

    rtime.start = icaltime_from_timet( time(0),0);

    cluster = icalfileset_new(OUTPUT_FILE);

    assert(cluster != 0);

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
	    icalcomponent *clone, *inner;
	    icalproperty *p;

	    inner = icalcomponent_get_first_component(itr,ICAL_VEVENT_COMPONENT);
            if (inner == 0){
              continue;
            }

	    /* Change the dtstart and dtend times in the component
               pointed to by Itr*/

	    clone = icalcomponent_new_clone(itr);
	    inner = icalcomponent_get_first_component(itr,ICAL_VEVENT_COMPONENT);

	    assert(icalerrno == ICAL_NO_ERROR);
	    assert(inner !=0);

	    /* DTSTART*/
	    p = icalcomponent_get_first_property(inner,ICAL_DTSTART_PROPERTY);
	    assert(icalerrno  == ICAL_NO_ERROR);

	    if (p == 0){
		p = icalproperty_new_dtstart(rtime.start);
		icalcomponent_add_property(inner,p);
	    } else {
		icalproperty_set_dtstart(p,rtime.start);
	    }
	    assert(icalerrno  == ICAL_NO_ERROR);

	    /* DTEND*/
	    p = icalcomponent_get_first_property(inner,ICAL_DTEND_PROPERTY);
	    assert(icalerrno  == ICAL_NO_ERROR);

	    if (p == 0){
		p = icalproperty_new_dtstart(rtime.end);
		icalcomponent_add_property(inner,p);
	    } else {
		icalproperty_set_dtstart(p,rtime.end);
	    }
	    assert(icalerrno  == ICAL_NO_ERROR);
	    
	    printf("\n----------\n%s\n---------\n",icalcomponent_as_ical_string(inner));

	    error = icaldirset_add_component(s,
					     icalcomponent_new_clone(itr));
	    
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
	c = icaldirset_get_next_component(s)){

	if (c != 0){
	    printf("%s", icalcomponent_as_ical_string(c));;
	} else {
	    printf("Failed to get component\n");
	}

    }

    /* Remove all of the components */
    i=0;
    while((c=icaldirset_get_current_component(s)) != 0 ){
	i++;

	icaldirset_remove_component(s,c);
    }
	

    icaldirset_free(s);
    return 0;
}

#if 0
void test_calendar()
{
    icalcomponent *comp;
    icalfileset *c;
    icaldirset *s;
    icalcalendar* calendar = icalcalendar_new("calendar");
    icalerrorenum error;
    struct icaltimetype atime = icaltime_from_timet( time(0),0,0);

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


int main(int argc, char *argv[])
{

/*    printf("\n------------Test File Set---------------\n");
      test_fileset(); */

    printf("\n------------Test Dir Set---------------\n");
    test_dirset();

#if 0    


    printf("\n------------Test Calendar---------------\n");
    test_calendar();

#endif

    return 0;
}



