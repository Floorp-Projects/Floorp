/* errors.c */

#include "ical.h"
#include <stdio.h>

void program_errors()
{
    /*Most routines will set icalerrno on errors. This is an
      enumeration defined in icalerror.h */
    
    icalcomponent *c; 

    icalerror_clear_errno();

    c = icalcomponent_new(ICAL_VEVENT_COMPONENT);

    if (icalerrno != ICAL_NO_ERROR){

	fprintf(stderr,"Horrible libical error: %s\n",
		icalerror_strerror(icalerrno));
		
    }

}

void component_errors(icalcomponent *comp)
{
    int errors;
    icalproperty *p;

    /* presume that we just got this component from the parser */

    errors = icalcomponent_count_errors(comp);

    printf("This component has %d parsing errors\n", errors);

    /* Print out all of the parsing errors. This is not strictly
       correct, because it does not descend into any sub-components,
       as icalcomponent_count_errors() does. */

    for(p = icalcomponent_get_first_property(comp,ICAL_XLICERROR_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(comp,ICAL_XLICERROR_PROPERTY))
    {
	
	printf("-- The error is %s:\n",icalproperty_get_xlicerror(p));
    }



    /* Check the component for iTIP compilance, and add more
       X-LIC-ERROR properties if it is non-compilant. */
    icalrestriction_check(comp);
    

    /* Count the new errors.  */
    if(errors != icalcomponent_count_errors(comp)){
       printf(" -- The component also has iTIP restriction errors \n");	
    }

    /* Since there are iTIP restriction errors, it may be impossible
       to process this component as an iTIP request. In this case, the
       X-LIC-ERROR proeprties should be expressed as REQUEST-STATUS
       properties in the reply. This following routine makes this
       conversion */


    icalcomponent_convert_errors(comp);

}
