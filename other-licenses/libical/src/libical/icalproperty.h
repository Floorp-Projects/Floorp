/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalproperty.h
  CREATOR: eric 20 March 1999


  $Id: icalproperty.h,v 1.3 2004/03/12 22:20:52 mostafah%oeone.com Exp $
  $Locker:  $

  

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalparam.h

  ======================================================================*/


#ifndef ICALPROPERTY_H
#define ICALPROPERTY_H

#include <time.h>


#include "icalderivedparameter.h"

#include "icalvalue.h"  
#include "icalrecur.h"

/* Actually in icalderivedproperty.h:
   typedef void icalproperty; */

#include "icalderivedproperty.h" /* To get icalproperty_kind enumerations */

icalproperty* icalproperty_new(icalproperty_kind kind);

icalproperty* icalproperty_new_clone(icalproperty * prop);

icalproperty* icalproperty_new_from_string(const char* str);

const char* icalproperty_as_ical_string(icalproperty* prop);

void  icalproperty_free(icalproperty* prop);

icalproperty_kind icalproperty_isa(icalproperty* property);
int icalproperty_isa_property(void* property);

void icalproperty_add_parameter(icalproperty* prop,icalparameter* parameter);
void icalproperty_set_parameter(icalproperty* prop,icalparameter* parameter);
void icalproperty_set_parameter_from_string(icalproperty* prop,
                                            const char* name, const char* value);
const char* icalproperty_get_parameter_as_string(icalproperty* prop,
                                                 const char* name);

void icalproperty_remove_parameter(icalproperty* prop,
				   icalparameter_kind kind);

int icalproperty_count_parameters(icalproperty* prop);

/* Iterate through the parameters */
icalparameter* icalproperty_get_first_parameter(icalproperty* prop,
						icalparameter_kind kind);
icalparameter* icalproperty_get_next_parameter(icalproperty* prop,
						icalparameter_kind kind);
/* Access the value of the property */
void icalproperty_set_value(icalproperty* prop, icalvalue* value);
void icalproperty_set_value_from_string(icalproperty* prop,const char* value, const char* kind);

icalvalue* icalproperty_get_value(icalproperty* prop);
const char* icalproperty_get_value_as_string(icalproperty* prop);

/* Deal with X properties */

void icalproperty_set_x_name(icalproperty* prop, const char* name);
const char* icalproperty_get_x_name(icalproperty* prop);

/* Return the name of the property -- the type name converted to a
   string, or the value of _get_x_name if the type is and X property */
const char* icalproperty_get_name (icalproperty* prop);

icalvalue_kind icalparameter_value_to_value_kind(icalparameter_value value);

/* Convert kinds to string and get default value type */

icalvalue_kind icalproperty_kind_to_value_kind(icalproperty_kind kind);
icalproperty_kind icalproperty_value_kind_to_kind(icalvalue_kind kind);
const char* icalproperty_kind_to_string(icalproperty_kind kind);
icalproperty_kind icalproperty_string_to_kind(const char* string);

icalproperty_method icalproperty_string_to_method(const char* str);
const char* icalproperty_method_to_string(icalproperty_method method);


const char* icalproperty_enum_to_string(int e);
int icalproperty_string_to_enum(const char* str);

const char* icalproperty_status_to_string(icalproperty_status);
icalproperty_status icalproperty_string_to_status(const char* string);

int icalproperty_enum_belongs_to_property(icalproperty_kind kind, int e);




#endif /*ICALPROPERTY_H*/
