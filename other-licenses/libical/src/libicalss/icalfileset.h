/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalfileset.h
 CREATOR: eric 23 December 1999


 $Id: icalfileset.h,v 1.5 2002/11/06 21:22:43 mostafah%oeone.com Exp $
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

#ifndef ICALFILESET_H
#define ICALFILESET_H

#include "ical.h"
#include "icalset.h"
#include "icalgauge.h"
#ifndef XP_MAC
#include <sys/types.h> /* For open() flags and mode */
#include <sys/stat.h> /* For open() flags and mode */
#endif
#include <fcntl.h> /* For open() flags and mode */

#ifdef WIN32
#define mode_t int
#endif

extern int icalfileset_safe_saves;

typedef void icalfileset;


/* icalfileset
   icalfilesetfile
   icalfilesetdir
*/


icalfileset* icalfileset_new(const char* path);
icalfileset* icalfileset_new_reader(const char* path);
icalfileset* icalfileset_new_writer(const char* path);


/* Like _new, but takes open() flags for opening the file */
icalfileset* icalfileset_new_open(const char* path, 
				  int flags, mode_t mode);

void icalfileset_free(icalfileset* cluster);

const char* icalfileset_path(icalfileset* cluster);

/* Mark the cluster as changed, so it will be written to disk when it
   is freed. Commit writes to disk immediately. */
void icalfileset_mark(icalfileset* cluster);
icalerrorenum icalfileset_commit(icalfileset* cluster); 

icalerrorenum icalfileset_add_component(icalfileset* cluster,
					icalcomponent* child);

icalerrorenum icalfileset_remove_component(icalfileset* cluster,
					   icalcomponent* child);

int icalfileset_count_components(icalfileset* cluster,
				 icalcomponent_kind kind);

/* Restrict the component returned by icalfileset_first, _next to those
   that pass the gauge. _clear removes the gauge */
icalerrorenum icalfileset_select(icalfileset* store, icalgauge* gauge);
void icalfileset_clear(icalfileset* store);

/* Get and search for a component by uid */
icalcomponent* icalfileset_fetch(icalfileset* cluster, const char* uid);
int icalfileset_has_uid(icalfileset* cluster, const char* uid);
icalcomponent* icalfileset_fetch_match(icalfileset* set, icalcomponent *c);


/* Modify components according to the MODIFY method of CAP. Works on
   the currently selected components. */
icalerrorenum icalfileset_modify(icalfileset* store, icalcomponent *oldcomp,
			       icalcomponent *newcomp);

/* Iterate through components. If a guage has been defined, these
   will skip over components that do not pass the gauge */

icalcomponent* icalfileset_get_current_component (icalfileset* cluster);
icalcomponent* icalfileset_get_first_component(icalfileset* cluster);
icalcomponent* icalfileset_get_next_component(icalfileset* cluster);
/* Return a reference to the internal component. You probably should
   not be using this. */

icalcomponent* icalfileset_get_component(icalfileset* cluster);


#endif /* !ICALFILESET_H */



