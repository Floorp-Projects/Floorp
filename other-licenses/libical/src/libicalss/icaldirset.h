/* -*- Mode: C -*- */
/*======================================================================
 FILE: icaldirset.h
 CREATOR: eric 28 November 1999


 $Id: icaldirset.h,v 1.3 2002/04/18 18:47:30 mostafah%oeone.com Exp $
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

#ifndef ICALDIRSET_H
#define ICALDIRSET_H

#include "ical.h"

/* icaldirset Routines for storing, fetching, and searching for ical
 * objects in a database */

typedef void icaldirset;


icaldirset* icaldirset_new(const char* path);

icaldirset* icaldirset_new_reader(const char* path);
icaldirset* icaldirset_new_writer(const char* path);


void icaldirset_free(icaldirset* store);

const char* icaldirset_path(icaldirset* store);

/* Mark the cluster as changed, so it will be written to disk when it
   is freed. Commit writes to disk immediately*/
void icaldirset_mark(icaldirset* store);
icalerrorenum icaldirset_commit(icaldirset* store); 

icalerrorenum icaldirset_add_component(icaldirset* store, icalcomponent* comp);
icalerrorenum icaldirset_remove_component(icaldirset* store, icalcomponent* comp);

int icaldirset_count_components(icaldirset* store,
			       icalcomponent_kind kind);

/* Restrict the component returned by icaldirset_first, _next to those
   that pass the gauge. _clear removes the gauge. */
icalerrorenum icaldirset_select(icaldirset* store, icalcomponent* gauge);
void icaldirset_clear(icaldirset* store);

/* Get a component by uid */
icalcomponent* icaldirset_fetch(icaldirset* store, const char* uid);
int icaldirset_has_uid(icaldirset* store, const char* uid);
icalcomponent* icaldirset_fetch_match(icaldirset* set, icalcomponent *c);

/* Modify components according to the MODIFY method of CAP. Works on
   the currently selected components. */
icalerrorenum icaldirset_modify(icaldirset* store, icalcomponent *oldc,
			       icalcomponent *newc);

/* Iterate through the components. If a guage has been defined, these
   will skip over components that do not pass the gauge */

icalcomponent* icaldirset_get_current_component(icaldirset* store);
icalcomponent* icaldirset_get_first_component(icaldirset* store);
icalcomponent* icaldirset_get_next_component(icaldirset* store);

#endif /* !ICALDIRSET_H */



