/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalparam.h
  CREATOR: eric 20 March 1999


  $Id: icalparameter.h,v 1.2 2001/12/21 18:56:22 mikep%oeone.com Exp $
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

#ifndef ICALPARAM_H
#define ICALPARAM_H

#include "icalderivedparameter.h"

/* Declared in icalderivedparameter.h */
/*typedef void icalparameter;*/

icalparameter* icalparameter_new(icalparameter_kind kind);
icalparameter* icalparameter_new_clone(icalparameter* p);

/* Create from string of form "PARAMNAME=VALUE" */
icalparameter* icalparameter_new_from_string(const char* value);

/* Create from just the value, the part after the "=" */
icalparameter* icalparameter_new_from_value_string(icalparameter_kind kind, const char* value);

void icalparameter_free(icalparameter* parameter);

char* icalparameter_as_ical_string(icalparameter* parameter);

int icalparameter_is_valid(icalparameter* parameter);

icalparameter_kind icalparameter_isa(icalparameter* parameter);

int icalparameter_isa_parameter(void* param);

/* Acess the name of an X parameer */
void icalparameter_set_xname (icalparameter* param, const char* v);
const char* icalparameter_get_xname(icalparameter* param);
void icalparameter_set_xvalue (icalparameter* param, const char* v);
const char* icalparameter_get_xvalue(icalparameter* param);

/* Convert enumerations */

const char* icalparameter_kind_to_string(icalparameter_kind kind);
icalparameter_kind icalparameter_string_to_kind(const char* string);



#endif 
