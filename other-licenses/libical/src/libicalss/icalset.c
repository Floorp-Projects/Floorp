/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalset.c
 CREATOR: eric 17 Jul 2000


 Icalset is the "base class" for representations of a collection of
 iCal components. Derived classes (actually delegates) include:
 
    icalfileset   Store components in a single file
    icaldirset    Store components in multiple files in a directory
    icalheapset   Store components on the heap
    icalmysqlset  Store components in a mysql database. 

 $Id: icalset.c,v 1.5 2002/11/06 21:22:44 mostafah%oeone.com Exp $
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
#include "icalset.h"
#include "icalfileset.h"
#include "icalfilesetimpl.h"
#include "icaldirset.h"
#include "icaldirsetimpl.h"
#include <stdlib.h>
#ifdef XP_MAC
#include <string.h>
#endif
/*#include "icalheapset.h"*/
/*#include "icalmysqlset.h"*/

#define ICALSET_ID "set "

struct icalset_fp {
	void (*free)(icalset* set);
	const char* (*path)(icalset* set);
	void (*mark)(icalset* set);
	icalerrorenum (*commit)(icalset* set); 
	icalerrorenum (*add_component)(icalset* set, icalcomponent* comp);
	icalerrorenum (*remove_component)(icalset* set, icalcomponent* comp);
	int (*count_components)(icalset* set,
			     icalcomponent_kind kind);
	icalerrorenum (*select)(icalset* set, icalcomponent* gauge);
	void (*clear)(icalset* set);
	icalcomponent* (*fetch)(icalset* set, const char* uid);
	icalcomponent* (*fetch_match)(icalset* set, icalcomponent *comp);
	int (*has_uid)(icalset* set, const char* uid);
	icalerrorenum (*modify)(icalset* set, icalcomponent *old,
				     icalcomponent *new);
	icalcomponent* (*get_current_component)(icalset* set);	
	icalcomponent* (*get_first_component)(icalset* set);
	icalcomponent* (*get_next_component)(icalset* set);
};

struct icalset_fp icalset_dirset_fp = {
    icaldirset_free,
    icaldirset_path,
    icaldirset_mark,
    icaldirset_commit,
    icaldirset_add_component,
    icaldirset_remove_component,
    icaldirset_count_components,
    icaldirset_select,
    icaldirset_clear,
    icaldirset_fetch,
    icaldirset_fetch_match,
    icaldirset_has_uid,
    icaldirset_modify,
    icaldirset_get_current_component,
    icaldirset_get_first_component,
    icaldirset_get_next_component
};


struct icalset_fp icalset_fileset_fp = {
    icalfileset_free,
    icalfileset_path,
    icalfileset_mark,
    icalfileset_commit,
    icalfileset_add_component,
    icalfileset_remove_component,
    icalfileset_count_components,
    icalfileset_select,
    icalfileset_clear,
    icalfileset_fetch,
    icalfileset_fetch_match,
    icalfileset_has_uid,
    icalfileset_modify,
    icalfileset_get_current_component,
    icalfileset_get_first_component,
    icalfileset_get_next_component
};

struct icalset_impl {

	char id[5]; /* "set " */

	void *derived_impl;
	struct icalset_fp *fp;
};

/* Figure out what was actually passed in as the set. This could be a
   set or and of the derived types such as dirset or fileset. Note
   this routine returns a value, not a reference, to avoid memory
   leaks in the methods */
struct icalset_impl icalset_get_impl(icalset* set)
{
    struct icalset_impl  impl;

    memset(&impl,0,sizeof(impl));
    icalerror_check_arg_re( (set!=0),"set",impl);

    if(strcmp((char*)set,ICALSET_ID)==0) {
	/* It is actually a set, so just sent the reference back out. */
	return *(struct icalset_impl*)set;
    } else if(strcmp((char*)set,ICALFILESET_ID)==0) {
	/* Make a new set from the fileset */
	impl.fp = &icalset_fileset_fp;
	impl.derived_impl = set;
	strcpy(impl.id,ICALFILESET_ID);/* HACK. Is this necessary? */
	return impl;
    } else if(strcmp((char*)set,ICALDIRSET_ID)==0) {
	/* Make a new set from the dirset */
	impl.fp = &icalset_dirset_fp;
	impl.derived_impl = set;
	strcpy(impl.id,ICALDIRSET_ID);/* HACK. Is this necessary? */
	return impl;
    } else {
	/* The type of set is unknown, so throw an error */
	icalerror_assert((0),"Unknown set type");
	return impl;
    }
}


struct icalset_impl* icalset_new_impl()
{
    
    struct icalset_impl* impl;

    if ( ( impl = (struct icalset_impl*)
	   malloc(sizeof(struct icalset_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    strcpy(impl->id,ICALSET_ID);

    impl->derived_impl = 0;
    impl->fp = 0;

    return impl;
}

struct icalset_impl* icalset_new_file_from_ref(icalfileset *fset)
{
    struct icalset_impl *impl = icalset_new_impl();

    icalerror_check_arg_rz( (fset!=0),"fset");

    if(impl == 0){
	free(impl);
	return 0;
    }

    impl->derived_impl = fset;

    if (impl->derived_impl == 0){
	free(impl);
	return 0;
    }

    impl->fp = &icalset_fileset_fp;

    return (struct icalset_impl*)impl;
}

icalset* icalset_new_file(const char* path)
{
    icalfileset *fset = icalfileset_new(path);

    if(fset == 0){
	return 0;
    }

    return (icalset*)icalset_new_file_from_ref(fset);
}

icalset* icalset_new_file_writer(const char* path)
{
    icalfileset *fset = icalfileset_new_writer(path);

    if(fset == 0){
	return 0;
    }

    return (icalset*)icalset_new_file_from_ref(fset);
}

icalset* icalset_new_file_reader(const char* path)
{
    icalfileset *fset = icalfileset_new_reader(path);

    if(fset == 0){
	return 0;
    }

    return (icalset*)icalset_new_file_from_ref(fset);
}

icalset* icalset_new_dir_from_ref(icaldirset *dset)
{

    struct icalset_impl *impl = icalset_new_impl();

    icalerror_check_arg_rz( (dset!=0),"dset");

    if(impl == 0){
	return 0;
    }

    impl->derived_impl = dset;

    if (impl->derived_impl == 0){
	free(impl);
	return 0;
    }

    impl->fp = &icalset_dirset_fp;

    return impl;
}

icalset* icalset_new_dir(const char* path)
{
    icaldirset *dset = icaldirset_new(path);

    if(dset == 0){
	return 0;
    }

    return icalset_new_dir_from_ref(dset);
}

icalset* icalset_new_dir_writer(const char* path)
{
    icaldirset *dset = icaldirset_new_writer(path);

    if(dset == 0){
	return 0;
    }

    return icalset_new_dir_from_ref(dset);
}

icalset* icalset_new_dir_reader(const char* path)
{
    icaldirset *dset = icaldirset_new_reader(path);

    if(dset == 0){
	return 0;
    }

    return icalset_new_dir_from_ref(dset);
}

icalset* icalset_new_heap(void)
{
    struct icalset_impl *impl = icalset_new_impl();


    if(impl == 0){
	free(impl);
	return 0;
    }

    return 0;
}

icalset* icalset_new_mysql(const char* path)
{
    struct icalset_impl *impl = icalset_new_impl();

    if(impl == 0){
	free(impl);
	return 0;
    }

    return 0;
}

void icalset_free(icalset* set)
{
    struct icalset_impl impl = icalset_get_impl(set);
    (*(impl.fp->free))(impl.derived_impl);

   if(!strcmp((char*)set,ICALSET_ID)) {    
       free(set);
   }
}

const char* icalset_path(icalset* set)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->path))(impl.derived_impl);
}

void icalset_mark(icalset* set)
{
    struct icalset_impl impl = icalset_get_impl(set);
    (*(impl.fp->mark))(impl.derived_impl);
}

icalerrorenum icalset_commit(icalset* set)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->commit))(impl.derived_impl);
}

icalerrorenum icalset_add_component(icalset* set, icalcomponent* comp)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->add_component))(impl.derived_impl,comp);
}

icalerrorenum icalset_remove_component(icalset* set, icalcomponent* comp)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->remove_component))(impl.derived_impl,comp);
}

int icalset_count_components(icalset* set,icalcomponent_kind kind)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->count_components))(impl.derived_impl,kind);
}

icalerrorenum icalset_select(icalset* set, icalcomponent* gauge)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->select))(impl.derived_impl,gauge);
}

void icalset_clear(icalset* set)
{
    struct icalset_impl impl = icalset_get_impl(set);
    (*(impl.fp->clear))(impl.derived_impl);
}

icalcomponent* icalset_fetch(icalset* set, const char* uid)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->fetch))(impl.derived_impl,uid);
}

icalcomponent* icalset_fetch_match(icalset* set, icalcomponent *comp)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->fetch_match))(impl.derived_impl,comp);
}


int icalset_has_uid(icalset* set, const char* uid)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->has_uid))(impl.derived_impl,uid);
}

icalerrorenum icalset_modify(icalset* set, icalcomponent *old,
			       icalcomponent *new)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->modify))(impl.derived_impl,old,new);
}

icalcomponent* icalset_get_current_component(icalset* set)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->get_current_component))(impl.derived_impl);
}

icalcomponent* icalset_get_first_component(icalset* set)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->get_first_component))(impl.derived_impl);
}

icalcomponent* icalset_get_next_component(icalset* set)
{
    struct icalset_impl impl = icalset_get_impl(set);
    return (*(impl.fp->get_next_component))(impl.derived_impl);
}




