/* -*- Mode: C -*-
  ======================================================================
  FILE: testclassify.c
  CREATOR: eric 11 February 2000
  
  $Id: testclassify.c,v 1.1 2001/11/15 19:27:51 mikep%oeone.com Exp $
  $Locker:  $
    
 (C) COPYRIGHT 2000 Eric Busboom
 http://www.softwarestudio.org

 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 
 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


 ======================================================================*/

#include <stdio.h> /* for printf */
#include "ical.h"
#include <errno.h>
#include <string.h> /* For strerror */
#include "icalset.h"
#include "icalclassify.h"


struct class_map {
	ical_class class;
	char *str;
} class_map[] = {
    {ICAL_NO_CLASS,"No class"},
    {ICAL_PUBLISH_NEW_CLASS,"New Publish"},
    {ICAL_PUBLISH_UPDATE_CLASS,"Update Publish"},
    {ICAL_REQUEST_NEW_CLASS,"New request"},
    {ICAL_REQUEST_UPDATE_CLASS,"Update"},
    {ICAL_REQUEST_RESCHEDULE_CLASS,"Reschedule"},
    {ICAL_REQUEST_DELEGATE_CLASS,"Delegate"},
    {ICAL_REQUEST_NEW_ORGANIZER_CLASS,"New Organizer"},
    {ICAL_REQUEST_FORWARD_CLASS,"Forward"},
    {ICAL_REQUEST_STATUS_CLASS,"Status request"},
    {ICAL_REPLY_ACCEPT_CLASS,"Accept reply"},
    {ICAL_REPLY_DECLINE_CLASS,"Decline reply"},
    {ICAL_REPLY_CRASHER_ACCEPT_CLASS,"Crasher's accept reply"},
    {ICAL_REPLY_CRASHER_DECLINE_CLASS,"Crasher's decline reply"},
    {ICAL_ADD_INSTANCE_CLASS,"Add instance"},
    {ICAL_CANCEL_EVENT_CLASS,"Cancel event"},
    {ICAL_CANCEL_INSTANCE_CLASS,"Cancel instance"},
    {ICAL_CANCEL_ALL_CLASS,"Cancel all instances"},
    {ICAL_REFRESH_CLASS,"Refresh"},
    {ICAL_COUNTER_CLASS,"Counter"},
    {ICAL_DECLINECOUNTER_CLASS,"Decline counter"},
    {ICAL_MALFORMED_CLASS,"Malformed"}, 
    {ICAL_OBSOLETE_CLASS,"Obsolete"},
    {ICAL_MISSEQUENCED_CLASS,"Missequenced"},
    {ICAL_UNKNOWN_CLASS,"Unknown"}
};

char* find_class_string(ical_class class)
{
    int i; 

    for (i = 0;class_map[i].class != ICAL_UNKNOWN_CLASS;i++){
	if (class_map[i].class == class){
	    return class_map[i].str;
	}
    }

    return "Unknown";
}


int main(int argc, char* argv[])
{
    icalcomponent *c;
    int i=0;

    icalset* f = icalset_new_file("../../test-data/incoming.ics");
    icalset* cal = icalset_new_file("../../test-data/calendar.ics");

    assert(f!= 0);
    assert(cal!=0);
	

    /* Foreach incoming message */
    for(c=icalset_get_first_component(f);c!=0;
	c=icalset_get_next_component(f)){
	
	ical_class class;
	icalcomponent *match;
	icalcomponent *inner = icalcomponent_get_first_real_component(c);
	icalcomponent *p;
	const char *this_uid;
	const char *i_x_note=0;
	const char *c_x_note=0;

	i++;

	if(inner == 0){
	    continue;
	}

	p = icalcomponent_get_first_property(inner,ICAL_UID_PROPERTY);
	this_uid = icalproperty_get_uid(p);

	assert(this_uid != 0);

	/* Find a booked component that is matched to the incoming
	   message, based on the incoming component's UID, SEQUENCE
	   and RECURRENCE-ID*/

	match = icalset_fetch(cal,this_uid);

	class = icalclassify(c,match,"A@example.com");

	for(p = icalcomponent_get_first_property(c,ICAL_X_PROPERTY);
	    p!= 0;
	    p = icalcomponent_get_next_property(c,ICAL_X_PROPERTY)){
	    if(strcmp(icalproperty_get_x_name(p),"X-LIC-NOTE")==0){
		i_x_note = icalproperty_get_x(p);
	    }
	}


	if(i_x_note == 0){
	    i_x_note = "None";
	}

	for(p = icalcomponent_get_first_property(match,ICAL_X_PROPERTY);
	    p!= 0;
	    p = icalcomponent_get_next_property(match,ICAL_X_PROPERTY)){
	    if(strcmp(icalproperty_get_x_name(p),"X-LIC-NOTE")==0){
		c_x_note = icalproperty_get_x(p);
	    }
	}

	if(c_x_note == 0){
	    c_x_note = "None";
	}


	printf("Test %d\nIncoming:      %s\nMatched:       %s\nClassification: %s\n\n",i,i_x_note,c_x_note,find_class_string(class));	
    }

    return 0;
}


