/* -*- Mode: C -*-
  ======================================================================
  FILE: usecases.c
  CREATOR: eric 03 April 1999
  
  DESCRIPTION:
  
  $Id: usecases.c,v 1.1 2001/12/21 19:03:54 mikep%oeone.com Exp $
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

char str[] = "BEGIN:VCALENDAR\
PRODID:\"-//RDU Software//NONSGML HandCal//EN\"\
VERSION:2.0\
BEGIN:VEVENT\
DTSTAMP:19980309T231000Z\
UID:guid-1.host1.com\
ORGANIZER;ROLE=CHAIR:MAILTO:mrbig@host.com\
ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com\
DESCRIPTION:Project XYZ Review Meeting\
CATEGORIES:MEETING\
CREATED:19980309T130000Z\
SUMMARY:XYZ Project Review\
DTSTART;TZID=US-Eastern:19980312T083000\
DTEND;TZID=US-Eastern:19980312T093000\
END:VEVENT\
END:VCALENDAR";




/* Here are some ways to work with values. */
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


}

void test_parameters()
{
    icalparameter *p;

    p = icalparameter_new_cn("A Common Name");

    printf("Common Name: %s\n",icalparameter_get_cn(p));

    printf("As String: %s\n",icalparameter_as_ical_string(p));
}


