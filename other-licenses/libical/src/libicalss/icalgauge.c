/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalgauge.c
 CREATOR: eric 23 December 1999


 $Id: icalgauge.c,v 1.3 2002/03/14 15:17:57 mikep%oeone.com Exp $
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

#include "ical.h"
#include "icalgauge.h"
#include "icalgaugeimpl.h"
#include <stdlib.h>

extern char* input_buffer;
extern char* input_buffer_p;
int ssparse(void);

struct icalgauge_impl *icalss_yy_gauge;

icalgauge* icalgauge_new_from_sql(char* sql)
{
    struct icalgauge_impl *impl;

    int r;
    
    if ( ( impl = (struct icalgauge_impl*)
	   malloc(sizeof(struct icalgauge_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    impl->select = pvl_newlist();
    impl->from = pvl_newlist();
    impl->where = pvl_newlist();

    icalss_yy_gauge = impl;

    input_buffer_p = input_buffer = sql;
    r = ssparse();

    return impl;
}


void icalgauge_free(icalgauge* gauge)
{
      struct icalgauge_impl *impl =  (struct icalgauge_impl*)gauge;
      struct icalgauge_where *w;

      assert(impl->select != 0);
      assert(impl->where != 0);
      assert(impl->from != 0);

      if(impl->select){
	  while( (w=pvl_pop(impl->select)) != 0){
	      if(w->value != 0){
		  free(w->value);
	      }
	      free(w);
	  }
	  pvl_free(impl->select);
      }

      if(impl->where){
	  while( (w=pvl_pop(impl->where)) != 0){
	      
	      if(w->value != 0){
		  free(w->value);
	      }
	      free(w);
	  }
	  pvl_free(impl->where);
      }

      if(impl->from){
	  pvl_free(impl->from);
      }

	  free(impl);
      
}


int icalgauge_compare_recurse(icalcomponent* comp, icalcomponent* gauge)
{
    int pass = 1,localpass = 0;
    icalproperty *p;
    icalcomponent *child,*subgauge; 
    icalcomponent_kind gaugekind, compkind;

    icalerror_check_arg_rz( (comp!=0), "comp");
    icalerror_check_arg_rz( (gauge!=0), "gauge");

    gaugekind = icalcomponent_isa(gauge);
    compkind = icalcomponent_isa(comp);

    if( ! (gaugekind == compkind || gaugekind == ICAL_ANY_COMPONENT) ){
	return 0;
    }   

    /* Test properties. For each property in the gauge, search through
       the component for a similar property. If one is found, compare
       the two properties value with the comparison specified in the
       gauge with the X-LIC-COMPARETYPE parameter */
    
    for(p = icalcomponent_get_first_property(gauge,ICAL_ANY_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(gauge,ICAL_ANY_PROPERTY)){
	
	icalproperty* targetprop; 
	icalparameter* compareparam;
	icalparameter_xliccomparetype compare;
	int rel; /* The relationship between the gauge and target values.*/
	
	/* Extract the comparison type from the gauge. If there is no
	   comparison type, assume that it is "EQUAL" */
	
	compareparam = icalproperty_get_first_parameter(
	    p,
	    ICAL_XLICCOMPARETYPE_PARAMETER);
	
	if (compareparam!=0){
	    compare = icalparameter_get_xliccomparetype(compareparam);
	} else {
	    compare = ICAL_XLICCOMPARETYPE_EQUAL;
	}
	
	/* Find a property in the component that has the same type
	   as the gauge property. HACK -- multiples of a single
	   property type in the gauge will match only the first
	   instance in the component */
	
	targetprop = icalcomponent_get_first_property(comp,
						      icalproperty_isa(p));
	
	if(targetprop != 0){
	
	    /* Compare the values of the gauge property and the target
	       property */
	    
	    rel = icalvalue_compare(icalproperty_get_value(p),
				    icalproperty_get_value(targetprop));
	    
	    /* Now see if the comparison is equavalent to the comparison
	       specified in the gauge */
	    
	    if (rel == compare){ 
		localpass++; 
	    } else if (compare == ICAL_XLICCOMPARETYPE_LESSEQUAL && 
		       ( rel == ICAL_XLICCOMPARETYPE_LESS ||
			 rel == ICAL_XLICCOMPARETYPE_EQUAL)) {
		localpass++;
	    } else if (compare == ICAL_XLICCOMPARETYPE_GREATEREQUAL && 
		       ( rel == ICAL_XLICCOMPARETYPE_GREATER ||
			 rel == ICAL_XLICCOMPARETYPE_EQUAL)) {
		localpass++;
	    } else if (compare == ICAL_XLICCOMPARETYPE_NOTEQUAL && 
		       ( rel == ICAL_XLICCOMPARETYPE_GREATER ||
			 rel == ICAL_XLICCOMPARETYPE_LESS)) {
		localpass++;
	    } else {
		localpass = 0;
	    }
	    
	    pass = pass && (localpass>0);
	}
    }
    
    /* Test subcomponents. Look for a child component that has a
       counterpart in the gauge. If one is found, recursively call
       icaldirset_test */
    
    for(subgauge = icalcomponent_get_first_component(gauge,ICAL_ANY_COMPONENT);
	subgauge != 0;
	subgauge = icalcomponent_get_next_component(gauge,ICAL_ANY_COMPONENT)){
	
	gaugekind = icalcomponent_isa(subgauge);

	if (gaugekind == ICAL_ANY_COMPONENT){
	    child = icalcomponent_get_first_component(comp,ICAL_ANY_COMPONENT);
	} else {
	    child = icalcomponent_get_first_component(comp,gaugekind);
	}
	
	if(child !=0){
	    localpass = icalgauge_compare_recurse(child,subgauge);
	    pass = pass && localpass;
	} else {
	    pass = 0;
	}
    }
    
    return pass;   
}


int icalgauge_compare(icalgauge* gauge,icalcomponent* comp)
{

    struct icalgauge_impl *impl = (struct icalgauge_impl*)gauge;
    icalcomponent *inner; 
    int local_pass = 0;
    int last_clause = 1, this_clause = 1;
    pvl_elem e;

    icalerror_check_arg_rz( (comp!=0), "comp");
    icalerror_check_arg_rz( (gauge!=0), "gauge");
    
    inner = icalcomponent_get_first_real_component(comp);

    if(inner == 0){
        inner = comp;
    }


    /* Check that this component is one of the FROM types */
    local_pass = 0;
    for(e = pvl_head(impl->from);e!=0;e=pvl_next(e)){
	icalcomponent_kind k = (icalcomponent_kind)pvl_data(e);

	if(k == icalcomponent_isa(inner)){
	    local_pass=1;
	}
    }
    
    if(local_pass == 0){
	return 0;
    }

    
    /* Check each where clause against the component */
    for(e = pvl_head(impl->where);e!=0;e=pvl_next(e)){
	struct icalgauge_where *w = pvl_data(e);
	icalcomponent *sub_comp;
	icalvalue *v;
	icalproperty *prop;
	icalvalue_kind vk;

	if(w->prop == ICAL_NO_PROPERTY || w->value == 0){
	    icalerror_set_errno(ICAL_INTERNAL_ERROR);
	    return 0;
	}

	/* First, create a value from the gauge */
	vk = icalenum_property_kind_to_value_kind(w->prop);

	if(vk == ICAL_NO_VALUE){
            icalerror_set_errno(ICAL_INTERNAL_ERROR);
	    return 0;
	}

	v = icalvalue_new_from_string(vk,w->value);

	if (v == 0){
	    /* Keep error set by icalvalue_from-string*/
	    return 0;
	}
	    
	/* Now find the corresponding property in the component,
	   descending into a sub-component if necessary */

	if(w->comp == ICAL_NO_COMPONENT){
	    sub_comp = inner;
	} else {
	    sub_comp = icalcomponent_get_first_component(inner,w->comp);
	    if(sub_comp == 0){
		return 0;
	    }
	}	   

	this_clause = 0;
	local_pass = 0;
	for(prop = icalcomponent_get_first_property(sub_comp,w->prop);
	    prop != 0;
	    prop = icalcomponent_get_next_property(sub_comp,w->prop)){
	    icalvalue* prop_value;
	    icalgaugecompare relation;

	    prop_value = icalproperty_get_value(prop);

	    relation = (icalgaugecompare)icalvalue_compare(prop_value,v);
	    
	    if (relation  == w->compare){ 
		local_pass++; 
	    } else if (w->compare == ICALGAUGECOMPARE_LESSEQUAL && 
		       ( relation  == ICALGAUGECOMPARE_LESS ||
			 relation  == ICALGAUGECOMPARE_EQUAL)) {
		local_pass++;
	    } else if (w->compare == ICALGAUGECOMPARE_GREATEREQUAL && 
		       ( relation  == ICALGAUGECOMPARE_GREATER ||
			 relation  == ICALGAUGECOMPARE_EQUAL)) {
		local_pass++;
	    } else if (w->compare == ICALGAUGECOMPARE_NOTEQUAL && 
		       ( relation  == ICALGAUGECOMPARE_GREATER ||
			 relation  == ICALGAUGECOMPARE_LESS)) {
		local_pass++;
	    } else {
		local_pass = 0;
	    }
	}
    
	this_clause = local_pass > 0 ? 1 : 0;

	/* Now look at the logic operator for this clause to see how
           the value should be merge with the previous clause */

	if(w->logic == ICALGAUGELOGIC_AND){
	    last_clause = this_clause && last_clause;
	} else if(w->logic == ICALGAUGELOGIC_OR) {
	    last_clause = this_clause || last_clause;
	} else {
	    last_clause = this_clause;
	}

	icalvalue_free(v);
    }


    return last_clause;

}
    

void icalgauge_dump(icalcomponent* gauge)
{

    pvl_elem *p;
    struct icalgauge_impl *impl = (struct icalgauge_impl*)gauge;
  
  
    printf("--- Select ---\n");
    for(p = pvl_head(impl->select);p!=0;p=pvl_next(p)){
	struct icalgauge_where *w = pvl_data(p);

	if(w->comp != ICAL_NO_COMPONENT){
	    printf("%s ",icalenum_component_kind_to_string(w->comp));
	}

	if(w->prop != ICAL_NO_PROPERTY){
	    printf("%s ",icalenum_property_kind_to_string(w->prop));
	}
	
	if (w->compare != ICALGAUGECOMPARE_NONE){
	    printf("%d ",w->compare);
	}


	if (w->value!=0){
	    printf("%s",w->value);
	}


	printf("\n");
    }

    printf("--- From ---\n");
    for(p = pvl_head(impl->from);p!=0;p=pvl_next(p)){
	icalcomponent_kind k = (icalcomponent_kind)pvl_data(p);

	printf("%s\n",icalenum_component_kind_to_string(k));
    }

    printf("--- Where ---\n");
    for(p = pvl_head(impl->where);p!=0;p=pvl_next(p)){
	struct icalgauge_where *w = pvl_data(p);

	if(w->logic != ICALGAUGELOGIC_NONE){
	    printf("%d ",w->logic);
	}
	
	if(w->comp != ICAL_NO_COMPONENT){
	    printf("%s ",icalenum_component_kind_to_string(w->comp));
	}

	if(w->prop != ICAL_NO_PROPERTY){
	    printf("%s ",icalenum_property_kind_to_string(w->prop));
	}
	
	if (w->compare != ICALGAUGECOMPARE_NONE){
	    printf("%d ",w->compare);
	}


	if (w->value!=0){
	    printf("%s",w->value);
	}


	printf("\n");
    }

    
}

