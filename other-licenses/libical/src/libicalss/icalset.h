/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalset.h
 CREATOR: eric 28 November 1999


 Icalset is the "base class" for representations of a collection of
 iCal components. Derived classes (actually delegatees) include:
 
    icalfileset   Store componetns in a single file
    icaldirset    Store components in multiple files in a directory
    icalheapset   Store components on the heap
    icalmysqlset  Store components in a mysql database. 

 $Id: icalset.h,v 1.3 2002/04/18 18:47:31 mostafah%oeone.com Exp $
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

#ifndef ICALSET_H
#define ICALSET_H

#include <limits.h> /* For PATH_MAX */
#include "ical.h"
#include "icalerror.h"

#ifdef PATH_MAX
#define ICAL_PATH_MAX PATH_MAX
#else
#define ICAL_PATH_MAX 1024
#endif




typedef void icalset;

typedef enum icalset_kind {
    ICAL_FILE_SET,
    ICAL_DIR_SET,
    ICAL_HEAP_SET,
    ICAL_MYSQL_SET,
    ICAL_CAP_SET
} icalset_kind;


/* Create a specific derived type of set */
icalset* icalset_new_file(const char* path);
icalset* icalset_new_file_reader(const char* path);
icalset* icalset_new_file_writer(const char* path);

icalset* icalset_new_dir(const char* path);
icalset* icalset_new_file_reader(const char* path);
icalset* icalset_new_file_writer(const char* path);

icalset* icalset_new_heap(void);
icalset* icalset_new_mysql(const char* path);
/*icalset* icalset_new_cap(icalcstp* cstp);*/

void icalset_free(icalset* set);

const char* icalset_path(icalset* set);

/* Mark the cluster as changed, so it will be written to disk when it
   is freed. Commit writes to disk immediately*/
void icalset_mark(icalset* set);
icalerrorenum icalset_commit(icalset* set); 

icalerrorenum icalset_add_component(icalset* set, icalcomponent* comp);
icalerrorenum icalset_remove_component(icalset* set, icalcomponent* comp);

int icalset_count_components(icalset* set,
			     icalcomponent_kind kind);

/* Restrict the component returned by icalset_first, _next to those
   that pass the gauge. _clear removes the gauge. */
icalerrorenum icalset_select(icalset* set, icalcomponent* gauge);
void icalset_clear_select(icalset* set);

/* Get a component by uid */
icalcomponent* icalset_fetch(icalset* set, const char* uid);
int icalset_has_uid(icalset* set, const char* uid);
icalcomponent* icalset_fetch_match(icalset* set, icalcomponent *c);

/* Modify components according to the MODIFY method of CAP. Works on
   the currently selected components. */
icalerrorenum icalset_modify(icalset* set, icalcomponent *oldc,
			       icalcomponent *newc);

/* Iterate through the components. If a guage has been defined, these
   will skip over components that do not pass the gauge */

icalcomponent* icalset_get_current_component(icalset* set);
icalcomponent* icalset_get_first_component(icalset* set);
icalcomponent* icalset_get_next_component(icalset* set);

#endif /* !ICALSET_H */



