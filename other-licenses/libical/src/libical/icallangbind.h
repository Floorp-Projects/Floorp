/* -*- Mode: C -*-
  ======================================================================
  FILE: icallangbind.h
  CREATOR: eric 25 jan 2001
  
  DESCRIPTION:
  
  $Id: icallangbind.h,v 1.2 2001/12/21 18:56:21 mikep%oeone.com Exp $
  $Locker:  $

  (C) COPYRIGHT 1999 Eric Busboom 
  http://www.softwarestudio.org
  
  This package is free software and is provided "as is" without
  express or implied warranty.  It may be used, redistributed and/or
  modified under the same terms as perl itself. ( Either the Artistic
  License or the GPL. )

  ======================================================================*/

#ifndef __ICALLANGBIND_H__
#define __ICALLANGBIND_H__

int* icallangbind_new_array(int size);
void icallangbind_free_array(int* array);
int icallangbind_access_array(int* array, int index);
icalproperty* icallangbind_get_property(icalcomponent *c, int n, const char* prop);
const char* icallangbind_get_property_val(icalproperty* p);
const char* icallangbind_get_parameter(icalproperty *p, const char* parameter);
icalcomponent* icallangbind_get_component(icalcomponent *c, const char* comp);

icalproperty* icallangbind_get_first_property(icalcomponent *c,
                                              const char* prop);

icalproperty* icallangbind_get_next_property(icalcomponent *c,
                                              const char* prop);

icalcomponent* icallangbind_get_first_component(icalcomponent *c,
                                              const char* comp);

icalcomponent* icallangbind_get_next_component(icalcomponent *c,
                                              const char* comp);


const char* icallangbind_property_eval_string(icalproperty* prop, char* sep);


int icallangbind_string_to_open_flag(const char* str);
#endif /*__ICALLANGBIND_H__*/
