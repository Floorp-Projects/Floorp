/* Access_component.c */

#include "ical.h"

#include <assert.h>
#include <string.h> /* for strdup */
#include <stdlib.h> /* for malloc */
#include <stdio.h> /* for printf */
#include <time.h> /* for time() */
#include "icalmemory.h"

void do_something(icalcomponent *c);

/* Creating iCal Components 

   There are two ways to create new component in libical. You can
   build the component from primitive parts, or you can create it
   from a string.

   There are two variations of the API for building the component from
   primitive parts. In the first variation, you add each parameter and
   value to a property, and then add each property to a
   component. This results in a long series of function calls. This
   style is show in create_new_component()

   The second variation uses vargs lists to nest many primitive part
   constructors, resulting in a compact, neatly formated way to create
   components. This style is shown in create_new_component_with_va_args()

  
   
*/
   
icalcomponent* create_new_component()
{

    /* variable definitions */
    icalcomponent* calendar;
    icalcomponent* event;
    struct icaltimetype atime = icaltime_from_timet( time(0),0);
    struct icalperiodtype rtime;
    icalproperty* property;

    /* Define a time type that will use as data later. */
    rtime.start = icaltime_from_timet( time(0),0);
    rtime.end = icaltime_from_timet( time(0),0);
    rtime.end.hour++;

    /* Create calendar and add properties */

    calendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
    
    /* Nearly every libical function call has the same general
       form. The first part of the name defines the 'class' for the
       function, and the first argument will be a pointer to a struct
       of that class. So, icalcomponent_ functions will all take
       icalcomponent* as their first argument. */

    /* The next call creates a new proeprty and immediately adds it to the 
       'calendar' component. */ 

    icalcomponent_add_property(
	calendar,
	icalproperty_new_version("2.0")
	);

    
    /* Here is the short version of the memory rules: 

         If the routine name has "new" in it: 
	     Caller owns the returned memory. 
             If you pass in a string, the routine takes the memory. 	 

         If the routine name has "add" in it:
	     The routine takes control of the component, property, 
	     parameter or value memory.

         If the routine returns a string ( "get" and "as_ical_string" )
	     The library owns the returned memory. 

	  There are more rules, so refer to the documentation for more 
	  details. 

    */

    icalcomponent_add_property(
	calendar,
	icalproperty_new_prodid("-//RDU Software//NONSGML HandCal//EN")
	);
    
    /* Add an event */

    event = icalcomponent_new(ICAL_VEVENT_COMPONENT);

    icalcomponent_add_property(
	event,
	icalproperty_new_dtstamp(atime)
	);

    /* In the previous call, atime is a struct, and it is passed in by value. 
       This is how all compound types of values are handled. */

    icalcomponent_add_property(
	event,
	icalproperty_new_uid("guid-1.host1.com")
	);

    /* add a property that has parameters */
    property = icalproperty_new_organizer("mailto:mrbig@host.com");
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_role(ICAL_ROLE_CHAIR)
	);

    icalcomponent_add_property(event,property);

    /* In this style of component creation, you need to use an extra
       call to add parameters to properties, but the form of this
       operation is the same as adding a property to a component */

    /* add another property that has parameters */
    property = icalproperty_new_attendee("mailto:employee-A@host.com");
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_role(ICAL_ROLE_REQPARTICIPANT)
	);

    icalproperty_add_parameter(
	property,
	icalparameter_new_rsvp(1)
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
	icalproperty_new_class(ICAL_CLASS_PUBLIC)
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
	icalparameter_new_tzid("US-Eastern")
	);

    icalcomponent_add_property(event,property);


    property = icalproperty_new_dtend(atime);
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_tzid("US-Eastern")
	);

    icalcomponent_add_property(event,property);

    icalcomponent_add_property(
	event,
	icalproperty_new_location("1CP Conference Room 4350")
	);

    icalcomponent_add_component(calendar,event);

    return calendar;
}


/* Now, create the same component as in the previous routine, but use
the constructor style. */

icalcomponent* create_new_component_with_va_args()
{

    /* This is a similar set up to the last routine */
    icalcomponent* calendar;
    struct icaltimetype atime = icaltime_from_timet( time(0),0);
    struct icalperiodtype rtime;
    
    rtime.start = icaltime_from_timet( time(0),0);
    rtime.end = icaltime_from_timet( time(0),0);
    rtime.end.hour++;

    /* Some of these routines are the same as those in the previous
       routine, but we've also added several 'vanew' routines. These
       'vanew' routines take a list of properties, parameters or
       values and add each of them to the parent property or
       component. */

    calendar = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalproperty_new_version("2.0"),
	    icalproperty_new_prodid("-//RDU Software//NONSGML HandCal//EN"),
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,
		icalproperty_new_dtstamp(atime),
		icalproperty_new_uid("guid-1.host1.com"),
		icalproperty_vanew_organizer(
		    "mailto:mrbig@host.com",
		    icalparameter_new_role(ICAL_ROLE_CHAIR),
		    0
		    ),
		icalproperty_vanew_attendee(
		    "mailto:employee-A@host.com",
		    icalparameter_new_role(ICAL_ROLE_REQPARTICIPANT),
		    icalparameter_new_rsvp(1),
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
		    icalparameter_new_tzid("US-Eastern"),
		    0
		    ),
		icalproperty_vanew_dtend(
		    atime,
		    icalparameter_new_tzid("US-Eastern"),
		    0
		    ),
		icalproperty_new_location("1CP Conference Room 4350"),
		0
		),
	    0
	    );

   
    /* Note that properties with no parameters can use the regular
       'new' constructor, while those with parameters use the 'vanew'
       constructor. And, be sure that the last argument in the 'vanew'
       call is a zero. Without, your program will probably crash. */

    return calendar;
}


void find_sub_components(icalcomponent* comp)
{
    icalcomponent *c;
    
    /* The second parameter to icalcomponent_get_first_component
       indicates the type of component to search for. This will
       iterate through all sub-components */
    for(c = icalcomponent_get_first_component(comp,ICAL_ANY_COMPONENT);
	c != 0;
	c = icalcomponent_get_next_component(comp,ICAL_ANY_COMPONENT)){

	do_something(c);
    }

    /* This will iterate only though VEVENT sub-components */

    for(c = icalcomponent_get_first_component(comp,ICAL_VEVENT_COMPONENT);
	c != 0;
	c = icalcomponent_get_next_component(comp,ICAL_VEVENT_COMPONENT)){

	do_something(c);
    }

}

/* Ical components only have one internal iterator, so removing the
   object that the iterator points to can cause problems. Here is the
   right way to remove components */

void remove_vevent_sub_components(icalcomponent* comp){
    
    icalcomponent *c, *next;

    for( c = icalcomponent_get_first_component(comp,ICAL_VEVENT_COMPONENT);
	 c != 0;
	 c = next)
    {
	next  = icalcomponent_get_next_component(comp,ICAL_VEVENT_COMPONENT);

	icalcomponent_remove_component(comp,c);

	do_something(c);
    }

}

