/* access_properties_and_parameters.c */

#include "ical.h"
#include <string.h>

/* Get a particular parameter out of a component. This routine will
   return a list of strings of all attendees who are required. Note
   that this routine assumes that the component that we pass in is a
   VEVENT.  */
   
void get_required_attendees(icalcomponent* event)
{
    icalproperty* p;
    icalparameter* parameter;
    
    assert(event != 0);
    assert(icalcomponent_isa(event) == ICAL_VEVENT_COMPONENT);
    
    /* This loop iterates over all of the ATTENDEE properties in the
       event */
    
    /* The iteration routines save their state in the event
       struct, so the are not thread safe unless you lock the whole
       component. */
    
    for(
	p = icalcomponent_get_first_property(event,ICAL_ATTENDEE_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(event,ICAL_ATTENDEE_PROPERTY)
	) {
	
	/* Get the first ROLE parameter in the property. There should
           only be one, so we won't bother to iterate over them. But,
           you can iterate over parameters just like with properties */

	parameter = icalproperty_get_first_parameter(p,ICAL_ROLE_PARAMETER);

	/* If the parameter indicates the participant is required, get
           the attendees name and stick a copy of it into the output
           array */

	if ( icalparameter_get_role(parameter) == ICAL_ROLE_REQPARTICIPANT) 
	{
	    /* Remember, the caller does not own this string, so you
               should strdup it if you want to change it. */
	    const char *attendee = icalproperty_get_attendee(p);
	}
    }

}

/* Here is a similar example. If an attendee has a PARTSTAT of
   NEEDSACTION or has no PARTSTAT parameter, change it to
   TENTATIVE. */
   
void update_attendees(icalcomponent* event)
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

	    /* There was no PARTSTAT parameter, so add one.  */
	    icalproperty_add_parameter(
		p,
		icalparameter_new_partstat(ICAL_PARTSTAT_TENTATIVE)
		);

	} else if (icalparameter_get_partstat(parameter) == ICAL_PARTSTAT_NEEDSACTION) {
	    /* Remove the NEEDSACTION parameter and replace it with
               TENTATIVE */
	    
	    icalproperty_remove_parameter(p,ICAL_PARTSTAT_PARAMETER);
	    
	    /* Don't forget to free it */
	    icalparameter_free(parameter);

	    /* Add a new one */
	    icalproperty_add_parameter(
		p,
		icalparameter_new_partstat(ICAL_PARTSTAT_TENTATIVE)
		);
	}

    }
}

/* Here are some examples of manipulating properties */

void test_properties()
{
    icalproperty *prop;
    icalparameter *param;
    icalvalue *value;

    icalproperty *clone;

    /* Create a new property */
    prop = icalproperty_vanew_comment(
	"Another Comment",
	icalparameter_new_cn("A Common Name 1"),
	icalparameter_new_cn("A Common Name 2"),
	icalparameter_new_cn("A Common Name 3"),
       	icalparameter_new_cn("A Common Name 4"),
	0); 

    /* Iterate through all of the parameters in the property */
    for(param = icalproperty_get_first_parameter(prop,ICAL_ANY_PARAMETER);
	param != 0; 
	param = icalproperty_get_next_parameter(prop,ICAL_ANY_PARAMETER)) {
						
	printf("Prop parameter: %s\n",icalparameter_get_cn(param));
    }    

    /* Get a string representation of the property's value */
    printf("Prop value: %s\n",icalproperty_get_comment(prop));

    /* Spit out the property in its RFC 2445 representation */
    printf("As iCAL string:\n %s\n",icalproperty_as_ical_string(prop));
    
    /* Make a copy of the property. Caller owns the memory */
    clone = icalproperty_new_clone(prop);

    /* Get a reference to the value within the clone property */
    value = icalproperty_get_value(clone);

    printf("Value: %s",icalvalue_as_ical_string(value));

    /* Free the original and the clone */
    icalproperty_free(clone);
    icalproperty_free(prop);

}
