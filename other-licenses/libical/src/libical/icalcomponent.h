/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalcomponent.h
 CREATOR: eric 20 March 1999


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalcomponent.h

======================================================================*/

#ifndef ICALCOMPONENT_H
#define ICALCOMPONENT_H

#include "icalproperty.h"
#include "icalvalue.h"
#include "icalenums.h" /* defines icalcomponent_kind */
#include "icalattendee.h"
#include "pvl.h"

typedef void icalcomponent;

/* This is exposed so that callers will not have to allocate and
   deallocate iterators. Pretend that you can't see it. */
typedef struct icalcompiter
{
	icalcomponent_kind kind;
	pvl_elem iter;

} icalcompiter;

icalcomponent* icalcomponent_new(icalcomponent_kind kind);
icalcomponent* icalcomponent_new_clone(icalcomponent* component);
icalcomponent* icalcomponent_new_from_string(char* str);
icalcomponent* icalcomponent_vanew(icalcomponent_kind kind, ...);
void icalcomponent_free(icalcomponent* component);

char* icalcomponent_as_ical_string(icalcomponent* component);

int icalcomponent_is_valid(icalcomponent* component);

icalcomponent_kind icalcomponent_isa(icalcomponent* component);

int icalcomponent_isa_component (void* component);

/* 
 * Working with properties
 */

void icalcomponent_add_property(icalcomponent* component,
				icalproperty* property);

void icalcomponent_remove_property(icalcomponent* component,
				   icalproperty* property);

int icalcomponent_count_properties(icalcomponent* component,
				   icalproperty_kind kind);

/* Iterate through the properties */
icalproperty* icalcomponent_get_current_property(icalcomponent* component);

icalproperty* icalcomponent_get_first_property(icalcomponent* component,
					      icalproperty_kind kind);
icalproperty* icalcomponent_get_next_property(icalcomponent* component,
					      icalproperty_kind kind);


/* 
 * Working with components
 */ 


/* Return the first VEVENT, VTODO or VJOURNAL sub-component of cop, or
   comp if it is one of those types */

icalcomponent* icalcomponent_get_inner(icalcomponent* comp);


void icalcomponent_add_component(icalcomponent* parent,
				icalcomponent* child);

void icalcomponent_remove_component(icalcomponent* parent,
				icalcomponent* child);

int icalcomponent_count_components(icalcomponent* component,
				   icalcomponent_kind kind);

/* Iteration Routines. There are two forms of iterators, internal and
external. The internal ones came first, and are almost completely
sufficient, but they fail badly when you want to construct a loop that
removes components from the container.*/


/* Iterate through components */
icalcomponent* icalcomponent_get_current_component (icalcomponent* component);

icalcomponent* icalcomponent_get_first_component(icalcomponent* component,
					      icalcomponent_kind kind);
icalcomponent* icalcomponent_get_next_component(icalcomponent* component,
					      icalcomponent_kind kind);

/* Using external iterators */
icalcompiter icalcomponent_begin_component(icalcomponent* component,
					   icalcomponent_kind kind);
icalcompiter icalcomponent_end_component(icalcomponent* component,
					 icalcomponent_kind kind);
icalcomponent* icalcompiter_next(icalcompiter* i);
icalcomponent* icalcompiter_prior(icalcompiter* i);
icalcomponent* icalcompiter_deref(icalcompiter* i);




/* Working with embedded error properties */

int icalcomponent_count_errors(icalcomponent* component);

/* Remove all X-LIC-ERROR properties*/
void icalcomponent_strip_errors(icalcomponent* component);

/* Convert some X-LIC-ERROR properties into RETURN-STATUS properties*/
void icalcomponent_convert_errors(icalcomponent* component);

/* Internal operations. They are private, and you should not be using them. */
icalcomponent* icalcomponent_get_parent(icalcomponent* component);
void icalcomponent_set_parent(icalcomponent* component, 
			      icalcomponent* parent);

/* Kind conversion routiens */

icalcomponent_kind icalcomponent_string_to_kind(const char* string);

const char* icalcomponent_kind_to_string(icalcomponent_kind kind);


/************* Derived class methods.  ****************************

If the code was in an OO language, the remaining routines would be
members of classes derived from icalcomponent. Don't call them on the
wrong component subtypes. */

/* For VCOMPONENT: Return a reference to the first VEVENT, VTODO or
   VJOURNAL */
icalcomponent* icalcomponent_get_first_real_component(icalcomponent *c);

/* For VEVENT, VTODO, VJOURNAL and VTIMEZONE: report the start and end
   times of an event in UTC */
struct icaltime_span icalcomponent_get_span(icalcomponent* comp);

/******************** Convienience routines **********************/

void icalcomponent_set_dtstart(icalcomponent* comp, struct icaltimetype v);
struct icaltimetype icalcomponent_get_dtstart(icalcomponent* comp);

/* For the icalcomponent routines only, dtend and duration are tied
   together. If you call the set routine for one and the other exists,
   the routine will calculate the change to the other. That is, if
   there is a DTEND and you call set_duration, the routine will modify
   DTEND to be the sum of DTSTART and the duration. If you call a get
   routine for one and the other exists, the routine will calculate
   the return value. If you call a set routine and neither exists, the
   routine will create the apcompriate comperty */


struct icaltimetype icalcomponent_get_dtend(icalcomponent* comp);
void icalcomponent_set_dtend(icalcomponent* comp, struct icaltimetype v);

void icalcomponent_set_duration(icalcomponent* comp, 
				struct icaldurationtype v);
struct icaldurationtype icalcomponent_get_duration(icalcomponent* comp);

void icalcomponent_set_method(icalcomponent* comp, icalproperty_method method);
icalproperty_method icalcomponent_get_method(icalcomponent* comp);

struct icaltimetype icalcomponent_get_dtstamp(icalcomponent* comp);
void icalcomponent_set_dtstamp(icalcomponent* comp, struct icaltimetype v);


void icalcomponent_set_summary(icalcomponent* comp, const char* v);
const char* icalcomponent_get_summary(icalcomponent* comp);

void icalcomponent_set_comment(icalcomponent* comp, const char* v);
const char* icalcomponent_get_comment(icalcomponent* comp);

void icalcomponent_set_uid(icalcomponent* comp, const char* v);
const char* icalcomponent_get_uid(icalcomponent* comp);

void icalcomponent_set_recurrenceid(icalcomponent* comp, 
				    struct icaltimetype v);
struct icaltimetype icalcomponent_get_recurrenceid(icalcomponent* comp);


void icalcomponent_set_organizer(icalcomponent* comp, 
				 struct icalorganizertype org);
                                 struct icalorganizertype icalcomponent_get_organizer(icalcomponent* comp);


void icalcomponent_add_attendee(icalcomponent *comp,
				struct icalattendeetype attendee);

int icalcomponent_remove_attendee(icalcomponent *comp, char* cuid);

/* Get the Nth attendee. Out of range indices return an attendee
   with cuid == 0 */
struct icalattendeetype icalcomponent_get_attendee(icalcomponent *comp,
  int index);




/*************** Type Specific routines ***************/

icalcomponent* icalcomponent_new_vcalendar();
icalcomponent* icalcomponent_new_vevent();
icalcomponent* icalcomponent_new_vtodo();
icalcomponent* icalcomponent_new_vjournal();
icalcomponent* icalcomponent_new_valarm();
icalcomponent* icalcomponent_new_vfreebusy();
icalcomponent* icalcomponent_new_vtimezone();
icalcomponent* icalcomponent_new_xstandard();
icalcomponent* icalcomponent_new_xdaylight();



#endif /* !ICALCOMPONENT_H */



